#include "pch.h"
#include "DockModel.h"

#include <set>

namespace DockWMac::dock
{
    namespace
    {
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
            auto duplicate = std::find_if(result.begin(), result.end(), [&](DockItem const& existing) {
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

        struct ExecutableAliasOwner
        {
            std::wstring itemId;
            bool hasExplicitAppUserModelId{};
        };

        void RegisterExecutableAliases(std::map<std::wstring, ExecutableAliasOwner>& owners,
                                       std::set<std::wstring>& ambiguous, shell::PinnedApp const& app,
                                       std::wstring const& itemId)
        {
            for (auto const& alias : ExecutableAliasesForPinned(app))
            {
                if (ambiguous.contains(alias))
                {
                    continue;
                }

                auto const owner = ExecutableAliasOwner{ itemId, !app.appUserModelId.empty() };
                auto const [it, inserted] = owners.emplace(alias, owner);
                if (!inserted && it->second.itemId != itemId)
                {
                    owners.erase(it);
                    ambiguous.insert(alias);
                }
            }
        }

        std::wstring ResolvePinnedItemId(shell::PinnedApp const& app,
                                         std::map<std::wstring, ExecutableAliasOwner> const& executableAliasOwners)
        {
            auto id = IdentityForPinned(app);
            if (!app.appUserModelId.empty())
            {
                return id;
            }

            for (auto const& alias : ExecutableAliasesForPinned(app))
            {
                if (auto const it = executableAliasOwners.find(alias); it != executableAliasOwners.end())
                {
                    return it->second.itemId;
                }
            }
            return id;
        }

        std::wstring ResolveWindowItemId(shell::WindowInfo const& window, std::wstring id,
                                         std::map<std::wstring, ExecutableAliasOwner> const& executableAliasOwners)
        {
            auto pathIdentity = window;
            pathIdentity.appUserModelId.clear();
            auto const it = executableAliasOwners.find(IdentityForWindow(pathIdentity));
            if (it == executableAliasOwners.end())
            {
                return id;
            }

            // An explicit window AUMID must not override another explicit pinned
            // group. A path-identified pin can safely adopt the window's stronger
            // identity when the executable alias has a single owner.
            if (!window.appUserModelId.empty() && it->second.hasExplicitAppUserModelId)
            {
                return id;
            }
            return it->second.itemId;
        }

        bool SamePinnedApp(shell::PinnedApp const& left, shell::PinnedApp const& right)
        {
            return left.name == right.name && left.linkPath == right.linkPath && left.targetPath == right.targetPath &&
                   left.arguments == right.arguments && left.appUserModelId == right.appUserModelId &&
                   left.iconPath == right.iconPath;
        }

        bool SamePinnedApps(std::vector<shell::PinnedApp> const& left, std::vector<shell::PinnedApp> const& right)
        {
            return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), SamePinnedApp);
        }

        void ApplyPinnedApp(DockItem& item, shell::PinnedApp const& app, bool systemPin, bool localPin)
        {
            if (item.id.empty())
            {
                item.id = IdentityForPinned(app);
            }

            item.displayName =
                FirstNonEmpty({ item.displayName, app.name, FileName(app.targetPath), app.appUserModelId });
            item.linkPath = FirstNonEmpty({ item.linkPath, app.linkPath });
            item.targetPath = FirstNonEmpty({ item.targetPath, app.targetPath });
            item.arguments = FirstNonEmpty({ item.arguments, app.arguments });
            item.appUserModelId = FirstNonEmpty({ item.appUserModelId, app.appUserModelId });
            item.iconPath = FirstNonEmpty({ item.iconPath, app.iconPath, app.linkPath, app.targetPath });
            item.pinned = true;
            item.systemPinned = item.systemPinned || systemPin;
            item.localPinned = item.localPinned || localPin;
            item.transientRunningOnly = false;
        }
    } // namespace

    bool ApplyImportedTaskbarPins(DockState& state, std::vector<shell::PinnedApp> importedPins)
    {
        if (SamePinnedApps(state.importedTaskbarPins, importedPins))
        {
            return false;
        }

        state.importedTaskbarPins = std::move(importedPins);
        return true;
    }

    std::vector<DockItem> BuildDockItems(std::vector<shell::PinnedApp> const& pinnedApps,
                                         std::vector<shell::WindowInfo> const& windows, DockState const& state)
    {
        std::map<std::wstring, DockItem> byId;
        std::map<std::wstring, ExecutableAliasOwner> executableAliasOwners;
        std::set<std::wstring> ambiguousExecutableAliases;
        std::vector<std::wstring> pinnedSourceOrder;
        std::vector<std::wstring> transientRunningOrder;

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
            RegisterExecutableAliases(executableAliasOwners, ambiguousExecutableAliases, app, id);
            pinnedSourceOrder.push_back(id);
        }

        for (auto const& app : state.localPins)
        {
            auto id = ResolvePinnedItemId(app, executableAliasOwners);
            if (id.empty())
            {
                continue;
            }

            auto& item = byId[id];
            item.id = id;
            ApplyPinnedApp(item, app, false, true);
            RegisterExecutableAliases(executableAliasOwners, ambiguousExecutableAliases, app, id);
            pinnedSourceOrder.push_back(id);
        }

        for (auto const& window : windows)
        {
            if (!window.isTaskbarCandidate)
            {
                continue;
            }

            auto id = IdentityForWindow(window);
            if (id.empty())
            {
                continue;
            }

            id = ResolveWindowItemId(window, std::move(id), executableAliasOwners);

            auto& item = byId[id];
            if (item.id.empty())
            {
                item.id = id;
                item.displayName =
                    FirstNonEmpty({ FileName(window.executablePath), window.title, window.appUserModelId });
                item.targetPath = window.executablePath;
                item.appUserModelId = window.appUserModelId;
                item.iconPath = FirstNonEmpty({ window.iconPath, window.executablePath });
                item.transientRunningOnly = true;
                transientRunningOrder.push_back(id);
            }

            item.running = true;
            item.transientRunningOnly = !item.pinned;
            item.foreground = item.foreground || window.foreground;
            item.appUserModelId = FirstNonEmpty({ item.appUserModelId, window.appUserModelId });
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
            if (auto it = byId.find(id); it != byId.end() && it->second.pinned)
            {
                AppendOrdered(result, it->second);
            }
        }
        for (auto const& id : pinnedSourceOrder)
        {
            if (auto it = byId.find(id); it != byId.end())
            {
                AppendOrdered(result, it->second);
            }
        }
        for (auto const& id : transientRunningOrder)
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

    bool MoveDockItem(std::vector<DockItem>& items, size_t fromIndex, size_t toIndex)
    {
        if (fromIndex >= items.size() || toIndex >= items.size() || fromIndex == toIndex)
        {
            return false;
        }

        auto item = std::move(items[fromIndex]);
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(fromIndex));
        items.insert(items.begin() + static_cast<std::ptrdiff_t>(toIndex), std::move(item));
        return true;
    }
} // namespace DockWMac::dock
