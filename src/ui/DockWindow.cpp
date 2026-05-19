/**
 * @file DockWindow.cpp
 * @brief Dock 主窗口实现
 *
 * 无边框、半透明背景、置顶显示的 Dock 主窗体。
 * 使用手动定位管理 DockItem，实现 macOS 风格变量间隙鱼眼动画。
 * 所有 item 底部对齐，放大时向上生长，间隙随鱼眼效果动态变化。
 */

#include "ui/DockWindow.h"
#include "ui/DockItem.h"
#include "core/DockManager.h"
#include "core/SysHelper.h"
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
#include <QEnterEvent>
#include <QTimer>
#include <QWindow>
#include <QDir>
#include <QFile>


DockWindow::DockWindow(QWidget *parent)
    : QWidget(parent)
    , m_dockManager(nullptr)
    , m_sysHelper(nullptr)
    , m_baseIconSize(48)
    , m_fixedWindowH(0)
    , m_opacity(0.95)
    , m_isHidden(false)
    , m_monitorIndex(-1)
    , m_hoveredIndex(-1)
    , m_clickTimer(new QTimer(this))
    , m_pendingClickItem(nullptr)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAcceptDrops(true);
    setMouseTracking(true);
    installEventFilter(this);

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

    // 单双击检测定时器（250ms 内无第二次点击则为单击）
    m_clickTimer->setSingleShot(true);
    m_clickTimer->setInterval(250);
    connect(m_clickTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingClickItem) {
            handleSingleClick(m_pendingClickItem);
            m_pendingClickItem = nullptr;
        }
    });
}

void DockWindow::setDockManager(DockManager *manager)
{
    m_dockManager = manager;
    connect(manager, &DockManager::itemAdded, this, &DockWindow::onItemAdded);
    connect(manager, &DockManager::itemRemoved, this, &DockWindow::onItemRemoved);
    connect(manager, &DockManager::itemStateChanged, this, &DockWindow::onItemStateChanged);
    connect(manager, &DockManager::stateChanged, this, &DockWindow::onStateChanged);
}

void DockWindow::setSysHelper(SysHelper *helper)
{
    m_sysHelper = helper;
}

void DockWindow::setMonitor(int index)
{
    m_monitorIndex = index;
    updatePosition();
}

// ─── 布局计算 ────────────────────────────────────────────

/**
 * @brief 根据当前 visualScale 重新计算所有 item 的位置
 *
 * 窗口尺寸完全固定，不随动画变化。
 * 背景条绘制在窗口底部固定区域，图标在背景条上方浮动缩放。
 * 图标底部对齐到背景条顶部，放大时向上生长。
 */
void DockWindow::relayoutItems()
{
    if (m_items.isEmpty()) return;

    int baseSize = m_items[0]->baseSize();
    int maxItemW = static_cast<int>(baseSize * 1.5);  // 最大缩放后的单图标宽度
    int n = m_items.size();

    // ── 窗口高度固定，宽度随内容动态变化 ──
    int barH = kMarginTop + maxItemW + kMarginBottom;
    m_fixedWindowH = barH;

    // ── 计算当前实际内容宽度（含变量间隙）──
    int contentW = 0;
    for (int i = 0; i < n; ++i) {
        contentW += m_items[i]->width();
        if (i < n - 1) {
            int gap = kBaseSpacing;
            if (m_hoveredIndex >= 0 && (i == m_hoveredIndex || i + 1 == m_hoveredIndex)) {
                gap = kExtraSpacing;
            }
            contentW += gap;
        }
    }

    // ── 窗口宽度 = 内容宽度 + 两侧边距（无多余空白）──
    int windowW = qMax(contentW + 2 * kMarginH, 20);
    setFixedSize(windowW, m_fixedWindowH);

    // ── 图标在窗口内居中排列 ──
    int x = (windowW - contentW) / 2;
    int iconBottom = m_fixedWindowH - kMarginBottom;
    for (int i = 0; i < n; ++i) {
        DockItem *item = m_items[i];
        int y = iconBottom - item->height();
        item->move(x, y);
        x += item->width();
        if (i < n - 1) {
            int gap = kBaseSpacing;
            if (m_hoveredIndex >= 0 && (i == m_hoveredIndex || i + 1 == m_hoveredIndex)) {
                gap = kExtraSpacing;
            }
            x += gap;
        }
    }

    update();  // 触发重绘背景
}

// ─── 窗口定位 ────────────────────────────────────────────

void DockWindow::updatePosition()
{
    QScreen *targetScreen = nullptr;

    if (m_monitorIndex >= 0) {
        const auto screens = QGuiApplication::screens();
        if (m_monitorIndex < screens.size()) {
            targetScreen = screens.at(m_monitorIndex);
        }
    }

    if (!targetScreen) {
        targetScreen = QGuiApplication::screenAt(QCursor::pos());
    }
    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }

    if (targetScreen) {
        QRect geo = targetScreen->availableGeometry();
        int w = width();
        int h = height();
        move(geo.x() + (geo.width() - w) / 2,
             geo.y() + geo.height() - h - 10);
    }
}

bool DockWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(message);
    Q_UNUSED(result);
    if (eventType == "xcb_generic_event_t") {
        QMetaObject::invokeMethod(this, [this]() {
            updatePosition();
        }, Qt::QueuedConnection);
    }
    return false;
}

// ─── 绘制 ────────────────────────────────────────────────

void DockWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_items.isEmpty()) return;

    // 背景条固定在窗口底部，高度不变
    int barH = kMarginTop + m_items[0]->baseSize() + kMarginBottom;
    int barTop = height() - barH;
    QRect barRect(0, barTop, width(), barH);

    QColor bgColor(48, 48, 48, static_cast<int>(m_opacity * 255));
    painter.setBrush(bgColor);
    painter.setPen(QPen(QColor(80, 80, 80, 100), 1));
    painter.drawRoundedRect(barRect.adjusted(1, 1, -1, -1), 14, 14);
}

// ─── 鼠标事件 ─────────────────────────────────────────────

void DockWindow::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    if (m_isHidden && m_dockManager) {
        m_dockManager->onWinKeyPressed();
    }
}

void DockWindow::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    resetFishEyeEffect();
}

/**
 * @brief 检查鼠标位置是否在某个 item 范围内
 * @return item 索引，如果不在任何 item 上则返回 -1
 */
int DockWindow::itemAtPos(int mouseX, int mouseY) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        QRect r = m_items[i]->geometry();
        if (mouseX >= r.left() && mouseX <= r.right() &&
            mouseY >= r.top() && mouseY <= r.bottom()) {
            return i;
        }
    }
    return -1;
}

void DockWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_items.isEmpty()) return;

    int index = itemAtPos(event->pos().x(), event->pos().y());
    if (index != m_hoveredIndex) {
        if (index >= 0)
            applyFishEyeEffect(index);
        else
            resetFishEyeEffect();
    }
}

/**
 * @brief 拦截子 item 的鼠标移动事件，转发给 DockWindow 处理鱼眼
 *
 * 解决子 widget 消费 mouseMoveEvent 导致父窗口收不到事件的问题。
 */
bool DockWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseMove && obj->isWidgetType()) {
        QWidget *w = static_cast<QWidget *>(obj);
        // 只处理属于 DockItem 的子组件
        if (w->parent() == this && !m_items.isEmpty()) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            QPoint posInDock = mapFromGlobal(me->globalPosition().toPoint());
            int index = itemAtPos(posInDock.x(), posInDock.y());
            if (index != m_hoveredIndex) {
                if (index >= 0)
                    applyFishEyeEffect(index);
                else
                    resetFishEyeEffect();
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ─── 项目管理 ─────────────────────────────────────────────

void DockWindow::onItemAdded(const DockItemData &data)
{
    if (m_itemMap.contains(data.appId)) return;

    DockItem *item = new DockItem(data.appId, data.iconPath, data.displayName, this);
    item->setExecPath(data.execPath);
    item->setRunning(data.isRunning);
    item->setBadgeCount(data.badgeCount);

    m_itemMap[data.appId] = item;
    m_items.append(item);

    // 连接点击：单双击检测
    connect(item, &DockItem::clicked, this, [this, item](const QString &) {
        if (m_pendingClickItem == item) {
            // 第二次点击 → 双击：打开新窗口
            m_clickTimer->stop();
            m_pendingClickItem = nullptr;
            handleDoubleClick(item);
        } else {
            // 第一次点击 → 启动定时器等待可能的第二次点击
            m_pendingClickItem = item;
            m_clickTimer->start();
        }
    });

    // 连接固定/取消固定请求到 DockManager
    connect(item, &DockItem::pinRequested, this, [this](const QString &appId, bool pin) {
        if (!m_dockManager) return;
        if (pin)
            m_dockManager->pinItem(appId);
        else
            m_dockManager->unpinItem(appId);
    });

    item->installEventFilter(this);
    item->show();
    relayoutItems();
    updatePosition();
}

void DockWindow::onItemRemoved(const QString &appId)
{
    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;

    DockItem *item = it.value();
    m_itemMap.erase(it);
    m_items.removeOne(item);

    // 清理动画
    auto animIt = m_fishEyeAnims.find(item);
    if (animIt != m_fishEyeAnims.end()) {
        animIt.value()->stop();
        animIt.value()->deleteLater();
        m_fishEyeAnims.erase(animIt);
    }

    item->deleteLater();
    relayoutItems();
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
        auto *anim = new QPropertyAnimation(this, "windowOpacity");
        anim->setDuration(200);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutBack);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        break;
    }
    case DockState::Hidden: {
        m_isHidden = true;
        auto *anim = new QPropertyAnimation(this, "windowOpacity");
        anim->setDuration(200);
        anim->setEndValue(0.0);
        anim->setEasingCurve(QEasingCurve::OutQuad);
        connect(anim, &QPropertyAnimation::finished, this, [this]() {
            hide();
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        break;
    }
    case DockState::Animating:
        break;
    }
}

// ─── 单双击处理 ────────────────────────────────────────────

/**
 * @brief 启动应用（新窗口）
 */
void DockWindow::launchApp(DockItem *item)
{
    if (!item || item->execPath().isEmpty()) return;
    QStringList parts = item->execPath().split(' ');
    QString program = parts.takeFirst();
    QProcess::startDetached(program, parts);
}

/**
 * @brief 单击处理
 *
 * 0 个窗口 → 启动应用
 * 1 个窗口 → 激活已有窗口
 * 2+ 个窗口 → 调用系统窗口选择器
 *
 * 如果 item 标记为 running 但 getWindowCount 返回 0（WM_CLASS 不匹配），
 * 直接尝试 activateWindow 做 fallback。
 */
void DockWindow::handleSingleClick(DockItem *item)
{
    if (!m_sysHelper || !item) {
        launchApp(item);
        return;
    }

    // 从 appId 推导 WM_CLASS（org.gnome.Nautilus → Nautilus）
    QString wmClass = item->appId();
    int dotIdx = wmClass.lastIndexOf('.');
    if (dotIdx >= 0) {
        wmClass = wmClass.mid(dotIdx + 1);
    }

    if (item->isRunning()) {
        // 应用正在运行 → 尝试激活已有窗口
        int count = m_sysHelper->getWindowCount(wmClass);
        if (count == 1) {
            m_sysHelper->activateWindow(wmClass);
            return;
        } else if (count > 1) {
            m_sysHelper->showWindowPicker();
            return;
        }
        // getWindowCount 返回 0 但 isRunning=true → WM_CLASS 可能不匹配
        // fallback：直接调用 activateWindow，它内部会重新查找
        if (m_sysHelper->activateWindow(wmClass)) {
            return;
        }
        // 二次 fallback：用原始 appId 试一次
        if (m_sysHelper->activateWindow(item->appId())) {
            return;
        }
        // 都失败了，降级为启动新实例
        launchApp(item);
    } else {
        // 应用未运行 → 启动
        launchApp(item);
    }
}

/**
 * @brief 双击处理：始终打开新窗口
 */
void DockWindow::handleDoubleClick(DockItem *item)
{
    launchApp(item);
}

QVariant DockWindow::isItemPinned(QVariant appId)
{
    if (!m_dockManager) return false;
    return m_dockManager->isPinned(appId.toString());
}

// ─── 鱼眼动画 ─────────────────────────────────────────────

void DockWindow::animateItemToScale(DockItem *item, qreal targetScale)
{
    auto it = m_fishEyeAnims.find(item);
    if (it != m_fishEyeAnims.end()) {
        it.value()->stop();
        it.value()->deleteLater();
    }

    if (qFuzzyCompare(item->visualScale(), targetScale)) {
        m_fishEyeAnims.remove(item);
        return;
    }

    QPropertyAnimation *anim = new QPropertyAnimation(item, "visualScale", this);
    anim->setDuration(180);
    anim->setStartValue(item->visualScale());
    anim->setEndValue(targetScale);
    anim->setEasingCurve(QEasingCurve::OutQuad);

    // 每帧动画后重新布局
    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
        relayoutItems();
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);

    connect(anim, &QPropertyAnimation::finished, this, [this, item]() {
        m_fishEyeAnims.remove(item);
    });

    m_fishEyeAnims[item] = anim;
}

void DockWindow::applyFishEyeEffect(int hoveredIndex)
{
    m_hoveredIndex = hoveredIndex;

    for (int i = 0; i < m_items.size(); ++i) {
        qreal factor = (i == hoveredIndex) ? 1.5 : 1.0;
        animateItemToScale(m_items[i], factor);
    }
}

void DockWindow::resetFishEyeEffect()
{
    m_hoveredIndex = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        animateItemToScale(m_items[i], 1.0);
    }
}

// ─── DPI ──────────────────────────────────────────────────

void DockWindow::updateDpiScale()
{
    QScreen *screen = windowHandle() ? windowHandle()->screen() : QGuiApplication::primaryScreen();
    if (!screen) return;

    qreal dpi = screen->logicalDotsPerInch();
    qreal scale = qBound(0.5, dpi / 96.0, 3.0);
    m_baseIconSize = static_cast<int>(48 * scale);

    qInfo() << "DPI:" << dpi << "scale:" << scale << "iconSize:" << m_baseIconSize;
}

// ─── ProcessMonitor 响应 ─────────────────────────────────────

void DockWindow::onAppRunningStateChanged(const QString &appId, bool isRunning)
{
    auto it = m_itemMap.find(appId);
    if (it != m_itemMap.end()) {
        it.value()->setRunning(isRunning);
    }
}

void DockWindow::onNewRunningAppDetected(const DockItemData &item)
{
    if (m_dockManager) {
        m_dockManager->addTransientItem(item);
    }
}

void DockWindow::onRunningAppExited(const QString &appId)
{
    if (m_dockManager) {
        m_dockManager->removeTransientItem(appId);
    }
}

// ─── 拖拽 ─────────────────────────────────────────────────

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
    for (int i = 0; i < m_items.size(); ++i) {
        if (event->position().toPoint().x() > m_items[i]->geometry().center().x()) {
            dropIndex = i + 1;
        }
    }

    m_items.removeOne(draggedItem);
    m_items.insert(dropIndex, draggedItem);
    relayoutItems();

    event->acceptProposedAction();
}
