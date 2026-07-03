#include "pch.h"
#include "../dock/DockModel.h"

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

    DockWMac::shell::PinnedApp Pin(
        std::wstring name,
        std::wstring targetPath,
        std::wstring appUserModelId = {},
        std::wstring linkPath = {})
    {
        DockWMac::shell::PinnedApp app;
        app.name = std::move(name);
        app.targetPath = std::move(targetPath);
        app.appUserModelId = std::move(appUserModelId);
        app.linkPath = std::move(linkPath);
        return app;
    }

    DockWMac::shell::WindowInfo Window(
        HWND hwnd,
        std::wstring title,
        std::wstring executablePath,
        std::wstring appUserModelId = {},
        bool foreground = false)
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
}

int main()
{
    SystemPinnedAndRunningMerge();
    HiddenSystemPinStillAllowsVisibleRunningItem();
    LocalPinsParticipateInOrdering();
    MultiWindowClickShowsChooser();
    ForegroundClickMinimizesAndBackgroundClickActivates();
    PinnedWithoutWindowLaunches();
    WindowTitleIsNotIdentityFallback();

    if (g_failures != 0)
    {
        std::cerr << g_failures << " test failure(s)\n";
        return EXIT_FAILURE;
    }

    std::cout << "DockModel tests passed\n";
    return EXIT_SUCCESS;
}
