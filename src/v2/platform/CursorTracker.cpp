#include "pch.h"
#include "CursorTracker.h"

namespace DockWMac::platform
{
    std::optional<RelativeCursorPosition> GetCursorPositionRelativeToWindow(
        HWND hwnd,
        double logicalWidth,
        double logicalHeight)
    {
        if (!hwnd)
        {
            return std::nullopt;
        }

        POINT cursor{};
        RECT rect{};
        if (!GetCursorPos(&cursor) || !GetWindowRect(hwnd, &rect))
        {
            return std::nullopt;
        }

        if (cursor.x < rect.left || cursor.x >= rect.right || cursor.y < rect.top || cursor.y >= rect.bottom)
        {
            return RelativeCursorPosition{ 0.0, 0.0, false };
        }

        const auto pixelWidth = (std::max)(1L, rect.right - rect.left);
        const auto pixelHeight = (std::max)(1L, rect.bottom - rect.top);
        const auto logicalX = static_cast<double>(cursor.x - rect.left) *
            logicalWidth / static_cast<double>(pixelWidth);
        const auto logicalY = static_cast<double>(cursor.y - rect.top) *
            logicalHeight / static_cast<double>(pixelHeight);

        return RelativeCursorPosition{ logicalX, logicalY, true };
    }
}
