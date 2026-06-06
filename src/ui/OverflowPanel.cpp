/**
 * @file OverflowPanel.cpp
 * @brief 溢出弹出面板实现
 *
 * 从 DockWindow 提取溢出弹出菜单的创建逻辑。
 * 溢出图标生命周期仍由 DockWindow 管理。
 */
#include "ui/OverflowPanel.h"
#include "ui/DockItem.h"
#include "core/DockManager.h"
#include "core/SysHelper.h"
#include "core/WindowCache.h"
#include "core/AppIdHelper.h"

#include <QVBoxLayout>
#include <QPushButton>

OverflowPanel::OverflowPanel(QObject *parent)
    : QObject(parent)
{
}

void OverflowPanel::setDockManager(DockManager *manager)
{
    m_dockManager = manager;
}

void OverflowPanel::setSysHelper(SysHelper *helper)
{
    m_sysHelper = helper;
}

void OverflowPanel::setWindowCache(WindowCache *cache)
{
    m_windowCache = cache;
}

void OverflowPanel::showPopup(DockItem *anchorItem, QWidget * /*dockParent*/)
{
    if (!m_dockManager || !anchorItem) return;

    hidePopup();

    auto overflowItems = m_dockManager->overflowItems();
    if (overflowItems.isEmpty()) return;

    m_popup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_popup->setAttribute(Qt::WA_TranslucentBackground);

    int itemH = 40, popupW = 200;
    int popupH = overflowItems.size() * itemH + 16;
    m_popup->setFixedSize(popupW, popupH);

    QPoint globalPos = anchorItem->mapToGlobal(QPoint(0, 0));
    m_popup->move(globalPos.x() - popupW / 2 + anchorItem->width() / 2,
                  globalPos.y() - popupH - 8);

    QVBoxLayout *layout = new QVBoxLayout(m_popup);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(2);

    for (const auto &data : overflowItems) {
        QPushButton *btn = new QPushButton(data.displayName, m_popup);
        btn->setFixedHeight(itemH - 4);
        btn->setStyleSheet(
            "QPushButton { background: rgba(60,60,60,200); color: white; "
            "border: none; border-radius: 6px; text-align: left; padding-left: 12px; font-size: 13px; }"
            "QPushButton:hover { background: rgba(80,80,80,220); }"
        );
        connect(btn, &QPushButton::clicked, this, [this, data]() {
            if (m_windowCache) {
                QString wmClass = AppIdHelper::deriveWmClass(data.execPath, data.appId);
                m_windowCache->activateWindow(wmClass);
            }
            hidePopup();
        });
        layout->addWidget(btn);
    }
    m_popup->show();
}

void OverflowPanel::hidePopup()
{
    if (m_popup) {
        m_popup->close();
        m_popup->deleteLater();
        m_popup = nullptr;
    }
}
