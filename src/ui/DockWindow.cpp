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
#include <QGraphicsOpacityEffect>
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
    , m_opacityEffect(new QGraphicsOpacityEffect(this))
    , m_blurInitialized(false)
    , m_isLightTheme(false)
    , m_themeTimer(new QTimer(this))
    , m_clickTimer(new QTimer(this))
    , m_pendingClickItem(nullptr)
    , m_overflowItem(nullptr)
    , m_overflowPopup(nullptr)
    , m_windowCountTimer(new QTimer(this))
    , m_previewTimer(new QTimer(this))
    , m_previewPopup(nullptr)
    , m_previewItem(nullptr)
    , m_bottomEdgeTimer(new QTimer(this))
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAcceptDrops(true);
    setMouseTracking(true);
    installEventFilter(this);

    // 透明度效果（用于显示/隐藏动画）
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);

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

    // 窗口预览延迟定时器（悬停 500ms 后显示预览）
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(500);
    connect(m_previewTimer, &QTimer::timeout, this, [this]() {
        if (m_previewItem) {
            showWindowPreview(m_previewItem);
        }
    });

    // 底部边缘唤起定时器（Hidden 状态每 200ms 检查鼠标位置）
    m_bottomEdgeTimer->setInterval(200);
    connect(m_bottomEdgeTimer, &QTimer::timeout, this, [this]() {
        if (!m_isHidden || !m_sysHelper || !m_dockManager) return;
        if (!m_sysHelper->isTaskbarAutoHideEnabled()) return;

        QPoint cursor = m_sysHelper->cursorPos();
        if (cursor.isNull()) return;

        // 获取当前 dock 所在屏幕的底部边缘
        QScreen *scr = QGuiApplication::screenAt(cursor);
        if (!scr) scr = QGuiApplication::primaryScreen();
        if (!scr) return;

        QRect geo = scr->availableGeometry();
        int bottomEdge = geo.y() + geo.height();
        // 鼠标在屏幕底部 5px 范围内触发
        if (cursor.y() >= bottomEdge - 5 && cursor.y() <= bottomEdge) {
            m_bottomEdgeTimer->stop();
            m_dockManager->onWinKeyPressed();
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
    connect(manager, &DockManager::overflowChanged, this, &DockWindow::onOverflowChanged);
    connect(manager, &DockManager::itemWindowCountChanged, this, &DockWindow::onItemWindowCountChanged);
}

void DockWindow::setSysHelper(SysHelper *helper)
{
    m_sysHelper = helper;
    // 延迟初始化毛玻璃效果（等待窗口创建完成）
    QTimer::singleShot(100, this, &DockWindow::initBlurEffect);
    // 刷新主题
    updateTheme();
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
        QRect geo = targetScreen->availableGeometry();
        int w = width();
        int h = height();
        move(geo.x() + (geo.width() - w) / 2,
             geo.y() + geo.height() - h - 10);
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

    // 背景条固定在窗口底部，高度不变
    int barH = kMarginTop + m_items[0]->baseSize() + kMarginBottom;
    int barTop = height() - barH;
    QRect barRect(0, barTop, width(), barH);

    // 主题自适应背景色
    QColor bgColor;
    QColor borderColor;
    if (m_isLightTheme) {
        bgColor = QColor(245, 245, 245, static_cast<int>(0.75 * 255));
        borderColor = QColor(200, 200, 200, 120);
    } else {
        bgColor = QColor(40, 40, 40, static_cast<int>(0.7 * 255));
        borderColor = QColor(80, 80, 80, 100);
    }

    painter.setBrush(bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawRoundedRect(barRect.adjusted(1, 1, -1, -1), 16, 16);
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
    if (!m_sysHelper) return;
    bool newTheme = m_sysHelper->isLightTheme();
    if (newTheme != m_isLightTheme) {
        m_isLightTheme = newTheme;
        qInfo() << "主题切换:" << (m_isLightTheme ? "亮色" : "暗色");
        update();  // 触发重绘
    }
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

            // 窗口预览：悬停时启动延迟定时器，离开时取消
            if (index >= 0) {
                DockItem *hoveredItem = m_items[index];
                if (hoveredItem != m_previewItem) {
                    hideWindowPreview();
                    m_previewItem = hoveredItem;
                    m_previewTimer->start();
                }
            } else {
                hideWindowPreview();
                m_previewTimer->stop();
                m_previewItem = nullptr;
            }
        }
    }

    // 窗口预览缩略图点击 → 激活对应窗口
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget *w = qobject_cast<QWidget *>(obj);
        if (w) {
            QVariant v = w->property("previewHwnd");
            if (v.isValid()) {
                HWND hwnd = reinterpret_cast<HWND>(v.value<qintptr>());
                if (hwnd && IsWindow(hwnd)) {
                    hideWindowPreview();
                    AllowSetForegroundWindow(ASFW_ANY);
                    SetForegroundWindow(hwnd);
                    ShowWindow(hwnd, SW_RESTORE);
                    SetFocus(hwnd);
                }
                return true;
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

    // 图标添加动画：从 scale 0.0 → 1.0
    animateItemAdd(item);
}

void DockWindow::onItemRemoved(const QString &appId)
{
    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;

    DockItem *item = it.value();
    m_itemMap.erase(it);

    // 图标移除动画：scale 1.0 → 0.0，完成后删除
    animateItemRemove(item, appId);
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
        QRect geo = screen() ? screen()->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
        int targetY = geo.y() + geo.height() - height() - 10;
        int targetX = geo.x() + (geo.width() - width()) / 2;
        QPoint startPos(targetX, targetY + 60);  // 从下方 60px 处滑入
        QPoint endPos(targetX, targetY);

        m_slideAnim = new QPropertyAnimation(this, "pos");
        m_slideAnim->setDuration(300);
        m_slideAnim->setStartValue(startPos);
        m_slideAnim->setEndValue(endPos);
        m_slideAnim->setEasingCurve(QEasingCurve::OutCubic);

        m_opacityEffect->setOpacity(0.0);
        QPropertyAnimation *fadeAnim = new QPropertyAnimation(m_opacityEffect, "opacity");
        fadeAnim->setDuration(300);
        fadeAnim->setStartValue(0.0);
        fadeAnim->setEndValue(1.0);
        fadeAnim->setEasingCurve(QEasingCurve::OutCubic);

        // Qt 的 DeleteWhenStopped 会在动画结束后自动删除
        // 使用 destroyed 信号清除悬空指针
        connect(m_slideAnim, &QObject::destroyed, this, [this]() {
            m_slideAnim = nullptr;
        });

        m_slideAnim->start(QAbstractAnimation::DeleteWhenStopped);
        fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
        break;
    }
    case DockState::Hidden: {
        m_isHidden = true;

        // 启动底部边缘轮询（仅在任务栏自动隐藏开启时生效）
        if (m_sysHelper && m_sysHelper->isTaskbarAutoHideEnabled()) {
            m_bottomEdgeTimer->start();
        }

        // 向下滑出 + 淡出
        QPoint currentPos = pos();
        QPoint endPos(currentPos.x(), currentPos.y() + 60);

        m_slideAnim = new QPropertyAnimation(this, "pos");
        m_slideAnim->setDuration(250);
        m_slideAnim->setStartValue(currentPos);
        m_slideAnim->setEndValue(endPos);
        m_slideAnim->setEasingCurve(QEasingCurve::InCubic);

        QPropertyAnimation *fadeAnim = new QPropertyAnimation(m_opacityEffect, "opacity");
        fadeAnim->setDuration(250);
        fadeAnim->setStartValue(1.0);
        fadeAnim->setEndValue(0.0);
        fadeAnim->setEasingCurve(QEasingCurve::InCubic);

        connect(m_slideAnim, &QObject::destroyed, this, [this]() {
            m_slideAnim = nullptr;
        });

        // hide() 在动画完成后通过 finished 信号触发
        connect(fadeAnim, &QPropertyAnimation::finished, this, [this]() {
            hide();
            m_opacityEffect->setOpacity(1.0);
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
    if (!m_sysHelper || !m_dockManager) return;

    // 更新所有运行中图标的窗口数量
    for (auto it = m_itemMap.begin(); it != m_itemMap.end(); ++it) {
        DockItem *item = it.value();
        if (!item->isRunning()) continue;

        // 从 appId 推导 WM_CLASS，优先用 execPath basename
        QString wmClass;
        QString execPath = item->execPath();
        if (!execPath.isEmpty()) {
            QFileInfo fi(execPath);
            wmClass = fi.baseName();
        } else {
            wmClass = item->appId();
            int dotIdx = wmClass.lastIndexOf('.');
            if (dotIdx >= 0) {
                wmClass = wmClass.mid(dotIdx + 1);
            }
        }

        int count = m_sysHelper->getWindowCount(wmClass);
        if (count > 0) {
            m_dockManager->updateWindowCount(item->appId(), count);
        }
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

        // 点击事件：弹出溢出菜单
        connect(m_overflowItem, &DockItem::clicked, this, [this](const QString &) {
            showOverflowPopup();
        });

        m_overflowItem->installEventFilter(this);
        m_overflowItem->show();
        relayoutItems();
        updatePosition();
        animateItemAdd(m_overflowItem);

    } else if (!hasOverflow && m_overflowItem) {
        // 移除抽屉图标
        auto it = m_itemMap.find("__overflow__");
        if (it != m_itemMap.end()) {
            m_itemMap.erase(it);
        }
        m_items.removeOne(m_overflowItem);
        animateItemRemove(m_overflowItem, "__overflow__");
        m_overflowItem = nullptr;
    }
}

void DockWindow::showOverflowPopup()
{
    if (!m_dockManager) return;

    // 关闭之前的弹出窗口
    if (m_overflowPopup) {
        m_overflowPopup->close();
        m_overflowPopup->deleteLater();
        m_overflowPopup = nullptr;
    }

    auto overflowItems = m_dockManager->overflowItems();
    if (overflowItems.isEmpty()) return;

    // 创建弹出窗口
    m_overflowPopup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_overflowPopup->setAttribute(Qt::WA_TranslucentBackground);

    int itemH = 40;
    int popupW = 200;
    int popupH = overflowItems.size() * itemH + 16;
    m_overflowPopup->setFixedSize(popupW, popupH);

    // 在抽屉图标上方显示
    QPoint globalPos = m_overflowItem->mapToGlobal(QPoint(0, 0));
    m_overflowPopup->move(globalPos.x() - popupW / 2 + m_overflowItem->width() / 2,
                          globalPos.y() - popupH - 8);

    // 绘制背景和列表项
    QVBoxLayout *layout = new QVBoxLayout(m_overflowPopup);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(2);

    for (const auto &data : overflowItems) {
        QPushButton *btn = new QPushButton(data.displayName, m_overflowPopup);
        btn->setFixedHeight(itemH - 4);
        btn->setStyleSheet(
            "QPushButton { background: rgba(60,60,60,200); color: white; "
            "border: none; border-radius: 6px; text-align: left; padding-left: 12px; font-size: 13px; }"
            "QPushButton:hover { background: rgba(80,80,80,220); }"
        );
        connect(btn, &QPushButton::clicked, this, [this, data]() {
            if (m_sysHelper) {
                QString wmClass;
                if (!data.execPath.isEmpty()) {
                    QFileInfo fi(data.execPath);
                    wmClass = fi.baseName();
                } else {
                    wmClass = data.appId;
                    int dotIdx = wmClass.lastIndexOf('.');
                    if (dotIdx >= 0) wmClass = wmClass.mid(dotIdx + 1);
                }
                m_sysHelper->activateWindow(wmClass);
            }
            if (m_overflowPopup) {
                m_overflowPopup->close();
            }
        });
        layout->addWidget(btn);
    }

    m_overflowPopup->show();
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

    // 推导 WM_CLASS，优先用 execPath basename（更精确）
    QString wmClass;
    QString execPath = item->execPath();
    if (!execPath.isEmpty()) {
        QFileInfo fi(execPath);
        wmClass = fi.baseName();
    } else {
        wmClass = item->appId();
        int dotIdx = wmClass.lastIndexOf('.');
        if (dotIdx >= 0) {
            wmClass = wmClass.mid(dotIdx + 1);
        }
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

// ─── 图标添加/移除动画 ──────────────────────────────────────

void DockWindow::animateItemAdd(DockItem *item)
{
    if (!item) return;

    // 初始状态：scale 0.0
    item->setVisualScale(0.0);

    QPropertyAnimation *anim = new QPropertyAnimation(item, "visualScale", this);
    anim->setDuration(200);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutBack);

    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
        relayoutItems();
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void DockWindow::animateItemRemove(DockItem *item, const QString &appId)
{
    if (!item) return;

    // 清理鱼眼动画
    auto animIt = m_fishEyeAnims.find(item);
    if (animIt != m_fishEyeAnims.end()) {
        animIt.value()->stop();
        animIt.value()->deleteLater();
        m_fishEyeAnims.erase(animIt);
    }

    QPropertyAnimation *anim = new QPropertyAnimation(item, "visualScale", this);
    anim->setDuration(150);
    anim->setStartValue(item->visualScale());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);

    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
        relayoutItems();
    });

    connect(anim, &QPropertyAnimation::finished, this, [this, item, appId]() {
        m_items.removeOne(item);
        item->deleteLater();
        relayoutItems();
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
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
    anim->setEasingCurve(QEasingCurve::OutBack);

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
        int dist = qAbs(i - hoveredIndex);
        qreal factor;
        switch (dist) {
        case 0:  factor = 1.5;  break;  // 悬停图标
        case 1:  factor = 1.25; break;  // 相邻图标
        case 2:  factor = 1.1;  break;  // 次相邻
        default: factor = 1.0;  break;  // 其余
        }
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

// ─── 窗口预览 ──────────────────────────────────────────────

void DockWindow::showWindowPreview(DockItem *item)
{
    if (!item || !m_sysHelper) return;

    // 推导 WM_CLASS
    QString wmClass;
    QString execPath = item->execPath();
    if (!execPath.isEmpty()) {
        QFileInfo fi(execPath);
        wmClass = fi.baseName();
    } else {
        wmClass = item->appId();
        int dotIdx = wmClass.lastIndexOf('.');
        if (dotIdx >= 0) wmClass = wmClass.mid(dotIdx + 1);
    }
    if (wmClass.isEmpty()) return;

    QString lowerClass = wmClass.toLower();

    // 枚举匹配进程的可见窗口
    struct WinInfo { HWND hwnd; QString title; };

    // 使用结构体传递匹配参数
    struct EnumCtx {
        QString targetClass;
        QList<WinInfo> windows;
    } ctx{lowerClass, {}};

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto *ctx = reinterpret_cast<EnumCtx *>(lParam);
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

        wchar_t title[256] = {0};
        GetWindowTextW(hwnd, title, 255);
        if (wcslen(title) < 2) return TRUE;

        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) return TRUE;

        wchar_t exeName[MAX_PATH] = {0};
        DWORD size = MAX_PATH;
        BOOL matched = FALSE;
        if (QueryFullProcessImageNameW(hProcess, 0, exeName, &size)) {
            QString exe = QFileInfo(QString::fromWCharArray(exeName)).baseName().toLower();
            if (exe == ctx->targetClass) matched = TRUE;
        }
        CloseHandle(hProcess);
        if (!matched) return TRUE;

        ctx->windows.append({hwnd, QString::fromWCharArray(title)});
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    if (ctx.windows.isEmpty()) return;

    // 限制最多显示 6 个窗口预览
    int maxPreviews = qMin(ctx.windows.size(), 6);

    // 创建预览弹出面板
    hideWindowPreview();
    m_previewPopup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_previewPopup->setAttribute(Qt::WA_TranslucentBackground);
    m_previewPopup->setAttribute(Qt::WA_ShowWithoutActivating);

    int thumbW = 160;
    int thumbH = 100;
    int spacing = 8;
    int padding = 10;
    int titleH  = 20;
    int popupW  = maxPreviews * thumbW + (maxPreviews - 1) * spacing + 2 * padding;
    int popupH  = thumbH + titleH + 2 * padding;
    m_previewPopup->setFixedSize(popupW, popupH);

    // 背景和布局
    m_previewPopup->setStyleSheet(
        "background: rgba(30, 30, 30, 220); border-radius: 10px;");

    for (int i = 0; i < maxPreviews; ++i) {
        const auto &wi = ctx.windows[i];

        // 捕获窗口缩略图（安全模式：先检测响应，再 PrintWindow）
        QPixmap thumb(thumbW, thumbH);
        thumb.fill(QColor(50, 50, 50));
        bool captured = false;

#ifdef Q_OS_WIN
        // 检测窗口是否响应，避免 PrintWindow 卡死事件循环
        DWORD_PTR result = 0;
        LRESULT checkOk = SendMessageTimeoutW(wi.hwnd, WM_NULL, 0, 0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 80, &result);
        if (checkOk) {
            RECT rect;
            if (GetWindowRect(wi.hwnd, &rect)) {
                int winW = rect.right - rect.left;
                int winH = rect.bottom - rect.top;
                if (winW > 0 && winH > 0) {
                    // 限制窗口尺寸，避免超大窗口的位图分配
                    int capW = qMin(winW, 640);
                    int capH = qMin(winH, 400);
                    HDC hdcWindow = GetDC(wi.hwnd);
                    HDC hdcMem = CreateCompatibleDC(hdcWindow);
                    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, capW, capH);
                    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBitmap);

                    // PW_CLIENTONLY 比 PW_RENDERFULLCONTENT 更安全，不会触发 DWM 重绘
                    if (PrintWindow(wi.hwnd, hdcMem, PW_CLIENTONLY)) {
                        BITMAPINFO bmi = {};
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = capW;
                        bmi.bmiHeader.biHeight = -capH;
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 32;
                        bmi.bmiHeader.biCompression = BI_RGB;

                        QImage img(capW, capH, QImage::Format_ARGB32);
                        GetDIBits(hdcMem, hBitmap, 0, capH, img.bits(), &bmi, DIB_RGB_COLORS);
                        thumb = QPixmap::fromImage(img).scaled(thumbW, thumbH,
                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        captured = true;

                        // 居中放置缩略图
                        if (thumb.width() < thumbW || thumb.height() < thumbH) {
                            QPixmap centered(thumbW, thumbH);
                            centered.fill(QColor(50, 50, 50));
                            QPainter pp(&centered);
                            pp.drawPixmap((thumbW - thumb.width()) / 2,
                                          (thumbH - thumb.height()) / 2, thumb);
                            pp.end();
                            thumb = centered;
                        }
                    }

                    SelectObject(hdcMem, hOld);
                    DeleteObject(hBitmap);
                    DeleteDC(hdcMem);
                    ReleaseDC(wi.hwnd, hdcWindow);
                }
            }
        }

        // PrintWindow 失败：回退到应用图标
        if (!captured && !item->execPath().isEmpty()) {
            QFileIconProvider provider;
            QIcon appIcon = provider.icon(QFileInfo(item->execPath()));
            if (!appIcon.isNull()) {
                QPixmap iconPix = appIcon.pixmap(48, 48);
                QPainter pp(&thumb);
                pp.drawPixmap((thumbW - 48) / 2, (thumbH - 48) / 2, iconPix);
                pp.end();
            }
        }
#endif

        // 缩略图标签
        QLabel *thumbLabel = new QLabel(m_previewPopup);
        thumbLabel->setPixmap(thumb);
        thumbLabel->setFixedSize(thumbW, thumbH);
        thumbLabel->setStyleSheet("background: transparent;");
        thumbLabel->move(padding + i * (thumbW + spacing), padding);

        // 窗口标题标签
        QLabel *titleLabel = new QLabel(m_previewPopup);
        QString elidedTitle = wi.title;
        if (elidedTitle.length() > 20) {
            elidedTitle = elidedTitle.left(19) + "...";
        }
        titleLabel->setText(elidedTitle);
        titleLabel->setFixedSize(thumbW, titleH);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(
            "background: transparent; color: white; font-size: 11px;");
        titleLabel->move(padding + i * (thumbW + spacing), padding + thumbH + 4);

        // 点击缩略图切换到对应窗口
        HWND targetHwnd = wi.hwnd;
        thumbLabel->setCursor(Qt::PointingHandCursor);
        // 使用 eventFilter 处理点击（在 thumbLabel 上安装事件过滤器）
        thumbLabel->installEventFilter(this);
        // 存储 hwnd 到 property
        thumbLabel->setProperty("previewHwnd", reinterpret_cast<qintptr>(targetHwnd));
    }

    // 定位弹出面板：在 dock 窗口上方居中
    QPoint itemCenter = item->mapToGlobal(
        QPoint(item->width() / 2, 0));
    int popupX = itemCenter.x() - popupW / 2;
    int popupY = mapToGlobal(QPoint(0, 0)).y() - popupH - 12;

    // 确保不超出屏幕
    QScreen *screen = QGuiApplication::screenAt(itemCenter);
    if (screen) {
        QRect geo = screen->availableGeometry();
        popupX = qBound(geo.left() + 8, popupX, geo.right() - popupW - 8);
        if (popupY < geo.top()) popupY = geo.top() + 8;
    }

    m_previewPopup->move(popupX, popupY);
    m_previewPopup->show();
}

void DockWindow::hideWindowPreview()
{
    if (m_previewPopup) {
        m_previewPopup->close();
        m_previewPopup->deleteLater();
        m_previewPopup = nullptr;
    }
    m_previewItem = nullptr;
}
