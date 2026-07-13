#pragma once

#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <unordered_map>
#include <windows.h>
#include <wincodec.h>

#include "../dock/DockModel.h"
#include "../infra/AppSettings.h"

namespace DockWMac::render
{
    class NativeDockHost
    {
    public:
        using DockActionHandler = std::function<void(DockWMac::dock::DockAction const&)>;
        using DockOrderChangedHandler = std::function<void(std::vector<std::wstring> const&)>;
        using DockPreviewHandler = std::function<void(DockWMac::dock::DockWindowRef const&)>;
        using DockWindowGroupPreviewHandler = std::function<bool(std::vector<DockWMac::dock::DockWindowRef> const&)>;
        using DockPreviewHideHandler = std::function<void()>;
        using SystemEnvironmentChangedHandler = std::function<void()>;
        using SystemSettingsChangedHandler = std::function<void()>;
        using DiagnosticHandler = std::function<void(std::string_view)>;

        explicit NativeDockHost(
            DiagnosticHandler diagnosticHandler = {},
            bool createGraphicsResources = true);
        ~NativeDockHost();

        NativeDockHost(NativeDockHost const&) = delete;
        NativeDockHost& operator=(NativeDockHost const&) = delete;

        void Configure(
            DockWMac::infra::AppSettings settings,
            std::vector<DockWMac::dock::DockItem> items,
            DockActionHandler actionHandler,
            DockOrderChangedHandler orderChangedHandler,
            DockPreviewHandler previewHandler,
            DockWindowGroupPreviewHandler groupPreviewHandler,
            DockPreviewHideHandler previewHideHandler,
            SystemEnvironmentChangedHandler systemEnvironmentChangedHandler = {},
            SystemSettingsChangedHandler systemSettingsChangedHandler = {});

        void Show();
        void Hide();
        void Close();
        void ShowStatusMessage(std::wstring message, bool error = false,
                               std::chrono::milliseconds duration =
                                   std::chrono::milliseconds{3500});
        HWND WindowHandle() const;
        std::optional<POINT> ItemCenterForDiagnostics(size_t index) const;
        void SetHoveringForDiagnostics(bool hovering);
        double HoverAmountForDiagnostics() const;
        bool HoverAnimationRunningForDiagnostics() const;
        bool HasIndependentCompositionLayersForDiagnostics() const;

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                        LPARAM lparam);

      private:
        struct ItemVisual {
          size_t index{};
          D2D1_RECT_F iconRect{};
          D2D1_RECT_F hitRect{};
        };

        LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

        void CreateHostWindow(bool createGraphicsResources);
        void CreateDeviceResources();
        void CreateCompositionResources();
        void ResizeAndPosition(bool showWindow);
        void ResizeCompositionSurfaces();
        void Render();
        void DiscardGraphicsResources();
        void StartAutoHideTimer();
        void StopAutoHideTimer();
        void UpdateAutoHide();
        void SetAutoHidden(bool hidden);
        bool AutoHideEnabled() const;
        void StartHoverAnimation();
        void StopHoverAnimation();
        void AdvanceHoverAnimation();

        int32_t Scale(double value) const;
        double Unscale(int32_t value) const;
        double MainAxisLength(size_t visibleItems) const;
        int32_t WindowWidth() const;
        int32_t WindowHeight() const;
        bool IsVertical() const;
        double MainExtentDip() const;
        double CrossExtentDip() const;
        double MainAxisPosition(POINTS point) const;
        double ShelfTangentLine() const;
        D2D1_RECT_F ShelfRectDip() const;
        std::vector<ItemVisual> BuildItemVisuals(double pointerAxis,
                                                 bool hovering) const;
        std::optional<size_t> HitTest(POINTS point) const;
        void UpdateHover(POINTS point);
        void ResetHover();
        void ShowPreviewForItem(DockWMac::dock::DockItem const &item);
        void TrackMouseLeave();
        void ClearStatusMessage();
        void BeginPointerPress(POINTS point);
        void UpdateDrag(POINTS point);
        void CompletePointerPress(POINTS point);
        void ResetDrag();
        size_t CalculateInsertIndex(POINTS point) const;
        std::vector<std::wstring> CurrentOrder() const;
        void HandleClick(POINTS point);
        void HandleLaunchNewInstance(POINTS point);
        void HandleMenuSelect(WPARAM wparam, LPARAM lparam);
        void ShowItemMenu(size_t index, POINTS point);
        void ShowDockMenu(POINTS point);
        ID2D1Bitmap1 *LoadIconBitmap(std::wstring const &path);
        void Trace(std::string_view message) const;

        DockWMac::infra::AppSettings m_settings;
        std::vector<DockWMac::dock::DockItem> m_items;
        DockActionHandler m_actionHandler;
        DockOrderChangedHandler m_orderChangedHandler;
        DockPreviewHandler m_previewHandler;
        DockWindowGroupPreviewHandler m_groupPreviewHandler;
        DockPreviewHideHandler m_previewHideHandler;
        SystemEnvironmentChangedHandler m_systemEnvironmentChangedHandler;
        SystemSettingsChangedHandler m_systemSettingsChangedHandler;
        DiagnosticHandler m_diagnosticHandler;

        HWND m_hwnd{};
        uint32_t m_dpi{USER_DEFAULT_SCREEN_DPI};
        int32_t m_width{};
        int32_t m_height{};
        bool m_visible{};
        bool m_trackingMouse{};
        bool m_hovering{};
        bool m_pointerPressed{};
        bool m_dragging{};
        bool m_autoHidden{};
        bool m_menuOpen{};
        bool m_statusError{};
        bool m_hoverAnimationRunning{};
        std::unordered_map<UINT_PTR, DockWMac::dock::DockWindowRef>
            m_menuWindowCommands;
        double m_pointerAxis{};
        double m_hoverAmount{};
        uint64_t m_lastInsideAutoHideTick{};
        uint64_t m_lastHoverAnimationTick{};
        std::wstring m_statusMessage;
        POINTS m_pressPoint{};
        std::optional<size_t> m_hoverIndex;
        std::optional<size_t> m_pressedIndex;
        std::optional<size_t> m_dragIndex;
        std::optional<size_t> m_dragTargetIndex;

        winrt::com_ptr<ID3D11Device> m_d3dDevice;
        winrt::com_ptr<IDXGIDevice> m_dxgiDevice;
        winrt::com_ptr<IDCompositionDevice> m_dcompDevice;
        winrt::com_ptr<IDCompositionTarget> m_dcompTarget;
        winrt::com_ptr<IDCompositionVisual> m_rootVisual;
        winrt::com_ptr<IDCompositionVisual> m_shelfVisual;
        winrt::com_ptr<IDCompositionVisual> m_iconVisual;
        winrt::com_ptr<IDCompositionSurface> m_shelfSurface;
        winrt::com_ptr<IDCompositionSurface> m_iconSurface;
        winrt::com_ptr<ID2D1Factory1> m_d2dFactory;
        winrt::com_ptr<ID2D1Device> m_d2dDevice;
        winrt::com_ptr<ID2D1DeviceContext> m_d2dContext;
        winrt::com_ptr<IDWriteFactory> m_dwriteFactory;
        winrt::com_ptr<IWICImagingFactory> m_wicFactory;
        winrt::com_ptr<IDWriteTextFormat> m_labelFormat;
        winrt::com_ptr<IDWriteTextFormat> m_statusFormat;
        std::map<std::wstring, winrt::com_ptr<ID2D1Bitmap1>> m_iconBitmaps;
    };
    } // namespace DockWMac::render
