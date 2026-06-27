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
 * 通过 visualScale 属性实现鱼眼动画，以底部中心为锚点放大。
 */

class DockItem : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal visualScale READ visualScale WRITE setVisualScale)
public:
    explicit DockItem(const QString &appId, const QString &iconPath,
                      const QString &displayName, QWidget *parent = nullptr);

    void setBaseSize(int size) { m_baseSize = size; }
    void setRunning(bool running);
    bool isRunning() const { return m_isRunning; }
    void setBadgeCount(int count);
    void setWindowCount(int count);
    int windowCount() const { return m_windowCount; }
    void setForegroundActive(bool active);
    bool isForegroundActive() const { return m_isForegroundActive; }
    void setHasNotifications(bool has);
    void triggerInteractionIndicator();

    QString appId() const { return m_appId; }
    QString displayName() const { return m_displayName; }
    QString execPath() const { return m_execPath; }
    void setExecPath(const QString &path) { m_execPath = path; }
    QString targetPath() const { return m_targetPath; }
    void setTargetPath(const QString &path) { m_targetPath = path; }
    QString appUserModelId() const { return m_appUserModelId; }
    void setAppUserModelId(const QString &id) { m_appUserModelId = id; }

    /** @brief 视觉缩放比例（1.0 = 原始大小，动画驱动） */
    qreal visualScale() const { return m_visualScale; }
    void setVisualScale(qreal scale);

    /** @brief 基础图标尺寸（未缩放） */
    int baseSize() const { return m_baseSize; }

signals:
    void clicked(const QString &appId);
    void rightClicked(const QString &appId);
    /** @brief 固定/取消固定请求 */
    void pinRequested(const QString &appId, bool pin);
    /** @brief 鼠标进入/离开，用于鱼眼效果 */
    void hoverEntered(int index);
    void hoverLeft();

private slots:
    void onFlashTimerTick();
    void onFadeTimerTick();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QString m_appId;
    QString m_displayName;
    QString m_execPath;
    QString m_targetPath;
    QString m_appUserModelId;
    QPixmap m_icon;
    bool    m_isRunning;
    int     m_badgeCount;
    int     m_baseSize = 48;
    int     m_windowCount;          // 窗口数量（>1 时绘制堆叠效果）
    bool    m_isForegroundActive = false;  // 前台激活指示器
    bool    m_isHovered;
    qreal   m_visualScale;   // 绘制缩放比（1.0 = 原始大小）

    // 通知闪烁
    bool    m_hasNotifications = false;
    QTimer *m_flashTimer = nullptr;
    bool    m_flashVisible = true;

    // 交互后渐隐
    QTimer *m_fadeTimer = nullptr;
    qreal   m_statusOpacity = 0.0;
    static constexpr int kSolidDelay = 3000;
    static constexpr int kFadeDuration = 1000;
    static constexpr int kFadeInterval = 50;

    QPoint  m_dragStartPos;
};

#endif // DOCKITEM_H
