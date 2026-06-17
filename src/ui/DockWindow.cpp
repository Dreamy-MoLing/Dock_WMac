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
#include <QScreen>
#include <QGuiApplication>
#include <QPropertyAnimation>
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
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QUrl>

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

    // 窗口数量定时更新（每 10 秒兜底）
    m_windowCountTimer->setInterval(10000);
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

    // 通知状态捕获：窗口变为可见时触发 DockItem 绿色光点
    connect(m_sysHelper, &SysHelper::windowShowOccurred, this,
        [this](DWORD pid) {
            if (!m_windowCache) return;
            QString appId = m_windowCache->getAppIdForPid(pid);
            if (appId.isEmpty()) return;
            auto it = m_itemMap.find(appId);
            if (it != m_itemMap.end()) {
                it.value()->triggerInteractionIndicator();
            }
        });
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

// ─── 绘制 & 毛玻璃 & 主题 → DockWindow_theme.cpp ───────────
// ─── 显示/隐藏过渡动画 → DockWindow_transition.cpp ──────────
// ─── 图标生命周期管理 → DockWindow_itemmanager.cpp ──────────

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


