#include "pch.h"
#include "DockModel.h"

namespace DockWMac::dock
{
    namespace
    {
        std::wstring Lower(std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch)
            {
                return static_cast<wchar_t>(::towlower(ch));
            });
            return value;
        }

        std::wstring FileName(std::wstring const& path)
        {
            if (path.empty())
            {
                return {};
            }
            return std::filesystem::path{ path }.stem().wstring();
        }

        std::wstring FirstNonEmpty(std::initializer_list<std::wstring> values)
        {
            for (auto const& value : values)
            {
                if (!value.empty())
                {
                    return value;
                }
            }
            return {};
        }

        void AppendOrdered(std::vector<DockItem>& result, DockItem item)
        {
            auto duplicate = std::find_if(result.begin(), result.end(), [&](DockItem const& existing)
            {
                return existing.id == item.id;
            });
            if (duplicate == result.end())
            {
                result.push_back(std::move(item));
            }
        }

        bool Contains(std::vector<std::wstring> const& values, std::wstring const& value)
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        void ApplyPinnedApp(DockItem& item, shell::PinnedApp const& app, bool systemPin, bool localPin)
        {
            if (item.id.empty())
            {
                item.id = IdentityForPinned(app);
            }

            item.displayName = FirstNonEmpty({ item.displayName, app.name, FileName(app.targetPath), app.appUserModelId });
            item.linkPath = FirstNonEmpty({ item.linkPath, app.linkPath });
            item.targetPath = FirstNonEmpty({ item.targetPath, app.targetPath });
            item.arguments = FirstNonEmpty({ item.arguments, app.arguments });
            item.appUserModelId = FirstNonEmpty({ item.appUserModelId, app.appUserModelId });
            item.iconPath = FirstNonEmpty({ item.iconPath, app.iconPath, app.linkPath, app.targetPath });
            item.pinned = true;
            item.systemPinned = item.systemPinned || systemPin;
            item.localPinned = item.localPinned || localPin;
        }
    }

    std::wstring IdentityForPinned(shell::PinnedApp const& app)
    {
        return Lower(FirstNonEmpty({
            app.appUserModelId,
            app.targetPath,
            app.linkPath,
            app.name,
        }));
    }

    std::wstring IdentityForWindow(shell::WindowInfo const& window)
    {
        return Lower(FirstNonEmpty({
            window.appUserModelId,
            window.executablePath,
            window.title,
        }));
    }

    std::vector<DockItem> BuildDockItems(
        std::vector<shell::PinnedApp> const& pinnedApps,
        std::vector<shell::WindowInfo> const& windows,
        DockState const& state)
    {
        std::map<std::wstring, DockItem> byId;
        std::vector<std::wstring> sourceOrder;

        for (auto const& app : pinnedApps)
        {
            auto id = IdentityForPinned(app);
            if (id.empty() || Contains(state.hiddenSystemPins, id))
            {
                continue;
            }

            auto& item = byId[id];
            item.id = id;
            ApplyPinnedApp(item, app, true, false);
            sourceOrder.push_back(id);
        }

        for (auto const& app : state.localPins)
        {
            auto id = IdentityForPinned(app);
            if (id.empty())
            {
                continue;
            }

            auto& item = byId[id];
            item.id = id;
            ApplyPinnedApp(item, app, false, true);
            sourceOrder.push_back(id);
        }

        for (auto const& window : windows)
        {
            auto id = IdentityForWindow(window);
            if (id.empty())
            {
                continue;
            }

            auto& item = byId[id];
            if (item.id.empty())
            {
                item.id = id;
                item.displayName = FirstNonEmpty({ FileName(window.executablePath), window.title, window.appUserModelId });
                item.targetPath = window.executablePath;
                item.appUserModelId = window.appUserModelId;
                item.iconPath = FirstNonEmpty({ window.iconPath, window.executablePath });
                sourceOrder.push_back(id);
            }

            item.running = true;
            item.foreground = item.foreground || window.foreground;
            item.iconPath = FirstNonEmpty({ item.iconPath, window.iconPath, window.executablePath });
            if (item.displayName.empty())
            {
                item.displayName = FirstNonEmpty({ FileName(window.executablePath), window.title });
            }
            item.windows.push_back(DockWindowRef{
                window.hwnd,
                window.title,
                window.minimized,
                window.cloaked,
                window.foreground,
            });
        }

        std::vector<DockItem> result;
        for (auto const& id : state.order)
        {
            if (auto it = byId.find(id); it != byId.end())
            {
                AppendOrdered(result, it->second);
            }
        }
        for (auto const& id : sourceOrder)
        {
            if (auto it = byId.find(id); it != byId.end())
            {
                AppendOrdered(result, it->second);
            }
        }

        return result;
    }

    DockAction DecideClickAction(DockItem const& item)
    {
        DockAction action;
        action.itemId = item.id;

        if (!item.running)
        {
            action.kind = DockActionKind::Launch;
            return action;
        }

        if (item.windows.size() > 1)
        {
            action.kind = DockActionKind::ShowWindowChooser;
            return action;
        }

        if (item.windows.empty())
        {
            return action;
        }

        action.hwnd = item.windows.front().hwnd;
        action.kind = item.foreground ? DockActionKind::MinimizeWindow : DockActionKind::ActivateWindow;
        return action;
    }
}
