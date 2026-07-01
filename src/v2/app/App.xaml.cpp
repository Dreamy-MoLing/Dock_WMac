#include "pch.h"
#include "App.xaml.h"
#include "../infra/Logger.h"
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

        auto window = winrt::make<DockWindow>();
        winrt::get_self<DockWindow>(window)->Configure(m_settings);
        m_window = window;
        m_window.Activate();
        winrt::get_self<DockWindow>(window)->ApplyPlacement();
    }
}
