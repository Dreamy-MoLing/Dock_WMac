#include "pch.h"
#include "DockPlacement.h"

namespace DockWMac::platform
{
    namespace
    {
        struct RegionHandle
        {
            HRGN value{};

            ~RegionHandle()
            {
                if (value)
                {
                    DeleteObject(value);
                }
            }

            RegionHandle() = default;
            explicit RegionHandle(HRGN region) : value(region) {}
            RegionHandle(RegionHandle const&) = delete;
            RegionHandle& operator=(RegionHandle const&) = delete;
            RegionHandle(RegionHandle&& other) noexcept : value(other.value)
            {
                other.value = {};
            }
            RegionHandle& operator=(RegionHandle&& other) noexcept
            {
                if (this != &other)
                {
                    if (value)
                    {
                        DeleteObject(value);
                    }
                    value = other.value;
                    other.value = {};
                }
                return *this;
            }

            HRGN release()
            {
                auto region = value;
                value = {};
                return region;
            }
        };

        void SuppressDwmBorder(HWND hwnd)
        {
            constexpr DWORD borderColorAttribute = DWMWA_BORDER_COLOR;
            constexpr COLORREF noBorder = 0xFFFFFFFE;
            DwmSetWindowAttribute(hwnd, borderColorAttribute, &noBorder, sizeof(noBorder));
        }

        constexpr int32_t DockItemExtent = 66;
        constexpr int32_t DockItemGap = 10;
        constexpr int32_t DockEndPadding = 34;
        constexpr int32_t DockShelfMargin = 14;
        constexpr int32_t DockShelfThickness = 47;
        constexpr int32_t DockShelfRadius = 18;
        constexpr int32_t DockCellHeight = 72;
        constexpr int32_t DockRowBottomMargin = 22;
        constexpr int32_t DockIconSize = 56;
        constexpr int32_t DockGlyphBottomInset = 8;
        constexpr double DockMaxMagnification = 1.68;
        constexpr double DockHoverLift = 12.0;
        constexpr double DockActiveLift = 6.0;
        constexpr int32_t RegionPad = 4;

        int32_t DockItemsLength(HWND hwnd, size_t visibleItemCount)
        {
            if (visibleItemCount == 0)
            {
                return ScaleForWindow(hwnd, DockEndPadding * 2);
            }

            return ScaleForWindow(
                hwnd,
                DockEndPadding * 2 +
                    static_cast<int32_t>(visibleItemCount) * DockItemExtent +
                    static_cast<int32_t>(visibleItemCount - 1) * DockItemGap);
        }

        void UnionRegion(HRGN target, HRGN source)
        {
            if (target && source)
            {
                CombineRgn(target, target, source, RGN_OR);
            }
        }

        void AddRoundRect(HRGN target, int32_t left, int32_t top, int32_t right, int32_t bottom, int32_t radius)
        {
            RegionHandle region{ CreateRoundRectRgn(
                left,
                top,
                (std::max)(left + 1, right) + 1,
                (std::max)(top + 1, bottom) + 1,
                radius,
                radius) };
            UnionRegion(target, region.value);
        }
    }

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

    void ApplyDockWindowSwitcherBehavior(HWND hwnd)
    {
        if (!hwnd)
        {
            return;
        }

        auto exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        exStyle &= ~WS_EX_APPWINDOW;
        exStyle |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

        SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
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

        ApplyDockWindowSwitcherBehavior(hwnd);

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

        ApplyDockWindowSwitcherBehavior(hwnd);

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

    void ApplyDockWindowTransparency(HWND hwnd, bool highContrast)
    {
        UNREFERENCED_PARAMETER(highContrast);

        if (!hwnd)
        {
            return;
        }

        auto exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        exStyle &= ~WS_EX_LAYERED;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

        DWM_BLURBEHIND blur{};
        blur.dwFlags = DWM_BB_ENABLE;
        blur.fEnable = FALSE;
        DwmEnableBlurBehindWindow(hwnd, &blur);

        SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        SuppressDwmBorder(hwnd);
    }

    void ApplyDockWindowShape(HWND hwnd, DockPlacement placement, int32_t width, int32_t height, size_t visibleItemCount)
    {
        if (!hwnd)
        {
            return;
        }

        const auto railLength = DockItemsLength(hwnd, visibleItemCount);
        const auto shelfMargin = ScaleForWindow(hwnd, DockShelfMargin);
        const auto shelfThickness = ScaleForWindow(hwnd, DockShelfThickness);
        const auto shelfRadius = ScaleForWindow(hwnd, DockShelfRadius);
        const auto endPadding = ScaleForWindow(hwnd, DockEndPadding);
        const auto itemExtent = ScaleForWindow(hwnd, DockItemExtent);
        const auto itemGap = ScaleForWindow(hwnd, DockItemGap);
        const auto cellHeight = ScaleForWindow(hwnd, DockCellHeight);
        const auto rowBottom = ScaleForWindow(hwnd, DockRowBottomMargin);
        const auto maxIcon = ScaleForWindow(hwnd, static_cast<int32_t>(std::ceil(DockIconSize * DockMaxMagnification)));
        const auto maxLift = ScaleForWindow(hwnd, static_cast<int32_t>(std::ceil(DockHoverLift + DockActiveLift)));
        const auto pad = ScaleForWindow(hwnd, RegionPad);

        RegionHandle region{ CreateRectRgn(0, 0, 0, 0) };
        if (!region.value)
        {
            SetWindowRgn(hwnd, nullptr, TRUE);
            SuppressDwmBorder(hwnd);
            return;
        }

        int32_t left{};
        int32_t top{};
        int32_t right{};
        int32_t bottom{};
        if (placement == DockPlacement::Bottom)
        {
            left = (std::max)(0, (width - railLength) / 2);
            right = (std::min)(width, left + railLength);
            top = (std::max)(0, height - shelfMargin - shelfThickness);
            bottom = (std::min)(height, top + shelfThickness);
        }
        else
        {
            top = (std::max)(0, (height - railLength) / 2);
            bottom = (std::min)(height, top + railLength);
            left = placement == DockPlacement::Left
                ? (std::max)(0, width - shelfMargin - shelfThickness)
                : shelfMargin;
            right = (std::min)(width, left + shelfThickness);
        }

        AddRoundRect(region.value, left, top, right, bottom, shelfRadius * 2);

        if (visibleItemCount > 0)
        {
            if (placement == DockPlacement::Bottom)
            {
                const auto railLeft = (std::max)(0, (width - railLength) / 2);
                const auto cellTop = height - rowBottom - cellHeight;
                const auto glyphBottom = cellTop + cellHeight - ScaleForWindow(hwnd, DockGlyphBottomInset);
                const auto iconTop = glyphBottom - maxIcon - maxLift - pad;
                const auto iconBottom = glyphBottom + pad;

                for (size_t index = 0; index < visibleItemCount; ++index)
                {
                    const auto slotLeft = railLeft + endPadding + static_cast<int32_t>(index) * (itemExtent + itemGap);
                    const auto centerX = slotLeft + itemExtent / 2;
                    AddRoundRect(
                        region.value,
                        (std::max)(0, centerX - maxIcon / 2 - pad),
                        (std::max)(0, iconTop),
                        (std::min)(width, centerX + maxIcon / 2 + pad),
                        (std::min)(height, iconBottom),
                        shelfRadius);
                }
            }
            else
            {
                const auto railTop = (std::max)(0, (height - railLength) / 2);
                const auto cellLeft = placement == DockPlacement::Left
                    ? width - rowBottom - cellHeight
                    : rowBottom;
                const auto glyphEdge = placement == DockPlacement::Left
                    ? cellLeft + cellHeight - ScaleForWindow(hwnd, DockGlyphBottomInset)
                    : cellLeft + ScaleForWindow(hwnd, DockGlyphBottomInset);
                const auto iconLeft = placement == DockPlacement::Left
                    ? glyphEdge - maxIcon - maxLift - pad
                    : glyphEdge - pad;
                const auto iconRight = placement == DockPlacement::Left
                    ? glyphEdge + pad
                    : glyphEdge + maxIcon + maxLift + pad;

                for (size_t index = 0; index < visibleItemCount; ++index)
                {
                    const auto slotTop = railTop + endPadding + static_cast<int32_t>(index) * (itemExtent + itemGap);
                    const auto centerY = slotTop + itemExtent / 2;
                    AddRoundRect(
                        region.value,
                        (std::max)(0, iconLeft),
                        (std::max)(0, centerY - maxIcon / 2 - pad),
                        (std::min)(width, iconRight),
                        (std::min)(height, centerY + maxIcon / 2 + pad),
                        shelfRadius);
                }
            }
        }

        if (SetWindowRgn(hwnd, region.value, TRUE) != 0)
        {
            region.release();
        }
        SuppressDwmBorder(hwnd);
    }
}
