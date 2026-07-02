#include "pch.h"
#include "ShellIntegration.h"

#include <gdiplus.h>

namespace DockWMac::shell
{
    namespace
    {
        struct IconHandle
        {
            HICON value{};

            ~IconHandle()
            {
                if (value)
                {
                    DestroyIcon(value);
                }
            }

            IconHandle() = default;
            IconHandle(IconHandle const&) = delete;
            IconHandle& operator=(IconHandle const&) = delete;
            IconHandle(IconHandle&& other) noexcept : value(other.value)
            {
                other.value = {};
            }
            IconHandle& operator=(IconHandle&& other) noexcept
            {
                if (this != &other)
                {
                    if (value)
                    {
                        DestroyIcon(value);
                    }
                    value = other.value;
                    other.value = {};
                }
                return *this;
            }
        };

        struct GdiPlusSession
        {
            ULONG_PTR token{};

            GdiPlusSession()
            {
                Gdiplus::GdiplusStartupInput input;
                Gdiplus::GdiplusStartup(&token, &input, nullptr);
            }

            ~GdiPlusSession()
            {
                if (token)
                {
                    Gdiplus::GdiplusShutdown(token);
                }
            }

            explicit operator bool() const
            {
                return token != 0;
            }
        };

        std::optional<CLSID> EncoderClsid(std::wstring_view mimeType)
        {
            UINT count{};
            UINT size{};
            if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || count == 0 || size == 0)
            {
                return std::nullopt;
            }

            std::vector<BYTE> buffer(size);
            auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
            if (Gdiplus::GetImageEncoders(count, size, encoders) != Gdiplus::Ok)
            {
                return std::nullopt;
            }

            for (UINT i = 0; i < count; ++i)
            {
                if (encoders[i].MimeType && mimeType == encoders[i].MimeType)
                {
                    return encoders[i].Clsid;
                }
            }
            return std::nullopt;
        }

        std::wstring HexHash(std::wstring const& value)
        {
            std::wstringstream stream;
            stream << std::hex << std::hash<std::wstring>{}(value);
            return stream.str();
        }

        std::filesystem::path IconCachePath(
            std::filesystem::path const& cacheDir,
            std::wstring const& cacheKey,
            std::wstring const& sourcePath)
        {
            return cacheDir / (L"dock-v2-" + HexHash(cacheKey + L"|" + sourcePath) + L".png");
        }

        std::optional<int> SystemImageIndex(std::wstring const& sourcePath)
        {
            if (sourcePath.empty())
            {
                return std::nullopt;
            }

            SHFILEINFOW fileInfo{};
            auto flags = SHGFI_SYSICONINDEX | SHGFI_LARGEICON;
            if (!std::filesystem::exists(sourcePath))
            {
                flags |= SHGFI_USEFILEATTRIBUTES;
            }

            if (!SHGetFileInfoW(
                sourcePath.c_str(),
                FILE_ATTRIBUTE_NORMAL,
                &fileInfo,
                sizeof(fileInfo),
                flags))
            {
                return std::nullopt;
            }

            return fileInfo.iIcon;
        }

        bool TryIconFromSystemImageList(int imageIndex, int imageListKind, IconHandle& icon)
        {
            IImageList* imageList{};
            if (FAILED(SHGetImageList(imageListKind, IID_IImageList, reinterpret_cast<void**>(&imageList))) || !imageList)
            {
                return false;
            }

            HICON extracted{};
            const auto result = imageList->GetIcon(imageIndex, ILD_TRANSPARENT, &extracted);
            imageList->Release();
            if (FAILED(result) || !extracted)
            {
                return false;
            }

            icon.value = extracted;
            return true;
        }

        bool TryLegacyFileIcon(std::wstring const& sourcePath, IconHandle& icon)
        {
            SHFILEINFOW fileInfo{};
            if (!SHGetFileInfoW(
                sourcePath.c_str(),
                FILE_ATTRIBUTE_NORMAL,
                &fileInfo,
                sizeof(fileInfo),
                SHGFI_ICON | SHGFI_LARGEICON))
            {
                return false;
            }

            icon.value = fileInfo.hIcon;
            return icon.value != nullptr;
        }

        IconHandle ExtractBestIcon(std::wstring const& sourcePath)
        {
            IconHandle icon;
            if (auto imageIndex = SystemImageIndex(sourcePath))
            {
                if (TryIconFromSystemImageList(*imageIndex, SHIL_JUMBO, icon) ||
                    TryIconFromSystemImageList(*imageIndex, SHIL_EXTRALARGE, icon) ||
                    TryIconFromSystemImageList(*imageIndex, SHIL_LARGE, icon))
                {
                    return icon;
                }
            }

            TryLegacyFileIcon(sourcePath, icon);
            return icon;
        }

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

        std::wstring ExpandEnvironmentPath(std::wstring const& path)
        {
            if (path.empty())
            {
                return {};
            }

            std::wstring expanded(32767, L'\0');
            const auto size = ExpandEnvironmentStringsW(
                path.c_str(),
                expanded.data(),
                static_cast<DWORD>(expanded.size()));
            if (size == 0 || size > expanded.size())
            {
                return path;
            }

            expanded.resize(size - 1);
            return expanded;
        }

        std::wstring ShortcutIconPath(IShellLinkW* link)
        {
            if (!link)
            {
                return {};
            }

            wchar_t iconPath[MAX_PATH]{};
            int iconIndex{};
            if (FAILED(link->GetIconLocation(iconPath, MAX_PATH, &iconIndex)) || iconPath[0] == L'\0')
            {
                return {};
            }

            return ExpandEnvironmentPath(iconPath);
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
            app.iconPath = ShortcutIconPath(link.get());
            if (app.iconPath.empty())
            {
                app.iconPath = app.targetPath;
            }
            return app;
        }

        bool IsTaskbarClass(std::wstring_view className)
        {
            return className == L"Shell_TrayWnd" ||
                className == L"Shell_SecondaryTrayWnd" ||
                className == L"DV2ControlHost" ||
                className == L"MsgrIMEWindowClass";
        }

        bool IsHelperWindowClass(std::wstring_view className)
        {
            return className == L"MSCTFIME UI" ||
                className == L"IME" ||
                className == L"Default IME" ||
                className == L"ApplicationFrameInputSinkWindow" ||
                className == L"Windows.UI.Core.CoreWindow";
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

        struct WindowFilterResult
        {
            bool isTaskbarCandidate{};
            std::wstring reason;
        };

        WindowFilterResult EvaluateTaskbarCandidateWindow(HWND hwnd, std::wstring const& className, std::wstring const& title, LONG_PTR exStyle)
        {
            if (!IsWindow(hwnd))
            {
                return { false, L"invalid-window" };
            }

            if (!IsWindowVisible(hwnd))
            {
                return { false, L"not-visible" };
            }

            DWORD processId{};
            GetWindowThreadProcessId(hwnd, &processId);
            if (processId == GetCurrentProcessId())
            {
                return { false, L"own-process" };
            }

            if ((exStyle & WS_EX_TOOLWINDOW) != 0)
            {
                return { false, L"tool-window" };
            }

            if (GetWindow(hwnd, GW_OWNER) && (exStyle & WS_EX_APPWINDOW) == 0)
            {
                return { false, L"owner-without-appwindow" };
            }

            if (IsTaskbarClass(className))
            {
                return { false, L"taskbar-class" };
            }

            if (IsHelperWindowClass(className))
            {
                return { false, L"helper-class" };
            }

            if (IsCloaked(hwnd))
            {
                return { false, L"cloaked" };
            }

            if (title.empty())
            {
                return { false, L"empty-title" };
            }

            return { true, L"included" };
        }

        struct EnumContext
        {
            std::vector<WindowInfo>* windows{};
            HWND foreground{};
            bool includeFiltered{};
        };

        BOOL CALLBACK EnumWindowProc(HWND hwnd, LPARAM lparam)
        {
            auto* context = reinterpret_cast<EnumContext*>(lparam);
            if (!context || !context->windows)
            {
                return TRUE;
            }

            DWORD processId{};
            GetWindowThreadProcessId(hwnd, &processId);

            WindowInfo info;
            info.hwnd = hwnd;
            info.processId = processId;
            info.title = WindowTitle(hwnd);
            info.className = WindowClass(hwnd);
            info.exStyle = static_cast<LONG_PTR>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
            auto const filter = EvaluateTaskbarCandidateWindow(hwnd, info.className, info.title, info.exStyle);
            info.isTaskbarCandidate = filter.isTaskbarCandidate;
            info.filteredReason = filter.reason;

            if (!info.isTaskbarCandidate && !context->includeFiltered)
            {
                return TRUE;
            }

            info.executablePath = ProcessPath(processId);
            info.appUserModelId = WindowAppUserModelId(hwnd);
            info.iconPath = info.executablePath;
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
        EnumContext context{ &windows, GetForegroundWindow(), false };
        EnumWindows(EnumWindowProc, reinterpret_cast<LPARAM>(&context));
        return windows;
    }

    std::vector<WindowInfo> EnumerateTopLevelWindowsForDiagnostics()
    {
        std::vector<WindowInfo> windows;
        EnumContext context{ &windows, GetForegroundWindow(), true };
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

    std::wstring CacheIconForPath(
        std::wstring const& sourcePath,
        std::filesystem::path const& cacheDir,
        std::wstring const& cacheKey)
    {
        if (sourcePath.empty() || cacheDir.empty())
        {
            return {};
        }

        const auto output = IconCachePath(cacheDir, cacheKey, sourcePath);
        if (std::filesystem::exists(output))
        {
            return output.wstring();
        }

        std::filesystem::create_directories(cacheDir);

        auto icon = ExtractBestIcon(sourcePath);
        if (!icon.value)
        {
            return {};
        }

        GdiPlusSession gdiplus;
        if (!gdiplus)
        {
            return {};
        }

        auto pngEncoder = EncoderClsid(L"image/png");
        if (!pngEncoder)
        {
            return {};
        }

        Gdiplus::Bitmap bitmap(icon.value);
        if (bitmap.GetLastStatus() != Gdiplus::Ok)
        {
            return {};
        }

        if (bitmap.Save(output.c_str(), &*pngEncoder, nullptr) != Gdiplus::Ok)
        {
            return {};
        }

        return output.wstring();
    }
}
