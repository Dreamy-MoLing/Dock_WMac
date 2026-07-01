#include "pch.h"
#include "DockWindow.xaml.h"

#if __has_include("DockWindow.g.cpp")
#include "DockWindow.g.cpp"
#endif

namespace winrt::DockWMac::implementation
{
    DockWindow::DockWindow()
    {
        Title(L"DockWindow");
        ExtendsContentIntoTitleBar(true);

        auto root = winrt::Microsoft::UI::Xaml::Controls::Grid{};
        root.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{
            winrt::Windows::UI::Color{ 0xAA, 0x10, 0x18, 0x20 }
        });
        Content(root);

        if (auto window = AppWindow())
        {
            window.Resize({ 720, 96 });
        }
    }
}
