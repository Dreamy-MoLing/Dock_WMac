/**
 * @file DockItem.cpp
 * @brief 单个 Dock 图标组件实现
 *
 * 通过 visualScale 属性实现 macOS 风格鱼眼动画。
 * 锚点为底部中心，图标向上生长，widget 尺寸随缩放变化。
 */

#include "ui/DockItem.h"
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
#include <QDebug>
#include <QEnterEvent>
#include <QApplication>
#include <QMimeData>

DockItem::DockItem(const QString &appId, const QString &iconPath,
                   const QString &displayName, QWidget *parent)
    : QWidget(parent)
    , m_appId(appId)
    , m_displayName(displayName)
    , m_isRunning(false)
    , m_badgeCount(0)
    , m_isHovered(false)
    , m_visualScale(1.0)
    , m_dragStartPos(0, 0)
{
    setFixedSize(48, 48);
    setCursor(Qt::PointingHandCursor);
    setToolTip(displayName);
    setMouseTracking(true);
    setAcceptDrops(true);

    // 加载图标：优先绝对路径，其次主题图标，最后占位符
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        m_icon.load(iconPath);
    }
    if (m_icon.isNull() && !iconPath.isEmpty()) {
        QIcon themedIcon = QIcon::fromTheme(iconPath);
        if (!themedIcon.isNull()) {
            m_icon = themedIcon.pixmap(64, 64);
        }
    }
    if (m_icon.isNull()) {
        m_icon = QPixmap(64, 64);
        m_icon.fill(QColor(80, 80, 80));
        QPainter p(&m_icon);
        p.setPen(Qt::white);
        QFont font = p.font();
        font.setPixelSize(32);
        font.setBold(true);
        p.setFont(font);
        p.drawText(m_icon.rect(), Qt::AlignCenter,
                   displayName.isEmpty() ? "?" : displayName.left(1).toUpper());
    }
}

void DockItem::setRunning(bool running)
{
    m_isRunning = running;
    update();
}

void DockItem::setBadgeCount(int count)
{
    m_badgeCount = count;
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

    // 按下效果（下沉 2px）
    if (m_isHovered && QApplication::mouseButtons() & Qt::LeftButton) {
        painter.translate(0, 2);
    }

    // 运行指示灯（底部小圆点）
    if (m_isRunning) {
        painter.setBrush(QColor(180, 220, 80));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(width() / 2.0, height() - 4), 3, 3);
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

    QStringList parts = m_execPath.split(' ');
    QString program = parts.takeFirst();
    QStringList args = parts;
    args.removeAll("%f");
    args.removeAll("%F");
    args.removeAll("%u");
    args.removeAll("%U");
    args.append(filePaths);

    QProcess::startDetached(program, args);
    event->acceptProposedAction();
}

void DockItem::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    QAction *launchAction = menu.addAction("启动");
    connect(launchAction, &QAction::triggered, this, [this]() {
        if (!m_execPath.isEmpty()) {
            QStringList parts = m_execPath.split(' ');
            QString program = parts.takeFirst();
            QProcess::startDetached(program, parts);
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
