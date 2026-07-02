#include "pch.h"
#include "DockStateDump.h"
#include "RuntimePaths.h"
#include "../dock/DockModel.h"
#include "../dock/DockStateStore.h"
#include "../shell/ShellIntegration.h"

namespace DockWMac::infra
{
    namespace
    {
        winrt::Windows::Data::Json::JsonValue StringValue(std::wstring const& value)
        {
            return winrt::Windows::Data::Json::JsonValue::CreateStringValue(winrt::hstring{ value });
        }

        winrt::Windows::Data::Json::JsonValue StringValue(std::filesystem::path const& value)
        {
            return StringValue(value.wstring());
        }

        winrt::Windows::Data::Json::JsonValue BoolValue(bool value)
        {
            return winrt::Windows::Data::Json::JsonValue::CreateBooleanValue(value);
        }

        winrt::Windows::Data::Json::JsonValue NumberValue(double value)
        {
            return winrt::Windows::Data::Json::JsonValue::CreateNumberValue(value);
        }

        winrt::Windows::Data::Json::JsonArray StringArray(std::vector<std::wstring> const& values)
        {
            winrt::Windows::Data::Json::JsonArray array;
            for (auto const& value : values)
            {
                array.Append(StringValue(value));
            }
            return array;
        }

        winrt::Windows::Data::Json::JsonObject PinnedAppJson(DockWMac::shell::PinnedApp const& app)
        {
            winrt::Windows::Data::Json::JsonObject object;
            object.SetNamedValue(L"name", StringValue(app.name));
            object.SetNamedValue(L"linkPath", StringValue(app.linkPath));
            object.SetNamedValue(L"targetPath", StringValue(app.targetPath));
            object.SetNamedValue(L"arguments", StringValue(app.arguments));
            object.SetNamedValue(L"appUserModelId", StringValue(app.appUserModelId));
            object.SetNamedValue(L"iconPath", StringValue(app.iconPath));
            return object;
        }

        winrt::Windows::Data::Json::JsonArray PinnedAppArray(std::vector<DockWMac::shell::PinnedApp> const& apps)
        {
            winrt::Windows::Data::Json::JsonArray array;
            for (auto const& app : apps)
            {
                array.Append(PinnedAppJson(app));
            }
            return array;
        }

        std::wstring HwndString(HWND hwnd)
        {
            std::wstringstream stream;
            stream << L"0x" << std::hex << reinterpret_cast<std::uintptr_t>(hwnd);
            return stream.str();
        }

        std::wstring HexString(LONG_PTR value)
        {
            std::wstringstream stream;
            stream << L"0x" << std::hex << static_cast<unsigned long long>(value);
            return stream.str();
        }

        winrt::Windows::Data::Json::JsonObject WindowJson(DockWMac::shell::WindowInfo const& window)
        {
            winrt::Windows::Data::Json::JsonObject object;
            object.SetNamedValue(L"hwnd", StringValue(HwndString(window.hwnd)));
            object.SetNamedValue(L"processId", NumberValue(static_cast<double>(window.processId)));
            object.SetNamedValue(L"title", StringValue(window.title));
            object.SetNamedValue(L"className", StringValue(window.className));
            object.SetNamedValue(L"exStyle", StringValue(HexString(window.exStyle)));
            object.SetNamedValue(L"processPath", StringValue(window.executablePath));
            object.SetNamedValue(L"appUserModelId", StringValue(window.appUserModelId));
            object.SetNamedValue(L"iconPath", StringValue(window.iconPath));
            object.SetNamedValue(L"minimized", BoolValue(window.minimized));
            object.SetNamedValue(L"cloaked", BoolValue(window.cloaked));
            object.SetNamedValue(L"foreground", BoolValue(window.foreground));
            object.SetNamedValue(L"isTaskbarCandidate", BoolValue(window.isTaskbarCandidate));
            object.SetNamedValue(L"filteredReason", StringValue(window.filteredReason));
            return object;
        }

        winrt::Windows::Data::Json::JsonArray WindowArray(std::vector<DockWMac::shell::WindowInfo> const& windows)
        {
            winrt::Windows::Data::Json::JsonArray array;
            for (auto const& window : windows)
            {
                array.Append(WindowJson(window));
            }
            return array;
        }

        winrt::Windows::Data::Json::JsonObject DockItemJson(DockWMac::dock::DockItem const& item)
        {
            winrt::Windows::Data::Json::JsonObject object;
            object.SetNamedValue(L"id", StringValue(item.id));
            object.SetNamedValue(L"displayName", StringValue(item.displayName));
            object.SetNamedValue(L"pinned", BoolValue(item.pinned));
            object.SetNamedValue(L"systemPinned", BoolValue(item.systemPinned));
            object.SetNamedValue(L"localPinned", BoolValue(item.localPinned));
            object.SetNamedValue(L"transientRunningOnly", BoolValue(item.transientRunningOnly));
            object.SetNamedValue(L"running", BoolValue(item.running));
            object.SetNamedValue(L"foreground", BoolValue(item.foreground));
            object.SetNamedValue(L"windowCount", NumberValue(static_cast<double>(item.windows.size())));
            return object;
        }

        winrt::Windows::Data::Json::JsonArray DockItemArray(std::vector<DockWMac::dock::DockItem> const& items)
        {
            winrt::Windows::Data::Json::JsonArray array;
            for (auto const& item : items)
            {
                array.Append(DockItemJson(item));
            }
            return array;
        }

        std::vector<DockWMac::shell::WindowInfo> TaskbarCandidateWindows(std::vector<DockWMac::shell::WindowInfo> const& windows)
        {
            std::vector<DockWMac::shell::WindowInfo> candidates;
            std::copy_if(windows.begin(), windows.end(), std::back_inserter(candidates), [](auto const& window)
            {
                return window.isTaskbarCandidate;
            });
            return candidates;
        }

        std::wstring TimestampForFile()
        {
            auto const now = std::chrono::system_clock::now();
            auto const time = std::chrono::system_clock::to_time_t(now);
            std::tm local{};
            localtime_s(&local, &time);

            std::wstringstream stream;
            stream << std::put_time(&local, L"%Y%m%d-%H%M%S");
            return stream.str();
        }

        std::wstring TimestampIsoLike()
        {
            auto const now = std::chrono::system_clock::now();
            auto const time = std::chrono::system_clock::to_time_t(now);
            std::tm local{};
            localtime_s(&local, &time);

            std::wstringstream stream;
            stream << std::put_time(&local, L"%Y-%m-%dT%H:%M:%S");
            return stream.str();
        }
    }

    int RunDockStateDump()
    {
        try
        {
            auto const paths = ResolveRuntimePaths();
            auto const diagnosticsDir = paths.userDataDir / L"diagnostics";
            std::filesystem::create_directories(diagnosticsDir);

            auto const systemPinned = DockWMac::shell::ReadTaskbarPinnedItems();
            auto const dockState = DockWMac::dock::LoadDockState(paths.dockStateFile);
            auto const diagnosticWindows = DockWMac::shell::EnumerateTopLevelWindowsForDiagnostics();
            auto candidateWindows = TaskbarCandidateWindows(diagnosticWindows);
            auto const dockItems = DockWMac::dock::BuildDockItems(systemPinned, candidateWindows, dockState);

            winrt::Windows::Data::Json::JsonObject root;
            root.SetNamedValue(L"generatedAt", StringValue(TimestampIsoLike()));
            root.SetNamedValue(L"dockStatePath", StringValue(paths.dockStateFile));
            root.SetNamedValue(L"systemTaskbarPinnedItems", PinnedAppArray(systemPinned));
            root.SetNamedValue(L"localPins", PinnedAppArray(dockState.localPins));
            root.SetNamedValue(L"hiddenSystemPins", StringArray(dockState.hiddenSystemPins));
            root.SetNamedValue(L"enumeratedWindows", WindowArray(diagnosticWindows));
            root.SetNamedValue(L"dockItems", DockItemArray(dockItems));

            auto const outputPath = diagnosticsDir / (L"dock-state-" + TimestampForFile() + L".json");
            std::ofstream output{ outputPath, std::ios::binary | std::ios::trunc };
            if (!output)
            {
                return 1;
            }

            output << winrt::to_string(root.Stringify());
            ::OutputDebugStringW((L"Dock state dump written: " + outputPath.wstring() + L"\n").c_str());
            return 0;
        }
        catch (...)
        {
            return 1;
        }
    }
}
