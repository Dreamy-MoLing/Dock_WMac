#pragma once

#include <functional>
#include <string>
#include <vector>
#include <windows.h>

namespace DockWMac::shell
{
    struct PreviewSource
    {
        HWND hwnd{};
        std::wstring title;
        bool minimized{};
        bool unavailable{};
    };

    class DwmPreviewHost
    {
    public:
        using ActivateHandler = std::function<void(HWND)>;

        DwmPreviewHost() = default;
        ~DwmPreviewHost();

        DwmPreviewHost(DwmPreviewHost const&) = delete;
        DwmPreviewHost& operator=(DwmPreviewHost const&) = delete;

        bool Show(HWND source);
        bool Show(PreviewSource const& source);
        bool ShowGroup(std::vector<PreviewSource> const& sources);
        void SetActivateHandler(ActivateHandler handler);
        void RequestHide();
        void CancelHide();
        void HideImmediate();
        void Hide();
        void OnMouseMove();
        void OnMouseLeave();
        void OnHideTimer();
        void OnClick(POINTS point);
        void Paint();
        size_t ActivatableTargetCountForDiagnostics() const;
        size_t RegisteredThumbnailCountForDiagnostics() const;
        HWND WindowHandleForDiagnostics() const;
        std::optional<POINTS> ActivatablePointForDiagnostics(size_t index) const;

    private:
        struct HitTarget
        {
            RECT bounds{};
            HWND hwnd{};
        };

        struct PreviewCard
        {
            RECT bounds{};
            RECT thumbnailBounds{};
            std::wstring title;
            std::wstring status;
            bool unavailable{};
        };

        bool EnsureWindow();
        void ClearThumbnails();

        HWND m_hwnd{};
        std::vector<HTHUMBNAIL> m_thumbnails;
        std::vector<HitTarget> m_hitTargets;
        std::vector<PreviewCard> m_cards;
        ActivateHandler m_activateHandler;
        bool m_trackingMouse{};
    };
}
