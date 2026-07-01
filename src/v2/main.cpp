#include "pch.h"
#include "app/App.xaml.h"
#include "infra/SelfCheck.h"

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (commandLine && std::wstring_view{ commandLine }.find(L"--self-check") != std::wstring_view::npos)
    {
        return ::DockWMac::infra::RunSelfCheck();
    }

    winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
    {
        winrt::make<winrt::DockWMac::implementation::App>();
    });
    return 0;
}
