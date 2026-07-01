#ifndef DOCKANIMATION_H
#define DOCKANIMATION_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QPropertyAnimation>
#include <QTimer>
#include <functional>

class DockItem;
class ConfigManager;

/**
 * @file DockAnimation.h
 * @brief Dock 动画控制器
 *
 * 封装所有 QPropertyAnimation 逻辑：鱼眼放大、图标添加/移除动画。
 * 从 DockWindow 拆分出来，通过 relayoutRequested() 信号通知布局更新。
 */

class DockAnimation : public QObject {
    Q_OBJECT
public:
    explicit DockAnimation(QObject *parent = nullptr);

    void setConfig(ConfigManager *config) { m_config = config; }

    // ── 鱼眼 ──
    void applyFishEye(int hoveredIndex, const QList<DockItem *> &items);
    void resetFishEye(const QList<DockItem *> &items);
    void lockFishEye(int index);
    void unlockFishEye(const QList<DockItem *> &items);
    bool isFishEyeLocked() const { return m_fishEyeLocked; }
    int  fishEyeLockedIndex() const { return m_fishEyeLockedIndex; }

    // ── 图标添加/移除 ──
    void animateItemAdd(DockItem *item);
    void animateItemRemove(DockItem *item, std::function<void()> onFinished);

    // ── 动画时长 ──
    int  animDuration(int fallbackMs) const;

signals:
    /** @brief 需要重新布局（通过批处理定时器去抖） */
    void relayoutRequested();

private:
    void animateItemToScale(DockItem *item, qreal targetScale);
    void requestRelayout();

    ConfigManager *m_config = nullptr;

    // 鱼眼动画
    QMap<DockItem *, QPropertyAnimation *> m_fishEyeAnims;
    bool m_fishEyeLocked = false;
    int  m_fishEyeLockedIndex = -1;

    // 帧级批处理 relayout（避免每帧多次 O(N²) 计算）
    QTimer *m_batchRelayoutTimer;
};

#endif // DOCKANIMATION_H
