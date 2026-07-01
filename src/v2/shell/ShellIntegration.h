#pragma once

#include "ShellTypes.h"

namespace DockWMac::shell
{
    std::vector<PinnedApp> ReadTaskbarPinnedItems();
    std::vector<WindowInfo> EnumerateTopLevelWindows();

    bool LaunchPinnedApp(PinnedApp const& app);
    bool ActivateWindow(HWND hwnd);
}
