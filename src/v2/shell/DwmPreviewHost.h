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
        void Hide();

    private:
        bool EnsureWindow();
        void ClearThumbnail();

        HWND m_hwnd{};
        HTHUMBNAIL m_thumbnail{};
    };
}
