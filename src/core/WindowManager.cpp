/**
 * @file WindowManager.cpp
 * @brief Windows 窗口枚举与操作实现
 *
 * 从 SysHelper 迁移 Win32 EnumWindows + 窗口激活逻辑。
 */
#include "core/WindowManager.h"

#include <windows.h>
#include <QFileInfo>
#include <QDebug>

#pragma comment(lib, "user32.lib")

WindowManager::WindowManager(QObject *parent)
    : QObject(parent)
{
}

int WindowManager::getWindowCount(const QString &wmClass)
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

bool WindowManager::activateWindow(const QString &wmClass)
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

void WindowManager::showWindowPicker()
{
    // 触发 Task View（Windows 10/11）
    keybd_event(VK_LWIN, 0, 0, 0);
    keybd_event(VK_TAB, 0, 0, 0);
    keybd_event(VK_TAB, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
}
