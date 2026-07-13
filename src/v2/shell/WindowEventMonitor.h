#pragma once

#include <functional>
#include <vector>
#include <windows.h>

namespace DockWMac::shell
{
    enum class WindowEventRefreshReason
    {
        WindowLifecycle,
        ForegroundChanged,
        MinimizedChanged,
    };

    using WindowEventRefreshHandler = std::function<void(WindowEventRefreshReason)>;

    bool IsRefreshWorthyWindowEvent(DWORD event, LONG idObject, LONG idChild, HWND hwnd);

    class WindowEventMonitor
    {
    public:
        WindowEventMonitor() = default;
        ~WindowEventMonitor();

        WindowEventMonitor(WindowEventMonitor const&) = delete;
        WindowEventMonitor& operator=(WindowEventMonitor const&) = delete;

        bool Start(WindowEventRefreshHandler handler);
        void Stop();
        bool Running() const;

    private:
        static void CALLBACK WinEventCallback(
            HWINEVENTHOOK hook,
            DWORD event,
            HWND hwnd,
            LONG idObject,
            LONG idChild,
            DWORD eventThread,
            DWORD eventTime);

        void RegisterHook(DWORD eventMin, DWORD eventMax);
        void Notify(DWORD event) const;

        std::vector<HWINEVENTHOOK> m_hooks;
        WindowEventRefreshHandler m_handler;
    };
}
