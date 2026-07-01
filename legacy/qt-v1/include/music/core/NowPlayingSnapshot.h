#ifndef NOWPLAYINGSNAPSHOT_H
#define NOWPLAYINGSNAPSHOT_H

#include <QMetaType>
#include <QByteArray>
#include <QString>

namespace music {

enum class PlaybackStatus {
    Unavailable,
    Stopped,
    Paused,
    Playing
};

struct NowPlayingSnapshot {
    bool available = false;
    QString source;
    QString title;
    QString artist;
    QString album;
    qint64 positionMs = 0;
    qint64 durationMs = 0;
    PlaybackStatus status = PlaybackStatus::Unavailable;
    bool canPlayPause = false;
    bool canPrevious = false;
    bool canNext = false;
    bool hasArtwork = false;
    QByteArray artworkData;
    qreal audioLevel = 0.0;
    bool hasLyrics = false;
    QString previousLyric;
    QString currentLyric;
    QString nextLyric;
};

} // namespace music

Q_DECLARE_METATYPE(music::NowPlayingSnapshot)

#endif // NOWPLAYINGSNAPSHOT_H
