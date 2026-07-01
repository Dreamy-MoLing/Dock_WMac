#pragma once

namespace DockWMac::shell
{
    struct PinnedApp
    {
        std::wstring name;
        std::wstring linkPath;
        std::wstring targetPath;
        std::wstring arguments;
        std::wstring appUserModelId;
    };

    struct WindowInfo
    {
        HWND hwnd{};
        uint32_t processId{};
        std::wstring title;
        std::wstring executablePath;
        std::wstring appUserModelId;
        bool minimized{};
        bool cloaked{};
        bool foreground{};
    };
}
