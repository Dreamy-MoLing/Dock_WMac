/**
 * @file Application.h
 * @brief 应用程序生命周期管理类
 *
 * 封装 main.cpp 中的所有初始化逻辑：单实例检测、IPC 启动、
 * 配置加载、DockManager/DockWindow 组装、信号连接等。
 * main() 仅需创建 Application 并调用 run()。
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QObject>
#include <QSharedMemory>

class ConfigManager;
class SysHelper;
class IPCHelper;
class DockManager;
class DockWindow;
class ProcessMonitor;

class Application : public QObject {
    Q_OBJECT
public:
    explicit Application(int &argc, char **argv);
    ~Application();

    /** @brief 初始化所有组件并进入事件循环，返回退出码 */
    int run();

private:
    void setupLogging();
    bool checkSingleInstance();
    void setupIPC();
    void loadPinnedItems();
    void connectPersistence();

    // 构造时保存，run() 中传给 QApplication
    int &m_argc;
    char **m_argv;

    // 全生命周期对象（RAII）
    QSharedMemory m_singleInstance;

    // run() 中按序创建，退出前按逆序销毁
    ConfigManager *m_config = nullptr;
    IPCHelper *m_ipcHelper = nullptr;
    SysHelper *m_sysHelper = nullptr;
    DockManager *m_dockManager = nullptr;
    DockWindow *m_dockWindow = nullptr;
    ProcessMonitor *m_processMonitor = nullptr;
};

#endif // APPLICATION_H
