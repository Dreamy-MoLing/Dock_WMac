#include "pch.h"
#include "NativeDockHost.h"
#include "../platform/CursorTracker.h"
#include "../platform/DockPlacement.h"
#include "../ui/DockIconLayout.h"

namespace DockWMac::render
{
    namespace
    {
        constexpr wchar_t DockHostClassName[] = L"DockWMacNativeDockHost";
        constexpr double IconSize = 56.0;
        constexpr double IconImageSize = 54.0;
        constexpr double ItemSlot = 66.0;
        constexpr double ItemGap = 10.0;
        constexpr double DockCrossAxis = 176.0;
        constexpr double DockEndPadding = 34.0;
        constexpr double ShelfMargin = 14.0;
        constexpr double ShelfThickness = IconSize * 0.52 + 18.0;
        constexpr double ShelfRadius = 18.0;
        constexpr double MaxMagnification = 1.68;
        constexpr double ActiveLift = 6.0;
        constexpr double MagnificationRange = IconSize * 2.2;
        constexpr UINT_PTR AutoHideTimerId = 100;
        constexpr UINT_PTR StatusTimerId = 101;
        constexpr UINT_PTR HoverAnimationTimerId = 102;
        constexpr UINT_PTR MenuPinCommand = 100;
        constexpr UINT_PTR MenuCloseWindowCommand = 101;
        constexpr UINT_PTR MenuCloseAllWindowsCommand = 102;
        constexpr UINT_PTR MenuAutoHideCommand = 200;
        constexpr UINT_PTR MenuPlaceBottomCommand = 201;
        constexpr UINT_PTR MenuPlaceLeftCommand = 202;
        constexpr UINT_PTR MenuPlaceRightCommand = 203;
        constexpr UINT_PTR MenuExitCommand = 204;
        constexpr UINT_PTR MenuWindowBaseCommand = 1000;
        constexpr UINT AutoHidePollMs = 160;
        constexpr UINT HoverAnimationFrameMs = 16;
        constexpr uint64_t AutoHideCollapseDelayMs = 650;

        UINT TaskbarCreatedMessage()
        {
            static const UINT message = RegisterWindowMessageW(L"TaskbarCreated");
            return message;
        }

        D2D1_COLOR_F Color(uint8_t r, uint8_t g, uint8_t b, float a = 1.0f)
        {
            return D2D1::ColorF(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                a);
        }

        D2D1_RECT_F ScaleRect(D2D1_RECT_F rect, float scale)
        {
            return D2D1::RectF(
                rect.left * scale,
                rect.top * scale,
                rect.right * scale,
                rect.bottom * scale);
        }

        void RegisterDockHostClass()
        {
            static bool registered = false;
            if (registered)
            {
                return;
            }

            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc = NativeDockHost::WndProc;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            windowClass.lpszClassName = DockHostClassName;
            RegisterClassW(&windowClass);
            registered = true;
        }

        std::wstring Initials(std::wstring const& value)
        {
            if (value.empty())
            {
                return L"?";
            }
            return std::wstring{ 1, static_cast<wchar_t>(::towupper(value.front())) };
        }

        DockWMac::ui::DockIconMetrics IconMetrics()
        {
            return {
                IconSize,
                MaxMagnification,
                ActiveLift,
                MagnificationRange,
            };
        }

        std::wstring WindowMenuText(DockWMac::dock::DockWindowRef const& window)
        {
            auto text = window.title.empty() ? L"Window" : window.title;
            if (window.foreground)
            {
                text += L"  (Active)";
            }
            else if (window.minimized)
            {
                text += L"  (Minimized)";
            }
            if (window.cloaked)
            {
                text += L"  (Unavailable)";
            }
            return text;
        }

        void CheckStep(HRESULT hr, wchar_t const* step)
        {
            if (FAILED(hr))
            {
                throw winrt::hresult_error(hr, step);
            }
        }
    }

    NativeDockHost::NativeDockHost(DiagnosticHandler diagnosticHandler, bool createGraphicsResources) :
        m_diagnosticHandler(std::move(diagnosticHandler))
    {
        Trace("NativeDockHost constructing.");
        CreateHostWindow(createGraphicsResources);
        Trace("NativeDockHost constructed.");
    }

    NativeDockHost::~NativeDockHost()
    {
        Close();
    }

    void NativeDockHost::Configure(
        DockWMac::infra::AppSettings settings,
        std::vector<DockWMac::dock::DockItem> items,
        DockActionHandler actionHandler,
        DockOrderChangedHandler orderChangedHandler,
        DockPreviewHandler previewHandler,
        DockWindowGroupPreviewHandler groupPreviewHandler,
        DockPreviewHideHandler previewHideHandler,
        SystemEnvironmentChangedHandler systemEnvironmentChangedHandler,
        SystemSettingsChangedHandler systemSettingsChangedHandler)
    {
        m_settings = std::move(settings);
        m_items = std::move(items);
        m_actionHandler = std::move(actionHandler);
        m_orderChangedHandler = std::move(orderChangedHandler);
        m_previewHandler = std::move(previewHandler);
        m_groupPreviewHandler = std::move(groupPreviewHandler);
        m_previewHideHandler = std::move(previewHideHandler);
        m_systemEnvironmentChangedHandler = std::move(systemEnvironmentChangedHandler);
        m_systemSettingsChangedHandler = std::move(systemSettingsChangedHandler);
        if (m_settings.reducedMotion)
        {
            m_hoverAmount = m_hovering ? 1.0 : 0.0;
            StopHoverAnimation();
        }
        if (!AutoHideEnabled())
        {
            m_autoHidden = false;
            StopAutoHideTimer();
        }
        else if (m_visible)
        {
            StartAutoHideTimer();
        }
        ResizeAndPosition(m_visible);
        Render();
    }

    void NativeDockHost::Show()
    {
        m_visible = true;
        m_lastInsideAutoHideTick = GetTickCount64();
        if (AutoHideEnabled())
        {
            StartAutoHideTimer();
        }
        ResizeAndPosition(true);
        Render();
    }

    void NativeDockHost::Hide()
    {
        m_visible = false;
        m_hovering = false;
        m_hoverAmount = 0.0;
        StopHoverAnimation();
        StopAutoHideTimer();
        ClearStatusMessage();
        if (m_hwnd)
        {
            ShowWindow(m_hwnd, SW_HIDE);
        }
    }

    void NativeDockHost::Close()
    {
        if (!m_hwnd)
        {
            return;
        }

        KillTimer(m_hwnd, StatusTimerId);
        DestroyWindow(m_hwnd);
    }

    void NativeDockHost::ShowStatusMessage(std::wstring message, bool error, std::chrono::milliseconds duration)
    {
        if (!m_hwnd)
        {
            return;
        }

        m_statusMessage = std::move(message);
        m_statusError = error;
        if (m_statusMessage.empty())
        {
            ClearStatusMessage();
            return;
        }

        auto timeout = duration.count();
        timeout = std::clamp<int64_t>(timeout, 500, 10000);
        SetTimer(m_hwnd, StatusTimerId, static_cast<UINT>(timeout), nullptr);
        Render();
    }

    HWND NativeDockHost::WindowHandle() const
    {
        return m_hwnd;
    }

    std::optional<POINT> NativeDockHost::ItemCenterForDiagnostics(size_t index) const
    {
        auto const visuals = BuildItemVisuals(m_pointerAxis, m_hovering);
        if (index >= visuals.size()) {
          return std::nullopt;
        }

        auto const &rect = visuals[index].iconRect;
        return POINT{
            Scale((static_cast<double>(rect.left) + rect.right) / 2.0),
            Scale((static_cast<double>(rect.top) + rect.bottom) / 2.0),
        };
    }

    void NativeDockHost::SetHoveringForDiagnostics(bool hovering) {
      m_hovering = hovering;
      if (m_settings.reducedMotion) {
        m_hoverAmount = hovering ? 1.0 : 0.0;
        StopHoverAnimation();
      } else {
        StartHoverAnimation();
      }
      Render();
    }

    double NativeDockHost::HoverAmountForDiagnostics() const {
      return m_hoverAmount;
    }

    bool NativeDockHost::HoverAnimationRunningForDiagnostics() const {
      return m_hoverAnimationRunning;
    }

    bool NativeDockHost::HasIndependentCompositionLayersForDiagnostics() const
    {
        return m_rootVisual &&
            m_shelfVisual &&
            m_iconVisual &&
            m_shelfSurface &&
            m_iconSurface &&
            m_shelfVisual.get() != m_iconVisual.get() &&
            m_shelfSurface.get() != m_iconSurface.get();
    }

    LRESULT CALLBACK NativeDockHost::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_NCCREATE)
        {
            auto create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            auto host = reinterpret_cast<NativeDockHost*>(create->lpCreateParams);
            host->m_hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
            return TRUE;
        }

        auto host = reinterpret_cast<NativeDockHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (host)
        {
            return host->HandleMessage(message, wparam, lparam);
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    LRESULT NativeDockHost::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == TaskbarCreatedMessage())
        {
            if (m_systemEnvironmentChangedHandler)
            {
                m_systemEnvironmentChangedHandler();
            }
            return 0;
        }

        switch (message)
        {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_TIMER:
            if (wparam == AutoHideTimerId)
            {
                UpdateAutoHide();
                return 0;
            }
            if (wparam == StatusTimerId)
            {
                ClearStatusMessage();
                return 0;
            }
            if (wparam == HoverAnimationTimerId)
            {
                AdvanceHoverAnimation();
                return 0;
            }
            break;
        case WM_DPICHANGED:
            m_dpi = HIWORD(wparam);
            ResizeAndPosition(m_visible);
            Render();
            return 0;
        case WM_DISPLAYCHANGE:
            ResizeAndPosition(m_visible);
            Render();
            return 0;
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (m_systemSettingsChangedHandler)
            {
                m_systemSettingsChangedHandler();
            }
            else
            {
                ResizeAndPosition(m_visible);
                Render();
            }
            return 0;
        case WM_MOUSEMOVE:
            if (AutoHideEnabled() && m_autoHidden)
            {
                SetAutoHidden(false);
                return 0;
            }
            if (m_pointerPressed)
            {
                UpdateDrag(MAKEPOINTS(lparam));
            }
            else
            {
                UpdateHover(MAKEPOINTS(lparam));
            }
            return 0;
        case WM_MOUSELEAVE:
            if (!m_dragging)
            {
                ResetHover();
            }
            if (AutoHideEnabled())
            {
                m_lastInsideAutoHideTick = GetTickCount64();
            }
            return 0;
        case WM_LBUTTONDOWN:
            BeginPointerPress(MAKEPOINTS(lparam));
            return 0;
        case WM_LBUTTONUP:
            CompletePointerPress(MAKEPOINTS(lparam));
            return 0;
        case WM_MBUTTONUP:
            HandleLaunchNewInstance(MAKEPOINTS(lparam));
            return 0;
        case WM_RBUTTONUP:
            if (m_dragging)
            {
                return 0;
            }
            if (auto index = HitTest(MAKEPOINTS(lparam)))
            {
                ShowItemMenu(*index, MAKEPOINTS(lparam));
            }
            else
            {
                ShowDockMenu(MAKEPOINTS(lparam));
            }
            return 0;
        case WM_MENUSELECT:
            HandleMenuSelect(wparam, lparam);
            return 0;
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
            ResetDrag();
            return 0;
        case WM_CLOSE:
            Close();
            return 0;
        case WM_DESTROY:
            StopAutoHideTimer();
            StopHoverAnimation();
            KillTimer(m_hwnd, StatusTimerId);
            SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
            m_hwnd = nullptr;
            m_visible = false;
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }

        return DefWindowProcW(m_hwnd, message, wparam, lparam);
    }

    void NativeDockHost::CreateHostWindow(bool createGraphicsResources)
    {
        RegisterDockHostClass();

        Trace("CreateWindowExW.");
        m_hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
            DockHostClassName,
            L"Dock_WMac",
            WS_POPUP,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1,
            1,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            this);
        winrt::check_bool(m_hwnd != nullptr);
        Trace("CreateWindowExW succeeded.");
        m_dpi = GetDpiForWindow(m_hwnd);
        if (m_dpi == 0)
        {
            m_dpi = USER_DEFAULT_SCREEN_DPI;
        }

        ::DockWMac::platform::ApplyDockWindowSwitcherBehavior(m_hwnd);
        if (!createGraphicsResources)
        {
            return;
        }

        Trace("CreateDeviceResources.");
        CreateDeviceResources();
        Trace("CreateDeviceResources succeeded.");
        CreateCompositionResources();
    }

    void NativeDockHost::CreateDeviceResources()
    {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        D3D_FEATURE_LEVEL createdLevel{};
        auto hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            m_d3dDevice.put(),
            &createdLevel,
            nullptr);
#if defined(_DEBUG)
        if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
        {
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                flags,
                featureLevels,
                static_cast<UINT>(std::size(featureLevels)),
                D3D11_SDK_VERSION,
                m_d3dDevice.put(),
                &createdLevel,
                nullptr);
        }
#endif
        winrt::check_hresult(hr);
        winrt::check_hresult(m_d3dDevice->QueryInterface(m_dxgiDevice.put()));

        D2D1_FACTORY_OPTIONS options{};
#if defined(_DEBUG)
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        winrt::check_hresult(D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory1),
            &options,
            m_d2dFactory.put_void()));
        winrt::check_hresult(m_d2dFactory->CreateDevice(m_dxgiDevice.get(), m_d2dDevice.put()));
        winrt::check_hresult(m_d2dDevice->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            m_d2dContext.put()));
        winrt::check_hresult(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(m_dwriteFactory.put())));
        winrt::check_hresult(CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(m_wicFactory.put())));
        winrt::check_hresult(m_dwriteFactory->CreateTextFormat(
            L"Segoe UI Variable", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            static_cast<FLOAT>(Scale(22.0)), L"", m_labelFormat.put()));
        m_labelFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_labelFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        winrt::check_hresult(m_dwriteFactory->CreateTextFormat(
            L"Segoe UI Variable", nullptr, DWRITE_FONT_WEIGHT_MEDIUM,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            static_cast<FLOAT>(Scale(13.0)), L"", m_statusFormat.put()));
        m_statusFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_statusFormat->SetParagraphAlignment(
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_statusFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    void NativeDockHost::CreateCompositionResources() {
      Trace("DCompositionCreateDevice.");
      CheckStep(DCompositionCreateDevice(m_dxgiDevice.get(),
                                         __uuidof(IDCompositionDevice),
                                         m_dcompDevice.put_void()),
                L"DCompositionCreateDevice");
      Trace("CreateTargetForHwnd.");
      CheckStep(
          m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, m_dcompTarget.put()),
          L"IDCompositionDevice::CreateTargetForHwnd");
      Trace("CreateVisuals.");
      CheckStep(m_dcompDevice->CreateVisual(m_rootVisual.put()),
                L"IDCompositionDevice::CreateVisual");
      CheckStep(m_dcompDevice->CreateVisual(m_shelfVisual.put()),
                L"IDCompositionDevice::CreateShelfVisual");
      CheckStep(m_dcompDevice->CreateVisual(m_iconVisual.put()),
                L"IDCompositionDevice::CreateIconVisual");
      CheckStep(m_rootVisual->AddVisual(m_shelfVisual.get(), FALSE, nullptr),
                L"IDCompositionVisual::AddShelfVisual");
      CheckStep(m_rootVisual->AddVisual(m_iconVisual.get(), TRUE,
                                        m_shelfVisual.get()),
                L"IDCompositionVisual::AddIconVisual");
      Trace("SetRoot.");
      CheckStep(m_dcompTarget->SetRoot(m_rootVisual.get()),
                L"IDCompositionTarget::SetRoot");
      ResizeCompositionSurfaces();
      CheckStep(m_dcompDevice->Commit(), L"IDCompositionDevice::Commit");
      Trace("CreateCompositionResources succeeded.");
    }

    void NativeDockHost::ResizeAndPosition(bool showWindow) {
      if (!m_hwnd) {
        return;
      }

      const auto fullWidth = WindowWidth();
      const auto fullHeight = WindowHeight();
      const auto rect =
          (AutoHideEnabled() && m_autoHidden)
              ? DockWMac::platform::CalculateDockAutoHideRect(
                    m_hwnd, m_settings.placement, fullWidth, fullHeight)
              : DockWMac::platform::CalculateDockRect(
                    m_hwnd, m_settings.placement, fullWidth, fullHeight);
      m_width = rect.width;
      m_height = rect.height;
      SetWindowPos(m_hwnd, HWND_TOPMOST, rect.x, rect.y, rect.width,
                   rect.height,
                   SWP_NOACTIVATE | (showWindow ? SWP_SHOWWINDOW : 0));
      DockWMac::platform::ApplyDockWindowTransparency(m_hwnd,
                                                      m_settings.highContrast);
      if (AutoHideEnabled() && m_autoHidden) {
        SetWindowRgn(m_hwnd, nullptr, TRUE);
      } else {
        DockWMac::platform::ApplyDockWindowShape(
            m_hwnd, m_settings.placement, m_width, m_height, m_items.size());
      }

      if (showWindow && !m_dcompDevice) {
        CreateCompositionResources();
      } else {
        ResizeCompositionSurfaces();
      }
    }

    void NativeDockHost::ResizeCompositionSurfaces() {
      if (!m_dcompDevice || !m_shelfVisual || !m_iconVisual || m_width <= 0 ||
          m_height <= 0) {
        return;
      }

      m_shelfSurface = nullptr;
      m_iconSurface = nullptr;
      m_iconBitmaps.clear();
      CheckStep(m_dcompDevice->CreateSurface(
                    static_cast<UINT>(m_width), static_cast<UINT>(m_height),
                    DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED,
                    m_shelfSurface.put()),
                L"IDCompositionDevice::CreateShelfSurface");
      CheckStep(m_dcompDevice->CreateSurface(
                    static_cast<UINT>(m_width), static_cast<UINT>(m_height),
                    DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED,
                    m_iconSurface.put()),
                L"IDCompositionDevice::CreateIconSurface");
      CheckStep(m_shelfVisual->SetContent(m_shelfSurface.get()),
                L"IDCompositionVisual::SetShelfContent");
      CheckStep(m_iconVisual->SetContent(m_iconSurface.get()),
                L"IDCompositionVisual::SetIconContent");
      CheckStep(m_dcompDevice->Commit(),
                L"IDCompositionDevice::CommitSurfaces");
    }

    void NativeDockHost::Render() {
      if (!m_shelfSurface || !m_iconSurface || !m_visible) {
        return;
      }

      for (auto layerIndex = 0; layerIndex < 2; ++layerIndex) {
        const auto shelfLayer = layerIndex == 0;
        auto surface = shelfLayer ? m_shelfSurface.get() : m_iconSurface.get();
        POINT offset{};
        RECT updateRect{0, 0, m_width, m_height};
        winrt::com_ptr<IDXGISurface> dxgiSurface;
        HRESULT hr = surface->BeginDraw(&updateRect, __uuidof(IDXGISurface),
                                        dxgiSurface.put_void(), &offset);
        if (hr == DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED) {
          return;
        }
        if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED ||
            hr == DXGI_ERROR_DEVICE_RESET) {
          DiscardGraphicsResources();
          CreateDeviceResources();
          CreateCompositionResources();
          return;
        }
        CheckStep(hr, shelfLayer ? L"IDCompositionSurface::BeginDrawShelf"
                                 : L"IDCompositionSurface::BeginDrawIcons");

        D2D1_BITMAP_PROPERTIES1 targetProperties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED),
            static_cast<FLOAT>(m_dpi), static_cast<FLOAT>(m_dpi));
        winrt::com_ptr<ID2D1Bitmap1> targetBitmap;
        CheckStep(m_d2dContext->CreateBitmapFromDxgiSurface(
                      dxgiSurface.get(), &targetProperties, targetBitmap.put()),
                  L"ID2D1DeviceContext::CreateBitmapFromDxgiSurface");

        auto dc = m_d2dContext.get();
        dc->SetTarget(targetBitmap.get());
        dc->SetTransform(D2D1::Matrix3x2F::Translation(
            static_cast<float>(offset.x), static_cast<float>(offset.y)));
        dc->BeginDraw();
        dc->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));

        winrt::com_ptr<ID2D1SolidColorBrush> shelfBrush;
        winrt::com_ptr<ID2D1SolidColorBrush> shelfBorderBrush;
        winrt::com_ptr<ID2D1SolidColorBrush> fallbackIconBrush;
        winrt::com_ptr<ID2D1SolidColorBrush> textBrush;
        winrt::com_ptr<ID2D1SolidColorBrush> indicatorBrush;
        winrt::com_ptr<ID2D1SolidColorBrush> statusBrush;
        winrt::com_ptr<ID2D1SolidColorBrush> statusBorderBrush;
        auto const lightTheme =
            m_settings.lightTheme && !m_settings.highContrast;
        if (shelfLayer) {
          dc->CreateSolidColorBrush(m_settings.highContrast
                                        ? Color(0, 0, 0, 1.0f)
                                    : lightTheme ? Color(245, 247, 250, 0.72f)
                                                 : Color(18, 22, 32, 0.58f),
                                    shelfBrush.put());
          dc->CreateSolidColorBrush(m_settings.highContrast
                                        ? Color(255, 255, 255, 1.0f)
                                    : lightTheme ? Color(0, 0, 0, 0.18f)
                                                 : Color(255, 255, 255, 0.16f),
                                    shelfBorderBrush.put());
        } else {
          dc->CreateSolidColorBrush(m_settings.highContrast
                                        ? Color(0, 0, 0, 1.0f)
                                    : lightTheme ? Color(216, 221, 229, 0.96f)
                                                 : Color(75, 85, 102, 0.94f),
                                    fallbackIconBrush.put());
          dc->CreateSolidColorBrush(lightTheme ? Color(20, 24, 32, 1.0f)
                                               : Color(255, 255, 255, 1.0f),
                                    textBrush.put());
          dc->CreateSolidColorBrush(m_settings.highContrast
                                        ? Color(255, 255, 0, 1.0f)
                                        : Color(96, 205, 255, 1.0f),
                                    indicatorBrush.put());
          dc->CreateSolidColorBrush(m_settings.highContrast
                                        ? Color(0, 0, 0, 1.0f)
                                        : (m_statusError
                                               ? Color(128, 31, 31, 0.94f)
                                               : Color(24, 28, 38, 0.92f)),
                                    statusBrush.put());
          dc->CreateSolidColorBrush(m_settings.highContrast
                                        ? Color(255, 255, 255, 1.0f)
                                        : (m_statusError
                                               ? Color(255, 160, 160, 0.80f)
                                               : Color(255, 255, 255, 0.22f)),
                                    statusBorderBrush.put());
        }

        const auto scale = static_cast<float>(m_dpi) / USER_DEFAULT_SCREEN_DPI;
        if (AutoHideEnabled() && m_autoHidden) {
          if (shelfLayer) {
            const auto widthDip = Unscale(m_width);
            const auto heightDip = Unscale(m_height);
            const auto triggerRadius =
                (std::min)(widthDip, heightDip) * scale / 2.0;
            auto triggerRect = D2D1::RoundedRect(
                D2D1::RectF(0.0f, 0.0f, static_cast<float>(widthDip * scale),
                            static_cast<float>(heightDip * scale)),
                static_cast<float>(triggerRadius),
                static_cast<float>(triggerRadius));
            dc->FillRoundedRectangle(triggerRect, shelfBrush.get());
            dc->DrawRoundedRectangle(triggerRect, shelfBorderBrush.get(), 1.0f);
          }

          hr = dc->EndDraw();
          dc->SetTarget(nullptr);
          auto endHr = surface->EndDraw();
          if (hr == D2DERR_RECREATE_TARGET || endHr == D2DERR_RECREATE_TARGET) {
            DiscardGraphicsResources();
            CreateDeviceResources();
            CreateCompositionResources();
            return;
          }
          CheckStep(hr, L"ID2D1DeviceContext::EndDrawAutoHide");
          CheckStep(endHr, L"IDCompositionSurface::EndDrawAutoHide");
          continue;
        }

        const auto shelfEdge = ShelfTangentLine();
        const auto shelfRectDip = ShelfRectDip();
        const auto shelfRect =
            D2D1::RoundedRect(ScaleRect(shelfRectDip, scale),
                              static_cast<float>(ShelfRadius * scale),
                              static_cast<float>(ShelfRadius * scale));
        if (shelfLayer) {
          dc->FillRoundedRectangle(shelfRect, shelfBrush.get());
          dc->DrawRoundedRectangle(shelfRect, shelfBorderBrush.get(), 1.0f);
        }

        if (!shelfLayer) {
          auto visuals = BuildItemVisuals(m_pointerAxis, m_hovering);
          for (auto const &visual : visuals) {
            auto const &item = m_items[visual.index];
            auto iconRect = D2D1::RectF(
                visual.iconRect.left * scale, visual.iconRect.top * scale,
                visual.iconRect.right * scale, visual.iconRect.bottom * scale);

            bool iconDrawn = false;
            if (!item.iconPath.empty()) {
              if (auto bitmap = LoadIconBitmap(item.iconPath)) {
                dc->DrawBitmap(bitmap, iconRect, 1.0f,
                               D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                iconDrawn = true;
              }
            }

            if (!iconDrawn) {
              const auto radius = 12.0f * scale;
              auto rounded = D2D1::RoundedRect(iconRect, radius, radius);
              dc->FillRoundedRectangle(rounded, fallbackIconBrush.get());
              auto label = Initials(item.displayName);
              dc->DrawTextW(label.c_str(), static_cast<UINT32>(label.size()),
                            m_labelFormat.get(), iconRect, textBrush.get());
            }

            if (item.running) {
              if (!IsVertical()) {
                const auto centerX = (iconRect.left + iconRect.right) / 2.0f;
                const auto dotY =
                    static_cast<float>((shelfEdge + 11.0) * scale);
                if (item.windows.size() > 1) {
                  auto pill = D2D1::RoundedRect(
                      D2D1::RectF(centerX - 11.0f * scale, dotY,
                                  centerX + 11.0f * scale, dotY + 4.0f * scale),
                      3.0f * scale, 3.0f * scale);
                  dc->FillRoundedRectangle(pill, indicatorBrush.get());
                } else {
                  dc->FillEllipse(D2D1::Ellipse({centerX, dotY + 3.5f * scale},
                                                3.5f * scale, 3.5f * scale),
                                  indicatorBrush.get());
                }
              } else {
                const auto centerY = (iconRect.top + iconRect.bottom) / 2.0f;
                const auto side =
                    m_settings.placement ==
                            DockWMac::platform::DockPlacement::Left
                        ? 1.0
                        : -1.0;
                const auto dotX =
                    static_cast<float>((shelfEdge + side * 11.0) * scale);
                if (item.windows.size() > 1) {
                  auto pill = D2D1::RoundedRect(
                      D2D1::RectF(dotX - 2.0f * scale, centerY - 11.0f * scale,
                                  dotX + 2.0f * scale, centerY + 11.0f * scale),
                      3.0f * scale, 3.0f * scale);
                  dc->FillRoundedRectangle(pill, indicatorBrush.get());
                } else {
                  dc->FillEllipse(D2D1::Ellipse({dotX, centerY}, 3.5f * scale,
                                                3.5f * scale),
                                  indicatorBrush.get());
                }
              }
            }
          }

          if (!m_statusMessage.empty() && m_statusFormat) {
            const auto widthDip = Unscale(m_width);
            const auto heightDip = Unscale(m_height);
            const auto statusWidth = std::clamp(widthDip * 0.46, 220.0, 420.0);
            constexpr auto statusHeight = 32.0;
            const auto left = (widthDip - statusWidth) / 2.0;
            const auto top =
                IsVertical()
                    ? 12.0
                    : std::clamp(static_cast<double>(shelfRectDip.top) - 48.0,
                                 8.0, heightDip - statusHeight - 8.0);
            const auto statusRect = D2D1::RoundedRect(
                D2D1::RectF(static_cast<float>(left * scale),
                            static_cast<float>(top * scale),
                            static_cast<float>((left + statusWidth) * scale),
                            static_cast<float>((top + statusHeight) * scale)),
                10.0f * scale, 10.0f * scale);
            dc->FillRoundedRectangle(statusRect, statusBrush.get());
            dc->DrawRoundedRectangle(statusRect, statusBorderBrush.get(), 1.0f);
            auto textRect = statusRect.rect;
            textRect.left += 12.0f * scale;
            textRect.right -= 12.0f * scale;
            dc->DrawTextW(m_statusMessage.c_str(),
                          static_cast<UINT32>(m_statusMessage.size()),
                          m_statusFormat.get(), textRect, textBrush.get(),
                          D2D1_DRAW_TEXT_OPTIONS_CLIP);
          }
        }

        hr = dc->EndDraw();
        dc->SetTarget(nullptr);
        auto endHr = surface->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET || endHr == D2DERR_RECREATE_TARGET) {
          DiscardGraphicsResources();
          CreateDeviceResources();
          CreateCompositionResources();
          return;
        }
        CheckStep(hr, shelfLayer ? L"ID2D1DeviceContext::EndDrawShelf"
                                 : L"ID2D1DeviceContext::EndDrawIcons");
        CheckStep(endHr, shelfLayer ? L"IDCompositionSurface::EndDrawShelf"
                                    : L"IDCompositionSurface::EndDrawIcons");
      }
      CheckStep(m_dcompDevice->Commit(), L"IDCompositionDevice::CommitRender");
    }

    void NativeDockHost::DiscardGraphicsResources() {
      m_iconBitmaps.clear();
      m_labelFormat = nullptr;
      m_statusFormat = nullptr;
      m_shelfSurface = nullptr;
      m_iconSurface = nullptr;
      m_shelfVisual = nullptr;
      m_iconVisual = nullptr;
      m_rootVisual = nullptr;
      m_dcompTarget = nullptr;
      m_dcompDevice = nullptr;
      m_d2dContext = nullptr;
      m_d2dDevice = nullptr;
      m_d2dFactory = nullptr;
      m_dwriteFactory = nullptr;
      m_wicFactory = nullptr;
      m_dxgiDevice = nullptr;
      m_d3dDevice = nullptr;
    }

    void NativeDockHost::StartAutoHideTimer() {
      if (!m_hwnd) {
        return;
      }
      SetTimer(m_hwnd, AutoHideTimerId, AutoHidePollMs, nullptr);
    }

    void NativeDockHost::StopAutoHideTimer()
    {
        if (m_hwnd)
        {
            KillTimer(m_hwnd, AutoHideTimerId);
        }
    }

    void NativeDockHost::UpdateAutoHide()
    {
        if (!AutoHideEnabled() || !m_visible || !m_hwnd)
        {
            return;
        }

        const auto cursor = DockWMac::platform::GetCursorPositionRelativeToWindow(
            m_hwnd,
            Unscale(m_width),
            Unscale(m_height));
        const auto now = GetTickCount64();
        if (cursor && cursor->insideWindow)
        {
            m_lastInsideAutoHideTick = now;
            if (m_autoHidden)
            {
                SetAutoHidden(false);
            }
            return;
        }

        if (!m_autoHidden &&
            !m_pointerPressed &&
            !m_dragging &&
            !m_menuOpen &&
            now - m_lastInsideAutoHideTick >= AutoHideCollapseDelayMs)
        {
            SetAutoHidden(true);
        }
    }

    void NativeDockHost::SetAutoHidden(bool hidden)
    {
        if (!AutoHideEnabled())
        {
            hidden = false;
        }

        if (m_autoHidden == hidden)
        {
            return;
        }

        if (hidden)
        {
            m_hovering = false;
            m_hoverIndex = std::nullopt;
            if (m_previewHideHandler)
            {
                m_previewHideHandler();
            }
        }
        else
        {
            m_lastInsideAutoHideTick = GetTickCount64();
        }

        m_autoHidden = hidden;
        ResizeAndPosition(m_visible);
        Render();
    }

    bool NativeDockHost::AutoHideEnabled() const
    {
        return m_settings.autoHide;
    }

    void NativeDockHost::StartHoverAnimation()
    {
        if (!m_hwnd || m_settings.reducedMotion || m_hoverAnimationRunning)
        {
            return;
        }

        const auto target = m_hovering ? 1.0 : 0.0;
        if (std::abs(target - m_hoverAmount) < 0.001)
        {
            m_hoverAmount = target;
            return;
        }

        m_lastHoverAnimationTick = GetTickCount64();
        m_hoverAnimationRunning = SetTimer(
            m_hwnd,
            HoverAnimationTimerId,
            HoverAnimationFrameMs,
            nullptr) != 0;
    }

    void NativeDockHost::StopHoverAnimation()
    {
        if (m_hwnd)
        {
            KillTimer(m_hwnd, HoverAnimationTimerId);
        }
        m_hoverAnimationRunning = false;
        m_lastHoverAnimationTick = 0;
    }

    void NativeDockHost::AdvanceHoverAnimation()
    {
        const auto now = GetTickCount64();
        const auto elapsed = m_lastHoverAnimationTick == 0
            ? static_cast<double>(HoverAnimationFrameMs)
            : static_cast<double>(now - m_lastHoverAnimationTick);
        m_lastHoverAnimationTick = now;
        m_hoverAmount = DockWMac::ui::AdvanceDockHoverAmount(
            m_hoverAmount,
            m_hovering,
            elapsed,
            m_settings.reducedMotion);
        Render();

        const auto target = m_hovering ? 1.0 : 0.0;
        if (m_hoverAmount == target)
        {
            StopHoverAnimation();
        }
    }

    int32_t NativeDockHost::Scale(double value) const
    {
        return static_cast<int32_t>(std::ceil(value * static_cast<double>(m_dpi) / USER_DEFAULT_SCREEN_DPI));
    }

    double NativeDockHost::Unscale(int32_t value) const
    {
        return static_cast<double>(value) * USER_DEFAULT_SCREEN_DPI / static_cast<double>(m_dpi);
    }

    double NativeDockHost::MainAxisLength(size_t visibleItems) const
    {
        if (visibleItems == 0)
        {
            return DockEndPadding * 2.0;
        }

        return DockEndPadding * 2.0 +
            static_cast<double>(visibleItems) * ItemSlot +
            static_cast<double>(visibleItems - 1) * ItemGap;
    }

    int32_t NativeDockHost::WindowWidth() const
    {
        auto logicalWidth = IsVertical()
            ? (std::max)(static_cast<double>(m_settings.dockHeight), DockCrossAxis)
            : (std::max)(static_cast<double>(m_settings.dockWidth), MainAxisLength(m_items.size()));
        return Scale(logicalWidth);
    }

    int32_t NativeDockHost::WindowHeight() const
    {
        auto logicalHeight = IsVertical()
            ? (std::max)(static_cast<double>(m_settings.dockWidth), MainAxisLength(m_items.size()))
            : (std::max)(static_cast<double>(m_settings.dockHeight), DockCrossAxis);
        return Scale(logicalHeight);
    }

    bool NativeDockHost::IsVertical() const
    {
        return DockWMac::ui::IsVerticalDockPlacement(m_settings.placement);
    }

    double NativeDockHost::MainExtentDip() const
    {
        return DockWMac::ui::DockMainAxisExtent(
            m_settings.placement,
            Unscale(m_width),
            Unscale(m_height));
    }

    double NativeDockHost::CrossExtentDip() const
    {
        return DockWMac::ui::DockCrossAxisExtent(
            m_settings.placement,
            Unscale(m_width),
            Unscale(m_height));
    }

    double NativeDockHost::MainAxisPosition(POINTS point) const
    {
        return DockWMac::ui::DockMainAxisPosition(
            m_settings.placement,
            Unscale(point.x),
            Unscale(point.y));
    }

    double NativeDockHost::ShelfTangentLine() const
    {
        const auto crossExtent = CrossExtentDip();
        switch (m_settings.placement)
        {
        case DockWMac::platform::DockPlacement::Left:
            return crossExtent - ShelfMargin - ShelfThickness;
        case DockWMac::platform::DockPlacement::Right:
            return ShelfMargin + ShelfThickness;
        case DockWMac::platform::DockPlacement::Bottom:
        default:
            return crossExtent - ShelfMargin - ShelfThickness;
        }
    }

    D2D1_RECT_F NativeDockHost::ShelfRectDip() const
    {
        const auto railLength = MainAxisLength(m_items.size());
        const auto railStart = (std::max)(0.0, (MainExtentDip() - railLength) / 2.0);
        const auto shelfEdge = ShelfTangentLine();

        switch (m_settings.placement)
        {
        case DockWMac::platform::DockPlacement::Left:
            return D2D1::RectF(
                static_cast<float>(shelfEdge),
                static_cast<float>(railStart),
                static_cast<float>(shelfEdge + ShelfThickness),
                static_cast<float>(railStart + railLength));
        case DockWMac::platform::DockPlacement::Right:
            return D2D1::RectF(
                static_cast<float>(shelfEdge - ShelfThickness),
                static_cast<float>(railStart),
                static_cast<float>(shelfEdge),
                static_cast<float>(railStart + railLength));
        case DockWMac::platform::DockPlacement::Bottom:
        default:
            return D2D1::RectF(
                static_cast<float>(railStart),
                static_cast<float>(shelfEdge),
                static_cast<float>(railStart + railLength),
                static_cast<float>(shelfEdge + ShelfThickness));
        }
    }

    std::vector<NativeDockHost::ItemVisual> NativeDockHost::BuildItemVisuals(double pointerAxis, bool hovering) const
    {
        std::vector<ItemVisual> visuals;
        const auto total = static_cast<double>(m_items.size()) * ItemSlot +
            static_cast<double>(m_items.empty() ? 0 : m_items.size() - 1) * ItemGap;
        const auto start = (std::max)(0.0, (MainExtentDip() - total) / 2.0);
        const auto shelfEdge = ShelfTangentLine();
        const auto shelfRect = ShelfRectDip();

        for (size_t index = 0; index < m_items.size(); ++index)
        {
            auto const& item = m_items[index];
            const auto center = start + static_cast<double>(index) * (ItemSlot + ItemGap) + ItemSlot / 2.0;
            auto pose = DockWMac::ui::CalculateDockIconPose(
                m_settings.placement,
                pointerAxis,
                center,
                shelfEdge,
                item.foreground,
                m_settings.reducedMotion || !hovering,
                IconMetrics(),
                m_hoverAmount);
            ItemVisual visual;
            visual.index = index;
            visual.iconRect = D2D1::RectF(
                static_cast<float>(pose.visualLeft),
                static_cast<float>(pose.visualTop),
                static_cast<float>(pose.visualRight),
                static_cast<float>(pose.visualBottom));
            if (!IsVertical())
            {
                visual.hitRect = D2D1::RectF(
                    static_cast<float>(center - ItemSlot / 2.0),
                    (std::min)(static_cast<float>(pose.visualTop - 12.0), shelfRect.top),
                    static_cast<float>(center + ItemSlot / 2.0),
                    (std::max)(static_cast<float>(pose.visualBottom + 12.0), shelfRect.bottom));
            }
            else
            {
                visual.hitRect = D2D1::RectF(
                    (std::min)(static_cast<float>(pose.visualLeft - 12.0), shelfRect.left),
                    static_cast<float>(center - ItemSlot / 2.0),
                    (std::max)(static_cast<float>(pose.visualRight + 12.0), shelfRect.right),
                    static_cast<float>(center + ItemSlot / 2.0));
            }
            visuals.push_back(visual);
        }

        return visuals;
    }

    std::optional<size_t> NativeDockHost::HitTest(POINTS point) const
    {
        const auto x = static_cast<float>(Unscale(point.x));
        const auto y = static_cast<float>(Unscale(point.y));
        for (auto const& visual : BuildItemVisuals(m_pointerAxis, m_hovering))
        {
            if (x >= visual.hitRect.left && x <= visual.hitRect.right &&
                y >= visual.hitRect.top && y <= visual.hitRect.bottom)
            {
                return visual.index;
            }
        }
        return std::nullopt;
    }

    void NativeDockHost::UpdateHover(POINTS point)
    {
        TrackMouseLeave();
        const auto wasHovering = m_hovering;
        m_hovering = true;
        m_lastInsideAutoHideTick = GetTickCount64();
        m_pointerAxis = MainAxisPosition(point);
        if (m_settings.reducedMotion)
        {
            m_hoverAmount = 1.0;
            StopHoverAnimation();
        }
        else if (!wasHovering)
        {
            StartHoverAnimation();
        }
        auto hover = HitTest(point);
        if (hover != m_hoverIndex)
        {
            m_hoverIndex = hover;
            if (hover)
            {
                ShowPreviewForItem(m_items[*hover]);
            }
            else if (m_previewHideHandler)
            {
                m_previewHideHandler();
            }
        }
        Render();
    }

    void NativeDockHost::ResetHover()
    {
        m_trackingMouse = false;
        m_hovering = false;
        m_hoverIndex = std::nullopt;
        if (m_previewHideHandler)
        {
            m_previewHideHandler();
        }
        if (m_settings.reducedMotion)
        {
            m_hoverAmount = 0.0;
            StopHoverAnimation();
        }
        else
        {
            StartHoverAnimation();
        }
        Render();
    }

    void NativeDockHost::ShowPreviewForItem(DockWMac::dock::DockItem const& item)
    {
        if (item.windows.empty())
        {
            return;
        }

        if (item.windows.size() > 1 && m_groupPreviewHandler && m_groupPreviewHandler(item.windows))
        {
            return;
        }

        if (!m_previewHandler)
        {
            return;
        }

        auto it = std::find_if(item.windows.begin(), item.windows.end(), [](auto const& window)
        {
            return !window.cloaked;
        });
        if (it == item.windows.end())
        {
            it = item.windows.begin();
        }
        m_previewHandler(*it);
    }

    void NativeDockHost::TrackMouseLeave()
    {
        if (m_trackingMouse)
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

    void NativeDockHost::ClearStatusMessage()
    {
        if (m_hwnd)
        {
            KillTimer(m_hwnd, StatusTimerId);
        }
        if (m_statusMessage.empty())
        {
            return;
        }

        m_statusMessage.clear();
        m_statusError = false;
        Render();
    }

    void NativeDockHost::BeginPointerPress(POINTS point)
    {
        TrackMouseLeave();
        m_lastInsideAutoHideTick = GetTickCount64();
        m_pointerPressed = true;
        m_dragging = false;
        m_pressPoint = point;
        m_pressedIndex = HitTest(point);
        m_dragIndex = m_pressedIndex;
        m_dragTargetIndex = m_pressedIndex;
        if (m_pressedIndex)
        {
            SetCapture(m_hwnd);
        }
    }

    void NativeDockHost::UpdateDrag(POINTS point)
    {
        TrackMouseLeave();
        m_hovering = true;
        m_lastInsideAutoHideTick = GetTickCount64();
        m_pointerAxis = MainAxisPosition(point);

        if (!m_pressedIndex || !m_dragIndex)
        {
            Render();
            return;
        }

        const auto dx = point.x - m_pressPoint.x;
        const auto dy = point.y - m_pressPoint.y;
        if (!m_dragging)
        {
            const auto threshold = GetSystemMetrics(SM_CXDRAG);
            if (std::abs(dx) < threshold && std::abs(dy) < threshold)
            {
                UpdateHover(point);
                return;
            }
            m_dragging = true;
            if (m_previewHideHandler)
            {
                m_previewHideHandler();
            }
        }

        auto insertIndex = CalculateInsertIndex(point);
        if (insertIndex > *m_dragIndex)
        {
            --insertIndex;
        }
        insertIndex = (std::min)(insertIndex, m_items.size() - 1);

        if (insertIndex == *m_dragIndex)
        {
            Render();
            return;
        }

        if (::DockWMac::dock::MoveDockItem(m_items, *m_dragIndex, insertIndex))
        {
            m_dragIndex = insertIndex;
            m_dragTargetIndex = insertIndex;
            m_hoverIndex = insertIndex;
        }
        Render();
    }

    void NativeDockHost::CompletePointerPress(POINTS point)
    {
        const auto wasDragging = m_dragging;

        if (GetCapture() == m_hwnd)
        {
            ReleaseCapture();
        }

        if (wasDragging && m_orderChangedHandler)
        {
            m_orderChangedHandler(CurrentOrder());
        }

        ResetDrag();
        if (!wasDragging)
        {
            if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
            {
                HandleLaunchNewInstance(point);
            }
            else
            {
                HandleClick(point);
            }
        }
        else
        {
            UpdateHover(point);
        }
    }

    void NativeDockHost::ResetDrag()
    {
        m_pointerPressed = false;
        m_dragging = false;
        m_pressedIndex = std::nullopt;
        m_dragIndex = std::nullopt;
        m_dragTargetIndex = std::nullopt;
    }

    size_t NativeDockHost::CalculateInsertIndex(POINTS point) const
    {
        if (m_items.empty())
        {
            return 0;
        }

        const auto axis = MainAxisPosition(point);
        const auto mainExtent = MainExtentDip();
        const auto total = static_cast<double>(m_items.size()) * ItemSlot +
            static_cast<double>(m_items.size() - 1) * ItemGap;
        const auto start = (std::max)(0.0, (mainExtent - total) / 2.0);
        const auto slot = ItemSlot + ItemGap;
        auto index = static_cast<int>((axis - start + ItemSlot / 2.0) / slot);
        index = std::clamp(index, 0, static_cast<int>(m_items.size()));
        return static_cast<size_t>(index);
    }

    std::vector<std::wstring> NativeDockHost::CurrentOrder() const
    {
        std::vector<std::wstring> order;
        order.reserve(m_items.size());
        for (auto const& item : m_items)
        {
            order.push_back(item.id);
        }
        return order;
    }

    void NativeDockHost::HandleClick(POINTS point)
    {
        if (!m_actionHandler)
        {
            return;
        }

        auto index = HitTest(point);
        if (!index || *index >= m_items.size())
        {
            return;
        }

        auto const& item = m_items[*index];
        auto action = DockWMac::dock::DecideClickAction(item);
        if (action.kind == DockWMac::dock::DockActionKind::ShowWindowChooser)
        {
            if (m_groupPreviewHandler && !item.windows.empty() && m_groupPreviewHandler(item.windows))
            {
                return;
            }
            ShowItemMenu(*index, point);
            return;
        }
        m_actionHandler(action);
    }

    void NativeDockHost::HandleLaunchNewInstance(POINTS point)
    {
        if (!m_actionHandler)
        {
            return;
        }

        auto const index = HitTest(point);
        if (!index || *index >= m_items.size())
        {
            return;
        }

        m_actionHandler({
            DockWMac::dock::DockActionKind::LaunchNewInstance,
            m_items[*index].id,
            nullptr,
        });
    }

    void NativeDockHost::HandleMenuSelect(WPARAM wparam, LPARAM lparam)
    {
        if (!m_menuOpen)
        {
            return;
        }

        const auto flags = HIWORD(wparam);
        if ((flags == 0xFFFF && lparam == 0) ||
            (flags & (MF_POPUP | MF_SEPARATOR | MF_DISABLED | MF_GRAYED)) != 0)
        {
            if (m_previewHideHandler)
            {
                m_previewHideHandler();
            }
            return;
        }

        const auto command = static_cast<UINT_PTR>(LOWORD(wparam));
        if (auto it = m_menuWindowCommands.find(command); it != m_menuWindowCommands.end() && m_previewHandler)
        {
            m_previewHandler(it->second);
            return;
        }

        if (m_previewHideHandler)
        {
            m_previewHideHandler();
        }
    }

    void NativeDockHost::ShowItemMenu(size_t index, POINTS point)
    {
        if (index >= m_items.size() || !m_actionHandler)
        {
            return;
        }

        auto const& item = m_items[index];
        m_menuWindowCommands.clear();
        HMENU menu = CreatePopupMenu();
        if (!menu)
        {
            return;
        }

        if (!item.windows.empty())
        {
            for (size_t windowIndex = 0; windowIndex < item.windows.size(); ++windowIndex)
            {
                auto const& window = item.windows[windowIndex];
                const auto commandId = MenuWindowBaseCommand + windowIndex;
                m_menuWindowCommands[commandId] = window;
                auto text = WindowMenuText(window);
                UINT flags = MF_STRING;
                if (window.foreground)
                {
                    flags |= MF_CHECKED;
                }
                if (window.cloaked)
                {
                    flags |= MF_GRAYED;
                }
                AppendMenuW(menu, flags, commandId, text.c_str());
            }
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        }
        AppendMenuW(menu, MF_STRING, MenuPinCommand, item.pinned ? L"Unpin from Dock" : L"Pin to Dock");
        if (!item.windows.empty())
        {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(
                menu,
                MF_STRING,
                item.windows.size() == 1 ? MenuCloseWindowCommand : MenuCloseAllWindowsCommand,
                item.windows.size() == 1 ? L"Close window" : L"Close all windows");
        }

        POINT screen{ point.x, point.y };
        ClientToScreen(m_hwnd, &screen);
        m_menuOpen = true;
        auto command = TrackPopupMenuEx(
            menu,
            TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NOANIMATION,
            screen.x,
            screen.y,
            m_hwnd,
            nullptr);
        m_menuOpen = false;
        m_lastInsideAutoHideTick = GetTickCount64();
        DestroyMenu(menu);
        m_menuWindowCommands.clear();
        if (m_previewHideHandler)
        {
            m_previewHideHandler();
        }

        if (command == MenuPinCommand)
        {
            m_actionHandler({
                item.pinned ? DockWMac::dock::DockActionKind::UnpinFromDock : DockWMac::dock::DockActionKind::PinToDock,
                item.id,
                nullptr,
            });
            return;
        }

        if (command == MenuCloseWindowCommand && item.windows.size() == 1)
        {
            m_actionHandler({
                DockWMac::dock::DockActionKind::CloseWindow,
                item.id,
                item.windows.front().hwnd,
            });
            return;
        }

        if (command == MenuCloseAllWindowsCommand && !item.windows.empty())
        {
            m_actionHandler({
                DockWMac::dock::DockActionKind::CloseAllWindows,
                item.id,
                nullptr,
            });
            return;
        }

        if (command >= MenuWindowBaseCommand)
        {
            const auto windowIndex = static_cast<size_t>(command - MenuWindowBaseCommand);
            if (windowIndex < item.windows.size())
            {
                m_actionHandler({
                    DockWMac::dock::DockActionKind::ActivateWindow,
                    item.id,
                    item.windows[windowIndex].hwnd,
                });
            }
        }
    }

    void NativeDockHost::ShowDockMenu(POINTS point)
    {
        if (!m_actionHandler)
        {
            return;
        }

        HMENU menu = CreatePopupMenu();
        HMENU placementMenu = CreatePopupMenu();
        if (!menu || !placementMenu)
        {
            if (placementMenu)
            {
                DestroyMenu(placementMenu);
            }
            if (menu)
            {
                DestroyMenu(menu);
            }
            return;
        }

        auto const placement = m_settings.placement;
        AppendMenuW(
            placementMenu,
            MF_STRING | (placement == DockWMac::platform::DockPlacement::Bottom ? MF_CHECKED : 0),
            MenuPlaceBottomCommand,
            L"Bottom");
        AppendMenuW(
            placementMenu,
            MF_STRING | (placement == DockWMac::platform::DockPlacement::Left ? MF_CHECKED : 0),
            MenuPlaceLeftCommand,
            L"Left");
        AppendMenuW(
            placementMenu,
            MF_STRING | (placement == DockWMac::platform::DockPlacement::Right ? MF_CHECKED : 0),
            MenuPlaceRightCommand,
            L"Right");

        AppendMenuW(
            menu,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(placementMenu),
            L"Dock position");
        AppendMenuW(
            menu,
            MF_STRING | (m_settings.autoHide ? MF_CHECKED : 0),
            MenuAutoHideCommand,
            L"Automatically hide Dock");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, MenuExitCommand, L"Exit Dock_WMac");

        POINT screen{ point.x, point.y };
        ClientToScreen(m_hwnd, &screen);
        m_menuOpen = true;
        auto const command = TrackPopupMenuEx(
            menu,
            TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NOANIMATION,
            screen.x,
            screen.y,
            m_hwnd,
            nullptr);
        m_menuOpen = false;
        m_lastInsideAutoHideTick = GetTickCount64();
        DestroyMenu(menu);

        DockWMac::dock::DockActionKind actionKind{ DockWMac::dock::DockActionKind::None };
        switch (command)
        {
        case MenuAutoHideCommand:
            actionKind = DockWMac::dock::DockActionKind::ToggleAutoHide;
            break;
        case MenuPlaceBottomCommand:
            actionKind = DockWMac::dock::DockActionKind::PlaceBottom;
            break;
        case MenuPlaceLeftCommand:
            actionKind = DockWMac::dock::DockActionKind::PlaceLeft;
            break;
        case MenuPlaceRightCommand:
            actionKind = DockWMac::dock::DockActionKind::PlaceRight;
            break;
        case MenuExitCommand:
            actionKind = DockWMac::dock::DockActionKind::ExitDock;
            break;
        default:
            break;
        }

        if (actionKind != DockWMac::dock::DockActionKind::None)
        {
            m_actionHandler({ actionKind, {}, nullptr });
        }
    }

    ID2D1Bitmap1* NativeDockHost::LoadIconBitmap(std::wstring const& path)
    {
        if (path.empty())
        {
            return nullptr;
        }
        if (auto it = m_iconBitmaps.find(path); it != m_iconBitmaps.end())
        {
            return it->second.get();
        }

        winrt::com_ptr<IWICBitmapDecoder> decoder;
        if (FAILED(m_wicFactory->CreateDecoderFromFilename(
            path.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            decoder.put())))
        {
            return nullptr;
        }

        winrt::com_ptr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(0, frame.put())))
        {
            return nullptr;
        }

        winrt::com_ptr<IWICFormatConverter> converter;
        if (FAILED(m_wicFactory->CreateFormatConverter(converter.put())))
        {
            return nullptr;
        }
        if (FAILED(converter->Initialize(
            frame.get(),
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom)))
        {
            return nullptr;
        }

        winrt::com_ptr<ID2D1Bitmap1> bitmap;
        D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            static_cast<FLOAT>(m_dpi),
            static_cast<FLOAT>(m_dpi));
        if (FAILED(m_d2dContext->CreateBitmapFromWicBitmap(converter.get(), &props, bitmap.put())))
        {
            return nullptr;
        }

        auto [it, inserted] = m_iconBitmaps.emplace(path, std::move(bitmap));
        UNREFERENCED_PARAMETER(inserted);
        return it->second.get();
    }

    void NativeDockHost::Trace(std::string_view message) const
    {
        if (m_diagnosticHandler)
        {
            m_diagnosticHandler(message);
        }
    }
}
