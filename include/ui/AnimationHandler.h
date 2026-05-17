#ifndef ANIMATIONHANDLER_H
#define ANIMATIONHANDLER_H

#include <QObject>
#include <QPropertyAnimation>
#include <QEasingCurve>

/**
 * @file AnimationHandler.h
 * @brief Dock 动画控制器
 *
 * 处理鱼眼缩放、Dock 隐藏/弹出、图标弹跳等动画的调度与参数管理。
 */

class AnimationHandler : public QObject {
    Q_OBJECT
public:
    explicit AnimationHandler(QObject *parent = nullptr);

    /** @brief 创建鱼眼放大动画 */
    QPropertyAnimation* createMagnifyAnimation(QObject *target, const QByteArray &property,
                                                qreal endValue, int durationMs = 150,
                                                QEasingCurve::Type curve = QEasingCurve::OutCubic);

    /** @brief 创建 Dock 隐藏动画 */
    QPropertyAnimation* createHideAnimation(QObject *target, const QByteArray &property,
                                             qreal endValue, int durationMs = 200,
                                             QEasingCurve::Type curve = QEasingCurve::OutQuad);

    /** @brief 创建 Dock 弹出动画 */
    QPropertyAnimation* createShowAnimation(QObject *target, const QByteArray &property,
                                             qreal endValue, int durationMs = 200,
                                             QEasingCurve::Type curve = QEasingCurve::OutBack);

signals:
    void animationFinished();

private:
    QPropertyAnimation* makeAnimation(QObject *target, const QByteArray &property,
                                      qreal endValue, int durationMs, QEasingCurve::Type curve);
};

#endif // ANIMATIONHANDLER_H
