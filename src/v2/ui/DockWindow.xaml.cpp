#include "pch.h"
#include "DockWindow.xaml.h"
#include "../platform/DockPlacement.h"

#if __has_include("DockWindow.g.cpp")
#include "DockWindow.g.cpp"
#endif

namespace winrt::DockWMac::implementation
{
    DockWindow::DockWindow()
    {
        Title(L"DockWindow");
        ExtendsContentIntoTitleBar(true);
        ConfigurePresenter();
        BuildContent();
    }

    void DockWindow::Configure(
        ::DockWMac::infra::AppSettings const& settings,
        std::vector<::DockWMac::dock::DockItem> items,
        DockActionHandler actionHandler)
    {
        m_settings = settings;
        m_items = std::move(items);
        m_actionHandler = std::move(actionHandler);
        if (auto window = AppWindow())
        {
            window.Resize({ m_settings.dockWidth, m_settings.dockHeight });
        }
        BuildContent();
    }

    void DockWindow::ApplyPlacement()
    {
        ::DockWMac::platform::ApplyDockWindowPlacement(
            WindowHandle(),
            m_settings.placement,
            m_settings.dockWidth,
            m_settings.dockHeight);
    }

    HWND DockWindow::WindowHandle() const
    {
        HWND hwnd{};
        auto native = this->try_as<IWindowNative>();
        if (native)
        {
            winrt::check_hresult(native->get_WindowHandle(&hwnd));
        }
        return hwnd;
    }

    void DockWindow::ConfigurePresenter()
    {
        if (auto window = AppWindow())
        {
            if (auto presenter = window.Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>())
            {
                presenter.SetBorderAndTitleBar(false, false);
                presenter.IsMaximizable(false);
                presenter.IsMinimizable(false);
                presenter.IsResizable(false);
            }
            window.Resize({ m_settings.dockWidth, m_settings.dockHeight });
        }
    }

    void DockWindow::BuildContent()
    {
        namespace Controls = winrt::Microsoft::UI::Xaml::Controls;
        namespace Media = winrt::Microsoft::UI::Xaml::Media;
        namespace Shapes = winrt::Microsoft::UI::Xaml::Shapes;
        namespace Xaml = winrt::Microsoft::UI::Xaml;

        auto root = Controls::Grid{};
        root.Background(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0x00, 0x00, 0x00, 0x00 } });

        auto shelf = Controls::Border{};
        shelf.CornerRadius({ 18, 18, 18, 18 });
        shelf.Padding({ 12, 8, 12, 10 });
        shelf.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
        shelf.VerticalAlignment(Xaml::VerticalAlignment::Center);
        shelf.Background(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xCC, 0x1C, 0x20, 0x28 } });
        shelf.BorderBrush(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0x33, 0xFF, 0xFF, 0xFF } });
        shelf.BorderThickness({ 1, 1, 1, 1 });

        auto row = Controls::StackPanel{};
        row.Orientation(Controls::Orientation::Horizontal);
        row.Spacing(10);
        shelf.Child(row);

        for (auto const& item : m_items)
        {
            auto button = Controls::Button{};
            button.Width(56);
            button.Height(66);
            button.Padding({ 0, 0, 0, 0 });
            button.Background(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0x00, 0x00, 0x00, 0x00 } });
            button.BorderThickness({ 0, 0, 0, 0 });

            auto transform = Media::CompositeTransform{};
            if (item.foreground)
            {
                transform.TranslateY(-4);
            }
            button.RenderTransform(transform);
            button.RenderTransformOrigin({ 0.5, 1.0 });

            auto cell = Controls::Grid{};
            auto rows = cell.RowDefinitions();
            rows.Append(Controls::RowDefinition{});
            rows.Append(Controls::RowDefinition{});

            auto glyph = Controls::Border{};
            glyph.Width(46);
            glyph.Height(46);
            glyph.CornerRadius({ 12, 12, 12, 12 });
            glyph.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            glyph.Background(Media::SolidColorBrush{
                item.running
                    ? winrt::Windows::UI::Color{ 0xFF, 0x31, 0x8F, 0xD8 }
                    : winrt::Windows::UI::Color{ 0xFF, 0x4B, 0x55, 0x66 }
            });

            auto label = Controls::TextBlock{};
            auto text = item.displayName.empty() ? L"?" : std::wstring{ 1, static_cast<wchar_t>(::towupper(item.displayName.front())) };
            label.Text(text);
            label.FontSize(20);
            label.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            label.Foreground(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xFF, 0xFF, 0xFF, 0xFF } });
            label.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            label.VerticalAlignment(Xaml::VerticalAlignment::Center);
            glyph.Child(label);

            Controls::Grid::SetRow(glyph, 0);
            cell.Children().Append(glyph);

            auto indicator = Controls::Border{};
            indicator.Width(item.windows.size() > 1 ? 20 : 6);
            indicator.Height(item.running ? (item.windows.size() > 1 ? 4 : 6) : 0);
            indicator.Margin({ 0, 5, 0, 0 });
            indicator.CornerRadius({ 4, 4, 4, 4 });
            indicator.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            indicator.Background(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xFF, 0x60, 0xCD, 0xFF } });
            Controls::Grid::SetRow(indicator, 1);
            cell.Children().Append(indicator);

            button.Content(cell);
            button.Click([this, item](winrt::Windows::Foundation::IInspectable const& sender, Xaml::RoutedEventArgs const&)
            {
                if (auto anchor = sender.try_as<Xaml::FrameworkElement>())
                {
                    HandleItemClick(item, anchor);
                }
            });

            button.PointerEntered([this, transform](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
            {
                if (!m_settings.reducedMotion)
                {
                    transform.ScaleX(1.18);
                    transform.ScaleY(1.18);
                    transform.TranslateY(-8);
                }
            });
            button.PointerExited([this, transform, item](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
            {
                transform.ScaleX(1.0);
                transform.ScaleY(1.0);
                transform.TranslateY(item.foreground ? -4 : 0);
            });

            row.Children().Append(button);
        }

        root.Children().Append(shelf);
        Content(root);
    }

    void DockWindow::ShowWindowChooser(
        ::DockWMac::dock::DockItem const& item,
        winrt::Microsoft::UI::Xaml::FrameworkElement const& anchor)
    {
        namespace Controls = winrt::Microsoft::UI::Xaml::Controls;
        auto flyout = Controls::MenuFlyout{};
        for (auto const& window : item.windows)
        {
            auto menuItem = Controls::MenuFlyoutItem{};
            menuItem.Text(window.title.empty() ? L"Window" : window.title);
            menuItem.Click([this, window](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
            {
                if (m_actionHandler)
                {
                    m_actionHandler(::DockWMac::dock::DockAction{ ::DockWMac::dock::DockActionKind::ActivateWindow, L"", window.hwnd });
                }
            });
            flyout.Items().Append(menuItem);
        }
        flyout.ShowAt(anchor);
    }

    void DockWindow::HandleItemClick(
        ::DockWMac::dock::DockItem const& item,
        winrt::Microsoft::UI::Xaml::FrameworkElement const& anchor)
    {
        auto action = ::DockWMac::dock::DecideClickAction(item);
        if (action.kind == ::DockWMac::dock::DockActionKind::ShowWindowChooser)
        {
            ShowWindowChooser(item, anchor);
            return;
        }

        if (m_actionHandler)
        {
            m_actionHandler(action);
        }
    }
}
