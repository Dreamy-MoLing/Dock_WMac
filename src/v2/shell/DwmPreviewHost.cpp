#include "pch.h"
#include "DwmPreviewHost.h"

namespace DockWMac::shell
{
    namespace
    {
        constexpr wchar_t PreviewClassName[] = L"DockWMacDwmPreviewHost";
        constexpr int32_t MaxPreviewWidth = 320;
        constexpr int32_t MaxPreviewHeight = 220;
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
            WS_POPUP | WS_BORDER,
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

    void DwmPreviewHost::ClearThumbnail()
    {
        if (m_thumbnail)
        {
            DwmUnregisterThumbnail(m_thumbnail);
            m_thumbnail = nullptr;
        }
    }

    bool DwmPreviewHost::Show(HWND source)
    {
        HideImmediate();
        if (!IsWindow(source) || IsIconic(source))
        {
            return false;
        }
        if (!EnsureWindow())
        {
            return false;
        }

        if (FAILED(DwmRegisterThumbnail(m_hwnd, source, &m_thumbnail)))
        {
            m_thumbnail = nullptr;
            return false;
        }

        SIZE sourceSize{};
        DwmQueryThumbnailSourceSize(m_thumbnail, &sourceSize);
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
        if (FAILED(DwmUpdateThumbnailProperties(m_thumbnail, &properties)))
        {
            Hide();
            return false;
        }

        return true;
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
        ClearThumbnail();
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
}
