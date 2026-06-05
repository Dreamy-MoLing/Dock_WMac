/**
 * @file DockManager.cpp
 * @brief Dock 状态机实现
 *
 * 管理 Dock 三种状态 (Docked/Hidden/Animating) 之间的转换逻辑，
 * 接收 SysHelper 的事件信号并作出状态决策，发射信号通知 UI 更新。
 *
 * 固定项（pinned）在配置中持久化保存。
 * 临时项（transient）随运行应用自动出现/消失。
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
    // 返回组合列表：固定项在前，临时项在后
    return m_pinnedItems + m_transientItems;
}

QList<DockItemData> DockManager::pinnedItems() const
{
    return m_pinnedItems;
}

QList<DockItemData> DockManager::transientItems() const
{
    return m_transientItems;
}

QList<DockItemData> DockManager::visibleItems() const
{
    // 固定项全部显示，不受 maxItems 限制
    QList<DockItemData> visible = m_pinnedItems;

    // 临时项补满到 maxItems
    int remaining = m_maxItems - visible.size();
    for (int i = 0; i < m_transientItems.size() && i < remaining; ++i) {
        visible.append(m_transientItems[i]);
    }

    return visible;
}

QList<DockItemData> DockManager::overflowItems() const
{
    // 被折叠的临时项：超出 visibleItems 容量的部分
    int pinnedCount = m_pinnedItems.size();
    int transientVisible = qMax(0, m_maxItems - pinnedCount);
    int transientTotal = m_transientItems.size();

    if (transientVisible >= transientTotal) {
        return {};  // 没有溢出
    }

    QList<DockItemData> overflow;
    for (int i = transientVisible; i < transientTotal; ++i) {
        overflow.append(m_transientItems[i]);
    }
    return overflow;
}

void DockManager::setMaxItems(int max)
{
    if (max < 1) max = 1;
    if (m_maxItems == max) return;
    m_maxItems = max;
    emit overflowChanged();
}

int DockManager::maxItems() const
{
    return m_maxItems;
}

void DockManager::updateWindowCount(const QString &appId, int count)
{
    // 在固定项中查找
    for (auto &item : m_pinnedItems) {
        if (item.appId == appId) {
            if (item.windowCount != count) {
                item.windowCount = count;
                emit itemWindowCountChanged(appId, count);
            }
            return;
        }
    }
    // 在临时项中查找
    for (auto &item : m_transientItems) {
        if (item.appId == appId) {
            if (item.windowCount != count) {
                item.windowCount = count;
                emit itemWindowCountChanged(appId, count);
            }
            return;
        }
    }
}

void DockManager::initialize(SysHelper *sysHelper)
{
    m_sysHelper = sysHelper;

    // 连接 SysHelper 信号到状态机处理槽
    connect(m_sysHelper, &SysHelper::foregroundWindowChanged,
            this, &DockManager::onForegroundWindowChanged);
    connect(m_sysHelper, &SysHelper::winKeyPressed,
            this, &DockManager::onWinKeyPressed);

    // 初始加载固定项（由 ConfigManager + IPCHelper 填充，通过 setPinnedItems 设置）
    if (m_pinnedItems.isEmpty()) {
        // 回退：直接从 SysHelper 读取固定项
        m_pinnedItems = m_sysHelper->getPinnedItems();
        for (const auto &item : m_pinnedItems) {
            emit itemAdded(item);
        }
    }
}

void DockManager::setPinnedItems(const QList<DockItemData> &items)
{
    // 清除现有的
    for (const auto &item : m_pinnedItems) {
        emit itemRemoved(item.appId);
    }

    m_pinnedItems = items;

    // 添加新的
    for (const auto &item : m_pinnedItems) {
        emit itemAdded(item);
    }

    emit overflowChanged();
}

void DockManager::addTransientItem(const DockItemData &item)
{
    // 检查是否已在固定项中
    for (const auto &p : m_pinnedItems) {
        if (p.appId == item.appId) return;
    }
    // 检查是否已存在于临时项
    for (const auto &t : m_transientItems) {
        if (t.appId == item.appId) return;
    }

    m_transientItems.append(item);

    // 检查是否在可见范围内
    int visibleTransientCount = qMax(0, m_maxItems - m_pinnedItems.size());
    if (m_transientItems.size() <= visibleTransientCount) {
        emit itemAdded(item);
    } else {
        // 超出范围，通知溢出变更
        emit overflowChanged();
    }
}

void DockManager::removeTransientItem(const QString &appId)
{
    for (int i = 0; i < m_transientItems.size(); ++i) {
        if (m_transientItems[i].appId == appId) {
            m_transientItems.removeAt(i);
            emit itemRemoved(appId);
            emit overflowChanged();
            return;
        }
    }
}

void DockManager::pinItem(const QString &appId)
{
    // 检查是否已在固定列表中
    for (const auto &p : m_pinnedItems) {
        if (p.appId == appId) return;
    }

    // 从临时项移到固定项
    DockItemData item;
    bool found = false;
    for (int i = 0; i < m_transientItems.size(); ++i) {
        if (m_transientItems[i].appId == appId) {
            item = m_transientItems[i];
            m_transientItems.removeAt(i);
            found = true;
            break;
        }
    }

    if (!found) {
        // 不在临时项中（可能是手动添加的固定）
        // 从 SysHelper 获取完整信息
        auto allItems = m_sysHelper->getPinnedItems();
        for (const auto &a : allItems) {
            if (a.appId == appId) {
                item = a;
                found = true;
                break;
            }
        }
        if (!found) return;
    }

    m_pinnedItems.append(item);

    // 重新发射信号：先移除再添加（让 UI 层知道位置变了）
    emit itemRemoved(appId);
    emit itemAdded(item);
    emit pinnedItemsChanged(m_pinnedItems);
}

void DockManager::unpinItem(const QString &appId)
{
    for (int i = 0; i < m_pinnedItems.size(); ++i) {
        if (m_pinnedItems[i].appId == appId) {
            m_pinnedItems.removeAt(i);
            emit itemRemoved(appId);
            emit pinnedItemsChanged(m_pinnedItems);
            return;
        }
    }
}

bool DockManager::isPinned(const QString &appId) const
{
    for (const auto &p : m_pinnedItems) {
        if (p.appId == appId) return true;
    }
    return false;
}

void DockManager::onForegroundWindowChanged(bool isMaximizedOrFullscreen)
{
    if (isMaximizedOrFullscreen && m_currentState == DockState::Docked) {
        m_sysHelper->installKeyboardHook();
        m_currentState = DockState::Animating;
        emit stateChanged(DockState::Animating);
        m_currentState = DockState::Hidden;
        emit stateChanged(DockState::Hidden);

    } else if (!isMaximizedOrFullscreen && m_currentState == DockState::Hidden) {
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
