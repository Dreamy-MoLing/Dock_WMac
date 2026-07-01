#pragma once

#include "RuntimePaths.h"

namespace DockWMac::infra
{
    void LogLine(RuntimePaths const& paths, std::string_view message);
}
