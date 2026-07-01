#ifndef OVERFLOWPANEL_H
#define OVERFLOWPANEL_H

#include <QObject>
#include <QWidget>

class DockManager;
class SysHelper;
class DockItem;
class WindowCache;

/**
 * @file OverflowPanel.h
 * @brief Dock 溢出弹出面板
 *
 * 仅管理溢出弹出菜单的创建/显示/销毁。
 * 溢出图标（"..." DockItem）保留在 DockWindow 中管理。
 */

class OverflowPanel : public QObject
{
    Q_OBJECT
public:
    explicit OverflowPanel(QObject *parent = nullptr);

    void setDockManager(DockManager *manager);
    void setSysHelper(SysHelper *helper);
    void setWindowCache(WindowCache *cache);

    /** @brief 在溢出图标上方弹出菜单 */
    void showPopup(DockItem *anchorItem, QWidget *dockParent);

    /** @brief 关闭弹出菜单 */
    void hidePopup();

private:
    DockManager *m_dockManager = nullptr;
    SysHelper *m_sysHelper = nullptr;
    WindowCache *m_windowCache = nullptr;
    QWidget *m_popup = nullptr;
};

#endif // OVERFLOWPANEL_H
