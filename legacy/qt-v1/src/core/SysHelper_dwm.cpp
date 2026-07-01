/**
 * @file SysHelper_dwm.cpp
 * @brief SysHelper DWM 模糊效果实现
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

    DeleteObject(hRgn);
}

bool SysHelper::isBlurSupported() const
{
    BOOL dwmEnabled = FALSE;
    HRESULT hr = DwmIsCompositionEnabled(&dwmEnabled);
    return SUCCEEDED(hr) && dwmEnabled;
}
