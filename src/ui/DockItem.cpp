/**
 * @file DockItem.cpp
 * @brief 单个 Dock 图标组件实现
 *
 * 通过 visualScale 属性实现 macOS 风格鱼眼动画。
 * 锚点为底部中心，图标向上生长，widget 尺寸随缩放变化。
 */

#include "ui/DockItem.h"
#include "core/IconProvider.h"
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QApplication>
#include <QMimeData>

#include <QFileIconProvider>

DockItem::DockItem(const QString &appId, const QString &iconPath,
                   const QString &displayName, QWidget *parent)
    : QWidget(parent)
    , m_appId(appId)
    , m_displayName(displayName)
    , m_isRunning(false)
    , m_badgeCount(0)
    , m_windowCount(1)
    , m_isHovered(false)
    , m_visualScale(1.0)
    , m_dragStartPos(0, 0)
{
    setFixedSize(48, 48);
    setCursor(Qt::PointingHandCursor);
    setToolTip(displayName);
    setMouseTracking(true);
    setAcceptDrops(true);

    // 通知闪烁定时器（500ms 交替）
    m_flashTimer = new QTimer(this);
    m_flashTimer->setInterval(500);
    connect(m_flashTimer, &QTimer::timeout, this, &DockItem::onFlashTimerTick);

    // 渐隐定时器（50ms 步进）
    m_fadeTimer = new QTimer(this);
    m_fadeTimer->setInterval(kFadeInterval);
    connect(m_fadeTimer, &QTimer::timeout, this, &DockItem::onFadeTimerTick);

    // 加载图标（IconProvider 内部处理 4 级回退 + 归一化到 64×64）
    m_icon = IconProvider::loadIcon(iconPath, displayName);
}

void DockItem::setRunning(bool running)
{
    if (m_isRunning == running) return;
    m_isRunning = running;
    // 应用启动时显示注意点（通知优先）
    if (running && !m_hasNotifications) {
        m_fadeTimer->stop();
        m_statusOpacity = 1.0;
    } else if (!running) {
        m_fadeTimer->stop();
        m_statusOpacity = 0.0;
    }
    update();
}

void DockItem::setBadgeCount(int count)
{
    m_badgeCount = count;
    update();
}

void DockItem::setWindowCount(int count)
{
    if (m_windowCount == count) return;
    int oldCount = m_windowCount;
    m_windowCount = count;

    // 新窗口出现 → 重新触发注意点（通知优先）
    if (count > oldCount && m_isRunning && !m_hasNotifications) {
        m_fadeTimer->stop();
        m_statusOpacity = 1.0;
    }
    update();
}

void DockItem::setForegroundActive(bool active)
{
    if (m_isForegroundActive == active) return;
    m_isForegroundActive = active;
    update();
}

void DockItem::setHasNotifications(bool has)
{
    if (m_hasNotifications == has) return;
    m_hasNotifications = has;
    if (has) {
        m_flashTimer->start();
    } else {
        m_flashTimer->stop();
        m_flashVisible = true;
    }
    update();
}

void DockItem::triggerInteractionIndicator()
{
    // 仅当窗口数 <= 1 且应用在运行时才显示指示器
    if (m_windowCount > 1 || !m_isRunning) return;

    // 通知闪烁期间不打断（通知优先级高于交互反馈）
    if (m_hasNotifications) return;

    // 停止之前的渐隐
    m_fadeTimer->stop();
    m_statusOpacity = 1.0;
    update();

    // 3 秒后开始渐隐
    QTimer::singleShot(kSolidDelay, this, [this]() {
        if (m_fadeTimer) m_fadeTimer->start();
    });
}

void DockItem::onFlashTimerTick()
{
    m_flashVisible = !m_flashVisible;
    update();
}

void DockItem::onFadeTimerTick()
{
    qreal step = static_cast<qreal>(kFadeInterval) / kFadeDuration;
    m_statusOpacity = qMax(0.0, m_statusOpacity - step);
    if (qFuzzyIsNull(m_statusOpacity)) {
        m_statusOpacity = 0.0;
        m_fadeTimer->stop();
    }
    update();
}

void DockItem::setVisualScale(qreal scale)
{
    if (qFuzzyCompare(m_visualScale, scale)) return;
    m_visualScale = scale;
    // widget 尺寸随缩放变化
    int sz = static_cast<int>(baseSize() * scale);
    setFixedSize(sz, sz);
    update();
}

void DockItem::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);

    int drawSize = qMin(width(), height()) - 4;
    int offset = (width() - drawSize) / 2;

    // 绘制图标
    QPixmap scaled = m_icon.scaled(drawSize, drawSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter.drawPixmap(offset, offset, scaled);

    // 图标倒影（绘制在图标下方，半透明翻转，高度为图标的 30%）
    int reflectionH = static_cast<int>(drawSize * 0.3);
    int reflectionTop = offset + drawSize;
    if (reflectionTop + reflectionH <= height()) {
        painter.save();
        painter.setOpacity(0.15);
        // 翻转绘制
        painter.translate(0, 2 * reflectionTop + reflectionH);
        painter.scale(1.0, -1.0);
        QRectF srcRect(0, 0, scaled.width(), scaled.height());
        QRectF dstRect(offset, reflectionTop, drawSize, reflectionH);
        painter.drawPixmap(dstRect, scaled, srcRect);
        painter.restore();
    }

    // 按下效果（下沉 2px）
    if (m_isHovered && QApplication::mouseButtons() & Qt::LeftButton) {
        painter.translate(0, 2);
    }

    // 前台激活指示器（半透明白色渐变覆盖）
    if (m_isForegroundActive) {
        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0.0, QColor(255, 255, 255, 40));   // 顶部更亮
        gradient.setColorAt(1.0, QColor(255, 255, 255, 10));   // 底部渐弱
        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 10, 10);
    }

    // 状态指示灯
    // - 单窗口 (windowCount <= 1)：光晕小点（内实心 3.5px + 外半透明光晕 7px）
    // - 多窗口 (windowCount > 1)：底部小横条（8×3px）
    // - 有通知：闪烁
    // - 交互后/新窗口：渐隐（statusOpacity > 0）
    if (m_isRunning) {
        bool shouldDraw = false;
        QColor indicatorColor;

        if (m_hasNotifications) {
            shouldDraw = m_flashVisible;
            indicatorColor = QColor(120, 210, 100);
        } else if (m_statusOpacity > 0.0) {
            shouldDraw = true;
            int alpha = static_cast<int>(m_statusOpacity * 255);
            indicatorColor = QColor(120, 210, 100, alpha);
        }

        if (shouldDraw) {
            painter.setPen(Qt::NoPen);

            if (m_windowCount <= 1) {
                // ── 方案 C: 光晕小点 ──
                // 外圈半透明光晕 (半径 3.5)
                QColor glowColor = indicatorColor;
                glowColor.setAlpha(static_cast<int>(indicatorColor.alpha() * 0.35));
                painter.setBrush(glowColor);
                painter.drawEllipse(QPointF(width() / 2.0, height() - 4), 3.5, 3.5);
                // 内实心小点 (半径 1.75)
                painter.setBrush(indicatorColor);
                painter.drawEllipse(QPointF(width() / 2.0, height() - 4), 1.75, 1.75);
            } else {
                // ── 方案 B: 小横条 ──
                painter.setBrush(indicatorColor);
                painter.drawRoundedRect(QRectF(width() / 2.0 - 4, height() - 5, 8, 3), 1.5, 1.5);
            }
        }
    }

    // 未读徽章（右上角红色圆 + 数字）
    if (m_badgeCount > 0) {
        QColor badgeColor(255, 59, 48);
        painter.setBrush(badgeColor);
        painter.setPen(Qt::NoPen);

        int badgeSize = 16;
        int bx = width() - badgeSize;
        painter.drawEllipse(bx, 0, badgeSize, badgeSize);

        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        QString text = (m_badgeCount > 99) ? "99+" : QString::number(m_badgeCount);
        painter.drawText(QRect(bx, 0, badgeSize, badgeSize), Qt::AlignCenter, text);
    }

}

void DockItem::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_isHovered = true;
    emit hoverEntered(-1);
    update();
}

void DockItem::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_isHovered = false;
    emit hoverLeft();
    update();
}

void DockItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
        emit clicked(m_appId);
    }
}

void DockItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) return;
    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) return;

    QMimeData *mimeData = new QMimeData;
    mimeData->setText(m_appId);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(m_icon.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    drag->exec(Qt::MoveAction);
}

void DockItem::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
        m_isHovered = true;
        update();
    }
}

void DockItem::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void DockItem::dropEvent(QDropEvent *event)
{
    m_isHovered = false;
    update();

    if (m_execPath.isEmpty()) return;

    QStringList filePaths;
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                filePaths << url.toLocalFile();
            }
        }
    } else if (event->mimeData()->hasText()) {
        filePaths << event->mimeData()->text();
    }

    if (filePaths.isEmpty()) return;

    // 用 QDir::toNativeSeparators 处理路径分隔符；不分割空格
    QString nativePath = QDir::toNativeSeparators(m_execPath);
    QProcess::startDetached(nativePath, filePaths);
    event->acceptProposedAction();
}

void DockItem::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    QAction *launchAction = menu.addAction("启动");
    connect(launchAction, &QAction::triggered, this, [this]() {
        if (!m_execPath.isEmpty()) {
            QString nativePath = QDir::toNativeSeparators(m_execPath);
            QProcess::startDetached(nativePath, QStringList());
        }
    });

    // 固定/取消固定
    bool isPinned = false;
    // 通过 parent（DockWindow）判断是否为固定项
    QMetaObject::invokeMethod(this, [this, &isPinned]() {
        // 查询父窗口的 dockManager
        QWidget *parent = parentWidget();
        if (!parent) return;
        // 使用属性查询（避免依赖 DockManager 头文件）
        QVariant ret;
        QMetaObject::invokeMethod(parent, "isItemPinned",
            Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, ret),
            Q_ARG(QVariant, m_appId));
        if (ret.isValid()) isPinned = ret.toBool();
    }, Qt::DirectConnection);

    if (isPinned) {
        QAction *removeAction = menu.addAction("从 Dock 移除");
        connect(removeAction, &QAction::triggered, this, [this]() {
            emit pinRequested(m_appId, false);
        });
    } else {
        QAction *pinAction = menu.addAction("固定到 Dock");
        connect(pinAction, &QAction::triggered, this, [this]() {
            emit pinRequested(m_appId, true);
        });
    }

    menu.addSeparator();

    QAction *quitAction = menu.addAction("退出 Dock");
    connect(quitAction, &QAction::triggered, this, []() {
        qApp->quit();
    });

    menu.exec(event->globalPos());
}
