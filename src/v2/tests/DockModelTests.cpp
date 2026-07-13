#include "pch.h"
#include "../app/DockSettingsActions.h"
#include "../dock/DockModel.h"
#include "../ui/DockIconLayout.h"

namespace
{
    int g_failures = 0;

    HWND TestHwnd(uintptr_t value)
    {
        return reinterpret_cast<HWND>(value);
    }

    void Expect(bool condition, char const* message)
    {
        if (!condition)
        {
            ++g_failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    DockWMac::shell::PinnedApp Pin(std::wstring name, std::wstring targetPath, std::wstring appUserModelId = {},
                                   std::wstring linkPath = {})
    {
        DockWMac::shell::PinnedApp app;
        app.name = std::move(name);
        app.targetPath = std::move(targetPath);
        app.appUserModelId = std::move(appUserModelId);
        app.linkPath = std::move(linkPath);
        return app;
    }

    DockWMac::shell::WindowInfo Window(HWND hwnd, std::wstring title, std::wstring executablePath,
                                       std::wstring appUserModelId = {}, bool foreground = false)
    {
        DockWMac::shell::WindowInfo window;
        window.hwnd = hwnd;
        window.title = std::move(title);
        window.executablePath = std::move(executablePath);
        window.appUserModelId = std::move(appUserModelId);
        window.foreground = foreground;
        window.isTaskbarCandidate = true;
        return window;
    }

    void SystemPinnedAndRunningMerge()
    {
        using namespace DockWMac;

        auto pinned = Pin(L"Example", L"C:\\Apps\\Example\\app.exe");
        auto running = Window(TestHwnd(0x1001), L"Example Window", L"C:\\Apps\\Example\\APP.exe");

        auto items = dock::BuildDockItems({ pinned }, { running }, {});

        Expect(items.size() == 1, "system pin and running window should merge");
        Expect(items[0].pinned, "merged item should remain pinned");
        Expect(items[0].systemPinned, "merged item should keep systemPinned");
        Expect(items[0].running, "merged item should be running");
        Expect(items[0].windows.size() == 1, "merged item should contain one window");
    }

    void ExplicitPinnedIdentityMatchesWindowExecutableFallback()
    {
        using namespace DockWMac;

        auto pinned = Pin(L"Example", L"C:\\Apps\\Example\\app.exe", L"Vendor.Example");
        auto running = Window(TestHwnd(0x1101), L"Example Window", L"c:\\apps\\example\\APP.EXE");

        auto items = dock::BuildDockItems({ pinned }, { running }, {});

        Expect(items.size() == 1, "AUMID pin should absorb a window that only exposes the same executable");
        Expect(items[0].id == L"vendor.example", "merged item should keep the pinned AUMID as canonical identity");
        Expect(items[0].pinned && items[0].running, "AUMID-to-executable match should keep one running pin");
        Expect(items[0].windows.size() == 1, "AUMID-to-executable match should attach the running window");
    }

    void PinnedIconExecutableMatchesWindowWhenShortcutTargetIsEmpty()
    {
        using namespace DockWMac;

        auto pinned = Pin(L"File Explorer", L"", L"Microsoft.Windows.Explorer");
        pinned.iconPath = L"C:\\Windows\\explorer.exe";
        auto running = Window(TestHwnd(0x1102), L"Explorer", L"c:\\windows\\EXPLORER.EXE");

        auto items = dock::BuildDockItems({ pinned }, { running }, {});

        Expect(items.size() == 1, "AUMID pin with an executable icon source should absorb its window");
        Expect(items[0].id == L"microsoft.windows.explorer", "Explorer merge should preserve its pinned identity");
        Expect(items[0].pinned && items[0].running, "Explorer should render as one running pinned item");
    }

    void PathIdentifiedPinAdoptsExplicitWindowIdentity()
    {
        using namespace DockWMac;

        auto pinned = Pin(L"Example", L"C:\\Apps\\Example\\app.exe");
        auto running = Window(TestHwnd(0x1105), L"Example Window", L"c:\\apps\\example\\APP.EXE", L"Vendor.Example");

        auto items = dock::BuildDockItems({ pinned }, { running }, {});

        Expect(items.size() == 1, "path-identified pin should absorb its explicitly identified window");
        Expect(items[0].pinned && items[0].running, "launched window should update the original pin");
        Expect(items[0].windows.size() == 1, "running window should attach to the original pin");
        Expect(items[0].appUserModelId == L"Vendor.Example", "merged item should retain the observed window AUMID");
    }

    void ExplicitWindowIdentityDoesNotMergeByExecutable()
    {
        using namespace DockWMac;

        auto pinned = Pin(L"Mode A", L"C:\\Apps\\Host\\host.exe", L"Vendor.ModeA");
        auto running = Window(TestHwnd(0x1103), L"Mode B", L"C:\\Apps\\Host\\host.exe", L"Vendor.ModeB");

        auto items = dock::BuildDockItems({ pinned }, { running }, {});

        Expect(items.size() == 2, "different explicit AUMIDs must remain separate taskbar groups");
        Expect(items[0].pinned && !items[0].running, "pinned explicit group should remain distinct");
        Expect(!items[1].pinned && items[1].running, "running explicit group should remain transient");
    }

    void AmbiguousExecutableAliasesDoNotPickAnArbitraryPin()
    {
        using namespace DockWMac;

        auto modeA = Pin(L"Mode A", L"C:\\Apps\\Host\\host.exe", L"Vendor.ModeA");
        auto modeB = Pin(L"Mode B", L"C:\\Apps\\Host\\host.exe", L"Vendor.ModeB");
        auto running = Window(TestHwnd(0x1104), L"Unknown mode", L"C:\\Apps\\Host\\host.exe");

        auto items = dock::BuildDockItems({ modeA, modeB }, { running }, {});

        Expect(items.size() == 3, "ambiguous executable fallback should not merge "
                                  "into an arbitrary explicit group");
        Expect(items[2].transientRunningOnly && items[2].running,
               "ambiguous executable fallback should remain a visible running-only "
               "item");
    }

    void ExplorerRefreshPreservesDockOwnedState()
    {
        using namespace DockWMac;

        dock::DockState state;
        state.order = { L"local", L"old-system" };
        state.importedTaskbarPins = { Pin(L"Old system", L"C:\\Apps\\Old\\app.exe") };
        state.localPins = { Pin(L"Local", L"C:\\Apps\\Local\\app.exe") };
        state.hiddenSystemPins = { L"hidden-system" };

        auto const originalOrder = state.order;
        auto const originalLocalTarget = state.localPins.front().targetPath;
        auto const originalHidden = state.hiddenSystemPins;
        auto const unchanged = dock::ApplyImportedTaskbarPins(state, state.importedTaskbarPins);
        Expect(!unchanged, "identical Explorer pin snapshot should not rewrite Dock state");

        auto const changed = dock::ApplyImportedTaskbarPins(state, { Pin(L"New system", L"C:\\Apps\\New\\app.exe") });
        Expect(changed, "changed Explorer pin snapshot should be applied");
        Expect(state.importedTaskbarPins.size() == 1, "Explorer refresh should replace only the imported snapshot");
        Expect(state.importedTaskbarPins.front().targetPath == L"C:\\Apps\\New\\app.exe",
               "Explorer refresh should store new system pins");
        Expect(state.order == originalOrder, "Explorer refresh should preserve Dock order");
        Expect(state.localPins.size() == 1 && state.localPins.front().targetPath == originalLocalTarget,
               "Explorer refresh should preserve local pins");
        Expect(state.hiddenSystemPins == originalHidden, "Explorer refresh should preserve hidden system pins");
    }

    void HiddenSystemPinStillAllowsVisibleRunningItem()
    {
        using namespace DockWMac;

        auto pinned = Pin(L"Hidden", L"C:\\Apps\\Hidden\\hidden.exe");
        auto id = dock::IdentityForPinned(pinned);

        dock::DockState state;
        state.hiddenSystemPins.push_back(id);

        auto running = Window(TestHwnd(0x1002), L"Hidden Window", L"C:\\Apps\\Hidden\\hidden.exe");
        auto items = dock::BuildDockItems({ pinned }, { running }, state);

        Expect(items.size() == 1, "hidden system pin should not hide a visible running window");
        Expect(!items[0].pinned, "hidden system pin should not remain pinned");
        Expect(items[0].transientRunningOnly, "hidden system pin should become running-only");
        Expect(items[0].running, "hidden system pin running item should be running");
    }

    void LocalPinsParticipateInOrdering()
    {
        using namespace DockWMac;

        auto systemPin = Pin(L"System", L"C:\\Apps\\System\\system.exe");
        auto localPin = Pin(L"Local", L"C:\\Apps\\Local\\local.exe");

        dock::DockState state;
        state.localPins.push_back(localPin);
        state.order = {
            dock::IdentityForPinned(localPin),
            dock::IdentityForPinned(systemPin),
        };

        auto items = dock::BuildDockItems({ systemPin }, {}, state);

        Expect(items.size() == 2, "system and local pins should both appear");
        Expect(items[0].localPinned, "local pin should follow persisted order");
        Expect(items[1].systemPinned, "system pin should follow persisted order");
    }

    void MultiWindowClickShowsChooser()
    {
        using namespace DockWMac;

        dock::DockItem item;
        item.id = L"multi";
        item.running = true;
        item.windows.push_back({ TestHwnd(0x2001), L"One", false, false, false });
        item.windows.push_back({ TestHwnd(0x2002), L"Two", false, false, false });

        auto action = dock::DecideClickAction(item);

        Expect(action.kind == dock::DockActionKind::ShowWindowChooser, "multi-window click should show chooser");
    }

    void ForegroundClickMinimizesAndBackgroundClickActivates()
    {
        using namespace DockWMac;

        dock::DockItem foreground;
        foreground.id = L"foreground";
        foreground.running = true;
        foreground.foreground = true;
        foreground.windows.push_back({ TestHwnd(0x3001), L"Foreground", false, false, true });

        auto minimize = dock::DecideClickAction(foreground);
        Expect(minimize.kind == dock::DockActionKind::MinimizeWindow, "foreground click should minimize");
        Expect(minimize.hwnd == TestHwnd(0x3001), "foreground minimize should target the window");

        dock::DockItem background;
        background.id = L"background";
        background.running = true;
        background.windows.push_back({ TestHwnd(0x3002), L"Background", false, false, false });

        auto activate = dock::DecideClickAction(background);
        Expect(activate.kind == dock::DockActionKind::ActivateWindow, "background click should activate");
        Expect(activate.hwnd == TestHwnd(0x3002), "background activate should target the window");
    }

    void MinimizedSingleWindowClickActivatesForRestore()
    {
        using namespace DockWMac;

        dock::DockItem item;
        item.id = L"minimized";
        item.running = true;
        item.windows.push_back({ TestHwnd(0x3003), L"Minimized", true, false, false });

        auto action = dock::DecideClickAction(item);

        Expect(action.kind == dock::DockActionKind::ActivateWindow,
               "minimized single-window click should activate for restore");
        Expect(action.hwnd == TestHwnd(0x3003), "minimized restore should target the minimized window");
    }

    void PinnedWithoutWindowLaunches()
    {
        using namespace DockWMac;

        dock::DockItem item;
        item.id = L"launch";
        item.pinned = true;
        item.running = false;

        auto action = dock::DecideClickAction(item);

        Expect(action.kind == dock::DockActionKind::Launch, "non-running pinned item should launch");
        Expect(action.itemId == L"launch", "launch action should carry item id");
    }

    void WindowTitleIsNotIdentityFallback()
    {
        using namespace DockWMac;

        auto titledWindow = Window(TestHwnd(0x4001), L"Only Title", L"");
        auto id = dock::IdentityForWindow(titledWindow);
        auto items = dock::BuildDockItems({}, { titledWindow }, {});

        Expect(id.empty(), "window identity should not fall back to title");
        Expect(items.empty(), "title-only window should not create a Dock item");
    }

    DockWMac::dock::DockItem TestItem(std::wstring id)
    {
        DockWMac::dock::DockItem item;
        item.id = std::move(id);
        return item;
    }

    void MoveDockItemReordersWithinBounds()
    {
        using namespace DockWMac;

        std::vector<dock::DockItem> items{
            TestItem(L"one"),
            TestItem(L"two"),
            TestItem(L"three"),
            TestItem(L"four"),
        };

        auto movedForward = dock::MoveDockItem(items, 0, 2);
        Expect(movedForward, "move forward should report changed");
        Expect(items[0].id == L"two", "move forward should shift preceding item");
        Expect(items[1].id == L"three", "move forward should keep target order");
        Expect(items[2].id == L"one", "move forward should place dragged item at requested index");

        auto movedBackward = dock::MoveDockItem(items, 3, 1);
        Expect(movedBackward, "move backward should report changed");
        Expect(items[1].id == L"four", "move backward should place dragged item at requested index");
        Expect(items[2].id == L"three", "move backward should shift displaced item");

        auto same = dock::MoveDockItem(items, 1, 1);
        Expect(!same, "same-index move should not report changed");
        Expect(items[1].id == L"four", "same-index move should preserve order");

        auto outOfRange = dock::MoveDockItem(items, 10, 0);
        Expect(!outOfRange, "out-of-range move should not report changed");
        Expect(items.size() == 4, "out-of-range move should not drop items");
    }

    void HoverPoseBottomTangentClamp()
    {
        using namespace DockWMac;

        ui::DockIconMetrics metrics;
        const auto shelfTop = 120.0;
        const auto center = 80.0;
        const auto pose =
            ui::CalculateDockIconPose(platform::DockPlacement::Bottom, center, center, shelfTop, true, false, metrics);

        Expect(std::abs(pose.scale - metrics.maxMagnification) < 0.0001,
               "bottom hover should reach max scale at pointer center");
        Expect(std::abs(pose.visualBottom - shelfTop) < 0.0001,
               "bottom max hover should clamp icon circle to shelf top");

        const auto reducedMotionPose =
            ui::CalculateDockIconPose(platform::DockPlacement::Bottom, center, center, shelfTop, true, true, metrics);
        Expect(std::abs(reducedMotionPose.scale - pose.scale) < 0.0001,
               "reduced motion should preserve hover terminal scale");
        Expect(std::abs(reducedMotionPose.visualBottom - shelfTop) < 0.0001,
               "reduced motion should preserve tangent clamp");
    }

    void HoverAnimationIsBoundedAndPreservesGeometry()
    {
        using namespace DockWMac;

        double amount{};
        auto previous = amount;
        for (int frame = 0; frame < 12; ++frame)
        {
            amount = ui::AdvanceDockHoverAmount(amount, true, 16.0, false);
            Expect(amount >= previous && amount <= 1.0, "hover entry animation should be monotonic and bounded");
            previous = amount;
        }
        Expect(amount > 0.98, "hover entry animation should approach its terminal pose promptly");

        const auto exitAmount = ui::AdvanceDockHoverAmount(amount, false, 16.0, false);
        Expect(exitAmount < amount && exitAmount >= 0.0, "hover exit animation should move toward rest");
        Expect(ui::AdvanceDockHoverAmount(0.35, true, 1.0, true) == 1.0,
               "reduced motion should enter the terminal pose immediately");
        Expect(ui::AdvanceDockHoverAmount(0.65, false, 1.0, true) == 0.0,
               "reduced motion should return to rest immediately");

        ui::DockIconMetrics metrics;
        const auto partialPose =
            ui::CalculateDockIconPose(platform::DockPlacement::Bottom, 100.0, 100.0, 140.0, false, false, metrics, 0.5);
        Expect(partialPose.scale > 1.0 && partialPose.scale < metrics.maxMagnification,
               "partial hover amount should produce an intermediate scale");
        Expect(std::abs(partialPose.visualBottom - 140.0) < 0.0001,
               "every hover animation frame should preserve the shelf tangent");
    }

    void HoverPoseSideTangents()
    {
        using namespace DockWMac;

        ui::DockIconMetrics metrics;
        const auto center = 96.0;
        const auto leftShelfEdge = 44.0;
        const auto leftPose = ui::CalculateDockIconPose(platform::DockPlacement::Left, center, center, leftShelfEdge,
                                                        true, false, metrics);
        Expect(std::abs(leftPose.visualRight - leftShelfEdge) < 0.0001,
               "left max hover should clamp icon circle to shelf edge");

        const auto rightShelfEdge = 44.0;
        const auto rightPose = ui::CalculateDockIconPose(platform::DockPlacement::Right, center, center, rightShelfEdge,
                                                         true, false, metrics);
        Expect(std::abs(rightPose.visualLeft - rightShelfEdge) < 0.0001,
               "right max hover should clamp icon circle to shelf edge");
    }

    void DockAxisHelpersRotateWithPlacement()
    {
        using namespace DockWMac;

        Expect(!ui::IsVerticalDockPlacement(platform::DockPlacement::Bottom),
               "bottom placement should use horizontal main axis");
        Expect(ui::IsVerticalDockPlacement(platform::DockPlacement::Left),
               "left placement should use vertical main axis");
        Expect(ui::IsVerticalDockPlacement(platform::DockPlacement::Right),
               "right placement should use vertical main axis");

        Expect(ui::DockMainAxisPosition(platform::DockPlacement::Bottom, 40.0, 80.0) == 40.0,
               "bottom main axis should be x");
        Expect(ui::DockMainAxisPosition(platform::DockPlacement::Left, 40.0, 80.0) == 80.0,
               "left main axis should be y");
        Expect(ui::DockMainAxisPosition(platform::DockPlacement::Right, 40.0, 80.0) == 80.0,
               "right main axis should be y");

        Expect(ui::DockMainAxisExtent(platform::DockPlacement::Bottom, 320.0, 160.0) == 320.0,
               "bottom main extent should be width");
        Expect(ui::DockMainAxisExtent(platform::DockPlacement::Left, 320.0, 160.0) == 160.0,
               "left main extent should be height");
        Expect(ui::DockCrossAxisExtent(platform::DockPlacement::Right, 320.0, 160.0) == 320.0,
               "right cross extent should be width");
    }

    void AutoHideTriggerRectRotatesWithPlacement()
    {
        using namespace DockWMac;

        const platform::DockRect full{ 100, 200, 320, 96 };
        const auto bottom = platform::CalculateDockAutoHideRectFromDockRect(full, platform::DockPlacement::Bottom, 8);
        Expect(bottom.x == 100, "bottom auto-hide trigger should keep dock x");
        Expect(bottom.y == 288, "bottom auto-hide trigger should sit on bottom edge");
        Expect(bottom.width == 320, "bottom auto-hide trigger should keep dock width");
        Expect(bottom.height == 8, "bottom auto-hide trigger should use requested thickness");

        const auto left = platform::CalculateDockAutoHideRectFromDockRect(full, platform::DockPlacement::Left, 10);
        Expect(left.x == 100, "left auto-hide trigger should sit on left edge");
        Expect(left.y == 200, "left auto-hide trigger should keep dock y");
        Expect(left.width == 10, "left auto-hide trigger should use requested thickness");
        Expect(left.height == 96, "left auto-hide trigger should keep dock height");

        const auto right = platform::CalculateDockAutoHideRectFromDockRect(full, platform::DockPlacement::Right, 12);
        Expect(right.x == 408, "right auto-hide trigger should sit on right edge");
        Expect(right.y == 200, "right auto-hide trigger should keep dock y");
        Expect(right.width == 12, "right auto-hide trigger should use requested thickness");
        Expect(right.height == 96, "right auto-hide trigger should keep dock height");

        const auto clamped =
            platform::CalculateDockAutoHideRectFromDockRect(full, platform::DockPlacement::Bottom, 400);
        Expect(clamped.y == 200, "oversized bottom trigger should clamp to full dock height");
        Expect(clamped.height == 96, "oversized trigger should not exceed dock extent");
    }

    void DpiScalingAndWorkAreaPlacementAreDeterministic()
    {
        using namespace DockWMac;

        Expect(platform::ScaleForDpi(96, 56) == 56, "100 percent DPI should preserve logical size");
        Expect(platform::ScaleForDpi(120, 56) == 70, "125 percent DPI should scale logical size");
        Expect(platform::ScaleForDpi(144, 56) == 84, "150 percent DPI should scale logical size");
        Expect(platform::ScaleForDpi(192, 56) == 112, "200 percent DPI should scale logical size");
        Expect(platform::ScaleForDpi(0, 56) == 56, "zero DPI should fall back to 100 percent");

        const platform::DockRect workArea{ -1920, 0, 1920, 1040 };
        const auto bottom =
            platform::CalculateDockRectForWorkArea(workArea, platform::DockPlacement::Bottom, 720, 96, 24);
        Expect(bottom.x == -1320 && bottom.y == 920, "bottom Dock should center in offset work area");
        Expect(bottom.width == 720 && bottom.height == 96, "bottom Dock should preserve requested extent");

        const auto left = platform::CalculateDockRectForWorkArea(workArea, platform::DockPlacement::Left, 120, 800, 24);
        Expect(left.x == -1896 && left.y == 120, "left Dock should align to offset work area");

        const auto right =
            platform::CalculateDockRectForWorkArea(workArea, platform::DockPlacement::Right, 120, 800, 24);
        Expect(right.x == -144 && right.y == 120, "right Dock should align to offset work area");
    }

    void DockSettingsActionsAreDeterministic()
    {
        using namespace DockWMac;

        infra::AppSettings settings;
        Expect(!settings.autoHide, "auto-hide should default off");
        Expect(app::ApplyDockSettingsAction(settings, dock::DockActionKind::ToggleAutoHide),
               "auto-hide command should change settings");
        Expect(settings.autoHide, "auto-hide command should enable auto-hide");
        Expect(app::ApplyDockSettingsAction(settings, dock::DockActionKind::ToggleAutoHide),
               "second auto-hide command should change settings");
        Expect(!settings.autoHide, "second auto-hide command should disable auto-hide");

        Expect(app::ApplyDockSettingsAction(settings, dock::DockActionKind::PlaceLeft),
               "left placement command should change bottom placement");
        Expect(settings.placement == platform::DockPlacement::Left,
               "left placement command should select left placement");
        Expect(!app::ApplyDockSettingsAction(settings, dock::DockActionKind::PlaceLeft),
               "repeating the active placement should be a no-op");
        Expect(app::ApplyDockSettingsAction(settings, dock::DockActionKind::PlaceRight),
               "right placement command should change left placement");
        Expect(settings.placement == platform::DockPlacement::Right,
               "right placement command should select right placement");
        Expect(app::ApplyDockSettingsAction(settings, dock::DockActionKind::PlaceBottom),
               "bottom placement command should change right placement");
        Expect(settings.placement == platform::DockPlacement::Bottom,
               "bottom placement command should restore bottom placement");
        Expect(!app::ApplyDockSettingsAction(settings, dock::DockActionKind::ExitDock),
               "non-settings commands should not mutate settings");
        Expect(!app::ApplyDockSettingsAction(settings, dock::DockActionKind::LaunchNewInstance),
               "new-instance commands should not mutate settings");
    }
} // namespace

int main()
{
    SystemPinnedAndRunningMerge();
    ExplicitPinnedIdentityMatchesWindowExecutableFallback();
    PinnedIconExecutableMatchesWindowWhenShortcutTargetIsEmpty();
    PathIdentifiedPinAdoptsExplicitWindowIdentity();
    ExplicitWindowIdentityDoesNotMergeByExecutable();
    AmbiguousExecutableAliasesDoNotPickAnArbitraryPin();
    ExplorerRefreshPreservesDockOwnedState();
    HiddenSystemPinStillAllowsVisibleRunningItem();
    LocalPinsParticipateInOrdering();
    MultiWindowClickShowsChooser();
    ForegroundClickMinimizesAndBackgroundClickActivates();
    MinimizedSingleWindowClickActivatesForRestore();
    PinnedWithoutWindowLaunches();
    WindowTitleIsNotIdentityFallback();
    MoveDockItemReordersWithinBounds();
    HoverPoseBottomTangentClamp();
    HoverAnimationIsBoundedAndPreservesGeometry();
    HoverPoseSideTangents();
    DockAxisHelpersRotateWithPlacement();
    AutoHideTriggerRectRotatesWithPlacement();
    DpiScalingAndWorkAreaPlacementAreDeterministic();
    DockSettingsActionsAreDeterministic();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " test failure(s)\n";
        return EXIT_FAILURE;
    }

    std::cout << "DockModel tests passed\n";
    return EXIT_SUCCESS;
}
