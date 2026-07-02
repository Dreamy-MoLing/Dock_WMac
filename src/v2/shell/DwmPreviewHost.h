#pragma once

namespace DockWMac::shell
{
    class DwmPreviewHost
    {
    public:
        DwmPreviewHost() = default;
        ~DwmPreviewHost();

        DwmPreviewHost(DwmPreviewHost const&) = delete;
        DwmPreviewHost& operator=(DwmPreviewHost const&) = delete;

        bool Show(HWND source);
        void RequestHide();
        void CancelHide();
        void HideImmediate();
        void Hide();
        void OnMouseMove();
        void OnMouseLeave();
        void OnHideTimer();

    private:
        bool EnsureWindow();
        void ClearThumbnail();

        HWND m_hwnd{};
        HTHUMBNAIL m_thumbnail{};
        bool m_trackingMouse{};
    };
}
