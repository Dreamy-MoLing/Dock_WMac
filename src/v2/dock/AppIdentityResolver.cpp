#include "pch.h"
#include "AppIdentityResolver.h"

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

        std::wstring NormalizePathIdentity(std::wstring const& path)
        {
            if (path.empty())
            {
                return {};
            }

            return Lower(std::filesystem::path{ path }.lexically_normal().wstring());
        }

        std::wstring NormalizeTextIdentity(std::wstring const& value)
        {
            return Lower(value);
        }

        void AppendUnique(std::vector<std::wstring>& values, std::wstring value)
        {
            if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end())
            {
                values.push_back(std::move(value));
            }
        }

        bool IsExecutablePath(std::wstring const& path)
        {
            auto extension = Lower(std::filesystem::path{ path }.extension().wstring());
            return extension == L".exe" || extension == L".com";
        }
    }

    std::wstring IdentityForPinned(shell::PinnedApp const& app)
    {
        if (!app.appUserModelId.empty())
        {
            return NormalizeTextIdentity(app.appUserModelId);
        }

        if (auto target = NormalizePathIdentity(app.targetPath); !target.empty())
        {
            return target;
        }

        if (auto link = NormalizePathIdentity(app.linkPath); !link.empty())
        {
            return link;
        }

        return NormalizeTextIdentity(app.name);
    }

    std::wstring IdentityForWindow(shell::WindowInfo const& window)
    {
        if (!window.appUserModelId.empty())
        {
            return NormalizeTextIdentity(window.appUserModelId);
        }

        return NormalizePathIdentity(window.executablePath);
    }

    std::vector<std::wstring> ExecutableAliasesForPinned(shell::PinnedApp const& app)
    {
        std::vector<std::wstring> aliases;
        if (IsExecutablePath(app.targetPath))
        {
            AppendUnique(aliases, NormalizePathIdentity(app.targetPath));
        }
        if (IsExecutablePath(app.iconPath))
        {
            AppendUnique(aliases, NormalizePathIdentity(app.iconPath));
        }
        return aliases;
    }
}
