/**
 * @file DockWindow_transition.cpp
 * @brief DockWindow 显示/隐藏过渡动画实现
 */

#include "ui/DockWindow.h"
#include "ui/DockAnimation.h"
#include <QPropertyAnimation>
#include <QScreen>
#include <QGuiApplication>

void DockWindow::onStateChanged(DockState newState)
{
    if (m_slideAnim) {
        m_slideAnim->stop();
        m_slideAnim->deleteLater();
        m_slideAnim = nullptr;
    }

    switch (newState) {
    case DockState::Docked: {
        m_isHidden = false;
        m_bottomEdgeTimer->stop();
        show();

        QRect geo = screen() ? screen()->geometry() : QGuiApplication::primaryScreen()->geometry();
        int targetY = geo.y() + geo.height() - height() - kBaseSpacing;
        int targetX = geo.x() + (geo.width() - width()) / 2;
        QPoint startPos(targetX, targetY + 60);
        QPoint endPos(targetX, targetY);

        m_slideAnim = new QPropertyAnimation(this, "pos");
        m_slideAnim->setDuration(m_animation->animDuration(300));
        m_slideAnim->setStartValue(startPos);
        m_slideAnim->setEndValue(endPos);
        m_slideAnim->setEasingCurve(QEasingCurve::OutCubic);

        QPropertyAnimation *fadeAnim = new QPropertyAnimation(this, "fadeOpacity");
        fadeAnim->setDuration(m_animation->animDuration(300));
        fadeAnim->setStartValue(0.0);
        fadeAnim->setEndValue(1.0);
        fadeAnim->setEasingCurve(QEasingCurve::OutCubic);

        connect(m_slideAnim, &QObject::destroyed, this, [this]() {
            m_slideAnim = nullptr;
        });

        m_slideAnim->start(QAbstractAnimation::DeleteWhenStopped);
        fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
        break;
    }
    case DockState::Hidden: {
        m_isHidden = true;
        m_bottomEdgeTimer->start();

        QPoint currentPos = pos();
        QPoint endPos(currentPos.x(), currentPos.y() + 60);

        m_slideAnim = new QPropertyAnimation(this, "pos");
        m_slideAnim->setDuration(250);
        m_slideAnim->setStartValue(currentPos);
        m_slideAnim->setEndValue(endPos);
        m_slideAnim->setEasingCurve(QEasingCurve::InCubic);

        QPropertyAnimation *fadeAnim = new QPropertyAnimation(this, "fadeOpacity");
        fadeAnim->setDuration(m_animation->animDuration(250));
        fadeAnim->setStartValue(1.0);
        fadeAnim->setEndValue(0.0);
        fadeAnim->setEasingCurve(QEasingCurve::InCubic);

        connect(m_slideAnim, &QObject::destroyed, this, [this]() {
            m_slideAnim = nullptr;
        });

        connect(fadeAnim, &QPropertyAnimation::finished, this, [this]() {
            hide();
            m_fadeOpacity = 1.0;
            setWindowOpacity(1.0);
        });

        m_slideAnim->start(QAbstractAnimation::DeleteWhenStopped);
        fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
        break;
    }
    }
}
