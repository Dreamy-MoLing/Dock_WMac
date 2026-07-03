#pragma once

#include "../shell/ShellTypes.h"

namespace DockWMac::dock
{
    std::wstring IdentityForPinned(shell::PinnedApp const& app);
    std::wstring IdentityForWindow(shell::WindowInfo const& window);
}
