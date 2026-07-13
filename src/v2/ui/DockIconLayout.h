#pragma once

#include <algorithm>
#include <cmath>

#include "../platform/DockPlacement.h"

namespace DockWMac::ui
{
    struct DockIconMetrics
    {
        double iconSize{ 56.0 };
        double maxMagnification{ 1.68 };
        double activeLift{ 6.0 };
        double magnificationRange{ 56.0 * 2.2 };
    };

    struct DockIconPose
    {
        double slotCenter{};
        double scale{ 1.0 };
        double lift{};
        double tangentX{};
        double tangentY{};
        double visualLeft{};
        double visualTop{};
        double visualRight{};
        double visualBottom{};
    };

    inline double DockMagnificationCurve(double distance, double range)
    {
        constexpr double pi = 3.14159265358979323846;
        const auto t = std::clamp(1.0 - distance / range, 0.0, 1.0);
        return 0.5 - 0.5 * std::cos(t * pi);
    }

    inline double AdvanceDockHoverAmount(
        double current,
        bool hovering,
        double elapsedMilliseconds,
        bool reducedMotion)
    {
        const auto target = hovering ? 1.0 : 0.0;
        if (reducedMotion)
        {
            return target;
        }

        current = std::clamp(current, 0.0, 1.0);
        const auto elapsed = std::clamp(elapsedMilliseconds, 0.0, 100.0);
        const auto timeConstant = hovering ? 45.0 : 55.0;
        const auto blend = 1.0 - std::exp(-elapsed / timeConstant);
        const auto next = current + (target - current) * blend;
        return std::abs(target - next) < 0.001 ? target : std::clamp(next, 0.0, 1.0);
    }

    inline bool IsVerticalDockPlacement(DockWMac::platform::DockPlacement placement)
    {
        return placement == DockWMac::platform::DockPlacement::Left ||
            placement == DockWMac::platform::DockPlacement::Right;
    }

    inline double DockMainAxisPosition(
        DockWMac::platform::DockPlacement placement,
        double x,
        double y)
    {
        return IsVerticalDockPlacement(placement) ? y : x;
    }

    inline double DockMainAxisExtent(
        DockWMac::platform::DockPlacement placement,
        double width,
        double height)
    {
        return IsVerticalDockPlacement(placement) ? height : width;
    }

    inline double DockCrossAxisExtent(
        DockWMac::platform::DockPlacement placement,
        double width,
        double height)
    {
        return IsVerticalDockPlacement(placement) ? width : height;
    }

    inline DockIconPose CalculateDockIconPose(
        DockWMac::platform::DockPlacement placement,
        double pointerAxis,
        double slotCenter,
        double shelfTangentLine,
        bool foreground,
        bool reducedMotion,
        DockIconMetrics metrics = {},
        double hoverAmount = 1.0)
    {
        (void)reducedMotion;
        const auto curve = DockMagnificationCurve(
            std::abs(pointerAxis - slotCenter),
            metrics.magnificationRange) * std::clamp(hoverAmount, 0.0, 1.0);
        const auto scale = 1.0 + (metrics.maxMagnification - 1.0) * curve;
        const auto radius = metrics.iconSize / 2.0;
        const auto maxTopLift = radius * (metrics.maxMagnification - 1.0);
        const auto tangentLift = radius * (scale - 1.0);
        const auto desiredLift = tangentLift + (foreground ? metrics.activeLift : 0.0);
        const auto lift = (std::min)(desiredLift, maxTopLift);
        const auto extent = metrics.iconSize * scale;
        const auto half = extent / 2.0;

        DockIconPose pose;
        pose.slotCenter = slotCenter;
        pose.scale = scale;
        pose.lift = lift;

        if (placement == DockWMac::platform::DockPlacement::Bottom)
        {
            pose.tangentX = slotCenter;
            pose.tangentY = shelfTangentLine;
            const auto centerY = shelfTangentLine - radius - lift;
            pose.visualLeft = slotCenter - half;
            pose.visualRight = slotCenter + half;
            pose.visualTop = centerY - half;
            pose.visualBottom = centerY + half;
            return pose;
        }

        pose.tangentX = shelfTangentLine;
        pose.tangentY = slotCenter;
        const auto away = placement == DockWMac::platform::DockPlacement::Left ? -1.0 : 1.0;
        const auto centerX = shelfTangentLine + away * (radius + lift);
        pose.visualLeft = centerX - half;
        pose.visualRight = centerX + half;
        pose.visualTop = slotCenter - half;
        pose.visualBottom = slotCenter + half;
        return pose;
    }
}
