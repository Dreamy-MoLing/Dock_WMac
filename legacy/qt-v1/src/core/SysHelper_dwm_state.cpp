/**
 * @file SysHelper_dwm_state.cpp
 * @brief SysHelper DWM 状态只读采集实现
 *
 * 三个只读 API：
 * - DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS) — DPI 感知窗口边界
 * - DwmGetWindowAttribute(DWMWA_CLOAKED) — Virtual Desktop cloaked 检测
 * - GetWindowDisplayAffinity — 截图保护状态检测
 */

#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include "core/SysHelper.h"

#include <windows.h>
#include <dwmapi.h>
#include <QDebug>

#pragma comment(lib, "dwmapi.lib")

bool SysHelper::getExtendedFrameBounds(HWND hwnd, RECT &outRect)
{
    if (!hwnd) return false;

    HRESULT hr = DwmGetWindowAttribute(
        hwnd,
        DWMWA_EXTENDED_FRAME_BOUNDS,  // = 9
        &outRect,
        sizeof(outRect));

    if (SUCCEEDED(hr)) {
        return true;
    }

    // Fallback: GetWindowRect（逻辑像素，DPI 下可能不准）
    return GetWindowRect(hwnd, &outRect);
}

bool SysHelper::isWindowCloaked(HWND hwnd)
{
    if (!hwnd) return false;

    DWORD cloaked = 0;
    HRESULT hr = DwmGetWindowAttribute(
        hwnd,
        DWMWA_CLOAKED,  // = 14, Windows 8+
        &cloaked,
        sizeof(cloaked));

    if (FAILED(hr)) {
        // Windows 7: DWMWA_CLOAKED 不支持，视为非 cloaked
        return false;
    }

    return cloaked != 0;
}

bool SysHelper::getWindowDisplayAffinity(HWND hwnd, DWORD &outAffinity)
{
    if (!hwnd) {
        outAffinity = 0;  // WDA_NONE
        return false;
    }

    BOOL result = GetWindowDisplayAffinity(hwnd, &outAffinity);
    if (!result) {
        outAffinity = 0;  // WDA_NONE — 假定无保护
        return false;
    }

    return true;
}
