#pragma once

namespace DockWMac::infra
{
    struct RuntimePaths
    {
        std::filesystem::path userDataDir;
        std::filesystem::path configFile;
        std::filesystem::path logDir;
        std::filesystem::path logFile;
    };

    RuntimePaths ResolveRuntimePaths();
    void EnsureRuntimePaths(RuntimePaths const& paths);
}
