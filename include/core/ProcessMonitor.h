#ifndef PROCESSMONITOR_H
#define PROCESSMONITOR_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QSet>
#include "Types.h"

/**
 * @file ProcessMonitor.h
 * @brief 应用进程状态监控器
 *
 * 使用 Win32 API 周期性检测已注册应用的运行状态，
 * 并扫描系统中新出现的运行应用（用于临时项管理）。
 * 通过信号通知 DockWindow，解耦 UI 层与系统层的直接依赖。
 */
class ProcessMonitor : public QObject {
    Q_OBJECT
public:
    explicit ProcessMonitor(QObject *parent = nullptr);

    /** @brief 开始周期性检测 */
    void start(int intervalMs = 2000);

    /** @brief 停止检测 */
    void stop();

    /** @brief 注册需要监控运行状态的应用
     *  @param execPath 可执行文件路径，用于更精确的进程名匹配（可选） */
    void registerApp(const QString &appId, const QString &execPath = QString());

    /** @brief 批量注册应用 */
    void registerApps(const QList<DockItemData> &items);

    /** @brief 取消注册 */
    void unregisterApp(const QString &appId);

signals:
    /** @brief 已注册应用的运行状态变化 */
    void appRunningStateChanged(const QString &appId, bool isRunning);

    /** @brief 检测到未注册的新应用在运行（用于临时项） */
    void newRunningAppDetected(const DockItemData &item);

    /** @brief 之前检测到的运行应用已退出 */
    void runningAppExited(const QString &appId);

private slots:
    void onTick();

private:
    static QString appIdToProcessName(const QString &appId);
    void scanTransientApps();

    QTimer *m_timer;
    int m_tickCount;

    // 监控的应用集合与状态缓存
    QSet<QString> m_registeredApps;        // appId 集合
    QMap<QString, QString> m_registeredExecPaths;  // appId → execPath
    QMap<QString, bool> m_runningCache;     // appId → isRunning

    // 已检测到的运行中应用（用于清理）
    QSet<QString> m_detectedRunningApps;
};

#endif // PROCESSMONITOR_H
