#pragma once

#include "App.g.h"
#include "SingleInstanceGuard.h"
#include "../dock/DockModel.h"
#include "../infra/AppSettings.h"
#include "../shell/DwmPreviewHost.h"
#include "../shell/ShellTypes.h"

namespace winrt::DockWMac::implementation
{
    struct DockWindow;

    struct App : AppT<App>
    {
        App();

        void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

    private:
        void StartRefreshTimer();
        void LoadSystemPinnedSnapshot();
        void RefreshDockItems(bool initializeOrder);
        void ConfigureDockWindow();
        void ApplyIconCache(std::vector<::DockWMac::dock::DockItem>& items);
        void HandleDockAction(::DockWMac::dock::DockAction const& action);
        void HandleDockOrderChanged(std::vector<std::wstring> const& order);
        void HandlePinToDock(std::wstring const& itemId);
        void HandleUnpinFromDock(std::wstring const& itemId);
        void ShowDockPreview(HWND hwnd);
        void HideDockPreview();

        ::DockWMac::app::SingleInstanceGuard m_instance;
        ::DockWMac::infra::RuntimePaths m_paths;
        ::DockWMac::infra::AppSettings m_settings;
        ::DockWMac::dock::DockState m_dockState;
        std::unique_ptr<::DockWMac::shell::DwmPreviewHost> m_previewHost;
        std::vector<::DockWMac::shell::PinnedApp> m_systemPinnedApps;
        std::vector<::DockWMac::dock::DockItem> m_items;
        winrt::Microsoft::UI::Xaml::Window m_window{ nullptr };
        DockWindow* m_dockWindow{};
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_refreshTimer{ nullptr };
    };
}

namespace winrt::DockWMac::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
