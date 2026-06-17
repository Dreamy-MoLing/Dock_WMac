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
#include "ui/WindowPreviewPanel.h"

#include <QDir>
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
    m_animation->resetFishEye(m_items);
    if (!m_animation->isFishEyeLocked())
        m_hoveredIndex = -1;
    m_windowPreview->startDelayedHide();
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
    if (m_items.isEmpty()) return;

    int index = itemAtPos(event->pos().x(), event->pos().y());
    if (index != m_hoveredIndex && !m_animation->isFishEyeLocked()) {
        m_hoveredIndex = index;
        if (index >= 0)
            m_animation->applyFishEye(index, m_items);
        else
            m_animation->resetFishEye(m_items);
    }
}

bool DockWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseMove && obj->isWidgetType()) {
        QWidget *w = static_cast<QWidget *>(obj);
        if (w->parent() == this && !m_items.isEmpty()) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            QPoint posInDock = mapFromGlobal(me->globalPosition().toPoint());
            int index = itemAtPos(posInDock.x(), posInDock.y());
            if (index != m_hoveredIndex && !m_animation->isFishEyeLocked()) {
                m_hoveredIndex = index;
                if (index >= 0)
                    m_animation->applyFishEye(index, m_items);
                else
                    m_animation->resetFishEye(m_items);
            }

            if (index >= 0) {
                DockItem *hoveredItem = m_items[index];
                if (hoveredItem != m_previewItem) {
                    m_windowPreview->hidePreview();
                    m_previewItem = hoveredItem;
                    m_windowPreview->showPreview(hoveredItem);
                }
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void DockWindow::launchApp(DockItem *item)
{
    if (!item || item->execPath().isEmpty()) return;
    QString nativePath = QDir::toNativeSeparators(item->execPath());
    QProcess::startDetached(nativePath, QStringList());
}

void DockWindow::handleSingleClick(DockItem *item)
{
    if (!item || !m_clickStateMachine) {
        launchApp(item);
        return;
    }

    QString wmClass = AppIdHelper::deriveWmClass(item->execPath(), item->appId());
    bool isRunning = item->isRunning();

    m_clickStateMachine->handleClick(wmClass, item->execPath(), isRunning);

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



