#include "pch.h"
#include "DockStateStore.h"

namespace DockWMac::dock
{
    namespace
    {
        std::string ReadFile(std::filesystem::path const& path)
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

        std::string ToUtf8(std::wstring const& value)
        {
            return winrt::to_string(value);
        }

        std::filesystem::path TempPathFor(std::filesystem::path const& path)
        {
            auto temp = path;
            temp += L".tmp";
            return temp;
        }

        std::filesystem::path BackupPathFor(std::filesystem::path const& path)
        {
            auto backup = path;
            backup += L".bak";
            return backup;
        }

        winrt::hstring HString(std::wstring const& value)
        {
            return winrt::hstring{ value };
        }

        std::wstring StringValue(
            winrt::Windows::Data::Json::JsonObject const& object,
            winrt::hstring const& key)
        {
            return object.HasKey(key) ? std::wstring{ object.GetNamedString(key) } : std::wstring{};
        }

        std::vector<std::wstring> StringArray(
            winrt::Windows::Data::Json::JsonObject const& object,
            winrt::hstring const& key)
        {
            std::vector<std::wstring> values;
            if (!object.HasKey(key))
            {
                return values;
            }

            for (auto const& entry : object.GetNamedArray(key))
            {
                if (entry.ValueType() == winrt::Windows::Data::Json::JsonValueType::String)
                {
                    values.push_back(std::wstring{ entry.GetString() });
                }
            }
            return values;
        }

        std::vector<shell::PinnedApp> PinnedArray(
            winrt::Windows::Data::Json::JsonObject const& object,
            winrt::hstring const& key)
        {
            std::vector<shell::PinnedApp> values;
            if (!object.HasKey(key))
            {
                return values;
            }

            for (auto const& entry : object.GetNamedArray(key))
            {
                if (entry.ValueType() != winrt::Windows::Data::Json::JsonValueType::Object)
                {
                    continue;
                }

                auto pinned = entry.GetObject();
                shell::PinnedApp app;
                app.name = StringValue(pinned, L"name");
                app.linkPath = StringValue(pinned, L"linkPath");
                app.targetPath = StringValue(pinned, L"targetPath");
                app.arguments = StringValue(pinned, L"arguments");
                app.appUserModelId = StringValue(pinned, L"appUserModelId");
                app.iconPath = StringValue(pinned, L"iconPath");
                values.push_back(std::move(app));
            }
            return values;
        }

        winrt::Windows::Data::Json::JsonArray ToJsonArray(std::vector<std::wstring> const& values)
        {
            winrt::Windows::Data::Json::JsonArray array;
            for (auto const& value : values)
            {
                array.Append(winrt::Windows::Data::Json::JsonValue::CreateStringValue(HString(value)));
            }
            return array;
        }

        winrt::Windows::Data::Json::JsonArray ToJsonArray(std::vector<shell::PinnedApp> const& values)
        {
            winrt::Windows::Data::Json::JsonArray array;
            for (auto const& value : values)
            {
                winrt::Windows::Data::Json::JsonObject object;
                object.SetNamedValue(L"name", winrt::Windows::Data::Json::JsonValue::CreateStringValue(HString(value.name)));
                object.SetNamedValue(L"linkPath", winrt::Windows::Data::Json::JsonValue::CreateStringValue(HString(value.linkPath)));
                object.SetNamedValue(L"targetPath", winrt::Windows::Data::Json::JsonValue::CreateStringValue(HString(value.targetPath)));
                object.SetNamedValue(L"arguments", winrt::Windows::Data::Json::JsonValue::CreateStringValue(HString(value.arguments)));
                object.SetNamedValue(L"appUserModelId", winrt::Windows::Data::Json::JsonValue::CreateStringValue(HString(value.appUserModelId)));
                object.SetNamedValue(L"iconPath", winrt::Windows::Data::Json::JsonValue::CreateStringValue(HString(value.iconPath)));
                array.Append(object);
            }
            return array;
        }

        winrt::Windows::Data::Json::JsonObject DockStateJson(DockState const& state)
        {
            winrt::Windows::Data::Json::JsonObject json;
            json.SetNamedValue(L"order", ToJsonArray(state.order));
            json.SetNamedValue(L"importedTaskbarPins", ToJsonArray(state.importedTaskbarPins));
            json.SetNamedValue(L"localPins", ToJsonArray(state.localPins));
            json.SetNamedValue(L"hiddenSystemPins", ToJsonArray(state.hiddenSystemPins));
            return json;
        }

        std::optional<DockState> TryLoadDockState(std::filesystem::path const& path)
        {
            if (!std::filesystem::exists(path))
            {
                return std::nullopt;
            }

            try
            {
                auto json = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(ReadFile(path)));
                DockState state;
                state.order = StringArray(json, L"order");
                state.importedTaskbarPins = PinnedArray(json, L"importedTaskbarPins");
                state.localPins = PinnedArray(json, L"localPins");
                state.hiddenSystemPins = StringArray(json, L"hiddenSystemPins");
                return state;
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        void ReplaceFileWithTemp(std::filesystem::path const& tempPath, std::filesystem::path const& targetPath)
        {
            if (MoveFileExW(
                tempPath.c_str(),
                targetPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                return;
            }

            std::error_code error;
            std::filesystem::rename(tempPath, targetPath, error);
            if (error)
            {
                std::filesystem::remove(tempPath, error);
            }
        }
    }

    DockState LoadDockState(std::filesystem::path const& path)
    {
        if (auto state = TryLoadDockState(path))
        {
            return *state;
        }
        if (auto backup = TryLoadDockState(BackupPathFor(path)))
        {
            return *backup;
        }
        return {};
    }

    void SaveDockState(std::filesystem::path const& path, DockState const& state)
    {
        std::filesystem::create_directories(path.parent_path());

        auto const tempPath = TempPathFor(path);
        auto const backupPath = BackupPathFor(path);
        auto const json = DockStateJson(state);

        std::ofstream output{ tempPath, std::ios::binary | std::ios::trunc };
        output << ToUtf8(std::wstring{ json.Stringify() });
        output.close();
        if (!output)
        {
            std::error_code cleanupError;
            std::filesystem::remove(tempPath, cleanupError);
            return;
        }

        std::error_code error;
        if (std::filesystem::exists(path, error))
        {
            std::filesystem::copy_file(path, backupPath, std::filesystem::copy_options::overwrite_existing, error);
        }

        ReplaceFileWithTemp(tempPath, path);
    }
}
