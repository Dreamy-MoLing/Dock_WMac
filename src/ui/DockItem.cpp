/**
 * @file DockItem.cpp
 * @brief 单个 Dock 图标组件实现
 *
 * 负责图标的绘制（缩放/运行指示灯/未读徽章）、
 * 鼠标悬浮放大效果、点击启动应用及右键菜单。
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
    , m_scaleFactor(1.0)
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
        // 占位符：灰色方块 + 首字母
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

void DockItem::setScaleFactor(qreal factor)
{
    if (qFuzzyCompare(m_scaleFactor, factor)) return;
    m_scaleFactor = factor;
    int baseSize = 48;
    int newSize = static_cast<int>(baseSize * factor);
    setFixedSize(newSize, newSize);
    m_scaledIcon = QPixmap();  // 清除缓存，下次绘制时重新缩放
    update();
}

void DockItem::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);

    int size = qMin(width(), height()) - 4;
    int offset = (width() - size) / 2;

    // 绘制图标（缓存缩放结果，尺寸变化时才重新缩放）
    QSize targetSize(size, size);
    if (m_scaledIcon.isNull() || m_scaledSize != targetSize) {
        m_scaledIcon = m_icon.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_scaledSize = targetSize;
    }
    painter.drawPixmap(offset, offset, m_scaledIcon);

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

void DockItem::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_isHovered = true;
    emit hoverEntered(-1);  // 由 DockWindow 处理索引
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

    // 开始拖拽
    QMimeData *mimeData = new QMimeData;
    mimeData->setText(m_appId);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(m_icon.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    drag->exec(Qt::MoveAction);
}

void DockItem::dragEnterEvent(QDragEnterEvent *event)
{
    // 接受文件拖入（URL 列表或文本路径）
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

    // 提取文件路径列表
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

    // 用应用打开文件
    QStringList parts = m_execPath.split(' ');
    QString program = parts.takeFirst();
    // 移除已有的参数（如 %f），用实际文件路径替换
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

    QAction *removeAction = menu.addAction("从 Dock 移除");
    connect(removeAction, &QAction::triggered, this, [this]() {
        // 发送信号，由 DockManager 处理
        emit rightClicked(m_appId);
    });

    menu.addSeparator();

    QAction *quitAction = menu.addAction("退出 Dock");
    connect(quitAction, &QAction::triggered, this, []() {
        qApp->quit();
    });

    menu.exec(event->globalPos());
}
