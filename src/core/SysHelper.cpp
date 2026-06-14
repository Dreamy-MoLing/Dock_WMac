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
#include <QDebug>
#include <QTimer>
#include <QCoreApplication>
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
