#ifndef DOCKMANAGER_H
#define DOCKMANAGER_H

#include <QObject>
#include <QList>
#include <QTimer>
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
 * 显隐逻辑：
 * - 常驻 (Docked)：dock 所在屏幕无全屏/最大化窗口 → 默认置顶可见
 * - 隐藏 (Hidden)：全屏持续 3 秒后确认隐藏，非短暂弹窗
 * - 唤醒：Win 键 / 鼠标触底 → 立即显示 + 1.5s 冷却防止开始菜单冲突
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

    /** @brief 预览面板激活期间阻止 Dock 隐藏 */
    void setPreviewActive(bool active);

    /** @brief 设置 Dock 所在屏幕索引（-1 = 主屏幕，用于全屏检测） */
    void setMonitorIndex(int index);
    int monitorIndex() const { return m_monitorIndex; }

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

    /** @brief 按 UI 顺序重排固定项并触发持久化 */
    bool reorderPinnedItems(const QStringList &orderedAppIds);

    /** @brief 检查应用是否为固定项 */
    bool isPinned(const QString &appId) const;

    /** @brief 更新指定应用的窗口数量 */
    void updateWindowCount(const QString &appId, int count);

    // ── 仅供测试使用的状态机内部接口 ────────────────────────
    // 这些方法不应在生产代码中直接调用，它们由 onHideDelayTimeout() /
    // onWinKeyPressed() / onFullscreenStateChanged() 内部驱动。

    /** @brief 执行 Docked → Hidden 转换（安装键盘钩子、发射信号） */
    void enterHiddenState();

    /** @brief 执行 Hidden → Docked 转换（卸载键盘钩子、发射信号） */
    void enterDockedState();

    /** @brief 测试：获取隐藏延迟定时器指针 */
    QTimer *hideDelayTimer() const { return m_hideDelayTimer; }

    /** @brief 测试：获取 Win 键冷却定时器指针 */
    QTimer *winKeyCooldownTimer() const { return m_winKeyCooldownTimer; }

public slots:
    /** @brief 处理屏幕最大化/全屏状态变化（信号来自 SysHelper） */
    void onFullscreenStateChanged(bool anyMaximizedOnAnyScreen);

    /** @brief 处理 Win 键按下事件（来自键盘钩子或鼠标触底） */
    void onWinKeyPressed();

private slots:
    void onHideDelayTimeout();
    void onWinKeyCooldownTimeout();

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

    // 屏幕感知
    int m_monitorIndex = -1;

    // 隐藏延迟（3s — 全屏确认非短暂弹窗后才隐藏）
    QTimer *m_hideDelayTimer = nullptr;
    static constexpr int kHideDelayMs = 3000;

    // Win 键冷却（1.5s — 防止开始菜单弹出后全屏检测把 dock 又按回去）
    QTimer *m_winKeyCooldownTimer = nullptr;
    static constexpr int kWinKeyCooldownMs = 1500;

    // 预览激活标志（阻止预览期间 Dock 被全屏检测隐藏）
    bool m_previewActive = false;
};

#endif // DOCKMANAGER_H

