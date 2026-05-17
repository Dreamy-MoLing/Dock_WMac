/**
 * @file SysHelper_win.cpp
 * @brief Windows 平台系统适配实现
 *
 * 使用 Win32 API、COM 接口实现任务栏固定项读取、窗口钩子与键盘钩子。
 */

#include "core/SysHelper.h"

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <QDir>
#include <QFileInfo>

QList<DockItemData> SysHelper::getPinnedItems()
{
    QList<DockItemData> items;
    // 遍历任务栏固定目录: %AppData%/.../TaskBar/*.lnk
    QString taskbarPath = QDir::homePath()
        + "/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/";
    QDir dir(taskbarPath);
    if (!dir.exists()) return items;

    // 后续实现: 读取 .lnk → IShellLink 解析 → 提取 AppUserModelID/路径/图标
    return items;
}

QString SysHelper::extractAppIcon(const QString &appId)
{
    // 后续实现: SHGetFileInfo 提取图标，优先返回高清缓存路径
    Q_UNUSED(appId);
    return {};
}

bool SysHelper::installWindowHook()
{
    // 后续实现: SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, ...)
    return false;
}

bool SysHelper::installKeyboardHook()
{
    // 后续实现: SetWindowsHookEx(WH_KEYBOARD_LL, ...)
    return false;
}

void SysHelper::uninstallKeyboardHook()
{
    // 后续实现: UnhookWindowsHookEx
}

bool SysHelper::getForegroundWindowState()
{
    // 后续实现: GetForegroundWindow → GetWindowPlacement → 判断最大化
    // 全屏判断: 窗口尺寸 == 屏幕尺寸
    return false;
}

bool SysHelper::setAutoStart(bool enabled)
{
    // Windows: 写入注册表 HKCU\Software\Microsoft\Windows\CurrentVersion\Run
    Q_UNUSED(enabled);
    return false;
}

bool SysHelper::isAutoStartEnabled() const
{
    return false;
}
