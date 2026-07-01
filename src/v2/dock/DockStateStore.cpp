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

        std::wstring FromUtf8(std::string const& value)
        {
            return winrt::to_hstring(value).c_str();
        }

        std::string EscapeJson(std::wstring const& value)
        {
            std::string escaped;
            for (auto ch : ToUtf8(value))
            {
                if (ch == '\\' || ch == '"')
                {
                    escaped.push_back('\\');
                }
                escaped.push_back(ch);
            }
            return escaped;
        }

        std::vector<std::wstring> ParseOrder(std::string const& json)
        {
            std::vector<std::wstring> values;
            auto key = json.find("\"order\"");
            if (key == std::string::npos)
            {
                return values;
            }

            auto open = json.find('[', key);
            auto close = json.find(']', open);
            if (open == std::string::npos || close == std::string::npos || close <= open)
            {
                return values;
            }

            auto pos = open + 1;
            while (pos < close)
            {
                auto quote = json.find('"', pos);
                if (quote == std::string::npos || quote >= close)
                {
                    break;
                }

                std::string value;
                for (auto i = quote + 1; i < close; ++i)
                {
                    if (json[i] == '\\' && i + 1 < close)
                    {
                        value.push_back(json[++i]);
                        continue;
                    }
                    if (json[i] == '"')
                    {
                        pos = i + 1;
                        values.push_back(FromUtf8(value));
                        break;
                    }
                    value.push_back(json[i]);
                }
            }
            return values;
        }
    }

    DockState LoadDockState(std::filesystem::path const& path)
    {
        DockState state;
        if (!std::filesystem::exists(path))
        {
            return state;
        }

        state.order = ParseOrder(ReadFile(path));
        return state;
    }

    void SaveDockState(std::filesystem::path const& path, DockState const& state)
    {
        std::filesystem::create_directories(path.parent_path());

        std::ofstream output{ path, std::ios::binary | std::ios::trunc };
        output << "{\n  \"order\": [";
        for (size_t i = 0; i < state.order.size(); ++i)
        {
            output << (i == 0 ? "\n    \"" : ",\n    \"") << EscapeJson(state.order[i]) << "\"";
        }
        output << "\n  ]\n}\n";
    }
}
