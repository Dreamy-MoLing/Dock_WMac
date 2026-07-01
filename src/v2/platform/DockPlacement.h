#pragma once

namespace DockWMac::platform
{
    enum class DockPlacement
    {
        Bottom,
        Left,
        Right,
    };

    struct DockRect
    {
        int32_t x{};
        int32_t y{};
        int32_t width{};
        int32_t height{};
    };

    std::wstring ToConfigString(DockPlacement placement);
    DockPlacement PlacementFromConfig(std::wstring_view value);

    DockRect CalculateDockRect(HWND hwnd, DockPlacement placement, int32_t width, int32_t height);
    DockRect CalculateDockAutoHideRect(HWND hwnd, DockPlacement placement, int32_t width, int32_t height);
    void ApplyDockWindowPlacement(HWND hwnd, DockPlacement placement, int32_t width, int32_t height);
    void ApplyDockWindowAutoHidePlacement(HWND hwnd, DockPlacement placement, int32_t width, int32_t height);
}
