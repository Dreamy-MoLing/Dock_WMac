#include "pch.h"
#include "AppSettings.h"

namespace DockWMac::infra
{
    namespace
    {
        std::string ReadUtf8(std::filesystem::path const& path)
        {
            std::ifstream input{ path, std::ios::binary };
            if (!input)
            {
                return {};
            }

            std::ostringstream buffer;
            buffer << input.rdbuf();
            return buffer.str();
        }

        void WriteUtf8(std::filesystem::path const& path, std::string const& text)
        {
            std::ofstream output{ path, std::ios::binary | std::ios::trunc };
            output << text;
        }

        int32_t ClampDimension(double value, int32_t fallback, int32_t min, int32_t max)
        {
            if (value < min || value > max)
            {
                return fallback;
            }
            return static_cast<int32_t>(value);
        }
    }

    AppSettings LoadAppSettings(RuntimePaths const& paths)
    {
        AppSettings settings;
        if (!std::filesystem::exists(paths.configFile))
        {
            return settings;
        }

        try
        {
            auto json = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(ReadUtf8(paths.configFile)));
            if (json.HasKey(L"placement"))
            {
                settings.placement = DockWMac::platform::PlacementFromConfig(json.GetNamedString(L"placement"));
            }
            if (json.HasKey(L"autoHide"))
            {
                settings.autoHide = json.GetNamedBoolean(L"autoHide");
            }
            if (json.HasKey(L"reducedMotion"))
            {
                settings.reducedMotion = json.GetNamedBoolean(L"reducedMotion");
            }
            if (json.HasKey(L"dockWidth"))
            {
                settings.dockWidth = ClampDimension(json.GetNamedNumber(L"dockWidth"), settings.dockWidth, 320, 1600);
            }
            if (json.HasKey(L"dockHeight"))
            {
                settings.dockHeight = ClampDimension(json.GetNamedNumber(L"dockHeight"), settings.dockHeight, 64, 240);
            }
        }
        catch (...)
        {
            return AppSettings{};
        }

        return settings;
    }

    void SaveAppSettings(RuntimePaths const& paths, AppSettings const& settings)
    {
        EnsureRuntimePaths(paths);

        winrt::Windows::Data::Json::JsonObject json;
        json.SetNamedValue(L"placement", winrt::Windows::Data::Json::JsonValue::CreateStringValue(
            DockWMac::platform::ToConfigString(settings.placement)));
        json.SetNamedValue(L"autoHide", winrt::Windows::Data::Json::JsonValue::CreateBooleanValue(settings.autoHide));
        json.SetNamedValue(L"reducedMotion", winrt::Windows::Data::Json::JsonValue::CreateBooleanValue(settings.reducedMotion));
        json.SetNamedValue(L"dockWidth", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(settings.dockWidth));
        json.SetNamedValue(L"dockHeight", winrt::Windows::Data::Json::JsonValue::CreateNumberValue(settings.dockHeight));

        WriteUtf8(paths.configFile, winrt::to_string(json.Stringify()));
    }
}
