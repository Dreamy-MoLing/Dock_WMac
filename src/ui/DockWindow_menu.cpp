/**
 * @file DockWindow_menu.cpp
 * @brief DockWindow background context menu handling.
 */

#include "ui/DockWindow.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QMenu>
#include <QProcess>
#include <QUrl>

void DockWindow::contextMenuEvent(QContextMenuEvent *event)
{
    int index = itemAtPos(event->pos().x(), event->pos().y());
    if (index >= 0) {
        return;
    }

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
