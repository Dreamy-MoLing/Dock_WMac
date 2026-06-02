/**
 * @file Application.cpp
 * @brief Application 类实现
 *
 * 将原 main.cpp 的 ~100 行初始化逻辑封装到独立的方法中：
 *   setupLogging()       — 日志系统初始化
 *   checkSingleInstance() — QSharedMemory 单实例检测
 *   setupIPC()           — IPCHelper 启动（含脚本路径回退）
 *   loadPinnedItems()    — 从配置恢复固定项列表
 *   connectPersistence() — 固定项变更自动保存
 */

#include "core/Application.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "core/ConfigManager.h"
#include "core/DockManager.h"
#include "core/IPCHelper.h"
#include "core/Logger.h"
#include "core/ProcessMonitor.h"
#include "core/SysHelper.h"
#include "core/Types.h"
#include "ui/DockWindow.h"

// ─── 构造 / 析构 ────────────────────────────────────────────

Application::Application(int &argc, char **argv)
    : QObject(nullptr)
    , m_argc(argc)
    , m_argv(argv)
    , m_singleInstance(QStringLiteral("Dock_WMac_Instance"))
{
}

Application::~Application()
{
    // run() 已在 QApplication 退出前清理，此处做安全兜底
    delete m_processMonitor;
    delete m_dockWindow;
    delete m_dockManager;
    delete m_sysHelper;
    delete m_ipcHelper;
    delete m_config;
}

// ─── 公共入口 ────────────────────────────────────────────────

int Application::run()
{
    // QApplication 必须最先创建、最后销毁
    QApplication app(m_argc, m_argv);
    app.setApplicationName(QStringLiteral("Dock_WMac"));
    app.setApplicationVersion(QStringLiteral("0.2.0"));

    setupLogging();

    if (!checkSingleInstance())
        return 0;

    setupIPC();

    // 核心层初始化
    m_config = new ConfigManager(this);
    m_config->setIPCHelper(m_ipcHelper);
    m_config->load();

    m_sysHelper = new SysHelper(this);
    m_sysHelper->setIPCHelper(m_ipcHelper);

    m_dockManager = new DockManager(this);
    loadPinnedItems();

    // UI 层初始化
    m_dockWindow = new DockWindow();
    m_dockWindow->setDockManager(m_dockManager);
    m_dockWindow->setSysHelper(m_sysHelper);

    // 触发 DockManager 信号连接
    m_dockManager->initialize(m_sysHelper);

    connectPersistence();

    // 创建 ProcessMonitor 并连接 DockWindow
    m_processMonitor = new ProcessMonitor(m_ipcHelper, this);
    m_processMonitor->registerApps(m_dockManager->pinnedItems());
    connect(m_processMonitor, &ProcessMonitor::appRunningStateChanged,
            m_dockWindow, &DockWindow::onAppRunningStateChanged);
    connect(m_processMonitor, &ProcessMonitor::newRunningAppDetected,
            m_dockWindow, &DockWindow::onNewRunningAppDetected);
    connect(m_processMonitor, &ProcessMonitor::runningAppExited,
            m_dockWindow, &DockWindow::onRunningAppExited);
    m_processMonitor->start();

    m_dockWindow->show();
    m_sysHelper->installWindowHook();

    // 隐藏原生任务栏（Windows only）
    m_sysHelper->hideNativeTaskbar();

    // 开机自启
    if (m_config->get(QStringLiteral("startWithSystem"), false).toBool()) {
        m_sysHelper->setAutoStart(true);
    }

    int exitCode = app.exec();

    // 恢复原生任务栏（Windows only）
    m_sysHelper->restoreNativeTaskbar();

    // ─── 清理：在 QApplication 销毁前删除所有 QObject 子对象 ───
    delete m_processMonitor;    m_processMonitor = nullptr;
    delete m_dockWindow;    m_dockWindow = nullptr;
    delete m_dockManager;   m_dockManager = nullptr;
    delete m_sysHelper;     m_sysHelper = nullptr;
    delete m_ipcHelper;     m_ipcHelper = nullptr;
    delete m_config;        m_config = nullptr;

    return exitCode;
}

// ─── 私有方法 ────────────────────────────────────────────────

void Application::setupLogging()
{
    Logger::init();
}

bool Application::checkSingleInstance()
{
    if (!m_singleInstance.create(1)) {
        m_singleInstance.attach();
        m_singleInstance.detach();
        if (!m_singleInstance.create(1)) {
            return false;   // 已有实例在运行
        }
    }
    return true;
}

void Application::setupIPC()
{
    m_ipcHelper = new IPCHelper(this);

    // 优先：安装目录 ../scripts/helper.py
    QString scriptPath = QCoreApplication::applicationDirPath()
                         + QStringLiteral("/../scripts/helper.py");

    // 回退：CMake 构建时二进制在 build/，脚本在项目根
    if (!QFileInfo::exists(scriptPath)) {
        QString altPath = QCoreApplication::applicationDirPath()
                          + QStringLiteral("/../../scripts/helper.py");
        if (QFileInfo::exists(altPath)) {
            scriptPath = altPath;
        }
    }

    m_ipcHelper->start(scriptPath);
}

void Application::loadPinnedItems()
{
    QVariantList savedPinned = m_config->get(
        QStringLiteral("pinned_apps"), QVariantList()).toList();

    QList<DockItemData> pinnedItems;
    pinnedItems.reserve(savedPinned.size());

    for (const QVariant &v : savedPinned) {
        QVariantMap entry = v.toMap();
        DockItemData item;
        item.appId       = entry.value(QStringLiteral("appId")).toString();
        item.displayName = entry.value(QStringLiteral("displayName")).toString();
        item.iconPath    = entry.value(QStringLiteral("iconPath")).toString();
        item.execPath    = entry.value(QStringLiteral("execPath")).toString();
        item.isRunning   = false;
        item.badgeCount  = 0;
        pinnedItems.append(item);
    }

    m_dockManager->setPinnedItems(pinnedItems);
}

void Application::connectPersistence()
{
    // 固定项变更时自动保存到配置
    connect(m_dockManager, &DockManager::pinnedItemsChanged,
        this, [this](const QList<DockItemData> &items) {
            QVariantList list;
            list.reserve(items.size());
            for (const auto &item : items) {
                QVariantMap entry;
                entry[QStringLiteral("appId")]       = item.appId;
                entry[QStringLiteral("displayName")] = item.displayName;
                entry[QStringLiteral("iconPath")]    = item.iconPath;
                entry[QStringLiteral("execPath")]    = item.execPath;
                list.append(entry);
            }
            m_config->set(QStringLiteral("pinned_apps"), list);
        });
}
