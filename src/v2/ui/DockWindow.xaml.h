#pragma once

#include "DockWindow.g.h"
#include "../infra/AppSettings.h"

namespace winrt::DockWMac::implementation
{
    struct DockWindow : DockWindowT<DockWindow>
    {
        DockWindow();

        void Configure(::DockWMac::infra::AppSettings const& settings);
        void ApplyPlacement();

    private:
        HWND WindowHandle() const;
        void ConfigurePresenter();
        void BuildContent();

        ::DockWMac::infra::AppSettings m_settings;
    };
}

namespace winrt::DockWMac::factory_implementation
{
    struct DockWindow : DockWindowT<DockWindow, implementation::DockWindow>
    {
    };
}
