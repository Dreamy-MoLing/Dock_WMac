#include "pch.h"
#include "DockRuntime.h"
#include "DockSettingsActions.h"
#include "../dock/DockStateStore.h"
#include "../infra/Logger.h"
#include "../platform/DockPlacement.h"
#include "../shell/ShellIntegration.h"

namespace DockWMac::app
{
    namespace
    {
        constexpr UINT_PTR RefreshTimerIdSeed = 0;
        constexpr UINT RefreshIntervalMs = 10000;

        bool Contains(std::vector<std::wstring> const& values, std::wstring const& value)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        void EraseValue(std::vector<std::wstring>& values, std::wstring const& value)
        {
            values.erase(std::remove(values.begin(), values.end(), value), values.end());
        }

        void EraseLocalPin(DockWMac::dock::DockState& state, std::wstring const& itemId)
        {
            state.localPins.erase(
                std::remove_if(state.localPins.begin(), state.localPins.end(), [&](auto const& pin)
                {
                    return DockWMac::dock::IdentityForPinned(pin) == itemId;
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

        std::vector<std::wstring> PersistentOrderFromItems(
            std::vector<std::wstring> const& requestedOrder,
            std::vector<DockWMac::dock::DockItem> const& items)
        {
            std::vector<std::wstring> persistent;
            for (auto const& id : requestedOrder)
            {
                auto it = std::find_if(items.begin(), items.end(), [&](auto const& item)
                {
                    return item.id == id && item.pinned && !item.transientRunningOnly;
                });
                if (it != items.end() && !Contains(persistent, id))
                {
                    persistent.push_back(id);
                }
            }
            return persistent;
        }

        std::vector<std::wstring> PinnedOrderFromItems(std::vector<DockWMac::dock::DockItem> const& items)
        {
            std::vector<std::wstring> order;
            for (auto const& item : items)
            {
                if (item.pinned && !item.transientRunningOnly && !Contains(order, item.id))
                {
                    order.push_back(item.id);
                }
            }
            return order;
        }

        bool SameWindowRef(DockWMac::dock::DockWindowRef const& left, DockWMac::dock::DockWindowRef const& right)
        {
            return left.hwnd == right.hwnd &&
                left.title == right.title &&
                left.minimized == right.minimized &&
                left.cloaked == right.cloaked &&
                left.foreground == right.foreground;
        }

        bool SameDockItem(DockWMac::dock::DockItem const& left, DockWMac::dock::DockItem const& right)
        {
            if (left.id != right.id ||
                left.displayName != right.displayName ||
                left.linkPath != right.linkPath ||
                left.targetPath != right.targetPath ||
                left.arguments != right.arguments ||
                left.appUserModelId != right.appUserModelId ||
                left.iconPath != right.iconPath ||
                left.pinned != right.pinned ||
                left.systemPinned != right.systemPinned ||
                left.localPinned != right.localPinned ||
                left.transientRunningOnly != right.transientRunningOnly ||
                left.running != right.running ||
                left.foreground != right.foreground ||
                left.windows.size() != right.windows.size())
            {
                return false;
            }

            for (size_t index = 0; index < left.windows.size(); ++index)
            {
                if (!SameWindowRef(left.windows[index], right.windows[index]))
                {
                    return false;
                }
            }
            return true;
        }

        bool SameDockItems(std::vector<DockWMac::dock::DockItem> const& left, std::vector<DockWMac::dock::DockItem> const& right)
        {
            if (left.size() != right.size())
            {
                return false;
            }

            for (size_t index = 0; index < left.size(); ++index)
            {
                if (!SameDockItem(left[index], right[index]))
                {
                    return false;
                }
            }
            return true;
        }

        std::string HResultMessage(winrt::hresult_error const& error)
        {
            std::ostringstream output;
            output << "hresult=0x" << std::hex << static_cast<uint32_t>(error.code());
            if (!error.message().empty())
            {
                output << " message=" << winrt::to_string(error.message());
            }
            return output.str();
        }
    }

    DockRuntime::DockRuntime()
    {
        m_paths = DockWMac::infra::ResolveRuntimePaths();
        DockWMac::infra::EnsureRuntimePaths(m_paths);
        m_settings = DockWMac::infra::LoadAppSettings(m_paths);
    }

    DockRuntime::~DockRuntime()
    {
        Stop();
    }

    int DockRuntime::Run()
    {
        if (!Start())
        {
            return m_exitCode;
        }

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            if (HandleThreadTimer(message))
            {
                continue;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        Stop();
        return static_cast<int>(message.wParam);
    }

    bool DockRuntime::Start()
    {
        if (m_instance.IsDuplicate())
        {
            DockWMac::infra::LogLine(m_paths, "Duplicate launch ignored.");
            m_exitCode = 0;
            return false;
        }

        DockWMac::infra::SaveAppSettings(m_paths, m_settings);
        DockWMac::infra::LogLine(m_paths, "Dock_WMac v2 native runtime starting.");
        try
        {
            m_previewHost = std::make_unique<DockWMac::shell::DwmPreviewHost>();
            m_previewHost->SetActivateHandler([this](HWND hwnd)
            {
                DockWMac::shell::ActivateWindow(hwnd);
                ScheduleDockRefresh(std::chrono::milliseconds{ 250 });
            });
            m_dockState = DockWMac::dock::LoadDockState(m_paths.dockStateFile);
            LoadSystemPinnedSnapshot();
            RefreshDockItems(true);

            m_dockHost = std::make_unique<DockWMac::render::NativeDockHost>(
                [this](std::string_view message)
                {
                    DockWMac::infra::LogLine(m_paths, message);
                });
            ConfigureDockWindow();
            m_dockHost->Show();
            StartRefreshTimers();
            m_started = true;
            DockWMac::infra::LogLine(m_paths, "Dock_WMac v2 native runtime started.");
            return true;
        }
        catch (winrt::hresult_error const& error)
        {
            DockWMac::infra::LogLine(m_paths, "Startup failed: " + HResultMessage(error));
        }
        catch (std::exception const& error)
        {
            DockWMac::infra::LogLine(m_paths, std::string{ "Startup failed: " } + error.what());
        }
        catch (...)
        {
            DockWMac::infra::LogLine(m_paths, "Startup failed: unknown exception.");
        }

        Stop();
        m_exitCode = 1;
        return false;
    }

    void DockRuntime::Stop()
    {
        if (!m_started && !m_dockHost && !m_previewHost && !m_refreshTimer && !m_actionRefreshTimer)
        {
            return;
        }

        StopRefreshTimers();
        m_windowEventMonitor.Stop();
        if (m_previewHost)
        {
            m_previewHost->Hide();
        }
        if (m_dockHost)
        {
            m_dockHost->Hide();
        }
        m_previewHost.reset();
        m_dockHost.reset();
        m_started = false;
    }

    void DockRuntime::StartRefreshTimers()
    {
        m_refreshTimer = SetTimer(nullptr, RefreshTimerIdSeed, RefreshIntervalMs, nullptr);
        if (!m_refreshTimer)
        {
            DockWMac::infra::LogLine(m_paths, "Refresh timer unavailable; relying on shell event refreshes.");
        }

        if (m_windowEventMonitor.Start([this](DockWMac::shell::WindowEventRefreshReason)
        {
            ScheduleDockRefresh(std::chrono::milliseconds{ 180 });
        }))
        {
            DockWMac::infra::LogLine(m_paths, "Window event monitor started.");
        }
        else
        {
            DockWMac::infra::LogLine(m_paths, "Window event monitor unavailable; using refresh timer fallback.");
        }
    }

    void DockRuntime::StopRefreshTimers()
    {
        if (m_refreshTimer)
        {
            KillTimer(nullptr, m_refreshTimer);
            m_refreshTimer = 0;
        }
        if (m_actionRefreshTimer)
        {
            KillTimer(nullptr, m_actionRefreshTimer);
            m_actionRefreshTimer = 0;
        }
    }

    void DockRuntime::ScheduleDockRefresh(std::chrono::milliseconds delay)
    {
        if (m_actionRefreshTimer)
        {
            KillTimer(nullptr, m_actionRefreshTimer);
            m_actionRefreshTimer = 0;
        }

        auto timeout = std::clamp<int64_t>(delay.count(), 1, 30000);
        m_actionRefreshTimer = SetTimer(nullptr, RefreshTimerIdSeed, static_cast<UINT>(timeout), nullptr);
        if (!m_actionRefreshTimer)
        {
            RefreshDockItems(false);
        }
    }

    bool DockRuntime::HandleThreadTimer(MSG const& message)
    {
        if (message.message != WM_TIMER || message.hwnd != nullptr)
        {
            return false;
        }

        auto const timer = static_cast<UINT_PTR>(message.wParam);
        if (timer == m_refreshTimer)
        {
            RefreshDockItems(false);
            return true;
        }

        if (timer == m_actionRefreshTimer)
        {
            KillTimer(nullptr, m_actionRefreshTimer);
            m_actionRefreshTimer = 0;
            RefreshDockItems(false);
            return true;
        }

        return false;
    }

    void DockRuntime::LoadSystemPinnedSnapshot()
    {
        auto systemPinnedApps = DockWMac::shell::ReadTaskbarPinnedItems();
        if (DockWMac::dock::ApplyImportedTaskbarPins(m_dockState, systemPinnedApps))
        {
            DockWMac::dock::SaveDockState(m_paths.dockStateFile, m_dockState);
            DockWMac::infra::LogLine(m_paths, "Imported taskbar pin snapshot updated.");
        }

        m_systemPinnedApps = std::move(systemPinnedApps);
    }

    void DockRuntime::RefreshDockItems(bool initializeOrder)
    {
        auto nextItems = DockWMac::dock::BuildDockItems(
            m_systemPinnedApps,
            DockWMac::shell::EnumerateTopLevelWindows(),
            m_dockState);
        ApplyIconCache(nextItems);

        if (initializeOrder && m_dockState.order.empty())
        {
            m_dockState.order = PinnedOrderFromItems(nextItems);
            DockWMac::dock::SaveDockState(m_paths.dockStateFile, m_dockState);
            nextItems = DockWMac::dock::BuildDockItems(
                m_systemPinnedApps,
                DockWMac::shell::EnumerateTopLevelWindows(),
                m_dockState);
            ApplyIconCache(nextItems);
        }

        if (!initializeOrder && SameDockItems(m_items, nextItems))
        {
            return;
        }

        m_items = std::move(nextItems);
        ConfigureDockWindow();
    }

    void DockRuntime::ConfigureDockWindow()
    {
        if (!m_dockHost)
        {
            return;
        }

        auto effectiveSettings = m_settings;
        auto const accessibility = DockWMac::platform::ReadSystemAccessibility();
        effectiveSettings.reducedMotion = effectiveSettings.reducedMotion || accessibility.reducedMotion;
        effectiveSettings.highContrast = accessibility.highContrast;
        effectiveSettings.lightTheme = accessibility.lightTheme;

        m_dockHost->Configure(
            effectiveSettings,
            m_items,
            [this](DockWMac::dock::DockAction const& action)
            {
                HandleDockAction(action);
            },
            [this](std::vector<std::wstring> const& order)
            {
                HandleDockOrderChanged(order);
            },
            [this](DockWMac::dock::DockWindowRef const& window)
            {
                ShowDockPreview(window);
            },
            [this](std::vector<DockWMac::dock::DockWindowRef> const& windows)
            {
                return ShowDockWindowGroupPreview(windows);
            },
            [this]()
            {
                HideDockPreview();
            },
            [this]()
            {
                HandleShellEnvironmentChanged();
            },
            [this]()
            {
                HandleSystemSettingsChanged();
            });
    }

    void DockRuntime::ApplyIconCache(std::vector<DockWMac::dock::DockItem>& items)
    {
        for (auto& item : items)
        {
            auto const source = FirstNonEmpty({ item.iconPath, item.targetPath, item.linkPath });
            auto iconPath = DockWMac::shell::CacheIconForPath(source, m_paths.iconCacheDir, item.id);
            if (!iconPath.empty())
            {
                item.iconPath = std::move(iconPath);
            }
        }
    }

    void DockRuntime::HandleDockAction(DockWMac::dock::DockAction const& action)
    {
        switch (action.kind)
        {
        case DockWMac::dock::DockActionKind::Launch:
        case DockWMac::dock::DockActionKind::LaunchNewInstance:
            if (auto it = std::find_if(m_items.begin(), m_items.end(), [&](auto const& item) { return item.id == action.itemId; }); it != m_items.end())
            {
                if (DockWMac::shell::LaunchPinnedApp({
                    it->displayName,
                    it->linkPath,
                    it->targetPath,
                    it->arguments,
                    it->appUserModelId,
                    it->iconPath,
                }))
                {
                    ScheduleDockRefresh(std::chrono::milliseconds{ 750 });
                }
                else
                {
                    auto const displayName = it->displayName.empty() ? std::wstring{ L"app" } : it->displayName;
                    if (m_dockHost)
                    {
                        m_dockHost->ShowStatusMessage(L"Could not launch " + displayName, true);
                    }
                    DockWMac::infra::LogLine(
                        m_paths,
                        "Launch failed for " + winrt::to_string(winrt::hstring{ displayName }));
                }
            }
            break;
        case DockWMac::dock::DockActionKind::ActivateWindow:
            DockWMac::shell::ActivateWindow(action.hwnd);
            ScheduleDockRefresh(std::chrono::milliseconds{ 250 });
            break;
        case DockWMac::dock::DockActionKind::MinimizeWindow:
            DockWMac::shell::MinimizeWindow(action.hwnd);
            ScheduleDockRefresh(std::chrono::milliseconds{ 250 });
            break;
        case DockWMac::dock::DockActionKind::CloseWindow:
            HandleCloseWindow(action.hwnd);
            break;
        case DockWMac::dock::DockActionKind::CloseAllWindows:
            HandleCloseAllWindows(action.itemId);
            break;
        case DockWMac::dock::DockActionKind::PinToDock:
            HandlePinToDock(action.itemId);
            break;
        case DockWMac::dock::DockActionKind::UnpinFromDock:
            HandleUnpinFromDock(action.itemId);
            break;
        case DockWMac::dock::DockActionKind::ToggleAutoHide:
        case DockWMac::dock::DockActionKind::PlaceBottom:
        case DockWMac::dock::DockActionKind::PlaceLeft:
        case DockWMac::dock::DockActionKind::PlaceRight:
        {
            if (ApplyDockSettingsAction(m_settings, action.kind))
            {
                DockWMac::infra::SaveAppSettings(m_paths, m_settings);
                ConfigureDockWindow();
                if (action.kind == DockWMac::dock::DockActionKind::ToggleAutoHide)
                {
                    if (m_dockHost)
                    {
                        m_dockHost->ShowStatusMessage(
                            m_settings.autoHide ? L"Automatic hiding enabled" : L"Automatic hiding disabled");
                    }
                    DockWMac::infra::LogLine(
                        m_paths,
                        m_settings.autoHide ? "Dock auto-hide enabled." : "Dock auto-hide disabled.");
                }
                else
                {
                    DockWMac::infra::LogLine(m_paths, "Dock placement updated.");
                }
            }
            break;
        }
        case DockWMac::dock::DockActionKind::ExitDock:
            if (m_dockHost && m_dockHost->WindowHandle())
            {
                PostMessageW(m_dockHost->WindowHandle(), WM_CLOSE, 0, 0);
            }
            break;
        case DockWMac::dock::DockActionKind::ShowWindowChooser:
        case DockWMac::dock::DockActionKind::None:
        default:
            break;
        }
    }

    void DockRuntime::HandleDockOrderChanged(std::vector<std::wstring> const& order)
    {
        m_dockState.order = PersistentOrderFromItems(order, m_items);
        DockWMac::dock::SaveDockState(m_paths.dockStateFile, m_dockState);
        DockWMac::infra::LogLine(m_paths, "Dock order updated.");

        std::vector<DockWMac::dock::DockItem> ordered;
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

    void DockRuntime::HandlePinToDock(std::wstring const& itemId)
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

        DockWMac::dock::SaveDockState(m_paths.dockStateFile, m_dockState);
        DockWMac::infra::LogLine(m_paths, "Dock item pinned locally.");
        RefreshDockItems(false);
    }

    void DockRuntime::HandleUnpinFromDock(std::wstring const& itemId)
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

        DockWMac::dock::SaveDockState(m_paths.dockStateFile, m_dockState);
        DockWMac::infra::LogLine(m_paths, "Dock item unpinned locally.");
        RefreshDockItems(false);
    }

    void DockRuntime::HandleCloseWindow(HWND hwnd)
    {
        if (!DockWMac::shell::RequestCloseWindow(hwnd) && m_dockHost)
        {
            m_dockHost->ShowStatusMessage(L"Windows did not allow this window to close", true);
        }
        ScheduleDockRefresh(std::chrono::milliseconds{ 500 });
    }

    void DockRuntime::HandleCloseAllWindows(std::wstring const& itemId)
    {
        auto const item = std::find_if(m_items.begin(), m_items.end(), [&](auto const& candidate)
        {
            return candidate.id == itemId;
        });
        if (item == m_items.end())
        {
            return;
        }

        size_t requested{};
        for (auto const& window : item->windows)
        {
            if (DockWMac::shell::RequestCloseWindow(window.hwnd))
            {
                ++requested;
            }
        }

        if (requested == 0 && m_dockHost)
        {
            m_dockHost->ShowStatusMessage(L"Windows did not allow these windows to close", true);
        }
        ScheduleDockRefresh(std::chrono::milliseconds{ 500 });
    }

    void DockRuntime::ShowDockPreview(DockWMac::dock::DockWindowRef const& window)
    {
        if (!m_previewHost)
        {
            return;
        }

        const DockWMac::shell::PreviewSource source{
            window.hwnd,
            window.title,
            window.minimized,
            window.cloaked,
        };
        if (!m_previewHost->Show(source))
        {
            DockWMac::infra::LogLine(m_paths, "DWM preview unavailable for window.");
        }
    }

    bool DockRuntime::ShowDockWindowGroupPreview(std::vector<DockWMac::dock::DockWindowRef> const& windows)
    {
        if (!m_previewHost)
        {
            return false;
        }

        std::vector<DockWMac::shell::PreviewSource> sources;
        sources.reserve(windows.size());
        for (auto const& window : windows)
        {
            sources.push_back({
                window.hwnd,
                window.title,
                window.minimized,
                window.cloaked,
            });
        }

        if (!m_previewHost->ShowGroup(sources))
        {
            DockWMac::infra::LogLine(m_paths, "DWM grouped preview unavailable for windows.");
            return false;
        }
        return true;
    }

    void DockRuntime::HideDockPreview()
    {
        if (m_previewHost)
        {
            m_previewHost->RequestHide();
        }
    }

    void DockRuntime::HandleShellEnvironmentChanged()
    {
        DockWMac::infra::LogLine(m_paths, "Shell environment changed; refreshing taskbar pins and windows.");
        LoadSystemPinnedSnapshot();
        RefreshDockItems(false);
    }

    void DockRuntime::HandleSystemSettingsChanged()
    {
        DockWMac::infra::LogLine(m_paths, "System appearance or accessibility settings changed; reconfiguring Dock.");
        ConfigureDockWindow();
    }
}
