/**
 * @file SysHelper_winhooks.cpp
 * @brief SysHelper 系统钩子 & 全屏检测实现
 *
 * WinEvent 钩子、键盘钩子、前台窗口状态、显示器全屏扫描。
 */

#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#ifndef EVENT_SYSTEM_MAXIMIZESTART
#define EVENT_SYSTEM_MAXIMIZESTART 0x0017
#endif

#include "core/SysHelper.h"

#include <windows.h>
#include <QDebug>
#include <QTimer>
#include <QMetaObject>

// ─── 全局钩子句柄 ──────────────────────────────────────────

extern HHOOK g_keyboardHook;
extern SysHelper *g_sysHelperForHook;

// ─── 键盘钩子回调 ──────────────────────────────────────────

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *pKb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) &&
            (pKb->vkCode == VK_LWIN || pKb->vkCode == VK_RWIN)) {
            if (g_sysHelperForHook) {
                QMetaObject::invokeMethod(g_sysHelperForHook, []() {
                    emit g_sysHelperForHook->winKeyPressed();
                }, Qt::QueuedConnection);
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// ─── 窗口事件钩子回调 ─────────────────────────────────────

static VOID CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
                                   HWND hwnd, LONG idObject, LONG idChild,
                                   DWORD dwEventThread, DWORD dwmsEventTime)
{
    Q_UNUSED(hWinEventHook);
    Q_UNUSED(idObject);
    Q_UNUSED(idChild);
    Q_UNUSED(dwEventThread);
    Q_UNUSED(dwmsEventTime);

    if (idObject != OBJID_WINDOW || hwnd == nullptr) return;
    if (!g_sysHelperForHook) return;

    switch (event) {
    case EVENT_SYSTEM_FOREGROUND:
    case EVENT_SYSTEM_MINIMIZESTART:
    case EVENT_SYSTEM_MAXIMIZESTART:
        QMetaObject::invokeMethod(g_sysHelperForHook, []() {
            bool maximized = g_sysHelperForHook->getForegroundWindowState();
            emit g_sysHelperForHook->foregroundWindowChanged(maximized);
        }, Qt::QueuedConnection);
        QMetaObject::invokeMethod(g_sysHelperForHook, []() {
            g_sysHelperForHook->triggerFullscreenDebounce();
        }, Qt::QueuedConnection);
        break;
    case EVENT_OBJECT_CREATE:
    case EVENT_OBJECT_DESTROY:
    case EVENT_OBJECT_SHOW:
    case EVENT_OBJECT_HIDE: {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != 0 && pid != 4) {
            QMetaObject::invokeMethod(g_sysHelperForHook, [pid]() {
                emit g_sysHelperForHook->windowEventOccurred(pid);
            }, Qt::QueuedConnection);
            // 窗口变为可见时，额外触发通知信号（用于 DockItem 绿色光点）
            if (event == EVENT_OBJECT_SHOW) {
                QMetaObject::invokeMethod(g_sysHelperForHook, [pid]() {
                    emit g_sysHelperForHook->windowShowOccurred(pid);
                }, Qt::QueuedConnection);
            }
        }
        break;
    }
    default:
        break;
    }
}

// ─── 析构函数 ──────────────────────────────────────────────

SysHelper::~SysHelper()
{
    if (g_sysHelperForHook == this) {
        g_sysHelperForHook = nullptr;
    }
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
}

// ─── 钩子安装/卸载 ─────────────────────────────────────────

bool SysHelper::installWindowHook()
{
    g_sysHelperForHook = this;

    HWINEVENTHOOK hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_MAXIMIZESTART,
        nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!hook) return false;

    hook = SetWinEventHook(
        EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (!hook) return false;

    QTimer::singleShot(500, this, [this]() {
        bool maximized = getForegroundWindowState();
        emit foregroundWindowChanged(maximized);
        bool anyMax = hasMaximizedOrFullscreenWindowOnMonitor();
        emit fullscreenStateChanged(anyMax);
    });

    return true;
}

bool SysHelper::installKeyboardHook()
{
    g_sysHelperForHook = this;
    if (g_keyboardHook) return true;

    g_keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL, LowLevelKeyboardProc,
        GetModuleHandle(nullptr), 0);

    if (g_keyboardHook) {
        qInfo() << "键盘钩子已安装";
        return true;
    }
    qWarning() << "键盘钩子安装失败，错误码:" << GetLastError();
    return false;
}

void SysHelper::uninstallKeyboardHook()
{
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
        qInfo() << "键盘钩子已卸载";
    }
}

// ─── 前台窗口状态 ──────────────────────────────────────────

bool SysHelper::getForegroundWindowState()
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return false;

    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    if (!GetWindowPlacement(hwnd, &wp)) return false;

    if (wp.showCmd == SW_SHOWMAXIMIZED) return true;

    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) return false;

    RECT windowRect;
    if (!getExtendedFrameBounds(hwnd, windowRect)) return false;

    int winW = windowRect.right - windowRect.left;
    int winH = windowRect.bottom - windowRect.top;
    int screenW = mi.rcMonitor.right - mi.rcMonitor.left;
    int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;

    return (abs(winW - screenW) <= 10 && abs(winH - screenH) <= 10);
}

// ─── 显示器全屏扫描 ────────────────────────────────────────

struct MonitorScanContext {
    HMONITOR hMonitor;
    bool found;
    int screenW;
    int screenH;
    SysHelper *helper;  // 用于调用 getExtendedFrameBounds
};

static bool isSystemUiWindow(HWND hwnd)
{
    wchar_t className[256] = {};
    if (!GetClassNameW(hwnd, className, 255)) return false;

    if (wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0) return true;
    if (wcscmp(className, L"MultitaskingViewFrame") == 0) return true;
    if (wcscmp(className, L"XamlExplorerHostIslandWindow") == 0) return true;

    return false;
}

static BOOL CALLBACK EnumMaximizedWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto *ctx = reinterpret_cast<MonitorScanContext *>(lParam);
    if (!ctx || ctx->found) return FALSE;

    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    if (isSystemUiWindow(hwnd)) return TRUE;

    HMONITOR winMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (winMonitor != ctx->hMonitor) return TRUE;

    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &wp) && wp.showCmd == SW_SHOWMAXIMIZED) {
        ctx->found = true;
        return FALSE;
    }

    RECT winRect;
    if (!ctx->helper->getExtendedFrameBounds(hwnd, winRect)) return TRUE;
    int winW = winRect.right - winRect.left;
    int winH = winRect.bottom - winRect.top;
    if (abs(winW - ctx->screenW) <= 10 && abs(winH - ctx->screenH) <= 10) {
        ctx->found = true;
        return FALSE;
    }

    return TRUE;
}

bool SysHelper::hasMaximizedOrFullscreenWindowOnMonitor(int monitorIndex)
{
    HMONITOR hMonitor = nullptr;

    if (monitorIndex < 0) {
        POINT pt = {0, 0};
        hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    } else {
        struct EnumCtx { int target; HMONITOR result; };
        EnumCtx enumCtx{monitorIndex, nullptr};
        EnumDisplayMonitors(nullptr, nullptr,
            [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto *ctx = reinterpret_cast<EnumCtx *>(lParam);
                if (ctx->target == 0) { ctx->result = hMon; return FALSE; }
                ctx->target--;
                return TRUE;
            }, reinterpret_cast<LPARAM>(&enumCtx));
        hMonitor = enumCtx.result;
    }

    if (!hMonitor) {
        POINT pt = {0, 0};
        hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }

    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) return false;
    int screenW = mi.rcMonitor.right - mi.rcMonitor.left;
    int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;

    MonitorScanContext ctx{hMonitor, false, screenW, screenH, this};
    EnumWindows(EnumMaximizedWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    return ctx.found;
}

void SysHelper::triggerFullscreenDebounce()
{
    if (m_fullscreenDebounceTimer) {
        m_fullscreenDebounceTimer->start();
    }
}
