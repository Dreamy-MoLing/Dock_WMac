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
 * 管理 Dock 两种状态 (Docked/Hidden) 之间的转换，
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

    /** @brief 可见项列表（固定项全部 + 截断后的临时项，受 maxItems 限制） */
    QList<DockItemData> visibleItems() const;

    /** @brief 被折叠的临时项（超出 maxItems 部分） */
    QList<DockItemData> overflowItems() const;

    /** @brief 设置最大可见图标数 */
    void setMaxItems(int max);

    /** @brief 获取最大可见图标数 */
    int maxItems() const;

    /** @brief 初始化：关联 SysHelper 并连接信号，加载固定项列表 */
    void initialize(SysHelper *sysHelper);

    /** @brief 设置固定项列表（来自配置持久化） */
    void setPinnedItems(const QList<DockItemData> &items);

    /** @brief 添加临时项（运行中的应用自动加入） */
    void addTransientItem(const DockItemData &item);

    /** @brief 移除临时项（应用退出时） */
    void removeTransientItem(const QString &appId);

    /** @brief 将指定应用固定到 Dock（调用方提供完整数据） */
    void pinItem(const DockItemData &item);

    /** @brief 从 Dock 移除固定项 */
    void unpinItem(const QString &appId);

    /** @brief 检查应用是否为固定项 */
    bool isPinned(const QString &appId) const;

    /** @brief 更新指定应用的窗口数量 */
    void updateWindowCount(const QString &appId, int count);

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
    /** @brief 被折叠项列表变更（通知 UI 更新抽屉图标） */
    void overflowChanged();
    /** @brief 应用窗口数量变更 */
    void itemWindowCountChanged(const QString &appId, int count);

private:
    DockState m_currentState;
    QList<DockItemData> m_pinnedItems;
    QList<DockItemData> m_transientItems;
    SysHelper *m_sysHelper;
    int m_maxItems = 16;
};

#endif // DOCKMANAGER_H
