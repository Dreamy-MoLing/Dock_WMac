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

#include "core/AppIdHelper.h"
#include "core/ConfigManager.h"
#include "core/DockManager.h"
#include "core/Logger.h"
#include "core/PathManager.h"
#include "core/PinnedItemsReader.h"
#include <QJsonDocument>
#include <QJsonArray>
#include "core/ProcessMonitor.h"
#include "core/SysHelper.h"
#include "core/WindowCache.h"
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
    delete m_windowCache;
    delete m_sysHelper;
    delete m_config;
}

// ─── 公共入口 ────────────────────────────────────────────────

int Application::run()
{
    // QApplication 必须最先创建、最后销毁
    QApplication app(m_argc, m_argv);
    app.setApplicationName(QStringLiteral("Dock_WMac"));
    app.setApplicationVersion(QStringLiteral("0.2.5"));

    setupLogging();

    if (!checkSingleInstance())
        return 0;

    // 核心层初始化
    m_config = new ConfigManager(this);
    m_config->load();

    m_sysHelper = new SysHelper(this);
    m_windowCache = new WindowCache(this);

    m_dockManager = new DockManager(this);
    m_dockManager->setMaxItems(m_config->get(QStringLiteral("maxItems"), 16).toInt());
    m_dockManager->setAutoHideEnabled(m_config->get(QStringLiteral("autoHide"), true).toBool());

    // UI 层初始化
    m_dockWindow = new DockWindow();
    m_dockWindow->setDockManager(m_dockManager);
    m_dockWindow->setSysHelper(m_sysHelper);
    m_dockWindow->setWindowCache(m_windowCache);
    m_dockWindow->setConfigManager(m_config);

    // 加载固定项（必须在信号连接之前）
    loadPinnedItems();

    // 触发 DockManager 信号连接
    m_dockManager->initialize(m_sysHelper);
    m_dockManager->setMonitorIndex(m_dockWindow->monitorIndex());

    connectPersistence();

    // 创建 ProcessMonitor 并连接 DockWindow
    m_processMonitor = new ProcessMonitor(this);
    m_processMonitor->registerApps(m_dockManager->pinnedItems());
    m_dockWindow->setProcessMonitor(m_processMonitor);
    m_processMonitor->start();

    // 上次异常退出可能遗留隐藏状态；每次启动先恢复，再按用户设置决定是否隐藏。
    m_sysHelper->restoreNativeTaskbar();
    m_dockWindow->show();
    m_sysHelper->installWindowHook();

    // WinEvent → WindowCache 增量刷新（50ms 防抖）
    connect(m_sysHelper, &SysHelper::windowEventOccurred, this,
        [this](DWORD pid) {
            QTimer::singleShot(50, this, [this, pid]() {
                m_windowCache->refreshForPid(pid);
            });
        });

    // 隐藏原生任务栏属于实验性显式选项。Dock 未成功显示时绝不隐藏。
    if (m_config->get(QStringLiteral("hideNativeTaskbar"), false).toBool()
        && m_dockWindow->isVisible() && m_dockWindow->winId() != 0) {
        m_sysHelper->hideNativeTaskbar();
    }

    // 注册信号处理，确保异常退出时恢复任务栏
    s_sysHelperForSignal = m_sysHelper;
    std::signal(SIGINT, signalRestoreTaskbar);
    std::signal(SIGTERM, signalRestoreTaskbar);
    std::signal(SIGABRT, signalRestoreTaskbar);

    // 任务栏隐藏后 availableGeometry 变化，延迟重新定位 Dock
    QTimer::singleShot(100, m_dockWindow, &DockWindow::requestUpdatePosition);

    // 开机自启
    // 每次启动同步当前路径，修复便携目录移动后残留的旧 Run 命令。
    m_sysHelper->setAutoStart(
        m_config->get(QStringLiteral("startWithSystem"), false).toBool());

    int exitCode = app.exec();

    // 恢复原生任务栏（Windows only）
    m_sysHelper->restoreNativeTaskbar();

    // ─── 清理：在 QApplication 销毁前删除所有 QObject 子对象 ───
    delete m_processMonitor;    m_processMonitor = nullptr;
    delete m_dockWindow;    m_dockWindow = nullptr;
    delete m_dockManager;   m_dockManager = nullptr;
    delete m_windowCache;   m_windowCache = nullptr;
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
    // 1. 从系统任务栏读取 .lnk（初始集）
    PinnedItemsReader reader;
    QList<DockItemData> pinnedItems = reader.getAllPinnedItems();
    QSet<QString> systemKeys;
    for (const auto &item : pinnedItems) {
        for (const QString &key : AppIdHelper::identityKeys(item))
            systemKeys.insert(key);
    }
    qInfo() << "从系统任务栏读取" << pinnedItems.size() << "个固定项";

    // 2. 如果 data/pinned.json 存在，合并用户专用固定项
    QString pinnedPath = PathManager::pinnedFile();
    if (QFileInfo::exists(pinnedPath)) {
        QFile file(pinnedPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();

            if (doc.isArray()) {
                const QJsonArray arr = doc.array();
                int mergedCount = 0;
                for (const QJsonValue &val : arr) {
                    QJsonObject obj = val.toObject();
                    QString execPath = obj["execPath"].toString();
                    if (execPath.isEmpty()) continue;

                    DockItemData item;
                    item.appId       = obj["appId"].toString();
                    item.displayName = obj["displayName"].toString();
                    item.iconPath    = obj["iconPath"].toString();
                    item.execPath    = execPath;
                    item.targetPath  = obj["targetPath"].toString();
                    item.arguments   = obj["arguments"].toString();
                    item.appUserModelId = obj["appUserModelId"].toString();
                    item.isRunning   = false;

                    bool existsInSystem = false;
                    for (const QString &key : AppIdHelper::identityKeys(item)) {
                        if (systemKeys.contains(key)) { existsInSystem = true; break; }
                    }
                    if (existsInSystem) continue;

                    pinnedItems.append(item);
                    ++mergedCount;
                }
                qInfo() << "从 pinned.json 合并" << mergedCount << "个用户项（文件总数" << arr.size() << "）";
            }
        }
    }

    m_dockManager->setPinnedItems(pinnedItems);
}

void Application::connectPersistence()
{
    // 固定项变更时保存到 data/pinned.json
    connect(m_dockManager, &DockManager::pinnedItemsChanged,
        this, [this](const QList<DockItemData> &items) {
            QSet<QString> systemKeys;
            PinnedItemsReader reader;
            const auto systemItems = reader.getAllPinnedItems();
            for (const auto &sysItem : systemItems) {
                for (const QString &key : AppIdHelper::identityKeys(sysItem))
                    systemKeys.insert(key);
            }

            QJsonArray arr;
            for (const auto &item : items) {
                bool existsInSystem = false;
                for (const QString &key : AppIdHelper::identityKeys(item)) {
                    if (systemKeys.contains(key)) { existsInSystem = true; break; }
                }
                if (existsInSystem) continue;

                QJsonObject obj;
                obj["appId"]       = item.appId;
                obj["displayName"] = item.displayName;
                obj["iconPath"]    = item.iconPath;
                obj["execPath"]    = item.execPath;
                obj["targetPath"]  = item.targetPath;
                obj["arguments"]   = item.arguments;
                obj["appUserModelId"] = item.appUserModelId;
                arr.append(obj);
            }

            PathManager::ensureDataDir();
            QFile file(PathManager::pinnedFile());
            if (file.open(QIODevice::WriteOnly)) {
                QJsonDocument doc(arr);
                file.write(doc.toJson(QJsonDocument::Indented));
                file.close();
            }
        });
}

