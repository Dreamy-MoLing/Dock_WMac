/**
 * @file DockManager.cpp
 * @brief Dock 状态机实现
 *
 * 管理 Dock 两种状态 (Docked/Hidden) 之间的转换逻辑，
 * 接收 SysHelper 的事件信号并作出状态决策，发射信号通知 UI 更新。
 *
 * 固定项（pinned）在配置中持久化保存。
 * 临时项（transient）随运行应用自动出现/消失。
 *
 * 显隐逻辑：
 * - 常驻 (Docked)：没有全屏窗口 → 默认置顶可见
 * - 隐藏延迟：检测到全屏 → 3s 确认（过滤开始菜单等短暂弹窗）→ 隐藏
 * - Win 键冷却：Win 键/鼠标触底唤醒 → 1.5s 内忽略全屏检测（防止开始菜单冲突）
 */

#include "core/DockManager.h"
#include "core/SysHelper.h"
#include <QDebug>

DockManager::DockManager(QObject *parent)
    : QObject(parent)
    , m_currentState(DockState::Docked)
    , m_sysHelper(nullptr)
{
    // 隐藏延迟定时器（3 秒 — 确认全屏不是短暂弹窗）
    m_hideDelayTimer = new QTimer(this);
    m_hideDelayTimer->setSingleShot(true);
    m_hideDelayTimer->setInterval(kHideDelayMs);
    connect(m_hideDelayTimer, &QTimer::timeout,
            this, &DockManager::onHideDelayTimeout);

    // Win 键冷却定时器（1.5 秒 — 防止开始菜单触发全屏检测）
    m_winKeyCooldownTimer = new QTimer(this);
    m_winKeyCooldownTimer->setSingleShot(true);
    m_winKeyCooldownTimer->setInterval(kWinKeyCooldownMs);
    connect(m_winKeyCooldownTimer, &QTimer::timeout,
            this, &DockManager::onWinKeyCooldownTimeout);
}

DockState DockManager::currentState() const
{
    return m_currentState;
}

QList<DockItemData> DockManager::items() const
{
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
    QList<DockItemData> visible = m_pinnedItems;

    int remaining = m_maxItems - visible.size();
    for (int i = 0; i < m_transientItems.size() && i < remaining; ++i) {
        visible.append(m_transientItems[i]);
    }

    return visible;
}

QList<DockItemData> DockManager::overflowItems() const
{
    int pinnedCount = m_pinnedItems.size();
    int transientVisible = qMax(0, m_maxItems - pinnedCount);
    int transientTotal = m_transientItems.size();

    if (transientVisible >= transientTotal) {
        return {};
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

void DockManager::setMonitorIndex(int index)
{
    m_monitorIndex = index;
}

void DockManager::updateWindowCount(const QString &appId, int count)
{
    for (auto &item : m_pinnedItems) {
        if (item.appId == appId) {
            if (item.windowCount != count) {
                item.windowCount = count;
                emit itemWindowCountChanged(appId, count);
            }
            return;
        }
    }
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

    // 连接 SysHelper 信号（SysHelper 发出主屏幕的全屏状态；内部自行按 m_monitorIndex 过滤）
    connect(m_sysHelper, &SysHelper::fullscreenStateChanged,
            this, &DockManager::onFullscreenStateChanged);
    connect(m_sysHelper, &SysHelper::winKeyPressed,
            this, &DockManager::onWinKeyPressed);
}

void DockManager::setPinnedItems(const QList<DockItemData> &items)
{
    for (const auto &item : m_pinnedItems) {
        emit itemRemoved(item.appId);
    }

    m_pinnedItems = items;

    for (const auto &item : m_pinnedItems) {
        emit itemAdded(item);
    }

    emit overflowChanged();
}

void DockManager::addTransientItem(const DockItemData &item)
{
    for (const auto &p : m_pinnedItems) {
        if (p.appId == item.appId) return;
    }
    for (const auto &t : m_transientItems) {
        if (t.appId == item.appId) return;
    }

    m_transientItems.append(item);

    int visibleTransientCount = qMax(0, m_maxItems - m_pinnedItems.size());
    if (m_transientItems.size() <= visibleTransientCount) {
        emit itemAdded(item);
    } else {
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

void DockManager::pinItem(const DockItemData &item)
{
    for (const auto &p : m_pinnedItems) {
        if (p.appId == item.appId) return;
    }

    bool wasTransient = false;
    for (int i = 0; i < m_transientItems.size(); ++i) {
        if (m_transientItems[i].appId == item.appId) {
            m_transientItems.removeAt(i);
            wasTransient = true;
            break;
        }
    }

    m_pinnedItems.append(item);

    if (wasTransient) {
        emit itemRemoved(item.appId);
    }
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

// ─── 状态机核心逻辑 ─────────────────────────────────────────

void DockManager::enterHiddenState()
{
    if (m_currentState == DockState::Hidden) return;
    m_hideDelayTimer->stop();
    m_sysHelper->installKeyboardHook();
    m_currentState = DockState::Hidden;
    emit stateChanged(DockState::Hidden);
}

void DockManager::enterDockedState()
{
    if (m_currentState == DockState::Docked) return;
    m_hideDelayTimer->stop();
    m_winKeyCooldownTimer->stop();
    m_sysHelper->uninstallKeyboardHook();
    m_currentState = DockState::Docked;
    emit stateChanged(DockState::Docked);
}

void DockManager::onFullscreenStateChanged(bool anyMaximizedOnAnyScreen)
{
    if (!m_sysHelper) return;

    // 用当前 dock 屏幕指数重新查询（屏幕感知 — 不依赖信号参数的主屏判断）
    bool anyMaxOnDockScreen = m_sysHelper->hasMaximizedOrFullscreenWindowOnMonitor(m_monitorIndex);

    if (anyMaxOnDockScreen && m_currentState == DockState::Docked) {
        // Win 键冷却期内忽略全屏信号（防止开始菜单导致误隐藏）
        if (m_winKeyCooldownTimer->isActive()) {
            qInfo() << "全屏信号在 Win 键冷却期内，忽略";
            return;
        }
        // 启动 3 秒延迟 — 确认全屏不是短暂弹窗
        if (!m_hideDelayTimer->isActive()) {
            qInfo() << "检测到全屏窗口，启动" << kHideDelayMs << "ms 隐藏延迟";
            m_hideDelayTimer->start();
        }

    } else if (!anyMaxOnDockScreen && m_currentState == DockState::Hidden) {
        // 全屏消失 → 立即显示
        qInfo() << "全屏窗口已关闭，恢复显示";
        enterDockedState();

    } else if (!anyMaxOnDockScreen && m_hideDelayTimer->isActive()) {
        // 延迟期间全屏消失 → 取消隐藏
        qInfo() << "全屏窗口在延迟期间消失，取消隐藏";
        m_hideDelayTimer->stop();
    }
}

void DockManager::onHideDelayTimeout()
{
    if (!m_sysHelper) return;

    // 延迟到期后重新确认全屏仍然存在
    bool anyMaxOnDockScreen = m_sysHelper->hasMaximizedOrFullscreenWindowOnMonitor(m_monitorIndex);

    if (anyMaxOnDockScreen && m_currentState == DockState::Docked) {
        qInfo() << "全屏确认持续" << kHideDelayMs << "ms，隐藏 Dock";
        enterHiddenState();
    } else {
        qInfo() << "延迟到期时全屏已消失，保持常驻";
    }
}

void DockManager::onWinKeyPressed()
{
    if (m_currentState == DockState::Hidden) {
        qInfo() << "Win键/触底唤醒，显示 Dock，启动" << kWinKeyCooldownMs << "ms 冷却";
        enterDockedState();
        // 启动冷却定时器 — 期间忽略全屏信号
        m_winKeyCooldownTimer->start();
    }
}

void DockManager::onWinKeyCooldownTimeout()
{
    qInfo() << "Win 键冷却结束，恢复正常全屏检测";
    // 冷却结束，重新检查全屏状态
    if (m_sysHelper && m_currentState == DockState::Docked) {
        bool anyMax = m_sysHelper->hasMaximizedOrFullscreenWindowOnMonitor(m_monitorIndex);
        if (anyMax) {
            qInfo() << "冷却结束后仍有全屏窗口，启动隐藏延迟";
            m_hideDelayTimer->start();
        }
    }
}
