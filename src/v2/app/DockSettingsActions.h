#pragma once

#include "../dock/DockModel.h"
#include "../infra/AppSettings.h"

namespace DockWMac::app
{
    inline bool ApplyDockSettingsAction(
        DockWMac::infra::AppSettings& settings,
        DockWMac::dock::DockActionKind action)
    {
        switch (action)
        {
        case DockWMac::dock::DockActionKind::ToggleAutoHide:
            settings.autoHide = !settings.autoHide;
            return true;
        case DockWMac::dock::DockActionKind::PlaceBottom:
            if (settings.placement == DockWMac::platform::DockPlacement::Bottom)
            {
                return false;
            }
            settings.placement = DockWMac::platform::DockPlacement::Bottom;
            return true;
        case DockWMac::dock::DockActionKind::PlaceLeft:
            if (settings.placement == DockWMac::platform::DockPlacement::Left)
            {
                return false;
            }
            settings.placement = DockWMac::platform::DockPlacement::Left;
            return true;
        case DockWMac::dock::DockActionKind::PlaceRight:
            if (settings.placement == DockWMac::platform::DockPlacement::Right)
            {
                return false;
            }
            settings.placement = DockWMac::platform::DockPlacement::Right;
            return true;
        default:
            return false;
        }
    }
}
