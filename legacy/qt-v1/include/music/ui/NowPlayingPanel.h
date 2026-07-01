#ifndef NOWPLAYINGPANEL_H
#define NOWPLAYINGPANEL_H

#include "music/core/NowPlayingSnapshot.h"

#include <QColor>
#include <QPixmap>
#include <QWidget>

class QPainter;
class QTimer;

namespace music {

class NowPlayingPanel : public QWidget {
    Q_OBJECT
public:
    explicit NowPlayingPanel(QWidget *parent = nullptr);

    bool isPanelActive() const;
    bool isExpanded() const { return m_expanded; }
    int effectiveWidth() const;
    QString currentLyric() const { return m_snapshot.currentLyric; }
    qreal audioLevel() const { return m_snapshot.audioLevel; }

    void setIconBaseSize(int size);

public slots:
    void setSnapshot(const music::NowPlayingSnapshot &snapshot);

signals:
    void layoutChanged();
    void previousRequested();
    void togglePlayPauseRequested();
    void nextRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class MediaButton {
        Previous,
        Play,
        Pause,
        Next
    };

    void updateSize();
    void updateArtwork();
    void updateAccentColor();
    void updateVisualTimer();
    void drawIcon(QPainter *painter, const QRect &rect);
    void drawExpandedPanel(QPainter *painter, const QRect &rect);
    void drawWave(QPainter *painter, const QRect &rect);
    void drawScrollingText(QPainter *painter, const QRect &rect, const QString &text);
    void drawMediaButton(QPainter *painter, const QRect &rect, MediaButton button, bool enabled);
    QRect iconRect() const;
    QRect panelRect() const;
    QRect previousRect() const;
    QRect playPauseRect() const;
    QRect nextRect() const;

    NowPlayingSnapshot m_snapshot;
    QPixmap m_artwork;
    QColor m_accentColor;
    int m_iconBaseSize = 48;
    bool m_expanded = true;
    QTimer *m_visualTimer = nullptr;
    qreal m_visualPhase = 0.0;
};

} // namespace music

#endif // NOWPLAYINGPANEL_H
