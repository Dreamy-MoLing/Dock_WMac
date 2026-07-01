#include "pch.h"
#include "DockWindow.xaml.h"
#include "../platform/DockPlacement.h"

#if __has_include("DockWindow.g.cpp")
#include "DockWindow.g.cpp"
#endif

namespace winrt::DockWMac::implementation
{
    DockWindow::DockWindow()
    {
        Title(L"DockWindow");
        ExtendsContentIntoTitleBar(true);
        ConfigurePresenter();
        BuildContent();
    }

    void DockWindow::Configure(::DockWMac::infra::AppSettings const& settings)
    {
        m_settings = settings;
        if (auto window = AppWindow())
        {
            window.Resize({ m_settings.dockWidth, m_settings.dockHeight });
        }
    }

    void DockWindow::ApplyPlacement()
    {
        ::DockWMac::platform::ApplyDockWindowPlacement(
            WindowHandle(),
            m_settings.placement,
            m_settings.dockWidth,
            m_settings.dockHeight);
    }

    HWND DockWindow::WindowHandle() const
    {
        HWND hwnd{};
        auto native = this->try_as<IWindowNative>();
        if (native)
        {
            winrt::check_hresult(native->get_WindowHandle(&hwnd));
        }
        return hwnd;
    }

    void DockWindow::ConfigurePresenter()
    {
        if (auto window = AppWindow())
        {
            if (auto presenter = window.Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
            {
                presenter.SetBorderAndTitleBar(false, false);
                presenter.IsMaximizable(false);
                presenter.IsMinimizable(false);
                presenter.IsResizable(false);
            }
            window.Resize({ m_settings.dockWidth, m_settings.dockHeight });
        }
    }

    void DockWindow::BuildContent()
    {
        auto root = winrt::Microsoft::UI::Xaml::Controls::Grid{};
        root.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{
            winrt::Windows::UI::Color{ 0xAA, 0x10, 0x18, 0x20 }
        });
        Content(root);
    }
}
