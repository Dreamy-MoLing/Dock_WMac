#pragma once

#include "../platform/DockPlacement.h"
#include "RuntimePaths.h"

namespace DockWMac::infra
{
    struct AppSettings
    {
        DockWMac::platform::DockPlacement placement{ DockWMac::platform::DockPlacement::Bottom };
        bool autoHide{ false };
        bool reducedMotion{ false };
        bool highContrast{ false };
        int32_t dockWidth{ 720 };
        int32_t dockHeight{ 96 };
    };

    AppSettings LoadAppSettings(RuntimePaths const& paths);
    void SaveAppSettings(RuntimePaths const& paths, AppSettings const& settings);
}
