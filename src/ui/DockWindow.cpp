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
#include <QCursor>
#include <QDebug>
#include <QTimer>
#include <QProcess>
#include <QWindow>
#include <QFileInfo>

DockWindow::DockWindow(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_animationHandler(new AnimationHandler(this))
    , m_dockManager(nullptr)
    , m_iconSize(48)
    , m_opacity(0.95)
    , m_isHidden(false)
    , m_monitorIndex(-1)
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

    // 监听屏幕变化（插拔显示器、分辨率变化、DPI 变化）
    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *) {
        updateDpiScale();
        updatePosition();
    });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
        updateDpiScale();
        updatePosition();
    });

    updateDpiScale();
    updatePosition();

    // 定期检测应用运行状态（每 2 秒）
    QTimer *stateTimer = new QTimer(this);
    connect(stateTimer, &QTimer::timeout, this, &DockWindow::checkRunningApps);
    stateTimer->start(2000);
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

void DockWindow::setMonitor(int index)
{
    m_monitorIndex = index;
    updatePosition();
}

void DockWindow::moveToScreen(QScreen *screen)
{
    if (!screen) return;

    QRect screenGeo = screen->availableGeometry();
    int dockH = m_iconSize + 24;
    int dockW = qMin(screenGeo.width() - 40, 800);

    setFixedHeight(dockH);
    setFixedWidth(dockW);
    move(screenGeo.x() + (screenGeo.width() - dockW) / 2,
         screenGeo.y() + screenGeo.height() - dockH - 10);
}

void DockWindow::updatePosition()
{
    QScreen *targetScreen = nullptr;

    if (m_monitorIndex >= 0) {
        // 指定显示器
        const auto screens = QGuiApplication::screens();
        if (m_monitorIndex < screens.size()) {
            targetScreen = screens.at(m_monitorIndex);
        }
    }

    // 回退到鼠标所在屏幕，再回退到主屏幕
    if (!targetScreen) {
        targetScreen = QGuiApplication::screenAt(QCursor::pos());
    }
    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }

    if (targetScreen) {
        moveToScreen(targetScreen);
    }
}

bool DockWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(message);
    Q_UNUSED(result);

    // X11 屏幕变化事件（分辨率变化等）
    if (eventType == "xcb_generic_event_t") {
        // 延迟更新位置，避免事件处理期间操作窗口
        QMetaObject::invokeMethod(this, [this]() {
            updatePosition();
        }, Qt::QueuedConnection);
    }

    return false;  // 不拦截事件
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

void DockWindow::updateDpiScale()
{
    QScreen *screen = windowHandle() ? windowHandle()->screen() : QGuiApplication::primaryScreen();
    if (!screen) return;

    qreal dpi = screen->logicalDotsPerInch();
    // 基准 DPI 为 96（1x 缩放）
    qreal scale = qBound(0.5, dpi / 96.0, 3.0);
    m_iconSize = static_cast<int>(48 * scale);

    // 更新所有 DockItem 的基础尺寸
    for (int i = 0; i < m_layout->count(); ++i) {
        DockItem *item = qobject_cast<DockItem *>(m_layout->itemAt(i)->widget());
        if (item) {
            item->setScaleFactor(1.0);  // 重置为基础尺寸
        }
    }

    qInfo() << "DPI 更新:" << dpi << "缩放:" << scale << "图标尺寸:" << m_iconSize;
}

void DockWindow::checkRunningApps()
{
    // 通过 /proc 检测已注册应用是否仍在运行
    for (auto it = m_itemMap.begin(); it != m_itemMap.end(); ++it) {
        DockItem *item = it.value();
        if (item->execPath().isEmpty()) continue;

        QString execName = item->execPath().split(' ').first();
        // 提取可执行文件名（去掉路径）
        QFileInfo fi(execName);
        QString processName = fi.fileName();

        // 使用 pgrep 检测进程
        QProcess pgrep;
        pgrep.start("pgrep", {"-x", processName});
        pgrep.waitForFinished(500);
        bool running = (pgrep.exitCode() == 0);

        item->setRunning(running);
    }
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
