#include "pch.h"
#include "App.xaml.h"
#include "../dock/DockStateStore.h"
#include "../infra/Logger.h"
#include "../shell/ShellIntegration.h"
#include "../ui/DockWindow.xaml.h"

#if __has_include("App.xaml.g.cpp")
#include "App.xaml.g.cpp"
#endif

namespace winrt::DockWMac::implementation
{
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

        auto dockState = ::DockWMac::dock::LoadDockState(m_paths.dockStateFile);
        m_items = ::DockWMac::dock::BuildDockItems(
            ::DockWMac::shell::ReadTaskbarPinnedItems(),
            ::DockWMac::shell::EnumerateTopLevelWindows(),
            dockState);

        if (dockState.order.empty())
        {
            for (auto const& item : m_items)
            {
                dockState.order.push_back(item.id);
            }
            ::DockWMac::dock::SaveDockState(m_paths.dockStateFile, dockState);
        }

        auto window = winrt::make<DockWindow>();
        winrt::get_self<DockWindow>(window)->Configure(
            m_settings,
            m_items,
            [this](::DockWMac::dock::DockAction const& action)
            {
                HandleDockAction(action);
            },
            [this](std::vector<std::wstring> const& order)
            {
                HandleDockOrderChanged(order);
            });
        m_window = window;
        m_window.Activate();
        winrt::get_self<DockWindow>(window)->ApplyPlacement();
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
                });
            }
            break;
        case ::DockWMac::dock::DockActionKind::ActivateWindow:
            ::DockWMac::shell::ActivateWindow(action.hwnd);
            break;
        case ::DockWMac::dock::DockActionKind::MinimizeWindow:
            ::DockWMac::shell::MinimizeWindow(action.hwnd);
            break;
        case ::DockWMac::dock::DockActionKind::ShowWindowChooser:
        case ::DockWMac::dock::DockActionKind::None:
        default:
            break;
        }
    }

    void App::HandleDockOrderChanged(std::vector<std::wstring> const& order)
    {
        ::DockWMac::dock::DockState dockState;
        dockState.order = order;
        ::DockWMac::dock::SaveDockState(m_paths.dockStateFile, dockState);
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
}
