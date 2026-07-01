#pragma once

#include "DockWindow.g.h"
#include "../dock/DockModel.h"
#include "../infra/AppSettings.h"

namespace winrt::DockWMac::implementation
{
    struct DockWindow : DockWindowT<DockWindow>
    {
        DockWindow();

        using DockActionHandler = std::function<void(::DockWMac::dock::DockAction const&)>;
        using DockOrderChangedHandler = std::function<void(std::vector<std::wstring> const&)>;
        using DockPreviewHandler = std::function<void(HWND)>;
        using DockPreviewHideHandler = std::function<void()>;

        void Configure(
            ::DockWMac::infra::AppSettings const& settings,
            std::vector<::DockWMac::dock::DockItem> items,
            DockActionHandler actionHandler,
            DockOrderChangedHandler orderChangedHandler,
            DockPreviewHandler previewHandler,
            DockPreviewHideHandler previewHideHandler);
        void ApplyPlacement();

    private:
        bool IsVertical() const;
        int32_t WindowWidth() const;
        int32_t WindowHeight() const;
        HWND WindowHandle() const;
        void ConfigurePresenter();
        void BuildContent();
        void ShowDock();
        void HideDock();
        double DockAxisPosition(
            winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
            winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args) const;
        void UpdateItemTransforms(
            winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
            winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void ResetItemTransforms();
        void ShowWindowChooser(
            ::DockWMac::dock::DockItem const& item,
            winrt::Microsoft::UI::Xaml::FrameworkElement const& anchor);
        void HandleItemClick(
            ::DockWMac::dock::DockItem const& item,
            winrt::Microsoft::UI::Xaml::FrameworkElement const& anchor);
        std::optional<size_t> IndexOfItem(std::wstring const& itemId) const;
        size_t CalculateInsertIndex(
            winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
            winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args) const;
        void BeginDrag(std::wstring itemId);
        void UpdateDragTarget(
            winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
            winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void CompleteDrag();
        void ResetDrag();
        void NotifyOrderChanged();
        void ShowPreviewForItem(::DockWMac::dock::DockItem const& item);
        void HidePreview();

        ::DockWMac::infra::AppSettings m_settings;
        std::vector<::DockWMac::dock::DockItem> m_items;
        DockActionHandler m_actionHandler;
        DockOrderChangedHandler m_orderChangedHandler;
        DockPreviewHandler m_previewHandler;
        DockPreviewHideHandler m_previewHideHandler;
        std::vector<winrt::Microsoft::UI::Xaml::Media::CompositeTransform> m_itemTransforms;
        std::wstring m_dragItemId;
        std::optional<size_t> m_dragTargetIndex;
        bool m_dragMoved{};
        bool m_suppressNextClick{};
        bool m_hidden{};
    };
}

namespace winrt::DockWMac::factory_implementation
{
    struct DockWindow : DockWindowT<DockWindow, implementation::DockWindow>
    {
    };
}
