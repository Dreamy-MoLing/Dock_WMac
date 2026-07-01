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

        void Configure(
            ::DockWMac::infra::AppSettings const& settings,
            std::vector<::DockWMac::dock::DockItem> items,
            DockActionHandler actionHandler);
        void ApplyPlacement();

    private:
        HWND WindowHandle() const;
        void ConfigurePresenter();
        void BuildContent();
        void ShowWindowChooser(
            ::DockWMac::dock::DockItem const& item,
            winrt::Microsoft::UI::Xaml::FrameworkElement const& anchor);
        void HandleItemClick(
            ::DockWMac::dock::DockItem const& item,
            winrt::Microsoft::UI::Xaml::FrameworkElement const& anchor);

        ::DockWMac::infra::AppSettings m_settings;
        std::vector<::DockWMac::dock::DockItem> m_items;
        DockActionHandler m_actionHandler;
    };
}

namespace winrt::DockWMac::factory_implementation
{
    struct DockWindow : DockWindowT<DockWindow, implementation::DockWindow>
    {
    };
}
