#include "music/ui/NowPlayingPanel.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>

#include <cmath>

namespace music {

namespace {

constexpr int kExpandedPanelWidth = 356;
constexpr qreal kPi = 3.14159265358979323846;

QString displayText(const QString &value, const QString &fallback)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

QColor averageColor(const QPixmap &pixmap)
{
    if (pixmap.isNull())
        return QColor(128, 92, 255);

    const QImage image = pixmap.toImage()
        .scaled(12, 12, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_ARGB32);

    int count = 0;
    int red = 0;
    int green = 0;
    int blue = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() < 32)
                continue;
            red += color.red();
            green += color.green();
            blue += color.blue();
            ++count;
        }
    }

    if (count == 0)
        return QColor(128, 92, 255);
    QColor color(red / count, green / count, blue / count);
    return color.saturation() < 80 ? color.lighter(135) : color;
}

} // namespace

NowPlayingPanel::NowPlayingPanel(QWidget *parent)
    : QWidget(parent)
    , m_accentColor(128, 92, 255)
{
    setObjectName(QStringLiteral("NowPlayingPanel"));
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    m_visualTimer = new QTimer(this);
    m_visualTimer->setInterval(50);
    connect(m_visualTimer, &QTimer::timeout, this, [this]() {
        m_visualPhase += 0.09 + qBound(0.0, m_snapshot.audioLevel, 1.0) * 0.16;
        update();
    });
    updateSize();
    hide();
}

bool NowPlayingPanel::isPanelActive() const
{
    return m_snapshot.available
        && m_snapshot.status != PlaybackStatus::Unavailable
        && m_snapshot.status != PlaybackStatus::Stopped;
}

int NowPlayingPanel::effectiveWidth() const
{
    if (!isPanelActive())
        return 0;
    return m_iconBaseSize + (m_expanded ? kExpandedPanelWidth : 0);
}

void NowPlayingPanel::setIconBaseSize(int size)
{
    const int bounded = qBound(32, size, 96);
    if (m_iconBaseSize == bounded)
        return;
    m_iconBaseSize = bounded;
    updateSize();
    emit layoutChanged();
}

void NowPlayingPanel::setSnapshot(const NowPlayingSnapshot &snapshot)
{
    const bool wasActive = isPanelActive();
    const int oldWidth = effectiveWidth();
    m_snapshot = snapshot;
    updateArtwork();
    updateAccentColor();
    updateSize();
    setVisible(isPanelActive());
    updateVisualTimer();
    update();

    if (wasActive != isPanelActive() || oldWidth != effectiveWidth())
        emit layoutChanged();
}

void NowPlayingPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (!isPanelActive())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_expanded)
        drawExpandedPanel(&painter, panelRect());
    drawIcon(&painter, iconRect());
}

void NowPlayingPanel::mousePressEvent(QMouseEvent *event)
{
    if (!isPanelActive() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint pos = event->position().toPoint();
    if (iconRect().contains(pos)) {
        m_expanded = !m_expanded;
        updateSize();
        updateVisualTimer();
        emit layoutChanged();
        update();
        event->accept();
        return;
    }

    if (m_expanded && previousRect().contains(pos) && m_snapshot.canPrevious) {
        emit previousRequested();
        return;
    }
    if (m_expanded && playPauseRect().contains(pos) && m_snapshot.canPlayPause) {
        emit togglePlayPauseRequested();
        return;
    }
    if (m_expanded && nextRect().contains(pos) && m_snapshot.canNext) {
        emit nextRequested();
        return;
    }

    QWidget::mousePressEvent(event);
}

void NowPlayingPanel::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        m_expanded = !m_expanded;
        updateSize();
        updateVisualTimer();
        emit layoutChanged();
        update();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void NowPlayingPanel::updateSize()
{
    const int panelHeight = qMax(96, m_iconBaseSize + 44);
    setFixedSize(qMax(1, effectiveWidth()), panelHeight);
}

void NowPlayingPanel::updateArtwork()
{
    QPixmap pixmap;
    if (m_snapshot.hasArtwork && !m_snapshot.artworkData.isEmpty())
        pixmap.loadFromData(m_snapshot.artworkData);
    m_artwork = pixmap;
}

void NowPlayingPanel::updateAccentColor()
{
    m_accentColor = averageColor(m_artwork);
}

void NowPlayingPanel::updateVisualTimer()
{
    const bool shouldRun = isPanelActive() && m_expanded;
    if (shouldRun && !m_visualTimer->isActive())
        m_visualTimer->start();
    else if (!shouldRun && m_visualTimer->isActive())
        m_visualTimer->stop();
}

QRect NowPlayingPanel::iconRect() const
{
    const int y = height() - m_iconBaseSize - 6;
    return QRect(0, y, m_iconBaseSize, m_iconBaseSize);
}

QRect NowPlayingPanel::panelRect() const
{
    return QRect(m_iconBaseSize - 10, 2, width() - m_iconBaseSize + 10, height() - 8);
}

QRect NowPlayingPanel::previousRect() const
{
    const QRect panel = panelRect();
    return QRect(panel.left() + 22, panel.top() + 10, 22, 22);
}

QRect NowPlayingPanel::playPauseRect() const
{
    const QRect panel = panelRect();
    return QRect(panel.left() + 18, panel.top() + 36, 30, 30);
}

QRect NowPlayingPanel::nextRect() const
{
    const QRect panel = panelRect();
    return QRect(panel.left() + 22, panel.bottom() - 24, 22, 22);
}

void NowPlayingPanel::drawIcon(QPainter *painter, const QRect &rect)
{
    painter->save();
    QColor glow = m_accentColor;
    glow.setAlpha(90);
    painter->setPen(Qt::NoPen);
    painter->setBrush(glow);
    painter->drawEllipse(rect.adjusted(-6, -6, 6, 6));

    QPainterPath clip;
    clip.addRoundedRect(rect, 12, 12);
    painter->setClipPath(clip);
    if (!m_artwork.isNull()) {
        painter->drawPixmap(rect, m_artwork.scaled(rect.size(),
            Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
        gradient.setColorAt(0.0, m_accentColor.lighter(140));
        gradient.setColorAt(1.0, m_accentColor.darker(150));
        painter->fillRect(rect, gradient);
        painter->setPen(QPen(Qt::white, 2));
        QFont font = painter->font();
        font.setPixelSize(qMax(18, rect.height() / 2));
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(rect, Qt::AlignCenter, QStringLiteral("♪"));
    }
    painter->setClipping(false);
    painter->setPen(QPen(QColor(255, 255, 255, 105), 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect.adjusted(0, 0, -1, -1), 12, 12);
    painter->restore();
}

void NowPlayingPanel::drawExpandedPanel(QPainter *painter, const QRect &rect)
{
    painter->save();
    QLinearGradient background(rect.topLeft(), rect.bottomRight());
    background.setColorAt(0.0, QColor(32, 35, 48, 238));
    background.setColorAt(1.0, QColor(9, 11, 18, 232));
    painter->setPen(QPen(QColor(255, 255, 255, 55), 1));
    painter->setBrush(background);
    painter->drawRoundedRect(rect, 18, 18);

    const QRect controlRect(rect.left() + 12, rect.top() + 8, 54, rect.height() - 16);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 18));
    painter->drawRoundedRect(controlRect, 14, 14);

    const QRect textRect(rect.left() + 78, rect.top() + 9,
                         rect.width() - 94, rect.height() - 16);
    drawWave(painter, textRect.adjusted(0, textRect.height() / 2 - 4, 0, -2));

    QFont titleFont = painter->font();
    titleFont.setPixelSize(11);
    painter->setFont(titleFont);
    painter->setPen(QColor(255, 255, 255, 145));
    const QString previous = m_snapshot.previousLyric.isEmpty()
        ? displayText(m_snapshot.artist, QStringLiteral("No lyrics"))
        : m_snapshot.previousLyric;
    drawScrollingText(painter, QRect(textRect.left(), textRect.top(),
        textRect.width(), 18), previous);

    QFont lyricFont = painter->font();
    lyricFont.setPixelSize(16);
    lyricFont.setBold(true);
    painter->setFont(lyricFont);
    painter->setPen(Qt::white);
    const QString current = m_snapshot.hasLyrics
        ? displayText(m_snapshot.currentLyric, QStringLiteral("..."))
        : displayText(m_snapshot.title, QStringLiteral("Unknown title"));
    drawScrollingText(painter, QRect(textRect.left(), textRect.top() + 24,
        textRect.width(), 28), current);

    QFont nextFont = painter->font();
    nextFont.setPixelSize(11);
    nextFont.setBold(false);
    painter->setFont(nextFont);
    painter->setPen(QColor(255, 255, 255, 125));
    const QString next = m_snapshot.nextLyric.isEmpty()
        ? displayText(m_snapshot.album, m_snapshot.source)
        : m_snapshot.nextLyric;
    drawScrollingText(painter, QRect(textRect.left(), textRect.bottom() - 17,
        textRect.width(), 18), next);

    drawMediaButton(painter, previousRect(), MediaButton::Previous, m_snapshot.canPrevious);
    drawMediaButton(painter, playPauseRect(),
        m_snapshot.status == PlaybackStatus::Playing ? MediaButton::Pause : MediaButton::Play,
        m_snapshot.canPlayPause);
    drawMediaButton(painter, nextRect(), MediaButton::Next, m_snapshot.canNext);

    painter->restore();
}

void NowPlayingPanel::drawWave(QPainter *painter, const QRect &rect)
{
    if (rect.width() <= 0 || rect.height() <= 0)
        return;

    const qreal level = qBound(0.0, m_snapshot.audioLevel, 1.0);
    const qreal visualLevel = 0.22 + level * 0.78;
    const qreal mid = rect.center().y();
    const qreal amp = qMax<qreal>(7.0, rect.height() * (0.22 + visualLevel * 0.56));

    QPainterPath path;
    path.moveTo(rect.left(), rect.bottom());
    path.lineTo(rect.left(), mid);
    const int segments = 10;
    for (int i = 0; i <= segments; ++i) {
        const qreal t = static_cast<qreal>(i) / segments;
        const qreal x = rect.left() + rect.width() * t;
        const qreal envelope = 0.45 + 0.55 * qSin(t * kPi);
        const qreal y = mid + qSin(t * kPi * 3.2 + m_visualPhase) * amp * envelope;
        if (i == 0)
            path.lineTo(x, y);
        else
            path.quadTo(x - rect.width() / (segments * 2.0), mid, x, y);
    }
    path.lineTo(rect.right(), rect.bottom());
    path.closeSubpath();

    QLinearGradient waveGradient(rect.topLeft(), rect.bottomRight());
    QColor a = m_accentColor.lighter(135);
    QColor b = m_accentColor.darker(130);
    a.setAlpha(120 + static_cast<int>(level * 70));
    b.setAlpha(72 + static_cast<int>(level * 54));
    waveGradient.setColorAt(0.0, a);
    waveGradient.setColorAt(1.0, b);

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(waveGradient);
    painter->drawPath(path);
    painter->restore();
}

void NowPlayingPanel::drawScrollingText(QPainter *painter, const QRect &rect, const QString &text)
{
    const QString label = text.trimmed();
    if (label.isEmpty() || rect.width() <= 0)
        return;

    const QFontMetrics metrics(painter->font());
    const int textWidth = metrics.horizontalAdvance(label);
    const int baseline = rect.top() + (rect.height() + metrics.ascent() - metrics.descent()) / 2;

    painter->save();
    painter->setClipRect(rect);
    if (textWidth <= rect.width()) {
        painter->drawText(rect.left(), baseline, label);
    } else {
        const int gap = 44;
        const qreal span = textWidth + gap;
        const int offset = static_cast<int>(std::fmod(m_visualPhase * 30.0, span));
        const int x = rect.left() - offset;
        painter->drawText(x, baseline, label);
        painter->drawText(x + textWidth + gap, baseline, label);
    }
    painter->restore();
}

void NowPlayingPanel::drawMediaButton(QPainter *painter, const QRect &rect,
                                      MediaButton button, bool enabled)
{
    painter->save();
    painter->setPen(Qt::NoPen);

    QColor fill = enabled ? QColor(255, 255, 255, 42) : QColor(255, 255, 255, 14);
    if (button == MediaButton::Play || button == MediaButton::Pause) {
        fill = enabled ? m_accentColor.lighter(125) : QColor(255, 255, 255, 16);
        fill.setAlpha(enabled ? 210 : 28);
    }
    painter->setBrush(fill);
    painter->drawEllipse(rect);

    const QColor iconColor = enabled ? QColor(255, 255, 255, 232) : QColor(255, 255, 255, 78);
    painter->setBrush(iconColor);
    painter->setPen(QPen(iconColor, qMax(1, rect.width() / 12), Qt::SolidLine, Qt::RoundCap));

    const QRectF icon = QRectF(rect).adjusted(rect.width() * 0.28, rect.height() * 0.25,
                                             -rect.width() * 0.26, -rect.height() * 0.25);
    QPainterPath path;
    switch (button) {
    case MediaButton::Previous:
        painter->drawLine(QPointF(icon.left(), icon.top()), QPointF(icon.left(), icon.bottom()));
        path.moveTo(icon.right(), icon.top());
        path.lineTo(icon.left() + icon.width() * 0.18, icon.center().y());
        path.lineTo(icon.right(), icon.bottom());
        path.closeSubpath();
        painter->drawPath(path);
        break;
    case MediaButton::Next:
        painter->drawLine(QPointF(icon.right(), icon.top()), QPointF(icon.right(), icon.bottom()));
        path.moveTo(icon.left(), icon.top());
        path.lineTo(icon.right() - icon.width() * 0.18, icon.center().y());
        path.lineTo(icon.left(), icon.bottom());
        path.closeSubpath();
        painter->drawPath(path);
        break;
    case MediaButton::Play:
        path.moveTo(icon.left(), icon.top());
        path.lineTo(icon.right(), icon.center().y());
        path.lineTo(icon.left(), icon.bottom());
        path.closeSubpath();
        painter->drawPath(path);
        break;
    case MediaButton::Pause: {
        const qreal barW = icon.width() * 0.28;
        painter->drawRoundedRect(QRectF(icon.left(), icon.top(), barW, icon.height()), 1.5, 1.5);
        painter->drawRoundedRect(QRectF(icon.right() - barW, icon.top(), barW, icon.height()), 1.5, 1.5);
        break;
    }
    }
    painter->restore();
}

} // namespace music
