/**
 * @file DockWindow_itemmanager.cpp
 * @brief DockWindow 图标生命周期管理实现
 */

#include "ui/DockWindow.h"
#include "ui/DockItem.h"
#include "ui/DockAnimation.h"
#include "ui/OverflowPanel.h"
#include "core/DockManager.h"
#include "core/ConfigManager.h"
#include "core/AppIdHelper.h"
#include "core/WindowCache.h"

#include <QCursor>

void DockWindow::onItemAdded(const DockItemData &data)
{
    if (m_itemMap.contains(data.appId)) return;

    DockItem *item = new DockItem(data.appId, data.iconPath, data.displayName, this);
    item->setExecPath(data.execPath);
    item->setTargetPath(data.targetPath);
    item->setAppUserModelId(data.appUserModelId);
    item->setRunning(data.isRunning);
    item->setBadgeCount(data.badgeCount);

    if (m_config) {
        int iconSize = m_config->get(QStringLiteral("iconSize"), 48).toInt();
        item->setBaseSize(qBound(24, iconSize, 128));
    }

    m_itemMap[data.appId] = item;
    m_items.append(item);

    connect(item, &DockItem::clicked, this, [this, item](const QString &) {
        handleSingleClick(item);
    });

    connect(item, &DockItem::pinRequested, this, [this](const QString &appId, bool pin) {
        if (!m_dockManager) return;
        if (pin) {
            for (const auto &d : m_dockManager->items()) {
                if (d.appId == appId) {
                    m_dockManager->pinItem(d);
                    return;
                }
            }
        } else {
            m_dockManager->unpinItem(appId);
        }
    });
    connect(item, &DockItem::hoverEntered, this, [this](int) {
        updateHoverStateAtGlobalPosition(QCursor::pos());
    });
    connect(item, &DockItem::hoverLeft, this, [this]() {
        updateHoverStateAtGlobalPosition(QCursor::pos());
    });

    item->installEventFilter(this);
    item->show();
    relayoutItems();

    m_animation->animateItemAdd(item);
}

void DockWindow::onItemRemoved(const QString &appId)
{
    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;

    DockItem *item = it.value();
    m_itemMap.erase(it);

    m_animation->animateItemRemove(item, [this, item]() {
        m_items.removeOne(item);
        item->deleteLater();
    });
}

void DockWindow::onItemStateChanged(const QString &appId, bool isRunning)
{
    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;
    it.value()->setRunning(isRunning);
}

void DockWindow::onItemWindowCountChanged(const QString &appId, int count)
{
    auto it = m_itemMap.find(appId);
    if (it != m_itemMap.end()) {
        it.value()->setWindowCount(count);
    }
}

void DockWindow::updateWindowCounts()
{
    if (!m_sysHelper || !m_dockManager || !m_windowCache) return;

    m_windowCache->refresh();

    for (auto it = m_itemMap.begin(); it != m_itemMap.end(); ++it) {
        DockItem *item = it.value();
        if (!item->isRunning()) continue;

        DockItemData data;
        data.appId = item->appId();
        data.execPath = item->execPath();
        data.targetPath = item->targetPath();
        data.appUserModelId = item->appUserModelId();
        QString wmClass = AppIdHelper::primaryIdentityKey(data);

        int count = m_windowCache->getWindowCount(wmClass);
        if (count > 0) {
            m_dockManager->updateWindowCount(item->appId(), count);
        }

        item->setForegroundActive(m_windowCache->isForegroundApp(wmClass));
    }
}

void DockWindow::updateOverflowItem()
{
    if (!m_dockManager) return;

    bool hasOverflow = !m_dockManager->overflowItems().isEmpty();

    if (hasOverflow && !m_overflowItem) {
        m_overflowItem = new DockItem("__overflow__", "", "...", this);
        m_overflowItem->setFixedSize(48, 48);
        m_itemMap["__overflow__"] = m_overflowItem;
        m_items.append(m_overflowItem);

        connect(m_overflowItem, &DockItem::clicked, this, [this](const QString &) {
            showOverflowPopup();
        });

        m_overflowItem->installEventFilter(this);
        m_overflowItem->show();
        relayoutItems();
        updatePosition();
        m_animation->animateItemAdd(m_overflowItem);

    } else if (!hasOverflow && m_overflowItem) {
        auto it = m_itemMap.find("__overflow__");
        if (it != m_itemMap.end()) {
            m_itemMap.erase(it);
        }
        DockItem *removing = m_overflowItem;
        m_overflowItem = nullptr;
        m_items.removeOne(removing);
        m_animation->animateItemRemove(removing, [removing]() {
            removing->deleteLater();
        });
    }
}

void DockWindow::showOverflowPopup()
{
    m_overflowPanel->showPopup(m_overflowItem, this);
}

void DockWindow::onOverflowChanged()
{
    updateOverflowItem();
}
