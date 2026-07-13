#pragma once

namespace DockWMac::platform
{
    struct RelativeCursorPosition
    {
        double x{};
        double y{};
        bool insideWindow{};
    };

    std::optional<RelativeCursorPosition> GetCursorPositionRelativeToWindow(
        HWND hwnd,
        double logicalWidth,
        double logicalHeight);
}
