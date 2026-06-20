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
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <QUrl>

#include "core/ConfigManager.h"
#include "core/SysHelper.h"

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

    QAction *hideTaskbarAction = menu.addAction("隐藏原生任务栏（实验性）");
    hideTaskbarAction->setCheckable(true);
    hideTaskbarAction->setChecked(
        m_config && m_config->get(QStringLiteral("hideNativeTaskbar"), false).toBool());
    hideTaskbarAction->setEnabled(m_config && m_sysHelper);
    connect(hideTaskbarAction, &QAction::triggered, this, [this](bool enabled) {
        if (!m_config || !m_sysHelper) return;

        if (enabled) {
            const auto answer = QMessageBox::warning(
                this,
                QStringLiteral("隐藏原生任务栏"),
                QStringLiteral("Dock 尚未提供开始菜单、通知区域、时钟和快速设置。\n"
                               "隐藏后可从 Dock 右键菜单恢复原生任务栏。是否继续？"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (answer != QMessageBox::Yes) return;
            m_sysHelper->hideNativeTaskbar();
        } else {
            m_sysHelper->restoreNativeTaskbar();
        }

        m_config->set(QStringLiteral("hideNativeTaskbar"), enabled);
        QTimer::singleShot(100, this, &DockWindow::requestUpdatePosition);
    });

    menu.addSeparator();

    QAction *quitAction = menu.addAction("退出 Dock");
    connect(quitAction, &QAction::triggered, this, []() {
        qApp->quit();
    });

    menu.exec(event->globalPos());
}
