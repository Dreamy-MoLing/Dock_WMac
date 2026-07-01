#include "pch.h"
#include "RuntimePaths.h"

namespace DockWMac::infra
{
    namespace
    {
        std::filesystem::path LocalAppData()
        {
            PWSTR rawPath{};
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &rawPath)) && rawPath)
            {
                std::filesystem::path path{ rawPath };
                CoTaskMemFree(rawPath);
                return path;
            }

            wchar_t fallback[MAX_PATH]{};
            const auto size = GetEnvironmentVariableW(L"LOCALAPPDATA", fallback, MAX_PATH);
            if (size > 0 && size < MAX_PATH)
            {
                return fallback;
            }

            return std::filesystem::temp_directory_path();
        }
    }

    RuntimePaths ResolveRuntimePaths()
    {
        RuntimePaths paths;
        paths.userDataDir = LocalAppData() / L"Dock_WMac";
        paths.configFile = paths.userDataDir / L"settings.json";
        paths.logDir = paths.userDataDir / L"logs";
        paths.logFile = paths.logDir / L"dock.log";
        return paths;
    }

    void EnsureRuntimePaths(RuntimePaths const& paths)
    {
        std::filesystem::create_directories(paths.userDataDir);
        std::filesystem::create_directories(paths.logDir);
    }
}
