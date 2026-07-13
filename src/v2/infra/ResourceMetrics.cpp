#include "pch.h"
#include "ResourceMetrics.h"
#include "RuntimePaths.h"

#include <psapi.h>
#include <tlhelp32.h>
#include <thread>
#include <type_traits>

namespace DockWMac::infra
{
    namespace
    {
        uint64_t FileTimeToUInt64(FILETIME const& value)
        {
            ULARGE_INTEGER converted{};
            converted.LowPart = value.dwLowDateTime;
            converted.HighPart = value.dwHighDateTime;
            return converted.QuadPart;
        }

        uint64_t ProcessCpuTime100Ns()
        {
            FILETIME created{};
            FILETIME exited{};
            FILETIME kernel{};
            FILETIME user{};
            if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
            {
                return 0;
            }

            return FileTimeToUInt64(kernel) + FileTimeToUInt64(user);
        }

        uint32_t CurrentProcessThreadCount()
        {
            const auto processId = GetCurrentProcessId();
            HANDLE rawSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (rawSnapshot == INVALID_HANDLE_VALUE)
            {
                return 0;
            }

            std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)> snapshot{ rawSnapshot, CloseHandle };
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);

            uint32_t count{};
            if (!Thread32First(snapshot.get(), &entry))
            {
                return 0;
            }

            do
            {
                if (entry.th32OwnerProcessID == processId)
                {
                    ++count;
                }
                entry.dwSize = sizeof(entry);
            } while (Thread32Next(snapshot.get(), &entry));

            return count;
        }

        uint32_t ProcessorCount()
        {
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            return (std::max)(uint32_t{ 1 }, static_cast<uint32_t>(info.dwNumberOfProcessors));
        }

        std::wstring TimestampForFile()
        {
            SYSTEMTIME now{};
            GetLocalTime(&now);
            std::wostringstream output;
            output << std::setfill(L'0')
                << std::setw(4) << now.wYear
                << std::setw(2) << now.wMonth
                << std::setw(2) << now.wDay
                << L"-"
                << std::setw(2) << now.wHour
                << std::setw(2) << now.wMinute
                << std::setw(2) << now.wSecond;
            return output.str();
        }
    }

    ResourceMetrics CaptureCurrentProcessResourceMetrics(std::chrono::milliseconds sampleDuration)
    {
        sampleDuration = (std::max)(sampleDuration, std::chrono::milliseconds{ 25 });

        const auto cpuBefore = ProcessCpuTime100Ns();
        const auto wallBefore = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(sampleDuration);
        const auto wallAfter = std::chrono::steady_clock::now();
        const auto cpuAfter = ProcessCpuTime100Ns();

        const auto elapsedMs = static_cast<uint32_t>((std::max)(
            int64_t{ 1 },
            std::chrono::duration_cast<std::chrono::milliseconds>(wallAfter - wallBefore).count()));

        ResourceMetrics metrics;
        metrics.processId = GetCurrentProcessId();
        metrics.sampleMilliseconds = elapsedMs;

        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb = sizeof(memory);
        if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)))
        {
            metrics.workingSetBytes = static_cast<uint64_t>(memory.WorkingSetSize);
            metrics.privateBytes = static_cast<uint64_t>(memory.PrivateUsage);
        }

        DWORD handleCount{};
        if (GetProcessHandleCount(GetCurrentProcess(), &handleCount))
        {
            metrics.handleCount = static_cast<uint32_t>(handleCount);
        }

        metrics.threadCount = CurrentProcessThreadCount();

        const auto cpuDelta = cpuAfter > cpuBefore ? cpuAfter - cpuBefore : 0;
        const auto denominator = static_cast<double>(elapsedMs) * 10000.0 * static_cast<double>(ProcessorCount());
        metrics.cpuUsagePercent = denominator > 0.0 ? (static_cast<double>(cpuDelta) * 100.0 / denominator) : 0.0;
        return metrics;
    }

    winrt::Windows::Data::Json::JsonObject ResourceMetricsJson(ResourceMetrics const& metrics)
    {
        winrt::Windows::Data::Json::JsonObject json;
        json.SetNamedValue(L"processId", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(metrics.processId));
        json.SetNamedValue(L"workingSetBytes", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(static_cast<double>(metrics.workingSetBytes)));
        json.SetNamedValue(L"privateBytes", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(static_cast<double>(metrics.privateBytes)));
        json.SetNamedValue(L"handleCount", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(metrics.handleCount));
        json.SetNamedValue(L"threadCount", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(metrics.threadCount));
        json.SetNamedValue(L"cpuUsagePercent", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(metrics.cpuUsagePercent));
        json.SetNamedValue(L"sampleMilliseconds", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(metrics.sampleMilliseconds));
        return json;
    }

    int RunResourceMetricsDump()
    {
        try
        {
            auto paths = ResolveRuntimePaths();
            auto diagnosticsDir = paths.userDataDir / L"diagnostics";
            std::filesystem::create_directories(diagnosticsDir);

            auto metrics = CaptureCurrentProcessResourceMetrics(std::chrono::milliseconds{ 250 });
            auto json = ResourceMetricsJson(metrics);
            auto outputPath = diagnosticsDir / (L"resource-metrics-" + TimestampForFile() + L".json");

            std::ofstream output{ outputPath, std::ios::binary | std::ios::trunc };
            if (!output)
            {
                return 1;
            }

            output << winrt::to_string(json.Stringify());
            ::OutputDebugStringW((L"Resource metrics dump written: " + outputPath.wstring() + L"\n").c_str());
            return 0;
        }
        catch (...)
        {
            return 1;
        }
    }
}
