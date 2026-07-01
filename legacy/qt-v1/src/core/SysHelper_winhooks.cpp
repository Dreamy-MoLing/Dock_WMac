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
#include <QFileInfo>
#include <QTimer>
#include <QMetaObject>
#include <QString>
#include <utility>

// ─── 全局钩子句柄 ──────────────────────────────────────────

extern HHOOK g_keyboardHook;
extern SysHelper *g_sysHelperForHook;
extern bool g_nativeTaskbarHidden;

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

static bool isShellTaskbarWindow(HWND hwnd)
{
    wchar_t className[256] = {};
    if (!GetClassNameW(hwnd, className, 255)) return false;
    return wcscmp(className, L"Shell_TrayWnd") == 0
        || wcscmp(className, L"Shell_SecondaryTrayWnd") == 0;
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

    if (g_nativeTaskbarHidden
        && (event == EVENT_OBJECT_CREATE || event == EVENT_OBJECT_SHOW)
        && isShellTaskbarWindow(hwnd)) {
        SysHelper *helper = g_sysHelperForHook;
        QMetaObject::invokeMethod(helper, [helper]() {
            if (g_nativeTaskbarHidden)
                helper->hideNativeTaskbar();
        }, Qt::QueuedConnection);
    }

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
    uninstallWindowHook();
    if (g_sysHelperForHook == this) {
        g_sysHelperForHook = nullptr;
    }
    uninstallKeyboardHook();
}

// ─── 钩子安装/卸载 ─────────────────────────────────────────

bool SysHelper::installWindowHook()
{
    g_sysHelperForHook = this;
    if (!m_windowHooks.isEmpty()) return true;

    HWINEVENTHOOK foregroundHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_MAXIMIZESTART,
        nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!foregroundHook) return false;
    m_windowHooks.append(foregroundHook);

    HWINEVENTHOOK objectHook = SetWinEventHook(
        EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (!objectHook) {
        uninstallWindowHook();
        return false;
    }
    m_windowHooks.append(objectHook);

    QTimer::singleShot(500, this, [this]() {
        bool maximized = getForegroundWindowState();
        emit foregroundWindowChanged(maximized);
        bool anyMax = hasMaximizedOrFullscreenWindowOnMonitor();
        emit fullscreenStateChanged(anyMax);
    });

    return true;
}

void SysHelper::uninstallWindowHook()
{
    for (HWINEVENTHOOK hook : std::as_const(m_windowHooks)) {
        if (hook)
            UnhookWinEvent(hook);
    }
    m_windowHooks.clear();
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

static QString windowClassName(HWND hwnd)
{
    wchar_t className[256] = {};
    if (!GetClassNameW(hwnd, className, 255)) return {};
    return QString::fromWCharArray(className);
}

static QString windowTitle(HWND hwnd)
{
    wchar_t title[256] = {};
    if (!GetWindowTextW(hwnd, title, 255)) return {};
    return QString::fromWCharArray(title);
}

static QString processBaseName(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return {};

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};

    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    const bool ok = QueryFullProcessImageNameW(process, 0, path, &size);
    CloseHandle(process);
    if (!ok) return {};

    return QFileInfo(QString::fromWCharArray(path)).baseName();
}

static bool isIgnoredFullscreenCandidate(HWND hwnd)
{
    const QString cls = windowClassName(hwnd);
    if (cls.isEmpty()) return false;

    if (cls == QStringLiteral("Progman")
        || cls == QStringLiteral("WorkerW")
        || cls == QStringLiteral("Shell_TrayWnd")
        || cls == QStringLiteral("Shell_SecondaryTrayWnd")
        || cls == QStringLiteral("Windows.UI.Core.CoreWindow")
        || cls == QStringLiteral("MultitaskingViewFrame")
        || cls == QStringLiteral("XamlExplorerHostIslandWindow")
        || cls == QStringLiteral("CEF-OSC-WIDGET")) {
        return true;
    }

    if (cls == QStringLiteral("ApplicationFrameWindow")) {
        const QString title = windowTitle(hwnd);
        const QString process = processBaseName(hwnd);
        if (title.contains(QStringLiteral("Realtek Audio Console"), Qt::CaseInsensitive)
            || process.compare(QStringLiteral("RtkUWP"), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

static BOOL CALLBACK EnumMaximizedWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto *ctx = reinterpret_cast<MonitorScanContext *>(lParam);
    if (!ctx || ctx->found) return FALSE;

    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    HMONITOR winMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (winMonitor != ctx->hMonitor) return TRUE;

    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &wp) && wp.showCmd == SW_SHOWMAXIMIZED) {
        if (isIgnoredFullscreenCandidate(hwnd)) {
            qDebug() << "忽略全屏候选: maximized" << windowClassName(hwnd)
                     << windowTitle(hwnd) << processBaseName(hwnd);
            return TRUE;
        }
        qDebug() << "接受全屏候选: maximized" << windowClassName(hwnd)
                 << windowTitle(hwnd) << processBaseName(hwnd);
        ctx->found = true;
        return FALSE;
    }

    RECT winRect;
    if (!ctx->helper->getExtendedFrameBounds(hwnd, winRect)) return TRUE;
    int winW = winRect.right - winRect.left;
    int winH = winRect.bottom - winRect.top;
    if (abs(winW - ctx->screenW) <= 10 && abs(winH - ctx->screenH) <= 10) {
        if (isIgnoredFullscreenCandidate(hwnd)) {
            qDebug() << "忽略全屏候选: fullscreen" << windowClassName(hwnd)
                     << windowTitle(hwnd) << processBaseName(hwnd);
            return TRUE;
        }
        qDebug() << "接受全屏候选: fullscreen" << windowClassName(hwnd)
                 << windowTitle(hwnd) << processBaseName(hwnd);
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
