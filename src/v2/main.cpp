#include "pch.h"
#include "app/App.xaml.h"

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
    {
        winrt::make<winrt::DockWMac::implementation::App>();
    });
    return 0;
}
