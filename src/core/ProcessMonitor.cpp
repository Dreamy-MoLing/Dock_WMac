/**
 * @file ProcessMonitor.cpp
 * @brief 应用进程状态监控器实现
 *
 * 使用 Win32 API (CreateToolhelp32Snapshot) 周期性检测进程运行状态，
 * 将变化以信号形式通知 UI 层。
 */

#include "core/ProcessMonitor.h"
#include <QFileInfo>
#include <QDebug>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

ProcessMonitor::ProcessMonitor(QObject *parent)
    : QObject(parent)
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

/**
 * @brief 获取当前运行中的进程名集合
 *
 * 使用 CreateToolhelp32Snapshot 枚举所有进程，
 * 返回小写的可执行文件名（不含 .exe）集合。
 */
static QSet<QString> getRunningProcessNames()
{
    QSet<QString> names;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return names;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (Process32First(hSnap, &pe)) {
        do {
            QString exeName = QString::fromWCharArray(pe.szExeFile).toLower();
            // 移除 .exe 后缀
            if (exeName.endsWith(".exe")) {
                exeName.chop(4);
            }
            names.insert(exeName);
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return names;
}

void ProcessMonitor::onTick()
{
    if (m_registeredApps.isEmpty()) {
        m_tickCount++;
        return;
    }

    // 每 4 次 tick 扫描一次新应用
    if (m_tickCount % 4 == 0) {
        scanTransientApps();
    }
    m_tickCount++;

    // 获取当前运行中的进程名
    QSet<QString> runningNames = getRunningProcessNames();

    // 检测已注册应用的运行状态
    for (const auto &appId : m_registeredApps) {
        QString processName = appIdToProcessName(appId);
        bool isRunning = runningNames.contains(processName);
        bool wasRunning = m_runningCache.value(appId, false);

        if (isRunning != wasRunning) {
            m_runningCache[appId] = isRunning;
            emit appRunningStateChanged(appId, isRunning);
        }
    }
}

void ProcessMonitor::scanTransientApps()
{
    QSet<QString> runningNames = getRunningProcessNames();

    // 当前运行中的应用 ID 集合
    QSet<QString> currentRunning;
    for (const auto &name : runningNames) {
        // 跳过系统进程
        if (name.isEmpty() || name == "system" || name == "idle") continue;

        currentRunning.insert(name);

        // 跳过已注册的（固定项或已有临时项）
        if (m_registeredApps.contains(name)) continue;
        // 跳过已检测到的
        if (m_detectedRunningApps.contains(name)) continue;

        // 新应用出现
        DockItemData item;
        item.appId = name;
        item.displayName = name;
        item.execPath = QString();
        item.iconPath = QString();
        item.isRunning = true;

        m_detectedRunningApps.insert(name);
        emit newRunningAppDetected(item);
    }

    // 清理已退出的临时应用
    QSet<QString> exited = m_detectedRunningApps - currentRunning;
    for (const auto &appId : exited) {
        m_detectedRunningApps.remove(appId);
        emit runningAppExited(appId);
    }
}
