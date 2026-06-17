/**
 * @file DockWindow_position.cpp
 * @brief DockWindow screen positioning, DPI, and native setting events.
 */

#include "ui/DockWindow.h"

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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
        QRect geo = targetScreen->geometry();
        int w = width();
        int h = height();
        move(geo.x() + (geo.width() - w) / 2,
             geo.y() + geo.height() - h - kBaseSpacing);
    }
}

void DockWindow::updateDpiScale()
{
    QScreen *screen = windowHandle() ? windowHandle()->screen() : QGuiApplication::primaryScreen();
    if (!screen) return;

    qreal dpi = screen->logicalDotsPerInch();
    qreal scale = qBound(0.5, dpi / 96.0, 3.0);
    m_baseIconSize = static_cast<int>(48 * scale);

    qInfo() << "DPI:" << dpi << "scale:" << scale << "iconSize:" << m_baseIconSize;
}

bool DockWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);

#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_SETTINGCHANGE) {
            QMetaObject::invokeMethod(this, &DockWindow::updateTheme, Qt::QueuedConnection);
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}
