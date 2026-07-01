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
        void Check(std::vector<std::string>& failures, std::string name, bool value)
        {
            if (!value)
            {
                failures.push_back(std::move(name));
            }
        }
    }

    int RunSelfCheck()
    {
        auto temp = std::filesystem::temp_directory_path() / L"Dock_WMac_v2_selfcheck";
        std::filesystem::remove_all(temp);

        RuntimePaths paths;
        paths.userDataDir = temp;
        paths.configFile = temp / L"settings.json";
        paths.dockStateFile = temp / L"dock_state.json";
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
        DockWMac::dock::SaveDockState(paths.dockStateFile, dockState);
        auto loadedDockState = DockWMac::dock::LoadDockState(paths.dockStateFile);

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

        auto enumeratedWindows = DockWMac::shell::EnumerateTopLevelWindows();

        std::vector<std::string> failures;
        Check(failures, "settings.placement", loaded.placement == DockWMac::platform::DockPlacement::Right);
        Check(failures, "settings.autoHide", loaded.autoHide);
        Check(failures, "settings.reducedMotion", loaded.reducedMotion);
        Check(failures, "settings.dockWidth", loaded.dockWidth == 800);
        Check(failures, "settings.dockHeight", loaded.dockHeight == 120);
        Check(failures, "dockState.order.size", loadedDockState.order.size() == 2);
        Check(failures, "dockState.order.first", !loadedDockState.order.empty() && loadedDockState.order.front() == L"second");
        Check(failures, "dockModel.items.size", items.size() == 3);
        Check(failures, "dockModel.first.id", !items.empty() && items.front().id == L"second");
        Check(failures, "dockModel.first.running", !items.empty() && items.front().running);
        Check(failures, "dockModel.first.foreground", !items.empty() && items.front().foreground);
        Check(failures, "dockModel.click.foreground", !items.empty() && DockWMac::dock::DecideClickAction(items.front()).kind == DockWMac::dock::DockActionKind::MinimizeWindow);
        Check(failures, "dockModel.click.multi", multiAction.kind == DockWMac::dock::DockActionKind::ShowWindowChooser);
        Check(failures, "platform.placement.left", DockWMac::platform::PlacementFromConfig(L"left") == DockWMac::platform::DockPlacement::Left);
        Check(failures, "platform.placement.fallback", DockWMac::platform::PlacementFromConfig(L"bad") == DockWMac::platform::DockPlacement::Bottom);
        Check(failures, "shell.enumerate.safe", enumeratedWindows.size() < 10000);

        if (failures.empty())
        {
            std::filesystem::remove_all(temp);
            return 0;
        }

        std::filesystem::create_directories(temp);
        std::ofstream output{ temp / L"selfcheck_failures.txt", std::ios::binary | std::ios::trunc };
        for (auto const& failure : failures)
        {
            output << failure << "\n";
        }
        return 1;
    }
}
