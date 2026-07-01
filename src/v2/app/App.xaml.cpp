#include "pch.h"
#include "App.xaml.h"
#include "../dock/DockStateStore.h"
#include "../infra/Logger.h"
#include "../platform/DockPlacement.h"
#include "../shell/ShellIntegration.h"
#include "../ui/DockWindow.xaml.h"

#if __has_include("App.xaml.g.cpp")
#include "App.xaml.g.cpp"
#endif

namespace winrt::DockWMac::implementation
{
    namespace
    {
        bool Contains(std::vector<std::wstring> const& values, std::wstring const& value)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        void EraseValue(std::vector<std::wstring>& values, std::wstring const& value)
        {
            values.erase(std::remove(values.begin(), values.end(), value), values.end());
        }

        void EraseLocalPin(::DockWMac::dock::DockState& state, std::wstring const& itemId)
        {
            state.localPins.erase(
                std::remove_if(state.localPins.begin(), state.localPins.end(), [&](auto const& pin)
                {
                    return ::DockWMac::dock::IdentityForPinned(pin) == itemId;
                }),
                state.localPins.end());
        }

        std::wstring FirstNonEmpty(std::initializer_list<std::wstring> values)
        {
            for (auto const& value : values)
            {
                if (!value.empty())
                {
                    return value;
                }
            }
            return {};
        }
    }

    App::App()
    {
        m_paths = ::DockWMac::infra::ResolveRuntimePaths();
        ::DockWMac::infra::EnsureRuntimePaths(m_paths);
        m_settings = ::DockWMac::infra::LoadAppSettings(m_paths);
    }

    void App::OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&)
    {
        if (m_instance.IsDuplicate())
        {
            ::DockWMac::infra::LogLine(m_paths, "Duplicate launch ignored.");
            Exit();
            return;
        }

        ::DockWMac::infra::SaveAppSettings(m_paths, m_settings);
        ::DockWMac::infra::LogLine(m_paths, "Dock_WMac v2 starting.");
        m_previewHost = std::make_unique<::DockWMac::shell::DwmPreviewHost>();
        m_dockState = ::DockWMac::dock::LoadDockState(m_paths.dockStateFile);
        RefreshDockItems(true);

        auto window = winrt::make<DockWindow>();
        m_dockWindow = winrt::get_self<DockWindow>(window);
        m_window = window;
        ConfigureDockWindow();
        m_window.Activate();
        m_dockWindow->ApplyPlacement();
        StartRefreshTimer();
    }

    void App::StartRefreshTimer()
    {
        m_refreshTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer{};
        m_refreshTimer.Interval(std::chrono::seconds{ 2 });
        m_refreshTimer.Tick([this](winrt::Windows::Foundation::IInspectable const&, winrt::Windows::Foundation::IInspectable const&)
        {
            RefreshDockItems(false);
        });
        m_refreshTimer.Start();
    }

    void App::RefreshDockItems(bool initializeOrder)
    {
        m_items = ::DockWMac::dock::BuildDockItems(
            ::DockWMac::shell::ReadTaskbarPinnedItems(),
            ::DockWMac::shell::EnumerateTopLevelWindows(),
            m_dockState);
        ApplyIconCache();

        if (initializeOrder && m_dockState.order.empty())
        {
            for (auto const& item : m_items)
            {
                m_dockState.order.push_back(item.id);
            }
            ::DockWMac::dock::SaveDockState(m_paths.dockStateFile, m_dockState);
        }

        ConfigureDockWindow();
    }

    void App::ConfigureDockWindow()
    {
        if (!m_window)
        {
            return;
        }

        if (!m_dockWindow)
        {
            return;
        }

        auto effectiveSettings = m_settings;
        auto const accessibility = ::DockWMac::platform::ReadSystemAccessibility();
        effectiveSettings.reducedMotion = effectiveSettings.reducedMotion || accessibility.reducedMotion;
        effectiveSettings.highContrast = accessibility.highContrast;

        m_dockWindow->Configure(
            effectiveSettings,
            m_items,
            [this](::DockWMac::dock::DockAction const& action)
            {
                HandleDockAction(action);
            },
            [this](std::vector<std::wstring> const& order)
            {
                HandleDockOrderChanged(order);
            },
            [this](HWND hwnd)
            {
                ShowDockPreview(hwnd);
            },
            [this]()
            {
                HideDockPreview();
            });
        m_dockWindow->ApplyPlacement();
    }

    void App::ApplyIconCache()
    {
        for (auto& item : m_items)
        {
            auto const source = FirstNonEmpty({ item.iconPath, item.targetPath, item.linkPath });
            auto iconPath = ::DockWMac::shell::CacheIconForPath(source, m_paths.iconCacheDir, item.id);
            if (!iconPath.empty())
            {
                item.iconPath = std::move(iconPath);
            }
        }
    }

    void App::HandleDockAction(::DockWMac::dock::DockAction const& action)
    {
        switch (action.kind)
        {
        case ::DockWMac::dock::DockActionKind::Launch:
            if (auto it = std::find_if(m_items.begin(), m_items.end(), [&](auto const& item) { return item.id == action.itemId; }); it != m_items.end())
            {
                ::DockWMac::shell::LaunchPinnedApp({
                    it->displayName,
                    it->linkPath,
                    it->targetPath,
                    it->arguments,
                    it->appUserModelId,
                    it->iconPath,
                });
            }
            break;
        case ::DockWMac::dock::DockActionKind::ActivateWindow:
            ::DockWMac::shell::ActivateWindow(action.hwnd);
            break;
        case ::DockWMac::dock::DockActionKind::MinimizeWindow:
            ::DockWMac::shell::MinimizeWindow(action.hwnd);
            break;
        case ::DockWMac::dock::DockActionKind::PinToDock:
            HandlePinToDock(action.itemId);
            break;
        case ::DockWMac::dock::DockActionKind::UnpinFromDock:
            HandleUnpinFromDock(action.itemId);
            break;
        case ::DockWMac::dock::DockActionKind::ShowWindowChooser:
        case ::DockWMac::dock::DockActionKind::None:
        default:
            break;
        }
    }

    void App::HandleDockOrderChanged(std::vector<std::wstring> const& order)
    {
        m_dockState.order = order;
        ::DockWMac::dock::SaveDockState(m_paths.dockStateFile, m_dockState);
        ::DockWMac::infra::LogLine(m_paths, "Dock order updated.");

        std::vector<::DockWMac::dock::DockItem> ordered;
        ordered.reserve(m_items.size());
        for (auto const& id : order)
        {
            if (auto it = std::find_if(m_items.begin(), m_items.end(), [&](auto const& item) { return item.id == id; }); it != m_items.end())
            {
                ordered.push_back(*it);
            }
        }
        for (auto const& item : m_items)
        {
            if (std::none_of(ordered.begin(), ordered.end(), [&](auto const& existing) { return existing.id == item.id; }))
            {
                ordered.push_back(item);
            }
        }
        m_items = std::move(ordered);
    }

    void App::HandlePinToDock(std::wstring const& itemId)
    {
        auto it = std::find_if(m_items.begin(), m_items.end(), [&](auto const& item)
        {
            return item.id == itemId;
        });
        if (it == m_items.end())
        {
            return;
        }

        EraseLocalPin(m_dockState, itemId);
        EraseValue(m_dockState.hiddenSystemPins, itemId);

        m_dockState.localPins.push_back({
            it->displayName,
            it->linkPath,
            it->targetPath,
            it->arguments,
            it->appUserModelId,
            FirstNonEmpty({ it->linkPath, it->targetPath }),
        });

        if (!Contains(m_dockState.order, itemId))
        {
            m_dockState.order.push_back(itemId);
        }

        ::DockWMac::dock::SaveDockState(m_paths.dockStateFile, m_dockState);
        ::DockWMac::infra::LogLine(m_paths, "Dock item pinned locally.");
        RefreshDockItems(false);
    }

    void App::HandleUnpinFromDock(std::wstring const& itemId)
    {
        auto it = std::find_if(m_items.begin(), m_items.end(), [&](auto const& item)
        {
            return item.id == itemId;
        });
        if (it == m_items.end())
        {
            return;
        }

        EraseLocalPin(m_dockState, itemId);
        if (it->systemPinned && !Contains(m_dockState.hiddenSystemPins, itemId))
        {
            m_dockState.hiddenSystemPins.push_back(itemId);
        }
        EraseValue(m_dockState.order, itemId);

        ::DockWMac::dock::SaveDockState(m_paths.dockStateFile, m_dockState);
        ::DockWMac::infra::LogLine(m_paths, "Dock item unpinned locally.");
        RefreshDockItems(false);
    }

    void App::ShowDockPreview(HWND hwnd)
    {
        if (m_previewHost && !m_previewHost->Show(hwnd))
        {
            ::DockWMac::infra::LogLine(m_paths, "DWM preview unavailable for window.");
        }
    }

    void App::HideDockPreview()
    {
        if (m_previewHost)
        {
            m_previewHost->Hide();
        }
    }
}
