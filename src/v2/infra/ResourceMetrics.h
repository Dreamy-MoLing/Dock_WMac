#pragma once

#include <chrono>
#include <winrt/Windows.Data.Json.h>

namespace DockWMac::infra
{
    struct ResourceMetrics
    {
        uint32_t processId{};
        uint64_t workingSetBytes{};
        uint64_t privateBytes{};
        uint32_t handleCount{};
        uint32_t threadCount{};
        double cpuUsagePercent{};
        uint32_t sampleMilliseconds{};
    };

    ResourceMetrics CaptureCurrentProcessResourceMetrics(std::chrono::milliseconds sampleDuration = std::chrono::milliseconds{ 250 });
    winrt::Windows::Data::Json::JsonObject ResourceMetricsJson(ResourceMetrics const& metrics);
    int RunResourceMetricsDump();
}
