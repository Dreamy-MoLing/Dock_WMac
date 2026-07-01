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
        DockActionHandler actionHandler,
        DockOrderChangedHandler orderChangedHandler)
    {
        m_settings = settings;
        m_items = std::move(items);
        m_actionHandler = std::move(actionHandler);
        m_orderChangedHandler = std::move(orderChangedHandler);
        if (auto window = AppWindow())
        {
            window.Resize({ WindowWidth(), WindowHeight() });
        }
        BuildContent();
    }

    void DockWindow::ApplyPlacement()
    {
        if (m_settings.autoHide)
        {
            HideDock();
            return;
        }

        ShowDock();
    }

    bool DockWindow::IsVertical() const
    {
        return m_settings.placement == ::DockWMac::platform::DockPlacement::Left ||
            m_settings.placement == ::DockWMac::platform::DockPlacement::Right;
    }

    int32_t DockWindow::WindowWidth() const
    {
        return IsVertical() ? m_settings.dockHeight : m_settings.dockWidth;
    }

    int32_t DockWindow::WindowHeight() const
    {
        return IsVertical() ? m_settings.dockWidth : m_settings.dockHeight;
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
            window.Resize({ WindowWidth(), WindowHeight() });
        }
    }

    void DockWindow::ShowDock()
    {
        m_hidden = false;
        ::DockWMac::platform::ApplyDockWindowPlacement(
            WindowHandle(),
            m_settings.placement,
            WindowWidth(),
            WindowHeight());
    }

    void DockWindow::HideDock()
    {
        if (!m_settings.autoHide || !m_dragItemId.empty())
        {
            return;
        }

        m_hidden = true;
        ::DockWMac::platform::ApplyDockWindowAutoHidePlacement(
            WindowHandle(),
            m_settings.placement,
            WindowWidth(),
            WindowHeight());
    }

    void DockWindow::BuildContent()
    {
        namespace Controls = winrt::Microsoft::UI::Xaml::Controls;
        namespace Media = winrt::Microsoft::UI::Xaml::Media;
        namespace Xaml = winrt::Microsoft::UI::Xaml;

        auto root = Controls::Grid{};
        root.Background(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0x00, 0x00, 0x00, 0x00 } });
        root.PointerEntered([this](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
        {
            if (m_settings.autoHide && m_hidden)
            {
                ShowDock();
            }
        });
        root.PointerExited([this](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
        {
            if (m_settings.autoHide)
            {
                HideDock();
            }
        });
        root.PointerMoved([this, root](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
        {
            UpdateDragTarget(root, args);
        });
        root.PointerReleased([this](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
        {
            CompleteDrag();
        });

        auto shelf = Controls::Border{};
        shelf.CornerRadius({ 18, 18, 18, 18 });
        shelf.Padding({ 12, 8, 12, 10 });
        shelf.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
        shelf.VerticalAlignment(Xaml::VerticalAlignment::Center);
        shelf.Background(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xCC, 0x1C, 0x20, 0x28 } });
        shelf.BorderBrush(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0x33, 0xFF, 0xFF, 0xFF } });
        shelf.BorderThickness({ 1, 1, 1, 1 });

        auto row = Controls::StackPanel{};
        row.Orientation(IsVertical() ? Controls::Orientation::Vertical : Controls::Orientation::Horizontal);
        row.Spacing(10);
        shelf.Child(row);

        auto marker = [&]()
        {
            auto insert = Controls::Border{};
            if (IsVertical())
            {
                insert.Width(36);
                insert.Height(3);
            }
            else
            {
                insert.Width(3);
                insert.Height(36);
            }
            insert.CornerRadius({ 2, 2, 2, 2 });
            insert.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            insert.VerticalAlignment(Xaml::VerticalAlignment::Center);
            insert.Background(Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xFF, 0x7D, 0xE8, 0xFF } });
            return insert;
        };

        for (size_t index = 0; index < m_items.size(); ++index)
        {
            if (m_dragTargetIndex && *m_dragTargetIndex == index)
            {
                row.Children().Append(marker());
            }

            auto const& item = m_items[index];
            auto button = Controls::Button{};
            button.Width(IsVertical() ? 66 : 56);
            button.Height(IsVertical() ? 56 : 66);
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
                if (m_suppressNextClick)
                {
                    m_suppressNextClick = false;
                    return;
                }

                if (auto anchor = sender.try_as<Xaml::FrameworkElement>())
                {
                    HandleItemClick(item, anchor);
                }
            });
            button.PointerPressed([this, item](winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
            {
                if (auto buttonElement = sender.try_as<Xaml::UIElement>())
                {
                    buttonElement.CapturePointer(args.Pointer());
                }
                BeginDrag(item.id);
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

        if (m_dragTargetIndex && *m_dragTargetIndex == m_items.size())
        {
            row.Children().Append(marker());
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

    std::optional<size_t> DockWindow::IndexOfItem(std::wstring const& itemId) const
    {
        auto it = std::find_if(m_items.begin(), m_items.end(), [&](auto const& item)
        {
            return item.id == itemId;
        });
        if (it == m_items.end())
        {
            return std::nullopt;
        }
        return static_cast<size_t>(std::distance(m_items.begin(), it));
    }

    size_t DockWindow::CalculateInsertIndex(
        winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
        winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args) const
    {
        if (m_items.empty())
        {
            return 0;
        }

        auto point = args.GetCurrentPoint(root).Position();
        const auto axis = IsVertical() ? point.Y : point.X;
        const auto extent = IsVertical() ? root.ActualHeight() : root.ActualWidth();
        constexpr double itemExtent = 56.0;
        constexpr double spacing = 10.0;
        const auto total = static_cast<double>(m_items.size()) * itemExtent +
            static_cast<double>(m_items.size() - 1) * spacing;
        const auto start = (std::max)(0.0, (extent - total) / 2.0);
        const auto slot = itemExtent + spacing;
        auto index = static_cast<int>((axis - start + itemExtent / 2.0) / slot);
        index = std::clamp(index, 0, static_cast<int>(m_items.size()));
        return static_cast<size_t>(index);
    }

    void DockWindow::BeginDrag(std::wstring itemId)
    {
        if (itemId.empty())
        {
            return;
        }

        m_dragItemId = std::move(itemId);
        m_dragTargetIndex = IndexOfItem(m_dragItemId);
        m_dragMoved = false;
        m_suppressNextClick = false;
    }

    void DockWindow::UpdateDragTarget(
        winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
        winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (m_dragItemId.empty())
        {
            return;
        }

        if (!args.GetCurrentPoint(root).Properties().IsLeftButtonPressed())
        {
            CompleteDrag();
            return;
        }

        auto target = CalculateInsertIndex(root, args);
        if (!m_dragTargetIndex || *m_dragTargetIndex != target)
        {
            m_dragTargetIndex = target;
            m_dragMoved = true;
            BuildContent();
        }
    }

    void DockWindow::CompleteDrag()
    {
        if (m_dragItemId.empty())
        {
            return;
        }

        const auto dragged = m_dragMoved;
        auto from = IndexOfItem(m_dragItemId);
        auto target = m_dragTargetIndex.value_or(from.value_or(0));
        auto changed = false;

        if (from)
        {
            auto item = m_items[*from];
            m_items.erase(m_items.begin() + static_cast<ptrdiff_t>(*from));

            if (target > *from)
            {
                --target;
            }
            target = (std::min)(target, m_items.size());

            if (target != *from)
            {
                m_items.insert(m_items.begin() + static_cast<ptrdiff_t>(target), std::move(item));
                changed = true;
            }
            else
            {
                m_items.insert(m_items.begin() + static_cast<ptrdiff_t>(*from), std::move(item));
            }
        }

        ResetDrag();
        m_suppressNextClick = dragged;
        if (changed)
        {
            NotifyOrderChanged();
        }
        if (changed || dragged)
        {
            BuildContent();
        }
    }

    void DockWindow::ResetDrag()
    {
        m_dragItemId.clear();
        m_dragTargetIndex = std::nullopt;
        m_dragMoved = false;
    }

    void DockWindow::NotifyOrderChanged()
    {
        if (!m_orderChangedHandler)
        {
            return;
        }

        std::vector<std::wstring> order;
        order.reserve(m_items.size());
        for (auto const& item : m_items)
        {
            order.push_back(item.id);
        }
        m_orderChangedHandler(order);
    }
}
