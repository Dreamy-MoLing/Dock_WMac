#pragma once

namespace DockWMac::platform
{
    enum class DockPlacement
    {
        Bottom,
        Left,
        Right,
    };

    enum class DockLayerKind
    {
        Combined,
        Shelf,
        Icons,
    };

    struct DockRect
    {
        int32_t x{};
        int32_t y{};
        int32_t width{};
        int32_t height{};
    };

    inline int32_t ClampAutoHideTriggerThickness(int32_t triggerThickness, int32_t extent)
    {
        if (extent <= 0)
        {
            return 0;
        }
        if (triggerThickness < 1)
        {
            return 1;
        }
        return triggerThickness > extent ? extent : triggerThickness;
    }

    inline DockRect CalculateDockAutoHideRectFromDockRect(
        DockRect rect,
        DockPlacement placement,
        int32_t triggerThickness)
    {
        switch (placement)
        {
        case DockPlacement::Left:
        {
            const auto clamped = ClampAutoHideTriggerThickness(triggerThickness, rect.width);
            rect.width = clamped;
            break;
        }
        case DockPlacement::Right:
        {
            const auto clamped = ClampAutoHideTriggerThickness(triggerThickness, rect.width);
            rect.x += rect.width - clamped;
            rect.width = clamped;
            break;
        }
        case DockPlacement::Bottom:
        default:
        {
            const auto clamped = ClampAutoHideTriggerThickness(triggerThickness, rect.height);
            rect.y += rect.height - clamped;
            rect.height = clamped;
            break;
        }
        }

        return rect;
    }

    std::wstring ToConfigString(DockPlacement placement);
    DockPlacement PlacementFromConfig(std::wstring_view value);

    struct SystemAccessibility
    {
        bool highContrast{};
        bool reducedMotion{};
        bool lightTheme{};
    };

    SystemAccessibility ReadSystemAccessibility();
    inline int32_t ScaleForDpi(uint32_t dpi, int32_t value)
    {
        if (dpi == 0)
        {
            dpi = USER_DEFAULT_SCREEN_DPI;
        }
        return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
    }
    int32_t ScaleForWindow(HWND hwnd, int32_t value);
    HMONITOR PrimaryMonitor();
    void ApplyDockWindowSwitcherBehavior(HWND hwnd);

    inline DockRect CalculateDockRectForWorkArea(
        DockRect workArea,
        DockPlacement placement,
        int32_t width,
        int32_t height,
        int32_t margin)
    {
        DockRect rect{ 0, 0, width, height };
        switch (placement)
        {
        case DockPlacement::Left:
            rect.x = workArea.x + margin;
            rect.y = workArea.y + (workArea.height - height) / 2;
            break;
        case DockPlacement::Right:
            rect.x = workArea.x + workArea.width - width - margin;
            rect.y = workArea.y + (workArea.height - height) / 2;
            break;
        case DockPlacement::Bottom:
        default:
            rect.x = workArea.x + (workArea.width - width) / 2;
            rect.y = workArea.y + workArea.height - height - margin;
            break;
        }
        return rect;
    }
    DockRect CalculateDockRect(HWND hwnd, DockPlacement placement, int32_t width, int32_t height);
    DockRect CalculateDockAutoHideRect(HWND hwnd, DockPlacement placement, int32_t width, int32_t height);
    void ApplyDockWindowPlacement(HWND hwnd, DockPlacement placement, int32_t width, int32_t height);
    void ApplyDockWindowAutoHidePlacement(HWND hwnd, DockPlacement placement, int32_t width, int32_t height);
    void ApplyDockWindowTransparency(HWND hwnd, bool highContrast);
    void ApplyDockWindowShape(
        HWND hwnd,
        DockPlacement placement,
        int32_t width,
        int32_t height,
        size_t visibleItemCount,
        DockLayerKind layer = DockLayerKind::Combined);
}
