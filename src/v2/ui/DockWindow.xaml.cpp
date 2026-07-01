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
        constexpr double Pi = 3.14159265358979323846;
        constexpr double IconSize = 64.0;
        constexpr double IconImageSize = 62.0;
        constexpr double ItemSlot = 72.0;
        constexpr double ItemGap = 10.0;
        constexpr double DockCrossAxis = 152.0;
        constexpr double DockEndPadding = 42.0;
        constexpr double ShelfThickness = 46.0;
        constexpr double ShelfRadius = 22.0;
        constexpr double MaxMagnification = 1.68;
        constexpr double HoverLift = 14.0;
        constexpr double ActiveLift = 6.0;

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

        winrt::Microsoft::UI::Xaml::Media::Brush Solid(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        {
            return winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{ Color(a, r, g, b) };
        }

        winrt::Microsoft::UI::Xaml::Media::Brush DockSurfaceBrush(bool highContrast)
        {
            namespace Media = winrt::Microsoft::UI::Xaml::Media;
            if (highContrast)
            {
                return Media::SolidColorBrush{ Color(0xFF, 0x00, 0x00, 0x00) };
            }

            auto brush = Media::AcrylicBrush{};
            brush.TintColor(Color(0xFF, 0x12, 0x16, 0x20));
            brush.TintOpacity(0.58);
            brush.FallbackColor(Color(0xE8, 0x12, 0x16, 0x20));
            return brush;
        }

        double MainAxisLength(size_t visibleItems)
        {
            if (visibleItems == 0)
            {
                return DockEndPadding * 2.0;
            }

            return DockEndPadding * 2.0 +
                static_cast<double>(visibleItems) * ItemSlot +
                static_cast<double>(visibleItems - 1) * ItemGap;
        }

        double MagnificationCurve(double distance)
        {
            const auto range = IconSize * 2.2;
            const auto t = std::clamp(1.0 - distance / range, 0.0, 1.0);
            return 0.5 - 0.5 * std::cos(t * Pi);
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
        auto logicalWidth = IsVertical()
            ? (std::max)(static_cast<double>(m_settings.dockHeight), DockCrossAxis)
            : (std::max)(static_cast<double>(m_settings.dockWidth), MainAxisLength(m_items.size()));

        return ::DockWMac::platform::ScaleForWindow(
            WindowHandle(),
            static_cast<int32_t>(std::ceil(logicalWidth)));
    }

    int32_t DockWindow::WindowHeight() const
    {
        auto logicalHeight = IsVertical()
            ? (std::max)(static_cast<double>(m_settings.dockWidth), MainAxisLength(m_items.size()))
            : (std::max)(static_cast<double>(m_settings.dockHeight), DockCrossAxis);

        return ::DockWMac::platform::ScaleForWindow(
            WindowHandle(),
            static_cast<int32_t>(std::ceil(logicalHeight)));
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

    double DockWindow::DockAxisPosition(
        winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
        winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args) const
    {
        auto point = args.GetCurrentPoint(root).Position();
        return IsVertical() ? point.Y : point.X;
    }

    void DockWindow::UpdateItemTransforms(
        winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
        winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (m_itemTransforms.empty() || m_settings.reducedMotion)
        {
            ResetItemTransforms();
            return;
        }

        const auto axis = DockAxisPosition(root, args);
        const auto extent = IsVertical() ? root.ActualHeight() : root.ActualWidth();
        const auto total = static_cast<double>(m_items.size()) * ItemSlot +
            static_cast<double>(m_items.empty() ? 0 : m_items.size() - 1) * ItemGap;
        const auto start = (std::max)(0.0, (extent - total) / 2.0);

        for (size_t index = 0; index < m_itemTransforms.size() && index < m_items.size(); ++index)
        {
            auto const& item = m_items[index];
            auto transform = m_itemTransforms[index];
            const auto center = start + static_cast<double>(index) * (ItemSlot + ItemGap) + ItemSlot / 2.0;
            const auto curve = MagnificationCurve(std::abs(axis - center));
            const auto scale = 1.0 + (MaxMagnification - 1.0) * curve;
            const auto lift = HoverLift * curve + (item.foreground ? ActiveLift : 0.0);

            transform.ScaleX(scale);
            transform.ScaleY(scale);
            transform.TranslateY(0.0);
            transform.TranslateX(0.0);

            if (IsVertical())
            {
                transform.TranslateX(m_settings.placement == ::DockWMac::platform::DockPlacement::Left ? lift : -lift);
            }
            else
            {
                transform.TranslateY(-lift);
            }
        }
    }

    void DockWindow::ResetItemTransforms()
    {
        for (size_t index = 0; index < m_itemTransforms.size() && index < m_items.size(); ++index)
        {
            auto transform = m_itemTransforms[index];
            transform.ScaleX(1.0);
            transform.ScaleY(1.0);
            transform.TranslateX(0.0);
            transform.TranslateY(0.0);
            if (m_items[index].foreground)
            {
                if (IsVertical())
                {
                    transform.TranslateX(m_settings.placement == ::DockWMac::platform::DockPlacement::Left ? ActiveLift : -ActiveLift);
                }
                else
                {
                    transform.TranslateY(-ActiveLift);
                }
            }
        }
    }

    void DockWindow::BuildContent()
    {
        namespace Controls = winrt::Microsoft::UI::Xaml::Controls;
        namespace Media = winrt::Microsoft::UI::Xaml::Media;
        namespace Xaml = winrt::Microsoft::UI::Xaml;

        m_itemTransforms.clear();

        const auto visibleItems = m_items.size() + (m_dragTargetIndex ? 1 : 0);
        const auto railLength = MainAxisLength(visibleItems);
        const auto logicalRootWidth = IsVertical()
            ? DockCrossAxis
            : (std::max)(static_cast<double>(m_settings.dockWidth), railLength);
        const auto logicalRootHeight = IsVertical()
            ? (std::max)(static_cast<double>(m_settings.dockWidth), railLength)
            : DockCrossAxis;

        auto root = Controls::Grid{};
        root.Width(logicalRootWidth);
        root.Height(logicalRootHeight);
        root.Background(m_settings.highContrast
            ? Solid(0xFF, 0x00, 0x00, 0x00)
            : Solid(0x01, 0x00, 0x00, 0x00));
        root.PointerEntered([this](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
        {
            if (m_settings.autoHide && m_hidden)
            {
                ShowDock();
            }
        });
        root.PointerExited([this](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
        {
            ResetItemTransforms();
            HidePreview();
            if (m_settings.autoHide)
            {
                HideDock();
            }
        });
        root.PointerMoved([this, root](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
        {
            UpdateItemTransforms(root, args);
            UpdateDragTarget(root, args);
        });
        root.PointerReleased([this](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
        {
            CompleteDrag();
        });

        auto shelf = Controls::Border{};
        shelf.CornerRadius({ ShelfRadius, ShelfRadius, ShelfRadius, ShelfRadius });
        if (IsVertical())
        {
            shelf.Width(ShelfThickness);
            shelf.Height(railLength);
            shelf.Margin({ 0, DockEndPadding * 0.35, 0, DockEndPadding * 0.35 });
            shelf.HorizontalAlignment(m_settings.placement == ::DockWMac::platform::DockPlacement::Left
                ? Xaml::HorizontalAlignment::Right
                : Xaml::HorizontalAlignment::Left);
            shelf.VerticalAlignment(Xaml::VerticalAlignment::Center);
        }
        else
        {
            shelf.Width(railLength);
            shelf.Height(ShelfThickness);
            shelf.Margin({ 0, 0, 0, 18 });
            shelf.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            shelf.VerticalAlignment(Xaml::VerticalAlignment::Bottom);
        }
        shelf.Background(DockSurfaceBrush(m_settings.highContrast));
        shelf.BorderBrush(m_settings.highContrast
            ? Solid(0xFF, 0xFF, 0xFF, 0xFF)
            : Solid(0x3A, 0xFF, 0xFF, 0xFF));
        shelf.BorderThickness({ 1, 1, 1, 1 });

        auto row = Controls::StackPanel{};
        row.Orientation(IsVertical() ? Controls::Orientation::Vertical : Controls::Orientation::Horizontal);
        row.Spacing(ItemGap);
        row.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
        row.VerticalAlignment(IsVertical() ? Xaml::VerticalAlignment::Center : Xaml::VerticalAlignment::Bottom);
        row.Margin(IsVertical()
            ? winrt::Microsoft::UI::Xaml::Thickness{ 0, DockEndPadding, 0, DockEndPadding }
            : winrt::Microsoft::UI::Xaml::Thickness{ DockEndPadding, 0, DockEndPadding, 28 });

        auto marker = [&]()
        {
            auto insert = Controls::Border{};
            if (IsVertical())
            {
                insert.Width(48);
                insert.Height(4);
            }
            else
            {
                insert.Width(4);
                insert.Height(54);
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
            button.Width(ItemSlot);
            button.Height(IsVertical() ? ItemSlot : 104);
            button.Padding({ 0, 0, 0, 0 });
            button.Background(Solid(0x00, 0x00, 0x00, 0x00));
            button.BorderBrush(Solid(0x00, 0x00, 0x00, 0x00));
            button.BorderThickness({ 0, 0, 0, 0 });

            auto transform = Media::CompositeTransform{};
            transform.CenterX(ItemSlot / 2.0);
            transform.CenterY(IsVertical() ? ItemSlot / 2.0 : 92.0);
            button.RenderTransform(transform);
            button.RenderTransformOrigin({ 0.5f, IsVertical() ? 0.5f : 1.0f });
            m_itemTransforms.push_back(transform);

            auto cell = Controls::Grid{};
            cell.Width(ItemSlot);
            cell.Height(IsVertical() ? ItemSlot : 96);

            auto glyph = Controls::Border{};
            const auto hasIcon = !item.iconPath.empty();
            glyph.Width(IconSize);
            glyph.Height(IconSize);
            glyph.CornerRadius({ hasIcon ? 10.0 : 16.0, hasIcon ? 10.0 : 16.0, hasIcon ? 10.0 : 16.0, hasIcon ? 10.0 : 16.0 });
            glyph.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            glyph.VerticalAlignment(Xaml::VerticalAlignment::Top);
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
                bitmap.DecodePixelWidth(static_cast<int32_t>(IconImageSize * MaxMagnification));
                bitmap.DecodePixelHeight(static_cast<int32_t>(IconImageSize * MaxMagnification));
                bitmap.UriSource(winrt::Windows::Foundation::Uri{ winrt::hstring{ FileUri(item.iconPath) } });

                auto image = Controls::Image{};
                image.Width(IconImageSize);
                image.Height(IconImageSize);
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
                label.FontSize(24);
                label.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                label.Foreground(Solid(0xFF, 0xFF, 0xFF, 0xFF));
                label.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
                label.VerticalAlignment(Xaml::VerticalAlignment::Center);
                glyph.Child(label);
            }
            cell.Children().Append(glyph);

            auto indicator = Controls::Border{};
            indicator.Width(item.windows.size() > 1 ? 22 : 7);
            indicator.Height(item.running ? (item.windows.size() > 1 ? 4 : 7) : 0);
            indicator.CornerRadius({ 4, 4, 4, 4 });
            indicator.HorizontalAlignment(Xaml::HorizontalAlignment::Center);
            indicator.VerticalAlignment(Xaml::VerticalAlignment::Bottom);
            indicator.Background(m_settings.highContrast
                ? Solid(0xFF, 0xFF, 0xFF, 0x00)
                : Solid(0xFF, 0x60, 0xCD, 0xFF));
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
            button.PointerEntered([this, item](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
            {
                ShowPreviewForItem(item);
            });
            button.PointerExited([this](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&)
            {
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
        ResetItemTransforms();
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

        const auto axis = DockAxisPosition(root, args);
        const auto extent = IsVertical() ? root.ActualHeight() : root.ActualWidth();
        const auto total = static_cast<double>(m_items.size()) * ItemSlot +
            static_cast<double>(m_items.size() - 1) * ItemGap;
        const auto start = (std::max)(0.0, (extent - total) / 2.0);
        const auto slot = ItemSlot + ItemGap;
        auto index = static_cast<int>((axis - start + ItemSlot / 2.0) / slot);
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
