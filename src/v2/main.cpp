#include "pch.h"
#include "app/DockRuntime.h"
#include "infra/DockStateDump.h"
#include "infra/ResourceMetrics.h"
#include "infra/SelfCheck.h"

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if (commandLine && std::wstring_view{ commandLine }.find(L"--self-check") != std::wstring_view::npos)
    {
        return ::DockWMac::infra::RunSelfCheck();
    }
    if (commandLine && std::wstring_view{ commandLine }.find(L"--dump-dock-state") != std::wstring_view::npos)
    {
        return ::DockWMac::infra::RunDockStateDump();
    }
    if (commandLine && std::wstring_view{ commandLine }.find(L"--dump-resource-metrics") != std::wstring_view::npos)
    {
        return ::DockWMac::infra::RunResourceMetricsDump();
    }

    ::DockWMac::app::DockRuntime runtime;
    return runtime.Run();
}
