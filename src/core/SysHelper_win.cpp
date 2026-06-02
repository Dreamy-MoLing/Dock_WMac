/**
 * @file SysHelper_win.cpp
 * @brief Windows 平台系统适配完整实现
 *
 * 使用 Win32 API、COM 接口实现任务栏固定项读取、窗口钩子与键盘钩子。
 * 平台适配：任务栏 .lnk 解析、SetWinEventHook、SetWindowsHookEx。
 */

#include "core/SysHelper.h"

#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

// MinGW 11.5 headers missing this constant (added in Windows 8 SDK)
#ifndef EVENT_SYSTEM_MAXIMIZESTART
#define EVENT_SYSTEM_MAXIMIZESTART 0x0017
#endif

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QTimer>
#include <QProcess>
#include <QCoreApplication>
#include <QSettings>
#include <QRegularExpression>
#include <QPixmap>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

// ─── 全局键盘钩子句柄 ─────────────────────────────────────

static HHOOK g_keyboardHook = nullptr;

/**
 * @brief 低级键盘钩子回调
 *
 * 捕获 Win 键按下事件，通过 Qt 信号通知 DockManager。
 */
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *pKb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) &&
            (pKb->vkCode == VK_LWIN || pKb->vkCode == VK_RWIN)) {
            // 通过全局单例发射信号
            // 使用 qApp 的 event loop 线程安全投递
            static SysHelper *s_helper = nullptr;
            if (!s_helper) {
                // 在第一次钩子触发时尝试查找，后续缓存
                s_helper = qobject_cast<SysHelper *>(QCoreApplication::instance()->findChild<SysHelper *>());
            }
            if (s_helper) {
                QMetaObject::invokeMethod(s_helper, [s_helper]() {
                    emit s_helper->winKeyPressed();
                }, Qt::QueuedConnection);
            }
            return 1;  // 阻止 Win 键传递给系统
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

    static SysHelper *s_helper = nullptr;
    if (!s_helper) {
        s_helper = qobject_cast<SysHelper *>(QCoreApplication::instance()->findChild<SysHelper *>());
    }
    if (!s_helper) return;

    switch (event) {
    case EVENT_SYSTEM_FOREGROUND:
    case EVENT_SYSTEM_MINIMIZESTART:
    case EVENT_SYSTEM_MAXIMIZESTART:
    case EVENT_OBJECT_LOCATIONCHANGE:
        // 延迟一帧后检查状态，确保窗口状态已更新
        QMetaObject::invokeMethod(s_helper, [s_helper]() {
            bool maximized = s_helper->getForegroundWindowState();
            emit s_helper->foregroundWindowChanged(maximized);
        }, Qt::QueuedConnection);
        break;
    default:
        break;
    }
}


// ─── 帮助函数 ────────────────────────────────────────────

/**
 * @brief 从 .lnk 快捷方式文件解析出目标路径
 */
static QString resolveShortcut(const QString &lnkPath)
{
    if (!QFileInfo::exists(lnkPath)) return {};

    // 使用 IShellLink COM 接口解析 .lnk
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return {};

    IShellLink *psl = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IShellLink, reinterpret_cast<void **>(&psl));
    if (FAILED(hr)) {
        CoUninitialize();
        return {};
    }

    IPersistFile *ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&ppf));
    if (FAILED(hr)) {
        psl->Release();
        CoUninitialize();
        return {};
    }

    // 加载 .lnk 文件
    WCHAR wszPath[MAX_PATH];
    lnkPath.toWCharArray(wszPath);
    wszPath[lnkPath.length()] = 0;
    hr = ppf->Load(wszPath, STGM_READ);

    QString execPath;
    if (SUCCEEDED(hr)) {
        WIN32_FIND_DATA wfd;
        WCHAR szPath[MAX_PATH];
        hr = psl->GetPath(szPath, MAX_PATH, &wfd, SLGP_RAWPATH);
        if (SUCCEEDED(hr)) {
            execPath = QString::fromWCharArray(szPath);
        }
    }

    ppf->Release();
    psl->Release();
    CoUninitialize();
    return execPath;
}


// ─── 公共接口实现 ─────────────────────────────────────────

QList<DockItemData> SysHelper::getPinnedItems()
{
    QList<DockItemData> items;

    // Windows 10/11 任务栏固定项路径
    QString taskbarPath = QDir::homePath()
        + "/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/";
    QDir dir(taskbarPath);
    if (!dir.exists()) {
        // 回退: Windows 7 风格路径
        taskbarPath = QDir::homePath()
            + "/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/";
        dir.setPath(taskbarPath);
    }
    if (!dir.exists()) return items;

    const auto entries = dir.entryInfoList({"*.lnk"}, QDir::Files, QDir::Name);
    for (const QFileInfo &fi : entries) {
        QString execPath = resolveShortcut(fi.absoluteFilePath());
        if (execPath.isEmpty()) continue;

        DockItemData item;
        item.appId = fi.completeBaseName();
        item.displayName = fi.completeBaseName();
        item.execPath = execPath;
        item.iconPath = fi.absoluteFilePath();  // .lnk 本身可作为图标源
        item.isRunning = false;
        item.badgeCount = 0;
        items.append(item);
    }

    return items;
}

QString SysHelper::extractAppIcon(const QString &appId)
{
    // 使用 SHGetFileInfo 提取大图标
    SHFILEINFO shfi;
    DWORD_PTR result = SHGetFileInfo(
        reinterpret_cast<const wchar_t *>(appId.utf16()),
        FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi),
        SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES);

    if (!result || !shfi.hIcon) {
        // 回退：直接尝试文件图标
        result = SHGetFileInfo(
            reinterpret_cast<const wchar_t *>(appId.utf16()),
            FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi),
            SHGFI_ICON | SHGFI_LARGEICON);
        if (!result || !shfi.hIcon) return {};
    }

    // HICON → QPixmap（Qt 5.15 MinGW 无 fromWinHICON/fromWinHBITMAP）
    // 使用 QImage 从 HICON 的位图数据手动构建
    QPixmap pix;
    ICONINFO iconInfo;
    memset(&iconInfo, 0, sizeof(iconInfo));
    if (GetIconInfo(shfi.hIcon, &iconInfo) && iconInfo.hbmColor) {
        // 获取位图信息
        BITMAP bm;
        memset(&bm, 0, sizeof(bm));
        if (GetObject(iconInfo.hbmColor, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            QImage img(bm.bmWidth, bm.bmHeight, QImage::Format_ARGB32);
            // 通过 GetDIBits 读取位图数据
            HDC hdc = GetDC(nullptr);
            if (hdc) {
                BITMAPINFO bi;
                memset(&bi, 0, sizeof(bi));
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = bm.bmWidth;
                bi.bmiHeader.biHeight = -bm.bmHeight;  // 倒置，使原点在左上
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
                if (GetDIBits(hdc, iconInfo.hbmColor, 0, bm.bmHeight,
                              img.bits(), &bi, DIB_RGB_COLORS)) {
                    pix = QPixmap::fromImage(img);
                }
                ReleaseDC(nullptr, hdc);
            }
        }
        if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    }
    DestroyIcon(shfi.hIcon);

    if (pix.isNull()) return {};

    QString cacheDir = QDir::tempPath() + "/dock_wmac_icons";
    QDir().mkpath(cacheDir);
    QString cachePath = cacheDir + "/" + appId + ".png";
    pix.save(cachePath, "PNG");
    return cachePath;
}

bool SysHelper::installWindowHook()
{
    // Windows 使用 SetWinEventHook 监听窗口事件
    // 注册三个核心事件:
    //   EVENT_SYSTEM_FOREGROUND  — 前台窗口切换
    //   EVENT_OBJECT_LOCATIONCHANGE — 窗口位置/大小变化
    //   EVENT_SYSTEM_MINIMIZESTART/MAXIMIZESTART — 最小化/最大化
    HWINEVENTHOOK hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!hook) return false;

    hook = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
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
    if (g_keyboardHook) return true;  // 已安装

    // 安装低级键盘钩子捕获 Win 键
    // 需要消息循环支持
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
        // 删除注册表项
        result = RegDeleteValue(hKey, L"Dock_WMac");
        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }

    // 写入程序路径
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
    // 枚举所有顶级窗口，匹配进程名
    struct Ctx { int count; QString target; };
    Ctx ctx{0, wmClass.toLower()};

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

        Ctx *pCtx = reinterpret_cast<Ctx *>(lParam);

        // 获取窗口进程名
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
    // 找到匹配的第一个窗口并激活
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
                return FALSE;  // 找到后停止枚举
            }
        }
        CloseHandle(hProcess);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    if (!ctx.hwnd) return false;

    // 激活窗口
    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(ctx.hwnd);
    ShowWindow(ctx.hwnd, SW_RESTORE);
    SetFocus(ctx.hwnd);
    return true;
}

void SysHelper::showWindowPicker()
{
    // 触发 Task View（Windows 10/11）
    // 方法: 发送 Win+Tab
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

    // 使用 DwmEnableBlurBehindWindow 实现毛玻璃效果
    DWM_BLURBEHIND bb;
    ZeroMemory(&bb, sizeof(bb));
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    // 设置模糊区域为整个窗口
    bb.hRgnBlur = nullptr;

    HRESULT hr = DwmEnableBlurBehindWindow(hwnd, &bb);
    if (FAILED(hr)) {
        qWarning() << "DwmEnableBlurBehindWindow 失败，hr:" << Qt::hex << hr;
    }
}

bool SysHelper::isBlurSupported() const
{
    // 检查 DWM 是否启用（Windows Vista+ 均支持）
    BOOL dwmEnabled = FALSE;
    HRESULT hr = DwmIsCompositionEnabled(&dwmEnabled);
    return SUCCEEDED(hr) && dwmEnabled;
}

bool SysHelper::isLightTheme() const
{
    // 读取 Windows 主题设置
    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) return true;  // 默认亮色

    DWORD value = 1;
    DWORD size = sizeof(value);
    result = RegQueryValueEx(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS) ? (value != 0) : true;
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
    if (g_taskbarHandle) {
        ShowWindow(g_taskbarHandle, SW_SHOW);
        g_taskbarHandle = nullptr;
    } else {
        // 尝试重新查找
        HWND hTaskbar = FindWindow(L"Shell_TrayWnd", nullptr);
        if (hTaskbar) ShowWindow(hTaskbar, SW_SHOW);
    }
}
