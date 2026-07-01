#pragma once

#include "DockWindow.g.h"

namespace winrt::DockWMac::implementation
{
    struct DockWindow : DockWindowT<DockWindow>
    {
        DockWindow();
    };
}

namespace winrt::DockWMac::factory_implementation
{
    struct DockWindow : DockWindowT<DockWindow, implementation::DockWindow>
    {
    };
}
