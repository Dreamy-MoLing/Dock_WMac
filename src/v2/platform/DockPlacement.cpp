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

        constexpr int32_t DockItemExtent = 66;
        constexpr int32_t DockItemGap = 10;
        constexpr int32_t DockEndPadding = 34;
        constexpr int32_t DockCrossAxis = 176;
        constexpr int32_t DockShelfBottomMargin = 14;
        constexpr int32_t DockShelfThickness = 47;
        constexpr int32_t DockRowBottomMargin = 22;
        constexpr int32_t DockCellHeight = 72;
        constexpr int32_t DockIconSize = 56;
        constexpr int32_t DockGlyphBottomInset = 8;
        constexpr double DockMaxMagnification = 1.68;
        constexpr double DockHoverLift = 12.0;
        constexpr double DockActiveLift = 6.0;
        constexpr double DockMagnificationRange = DockIconSize * 2.2;
        constexpr double Pi = 3.14159265358979323846;

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

        void SuppressDwmBorder(HWND hwnd)
        {
            constexpr DWORD borderColorAttribute = DWMWA_BORDER_COLOR;
            constexpr COLORREF noBorder = 0xFFFFFFFE;
            DwmSetWindowAttribute(hwnd, borderColorAttribute, &noBorder, sizeof(noBorder));
        }

        void UnionRegion(HRGN target, HRGN source)
        {
            if (target && source)
            {
                CombineRgn(target, target, source, RGN_OR);
            }
        }

        double MagnificationCurve(double distance)
        {
            const auto t = std::clamp(1.0 - distance / DockMagnificationRange, 0.0, 1.0);
            return 0.5 - 0.5 * std::cos(t * Pi);
        }

        RegionHandle BuildDockWindowRegion(
            HWND hwnd,
            DockPlacement placement,
            int32_t width,
            int32_t height,
            size_t visibleItemCount,
            double hoverAxis,
            bool hoverActive)
        {
            RegionHandle region{ CreateRectRgn(0, 0, 0, 0) };
            if (!region.value || visibleItemCount == 0)
            {
                return region;
            }

            const auto railLength = DockItemsLength(hwnd, visibleItemCount);
            const auto shelfBottom = ScaleForWindow(hwnd, DockShelfBottomMargin);
            const auto shelfThickness = ScaleForWindow(hwnd, DockShelfThickness);
            const auto shelfRadius = ScaleForWindow(hwnd, 18);
            const auto itemExtent = ScaleForWindow(hwnd, DockItemExtent);
            const auto itemGap = ScaleForWindow(hwnd, DockItemGap);
            const auto endPadding = ScaleForWindow(hwnd, DockEndPadding);
            const auto rowBottom = ScaleForWindow(hwnd, DockRowBottomMargin);
            const auto cellHeight = ScaleForWindow(hwnd, DockCellHeight);
            const auto indicatorPad = ScaleForWindow(hwnd, 5);

            if (placement == DockPlacement::Bottom)
            {
                const auto railLeft = (std::max)(0, (width - railLength) / 2);
                const auto shelfTop = (std::max)(0, height - shelfBottom - shelfThickness);
                const auto shelfBottomY = (std::max)(shelfTop + 1, height - shelfBottom);
                RegionHandle shelf{ CreateRoundRectRgn(
                    railLeft,
                    shelfTop,
                    railLeft + railLength + 1,
                    shelfBottomY + 1,
                    shelfRadius * 2,
                    shelfRadius * 2) };
                UnionRegion(region.value, shelf.value);

                const auto cellTop = height - rowBottom - cellHeight;
                for (size_t index = 0; index < visibleItemCount; ++index)
                {
                    const auto logicalCenter = DockEndPadding +
                        static_cast<double>(index) * (DockItemExtent + DockItemGap) +
                        DockItemExtent / 2.0;
                    const auto curve = hoverActive ? MagnificationCurve(std::abs(hoverAxis - logicalCenter)) : 0.0;
                    const auto scale = 1.0 + (DockMaxMagnification - 1.0) * curve;
                    const auto lift = DockHoverLift * curve;

                    const auto slotLeft = railLeft + endPadding + static_cast<int32_t>(index) * (itemExtent + itemGap);
                    const auto centerX = slotLeft + itemExtent / 2;
                    const auto glyphSize = ScaleForWindow(hwnd, static_cast<int32_t>(std::ceil(DockIconSize * scale + 8.0)));
                    const auto localGlyphBottom = DockCellHeight - DockGlyphBottomInset;
                    const auto glyphBottomLogical = DockCellHeight +
                        (localGlyphBottom - DockCellHeight) * scale -
                        lift;
                    const auto glyphBottom = cellTop + ScaleForWindow(hwnd, static_cast<int32_t>(std::ceil(glyphBottomLogical)));
                    const auto glyphTop = glyphBottom - glyphSize;
                    const auto glyphLeft = centerX - glyphSize / 2;
                    RegionHandle glyph{ CreateRoundRectRgn(
                        glyphLeft,
                        (std::max)(0, glyphTop),
                        glyphLeft + glyphSize + 1,
                        (std::min)(height, glyphBottom) + 1,
                        shelfRadius,
                        shelfRadius) };
                    UnionRegion(region.value, glyph.value);

                    RegionHandle indicator{ CreateEllipticRgn(
                        centerX - indicatorPad,
                        height - rowBottom - indicatorPad,
                        centerX + indicatorPad + 1,
                        height - rowBottom + indicatorPad + 1) };
                    UnionRegion(region.value, indicator.value);
                }
            }
            else
            {
                const auto railTop = (std::max)(0, (height - railLength) / 2);
                const auto shelfLeft = placement == DockPlacement::Left
                    ? (std::max)(0, width - shelfBottom - shelfThickness)
                    : shelfBottom;
                const auto shelfRight = shelfLeft + shelfThickness;
                RegionHandle shelf{ CreateRoundRectRgn(
                    shelfLeft,
                    railTop,
                    shelfRight + 1,
                    railTop + railLength + 1,
                    shelfRadius * 2,
                    shelfRadius * 2) };
                UnionRegion(region.value, shelf.value);

                const auto cellLeft = placement == DockPlacement::Left
                    ? width - rowBottom - cellHeight
                    : rowBottom;
                for (size_t index = 0; index < visibleItemCount; ++index)
                {
                    const auto logicalCenter = DockEndPadding +
                        static_cast<double>(index) * (DockItemExtent + DockItemGap) +
                        DockItemExtent / 2.0;
                    const auto curve = hoverActive ? MagnificationCurve(std::abs(hoverAxis - logicalCenter)) : 0.0;
                    const auto scale = 1.0 + (DockMaxMagnification - 1.0) * curve;
                    const auto lift = DockHoverLift * curve;
                    const auto slotTop = railTop + endPadding + static_cast<int32_t>(index) * (itemExtent + itemGap);
                    const auto centerY = slotTop + itemExtent / 2;
                    const auto glyphSize = ScaleForWindow(hwnd, static_cast<int32_t>(std::ceil(DockIconSize * scale + 8.0)));
                    const auto localGlyphEdge = DockCellHeight - DockGlyphBottomInset;
                    const auto glyphEdgeLogical = DockCellHeight +
                        (localGlyphEdge - DockCellHeight) * scale -
                        lift;
                    const auto glyphEdge = placement == DockPlacement::Left
                        ? cellLeft + ScaleForWindow(hwnd, static_cast<int32_t>(std::ceil(glyphEdgeLogical)))
                        : cellLeft + cellHeight - ScaleForWindow(hwnd, static_cast<int32_t>(std::ceil(glyphEdgeLogical)));
                    const auto glyphLeft = placement == DockPlacement::Left ? glyphEdge - glyphSize : glyphEdge;
                    RegionHandle glyph{ CreateRoundRectRgn(
                        (std::max)(0, glyphLeft),
                        centerY - glyphSize / 2,
                        (std::min)(width, glyphLeft + glyphSize) + 1,
                        centerY + glyphSize / 2 + 1,
                        shelfRadius,
                        shelfRadius) };
                    UnionRegion(region.value, glyph.value);
                }
            }

            return region;
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

    void ApplyDockWindowTransparency(HWND hwnd, bool highContrast)
    {
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

        if (visibleItemCount == 0)
        {
            SetWindowRgn(hwnd, nullptr, TRUE);
            SuppressDwmBorder(hwnd);
            return;
        }

        auto region = BuildDockWindowRegion(hwnd, placement, width, height, visibleItemCount, 0.0, false);
        if (!region.value)
        {
            SetWindowRgn(hwnd, nullptr, TRUE);
            SuppressDwmBorder(hwnd);
            return;
        }

        if (SetWindowRgn(hwnd, region.value, TRUE) != 0)
        {
            region.release();
        }
        SuppressDwmBorder(hwnd);
    }

    void ApplyDockWindowHoverShape(
        HWND hwnd,
        DockPlacement placement,
        int32_t width,
        int32_t height,
        size_t visibleItemCount,
        double hoverAxis,
        bool hoverActive)
    {
        if (!hwnd)
        {
            return;
        }

        if (visibleItemCount == 0)
        {
            SetWindowRgn(hwnd, nullptr, TRUE);
            SuppressDwmBorder(hwnd);
            return;
        }

        auto region = BuildDockWindowRegion(hwnd, placement, width, height, visibleItemCount, hoverAxis, hoverActive);
        if (!region.value)
        {
            return;
        }

        if (SetWindowRgn(hwnd, region.value, TRUE) != 0)
        {
            region.release();
        }
        SuppressDwmBorder(hwnd);
    }
}
