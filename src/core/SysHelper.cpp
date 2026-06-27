/**
 * @file SysHelper.cpp
 * @brief SysHelper 核心实现 — 构造、注册表、任务栏、主题
 *
 * 钩子/全屏检测 → SysHelper_winhooks.cpp
 * DWM 模糊 → SysHelper_dwm.cpp
 */

#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include "core/SysHelper.h"

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <QDebug>
#include <QTimer>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

// ─── 全局钩子句柄（定义，extern 声明在 SysHelper_winhooks.cpp）───

HHOOK g_keyboardHook = nullptr;
SysHelper *g_sysHelperForHook = nullptr;
bool g_nativeTaskbarHidden = false;

// ─── 构造函数 ──────────────────────────────────────────────

SysHelper::SysHelper(QObject *parent)
    : QObject(parent)
{
    m_fullscreenDebounceTimer = new QTimer(this);
    m_fullscreenDebounceTimer->setSingleShot(true);
    m_fullscreenDebounceTimer->setInterval(200);
    connect(m_fullscreenDebounceTimer, &QTimer::timeout, this, [this]() {
        bool anyMax = hasMaximizedOrFullscreenWindowOnMonitor();
        emit fullscreenStateChanged(anyMax);
    });
}

// ─── 注册表：开机自启 ──────────────────────────────────────

bool SysHelper::setAutoStart(bool enabled)
{
#ifdef DOCK_WMAC_TESTING
    Q_UNUSED(enabled);
    return false;
#else
    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) return false;

    if (!enabled) {
        result = RegDeleteValue(hKey, L"Dock_WMac");
        RegCloseKey(hKey);
        return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
    }

    const QString command = QStringLiteral("\"%1\"")
        .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    const std::wstring wCommand = command.toStdWString();

    result = RegSetValueEx(
        hKey, L"Dock_WMac", 0, REG_SZ,
        reinterpret_cast<const BYTE *>(wCommand.c_str()),
        static_cast<DWORD>((wCommand.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
#endif
}

bool SysHelper::isAutoStartEnabled() const
{
#ifdef DOCK_WMAC_TESTING
    return false;
#else
    HKEY hKey;
    LONG result = RegOpenKeyEx(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD size = 0;
    result = RegQueryValueEx(hKey, L"Dock_WMac", nullptr, &type, nullptr, &size);
    if (result != ERROR_SUCCESS || type != REG_SZ || size < sizeof(wchar_t)) {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<wchar_t> value(size / sizeof(wchar_t), L'\0');
    result = RegQueryValueEx(hKey, L"Dock_WMac", nullptr, &type,
                             reinterpret_cast<LPBYTE>(value.data()), &size);
    RegCloseKey(hKey);
    if (result != ERROR_SUCCESS) return false;

    const QString actual = QString::fromWCharArray(value.data()).trimmed();
    const QString expected = QStringLiteral("\"%1\"")
        .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    return actual.compare(expected, Qt::CaseInsensitive) == 0;
#endif
}

// ─── 主题检测 ──────────────────────────────────────────────

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

    return (size > 12) && (data[12] & 0x01);
}

// ─── 鼠标位置 ──────────────────────────────────────────────

QPoint SysHelper::cursorPos() const
{
    POINT pt;
    if (GetCursorPos(&pt)) {
        return QPoint(pt.x, pt.y);
    }
    return QPoint();
}

// ─── 原生任务栏管理 ─────────────────────────────────────────

#ifndef DOCK_WMAC_TESTING
static bool isTaskbarClass(HWND hwnd)
{
    wchar_t className[256] = {};
    if (!GetClassNameW(hwnd, className, 255)) return false;
    return wcscmp(className, L"Shell_TrayWnd") == 0
        || wcscmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

static std::vector<HWND> enumerateTaskbars()
{
    std::vector<HWND> taskbars;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto *items = reinterpret_cast<std::vector<HWND> *>(lParam);
        if (isTaskbarClass(hwnd))
            items->push_back(hwnd);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&taskbars));
    return taskbars;
}
#endif

void SysHelper::hideNativeTaskbar()
{
#ifdef DOCK_WMAC_TESTING
    return;
#else
    g_nativeTaskbarHidden = true;
    for (HWND hwnd : enumerateTaskbars()) {
        if (IsWindow(hwnd))
            ShowWindow(hwnd, SW_HIDE);
    }
#endif
}

void SysHelper::restoreNativeTaskbar()
{
#ifdef DOCK_WMAC_TESTING
    return;
#else
    g_nativeTaskbarHidden = false;
    for (HWND hwnd : enumerateTaskbars()) {
        if (IsWindow(hwnd)) {
            ShowWindow(hwnd, SW_SHOW);
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
    }
#endif
}
QString SysHelper::resolveShortcut(const QString &lnkPath)
{
    if (!QFileInfo::exists(lnkPath)) return {};

#ifdef Q_OS_WIN
    HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool didInitCom = SUCCEEDED(initHr);
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE) return {};

    IShellLink *psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLink, reinterpret_cast<void **>(&psl));
    if (FAILED(hr)) {
        if (didInitCom) CoUninitialize();
        return {};
    }

    IPersistFile *ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&ppf));
    if (FAILED(hr)) {
        psl->Release();
        if (didInitCom) CoUninitialize();
        return {};
    }

    const std::wstring path = QDir::toNativeSeparators(lnkPath).toStdWString();
    hr = ppf->Load(path.c_str(), STGM_READ);

    QString execPath;
    if (SUCCEEDED(hr)) {
        WIN32_FIND_DATA wfd;
        WCHAR szPath[MAX_PATH] = {};
        hr = psl->GetPath(szPath, MAX_PATH, &wfd, SLGP_RAWPATH);
        if (SUCCEEDED(hr)) {
            execPath = QString::fromWCharArray(szPath);
        }
    }

    ppf->Release();
    psl->Release();
    if (didInitCom) CoUninitialize();
    return execPath;
#else
    Q_UNUSED(lnkPath);
    return {};
#endif
}

bool SysHelper::launchPath(const QString &path, const QStringList &arguments)
{
    if (path.trimmed().isEmpty()) return false;
#ifdef Q_OS_WIN
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const std::wstring params = arguments.join(QLatin1Char(' ')).toStdWString();
    HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        nativePath.c_str(),
        params.empty() ? nullptr : params.c_str(),
        nullptr,
        SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
#else
    return QProcess::startDetached(path, arguments);
#endif
}