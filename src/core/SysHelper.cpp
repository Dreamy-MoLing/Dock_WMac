/**
 * @file SysHelper.cpp
 * @brief Windows 平台系统适配完整实现
 *
 * 使用 Win32 API、COM 接口实现任务栏固定项读取、窗口钩子与键盘钩子。
 * 平台适配：任务栏 .lnk 解析、SetWinEventHook、SetWindowsHookEx。
 */

// Windows SDK 版本宏必须在所有 #include 之前定义
#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

// Windows 8 SDK constant (not in older headers)
#ifndef EVENT_SYSTEM_MAXIMIZESTART
#define EVENT_SYSTEM_MAXIMIZESTART 0x0017
#endif

#include "core/SysHelper.h"

#include <windows.h>
#include <dwmapi.h>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QTimer>
#include <QProcess>
#include <QCoreApplication>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

// ─── 构造函数 ──────────────────────────────────────────────

SysHelper::SysHelper(QObject *parent)
    : QObject(parent)
{
}

// ─── 全局钩子句柄与 SysHelper 指针 ──────────────────────────

static HHOOK g_keyboardHook = nullptr;
static SysHelper *g_sysHelperForHook = nullptr;

/**
 * @brief 低级键盘钩子回调
 *
 * 捕获 Win 键按下事件，通过 Qt 信号通知 DockManager。
 * 使用全局指针避免 QCoreApplication::instance() 在极端时序下返回 nullptr。
 */
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
            // 不拦截 Win 键，让系统正常处理（开始菜单、Win+D 等快捷键）
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

    // 只处理顶级窗口事件
    if (idObject != OBJID_WINDOW || hwnd == nullptr) return;

    if (!g_sysHelperForHook) return;

    switch (event) {
    case EVENT_SYSTEM_FOREGROUND:
    case EVENT_SYSTEM_MINIMIZESTART:
    case EVENT_SYSTEM_MAXIMIZESTART:
        // 延迟一帧后检查状态，确保窗口状态已更新
        QMetaObject::invokeMethod(g_sysHelperForHook, []() {
            bool maximized = g_sysHelperForHook->getForegroundWindowState();
            emit g_sysHelperForHook->foregroundWindowChanged(maximized);
        }, Qt::QueuedConnection);
        break;
    default:
        break;
    }
}


// ─── 公共接口实现 ─────────────────────────────────────────

bool SysHelper::installWindowHook()
{
    g_sysHelperForHook = this;

    // Windows 使用 SetWinEventHook 监听窗口事件
    HWINEVENTHOOK hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!hook) return false;

    hook = SetWinEventHook(
        EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MAXIMIZESTART,
        nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!hook) return false;

    // 初始检测
    QTimer::singleShot(500, this, [this]() {
        bool maximized = getForegroundWindowState();
        emit foregroundWindowChanged(maximized);
    });

    return true;
}

bool SysHelper::installKeyboardHook()
{
    g_sysHelperForHook = this;
    if (g_keyboardHook) return true;  // 已安装

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
    g_sysHelperForHook = nullptr;
}

bool SysHelper::getForegroundWindowState()
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    // 检查是否是最小化状态（忽略）
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return false;

    // 检查最大化
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    if (!GetWindowPlacement(hwnd, &wp)) return false;

    if (wp.showCmd == SW_SHOWMAXIMIZED) return true;

    // 检查全屏：窗口尺寸 == 屏幕工作区尺寸
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) return false;

    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) return false;

    int winW = windowRect.right - windowRect.left;
    int winH = windowRect.bottom - windowRect.top;
    int screenW = mi.rcMonitor.right - mi.rcMonitor.left;
    int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;

    // 全屏：窗口覆盖整个屏幕（允许 10px 容差）
    return (abs(winW - screenW) <= 10 && abs(winH - screenH) <= 10);
}

bool SysHelper::setAutoStart(bool enabled)
{
    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) return false;

    if (!enabled) {
        result = RegDeleteValue(hKey, L"Dock_WMac");
        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }

    QString execPath = QCoreApplication::applicationFilePath();
    std::wstring wExecPath = execPath.toStdWString();

    result = RegSetValueEx(
        hKey, L"Dock_WMac", 0, REG_SZ,
        reinterpret_cast<const BYTE *>(wExecPath.c_str()),
        (wExecPath.size() + 1) * sizeof(wchar_t));
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool SysHelper::isAutoStartEnabled() const
{
    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) return false;

    result = RegQueryValueEx(hKey, L"Dock_WMac", nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

int SysHelper::getWindowCount(const QString &wmClass)
{
    struct Ctx { int count; QString target; };
    Ctx ctx{0, wmClass.toLower()};

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

        Ctx *pCtx = reinterpret_cast<Ctx *>(lParam);

        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) return TRUE;

        wchar_t exeName[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, exeName, &size)) {
            QString fullPath = QString::fromWCharArray(exeName);
            QString exe = QFileInfo(fullPath).fileName().toLower();
            exe.replace(".exe", "");
            if (exe == pCtx->target) {
                pCtx->count++;
            }
        }
        CloseHandle(hProcess);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return ctx.count;
}

bool SysHelper::activateWindow(const QString &wmClass)
{
    struct Ctx { HWND hwnd; QString target; };
    Ctx ctx{nullptr, wmClass.toLower()};

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

        Ctx *pCtx = reinterpret_cast<Ctx *>(lParam);

        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) return TRUE;

        wchar_t exeName[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, exeName, &size)) {
            QString fullPath = QString::fromWCharArray(exeName);
            QString exe = QFileInfo(fullPath).fileName().toLower();
            exe.replace(".exe", "");
            if (exe == pCtx->target) {
                pCtx->hwnd = hwnd;
                CloseHandle(hProcess);
                return FALSE;
            }
        }
        CloseHandle(hProcess);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    if (!ctx.hwnd) return false;

    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(ctx.hwnd);
    ShowWindow(ctx.hwnd, SW_RESTORE);
    SetFocus(ctx.hwnd);
    return true;
}

void SysHelper::showWindowPicker()
{
    // 触发 Task View（Windows 10/11）
    keybd_event(VK_LWIN, 0, 0, 0);
    keybd_event(VK_TAB, 0, 0, 0);
    keybd_event(VK_TAB, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
}

// ─── DWM 毛玻璃模糊 ─────────────────────────────────────────

void SysHelper::enableBlurBehindWindow(WId winId)
{
    HWND hwnd = reinterpret_cast<HWND>(winId);
    if (!hwnd) return;

    DWM_BLURBEHIND bb;
    ZeroMemory(&bb, sizeof(bb));
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    bb.hRgnBlur = nullptr;

    HRESULT hr = DwmEnableBlurBehindWindow(hwnd, &bb);
    if (FAILED(hr)) {
        qWarning() << "DwmEnableBlurBehindWindow 失败，hr:" << Qt::hex << hr;
    }
}

void SysHelper::enableBlurBehindWindow(WId winId, const QRect &blurRect, int cornerRadius)
{
    HWND hwnd = reinterpret_cast<HWND>(winId);
    if (!hwnd) return;

    // 创建圆角矩形区域，仅对背景区域应用模糊
    HRGN hRgn = CreateRoundRectRgn(
        blurRect.left(), blurRect.top(),
        blurRect.right(), blurRect.bottom(),
        cornerRadius, cornerRadius);

    DWM_BLURBEHIND bb;
    ZeroMemory(&bb, sizeof(bb));
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = TRUE;
    bb.hRgnBlur = hRgn;

    HRESULT hr = DwmEnableBlurBehindWindow(hwnd, &bb);
    if (FAILED(hr)) {
        qWarning() << "DwmEnableBlurBehindWindow(region) 失败，hr:" << Qt::hex << hr;
    }

    // DwmEnableBlurBehindWindow 会拷贝 region，安全删除
    DeleteObject(hRgn);
}

bool SysHelper::isBlurSupported() const
{
    BOOL dwmEnabled = FALSE;
    HRESULT hr = DwmIsCompositionEnabled(&dwmEnabled);
    return SUCCEEDED(hr) && dwmEnabled;
}

bool SysHelper::isLightTheme() const
{
    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) return true;

    DWORD value = 1;
    DWORD size = sizeof(value);
    result = RegQueryValueEx(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS) ? (value != 0) : true;
}

// ─── 任务栏自动隐藏检测 ─────────────────────────────────────

bool SysHelper::isTaskbarAutoHideEnabled() const
{
    // 读取 StuckRects3\Settings 二进制值，Windows 10/11 通用
    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects3",
        0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) return false;

    DWORD size = 0;
    result = RegQueryValueEx(hKey, L"Settings", nullptr, nullptr, nullptr, &size);
    if (result != ERROR_SUCCESS || size < 16) {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<BYTE> data(size);
    result = RegQueryValueEx(hKey, L"Settings", nullptr, nullptr, data.data(), &size);
    RegCloseKey(hKey);
    if (result != ERROR_SUCCESS) return false;

    // Settings[12] bit 0 = 任务栏自动隐藏
    return (size > 12) && (data[12] & 0x01);
}

QPoint SysHelper::cursorPos() const
{
    POINT pt;
    if (GetCursorPos(&pt)) {
        return QPoint(pt.x, pt.y);
    }
    return QPoint();
}

// ─── 原生任务栏管理 ─────────────────────────────────────────

static HWND g_taskbarHandle = nullptr;

void SysHelper::hideNativeTaskbar()
{
    HWND hTaskbar = FindWindow(L"Shell_TrayWnd", nullptr);
    if (hTaskbar) {
        g_taskbarHandle = hTaskbar;
        ShowWindow(hTaskbar, SW_HIDE);
    }
}

void SysHelper::restoreNativeTaskbar()
{
    HWND hTaskbar = g_taskbarHandle;
    if (!hTaskbar) {
        hTaskbar = FindWindow(L"Shell_TrayWnd", nullptr);
    }
    if (hTaskbar) {
        SetWindowPos(hTaskbar, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        g_taskbarHandle = nullptr;
    }
}
