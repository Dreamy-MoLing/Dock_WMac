#include "pch.h"
#include "AppSettings.h"
#include "SelfCheck.h"
#include "../platform/DockPlacement.h"

namespace DockWMac::infra
{
    namespace
    {
        bool Check(bool value)
        {
            return value;
        }
    }

    int RunSelfCheck()
    {
        auto temp = std::filesystem::temp_directory_path() / L"Dock_WMac_v2_selfcheck";
        std::filesystem::remove_all(temp);

        RuntimePaths paths;
        paths.userDataDir = temp;
        paths.configFile = temp / L"settings.json";
        paths.logDir = temp / L"logs";
        paths.logFile = paths.logDir / L"dock.log";

        AppSettings settings;
        settings.placement = DockWMac::platform::DockPlacement::Right;
        settings.autoHide = true;
        settings.reducedMotion = true;
        settings.dockWidth = 800;
        settings.dockHeight = 120;

        SaveAppSettings(paths, settings);
        auto loaded = LoadAppSettings(paths);

        const auto ok =
            Check(loaded.placement == DockWMac::platform::DockPlacement::Right) &&
            Check(loaded.autoHide) &&
            Check(loaded.reducedMotion) &&
            Check(loaded.dockWidth == 800) &&
            Check(loaded.dockHeight == 120) &&
            Check(DockWMac::platform::PlacementFromConfig(L"left") == DockWMac::platform::DockPlacement::Left) &&
            Check(DockWMac::platform::PlacementFromConfig(L"bad") == DockWMac::platform::DockPlacement::Bottom);

        std::filesystem::remove_all(temp);
        return ok ? 0 : 1;
    }
}
