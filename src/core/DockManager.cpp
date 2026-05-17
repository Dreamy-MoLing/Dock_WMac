/**
 * @file DockManager.cpp
 * @brief Dock 状态机实现
 *
 * 管理 Dock 三种状态 (Docked/Hidden/Animating) 之间的转换逻辑，
 * 接收 SysHelper 的事件信号并作出状态决策，发射信号通知 UI 更新。
 */

#include "core/DockManager.h"
#include "core/SysHelper.h"

DockManager::DockManager(QObject *parent)
    : QObject(parent)
    , m_currentState(DockState::Docked)
    , m_sysHelper(nullptr)
{
}

DockState DockManager::currentState() const
{
    return m_currentState;
}

QList<DockItemData> DockManager::items() const
{
    return m_items;
}

void DockManager::initialize(SysHelper *sysHelper)
{
    m_sysHelper = sysHelper;

    // 连接 SysHelper 信号到状态机处理槽
    connect(m_sysHelper, &SysHelper::foregroundWindowChanged,
            this, &DockManager::onForegroundWindowChanged);
    connect(m_sysHelper, &SysHelper::winKeyPressed,
            this, &DockManager::onWinKeyPressed);

    // 初始加载固定项
    m_items = m_sysHelper->getPinnedItems();
    for (const auto &item : m_items) {
        emit itemAdded(item);
    }
}

void DockManager::onForegroundWindowChanged(bool isMaximizedOrFullscreen)
{
    if (isMaximizedOrFullscreen && m_currentState == DockState::Docked) {
        // 前台窗口最大化/全屏 → 隐藏 Dock
        m_sysHelper->installKeyboardHook();
        m_currentState = DockState::Animating;
        emit stateChanged(DockState::Animating);
        m_currentState = DockState::Hidden;
        emit stateChanged(DockState::Hidden);

    } else if (!isMaximizedOrFullscreen && m_currentState == DockState::Hidden) {
        // 前台窗口恢复正常 → 弹出 Dock
        m_sysHelper->uninstallKeyboardHook();
        m_currentState = DockState::Animating;
        emit stateChanged(DockState::Animating);
        m_currentState = DockState::Docked;
        emit stateChanged(DockState::Docked);
    }
}

void DockManager::onWinKeyPressed()
{
    if (m_currentState != DockState::Hidden) return;

    m_currentState = DockState::Animating;
    emit stateChanged(DockState::Animating);
    m_currentState = DockState::Docked;
    emit stateChanged(DockState::Docked);
}


