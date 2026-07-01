#include "pch.h"
#include "App.xaml.h"
#include "../ui/DockWindow.xaml.h"

#if __has_include("App.xaml.g.cpp")
#include "App.xaml.g.cpp"
#endif

namespace winrt::DockWMac::implementation
{
    App::App()
    {
    }

    void App::OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&)
    {
        m_window = winrt::make<DockWindow>();
        m_window.Activate();
    }
}
