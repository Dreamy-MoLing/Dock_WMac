#pragma once

#include "ShellTypes.h"

namespace DockWMac::shell
{
    std::vector<PinnedApp> ReadTaskbarPinnedItems();
    std::vector<WindowInfo> EnumerateTopLevelWindows();
    std::vector<WindowInfo> EnumerateTopLevelWindowsForDiagnostics();

    bool LaunchPinnedApp(PinnedApp const& app);
    bool ActivateWindow(HWND hwnd);
    bool MinimizeWindow(HWND hwnd);
    bool RequestCloseWindow(HWND hwnd);
    std::wstring CacheIconForPath(
        std::wstring const& sourcePath,
        std::filesystem::path const& cacheDir,
        std::wstring const& cacheKey);
}
