/**
 * @file DockWindow_theme.cpp
 * @brief DockWindow 毛玻璃 & 主题相关实现
 */

#include "ui/DockWindow.h"
#include "ui/DockItem.h"
#include "core/SysHelper.h"
#include "core/ConfigManager.h"
#include <QPainter>
#include <QDebug>

// ─── 绘制 ────────────────────────────────────────────────

void DockWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_items.isEmpty() && !hasActiveRightAccessoryPanel()) return;

    int cornerRadius = m_config
        ? m_config->get(QStringLiteral("cornerRadius"), 16).toInt()
        : 16;

    int baseSize = m_items.isEmpty()
        ? (m_config ? m_config->get(QStringLiteral("iconSize"), 48).toInt() : 48)
        : m_items[0]->baseSize();
    int barH = kMarginTop + baseSize + kMarginBottom;
    int barTop = height() - barH;
    QRect barRect(0, barTop, width(), barH);

    // 柔和扩散阴影（多层渐变）
    painter.setPen(Qt::NoPen);
    for (int i = 3; i >= 0; --i) {
        int spread = (i + 1) * 3;
        int alpha = 12 - i * 3;
        painter.setBrush(QColor(0, 0, 0, alpha));
        painter.drawRoundedRect(barRect.adjusted(-spread, 1, spread, spread + 4),
                                cornerRadius + i * 2, cornerRadius + i * 2);
    }

    // 主题自适应背景色
    QColor bgColor;
    QColor borderColor;
    if (m_isLightTheme) {
        bgColor = QColor(245, 245, 245, static_cast<int>(0.65 * 255));
        borderColor = QColor(200, 200, 200, 80);
    } else {
        bgColor = QColor(40, 40, 40, static_cast<int>(0.55 * 255));
        borderColor = QColor(80, 80, 80, 60);
    }

    painter.setBrush(bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawRoundedRect(barRect.adjusted(1, 1, -1, -1), cornerRadius, cornerRadius);
}

// ─── 毛玻璃 & 主题 ──────────────────────────────────────────

void DockWindow::initBlurEffect()
{
    if (m_blurInitialized || !m_sysHelper) return;

    if (m_sysHelper->isBlurSupported()) {
        updateBlurRegion();
        m_blurInitialized = true;
        qInfo() << "DWM 毛玻璃效果已启用";
    } else {
        qInfo() << "DWM 模糊不支持，使用纯色半透明背景";
    }
}

void DockWindow::updateBlurRegion()
{
    if (!m_sysHelper || !m_blurInitialized) return;

    QSize currentSize(width(), height());
    if (currentSize == m_lastBlurSize) return;
    m_lastBlurSize = currentSize;

    int barH = kMarginTop + m_baseIconSize + kMarginBottom;
    QRect barRect(0, height() - barH, width(), barH);
    m_sysHelper->enableBlurBehindWindow(winId(), barRect, 16);
}

void DockWindow::updateTheme()
{
    if (!m_sysHelper) return;
    bool newTheme = m_sysHelper->isLightTheme();
    if (newTheme != m_isLightTheme) {
        m_isLightTheme = newTheme;
        qInfo() << "主题切换:" << (m_isLightTheme ? "亮色" : "暗色");
        update();
    }
}
