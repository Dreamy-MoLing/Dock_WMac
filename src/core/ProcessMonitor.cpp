/**
 * @file ProcessMonitor.cpp
 * @brief 应用进程状态监控器实现
 *
 * 通过 IPCHelper 周期性检测进程运行状态，
 * 将变化以信号形式通知 UI 层。
 */

#include "core/ProcessMonitor.h"
#include "core/IPCHelper.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>

ProcessMonitor::ProcessMonitor(IPCHelper *ipcHelper, QObject *parent)
    : QObject(parent)
    , m_ipcHelper(ipcHelper)
    , m_timer(new QTimer(this))
    , m_tickCount(0)
{
    connect(m_timer, &QTimer::timeout, this, &ProcessMonitor::onTick);
}

void ProcessMonitor::start(int intervalMs)
{
    m_tickCount = 0;
    m_timer->start(intervalMs);
}

void ProcessMonitor::stop()
{
    m_timer->stop();
}

void ProcessMonitor::registerApp(const QString &appId)
{
    m_registeredApps.insert(appId);
}

void ProcessMonitor::registerApps(const QList<DockItemData> &items)
{
    for (const auto &item : items) {
        m_registeredApps.insert(item.appId);
    }
}

void ProcessMonitor::unregisterApp(const QString &appId)
{
    m_registeredApps.remove(appId);
    m_runningCache.remove(appId);
}

QString ProcessMonitor::appIdToProcessName(const QString &appId)
{
    // org.gnome.Nautilus → Nautilus
    QString name = appId;
    int dotIdx = name.lastIndexOf('.');
    if (dotIdx >= 0) {
        name = name.mid(dotIdx + 1);
    }
    return name.toLower();
}

void ProcessMonitor::onTick()
{
    if (!m_ipcHelper || m_registeredApps.isEmpty()) {
        m_tickCount++;
        return;
    }

    // 每 4 次 tick 扫描一次新应用
    if (m_tickCount % 4 == 0) {
        scanTransientApps();
    }
    m_tickCount++;

    // 批量检测已注册应用的运行状态
    QStringList processNames;
    QList<QString> appIds;
    for (const auto &appId : m_registeredApps) {
        processNames << appIdToProcessName(appId);
        appIds << appId;
    }

    QJsonObject resp = m_ipcHelper->checkProcesses(processNames);
    if (resp.value("status").toString() != "ok") return;

    QJsonObject running = resp.value("data").toObject().value("is_running").toObject();
    for (int i = 0; i < appIds.size(); ++i) {
        const QString &appId = appIds[i];
        bool isRunning = running.value(processNames[i]).toBool();
        bool wasRunning = m_runningCache.value(appId, false);

        if (isRunning != wasRunning) {
            m_runningCache[appId] = isRunning;
            emit appRunningStateChanged(appId, isRunning);
        }
    }
}

void ProcessMonitor::scanTransientApps()
{
    QJsonObject resp = m_ipcHelper->scanRunningApps();
    if (resp.value("status").toString() != "ok") return;

    QJsonArray runningList = resp.value("data").toObject().value("apps").toArray();

    // 当前运行中的应用 ID 集合
    QSet<QString> currentRunning;
    for (const QJsonValue &v : runningList) {
        QJsonObject app = v.toObject();
        QString appId = app.value("app_id").toString();
        if (appId.isEmpty()) continue;
        currentRunning.insert(appId);

        // 跳过已注册的（固定项或已有临时项）
        if (m_registeredApps.contains(appId)) continue;
        // 跳过已检测到的
        if (m_detectedRunningApps.contains(appId)) continue;

        // 新应用出现
        DockItemData item;
        item.appId = appId;
        item.displayName = app.value("display_name").toString(appId);
        item.execPath = app.value("exec_path").toString();
        item.iconPath = app.value("icon_path").toString();
        item.isRunning = true;

        m_detectedRunningApps.insert(appId);
        emit newRunningAppDetected(item);
    }

    // 清理已退出的临时应用
    QSet<QString> exited = m_detectedRunningApps - currentRunning;
    for (const auto &appId : exited) {
        m_detectedRunningApps.remove(appId);
        emit runningAppExited(appId);
    }
}
