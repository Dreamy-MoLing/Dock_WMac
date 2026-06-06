#ifndef CLICKSTATEMACHINE_H
#define CLICKSTATEMACHINE_H

#include <QObject>
#include <QString>
#include <QProcess>

class WindowCache;

/**
 * @file ClickStateMachine.h
 * @brief 5 状态点击行为状态机
 *
 * S0 NoWindows       → LaunchApp
 * S0.5 BackgroundRunning → ShowHiddenWindow
 * S1 AllMinimized    → RestoreLastActive
 * S2 ForegroundActive → MinimizeAll
 * S3 BackgroundVisible → BringToForeground
 *
 * 双击始终走 LaunchApp。
 */

class ClickStateMachine : public QObject
{
    Q_OBJECT
public:
    enum class State {
        NoWindows,
        BackgroundRunning,
        AllMinimized,
        ForegroundActive,
        BackgroundVisible
    };

    enum class Action {
        None,
        LaunchApp,
        ShowHiddenWindow,
        RestoreLastActive,
        MinimizeAll,
        BringToForeground
    };

    explicit ClickStateMachine(WindowCache *cache, QObject *parent = nullptr);

    /** @brief 根据当前窗口状态和执行路径计算应执行的动作
     *  @param wmClass      目标应用 WM_CLASS
     *  @param execPath     可执行文件路径（用于 LaunchApp）
     *  @param isRunning    进程是否在运行（ProcessMonitor 确认）
     *  @return 应执行的动作
     */
    Action evaluate(const QString &wmClass, const QString &execPath, bool isRunning);

    /** @brief 执行指定动作 */
    void execute(Action action, const QString &wmClass, const QString &execPath);

    /** @brief 判定 + 执行（便捷方法，供 DockWindow 调用） */
    void handleClick(const QString &wmClass, const QString &execPath, bool isRunning);

    /** @brief 双击：始终启动新实例 */
    void handleDoubleClick(const QString &execPath);

private:
    State determineState(const QString &wmClass, bool isRunning);
    void launchApp(const QString &execPath);
    void showHiddenWindow(const QString &wmClass);
    void restoreLastActive(const QString &wmClass);
    void minimizeAll(const QString &wmClass);
    void bringToForeground(const QString &wmClass);

    WindowCache *m_cache;
};

#endif // CLICKSTATEMACHINE_H
