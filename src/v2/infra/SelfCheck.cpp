#include "pch.h"
#include "AppSettings.h"
#include "SelfCheck.h"
#include "../dock/DockModel.h"
#include "../dock/DockStateStore.h"
#include "../platform/DockPlacement.h"
#include "../shell/ShellIntegration.h"

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

        DockWMac::dock::DockState dockState;
        dockState.order = { L"second", L"first" };
        auto dockStatePath = temp / L"dock_state.json";
        DockWMac::dock::SaveDockState(dockStatePath, dockState);
        auto loadedDockState = DockWMac::dock::LoadDockState(dockStatePath);

        std::vector<DockWMac::shell::PinnedApp> pinned{
            { L"First", L"first.lnk", L"C:\\First.exe", L"", L"first" },
            { L"Second", L"second.lnk", L"C:\\Second.exe", L"", L"second" },
        };
        std::vector<DockWMac::shell::WindowInfo> windows{
            { reinterpret_cast<HWND>(1), 10, L"Second Window", L"C:\\Second.exe", L"second", false, false, true },
            { reinterpret_cast<HWND>(2), 11, L"Transient Window", L"C:\\Transient.exe", L"", false, false, false },
        };
        auto items = DockWMac::dock::BuildDockItems(pinned, windows, dockState);
        auto multiAction = DockWMac::dock::DecideClickAction(DockWMac::dock::DockItem{
            L"multi", L"Multi", L"", L"", L"", L"", true, true, false,
            { { reinterpret_cast<HWND>(3), L"A", false, false, false }, { reinterpret_cast<HWND>(4), L"B", false, false, false } },
        });

        const auto ok =
            Check(loaded.placement == DockWMac::platform::DockPlacement::Right) &&
            Check(loaded.autoHide) &&
            Check(loaded.reducedMotion) &&
            Check(loaded.dockWidth == 800) &&
            Check(loaded.dockHeight == 120) &&
            Check(loadedDockState.order.size() == 2) &&
            Check(loadedDockState.order.front() == L"second") &&
            Check(items.size() == 3) &&
            Check(items.front().id == L"second") &&
            Check(items.front().running) &&
            Check(items.front().foreground) &&
            Check(DockWMac::dock::DecideClickAction(items.front()).kind == DockWMac::dock::DockActionKind::ActivateWindow) &&
            Check(multiAction.kind == DockWMac::dock::DockActionKind::ShowWindowChooser) &&
            Check(DockWMac::platform::PlacementFromConfig(L"left") == DockWMac::platform::DockPlacement::Left) &&
            Check(DockWMac::platform::PlacementFromConfig(L"bad") == DockWMac::platform::DockPlacement::Bottom) &&
            Check(!DockWMac::shell::EnumerateTopLevelWindows().empty());

        std::filesystem::remove_all(temp);
        return ok ? 0 : 1;
    }
}
