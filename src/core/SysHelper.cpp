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
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

// ─── 全局钩子句柄（定义，extern 声明在 SysHelper_winhooks.cpp）───

HHOOK g_keyboardHook = nullptr;
SysHelper *g_sysHelperForHook = nullptr;

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

static HWND g_taskbarHandle = nullptr;

void SysHelper::hideNativeTaskbar()
{
#ifdef DOCK_WMAC_TESTING
    return;
#else
    HWND hTaskbar = FindWindow(L"Shell_TrayWnd", nullptr);
    if (hTaskbar) {
        g_taskbarHandle = hTaskbar;
        ShowWindow(hTaskbar, SW_HIDE);
    }
    HWND hSecondary = FindWindow(L"Shell_SecondaryTrayWnd", nullptr);
    if (hSecondary) {
        ShowWindow(hSecondary, SW_HIDE);
    }
#endif
}

void SysHelper::restoreNativeTaskbar()
{
#ifdef DOCK_WMAC_TESTING
    return;
#else
    HWND hTaskbar = g_taskbarHandle;
    if (!hTaskbar) {
        hTaskbar = FindWindow(L"Shell_TrayWnd", nullptr);
    }
    if (hTaskbar) {
        SetWindowPos(hTaskbar, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        g_taskbarHandle = nullptr;
    }
    HWND hSecondary = FindWindow(L"Shell_SecondaryTrayWnd", nullptr);
    if (hSecondary) {
        SetWindowPos(hSecondary, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
#endif
}

QString SysHelper::resolveShortcut(const QString &lnkPath)
{
    if (!QFileInfo::exists(lnkPath)) return {};

#ifdef Q_OS_WIN
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return {};

    IShellLink *psl = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IShellLink, reinterpret_cast<void **>(&psl));
    if (FAILED(hr)) {
        if (hr != RPC_E_CHANGED_MODE) CoUninitialize();
        return {};
    }

    IPersistFile *ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&ppf));
    if (FAILED(hr)) {
        psl->Release();
        if (hr != RPC_E_CHANGED_MODE) CoUninitialize();
        return {};
    }

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
    if (hr != RPC_E_CHANGED_MODE) CoUninitialize();
    return execPath;
#else
    Q_UNUSED(lnkPath);
    return {};
#endif
}
