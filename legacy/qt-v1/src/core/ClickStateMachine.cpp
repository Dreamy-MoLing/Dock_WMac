/**
 * @file ClickStateMachine.cpp
 * @brief 5 状态点击状态机实现
 */
#include "core/ClickStateMachine.h"
#include "core/WindowCache.h"
#include "core/SysHelper.h"

#include <QDir>
#include <QSet>
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
    handleClick(QStringList{wmClass}, execPath, isRunning);
}

void ClickStateMachine::handleClick(const QStringList &identityKeys, const QString &execPath, bool isRunning)
{
    QStringList keys;
    QSet<QString> seen;
    for (const QString &key : identityKeys) {
        const QString normalized = key.trimmed().toLower();
        if (normalized.isEmpty() || seen.contains(normalized))
            continue;
        seen.insert(normalized);
        keys.append(normalized);
    }

    if (keys.isEmpty()) {
        if (!isRunning)
            launchApp(execPath);
        return;
    }

    // 点击前对每个候选身份 key 做按需同步扫描。
    for (const QString &key : keys) {
        m_cache->scanForClass(key);
    }

    QString selectedKey = selectIdentityKey(keys);

    // 进程已运行但各身份 key 均无可交互窗口时，不贸然启动新实例；
    // 先做一次全量刷新后重试，仍无窗口则执行 None（保守 no-op）。
    if (selectedKey.isEmpty() && isRunning) {
        m_cache->refresh();
        for (const QString &key : keys) {
            m_cache->scanForClass(key);
        }
        selectedKey = selectIdentityKey(keys);
        if (selectedKey.isEmpty()) {
            execute(Action::None, QString(), execPath);
            return;
        }
    }

    if (selectedKey.isEmpty())
        selectedKey = keys.first();

    Action action = evaluate(selectedKey, execPath, isRunning);
    execute(action, selectedKey, execPath);
}

QString ClickStateMachine::selectIdentityKey(const QStringList &identityKeys) const
{
    for (const QString &key : identityKeys) {
        if (m_cache->hasVisibleWindows(key) || m_cache->hasMinimizedWindows(key)
            || m_cache->hasHiddenWindows(key)
            || !m_cache->getWindowsForPreview(key).isEmpty()) {
            return key;
        }
    }
    return {};
}

void ClickStateMachine::handleDoubleClick(const QString &execPath)
{
    launchApp(execPath);
}

// ─── 动作实现 ────────────────────────────────────────────

void ClickStateMachine::launchApp(const QString &execPath)
{
    if (execPath.isEmpty()) return;
    SysHelper::launchPath(execPath);
}

void ClickStateMachine::showHiddenWindow(const QString &wmClass)
{
    // 获取点击激活语义下的所有窗口（包括隐藏/无标题窗口），排除 toolwindow
    WindowList allWindows = m_cache->getWindowsForActivation(wmClass);

    // 查找不可见的窗口（包括最小化的）
    HWND target = nullptr;
    DWORD bestTime = 0;
    for (const auto &w : allWindows) {
        if (!w.isVisible && !w.isToolWindow && IsWindow(w.hwnd)) {
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
        SetFocus(target);
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

    // 如果窗口被最小化，先恢复
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}
