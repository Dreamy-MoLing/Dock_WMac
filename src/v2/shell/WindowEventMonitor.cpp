#include "pch.h"
#include "WindowEventMonitor.h"

#include <mutex>

namespace DockWMac::shell
{
    namespace
    {
        std::mutex g_hookMutex;
        std::map<HWINEVENTHOOK, WindowEventMonitor*> g_monitors;

        WindowEventRefreshReason RefreshReasonForEvent(DWORD event)
        {
            switch (event)
            {
            case EVENT_SYSTEM_FOREGROUND:
                return WindowEventRefreshReason::ForegroundChanged;
            case EVENT_SYSTEM_MINIMIZESTART:
            case EVENT_SYSTEM_MINIMIZEEND:
                return WindowEventRefreshReason::MinimizedChanged;
            default:
                return WindowEventRefreshReason::WindowLifecycle;
            }
        }
    }

    bool IsRefreshWorthyWindowEvent(DWORD event, LONG idObject, LONG idChild, HWND hwnd)
    {
        if (!hwnd || idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
        {
            return false;
        }

        switch (event)
        {
        case EVENT_SYSTEM_FOREGROUND:
        case EVENT_SYSTEM_MINIMIZESTART:
        case EVENT_SYSTEM_MINIMIZEEND:
        case EVENT_OBJECT_CREATE:
        case EVENT_OBJECT_DESTROY:
        case EVENT_OBJECT_SHOW:
        case EVENT_OBJECT_HIDE:
            return true;
        default:
            return false;
        }
    }

    WindowEventMonitor::~WindowEventMonitor()
    {
        Stop();
    }

    bool WindowEventMonitor::Start(WindowEventRefreshHandler handler)
    {
        Stop();
        if (!handler)
        {
            return false;
        }

        m_handler = std::move(handler);
        RegisterHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND);
        RegisterHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND);
        RegisterHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE);

        if (m_hooks.empty())
        {
            m_handler = nullptr;
            return false;
        }
        return true;
    }

    void WindowEventMonitor::Stop()
    {
        for (auto const hook : m_hooks)
        {
            {
                std::scoped_lock const lock{ g_hookMutex };
                g_monitors.erase(hook);
            }
            UnhookWinEvent(hook);
        }
        m_hooks.clear();
        m_handler = nullptr;
    }

    bool WindowEventMonitor::Running() const
    {
        return !m_hooks.empty();
    }

    void WindowEventMonitor::RegisterHook(DWORD eventMin, DWORD eventMax)
    {
        auto const hook = SetWinEventHook(
            eventMin,
            eventMax,
            nullptr,
            &WindowEventMonitor::WinEventCallback,
            0,
            0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (!hook)
        {
            return;
        }

        {
            std::scoped_lock const lock{ g_hookMutex };
            g_monitors[hook] = this;
        }
        m_hooks.push_back(hook);
    }

    void CALLBACK WindowEventMonitor::WinEventCallback(
        HWINEVENTHOOK hook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD,
        DWORD)
    {
        if (!IsRefreshWorthyWindowEvent(event, idObject, idChild, hwnd))
        {
            return;
        }

        WindowEventMonitor* monitor{};
        {
            std::scoped_lock const lock{ g_hookMutex };
            if (auto const it = g_monitors.find(hook); it != g_monitors.end())
            {
                monitor = it->second;
            }
        }

        if (monitor)
        {
            monitor->Notify(event);
        }
    }

    void WindowEventMonitor::Notify(DWORD event) const
    {
        if (m_handler)
        {
            m_handler(RefreshReasonForEvent(event));
        }
    }
}
