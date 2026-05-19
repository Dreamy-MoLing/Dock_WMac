#ifndef DOCKMANAGER_H
#define DOCKMANAGER_H

#include <QObject>
#include <QList>
#include <QStringList>
#include "Types.h"

class SysHelper;

/**
 * @file DockManager.h
 * @brief Dock 状态机与业务逻辑调度
 *
 * 管理 Dock 三种状态 (Docked/Hidden/Animating) 之间的转换，
 * 协调 SysHelper 的事件输入与 UI 层的渲染输出。
 *
 * 支持固定项（pinned，持久化保存）和临时项（transient，自动跟随运行应用出现/消失）。
 */

class DockManager : public QObject {
    Q_OBJECT
public:
    explicit DockManager(QObject *parent = nullptr);

    DockState currentState() const;
    QList<DockItemData> items() const;
    QList<DockItemData> pinnedItems() const;
    QList<DockItemData> transientItems() const;

    /** @brief 初始化：关联 SysHelper 并连接信号，加载固定项列表 */
    void initialize(SysHelper *sysHelper);

    /** @brief 设置固定项列表（来自配置持久化） */
    void setPinnedItems(const QList<DockItemData> &items);

    /** @brief 添加临时项（运行中的应用自动加入） */
    void addTransientItem(const DockItemData &item);

    /** @brief 移除临时项（应用退出时） */
    void removeTransientItem(const QString &appId);

    /** @brief 将指定应用固定到 Dock */
    void pinItem(const QString &appId);

    /** @brief 从 Dock 移除固定项 */
    void unpinItem(const QString &appId);

    /** @brief 检查应用是否为固定项 */
    bool isPinned(const QString &appId) const;

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
    /** @brief 固定项列表变更（用于通知 ConfigManager 持久化） */
    void pinnedItemsChanged(const QList<DockItemData> &items);

private:
    DockState m_currentState;
    QList<DockItemData> m_pinnedItems;
    QList<DockItemData> m_transientItems;
    SysHelper *m_sysHelper;
};

#endif // DOCKMANAGER_H
