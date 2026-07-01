#include "pch.h"
#include "ShellIntegration.h"

namespace DockWMac::shell
{
    namespace
    {
        std::wstring AppData()
        {
            PWSTR rawPath{};
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &rawPath)) && rawPath)
            {
                std::wstring path{ rawPath };
                CoTaskMemFree(rawPath);
                return path;
            }

            wchar_t fallback[MAX_PATH]{};
            const auto size = GetEnvironmentVariableW(L"APPDATA", fallback, MAX_PATH);
            return size > 0 && size < MAX_PATH ? std::wstring{ fallback } : std::wstring{};
        }

        std::filesystem::path TaskbarPinsFolder()
        {
            auto appData = AppData();
            if (appData.empty())
            {
                return {};
            }

            return std::filesystem::path{ appData } /
                L"Microsoft" / L"Internet Explorer" / L"Quick Launch" / L"User Pinned" / L"TaskBar";
        }

        std::wstring FileStem(std::filesystem::path const& path)
        {
            return path.stem().wstring();
        }

        std::wstring PropVariantString(PROPVARIANT const& value)
        {
            if (value.vt == VT_LPWSTR && value.pwszVal)
            {
                return value.pwszVal;
            }
            return {};
        }

        std::wstring ShortcutAppUserModelId(IShellLinkW* link)
        {
            auto store = winrt::com_ptr<IPropertyStore>{};
            if (!link || FAILED(link->QueryInterface(IID_PPV_ARGS(store.put()))))
            {
                return {};
            }

            PROPVARIANT value;
            PropVariantInit(&value);
            std::wstring appId;
            if (SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID, &value)))
            {
                appId = PropVariantString(value);
            }
            PropVariantClear(&value);
            return appId;
        }

        std::optional<PinnedApp> ResolveShortcut(std::filesystem::path const& path)
        {
            auto link = winrt::com_ptr<IShellLinkW>{};
            if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(link.put()))))
            {
                return std::nullopt;
            }

            auto persist = link.as<IPersistFile>();
            if (FAILED(persist->Load(path.c_str(), STGM_READ)))
            {
                return std::nullopt;
            }

            wchar_t target[MAX_PATH]{};
            WIN32_FIND_DATAW data{};
            link->GetPath(target, MAX_PATH, &data, SLGP_RAWPATH);

            wchar_t arguments[INFOTIPSIZE]{};
            link->GetArguments(arguments, INFOTIPSIZE);

            PinnedApp app;
            app.name = FileStem(path);
            app.linkPath = path.wstring();
            app.targetPath = target;
            app.arguments = arguments;
            app.appUserModelId = ShortcutAppUserModelId(link.get());
            return app;
        }

        bool IsTaskbarClass(std::wstring_view className)
        {
            return className == L"Shell_TrayWnd" ||
                className == L"Shell_SecondaryTrayWnd" ||
                className == L"DV2ControlHost" ||
                className == L"MsgrIMEWindowClass";
        }

        std::wstring WindowClass(HWND hwnd)
        {
            wchar_t className[128]{};
            GetClassNameW(hwnd, className, static_cast<int>(std::size(className)));
            return className;
        }

        std::wstring WindowTitle(HWND hwnd)
        {
            const auto length = GetWindowTextLengthW(hwnd);
            if (length <= 0)
            {
                return {};
            }

            std::wstring title(static_cast<size_t>(length), L'\0');
            GetWindowTextW(hwnd, title.data(), length + 1);
            return title;
        }

        std::wstring ProcessPath(uint32_t processId)
        {
            auto process = winrt::handle{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId) };
            if (!process)
            {
                return {};
            }

            std::wstring path(MAX_PATH, L'\0');
            DWORD size = static_cast<DWORD>(path.size());
            if (!QueryFullProcessImageNameW(process.get(), 0, path.data(), &size))
            {
                return {};
            }

            path.resize(size);
            return path;
        }

        std::wstring WindowAppUserModelId(HWND hwnd)
        {
            auto store = winrt::com_ptr<IPropertyStore>{};
            if (FAILED(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(store.put()))))
            {
                return {};
            }

            PROPVARIANT value;
            PropVariantInit(&value);
            std::wstring appId;
            if (SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID, &value)))
            {
                appId = PropVariantString(value);
            }
            PropVariantClear(&value);
            return appId;
        }

        bool IsCloaked(HWND hwnd)
        {
            BOOL cloaked{};
            return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked;
        }

        bool ShouldIncludeWindow(HWND hwnd)
        {
            if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER))
            {
                return false;
            }

            if (IsTaskbarClass(WindowClass(hwnd)))
            {
                return false;
            }

            return !WindowTitle(hwnd).empty();
        }

        struct EnumContext
        {
            std::vector<WindowInfo>* windows{};
            HWND foreground{};
        };

        BOOL CALLBACK EnumWindowProc(HWND hwnd, LPARAM lparam)
        {
            auto* context = reinterpret_cast<EnumContext*>(lparam);
            if (!context || !context->windows || !ShouldIncludeWindow(hwnd))
            {
                return TRUE;
            }

            DWORD processId{};
            GetWindowThreadProcessId(hwnd, &processId);

            WindowInfo info;
            info.hwnd = hwnd;
            info.processId = processId;
            info.title = WindowTitle(hwnd);
            info.executablePath = ProcessPath(processId);
            info.appUserModelId = WindowAppUserModelId(hwnd);
            info.minimized = IsIconic(hwnd) != FALSE;
            info.cloaked = IsCloaked(hwnd);
            info.foreground = hwnd == context->foreground;
            context->windows->push_back(std::move(info));
            return TRUE;
        }
    }

    std::vector<PinnedApp> ReadTaskbarPinnedItems()
    {
        std::vector<PinnedApp> items;
        const auto folder = TaskbarPinsFolder();
        if (folder.empty() || !std::filesystem::exists(folder))
        {
            return items;
        }

        for (auto const& entry : std::filesystem::directory_iterator(folder))
        {
            if (!entry.is_regular_file() || entry.path().extension() != L".lnk")
            {
                continue;
            }

            if (auto app = ResolveShortcut(entry.path()))
            {
                items.push_back(std::move(*app));
            }
        }

        return items;
    }

    std::vector<WindowInfo> EnumerateTopLevelWindows()
    {
        std::vector<WindowInfo> windows;
        EnumContext context{ &windows, GetForegroundWindow() };
        EnumWindows(EnumWindowProc, reinterpret_cast<LPARAM>(&context));
        return windows;
    }

    bool LaunchPinnedApp(PinnedApp const& app)
    {
        auto const& path = app.linkPath.empty() ? app.targetPath : app.linkPath;
        if (path.empty())
        {
            return false;
        }

        auto result = ShellExecuteW(
            nullptr,
            L"open",
            path.c_str(),
            app.linkPath.empty() && !app.arguments.empty() ? app.arguments.c_str() : nullptr,
            nullptr,
            SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(result) > 32;
    }

    bool ActivateWindow(HWND hwnd)
    {
        if (!IsWindow(hwnd))
        {
            return false;
        }

        if (IsIconic(hwnd))
        {
            ShowWindow(hwnd, SW_RESTORE);
        }

        AllowSetForegroundWindow(ASFW_ANY);
        return SetForegroundWindow(hwnd) != FALSE;
    }

    bool MinimizeWindow(HWND hwnd)
    {
        return IsWindow(hwnd) && ShowWindow(hwnd, SW_MINIMIZE);
    }
}
