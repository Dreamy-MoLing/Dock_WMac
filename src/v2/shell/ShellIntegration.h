#pragma once

#include "ShellTypes.h"

namespace DockWMac::shell
{
    std::vector<PinnedApp> ReadTaskbarPinnedItems();
    std::vector<WindowInfo> EnumerateTopLevelWindows();

    bool LaunchPinnedApp(PinnedApp const& app);
    bool ActivateWindow(HWND hwnd);
    bool MinimizeWindow(HWND hwnd);
    std::wstring CacheIconForPath(
        std::wstring const& sourcePath,
        std::filesystem::path const& cacheDir,
        std::wstring const& cacheKey);
}
