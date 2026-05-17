/**
 * @file AnimationHandler.cpp
 * @brief Dock 动画控制器实现
 *
 * 封装 QPropertyAnimation，提供预配置的鱼眼缩放、
 * Dock 隐藏/弹出、图标弹跳等动画工厂方法。
 */

#include "ui/AnimationHandler.h"

AnimationHandler::AnimationHandler(QObject *parent)
    : QObject(parent)
{
}

QPropertyAnimation* AnimationHandler::makeAnimation(
    QObject *target, const QByteArray &property,
    qreal endValue, int durationMs, QEasingCurve::Type curve)
{
    QPropertyAnimation *anim = new QPropertyAnimation(target, property, this);
    anim->setDuration(durationMs);
    anim->setEndValue(endValue);
    anim->setEasingCurve(curve);
    connect(anim, &QPropertyAnimation::finished, this, &AnimationHandler::animationFinished);
    return anim;
}

QPropertyAnimation* AnimationHandler::createMagnifyAnimation(
    QObject *target, const QByteArray &property,
    qreal endValue, int durationMs, QEasingCurve::Type curve)
{
    return makeAnimation(target, property, endValue, durationMs, curve);
}

QPropertyAnimation* AnimationHandler::createHideAnimation(
    QObject *target, const QByteArray &property,
    qreal endValue, int durationMs, QEasingCurve::Type curve)
{
    return makeAnimation(target, property, endValue, durationMs, curve);
}

QPropertyAnimation* AnimationHandler::createShowAnimation(
    QObject *target, const QByteArray &property,
    qreal endValue, int durationMs, QEasingCurve::Type curve)
{
    return makeAnimation(target, property, endValue, durationMs, curve);
}


