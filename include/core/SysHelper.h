#ifndef SYSHELPER_H
#define SYSHELPER_H

#include <QObject>
#include <QList>
#include "Types.h"

/**
 * @file SysHelper.h
 * @brief 系统适配层抽象接口
 *
 * 封装所有底层系统交互，包括任务栏固定项读取、窗口状态监听、全局热键等。
 * 平台差异实现在 SysHelper_win.cpp / SysHelper_linux.cpp 中。
 */

class SysHelper : public QObject {
    Q_OBJECT
public:
    explicit SysHelper(QObject *parent = nullptr);

    /** @brief 读取系统任务栏/面板固定项列表 */
    QList<DockItemData> getPinnedItems();

    /** @brief 提取应用图标（优先缓存，回退系统原生图标） */
    QString extractAppIcon(const QString &appId);

    /** @brief 注册全局窗口钩子，监听前台窗口状态变化 */
    bool installWindowHook();

    /** @brief 注册低级键盘钩子，捕获 Win 键（仅 Hidden 状态启用） */
    bool installKeyboardHook();

    /** @brief 移除键盘钩子 */
    void uninstallKeyboardHook();

    /** @brief 返回当前前台窗口是否最大化或全屏 */
    bool getForegroundWindowState();

signals:
    /** @brief 前台窗口状态变化 signal */
    void foregroundWindowChanged(bool isMaximizedOrFullscreen);

    /** @brief Win 键被按下 signal */
    void winKeyPressed();
};

#endif // SYSHELPER_H
