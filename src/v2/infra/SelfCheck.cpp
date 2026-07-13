#include "pch.h"
#include "AppSettings.h"
#include "ResourceMetrics.h"
#include "SelfCheck.h"
#include "../dock/DockModel.h"
#include "../dock/DockStateStore.h"
#include "../platform/DockPlacement.h"
#include "../render/NativeDockHost.h"
#include "../shell/DwmPreviewHost.h"
#include "../shell/ShellIntegration.h"
#include "../shell/WindowEventMonitor.h"
#include "../ui/DockIconLayout.h"

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

        bool PumpMessagesUntil(std::function<bool()> const& predicate, std::chrono::milliseconds timeout)
        {
            auto const deadline = std::chrono::steady_clock::now() + timeout;
            MSG message{};
            while (!predicate() && std::chrono::steady_clock::now() < deadline)
            {
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                Sleep(5);
            }
            return predicate();
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
        paths.iconCacheDir = temp / L"icons";
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
        dockState.order = { L"second", L"c:\\transient.exe", L"first" };
        dockState.importedTaskbarPins = { { L"Imported", L"imported.lnk", L"C:\\Imported.exe", L"", L"imported", L"imported.lnk" } };
        dockState.localPins = { { L"Local", L"", L"C:\\Local.exe", L"", L"local", L"" } };
        dockState.hiddenSystemPins = { L"hidden" };
        auto backupSeedState = dockState;
        backupSeedState.order = { L"backup", L"first" };
        DockWMac::dock::SaveDockState(paths.dockStateFile, backupSeedState);
        DockWMac::dock::SaveDockState(paths.dockStateFile, dockState);
        auto loadedDockState = DockWMac::dock::LoadDockState(paths.dockStateFile);
        {
            std::ofstream corruptState{ paths.dockStateFile, std::ios::binary | std::ios::trunc };
            corruptState << "{";
        }
        auto recoveredDockState = DockWMac::dock::LoadDockState(paths.dockStateFile);

        std::vector<DockWMac::shell::PinnedApp> pinned{
            { L"First", L"first.lnk", L"C:\\First.exe", L"", L"first", L"first.lnk" },
            { L"Second", L"second.lnk", L"C:\\Second.exe", L"", L"second", L"second.lnk" },
            { L"Hidden", L"hidden.lnk", L"C:\\Hidden.exe", L"", L"hidden", L"hidden.lnk" },
        };
        std::vector<DockWMac::shell::WindowInfo> windows{
            { reinterpret_cast<HWND>(1), 10, L"Second Window", L"C:\\Second.exe", L"second", L"C:\\Second.exe", false, false, true, true },
            { reinterpret_cast<HWND>(2), 11, L"Transient Window", L"C:\\Transient.exe", L"", L"C:\\Transient.exe", false, false, false, true },
            { reinterpret_cast<HWND>(5), 12, L"Background Window", L"C:\\Background.exe", L"", L"C:\\Background.exe", false, false, false, false },
        };
        auto items = DockWMac::dock::BuildDockItems(pinned, windows, dockState);
        auto pathPinIdentityItems = DockWMac::dock::BuildDockItems(
            { { L"Path Pin", L"path-pin.lnk", L"C:\\PathPin.exe", L"", L"", L"C:\\PathPin.exe" } },
            { { reinterpret_cast<HWND>(7), 13, L"Path Pin Window", L"c:\\PATHPIN.EXE", L"Vendor.PathPin",
                L"C:\\PathPin.exe", false, false, false, true } },
            {});
        auto multiAction = DockWMac::dock::DecideClickAction(DockWMac::dock::DockItem{
            L"multi", L"Multi", L"", L"", L"", L"", L"", true, true, false, false, true, false,
            { { reinterpret_cast<HWND>(3), L"A", false, false, false }, { reinterpret_cast<HWND>(4), L"B", false, false, false } },
        });
        auto minimizedAction = DockWMac::dock::DecideClickAction(DockWMac::dock::DockItem{
            L"minimized", L"Minimized", L"", L"", L"", L"", L"", true, true, false, false, true, false,
            { { reinterpret_cast<HWND>(6), L"Minimized", true, false, false } },
        });
        auto bottomHidden = DockWMac::platform::CalculateDockAutoHideRect(nullptr, DockWMac::platform::DockPlacement::Bottom, 800, 120);
        auto leftHidden = DockWMac::platform::CalculateDockAutoHideRect(nullptr, DockWMac::platform::DockPlacement::Left, 120, 800);
        auto rightHidden = DockWMac::platform::CalculateDockAutoHideRect(nullptr, DockWMac::platform::DockPlacement::Right, 120, 800);
        auto triggerThickness = DockWMac::platform::ScaleForWindow(nullptr, 8);
        DockWMac::ui::DockIconMetrics iconMetrics;
        const auto hoverCenter = 96.0;
        const auto shelfEdge = 120.0;
        const auto bottomHoverPose = DockWMac::ui::CalculateDockIconPose(
            DockWMac::platform::DockPlacement::Bottom,
            hoverCenter,
            hoverCenter,
            shelfEdge,
            true,
            false,
            iconMetrics);
        const auto bottomReducedMotionPose = DockWMac::ui::CalculateDockIconPose(
            DockWMac::platform::DockPlacement::Bottom,
            hoverCenter,
            hoverCenter,
            shelfEdge,
            true,
            true,
            iconMetrics);
        const auto leftHoverPose = DockWMac::ui::CalculateDockIconPose(
            DockWMac::platform::DockPlacement::Left,
            hoverCenter,
            hoverCenter,
            shelfEdge,
            true,
            false,
            iconMetrics);
        const auto rightHoverPose = DockWMac::ui::CalculateDockIconPose(
            DockWMac::platform::DockPlacement::Right, hoverCenter, hoverCenter,
            shelfEdge, true, false, iconMetrics);
        auto resourceMetrics =
            DockWMac::infra::CaptureCurrentProcessResourceMetrics(
                std::chrono::milliseconds{50});
        auto resourceMetricsJson =
            DockWMac::infra::ResourceMetricsJson(resourceMetrics);
        auto nativeHostClosePostsQuitOk = true;
        auto nativeHostTaskbarCreatedOk = true;
        auto nativeHostSystemSettingsChangedOk = true;
        auto nativeHostHoverAnimationOk = true;
        auto nativeHostReducedMotionOk = true;
        auto nativeHostCompositionLayersOk = true;
        auto nativeHostPrimaryDisplayOk = true;
        {
          DockWMac::render::NativeDockHost host;
          auto const hwnd = host.WindowHandle();
          nativeHostClosePostsQuitOk = hwnd && IsWindow(hwnd);
          uint32_t shellEnvironmentChangedCount{};
          uint32_t systemSettingsChangedCount{};
          DockWMac::dock::DockItem diagnosticItem;
          diagnosticItem.id = L"hover-animation-self-check";
          diagnosticItem.displayName = L"Hover animation";
          host.Configure(
              {}, {diagnosticItem}, {}, {}, {}, {}, {},
              [&shellEnvironmentChangedCount]() {
                ++shellEnvironmentChangedCount;
              },
              [&systemSettingsChangedCount]() {
                ++systemSettingsChangedCount;
              });
          nativeHostCompositionLayersOk =
              host.HasIndependentCompositionLayersForDiagnostics();
          auto const itemCenter = host.ItemCenterForDiagnostics(0);
          nativeHostHoverAnimationOk = itemCenter.has_value();
          if (itemCenter) {
            host.SetHoveringForDiagnostics(true);
            nativeHostHoverAnimationOk =
                host.HoverAnimationRunningForDiagnostics() &&
                PumpMessagesUntil(
                    [&host]() {
                      return !host.HoverAnimationRunningForDiagnostics();
                    },
                    std::chrono::milliseconds{1000}) &&
                std::abs(host.HoverAmountForDiagnostics() - 1.0) < 0.0001;

            host.SetHoveringForDiagnostics(false);
            nativeHostHoverAnimationOk =
                nativeHostHoverAnimationOk &&
                host.HoverAnimationRunningForDiagnostics() &&
                PumpMessagesUntil(
                    [&host]() {
                      return !host.HoverAnimationRunningForDiagnostics();
                    },
                    std::chrono::milliseconds{1000}) &&
                std::abs(host.HoverAmountForDiagnostics()) < 0.0001;

            DockWMac::infra::AppSettings reducedMotionSettings;
            reducedMotionSettings.reducedMotion = true;
            host.Configure(
                reducedMotionSettings, {diagnosticItem}, {}, {}, {}, {}, {},
                [&shellEnvironmentChangedCount]() {
                  ++shellEnvironmentChangedCount;
                },
                [&systemSettingsChangedCount]() {
                  ++systemSettingsChangedCount;
                });
            host.SetHoveringForDiagnostics(true);
            nativeHostReducedMotionOk =
                !host.HoverAnimationRunningForDiagnostics() &&
                std::abs(host.HoverAmountForDiagnostics() - 1.0) < 0.0001;
            host.SetHoveringForDiagnostics(false);
            nativeHostReducedMotionOk =
                nativeHostReducedMotionOk &&
                !host.HoverAnimationRunningForDiagnostics() &&
                std::abs(host.HoverAmountForDiagnostics()) < 0.0001;
          }
          auto const taskbarCreatedMessage =
              RegisterWindowMessageW(L"TaskbarCreated");
          nativeHostTaskbarCreatedOk =
              taskbarCreatedMessage != 0 &&
              SendMessageW(hwnd, taskbarCreatedMessage, 0, 0) == 0 &&
              shellEnvironmentChangedCount == 1;
          nativeHostSystemSettingsChangedOk =
              SendMessageW(hwnd, WM_SETTINGCHANGE, 0, 0) == 0 &&
              SendMessageW(hwnd, WM_THEMECHANGED, 0, 0) == 0 &&
              systemSettingsChangedCount == 2;
          auto const primaryMonitor = DockWMac::platform::PrimaryMonitor();
          nativeHostPrimaryDisplayOk =
              primaryMonitor && SendMessageW(hwnd, WM_DISPLAYCHANGE, 0, 0) == 0 &&
              MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) == primaryMonitor;
          host.Close();
          MSG quitMessage{};
          nativeHostClosePostsQuitOk =
              nativeHostClosePostsQuitOk && !host.WindowHandle() &&
              !IsWindow(hwnd) &&
              PeekMessageW(&quitMessage, nullptr, WM_QUIT, WM_QUIT,
                           PM_REMOVE) &&
              quitMessage.message == WM_QUIT;
        }
        auto const windowForegroundEventOk =
            DockWMac::shell::IsRefreshWorthyWindowEvent(
                EVENT_SYSTEM_FOREGROUND, OBJID_WINDOW, CHILDID_SELF,
                reinterpret_cast<HWND>(0x10));
        auto const windowMinimizeEventOk =
            DockWMac::shell::IsRefreshWorthyWindowEvent(
                EVENT_SYSTEM_MINIMIZEEND, OBJID_WINDOW, CHILDID_SELF,
                reinterpret_cast<HWND>(0x11));
        auto const childWindowEventIgnored = !DockWMac::shell::IsRefreshWorthyWindowEvent(
            EVENT_OBJECT_SHOW,
            OBJID_WINDOW,
            1,
            reinterpret_cast<HWND>(0x12));
        auto const clientObjectEventIgnored = !DockWMac::shell::IsRefreshWorthyWindowEvent(
            EVENT_OBJECT_SHOW,
            OBJID_CLIENT,
            CHILDID_SELF,
            reinterpret_cast<HWND>(0x13));

        auto enumeratedWindows = DockWMac::shell::EnumerateTopLevelWindows();
        auto previewWindow = std::find_if(enumeratedWindows.begin(), enumeratedWindows.end(), [](auto const& window)
        {
            return window.hwnd && !window.cloaked;
        });
        auto previewOk = true;
        auto previewSingleFallbackOk = true;
        auto previewMinimizedFallbackOk = true;
        auto previewGroupOk = true;
        auto previewSingleActivatableOk = true;
        auto previewGroupActivatableOk = true;
        auto previewMinimizedThumbnailOk = true;
        auto previewMinimizedHoverDoesNotRestoreOk = true;
        auto previewMinimizedClickRestoresOk = true;
        auto previewTwoWindowGroupOk = true;
        auto previewGroupMinimizedHoverDoesNotRestoreOk = true;
        auto previewGroupClickSelectsTargetOk = true;
        auto closeWindowRequestOk = true;
        auto invalidCloseWindowRejected = !DockWMac::shell::RequestCloseWindow(reinterpret_cast<HWND>(0x1));
        {
            auto closeSourceWindow = CreateWindowExW(
                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                L"STATIC",
                L"Dock_WMac close request self-check",
                WS_OVERLAPPEDWINDOW,
                -32000,
                -32000,
                240,
                160,
                nullptr,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr);
            closeWindowRequestOk = closeSourceWindow && DockWMac::shell::RequestCloseWindow(closeSourceWindow);
            if (closeWindowRequestOk)
            {
                MSG message{};
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                closeWindowRequestOk = !IsWindow(closeSourceWindow);
            }
            if (closeSourceWindow && IsWindow(closeSourceWindow))
            {
                DestroyWindow(closeSourceWindow);
            }
        }
        {
            DockWMac::shell::DwmPreviewHost preview;
            previewSingleFallbackOk = preview.Show({ reinterpret_cast<HWND>(0x1), L"Unavailable", false, true });
            preview.Hide();
        }
        {
            DockWMac::shell::DwmPreviewHost preview;
            previewMinimizedFallbackOk = preview.Show({ reinterpret_cast<HWND>(0x2), L"Minimized", true, false });
            preview.Hide();
        }
        if (previewWindow != enumeratedWindows.end())
        {
            DockWMac::shell::DwmPreviewHost preview;
            previewOk = preview.Show(previewWindow->hwnd);
            previewSingleActivatableOk = preview.ActivatableTargetCountForDiagnostics() > 0;
            preview.Hide();
            previewGroupOk = preview.ShowGroup({
                { previewWindow->hwnd, previewWindow->title, previewWindow->minimized, previewWindow->cloaked },
                { reinterpret_cast<HWND>(0x1), L"Unavailable", false, true },
            });
            previewGroupActivatableOk = preview.ActivatableTargetCountForDiagnostics() > 0;
            preview.Hide();
        }
        {
            auto sourceWindow = CreateWindowExW(
                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                L"STATIC",
                L"Dock_WMac minimized preview self-check",
                WS_OVERLAPPEDWINDOW,
                -32000,
                -32000,
                320,
                200,
                nullptr,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr);
            if (!sourceWindow)
            {
                previewMinimizedThumbnailOk = false;
                previewMinimizedHoverDoesNotRestoreOk = false;
                previewMinimizedClickRestoresOk = false;
            }
            else
            {
                ShowWindow(sourceWindow, SW_SHOWNOACTIVATE);
                ShowWindow(sourceWindow, SW_MINIMIZE);

                HWND activatedWindow{};
                DockWMac::shell::DwmPreviewHost preview;
                preview.SetActivateHandler([&activatedWindow](HWND hwnd)
                {
                    activatedWindow = hwnd;
                    DockWMac::shell::ActivateWindow(hwnd);
                });
                auto const shown = preview.Show({ sourceWindow, L"Minimized preview self-check", true, false });
                auto const activationPoint = preview.ActivatablePointForDiagnostics(0);
                previewMinimizedThumbnailOk = shown &&
                    preview.RegisteredThumbnailCountForDiagnostics() == 1 &&
                    preview.ActivatableTargetCountForDiagnostics() == 1;
                previewMinimizedHoverDoesNotRestoreOk = shown && IsIconic(sourceWindow);

                if (activationPoint && preview.WindowHandleForDiagnostics())
                {
                    SendMessageW(
                        preview.WindowHandleForDiagnostics(),
                        WM_LBUTTONUP,
                        0,
                        MAKELPARAM(activationPoint->x, activationPoint->y));
                }
                previewMinimizedClickRestoresOk = activatedWindow == sourceWindow && !IsIconic(sourceWindow);
                preview.HideImmediate();
                DestroyWindow(sourceWindow);
            }
        }
        {
            auto firstSourceWindow = CreateWindowExW(
                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                L"STATIC",
                L"Dock_WMac preview group first",
                WS_OVERLAPPEDWINDOW,
                -32000,
                -32000,
                360,
                220,
                nullptr,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr);
            auto secondSourceWindow = CreateWindowExW(
                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                L"STATIC",
                L"Dock_WMac preview group second",
                WS_OVERLAPPEDWINDOW,
                -32000,
                -32000,
                300,
                180,
                nullptr,
                nullptr,
                GetModuleHandleW(nullptr),
                nullptr);
            if (!firstSourceWindow || !secondSourceWindow)
            {
                previewTwoWindowGroupOk = false;
                previewGroupMinimizedHoverDoesNotRestoreOk = false;
                previewGroupClickSelectsTargetOk = false;
            }
            else
            {
                ShowWindow(firstSourceWindow, SW_SHOWNOACTIVATE);
                ShowWindow(secondSourceWindow, SW_SHOWNOACTIVATE);
                ShowWindow(secondSourceWindow, SW_MINIMIZE);

                HWND activatedWindow{};
                DockWMac::shell::DwmPreviewHost preview;
                preview.SetActivateHandler([&activatedWindow](HWND hwnd)
                {
                    activatedWindow = hwnd;
                    DockWMac::shell::ActivateWindow(hwnd);
                });
                auto const shown = preview.ShowGroup({
                    { firstSourceWindow, L"First preview", false, false },
                    { secondSourceWindow, L"Second minimized preview", true, false },
                });
                auto const secondActivationPoint = preview.ActivatablePointForDiagnostics(1);
                previewTwoWindowGroupOk = shown &&
                    preview.RegisteredThumbnailCountForDiagnostics() == 2 &&
                    preview.ActivatableTargetCountForDiagnostics() == 2;
                previewGroupMinimizedHoverDoesNotRestoreOk = shown && IsIconic(secondSourceWindow);

                if (secondActivationPoint && preview.WindowHandleForDiagnostics())
                {
                    SendMessageW(
                        preview.WindowHandleForDiagnostics(),
                        WM_LBUTTONUP,
                        0,
                        MAKELPARAM(secondActivationPoint->x, secondActivationPoint->y));
                }
                previewGroupClickSelectsTargetOk = activatedWindow == secondSourceWindow &&
                    !IsIconic(secondSourceWindow);
                preview.HideImmediate();
            }

            if (firstSourceWindow)
            {
                DestroyWindow(firstSourceWindow);
            }
            if (secondSourceWindow)
            {
                DestroyWindow(secondSourceWindow);
            }
        }

        wchar_t windowsDir[MAX_PATH]{};
        GetWindowsDirectoryW(windowsDir, MAX_PATH);
        auto explorerPath = std::filesystem::path{ windowsDir } / L"explorer.exe";
        auto iconOk = true;
        if (std::filesystem::exists(explorerPath))
        {
            auto iconPath = DockWMac::shell::CacheIconForPath(explorerPath.wstring(), paths.iconCacheDir, L"explorer");
            iconOk = !iconPath.empty() && std::filesystem::exists(iconPath);
        }

        std::vector<std::string> failures;
        Check(failures, "settings.placement", loaded.placement == DockWMac::platform::DockPlacement::Right);
        Check(failures, "settings.autoHide", loaded.autoHide);
        Check(failures, "settings.reducedMotion", loaded.reducedMotion);
        Check(failures, "settings.dockWidth", loaded.dockWidth == 800);
        Check(failures, "settings.dockHeight", loaded.dockHeight == 120);
        Check(failures, "dockState.order.size", loadedDockState.order.size() == 3);
        Check(failures, "dockState.order.first", !loadedDockState.order.empty() && loadedDockState.order.front() == L"second");
        Check(failures, "dockState.importedTaskbarPins", loadedDockState.importedTaskbarPins.size() == 1);
        Check(failures, "dockState.localPins", loadedDockState.localPins.size() == 1);
        Check(failures, "dockState.hiddenSystemPins", loadedDockState.hiddenSystemPins.size() == 1);
        Check(failures, "dockState.backupRecovery", !recoveredDockState.order.empty() && recoveredDockState.order.front() == L"backup");
        Check(failures, "dockModel.items.size", items.size() == 4);
        Check(failures, "dockModel.first.id", !items.empty() && items.front().id == L"second");
        Check(failures, "dockModel.first.running", !items.empty() && items.front().running);
        Check(failures, "dockModel.first.foreground", !items.empty() && items.front().foreground);
        Check(failures, "dockModel.hidden", std::none_of(items.begin(), items.end(), [](auto const& item) { return item.id == L"hidden"; }));
        Check(failures, "dockModel.localPinned", std::any_of(items.begin(), items.end(), [](auto const& item) { return item.id == L"local" && item.localPinned; }));
        Check(failures, "dockModel.transient.visible", std::any_of(items.begin(), items.end(), [](auto const& item) { return item.id == L"c:\\transient.exe" && item.running && item.transientRunningOnly && !item.pinned; }));
        Check(failures, "dockModel.transient.afterPinned", items.size() == 4 && items.back().id == L"c:\\transient.exe");
        Check(failures, "dockModel.background.filtered", std::none_of(items.begin(), items.end(), [](auto const& item) { return item.id == L"c:\\background.exe"; }));
        Check(
            failures,
            "dockModel.pathPinExplicitWindowIdentity",
            pathPinIdentityItems.size() == 1 && pathPinIdentityItems.front().pinned &&
                pathPinIdentityItems.front().running && pathPinIdentityItems.front().windows.size() == 1 &&
                pathPinIdentityItems.front().appUserModelId == L"Vendor.PathPin");
        Check(failures, "dockModel.click.foreground", !items.empty() && DockWMac::dock::DecideClickAction(items.front()).kind == DockWMac::dock::DockActionKind::MinimizeWindow);
        Check(failures, "dockModel.click.multi", multiAction.kind == DockWMac::dock::DockActionKind::ShowWindowChooser);
        Check(failures, "dockModel.click.minimizedRestore", minimizedAction.kind == DockWMac::dock::DockActionKind::ActivateWindow && minimizedAction.hwnd == reinterpret_cast<HWND>(6));
        Check(failures, "platform.placement.left", DockWMac::platform::PlacementFromConfig(L"left") == DockWMac::platform::DockPlacement::Left);
        Check(failures, "platform.placement.fallback",
              DockWMac::platform::PlacementFromConfig(L"bad") ==
                  DockWMac::platform::DockPlacement::Bottom);
        Check(failures, "platform.autohide.bottom",
              bottomHidden.width == 800 &&
                  bottomHidden.height == triggerThickness);
        Check(failures, "platform.autohide.left",
              leftHidden.width == triggerThickness && leftHidden.height == 800);
        Check(failures, "platform.autohide.right",
              rightHidden.width == triggerThickness &&
                  rightHidden.height == 800);
        Check(failures, "platform.dpi.scale", triggerThickness > 0);
        Check(failures, "ui.hover.bottom.scale",
              std::abs(bottomHoverPose.scale - iconMetrics.maxMagnification) <
                  0.0001);
        Check(failures, "ui.hover.bottom.tangent",
              std::abs(bottomHoverPose.visualBottom - shelfEdge) < 0.0001);
        Check(failures, "ui.hover.bottom.centerline",
              std::abs(bottomHoverPose.tangentX - hoverCenter) < 0.0001);
        Check(failures, "ui.hover.reducedMotion.scale",
              std::abs(bottomReducedMotionPose.scale - bottomHoverPose.scale) <
                  0.0001);
        Check(failures, "ui.hover.reducedMotion.tangent",
              std::abs(bottomReducedMotionPose.visualBottom - shelfEdge) <
                  0.0001);
        Check(failures, "ui.hover.left.tangent",
              std::abs(leftHoverPose.visualRight - shelfEdge) < 0.0001);
        Check(failures, "ui.hover.right.tangent",
              std::abs(rightHoverPose.visualLeft - shelfEdge) < 0.0001);
        Check(failures, "resourceMetrics.processId",
              resourceMetrics.processId == GetCurrentProcessId());
        Check(failures, "resourceMetrics.workingSet",
              resourceMetrics.workingSetBytes > 0);
        Check(failures, "resourceMetrics.privateBytes",
              resourceMetrics.privateBytes > 0);
        Check(failures, "resourceMetrics.handleCount",
              resourceMetrics.handleCount > 0);
        Check(failures, "resourceMetrics.threadCount",
              resourceMetrics.threadCount > 0);
        Check(failures, "resourceMetrics.cpuUsage",
              std::isfinite(resourceMetrics.cpuUsagePercent) &&
                  resourceMetrics.cpuUsagePercent >= 0.0);
        Check(failures, "resourceMetrics.sample",
              resourceMetrics.sampleMilliseconds >= 25);
        Check(failures, "resourceMetrics.json",
              resourceMetricsJson.HasKey(L"workingSetBytes") &&
                  resourceMetricsJson.HasKey(L"cpuUsagePercent"));
        Check(failures, "render.nativeHost.closePostsQuit",
              nativeHostClosePostsQuitOk);
        Check(failures, "render.nativeHost.taskbarCreated",
              nativeHostTaskbarCreatedOk);
        Check(failures, "render.nativeHost.systemSettingsChanged",
              nativeHostSystemSettingsChangedOk);
        Check(failures, "render.nativeHost.hoverAnimation",
              nativeHostHoverAnimationOk);
        Check(failures, "render.nativeHost.reducedMotion",
              nativeHostReducedMotionOk);
        Check(failures, "render.nativeHost.compositionLayers",
              nativeHostCompositionLayersOk);
        Check(failures, "render.nativeHost.primaryDisplay",
              nativeHostPrimaryDisplayOk);
        Check(failures, "shell.windowEvents.foreground",
              windowForegroundEventOk);
        Check(failures, "shell.windowEvents.minimize", windowMinimizeEventOk);
        Check(failures, "shell.windowEvents.childIgnored",
              childWindowEventIgnored);
        Check(failures, "shell.windowEvents.clientIgnored",
              clientObjectEventIgnored);
        Check(failures, "shell.closeWindow.request", closeWindowRequestOk);
        Check(failures, "shell.closeWindow.invalidRejected",
              invalidCloseWindowRejected);
        Check(failures, "shell.enumerate.safe",
              enumeratedWindows.size() < 10000);
        Check(failures, "shell.dwmPreview", previewOk);
        Check(failures, "shell.dwmPreview.singleFallback",
              previewSingleFallbackOk);
        Check(failures, "shell.dwmPreview.minimizedFallback",
              previewMinimizedFallbackOk);
        Check(failures, "shell.dwmPreview.group", previewGroupOk);
        Check(failures, "shell.dwmPreview.singleActivatable", previewSingleActivatableOk);
        Check(failures, "shell.dwmPreview.groupActivatable", previewGroupActivatableOk);
        Check(failures, "shell.dwmPreview.minimizedThumbnail", previewMinimizedThumbnailOk);
        Check(failures, "shell.dwmPreview.minimizedHoverDoesNotRestore", previewMinimizedHoverDoesNotRestoreOk);
        Check(failures, "shell.dwmPreview.minimizedClickRestores", previewMinimizedClickRestoresOk);
        Check(failures, "shell.dwmPreview.twoWindowGroup", previewTwoWindowGroupOk);
        Check(failures, "shell.dwmPreview.groupMinimizedHoverDoesNotRestore", previewGroupMinimizedHoverDoesNotRestoreOk);
        Check(failures, "shell.dwmPreview.groupClickSelectsTarget", previewGroupClickSelectsTargetOk);
        Check(failures, "shell.iconCache", iconOk);

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
