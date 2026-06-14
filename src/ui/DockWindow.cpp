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
#include "ui/DockAnimation.h"
#include "core/DockManager.h"
#include "core/SysHelper.h"
#include "core/AppIdHelper.h"
#include "core/ProcessMonitor.h"
#include "core/WindowCache.h"
#include "core/ClickStateMachine.h"
#include "core/ConfigManager.h"
#include "ui/WindowPreviewPanel.h"
#include "ui/OverflowPanel.h"
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QProcess>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QCursor>
#include <QEnterEvent>
#include <QTimer>
#include <QWindow>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QPushButton>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif


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
    , m_slideAnim(nullptr)
    , m_fadeOpacity(1.0)
    , m_blurInitialized(false)
    , m_isLightTheme(false)
    , m_themeTimer(new QTimer(this))
    , m_overflowItem(nullptr)
    , m_windowCountTimer(new QTimer(this))
    , m_previewItem(nullptr)
    , m_bottomEdgeTimer(new QTimer(this))
    , m_windowPreview(new WindowPreviewPanel(this))
    , m_overflowPanel(new OverflowPanel(this))
    , m_animation(new DockAnimation(this))
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

    // 主题轮询检测（每 5 秒检查一次，兼容无 WM_SETTINGCHANGE 的场景）
    m_themeTimer->setInterval(5000);
    connect(m_themeTimer, &QTimer::timeout, this, &DockWindow::updateTheme);
    m_themeTimer->start();

    // 初始主题检测
    updateTheme();

    // 窗口数量定时更新（每 2 秒）
    m_windowCountTimer->setInterval(2000);
    connect(m_windowCountTimer, &QTimer::timeout, this, &DockWindow::updateWindowCounts);
    m_windowCountTimer->start();

    // 底部边缘唤起定时器（Hidden 状态每 200ms 检查鼠标位置）
    m_bottomEdgeTimer->setInterval(200);
    connect(m_bottomEdgeTimer, &QTimer::timeout, this, [this]() {
        if (!m_isHidden || !m_sysHelper || !m_dockManager) return;

        QPoint cursor = m_sysHelper->cursorPos();
        if (cursor.isNull()) return;

        // 获取当前鼠标所在屏幕的物理底部边缘
        QScreen *scr = QGuiApplication::screenAt(cursor);
        if (!scr) scr = QGuiApplication::primaryScreen();
        if (!scr) return;

        QRect geo = scr->geometry();
        int bottomEdge = geo.y() + geo.height();
        // 鼠标在屏幕底部 8px 范围内触发（与 kBaseSpacing 一致）
        if (cursor.y() >= bottomEdge - kBaseSpacing && cursor.y() <= bottomEdge) {
            m_bottomEdgeTimer->stop();
            m_dockManager->onWinKeyPressed();
        }
    });

    // 动画 → 布局联动（批处理去抖，帧级合并）
    connect(m_animation, &DockAnimation::relayoutRequested, this, &DockWindow::relayoutItems);

}

void DockWindow::setDockManager(DockManager *manager)
{
    m_dockManager = manager;
    connect(manager, &DockManager::itemAdded, this, &DockWindow::onItemAdded);
    connect(manager, &DockManager::itemRemoved, this, &DockWindow::onItemRemoved);
    connect(manager, &DockManager::itemStateChanged, this, &DockWindow::onItemStateChanged);
    connect(manager, &DockManager::stateChanged, this, &DockWindow::onStateChanged);
    connect(manager, &DockManager::overflowChanged, this, &DockWindow::onOverflowChanged);
    connect(manager, &DockManager::itemWindowCountChanged, this, &DockWindow::onItemWindowCountChanged);

    m_overflowPanel->setDockManager(manager);
}

void DockWindow::setSysHelper(SysHelper *helper)
{
    m_sysHelper = helper;
    // 延迟初始化毛玻璃效果（等待窗口创建完成）
    QTimer::singleShot(100, this, &DockWindow::initBlurEffect);
    // 刷新主题
    updateTheme();
    // 设置预览面板的 SysHelper
    m_windowPreview->setSysHelper(m_sysHelper);
    m_overflowPanel->setSysHelper(m_sysHelper);
}

void DockWindow::setProcessMonitor(ProcessMonitor *monitor)
{
    connect(monitor, &ProcessMonitor::appRunningStateChanged,
            this, &DockWindow::onAppRunningStateChanged);
    connect(monitor, &ProcessMonitor::newRunningAppDetected,
            this, &DockWindow::onNewRunningAppDetected);
    connect(monitor, &ProcessMonitor::runningAppExited,
            this, &DockWindow::onRunningAppExited);
}

void DockWindow::setConfigManager(ConfigManager *config)
{
    m_config = config;
    m_animation->setConfig(config);
    if (config) {
        qreal opacity = config->get(QStringLiteral("opacity"), 0.95).toReal();
        setWindowOpacity(qBound(0.1, opacity, 1.0));
    }
}

void DockWindow::setFadeOpacity(qreal opacity)
{
    m_fadeOpacity = qBound(0.0, opacity, 1.0);
    setWindowOpacity(m_fadeOpacity);
    update();
}

void DockWindow::setWindowCache(WindowCache *cache)
{
    m_windowCache = cache;
    m_clickStateMachine = new ClickStateMachine(cache, this);

    m_windowPreview->setWindowCache(cache);
    m_overflowPanel->setWindowCache(cache);

    // 预览窗鱼眼联动 + 阻止 Dock 隐藏
    connect(m_windowPreview, &WindowPreviewPanel::previewShown, this, [this]() {
        if (m_hoveredIndex >= 0)
            m_animation->lockFishEye(m_hoveredIndex);
        if (m_dockManager)
            m_dockManager->setPreviewActive(true);
    });
    connect(m_windowPreview, &WindowPreviewPanel::previewHidden, this, [this]() {
        m_hoveredIndex = -1;
        m_animation->unlockFishEye(m_items);
        if (m_dockManager)
            m_dockManager->setPreviewActive(false);
    });
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
    qreal maxScale = m_config
        ? m_config->get(QStringLiteral("magnification"), 1.5).toReal()
        : 1.5;
    int maxItemW = static_cast<int>(baseSize * maxScale);
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

    // 保持 dock 在屏幕中央（防止鱼眼时右移）并更新毛玻璃区域
    updatePosition();
    updateBlurRegion();

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

void DockWindow::requestUpdatePosition()
{
    updatePosition();
}

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
        // 使用物理屏幕几何（不含任务栏扣除），间距 = 图标间距 kBaseSpacing
        QRect geo = targetScreen->geometry();
        int w = width();
        int h = height();
        move(geo.x() + (geo.width() - w) / 2,
             geo.y() + geo.height() - h - kBaseSpacing);
    }
}

bool DockWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);

    if (eventType == "xcb_generic_event_t") {
        QMetaObject::invokeMethod(this, [this]() {
            updatePosition();
        }, Qt::QueuedConnection);
    }
#ifdef Q_OS_WIN
    else if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_SETTINGCHANGE) {
            // 系统设置变更 → 检查主题是否切换
            QMetaObject::invokeMethod(this, &DockWindow::updateTheme, Qt::QueuedConnection);
        }
    }
#endif
    return false;
}

// ─── 绘制 ────────────────────────────────────────────────

void DockWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_items.isEmpty()) return;

    // 从 config 读取参数
    int cornerRadius = m_config
        ? m_config->get(QStringLiteral("cornerRadius"), 16).toInt()
        : 16;

    // 背景条固定在窗口底部，高度不变
    int baseSize = m_items[0]->baseSize();
    int barH = kMarginTop + baseSize + kMarginBottom;
    int barTop = height() - barH;
    QRect barRect(0, barTop, width(), barH);

    // 绘制柔和扩散阴影（多层渐变）
    painter.setPen(Qt::NoPen);
    for (int i = 3; i >= 0; --i) {
        int spread = (i + 1) * 3;
        int alpha = 12 - i * 3;
        painter.setBrush(QColor(0, 0, 0, alpha));
        painter.drawRoundedRect(barRect.adjusted(-spread, 1, spread, spread + 4),
                                cornerRadius + i * 2, cornerRadius + i * 2);
    }

    // 主题自适应背景色
    QColor bgColor;
    QColor borderColor;
    if (m_isLightTheme) {
        bgColor = QColor(245, 245, 245, static_cast<int>(0.65 * 255));
        borderColor = QColor(200, 200, 200, 80);
    } else {
        bgColor = QColor(40, 40, 40, static_cast<int>(0.55 * 255));
        borderColor = QColor(80, 80, 80, 60);
    }

    painter.setBrush(bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawRoundedRect(barRect.adjusted(1, 1, -1, -1), cornerRadius, cornerRadius);
}

// ─── 毛玻璃 & 主题 ──────────────────────────────────────────

void DockWindow::initBlurEffect()
{
    if (m_blurInitialized || !m_sysHelper) return;

    if (m_sysHelper->isBlurSupported()) {
        // 使用基于区域的模糊，仅模糊 dock 栏实际区域（避免上方透明区域被暗化）
        updateBlurRegion();
        m_blurInitialized = true;
        qInfo() << "DWM 毛玻璃效果已启用";
    } else {
        qInfo() << "DWM 模糊不支持，使用纯色半透明背景";
    }
}

void DockWindow::updateBlurRegion()
{
    if (!m_sysHelper || !m_blurInitialized) return;

    // 避免无变化时重复调用 DWM API（每帧动画都触发 relayoutItems）
    QSize currentSize(width(), height());
    if (currentSize == m_lastBlurSize) return;
    m_lastBlurSize = currentSize;

    // 模糊区域 = dock 背景条的实际范围（底部 baseSize + 上下间距）
    int barH = kMarginTop + m_baseIconSize + kMarginBottom;
    QRect barRect(0, height() - barH, width(), barH);
    m_sysHelper->enableBlurBehindWindow(winId(), barRect, 16);
}

void DockWindow::updateTheme()
{
    if (m_hasThemeOverride) {
        if (m_themeOverride != m_isLightTheme) {
            m_isLightTheme = m_themeOverride;
            update();
        }
        return;
    }
    if (!m_sysHelper) return;
    bool newTheme = m_sysHelper->isLightTheme();
    if (newTheme != m_isLightTheme) {
        m_isLightTheme = newTheme;
        qInfo() << "主题切换:" << (m_isLightTheme ? "亮色" : "暗色");
        update();
    }
}

void DockWindow::simulateHover(int index)
{
    if (index < 0 || index >= m_items.size()) return;
    m_hoveredIndex = index;
    m_animation->applyFishEye(index, m_items);
}

void DockWindow::setThemeOverride(bool isLight)
{
    m_hasThemeOverride = true;
    m_themeOverride = isLight;
    m_isLightTheme = isLight;
    update();
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
    m_animation->resetFishEye(m_items);
    if (!m_animation->isFishEyeLocked())
        m_hoveredIndex = -1;
    m_windowPreview->startDelayedHide();
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
    if (index != m_hoveredIndex && !m_animation->isFishEyeLocked()) {
        m_hoveredIndex = index;
        if (index >= 0)
            m_animation->applyFishEye(index, m_items);
        else
            m_animation->resetFishEye(m_items);
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
        if (w->parent() == this && !m_items.isEmpty()) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            QPoint posInDock = mapFromGlobal(me->globalPosition().toPoint());
            int index = itemAtPos(posInDock.x(), posInDock.y());
            if (index != m_hoveredIndex && !m_animation->isFishEyeLocked()) {
                m_hoveredIndex = index;
                if (index >= 0)
                    m_animation->applyFishEye(index, m_items);
                else
                    m_animation->resetFishEye(m_items);
            }

            // 窗口预览：悬停时委托给 WindowPreviewPanel
            if (index >= 0) {
                DockItem *hoveredItem = m_items[index];
                if (hoveredItem != m_previewItem) {
                    m_windowPreview->hidePreview();
                    m_previewItem = hoveredItem;
                    m_windowPreview->showPreview(hoveredItem);
                }
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

    // 从 config 读取图标基础大小
    if (m_config) {
        int iconSize = m_config->get(QStringLiteral("iconSize"), 48).toInt();
        item->setBaseSize(qBound(24, iconSize, 128));
    }

    m_itemMap[data.appId] = item;
    m_items.append(item);

    // 连接点击：直接处理单击
    connect(item, &DockItem::clicked, this, [this, item](const QString &) {
        handleSingleClick(item);
    });

    // 连接固定/取消固定请求到 DockManager
    connect(item, &DockItem::pinRequested, this, [this](const QString &appId, bool pin) {
        if (!m_dockManager) return;
        if (pin) {
            // 从当前项列表中查找完整数据
            for (const auto &d : m_dockManager->items()) {
                if (d.appId == appId) {
                    m_dockManager->pinItem(d);
                    return;
                }
            }
        } else {
            m_dockManager->unpinItem(appId);
        }
    });

    item->installEventFilter(this);
    item->show();
    relayoutItems();
    updatePosition();

    // 图标添加动画
    m_animation->animateItemAdd(item);
}

void DockWindow::onItemRemoved(const QString &appId)
{
    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;

    DockItem *item = it.value();
    m_itemMap.erase(it);

    m_animation->animateItemRemove(item, [this, item]() {
        m_items.removeOne(item);
        item->deleteLater();
    });
}

void DockWindow::onItemStateChanged(const QString &appId, bool isRunning)
{
    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;
    it.value()->setRunning(isRunning);
}

void DockWindow::onStateChanged(DockState newState)
{
    // 停止之前的动画（如果有）
    if (m_slideAnim) {
        m_slideAnim->stop();
        m_slideAnim->deleteLater();
        m_slideAnim = nullptr;
    }

    switch (newState) {
    case DockState::Docked: {
        m_isHidden = false;
        m_bottomEdgeTimer->stop();
        show();

        // 从底部滑入 + 淡入
        QRect geo = screen() ? screen()->geometry() : QGuiApplication::primaryScreen()->geometry();
        int targetY = geo.y() + geo.height() - height() - kBaseSpacing;
        int targetX = geo.x() + (geo.width() - width()) / 2;
        QPoint startPos(targetX, targetY + 60);  // 从下方 60px 处滑入
        QPoint endPos(targetX, targetY);

        m_slideAnim = new QPropertyAnimation(this, "pos");
        m_slideAnim->setDuration(m_animation->animDuration(300));
        m_slideAnim->setStartValue(startPos);
        m_slideAnim->setEndValue(endPos);
        m_slideAnim->setEasingCurve(QEasingCurve::OutCubic);

        QPropertyAnimation *fadeAnim = new QPropertyAnimation(this, "fadeOpacity");
        fadeAnim->setDuration(m_animation->animDuration(300));
        fadeAnim->setStartValue(0.0);
        fadeAnim->setEndValue(1.0);
        fadeAnim->setEasingCurve(QEasingCurve::OutCubic);

        connect(m_slideAnim, &QObject::destroyed, this, [this]() {
            m_slideAnim = nullptr;
        });

        m_slideAnim->start(QAbstractAnimation::DeleteWhenStopped);
        fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
        break;
    }
    case DockState::Hidden: {
        m_isHidden = true;

        // 启动底部边缘轮询（Dock 隐藏时始终生效）
        m_bottomEdgeTimer->start();

        // 向下滑出 + 淡出
        QPoint currentPos = pos();
        QPoint endPos(currentPos.x(), currentPos.y() + 60);

        m_slideAnim = new QPropertyAnimation(this, "pos");
        m_slideAnim->setDuration(250);
        m_slideAnim->setStartValue(currentPos);
        m_slideAnim->setEndValue(endPos);
        m_slideAnim->setEasingCurve(QEasingCurve::InCubic);

        QPropertyAnimation *fadeAnim = new QPropertyAnimation(this, "fadeOpacity");
        fadeAnim->setDuration(m_animation->animDuration(250));
        fadeAnim->setStartValue(1.0);
        fadeAnim->setEndValue(0.0);
        fadeAnim->setEasingCurve(QEasingCurve::InCubic);

        connect(m_slideAnim, &QObject::destroyed, this, [this]() {
            m_slideAnim = nullptr;
        });

        // hide() 在动画完成后通过 finished 信号触发
        connect(fadeAnim, &QPropertyAnimation::finished, this, [this]() {
            hide();
            m_fadeOpacity = 1.0;
            setWindowOpacity(1.0);
        });

        m_slideAnim->start(QAbstractAnimation::DeleteWhenStopped);
        fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
        break;
    }
    }
}

// ─── 溢出抽屉 & 窗口数量 ──────────────────────────────────

void DockWindow::onOverflowChanged()
{
    updateOverflowItem();
}

void DockWindow::onItemWindowCountChanged(const QString &appId, int count)
{
    auto it = m_itemMap.find(appId);
    if (it != m_itemMap.end()) {
        it.value()->setWindowCount(count);
    }
}

void DockWindow::updateWindowCounts()
{
    if (!m_sysHelper || !m_dockManager || !m_windowCache) return;

    // 先刷新缓存
    m_windowCache->refresh();

    // 更新所有运行中图标的窗口数量和前台状态
    for (auto it = m_itemMap.begin(); it != m_itemMap.end(); ++it) {
        DockItem *item = it.value();
        if (!item->isRunning()) continue;

        QString wmClass = AppIdHelper::deriveWmClass(item->execPath(), item->appId());

        int count = m_windowCache->getWindowCount(wmClass);
        if (count > 0) {
            m_dockManager->updateWindowCount(item->appId(), count);
        }

        // 更新前台激活指示器
        item->setForegroundActive(m_windowCache->isForegroundApp(wmClass));
    }
}

void DockWindow::updateOverflowItem()
{
    if (!m_dockManager) return;

    bool hasOverflow = !m_dockManager->overflowItems().isEmpty();

    if (hasOverflow && !m_overflowItem) {
        // 创建抽屉图标
        m_overflowItem = new DockItem("__overflow__", "", "...", this);
        m_overflowItem->setFixedSize(48, 48);
        m_itemMap["__overflow__"] = m_overflowItem;
        m_items.append(m_overflowItem);

        connect(m_overflowItem, &DockItem::clicked, this, [this](const QString &) {
            showOverflowPopup();
        });

        m_overflowItem->installEventFilter(this);
        m_overflowItem->show();
        relayoutItems();
        updatePosition();
        m_animation->animateItemAdd(m_overflowItem);

    } else if (!hasOverflow && m_overflowItem) {
        // 移除抽屉图标
        auto it = m_itemMap.find("__overflow__");
        if (it != m_itemMap.end()) {
            m_itemMap.erase(it);
        }
        DockItem *removing = m_overflowItem;
        m_overflowItem = nullptr;
        m_items.removeOne(removing);
        m_animation->animateItemRemove(removing, [removing]() {
            removing->deleteLater();
        });
    }
}

void DockWindow::showOverflowPopup()
{
    m_overflowPanel->showPopup(m_overflowItem, this);
}

// ─── 单双击处理 ────────────────────────────────────────────

/**
 * @brief 启动应用（新窗口）
 */
void DockWindow::launchApp(DockItem *item)
{
    if (!item || item->execPath().isEmpty()) return;
    // 不分割路径（路径可能包含空格如 "C:\Program Files\..."）
    QString nativePath = QDir::toNativeSeparators(item->execPath());
    QProcess::startDetached(nativePath, QStringList());
}

/**
 * @brief 单击处理 — 由 ClickStateMachine 5 状态状态机判定
 */
void DockWindow::handleSingleClick(DockItem *item)
{
    if (!item || !m_clickStateMachine) {
        launchApp(item);
        return;
    }

    QString wmClass = AppIdHelper::deriveWmClass(item->execPath(), item->appId());
    bool isRunning = item->isRunning();

    m_clickStateMachine->handleClick(wmClass, item->execPath(), isRunning);

    // 触发交互指示器（1 秒延迟等待窗口操作生效）
    QTimer::singleShot(1000, item, [item]() {
        item->triggerInteractionIndicator();
    });
}

QVariant DockWindow::isItemPinned(QVariant appId)
{
    if (!m_dockManager) return false;
    return m_dockManager->isPinned(appId.toString());
}

void DockWindow::unlockFishEye()
{
    m_hoveredIndex = -1;
    m_animation->unlockFishEye(m_items);
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

// ─── 右键菜单 ─────────────────────────────────────────────

void DockWindow::contextMenuEvent(QContextMenuEvent *event)
{
    // 检查右键位置是否在某个 DockItem 上
    int index = itemAtPos(event->pos().x(), event->pos().y());
    if (index >= 0) {
        // 在 DockItem 上，让 DockItem 自己处理右键菜单
        return;
    }

    // 在 Dock 背景空白区域，显示系统操作菜单
    QMenu menu(this);

    QAction *taskMgrAction = menu.addAction("任务管理器");
    connect(taskMgrAction, &QAction::triggered, this, []() {
        QProcess::startDetached("taskmgr", QStringList());
    });

    QAction *taskbarSettingsAction = menu.addAction("任务栏设置");
    connect(taskbarSettingsAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("ms-settings:taskbar"));
    });

    menu.addSeparator();

    QAction *quitAction = menu.addAction("退出 Dock");
    connect(quitAction, &QAction::triggered, this, []() {
        qApp->quit();
    });

    menu.exec(event->globalPos());
}

