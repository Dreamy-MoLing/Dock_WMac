#include "pch.h"
#include "DockWindow.xaml.h"
#include "../platform/DockPlacement.h"

#if __has_include("DockWindow.g.cpp")
#include "DockWindow.g.cpp"
#endif

namespace winrt::DockWMac::implementation
{
    namespace
    {
        winrt::Windows::UI::Color Color(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        {
            return winrt::Windows::UI::Color{ a, r, g, b };
        }

        std::wstring FileUri(std::wstring path)
        {
            if (path.empty())
            {
                return {};
            }

            std::replace(path.begin(), path.end(), L'\\', L'/');
            return L"file:///" + path;
        }

        winrt::Microsoft::UI::Xaml::Media::Brush DockSurfaceBrush(bool highContrast)
        {
            namespace Media = winrt::Microsoft::UI::Xaml::Media;
            if (highContrast)
            {
                return Media::SolidColorBrush{ Color(0xFF, 0x00, 0x00, 0x00) };
            }

            auto brush = Media::AcrylicBrush{};
            brush.TintColor(Color(0xFF, 0x10, 0x13, 0x1A));
            brush.TintOpacity(0.78);
            brush.FallbackColor(Color(0xEA, 0x10, 0x13, 0x1A));
            return brush;
        }

        winrt::Microsoft::UI::Xaml::Media::Brush Solid(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        {
            return winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{ Color(a, r, g, b) };
        }

        double ItemRailLength(size_t visibleItems)
        {
            if (visibleItems == 0)
            {
                return 0;
            }

            constexpr double itemExtent = 58.0;
            constexpr double itemGap = 10.0;
            constexpr double horizontalPadding = 28.0;
            return horizontalPadding +
                static_cast<double>(visibleItems) * itemExtent +
                static_cast<double>(visibleItems - 1) * itemGap;
        }
    }

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
        DockOrderChangedHandler orderChangedHandler,
        DockPreviewHandler previewHandler,
        DockPreviewHideHandler previewHideHandler)
    {
        m_settings = settings;
        m_items = std::move(items);
        m_actionHandler = std::move(actionHandler);
        m_orderChangedHandler = std::move(orderChangedHandler);
        m_previewHandler = std::move(previewHandler);
        m_previewHideHandler = std::move(previewHideHandler);
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
        auto logicalWidth = IsVertical() ? m_settings.dockHeight : m_settings.dockWidth;
        if (!IsVertical() && !m_items.empty())
        {
            constexpr int32_t itemExtent = 58;
            constexpr int32_t itemGap = 10;
            constexpr int32_t horizontalPadding = 34;
            auto contentWidth = horizontalPadding +
                static_cast<int32_t>(m_items.size()) * itemExtent +
                static_cast<int32_t>(m_items.size() - 1) * itemGap;
            logicalWidth = (std::max)(logicalWidth, (std::min)(contentWidth, 1500));
        }

        return ::DockWMac::platform::ScaleForWindow(
            WindowHandle(),
            logicalWidth);
    }

    int32_t DockWindow::WindowHeight() const
    {
        auto logicalHeight = IsVertical() ? m_settings.dockWidth : m_settings.dockHeight;
        if (IsVertical() && !m_items.empty())
        {
            constexpr int32_t itemExtent = 58;
            constexpr int32_t itemGap = 10;
            constexpr int32_t verticalPadding = 34;
            auto contentHeight = verticalPadding +
                static_cast<int32_t>(m_items.size()) * itemExtent +
                static_cast<int32_t>(m_items.size() - 1) * itemGap;
            logicalHeight = (std::max)(logicalHeight, (std::min)(contentHeight, 1500));
        }

        return ::DockWMac::platform::ScaleForWindow(
            WindowHandle(),
            logicalHeight);
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

        const auto visibleItems = m_items.size() + (m_dragTargetIndex ? 1 : 0);
        const auto railLength = ItemRailLength(visibleItems);
        const auto logicalRootWidth = IsVertical()
            ? static_cast<double>(m_settings.dockHeight)
            : (std::max)(static_cast<double>(m_settings.dockWidth), railLength + 6.0);
        const auto logicalRootHeight = IsVertical()
            ? (std::max)(static_cast<double>(m_settings.dockWidth), railLength + 6.0)
            : static_cast<double>(m_settings.dockHeight);

        auto root = Controls::Grid{};
        root.Width(logicalRootWidth);
        root.Height(logicalRootHeight);
        root.Background(m_settings.highContrast
            ? Solid(0xFF, 0x00, 0x00, 0x00)
            : Solid(0xFF, 0x10, 0x13, 0x1A));
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
        if (IsVertical())
        {
            shelf.Width(42);
            shelf.Height(railLength);
            shelf.Margin({ 0, 12, 0, 12 });
        }
        else
        {
            shelf.Width(railLength);
            shelf.Height(42);
            shelf.Margin({ 0, 0, 0, 10 });
        }
        shelf.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
        shelf.VerticalAlignment(IsVertical() ? Xaml::VerticalAlignment::Center : Xaml::VerticalAlignment::Bottom);
        shelf.Background(DockSurfaceBrush(m_settings.highContrast));
        shelf.BorderBrush(Media::SolidColorBrush{
            m_settings.highContrast
                ? Color(0xFF, 0xFF, 0xFF, 0xFF)
                : Color(0x00, 0xFF, 0xFF, 0xFF)
        });
        shelf.BorderThickness(m_settings.highContrast
            ? winrt::Microsoft::UI::Xaml::Thickness{ 1, 1, 1, 1 }
            : winrt::Microsoft::UI::Xaml::Thickness{ 0, 0, 0, 0 });
        shelf.Opacity(1.0);

        auto row = Controls::StackPanel{};
        row.Orientation(IsVertical() ? Controls::Orientation::Vertical : Controls::Orientation::Horizontal);
        row.Spacing(10);
        row.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
        row.VerticalAlignment(Xaml::VerticalAlignment::Center);

        auto marker = [&]()
        {
            auto insert = Controls::Border{};
            if (IsVertical())
            {
                insert.Width(40);
                insert.Height(3);
            }
            else
            {
                insert.Width(3);
                insert.Height(40);
            }
            insert.CornerRadius({ 2, 2, 2, 2 });
            insert.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            insert.VerticalAlignment(Xaml::VerticalAlignment::Center);
            insert.Background(Solid(0xFF, 0x7D, 0xE8, 0xFF));
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
            button.MinWidth(0);
            button.MinHeight(0);
            button.Width(IsVertical() ? 68 : 58);
            button.Height(IsVertical() ? 58 : 70);
            button.Padding({ 0, 0, 0, 0 });
            button.Background(Solid(0x00, 0x00, 0x00, 0x00));
            button.BorderThickness({ 0, 0, 0, 0 });

            auto transform = Media::CompositeTransform{};
            if (item.foreground)
            {
                transform.TranslateY(IsVertical() ? 0 : -6);
            }
            button.RenderTransform(transform);
            button.RenderTransformOrigin({ 0.5, 1.0 });

            auto cell = Controls::Grid{};
            auto rows = cell.RowDefinitions();
            rows.Append(Controls::RowDefinition{});
            rows.Append(Controls::RowDefinition{});

            auto glyph = Controls::Border{};
            const auto hasIcon = !item.iconPath.empty();
            glyph.Width(56);
            glyph.Height(56);
            glyph.CornerRadius({ hasIcon ? 8.0 : 14.0, hasIcon ? 8.0 : 14.0, hasIcon ? 8.0 : 14.0, hasIcon ? 8.0 : 14.0 });
            glyph.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            glyph.VerticalAlignment(Xaml::VerticalAlignment::Center);
            glyph.Background(hasIcon && !m_settings.highContrast
                ? Solid(0x00, 0x00, 0x00, 0x00)
                : item.running
                ? Solid(0xFF, 0x25, 0x78, 0xB7)
                : Solid(0xFF, 0x4B, 0x55, 0x66));
            glyph.BorderThickness(m_settings.highContrast
                ? winrt::Microsoft::UI::Xaml::Thickness{ 1, 1, 1, 1 }
                : winrt::Microsoft::UI::Xaml::Thickness{ 0, 0, 0, 0 });
            glyph.BorderBrush(Solid(0xFF, 0xFF, 0xFF, 0xFF));

            if (hasIcon)
            {
                auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage{};
                bitmap.UriSource(winrt::Windows::Foundation::Uri{ winrt::hstring{ FileUri(item.iconPath) } });

                auto image = Controls::Image{};
                image.Width(52);
                image.Height(52);
                image.Stretch(Media::Stretch::Uniform);
                image.Source(bitmap);
                image.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
                image.VerticalAlignment(Xaml::VerticalAlignment::Center);
                glyph.Child(image);
            }
            else
            {
                auto label = Controls::TextBlock{};
                auto text = item.displayName.empty() ? L"?" : std::wstring{ 1, static_cast<wchar_t>(::towupper(item.displayName.front())) };
                label.Text(text);
                label.FontSize(22);
                label.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                label.Foreground(Solid(0xFF, 0xFF, 0xFF, 0xFF));
                label.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
                label.VerticalAlignment(Xaml::VerticalAlignment::Center);
                glyph.Child(label);
            }

            Controls::Grid::SetRow(glyph, 0);
            cell.Children().Append(glyph);

            auto indicator = Controls::Border{};
            indicator.Width(item.windows.size() > 1 ? 20 : 6);
            indicator.Height(item.running ? (item.windows.size() > 1 ? 4 : 6) : 0);
            indicator.Margin({ 0, 3, 0, 0 });
            indicator.CornerRadius({ 4, 4, 4, 4 });
            indicator.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            indicator.Background(Media::SolidColorBrush{
                m_settings.highContrast
                    ? Color(0xFF, 0xFF, 0xFF, 0x00)
                    : Color(0xFF, 0x60, 0xCD, 0xFF)
            });
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
            button.RightTapped([this, item](winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args)
            {
                if (!m_actionHandler)
                {
                    return;
                }

                auto flyout = Controls::MenuFlyout{};
                auto command = Controls::MenuFlyoutItem{};
                auto kind = item.pinned
                    ? ::DockWMac::dock::DockActionKind::UnpinFromDock
                    : ::DockWMac::dock::DockActionKind::PinToDock;
                command.Text(item.pinned ? L"Unpin from Dock" : L"Pin to Dock");
                command.Click([this, item, kind](winrt::Windows::Foundation::IInspectable const&, Xaml::RoutedEventArgs const&)
                {
                    m_actionHandler(::DockWMac::dock::DockAction{ kind, item.id, nullptr });
                });
                flyout.Items().Append(command);

                if (auto anchor = sender.try_as<Xaml::FrameworkElement>())
                {
                    flyout.ShowAt(anchor);
                    args.Handled(true);
                }
            });
            button.PointerPressed([this, item](winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
            {
                if (auto buttonElement = sender.try_as<Xaml::UIElement>())
                {
                    if (!args.GetCurrentPoint(buttonElement).Properties().IsLeftButtonPressed())
                    {
                        return;
                    }
                    buttonElement.CapturePointer(args.Pointer());
                }
                BeginDrag(item.id);
            });

            button.PointerEntered([this, transform, item](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
            {
                if (!m_settings.reducedMotion)
                {
                    transform.ScaleX(1.32);
                    transform.ScaleY(1.32);
                    transform.TranslateY(IsVertical() ? 0 : -12);
                }
                ShowPreviewForItem(item);
            });
            button.PointerExited([this, transform, item](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
            {
                transform.ScaleX(1.0);
                transform.ScaleY(1.0);
                transform.TranslateY(item.foreground && !IsVertical() ? -6 : 0);
                HidePreview();
            });

            row.Children().Append(button);
        }

        if (m_dragTargetIndex && *m_dragTargetIndex == m_items.size())
        {
            row.Children().Append(marker());
        }

        root.Children().Append(shelf);
        root.Children().Append(row);
        Content(root);
        ::DockWMac::platform::ApplyDockWindowShape(
            WindowHandle(),
            m_settings.placement,
            WindowWidth(),
            WindowHeight(),
            visibleItems);
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
            menuItem.PointerEntered([this, window](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
            {
                if (m_previewHandler && !window.cloaked && !window.minimized)
                {
                    m_previewHandler(window.hwnd);
                }
            });
            menuItem.PointerExited([this](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
            {
                HidePreview();
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
        constexpr double itemExtent = 58.0;
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

    void DockWindow::ShowPreviewForItem(::DockWMac::dock::DockItem const& item)
    {
        if (!m_previewHandler || item.windows.empty())
        {
            return;
        }

        auto it = std::find_if(item.windows.begin(), item.windows.end(), [](auto const& window)
        {
            return !window.minimized && !window.cloaked;
        });
        if (it != item.windows.end())
        {
            m_previewHandler(it->hwnd);
        }
    }

    void DockWindow::HidePreview()
    {
        if (m_previewHideHandler)
        {
            m_previewHideHandler();
        }
    }
}
