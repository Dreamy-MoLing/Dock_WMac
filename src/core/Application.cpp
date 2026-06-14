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
#include <QElapsedTimer>
#include <QThread>
#include <QPixmap>
#include <csignal>

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
    app.setApplicationVersion(QStringLiteral("0.2.4"));

    // 检测 --screenshot 模式
    QStringList args = QCoreApplication::arguments();
    if (args.contains("--screenshot")) {
        m_screenshotMode = true;

        // 解析 --state hover:N
        int stateIdx = args.indexOf("--state");
        if (stateIdx >= 0 && stateIdx + 1 < args.size()) {
            QString stateVal = args[stateIdx + 1];
            if (stateVal.startsWith("hover:")) {
                m_screenshotHoverIndex = stateVal.section(":", 1).toInt();
            }
        }

        // 解析 --theme dark|light
        int themeIdx = args.indexOf("--theme");
        if (themeIdx >= 0 && themeIdx + 1 < args.size()) {
            m_screenshotTheme = args[themeIdx + 1].toLower();
        }

        // 解析 --out path
        int outIdx = args.indexOf("--out");
        if (outIdx >= 0 && outIdx + 1 < args.size()) {
            m_screenshotOutput = args[outIdx + 1];
        }

        return runScreenshotMode();
    }

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

    m_dockWindow->show();
    m_sysHelper->installWindowHook();

    // WinEvent → WindowCache 增量刷新（50ms 防抖）
    connect(m_sysHelper, &SysHelper::windowEventOccurred, this,
        [this](DWORD pid) {
            QTimer::singleShot(50, this, [this, pid]() {
                m_windowCache->refreshForPid(pid);
            });
        });

    // 隐藏原生任务栏（Windows only）
    m_sysHelper->hideNativeTaskbar();

    // 注册信号处理，确保异常退出时恢复任务栏
    s_sysHelperForSignal = m_sysHelper;
    std::signal(SIGINT, signalRestoreTaskbar);
    std::signal(SIGTERM, signalRestoreTaskbar);
    std::signal(SIGABRT, signalRestoreTaskbar);

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
    QSet<QString> systemExecPaths;  // 系统任务栏中的 execPath 集合
    for (const auto &item : pinnedItems) {
        systemExecPaths.insert(item.execPath.toLower());
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

                    // 去重：已在系统任务栏中的跳过
                    if (systemExecPaths.contains(execPath.toLower())) continue;

                    DockItemData item;
                    item.appId       = obj["appId"].toString();
                    item.displayName = obj["displayName"].toString();
                    item.iconPath    = obj["iconPath"].toString();
                    item.execPath    = execPath;
                    item.isRunning   = false;
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
            // 读取系统任务栏 execPath 集合，用于过滤
            QSet<QString> systemExecPaths;
            PinnedItemsReader reader;
            const auto systemItems = reader.getAllPinnedItems();
            for (const auto &sysItem : systemItems) {
                systemExecPaths.insert(sysItem.execPath.toLower());
            }

            QJsonArray arr;
            for (const auto &item : items) {
                // 只保存非系统任务栏中的项（Dock 专用固定项）
                if (systemExecPaths.contains(item.execPath.toLower())) continue;

                QJsonObject obj;
                obj["appId"]       = item.appId;
                obj["displayName"] = item.displayName;
                obj["iconPath"]    = item.iconPath;
                obj["execPath"]    = item.execPath;
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

int Application::runScreenshotMode()
{
    setupLogging();

    // 核心层初始化
    m_config = new ConfigManager(this);
    m_config->load();

    m_sysHelper = new SysHelper(this);
    m_windowCache = new WindowCache(this);

    m_dockManager = new DockManager(this);
    m_dockManager->setMaxItems(m_config->get(QStringLiteral("maxItems"), 16).toInt());

    // UI 层初始化
    m_dockWindow = new DockWindow();
    m_dockWindow->setDockManager(m_dockManager);
    m_dockWindow->setSysHelper(m_sysHelper);
    m_dockWindow->setWindowCache(m_windowCache);
    m_dockWindow->setConfigManager(m_config);

    // 加载固定项
    loadPinnedItems();

    // 触发 DockManager 信号连接
    m_dockManager->initialize(m_sysHelper);
    m_dockManager->setMonitorIndex(m_dockWindow->monitorIndex());

    // 创建 ProcessMonitor 并连接 DockWindow
    m_processMonitor = new ProcessMonitor(this);
    m_processMonitor->registerApps(m_dockManager->pinnedItems());
    m_dockWindow->setProcessMonitor(m_processMonitor);
    m_processMonitor->start();

    // 主题覆盖
    if (m_screenshotTheme == "dark") {
        m_dockWindow->setThemeOverride(false);
    } else if (m_screenshotTheme == "light") {
        m_dockWindow->setThemeOverride(true);
    }

    m_dockWindow->show();

    // 等待窗口渲染完成
    QApplication::processEvents();
    QApplication::processEvents();

    // 模拟悬停状态
    if (m_screenshotHoverIndex >= 0 && m_screenshotHoverIndex < m_dockWindow->itemCount()) {
        m_dockWindow->simulateHover(m_screenshotHoverIndex);
        // 等待鱼眼动画完成
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 300) {
            QApplication::processEvents();
            QThread::msleep(10);
        }
    }

    // 截图
    QString outputPath = m_screenshotOutput;
    if (outputPath.isEmpty()) {
        PathManager::ensureDataDir();
        outputPath = PathManager::dataDir() + "/screenshot.png";
    }

    QPixmap pixmap = m_dockWindow->grab();
    bool saved = pixmap.save(outputPath, "PNG");
    qInfo() << "Screenshot saved:" << outputPath << "ok:" << saved;

    // 清理
    delete m_processMonitor; m_processMonitor = nullptr;
    delete m_dockWindow; m_dockWindow = nullptr;
    delete m_dockManager; m_dockManager = nullptr;
    delete m_windowCache; m_windowCache = nullptr;
    delete m_sysHelper; m_sysHelper = nullptr;
    delete m_config; m_config = nullptr;

    return saved ? 0 : 1;
}

