#ifndef MUSICSESSIONSERVICE_H
#define MUSICSESSIONSERVICE_H

#include "music/core/NowPlayingSnapshot.h"

#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <memory>

namespace music {

class MusicSessionServicePrivate;

class MusicSessionService : public QObject {
    Q_OBJECT
public:
    explicit MusicSessionService(QObject *parent = nullptr);
    ~MusicSessionService() override;

    void start();
    void setExternalLyricsEnabled(bool enabled);

public slots:
    void refreshNowPlaying();
    void togglePlayPause();
    void previous();
    void next();

signals:
    void snapshotChanged(const music::NowPlayingSnapshot &snapshot);

private:
    void publishUnavailable(const QString &source = QString());
    void publishSnapshot(const NowPlayingSnapshot &snapshot);
    void publishAudioLevel(qreal level);
    void refreshLyricsIfNeeded(const NowPlayingSnapshot &snapshot);
    void applyLyrics(NowPlayingSnapshot *snapshot) const;
    void startAudioMonitor();
    void stopAudioMonitor();

    std::unique_ptr<MusicSessionServicePrivate> m_d;
    QTimer *m_pollTimer = nullptr;
    QFutureWatcher<NowPlayingSnapshot> *m_refreshWatcher = nullptr;
    NowPlayingSnapshot m_snapshot;
};

} // namespace music

#endif // MUSICSESSIONSERVICE_H
