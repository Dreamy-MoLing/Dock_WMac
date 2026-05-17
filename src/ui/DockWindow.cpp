/**
 * @file DockWindow.cpp
 * @brief Dock 主窗口实现
 *
 * 无边框、半透明背景、置顶显示的 Dock 主窗体。
 * 使用 QPainter 绘制底部 Dock 背景条，负责管理 DockItem 的布局。
 * 通过 DockManager 信号驱动图标增删和状态切换。
 */

#include "ui/DockWindow.h"
#include "ui/DockItem.h"
#include "ui/AnimationHandler.h"
#include "core/DockManager.h"
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QProcess>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

DockWindow::DockWindow(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_animationHandler(new AnimationHandler(this))
    , m_dockManager(nullptr)
    , m_iconSize(48)
    , m_opacity(0.95)
    , m_isHidden(false)
{
    // 窗口属性：无边框、透明背景、置顶
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAcceptDrops(true);

    // 水平布局管理 DockItem
    m_layout = new QHBoxLayout(this);
    m_layout->setSpacing(6);
    m_layout->setContentsMargins(12, 6, 12, 8);
    m_layout->setAlignment(Qt::AlignCenter);

    updatePosition();
}

void DockWindow::setDockManager(DockManager *manager)
{
    m_dockManager = manager;

    // 连接 DockManager 信号到 UI 槽
    connect(manager, &DockManager::itemAdded,
            this, &DockWindow::onItemAdded);
    connect(manager, &DockManager::itemRemoved,
            this, &DockWindow::onItemRemoved);
    connect(manager, &DockManager::itemStateChanged,
            this, &DockWindow::onItemStateChanged);
    connect(manager, &DockManager::stateChanged,
            this, &DockWindow::onStateChanged);
}

void DockWindow::updatePosition()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    int screenW = screen->availableSize().width();
    int screenH = screen->availableSize().height();
    int dockH = m_iconSize + 24;
    int dockW = qMin(screenW - 40, 800);

    setFixedHeight(dockH);
    setFixedWidth(dockW);
    move((screenW - dockW) / 2, screenH - dockH - 10);
}

void DockWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 半透明深色背景，圆角矩形
    QColor bgColor(48, 48, 48, static_cast<int>(m_opacity * 255));
    painter.setBrush(bgColor);
    painter.setPen(QPen(QColor(80, 80, 80, 100), 1));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 14, 14);
}

void DockWindow::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
    // 鼠标进入 Dock 区域，如果是隐藏状态则弹出
    if (m_isHidden && m_dockManager) {
        m_dockManager->onWinKeyPressed();
    }
}

void DockWindow::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    // 鼠标离开时重置鱼眼效果
    resetFishEyeEffect();
}

void DockWindow::addItem(DockItem *item)
{
    m_layout->addWidget(item);

    // 连接鱼眼信号
    connect(item, &DockItem::hoverEntered, this, [this, item](int) {
        int index = m_layout->indexOf(item);
        applyFishEyeEffect(index);
    });
    connect(item, &DockItem::hoverLeft, this, [this]() {
        resetFishEyeEffect();
    });

    // 连接点击启动
    connect(item, &DockItem::clicked, this, [this, item](const QString &appId) {
        Q_UNUSED(appId);
        if (item && !item->execPath().isEmpty()) {
            QStringList parts = item->execPath().split(' ');
            QString program = parts.takeFirst();
            QProcess::startDetached(program, parts);
        }
    });
}

void DockWindow::removeItem(DockItem *item)
{
    m_layout->removeWidget(item);
    item->deleteLater();
}

void DockWindow::clearItems()
{
    QLayoutItem *child;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
    m_itemMap.clear();
}

void DockWindow::onItemAdded(const DockItemData &data)
{
    if (m_itemMap.contains(data.appId)) return;

    DockItem *item = new DockItem(data.appId, data.iconPath, data.displayName, this);
    item->setExecPath(data.execPath);
    item->setRunning(data.isRunning);
    item->setBadgeCount(data.badgeCount);

    m_itemMap[data.appId] = item;
    addItem(item);
}

void DockWindow::onItemRemoved(const QString &appId)
{
    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;

    DockItem *item = it.value();
    m_itemMap.erase(it);
    removeItem(item);
}

void DockWindow::onItemStateChanged(const QString &appId, bool isRunning)
{
    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;

    it.value()->setRunning(isRunning);
}

void DockWindow::onStateChanged(DockState newState)
{
    switch (newState) {
    case DockState::Docked: {
        m_isHidden = false;
        show();
        // 弹出动画
        QPropertyAnimation *anim = m_animationHandler->createShowAnimation(
            this, "windowOpacity", 1.0, 200, QEasingCurve::OutBack);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        break;
    }
    case DockState::Hidden: {
        m_isHidden = true;
        // 隐藏动画
        QPropertyAnimation *anim = m_animationHandler->createHideAnimation(
            this, "windowOpacity", 0.0, 200, QEasingCurve::OutQuad);
        connect(anim, &QPropertyAnimation::finished, this, [this]() {
            hide();
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        break;
    }
    case DockState::Animating:
        // 过渡中，不额外处理
        break;
    }
}

void DockWindow::applyFishEyeEffect(int hoveredIndex)
{
    // 鱼眼效果：悬浮图标放大，相邻图标依次递减
    for (int i = 0; i < m_layout->count(); ++i) {
        DockItem *item = qobject_cast<DockItem *>(m_layout->itemAt(i)->widget());
        if (!item) continue;

        int distance = qAbs(i - hoveredIndex);
        qreal factor;
        switch (distance) {
        case 0:  factor = 1.5; break;   // 悬浮图标 150%
        case 1:  factor = 1.25; break;  // 相邻 125%
        case 2:  factor = 1.1; break;   // 次相邻 110%
        default: factor = 1.0; break;   // 其他 100%
        }
        item->setScaleFactor(factor);
    }
    m_layout->invalidate();
}

void DockWindow::resetFishEyeEffect()
{
    for (int i = 0; i < m_layout->count(); ++i) {
        DockItem *item = qobject_cast<DockItem *>(m_layout->itemAt(i)->widget());
        if (item) {
            item->setScaleFactor(1.0);
        }
    }
    m_layout->invalidate();
}

void DockWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void DockWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void DockWindow::dropEvent(QDropEvent *event)
{
    QString appId = event->mimeData()->text();
    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;

    DockItem *draggedItem = it.value();

    // 计算放置位置
    int dropIndex = 0;
    for (int i = 0; i < m_layout->count(); ++i) {
        DockItem *item = qobject_cast<DockItem *>(m_layout->itemAt(i)->widget());
        if (!item) continue;
        if (event->pos().x() > item->geometry().center().x()) {
            dropIndex = i + 1;
        }
    }

    // 移除并重新插入
    m_layout->removeWidget(draggedItem);
    m_layout->insertWidget(dropIndex, draggedItem);

    event->acceptProposedAction();
}
