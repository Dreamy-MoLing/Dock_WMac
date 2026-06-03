/**
 * @file Application.cpp
 * @brief Application 类实现
 *
 * 将原 main.cpp 的 ~100 行初始化逻辑封装到独立的方法中：
 *   setupLogging()       — 日志系统初始化
 *   checkSingleInstance() — QSharedMemory 单实例检测
 *   loadPinnedItems()    — 从配置恢复固定项列表
 *   connectPersistence() — 固定项变更自动保存
 */

#include "core/Application.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <csignal>

#include "core/ConfigManager.h"
#include "core/DockManager.h"
#include "core/Logger.h"
#include "core/ProcessMonitor.h"
#include "core/SysHelper.h"
#include "core/Types.h"
#include "ui/DockWindow.h"

// ─── 信号处理：异常退出时恢复原生任务栏 ────────────────────

static SysHelper *s_sysHelperForSignal = nullptr;

static void signalRestoreTaskbar(int)
{
    if (s_sysHelperForSignal) {
        s_sysHelperForSignal->restoreNativeTaskbar();
    }
    _exit(1);
}

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
    delete m_config;
}

// ─── 公共入口 ────────────────────────────────────────────────

int Application::run()
{
    // QApplication 必须最先创建、最后销毁
    QApplication app(m_argc, m_argv);
    app.setApplicationName(QStringLiteral("Dock_WMac"));
    app.setApplicationVersion(QStringLiteral("0.2.1b"));

    setupLogging();

    if (!checkSingleInstance())
        return 0;

    // 核心层初始化
    m_config = new ConfigManager(this);
    m_config->load();

    m_sysHelper = new SysHelper(this);

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
    m_processMonitor = new ProcessMonitor(this);
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

    // 注册信号处理，确保异常退出时恢复任务栏
    s_sysHelperForSignal = m_sysHelper;
    std::signal(SIGINT, signalRestoreTaskbar);
    std::signal(SIGTERM, signalRestoreTaskbar);
    std::signal(SIGABRT, signalRestoreTaskbar);
    atexit([]() {
        if (s_sysHelperForSignal) {
            s_sysHelperForSignal->restoreNativeTaskbar();
        }
    });

    // 任务栏隐藏后 availableGeometry 变化，延迟重新定位 Dock
    QTimer::singleShot(100, m_dockWindow, &DockWindow::requestUpdatePosition);

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
