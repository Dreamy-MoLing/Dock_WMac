#pragma once

#include "App.g.h"
#include "SingleInstanceGuard.h"
#include "../dock/DockModel.h"
#include "../infra/AppSettings.h"

namespace winrt::DockWMac::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

    private:
        void HandleDockAction(::DockWMac::dock::DockAction const& action);
        void HandleDockOrderChanged(std::vector<std::wstring> const& order);

        ::DockWMac::app::SingleInstanceGuard m_instance;
        ::DockWMac::infra::RuntimePaths m_paths;
        ::DockWMac::infra::AppSettings m_settings;
        std::vector<::DockWMac::dock::DockItem> m_items;
        winrt::Microsoft::UI::Xaml::Window m_window{ nullptr };
    };
}

namespace winrt::DockWMac::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
