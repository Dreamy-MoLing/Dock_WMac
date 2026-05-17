#ifndef DOCKITEM_H
#define DOCKITEM_H

#include <QWidget>
#include <QPixmap>
#include <QDrag>

/**
 * @file DockItem.h
 * @brief 单个 Dock 图标组件
 *
 * 负责图标绘制、鼠标悬浮/点击事件响应及状态显示（运行指示灯、未读徽章）。
 * 左键点击启动/切换应用，右键弹出上下文菜单。
 */

class DockItem : public QWidget {
    Q_OBJECT
public:
    explicit DockItem(const QString &appId, const QString &iconPath,
                      const QString &displayName, QWidget *parent = nullptr);

    void setRunning(bool running);
    void setBadgeCount(int count);

    QString appId() const { return m_appId; }
    QString displayName() const { return m_displayName; }
    QString execPath() const { return m_execPath; }
    void setExecPath(const QString &path) { m_execPath = path; }

    /** @brief 设置缩放比例（鱼眼效果用） */
    void setScaleFactor(qreal factor);

signals:
    void clicked(const QString &appId);
    void rightClicked(const QString &appId);
    /** @brief 鼠标进入/离开，用于鱼眼效果 */
    void hoverEntered(int index);
    void hoverLeft();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QString m_appId;
    QString m_displayName;
    QString m_execPath;
    QPixmap m_icon;
    bool    m_isRunning;
    int     m_badgeCount;
    bool    m_isHovered;
    qreal   m_scaleFactor;
    QPoint  m_dragStartPos;
};

#endif // DOCKITEM_H
