#include "pch.h"
#include "DockPlacement.h"

namespace DockWMac::platform
{
    std::wstring ToConfigString(DockPlacement placement)
    {
        switch (placement)
        {
        case DockPlacement::Left:
            return L"left";
        case DockPlacement::Right:
            return L"right";
        case DockPlacement::Bottom:
        default:
            return L"bottom";
        }
    }

    DockPlacement PlacementFromConfig(std::wstring_view value)
    {
        if (value == L"left")
        {
            return DockPlacement::Left;
        }
        if (value == L"right")
        {
            return DockPlacement::Right;
        }
        return DockPlacement::Bottom;
    }

    SystemAccessibility ReadSystemAccessibility()
    {
        SystemAccessibility accessibility;

        HIGHCONTRASTW highContrast{ sizeof(highContrast) };
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0))
        {
            accessibility.highContrast = (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
        }

        BOOL clientAreaAnimation = TRUE;
        if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &clientAreaAnimation, 0))
        {
            accessibility.reducedMotion = clientAreaAnimation == FALSE;
        }

        return accessibility;
    }

    int32_t ScaleForWindow(HWND hwnd, int32_t value)
    {
        auto dpi = hwnd ? GetDpiForWindow(hwnd) : GetDpiForSystem();
        if (dpi == 0)
        {
            dpi = USER_DEFAULT_SCREEN_DPI;
        }

        return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
    }

    DockRect CalculateDockRect(HWND hwnd, DockPlacement placement, int32_t width, int32_t height)
    {
        auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info{ sizeof(info) };
        if (!GetMonitorInfoW(monitor, &info))
        {
            info.rcWork = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        }

        const auto margin = ScaleForWindow(hwnd, 12);
        const auto workWidth = static_cast<int32_t>(info.rcWork.right - info.rcWork.left);
        const auto workHeight = static_cast<int32_t>(info.rcWork.bottom - info.rcWork.top);

        DockRect rect{ 0, 0, width, height };
        switch (placement)
        {
        case DockPlacement::Left:
            rect.x = info.rcWork.left + margin;
            rect.y = info.rcWork.top + (workHeight - height) / 2;
            break;
        case DockPlacement::Right:
            rect.x = info.rcWork.right - width - margin;
            rect.y = info.rcWork.top + (workHeight - height) / 2;
            break;
        case DockPlacement::Bottom:
        default:
            rect.x = info.rcWork.left + (workWidth - width) / 2;
            rect.y = info.rcWork.bottom - height - margin;
            break;
        }
        return rect;
    }

    DockRect CalculateDockAutoHideRect(HWND hwnd, DockPlacement placement, int32_t width, int32_t height)
    {
        const auto triggerThickness = ScaleForWindow(hwnd, 8);
        auto rect = CalculateDockRect(hwnd, placement, width, height);

        switch (placement)
        {
        case DockPlacement::Left:
            rect.width = triggerThickness;
            break;
        case DockPlacement::Right:
            rect.x += rect.width - triggerThickness;
            rect.width = triggerThickness;
            break;
        case DockPlacement::Bottom:
        default:
            rect.y += rect.height - triggerThickness;
            rect.height = triggerThickness;
            break;
        }

        return rect;
    }

    void ApplyDockWindowPlacement(HWND hwnd, DockPlacement placement, int32_t width, int32_t height)
    {
        if (!hwnd)
        {
            return;
        }

        const auto rect = CalculateDockRect(hwnd, placement, width, height);
        SetWindowPos(
            hwnd,
            HWND_TOPMOST,
            rect.x,
            rect.y,
            rect.width,
            rect.height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void ApplyDockWindowAutoHidePlacement(HWND hwnd, DockPlacement placement, int32_t width, int32_t height)
    {
        if (!hwnd)
        {
            return;
        }

        const auto rect = CalculateDockAutoHideRect(hwnd, placement, width, height);
        SetWindowPos(
            hwnd,
            HWND_TOPMOST,
            rect.x,
            rect.y,
            rect.width,
            rect.height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}
