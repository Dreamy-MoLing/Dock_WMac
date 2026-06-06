/**
 * @file ClickStateMachine.cpp
 * @brief 5 状态点击状态机实现
 */
#include "core/ClickStateMachine.h"
#include "core/WindowCache.h"

#include <QDir>
#include <windows.h>

ClickStateMachine::ClickStateMachine(WindowCache *cache, QObject *parent)
    : QObject(parent)
    , m_cache(cache)
{
}

ClickStateMachine::State ClickStateMachine::determineState(const QString &wmClass, bool isRunning)
{
    if (!isRunning)
        return State::NoWindows;

    bool hasVisible = m_cache->hasVisibleWindows(wmClass);
    bool hasMinimized = m_cache->hasMinimizedWindows(wmClass);
    bool hasHidden = m_cache->hasHiddenWindows(wmClass);

    if (!hasVisible && !hasMinimized && hasHidden)
        return State::BackgroundRunning;

    if (!hasVisible && hasMinimized)
        return State::AllMinimized;

    if (hasVisible && m_cache->isForegroundApp(wmClass))
        return State::ForegroundActive;

    if (hasVisible && !m_cache->isForegroundApp(wmClass))
        return State::BackgroundVisible;

    // 兜底：进程运行但无窗口 → BackgroundRunning
    return State::BackgroundRunning;
}

ClickStateMachine::Action ClickStateMachine::evaluate(
    const QString &wmClass, const QString &execPath, bool isRunning)
{
    Q_UNUSED(execPath);
    State state = determineState(wmClass, isRunning);

    switch (state) {
    case State::NoWindows:
        return Action::LaunchApp;
    case State::BackgroundRunning:
        return Action::ShowHiddenWindow;
    case State::AllMinimized:
        return Action::RestoreLastActive;
    case State::ForegroundActive:
        return Action::MinimizeAll;
    case State::BackgroundVisible:
        return Action::BringToForeground;
    }
    return Action::None;
}

void ClickStateMachine::execute(Action action, const QString &wmClass, const QString &execPath)
{
    switch (action) {
    case Action::LaunchApp:
        launchApp(execPath);
        break;
    case Action::ShowHiddenWindow:
        showHiddenWindow(wmClass);
        break;
    case Action::RestoreLastActive:
        restoreLastActive(wmClass);
        break;
    case Action::MinimizeAll:
        minimizeAll(wmClass);
        break;
    case Action::BringToForeground:
        bringToForeground(wmClass);
        break;
    case Action::None:
        break;
    }
}

void ClickStateMachine::handleClick(const QString &wmClass, const QString &execPath, bool isRunning)
{
    // 点击前做按需同步扫描
    m_cache->scanForClass(wmClass);
    Action action = evaluate(wmClass, execPath, isRunning);
    execute(action, wmClass, execPath);
}

void ClickStateMachine::handleDoubleClick(const QString &execPath)
{
    launchApp(execPath);
}

// ─── 动作实现 ────────────────────────────────────────────

void ClickStateMachine::launchApp(const QString &execPath)
{
    if (execPath.isEmpty()) return;
    QString nativePath = QDir::toNativeSeparators(execPath);
    QProcess::startDetached(nativePath, QStringList());
}

void ClickStateMachine::showHiddenWindow(const QString &wmClass)
{
    // 获取所有窗口（包括隐藏的），排除 toolwindow
    WindowList allWindows = m_cache->getWindowsForPreview(wmClass);

    // 查找不可见且非最小化的窗口
    HWND target = nullptr;
    DWORD bestTime = 0;
    for (const auto &w : allWindows) {
        if (!w.isVisible && !w.isMinimized && !w.isToolWindow && IsWindow(w.hwnd)) {
            if (!target || w.lastActiveTime > bestTime) {
                target = w.hwnd;
                bestTime = w.lastActiveTime;
            }
        }
    }

    if (target) {
        ShowWindow(target, SW_SHOW);
        AllowSetForegroundWindow(ASFW_ANY);
        SetForegroundWindow(target);
    }
}

void ClickStateMachine::restoreLastActive(const QString &wmClass)
{
    HWND hwnd = m_cache->getLastActiveHwnd(wmClass);
    if (!hwnd || !IsWindow(hwnd)) return;

    AllowSetForegroundWindow(ASFW_ANY);
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}

void ClickStateMachine::minimizeAll(const QString &wmClass)
{
    WindowList windows = m_cache->getWindowsForPreview(wmClass);
    for (const auto &w : windows) {
        if (w.isVisible && !w.isToolWindow && IsWindow(w.hwnd)) {
            ShowWindow(w.hwnd, SW_MINIMIZE);
        }
    }
}

void ClickStateMachine::bringToForeground(const QString &wmClass)
{
    HWND hwnd = m_cache->getLastActiveHwnd(wmClass);
    if (!hwnd || !IsWindow(hwnd)) return;

    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}
