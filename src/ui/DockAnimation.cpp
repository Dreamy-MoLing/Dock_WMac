/**
 * @file DockAnimation.cpp
 * @brief Dock 动画控制器实现
 *
 * 从 DockWindow 拆分：鱼眼高斯缩放动画 + 图标添加/移除动画。
 * relayout 通过批处理定时器去抖，帧级合并为单次调用。
 */

#include "ui/DockAnimation.h"
#include "ui/DockItem.h"
#include "core/ConfigManager.h"
#include <QtMath>
#include <QVariant>

DockAnimation::DockAnimation(QObject *parent)
    : QObject(parent)
    , m_batchRelayoutTimer(new QTimer(this))
{
    m_batchRelayoutTimer->setSingleShot(true);
    m_batchRelayoutTimer->setInterval(0);
    connect(m_batchRelayoutTimer, &QTimer::timeout, this, &DockAnimation::relayoutRequested);
}

void DockAnimation::requestRelayout()
{
    if (!m_batchRelayoutTimer->isActive())
        m_batchRelayoutTimer->start();
}

int DockAnimation::animDuration(int fallbackMs) const
{
    if (m_config) {
        return m_config->get(QStringLiteral("animationDuration"), fallbackMs).toInt();
    }
    return fallbackMs;
}

// ─── 鱼眼动画 ─────────────────────────────────────────────

void DockAnimation::animateItemToScale(DockItem *item, qreal targetScale)
{
    auto it = m_fishEyeAnims.find(item);
    if (it != m_fishEyeAnims.end()) {
        QPropertyAnimation *existing = it.value();
        if (existing->state() == QAbstractAnimation::Running) {
            existing->stop();
        }
        existing->setStartValue(item->visualScale());
        existing->setEndValue(targetScale);
        existing->start();
        return;
    }

    if (qFuzzyCompare(item->visualScale(), targetScale)) {
        return;
    }

    QPropertyAnimation *anim = new QPropertyAnimation(item, "visualScale", this);
    anim->setDuration(100);  // 鱼眼游走：100ms 跟手不拖尾
    anim->setStartValue(item->visualScale());
    anim->setEndValue(targetScale);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
        requestRelayout();
    });

    connect(anim, &QPropertyAnimation::finished, this, [this, item]() {
        m_fishEyeAnims.remove(item);
    });

    m_fishEyeAnims[item] = anim;
    anim->start();
}

void DockAnimation::applyFishEye(int hoveredIndex, const QList<DockItem *> &items)
{
    if (m_fishEyeLocked) return;

    qreal maxScale = m_config
        ? m_config->get(QStringLiteral("magnification"), 1.5).toReal()
        : 1.5;
    const qreal sigma = 1.5;

    for (int i = 0; i < items.size(); ++i) {
        qreal d = qAbs(i - hoveredIndex);
        qreal factor = 1.0 + (maxScale - 1.0) * qExp(-d * d / (2.0 * sigma * sigma));
        animateItemToScale(items[i], factor);
    }
}

void DockAnimation::resetFishEye(const QList<DockItem *> &items)
{
    if (m_fishEyeLocked) return;
    for (int i = 0; i < items.size(); ++i) {
        animateItemToScale(items[i], 1.0);
    }
}

void DockAnimation::lockFishEye(int index)
{
    m_fishEyeLocked = true;
    m_fishEyeLockedIndex = index;
}

void DockAnimation::unlockFishEye(const QList<DockItem *> &items)
{
    m_fishEyeLocked = false;
    m_fishEyeLockedIndex = -1;
    resetFishEye(items);
}

// ─── 图标添加/移除动画 ──────────────────────────────────────

void DockAnimation::animateItemAdd(DockItem *item)
{
    if (!item) return;

    item->setVisualScale(0.0);

    QPropertyAnimation *anim = new QPropertyAnimation(item, "visualScale", this);
    anim->setDuration(200);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutBack);

    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
        requestRelayout();
    });

    connect(anim, &QPropertyAnimation::finished, this, [this, item]() {
        QPropertyAnimation *bounce = new QPropertyAnimation(item, "visualScale", this);
        bounce->setDuration(400);
        bounce->setStartValue(1.0);
        bounce->setKeyValueAt(0.5, 1.2);
        bounce->setEndValue(1.0);
        bounce->setEasingCurve(QEasingCurve::OutBounce);

        connect(bounce, &QPropertyAnimation::valueChanged, this, [this]() {
            requestRelayout();
        });

        bounce->start(QAbstractAnimation::DeleteWhenStopped);
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void DockAnimation::animateItemRemove(DockItem *item, std::function<void()> onFinished)
{
    if (!item) return;

    // 清理关联的鱼眼动画
    auto animIt = m_fishEyeAnims.find(item);
    if (animIt != m_fishEyeAnims.end()) {
        animIt.value()->stop();
        animIt.value()->deleteLater();
        m_fishEyeAnims.erase(animIt);
    }

    QPropertyAnimation *anim = new QPropertyAnimation(item, "visualScale", this);
    anim->setDuration(animDuration(150));
    anim->setStartValue(item->visualScale());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);

    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
        requestRelayout();
    });

    connect(anim, &QPropertyAnimation::finished, this, [this, onFinished]() {
        onFinished();
        emit relayoutRequested();
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
