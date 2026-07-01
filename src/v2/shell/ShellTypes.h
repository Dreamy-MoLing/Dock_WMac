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
        std::wstring iconPath;
    };

    struct WindowInfo
    {
        HWND hwnd{};
        uint32_t processId{};
        std::wstring title;
        std::wstring executablePath;
        std::wstring appUserModelId;
        std::wstring iconPath;
        bool minimized{};
        bool cloaked{};
        bool foreground{};
    };
}
