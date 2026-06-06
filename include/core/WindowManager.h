#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QObject>
#include <QString>

/**
 * @file WindowManager.h
 * @brief Windows 窗口枚举与操作
 *
 * 提供按进程名枚举窗口、计数、激活、系统窗口选择器等功能。
 * 纯 Win32 实现，无 Qt 窗口依赖。
 * 从 SysHelper 分离以遵循单一职责原则。
 */

class WindowManager : public QObject
{
    Q_OBJECT
public:
    explicit WindowManager(QObject *parent = nullptr);

    /** @brief 获取指定进程名（WM_CLASS）的可见窗口数量 */
    int getWindowCount(const QString &wmClass);

    /** @brief 激活指定进程名的第一个可见窗口，返回是否成功 */
    bool activateWindow(const QString &wmClass);

    /** @brief 触发系统窗口选择器（Win+Tab） */
    void showWindowPicker();
};

#endif // WINDOWMANAGER_H
