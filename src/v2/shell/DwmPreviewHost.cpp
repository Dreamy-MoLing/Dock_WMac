#include "pch.h"
#include "DwmPreviewHost.h"

namespace DockWMac::shell
{
    namespace
    {
        constexpr wchar_t PreviewClassName[] = L"DockWMacDwmPreviewHost";
        constexpr int32_t MaxPreviewWidth = 320;
        constexpr int32_t MaxPreviewHeight = 220;
        constexpr int32_t GroupItemMaxWidth = 240;
        constexpr int32_t GroupItemMaxHeight = 150;
        constexpr int32_t GroupCardPadding = 8;
        constexpr int32_t GroupTextHeight = 44;
        constexpr int32_t GroupCardWidth = GroupItemMaxWidth + GroupCardPadding * 2;
        constexpr int32_t GroupCardHeight = GroupItemMaxHeight + GroupCardPadding * 2 + GroupTextHeight;
        constexpr int32_t GroupPadding = 12;
        constexpr int32_t GroupGap = 12;
        constexpr UINT_PTR HideTimerId = 1;
        constexpr UINT HideDelayMs = 300;

        LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (message == WM_NCCREATE)
            {
                auto create = reinterpret_cast<CREATESTRUCTW*>(lparam);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            }

            auto host = reinterpret_cast<DwmPreviewHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (host)
            {
                switch (message)
                {
                case WM_MOUSEMOVE:
                    host->OnMouseMove();
                    break;
                case WM_MOUSELEAVE:
                    host->OnMouseLeave();
                    break;
                case WM_LBUTTONUP:
                    host->OnClick(MAKEPOINTS(lparam));
                    return 0;
                case WM_PAINT:
                    host->Paint();
                    return 0;
                case WM_TIMER:
                    if (wparam == HideTimerId)
                    {
                        host->OnHideTimer();
                        return 0;
                    }
                    break;
                default:
                    break;
                }
            }

            if (message == WM_ERASEBKGND)
            {
                return 1;
            }
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }

        void RegisterPreviewClass()
        {
            static bool registered = false;
            if (registered)
            {
                return;
            }

            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc = PreviewWndProc;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            windowClass.lpszClassName = PreviewClassName;
            RegisterClassW(&windowClass);
            registered = true;
        }

        RECT WorkAreaNearCursor()
        {
            POINT cursor{};
            GetCursorPos(&cursor);
            auto monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info{ sizeof(info) };
            if (!GetMonitorInfoW(monitor, &info))
            {
                info.rcWork = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
            }
            return info.rcWork;
        }

        SIZE FitSize(SIZE source)
        {
            if (source.cx <= 0 || source.cy <= 0)
            {
                return { MaxPreviewWidth, 180 };
            }

            auto scale = (std::min)(
                static_cast<double>(MaxPreviewWidth) / static_cast<double>(source.cx),
                static_cast<double>(MaxPreviewHeight) / static_cast<double>(source.cy));
            scale = (std::min)(1.0, scale);
            return {
                (std::max)(160, static_cast<int32_t>(source.cx * scale)),
                (std::max)(96, static_cast<int32_t>(source.cy * scale)),
            };
        }

        SIZE FitGroupItemSize(SIZE source)
        {
            if (source.cx <= 0 || source.cy <= 0)
            {
                return { GroupItemMaxWidth, GroupItemMaxHeight };
            }

            auto scale = (std::min)(
                static_cast<double>(GroupItemMaxWidth) / static_cast<double>(source.cx),
                static_cast<double>(GroupItemMaxHeight) / static_cast<double>(source.cy));
            scale = (std::min)(1.0, scale);
            return {
                (std::max)(120, static_cast<int32_t>(source.cx * scale)),
                (std::max)(80, static_cast<int32_t>(source.cy * scale)),
            };
        }

        uint32_t GroupColumnCount(size_t count, RECT const& work)
        {
            if (count == 0)
            {
                return 0;
            }

            const auto workWidth = (std::max)(
                int32_t{ 1 },
                static_cast<int32_t>(work.right - work.left - GroupPadding * 2));
            const auto maxColumns = (std::max)(1, (workWidth + GroupGap) / (GroupCardWidth + GroupGap));
            return static_cast<uint32_t>((std::min<size_t>)(count, static_cast<size_t>(maxColumns)));
        }

        std::wstring WindowTitleOrFallback(PreviewSource const& source)
        {
            if (!source.title.empty())
            {
                return source.title;
            }
            return L"Window";
        }

        std::wstring PreviewStatus(PreviewSource const& source, bool thumbnailAvailable)
        {
            if (source.minimized)
            {
                return L"Minimized";
            }
            if (source.unavailable || !thumbnailAvailable)
            {
                return L"Preview unavailable";
            }
            return L"Live preview";
        }

        POINT PreviewOrigin(SIZE size)
        {
            POINT cursor{};
            GetCursorPos(&cursor);
            auto work = WorkAreaNearCursor();

            auto x = cursor.x - size.cx / 2;
            auto y = cursor.y - size.cy - 28;
            x = std::clamp(x, work.left + 8, work.right - size.cx - 8);
            if (y < work.top + 8)
            {
                y = cursor.y + 28;
            }
            y = std::clamp(y, work.top + 8, work.bottom - size.cy - 8);
            return { x, y };
        }
    }

    DwmPreviewHost::~DwmPreviewHost()
    {
        HideImmediate();
        if (m_hwnd)
        {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool DwmPreviewHost::EnsureWindow()
    {
        if (m_hwnd && IsWindow(m_hwnd))
        {
            return true;
        }

        RegisterPreviewClass();
        m_hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            PreviewClassName,
            L"Dock Preview",
            WS_POPUP,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            MaxPreviewWidth,
            MaxPreviewHeight,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            this);
        return m_hwnd != nullptr;
    }

    void DwmPreviewHost::ClearThumbnails()
    {
        for (auto thumbnail : m_thumbnails)
        {
            if (thumbnail)
            {
                DwmUnregisterThumbnail(thumbnail);
            }
        }
        m_thumbnails.clear();
        m_hitTargets.clear();
        m_cards.clear();
    }

    void DwmPreviewHost::SetActivateHandler(ActivateHandler handler)
    {
        m_activateHandler = std::move(handler);
    }

    bool DwmPreviewHost::Show(HWND source)
    {
        HideImmediate();
        if (!IsWindow(source))
        {
            return false;
        }
        if (!EnsureWindow())
        {
            return false;
        }

        HTHUMBNAIL thumbnail{};
        if (FAILED(DwmRegisterThumbnail(m_hwnd, source, &thumbnail)))
        {
            return false;
        }
        m_thumbnails.push_back(thumbnail);

        SIZE sourceSize{};
        DwmQueryThumbnailSourceSize(thumbnail, &sourceSize);
        auto size = FitSize(sourceSize);
        auto origin = PreviewOrigin(size);

        SetWindowPos(
            m_hwnd,
            HWND_TOPMOST,
            origin.x,
            origin.y,
            size.cx,
            size.cy,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);

        DWM_THUMBNAIL_PROPERTIES properties{};
        properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
        properties.rcDestination = { 0, 0, size.cx, size.cy };
        properties.fVisible = TRUE;
        properties.opacity = 255;
        if (FAILED(DwmUpdateThumbnailProperties(thumbnail, &properties)))
        {
            Hide();
            return false;
        }

        m_hitTargets.push_back({ { 0, 0, size.cx, size.cy }, source });
        return true;
    }

    bool DwmPreviewHost::Show(PreviewSource const& source)
    {
        if (!source.unavailable && IsWindow(source.hwnd) && Show(source.hwnd))
        {
            return true;
        }

        return ShowGroup({ source });
    }

    bool DwmPreviewHost::ShowGroup(std::vector<PreviewSource> const& sources)
    {
        HideImmediate();
        if (sources.empty())
        {
            return false;
        }
        if (!EnsureWindow())
        {
            return false;
        }

        struct PendingThumbnail
        {
            PreviewSource source;
            HWND hwnd{};
            HTHUMBNAIL thumbnail{};
            SIZE size{};
            bool thumbnailAvailable{};
        };

        std::vector<PendingThumbnail> pending;
        pending.reserve(sources.size());
        for (auto const& source : sources)
        {
            PendingThumbnail item;
            item.source = source;
            item.hwnd = source.hwnd;

            if (!source.unavailable && IsWindow(source.hwnd))
            {
                HTHUMBNAIL thumbnail{};
                if (SUCCEEDED(DwmRegisterThumbnail(m_hwnd, source.hwnd, &thumbnail)))
                {
                    SIZE sourceSize{};
                    DwmQueryThumbnailSourceSize(thumbnail, &sourceSize);
                    item.thumbnail = thumbnail;
                    item.size = FitGroupItemSize(sourceSize);
                    item.thumbnailAvailable = true;
                }
            }

            if (!item.thumbnailAvailable)
            {
                item.size = { GroupItemMaxWidth, GroupItemMaxHeight };
            }
            pending.push_back(std::move(item));
        }

        if (pending.empty())
        {
            return false;
        }

        auto work = WorkAreaNearCursor();
        const auto columns = GroupColumnCount(pending.size(), work);
        const auto rows = static_cast<uint32_t>((pending.size() + columns - 1) / columns);
        const auto width = GroupPadding * 2 + static_cast<int32_t>(columns) * GroupCardWidth + static_cast<int32_t>(columns - 1) * GroupGap;
        const auto height = GroupPadding * 2 + static_cast<int32_t>(rows) * GroupCardHeight + static_cast<int32_t>(rows - 1) * GroupGap;
        const SIZE hostSize{ width, height };
        const auto origin = PreviewOrigin(hostSize);

        SetWindowPos(
            m_hwnd,
            HWND_TOPMOST,
            origin.x,
            origin.y,
            hostSize.cx,
            hostSize.cy,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);

        for (size_t index = 0; index < pending.size(); ++index)
        {
            const auto row = static_cast<int32_t>(index / columns);
            const auto column = static_cast<int32_t>(index % columns);
            const auto cardLeft = GroupPadding + column * (GroupCardWidth + GroupGap);
            const auto cardTop = GroupPadding + row * (GroupCardHeight + GroupGap);
            const RECT cardBounds{
                cardLeft,
                cardTop,
                cardLeft + GroupCardWidth,
                cardTop + GroupCardHeight,
            };
            const auto left = cardLeft + GroupCardPadding + (GroupItemMaxWidth - pending[index].size.cx) / 2;
            const auto top = cardTop + GroupCardPadding + (GroupItemMaxHeight - pending[index].size.cy) / 2;
            const RECT destination{
                left,
                top,
                left + pending[index].size.cx,
                top + pending[index].size.cy,
            };

            m_cards.push_back({
                cardBounds,
                destination,
                WindowTitleOrFallback(pending[index].source),
                PreviewStatus(pending[index].source, pending[index].thumbnailAvailable),
                pending[index].source.unavailable || !pending[index].thumbnailAvailable,
            });

            if (!pending[index].source.unavailable && IsWindow(pending[index].hwnd))
            {
                m_hitTargets.push_back({ cardBounds, pending[index].hwnd });
            }

            if (!pending[index].thumbnailAvailable)
            {
                continue;
            }

            DWM_THUMBNAIL_PROPERTIES properties{};
            properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
            properties.rcDestination = destination;
            properties.fVisible = TRUE;
            properties.opacity = 255;
            if (FAILED(DwmUpdateThumbnailProperties(pending[index].thumbnail, &properties)))
            {
                DwmUnregisterThumbnail(pending[index].thumbnail);
                continue;
            }

            m_thumbnails.push_back(pending[index].thumbnail);
            pending[index].thumbnail = nullptr;
        }

        InvalidateRect(m_hwnd, nullptr, TRUE);
        UpdateWindow(m_hwnd);

        for (auto const& item : pending)
        {
            if (item.thumbnail)
            {
                DwmUnregisterThumbnail(item.thumbnail);
            }
        }

        if (m_cards.empty())
        {
            HideImmediate();
            return false;
        }

        return true;
    }

    void DwmPreviewHost::Paint()
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(m_hwnd, &paint);
        if (!dc)
        {
            return;
        }

        RECT client{};
        GetClientRect(m_hwnd, &client);

        auto background = CreateSolidBrush(RGB(18, 21, 26));
        FillRect(dc, &client, background);
        DeleteObject(background);

        auto cardBrush = CreateSolidBrush(RGB(34, 39, 48));
        auto cardBorder = CreatePen(PS_SOLID, 1, RGB(78, 86, 100));
        auto oldBrush = SelectObject(dc, cardBrush);
        auto oldPen = SelectObject(dc, cardBorder);

        SetBkMode(dc, TRANSPARENT);
        auto titleFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        auto oldFont = SelectObject(dc, titleFont);

        for (auto const& card : m_cards)
        {
            RoundRect(dc, card.bounds.left, card.bounds.top, card.bounds.right, card.bounds.bottom, 10, 10);

            if (card.unavailable)
            {
                auto fallbackBrush = CreateSolidBrush(RGB(25, 29, 36));
                FillRect(dc, &card.thumbnailBounds, fallbackBrush);
                DeleteObject(fallbackBrush);

                SetTextColor(dc, RGB(190, 198, 210));
                RECT fallbackText = card.thumbnailBounds;
                DrawTextW(dc, card.status.c_str(), -1, &fallbackText, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            RECT titleRect{
                card.bounds.left + GroupCardPadding,
                card.bounds.bottom - GroupTextHeight,
                card.bounds.right - GroupCardPadding,
                card.bounds.bottom - GroupTextHeight / 2,
            };
            SetTextColor(dc, RGB(244, 247, 252));
            DrawTextW(dc, card.title.c_str(), -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            RECT statusRect{
                card.bounds.left + GroupCardPadding,
                card.bounds.bottom - GroupTextHeight / 2,
                card.bounds.right - GroupCardPadding,
                card.bounds.bottom - GroupCardPadding,
            };
            SetTextColor(dc, card.unavailable ? RGB(246, 184, 92) : RGB(170, 179, 193));
            DrawTextW(dc, card.status.c_str(), -1, &statusRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        SelectObject(dc, oldFont);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(cardBorder);
        DeleteObject(cardBrush);
        EndPaint(m_hwnd, &paint);
    }

    void DwmPreviewHost::RequestHide()
    {
        if (!m_hwnd || !IsWindowVisible(m_hwnd))
        {
            HideImmediate();
            return;
        }

        SetTimer(m_hwnd, HideTimerId, HideDelayMs, nullptr);
    }

    void DwmPreviewHost::CancelHide()
    {
        if (m_hwnd)
        {
            KillTimer(m_hwnd, HideTimerId);
        }
    }

    void DwmPreviewHost::HideImmediate()
    {
        CancelHide();
        ClearThumbnails();
        if (m_hwnd)
        {
            ShowWindow(m_hwnd, SW_HIDE);
        }
        m_trackingMouse = false;
    }

    void DwmPreviewHost::Hide()
    {
        HideImmediate();
    }

    void DwmPreviewHost::OnMouseMove()
    {
        CancelHide();
        if (!m_hwnd || m_trackingMouse)
        {
            return;
        }

        TRACKMOUSEEVENT track{ sizeof(track) };
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = m_hwnd;
        if (TrackMouseEvent(&track))
        {
            m_trackingMouse = true;
        }
    }

    void DwmPreviewHost::OnMouseLeave()
    {
        m_trackingMouse = false;
        RequestHide();
    }

    void DwmPreviewHost::OnHideTimer()
    {
        HideImmediate();
    }

    void DwmPreviewHost::OnClick(POINTS point)
    {
        POINT cursor{ point.x, point.y };
        for (auto const& target : m_hitTargets)
        {
            if (PtInRect(&target.bounds, cursor))
            {
                auto hwnd = target.hwnd;
                HideImmediate();
                if (m_activateHandler)
                {
                    m_activateHandler(hwnd);
                }
                return;
            }
        }
    }

    size_t DwmPreviewHost::ActivatableTargetCountForDiagnostics() const
    {
        return m_hitTargets.size();
    }

    size_t DwmPreviewHost::RegisteredThumbnailCountForDiagnostics() const
    {
        return m_thumbnails.size();
    }

    HWND DwmPreviewHost::WindowHandleForDiagnostics() const
    {
        return m_hwnd;
    }

    std::optional<POINTS> DwmPreviewHost::ActivatablePointForDiagnostics(size_t index) const
    {
        if (index >= m_hitTargets.size())
        {
            return std::nullopt;
        }

        auto const& bounds = m_hitTargets[index].bounds;
        return POINTS{
            static_cast<SHORT>(bounds.left + (bounds.right - bounds.left) / 2),
            static_cast<SHORT>(bounds.top + (bounds.bottom - bounds.top) / 2),
        };
    }
}
