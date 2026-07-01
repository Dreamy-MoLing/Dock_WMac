#pragma once

#include "../shell/ShellTypes.h"

namespace DockWMac::dock
{
    struct DockWindowRef
    {
        HWND hwnd{};
        std::wstring title;
        bool minimized{};
        bool cloaked{};
        bool foreground{};
    };

    struct DockItem
    {
        std::wstring id;
        std::wstring displayName;
        std::wstring linkPath;
        std::wstring targetPath;
        std::wstring arguments;
        std::wstring appUserModelId;
        bool pinned{};
        bool running{};
        bool foreground{};
        std::vector<DockWindowRef> windows;
    };

    struct DockState
    {
        std::vector<std::wstring> order;
    };

    enum class DockActionKind
    {
        None,
        Launch,
        ActivateWindow,
        MinimizeWindow,
        ShowWindowChooser,
    };

    struct DockAction
    {
        DockActionKind kind{ DockActionKind::None };
        std::wstring itemId;
        HWND hwnd{};
    };

    std::wstring IdentityForPinned(shell::PinnedApp const& app);
    std::wstring IdentityForWindow(shell::WindowInfo const& window);

    std::vector<DockItem> BuildDockItems(
        std::vector<shell::PinnedApp> const& pinnedApps,
        std::vector<shell::WindowInfo> const& windows,
        DockState const& state);

    DockAction DecideClickAction(DockItem const& item);
}
