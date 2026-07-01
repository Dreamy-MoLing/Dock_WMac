#include "pch.h"
#include "Logger.h"

namespace DockWMac::infra
{
    void LogLine(RuntimePaths const& paths, std::string_view message)
    {
        try
        {
            EnsureRuntimePaths(paths);
            std::ofstream output{ paths.logFile, std::ios::binary | std::ios::app };
            output << message << "\n";
        }
        catch (...)
        {
        }
    }
}
