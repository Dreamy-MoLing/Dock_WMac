#ifndef DOCKMANAGER_H
#define DOCKMANAGER_H

#include <QObject>
#include <QList>
#include "Types.h"

class SysHelper;

/**
 * @file DockManager.h
 * @brief Dock 状态机与业务逻辑调度
 *
 * 管理 Dock 三种状态 (Docked/Hidden/Animating) 之间的转换，
 * 协调 SysHelper 的事件输入与 UI 层的渲染输出。
 * 内部通过 Qt 信号通知 UI 层更新。
 */

class DockManager : public QObject {
    Q_OBJECT
public:
    explicit DockManager(QObject *parent = nullptr);

    DockState currentState() const;
    QList<DockItemData> items() const;

    /** @brief 初始化：关联 SysHelper 并连接信号 */
    void initialize(SysHelper *sysHelper);

public slots:
    /** @brief 处理前台窗口状态变化 */
    void onForegroundWindowChanged(bool isMaximizedOrFullscreen);

    /** @brief 处理 Win 键按下事件 */
    void onWinKeyPressed();

signals:
    void stateChanged(DockState newState);
    void itemAdded(const DockItemData &item);
    void itemRemoved(const QString &appId);
    void itemStateChanged(const QString &appId, bool isRunning);

private:
    DockState m_currentState;
    QList<DockItemData> m_items;
    SysHelper *m_sysHelper;
};

#endif // DOCKMANAGER_H
