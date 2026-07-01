#pragma once

#include "DockModel.h"

namespace DockWMac::dock
{
    DockState LoadDockState(std::filesystem::path const& path);
    void SaveDockState(std::filesystem::path const& path, DockState const& state);
}
