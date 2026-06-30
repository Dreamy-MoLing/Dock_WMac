/**
 * @file DockWindow_input.cpp
 * @brief DockWindow mouse, click, drag, and item hit-test handling.
 */

#include "ui/DockWindow.h"
#include "ui/DockItem.h"
#include "ui/DockAnimation.h"
#include "core/AppIdHelper.h"
#include "core/ClickStateMachine.h"
#include "core/DockManager.h"
#include "core/SysHelper.h"
#include "core/Types.h"
#include "ui/WindowPreviewPanel.h"

#include <QDir>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEnterEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QProcess>
#include <QTimer>
#include <utility>

void DockWindow::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
}

void DockWindow::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (rect().contains(mapFromGlobal(QCursor::pos())))
        return;
    clearHoverState(true);
}

int DockWindow::itemAtPos(int mouseX, int mouseY) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        QRect r = m_items[i]->geometry();
        if (mouseX >= r.left() && mouseX <= r.right() &&
            mouseY >= r.top() && mouseY <= r.bottom()) {
            return i;
        }
    }
    return -1;
}

void DockWindow::mouseMoveEvent(QMouseEvent *event)
{
    updateHoverStateAtGlobalPosition(event->globalPosition().toPoint());
}

bool DockWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj->isWidgetType()) {
        QWidget *w = static_cast<QWidget *>(obj);
        if (w->parent() == this && !m_items.isEmpty()
            && (event->type() == QEvent::MouseMove
                || event->type() == QEvent::Enter
                || event->type() == QEvent::Leave)) {
            QPoint globalPos = QCursor::pos();
            if (event->type() == QEvent::MouseMove) {
                QMouseEvent *me = static_cast<QMouseEvent *>(event);
                globalPos = me->globalPosition().toPoint();
            }
            updateHoverStateAtGlobalPosition(globalPos);
        }
    }

    return QWidget::eventFilter(obj, event);
}

void DockWindow::updateHoverStateAtGlobalPosition(const QPoint &globalPos)
{
    if (m_items.isEmpty()) return;

    const QPoint posInDock = mapFromGlobal(globalPos);
    const int index = itemAtPos(posInDock.x(), posInDock.y());
    DockItem *hoveredItem = index >= 0 ? m_items[index] : nullptr;

    if (!hoveredItem) {
        clearHoverState(true);
        return;
    }

    m_windowPreview->cancelDelayedHide();

    if (hoveredItem != m_previewItem && (m_previewItem || m_windowPreview->isVisible())) {
        m_windowPreview->hidePreview();
    }

    if (!m_animation->isFishEyeLocked() && index != m_hoveredIndex) {
        m_hoveredIndex = index;
        m_animation->applyFishEye(index, m_items);
    }

    if (hoveredItem != m_previewItem) {
        m_previewItem = hoveredItem;
        m_windowPreview->showPreview(hoveredItem);
    }
}

void DockWindow::clearHoverState(bool delayedPreviewHide)
{
    if (!m_animation->isFishEyeLocked()) {
        m_hoveredIndex = -1;
        m_animation->resetFishEye(m_items);
    }

    if (!m_previewItem && !m_windowPreview->isVisible())
        return;

    if (delayedPreviewHide && m_windowPreview->isVisible())
        m_windowPreview->startDelayedHide();
    else
        m_windowPreview->hidePreview();
}

void DockWindow::launchApp(DockItem *item)
{
    if (!item || item->execPath().isEmpty()) return;
    SysHelper::launchPath(item->execPath());
}

void DockWindow::handleSingleClick(DockItem *item)
{
    if (!item || !m_clickStateMachine) {
        launchApp(item);
        return;
    }

    DockItemData data;
    data.appId = item->appId();
    data.execPath = item->execPath();
    data.targetPath = item->targetPath();
    data.appUserModelId = item->appUserModelId();
    const QStringList identityKeys = AppIdHelper::identityKeys(data);
    bool isRunning = item->isRunning();

    m_clickStateMachine->handleClick(identityKeys, item->execPath(), isRunning);

    QTimer::singleShot(1000, item, [item]() {
        item->triggerInteractionIndicator();
    });
}

QVariant DockWindow::isItemPinned(QVariant appId)
{
    if (!m_dockManager) return false;
    return m_dockManager->isPinned(appId.toString());
}

void DockWindow::unlockFishEye()
{
    m_hoveredIndex = -1;
    m_animation->unlockFishEye(m_items);
}
void DockWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void DockWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void DockWindow::dropEvent(QDropEvent *event)
{
    if (!m_dockManager || !event->mimeData()->hasText()) return;

    QString appId = event->mimeData()->text();
    if (appId == QStringLiteral("__overflow__") || !m_dockManager->isPinned(appId)) return;

    auto it = m_itemMap.find(appId);
    if (it == m_itemMap.end()) return;

    DockItem *draggedItem = it.value();

    int dropIndex = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        if (event->position().toPoint().x() > m_items[i]->geometry().center().x()) {
            dropIndex = i + 1;
        }
    }

    m_items.removeOne(draggedItem);
    m_items.insert(qBound(0, dropIndex, m_items.size()), draggedItem);
    relayoutItems();

    QStringList orderedPinnedAppIds;
    for (DockItem *item : std::as_const(m_items)) {
        if (item && item->appId() != QStringLiteral("__overflow__") && m_dockManager->isPinned(item->appId())) {
            orderedPinnedAppIds.append(item->appId());
        }
    }
    m_dockManager->reorderPinnedItems(orderedPinnedAppIds);

    event->acceptProposedAction();
}



