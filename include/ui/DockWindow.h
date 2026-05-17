#ifndef DOCKWINDOW_H
#define DOCKWINDOW_H

#include <QWidget>
#include <QHBoxLayout>
#include <QMap>
#include "core/Types.h"

class DockItem;
class DockManager;
class AnimationHandler;

/**
 * @file DockWindow.h
 * @brief Dock 主窗口
 *
 * 无边框、透明背景、置顶显示的 Dock 主窗体。
 * 使用 QPainter 绘制 Dock 背景，QHBoxLayout 管理 DockItem 布局。
 * 通过 DockManager 信号驱动 UI 更新。
 */

class DockWindow : public QWidget {
    Q_OBJECT
public:
    explicit DockWindow(QWidget *parent = nullptr);

    /** @brief 关联 DockManager，连接信号 */
    void setDockManager(DockManager *manager);

    /** @brief 添加一个 DockItem 到布局中 */
    void addItem(DockItem *item);

    /** @brief 从布局中移除 DockItem */
    void removeItem(DockItem *item);

    /** @brief 清空所有 DockItem */
    void clearItems();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onItemAdded(const DockItemData &data);
    void onItemRemoved(const QString &appId);
    void onItemStateChanged(const QString &appId, bool isRunning);
    void onStateChanged(DockState newState);

private:
    void applyFishEyeEffect(int hoveredIndex);
    void resetFishEyeEffect();
    void updatePosition();

    QHBoxLayout *m_layout;
    QMap<QString, DockItem *> m_itemMap;
    AnimationHandler *m_animationHandler;
    DockManager *m_dockManager;
    int m_iconSize;
    qreal m_opacity;
    bool m_isHidden;
};

#endif // DOCKWINDOW_H
