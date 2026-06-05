#ifndef DOCKWINDOW_H
#define DOCKWINDOW_H

#include <QWidget>
#include <QList>
#include <QMap>
#include <QScreen>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include "core/Types.h"

class DockItem;
class DockManager;
class SysHelper;

/**
 * @file DockWindow.h
 * @brief Dock 主窗口
 *
 * 无边框、透明背景、置顶显示的 Dock 主窗体。
 * 使用手动定位管理 DockItem，实现 macOS 风格变量间隙鱼眼动画。
 * 通过 DockManager 信号驱动 UI 更新，ProcessMonitor 驱动进程状态检测。
 */

class DockWindow : public QWidget {
    Q_OBJECT
public:
    explicit DockWindow(QWidget *parent = nullptr);

    /** @brief 关联 DockManager，连接信号 */
    void setDockManager(DockManager *manager);

    /** @brief 关联 SysHelper（窗口管理用） */
    void setSysHelper(SysHelper *helper);

    /** @brief 设置目标显示器编号（从 0 开始，-1 为鼠标当前所在屏幕） */
    void setMonitor(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onItemAdded(const DockItemData &data);
    void onItemRemoved(const QString &appId);
    void onItemStateChanged(const QString &appId, bool isRunning);
    void onStateChanged(DockState newState);
    void onOverflowChanged();
    void onItemWindowCountChanged(const QString &appId, int count);
    void updateWindowCounts();

    /** @brief 查询指定应用是否为固定项（DockItem 右键菜单通过 QMetaObject 调用） */
    QVariant isItemPinned(QVariant appId);

public slots:
    /** @brief 延迟重新定位 Dock（任务栏隐藏后可用区域变化时调用） */
    void requestUpdatePosition();

    /** @brief ProcessMonitor 信号：应用运行状态变化 */
    void onAppRunningStateChanged(const QString &appId, bool isRunning);

    /** @brief ProcessMonitor 信号：检测到新运行应用 */
    void onNewRunningAppDetected(const DockItemData &item);

    /** @brief ProcessMonitor 信号：运行应用退出 */
    void onRunningAppExited(const QString &appId);

private:
    void updatePosition();
    void updateDpiScale();
    void applyFishEyeEffect(int hoveredIndex);
    void resetFishEyeEffect();
    void launchApp(DockItem *item);
    void handleSingleClick(DockItem *item);
    void handleDoubleClick(DockItem *item);
    void animateItemToScale(DockItem *item, qreal targetScale);
    int  itemAtPos(int mouseX, int mouseY) const;
    void relayoutItems();

    // 毛玻璃 & 主题
    void initBlurEffect();
    void updateTheme();

    // 图标添加/移除动画
    void animateItemAdd(DockItem *item);
    void animateItemRemove(DockItem *item, const QString &appId);

    // 抽屉图标管理
    void updateOverflowItem();
    void showOverflowPopup();

    QList<DockItem *> m_items;
    QMap<QString, DockItem *> m_itemMap;
    DockManager *m_dockManager;
    SysHelper *m_sysHelper;
    int m_baseIconSize;
    int m_fixedWindowH;
    qreal m_opacity;
    bool m_isHidden;
    int m_monitorIndex;
    int m_hoveredIndex;

    // 鱼眼动画
    QMap<DockItem *, QPropertyAnimation *> m_fishEyeAnims;

    // 显示/隐藏动画
    QPropertyAnimation *m_slideAnim;
    QGraphicsOpacityEffect *m_opacityEffect;

    // 毛玻璃 & 主题
    bool m_blurInitialized;
    bool m_isLightTheme;
    QTimer *m_themeTimer;

    // 单双击检测
    QTimer *m_clickTimer;
    DockItem *m_pendingClickItem;

    // 抽屉图标（溢出折叠）
    DockItem *m_overflowItem;
    QWidget *m_overflowPopup;

    // 窗口数量更新定时器
    QTimer *m_windowCountTimer;

    // 布局常量
    static constexpr int kBaseSpacing = 8;
    static constexpr int kExtraSpacing = 18;
    static constexpr int kMarginH = 14;
    static constexpr int kMarginTop = 8;
    static constexpr int kMarginBottom = 10;
};

#endif // DOCKWINDOW_H
