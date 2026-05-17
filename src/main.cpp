/**
 * @file main.cpp
 * @brief 应用程序入口
 *
 * 初始化 QApplication、单实例检测、加载核心管理器并显示 Dock 窗口。
 * 连接 ConfigManager → DockManager → DockWindow 三层架构。
 */

#include <QApplication>
#include <QSharedMemory>
#include "core/DockManager.h"
#include "core/ConfigManager.h"
#include "core/SysHelper.h"
#include "core/Logger.h"
#include "ui/DockWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Dock_WMac");
    app.setApplicationVersion("0.1.0");

    // 初始化日志系统
    Logger::init();

    // 单实例检测
    QSharedMemory singleInstance("Dock_WMac_Instance");
    if (!singleInstance.create(1)) {
        return 0;  // 已有实例运行
    }

    // 初始化核心层
    ConfigManager config;
    config.load();

    SysHelper sysHelper;
    DockManager dockManager;
    dockManager.initialize(&sysHelper);

    // 初始化 UI 层并关联核心层
    DockWindow dockWindow;
    dockWindow.setDockManager(&dockManager);
    dockWindow.show();

    // 安装窗口钩子（监听前台窗口状态变化）
    sysHelper.installWindowHook();

    // 应用开机自启设置
    if (config.get("startWithSystem", false).toBool()) {
        sysHelper.setAutoStart(true);
    }

    return app.exec();
}
