#include "music/system/MusicSessionService.h"

#include "music/lyrics/LrcParser.h"
#include "music/system/AudioLevelSmoothing.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThreadPool>
#include <QUrl>
#include <QUrlQuery>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <thread>
#include <vector>

#if defined(DOCK_WMAC_HAS_WINRT_GSMTC)
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>
#endif

#if defined(DOCK_WMAC_HAS_WASAPI)
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#endif

namespace music {

namespace {

QString trackKey(const NowPlayingSnapshot &snapshot)
{
    return QStringList{
        snapshot.title.trimmed().toLower(),
        snapshot.artist.trimmed().toLower(),
        snapshot.album.trimmed().toLower(),
        QString::number(snapshot.durationMs / 1000)
    }.join(QLatin1Char('|'));
}

LyricsDocument plainLyricsDocument(const QString &lyrics, qint64 durationMs)
{
    LyricsDocument doc;
    const QString text = lyrics.trimmed();
    if (text.isEmpty())
        return doc;

    doc.lines.append({0, static_cast<int>(qMax<qint64>(durationMs, 300000)), text});
    doc.synced = false;
    return doc;
}

} // namespace

#if defined(DOCK_WMAC_HAS_WINRT_GSMTC)
namespace Control = winrt::Windows::Media::Control;
namespace Streams = winrt::Windows::Storage::Streams;

enum class PlaybackCommand {
    TogglePlayPause,
    Previous,
    Next
};

QString toQString(const winrt::hstring &value)
{
    return QString::fromWCharArray(value.c_str()).trimmed();
}

bool isAppleMusicSource(const QString &source)
{
    const QString normalized = source.toLower();
    return normalized.contains(QStringLiteral("applemusic"))
        || normalized.contains(QStringLiteral("apple.music"))
        || normalized.contains(QStringLiteral("apple music"));
}

int playbackScore(Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus status)
{
    switch (status) {
    case Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:
        return 60;
    case Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:
        return 25;
    case Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped:
        return 5;
    default:
        return 0;
    }
}

Control::GlobalSystemMediaTransportControlsSession selectSession(
    const Control::GlobalSystemMediaTransportControlsSessionManager &manager)
{
    Control::GlobalSystemMediaTransportControlsSession current = manager.GetCurrentSession();
    const QString currentSource = current ? toQString(current.SourceAppUserModelId()) : QString();

    Control::GlobalSystemMediaTransportControlsSession best{nullptr};
    int bestScore = -1;
    QString bestSource;

    const auto sessions = manager.GetSessions();
    for (uint32_t i = 0; i < sessions.Size(); ++i) {
        const auto session = sessions.GetAt(i);
        if (!session)
            continue;

        const QString source = toQString(session.SourceAppUserModelId());
        int score = 0;
        try {
            score += playbackScore(session.GetPlaybackInfo().PlaybackStatus());
        } catch (const winrt::hresult_error &) {
            // Keep the session selectable by source name even if playback info is temporarily unavailable.
        }

        if (!currentSource.isEmpty() && source == currentSource)
            score += 100;
        if (isAppleMusicSource(source) && (source == currentSource || score > 0))
            score += 40;

        if (!best || score > bestScore || (score == bestScore && source < bestSource)) {
            best = session;
            bestScore = score;
            bestSource = source;
        }
    }

    return best ? best : current;
}

QByteArray readArtworkData(
    const Control::GlobalSystemMediaTransportControlsSessionMediaProperties &media)
{
    const auto thumbnail = media.Thumbnail();
    if (!thumbnail)
        return {};

    try {
        const auto stream = thumbnail.OpenReadAsync().get();
        const auto size = stream.Size();
        if (size == 0 || size > 10 * 1024 * 1024)
            return {};

        Streams::Buffer buffer(static_cast<uint32_t>(size));
        const auto readBuffer = stream.ReadAsync(
            buffer,
            buffer.Capacity(),
            Streams::InputStreamOptions::None).get();

        std::vector<uint8_t> bytes(readBuffer.Length());
        if (bytes.empty())
            return {};

        const auto reader = Streams::DataReader::FromBuffer(readBuffer);
        reader.ReadBytes(bytes);
        return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                          static_cast<int>(bytes.size()));
    } catch (const winrt::hresult_error &error) {
        qWarning() << "GSMTC artwork read failed:" << toQString(error.message());
        return {};
    }
}

NowPlayingSnapshot unavailableSnapshot(const QString &source)
{
    NowPlayingSnapshot snapshot;
    snapshot.source = source;
    snapshot.status = PlaybackStatus::Unavailable;
    return snapshot;
}

NowPlayingSnapshot fetchNowPlayingSnapshot()
{
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        const auto manager = Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        const auto session = selectSession(manager);
        if (!session)
            return unavailableSnapshot(QStringLiteral("No active media session"));

        const auto media = session.TryGetMediaPropertiesAsync().get();
        const auto timeline = session.GetTimelineProperties();
        const auto playback = session.GetPlaybackInfo();
        const auto controls = playback.Controls();

        NowPlayingSnapshot snapshot;
        snapshot.available = true;
        snapshot.source = toQString(session.SourceAppUserModelId());
        if (snapshot.source.isEmpty())
            snapshot.source = QStringLiteral("Windows media session");
        snapshot.title = toQString(media.Title());
        snapshot.artist = toQString(media.Artist());
        snapshot.album = toQString(media.AlbumTitle());
        snapshot.positionMs = timeline.Position().count() / 10000;
        snapshot.durationMs = (timeline.EndTime().count() - timeline.StartTime().count()) / 10000;
        snapshot.canPlayPause = controls.IsPlayPauseToggleEnabled()
            || controls.IsPlayEnabled()
            || controls.IsPauseEnabled();
        snapshot.canPrevious = controls.IsPreviousEnabled();
        snapshot.canNext = controls.IsNextEnabled();
        snapshot.artworkData = readArtworkData(media);
        snapshot.hasArtwork = !snapshot.artworkData.isEmpty();

        switch (playback.PlaybackStatus()) {
        case Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:
            snapshot.status = PlaybackStatus::Playing;
            break;
        case Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:
            snapshot.status = PlaybackStatus::Paused;
            break;
        case Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped:
        case Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed:
            snapshot.status = PlaybackStatus::Stopped;
            break;
        default:
            snapshot.status = PlaybackStatus::Unavailable;
            break;
        }

        return snapshot;
    } catch (const winrt::hresult_error &error) {
        qWarning() << "GSMTC refresh failed:" << toQString(error.message());
        return unavailableSnapshot(QStringLiteral("GSMTC refresh failed"));
    }
}

void runPlaybackCommand(PlaybackCommand command)
{
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        const auto manager = Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        const auto session = selectSession(manager);
        if (!session)
            return;

        const auto playback = session.GetPlaybackInfo();
        const auto controls = playback.Controls();

        switch (command) {
        case PlaybackCommand::TogglePlayPause:
            if (playback.PlaybackStatus() == Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing
                && controls.IsPauseEnabled()) {
                session.TryPauseAsync().get();
            } else if (playback.PlaybackStatus() != Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing
                && controls.IsPlayEnabled()) {
                session.TryPlayAsync().get();
            } else if (controls.IsPlayPauseToggleEnabled()) {
                session.TryTogglePlayPauseAsync().get();
            }
            break;
        case PlaybackCommand::Previous:
            if (controls.IsPreviousEnabled())
                session.TrySkipPreviousAsync().get();
            break;
        case PlaybackCommand::Next:
            if (controls.IsNextEnabled())
                session.TrySkipNextAsync().get();
            break;
        }
    } catch (const winrt::hresult_error &error) {
        qWarning() << "GSMTC command failed:" << toQString(error.message());
    }
}
#endif

#if defined(DOCK_WMAC_HAS_WASAPI)
namespace {

using Microsoft::WRL::ComPtr;

qreal pcmSample(const BYTE *data, int bytesPerSample)
{
    if (bytesPerSample == 2)
        return *reinterpret_cast<const qint16 *>(data) / 32768.0;

    if (bytesPerSample == 3) {
        qint32 value = data[0] | (data[1] << 8) | (data[2] << 16);
        if (value & 0x800000)
            value |= 0xff000000;
        return value / 8388608.0;
    }

    if (bytesPerSample == 4)
        return *reinterpret_cast<const qint32 *>(data) / 2147483648.0;

    return 0.0;
}

qreal rmsLevel(const BYTE *data, UINT32 frames, const WAVEFORMATEX *format)
{
    if (!data || !format || frames == 0 || format->nChannels == 0)
        return 0.0;

    const int channels = format->nChannels;
    const int bits = format->wBitsPerSample;
    const int bytesPerSample = qMax(1, bits / 8);
    const bool floatSamples = bits == 32 && format->wFormatTag != WAVE_FORMAT_PCM;
    const int sampleCount = static_cast<int>(frames) * channels;

    double sum = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        const BYTE *sampleData = data + i * bytesPerSample;
        const qreal sample = floatSamples
            ? qBound(-1.0, static_cast<qreal>(*reinterpret_cast<const float *>(sampleData)), 1.0)
            : qBound(-1.0, pcmSample(sampleData, bytesPerSample), 1.0);
        sum += sample * sample;
    }

    if (sampleCount == 0)
        return 0.0;
    return qBound(0.0, std::sqrt(sum / sampleCount) * 2.0, 1.0);
}

} // namespace
#endif

class MusicSessionServicePrivate {
public:
#if defined(DOCK_WMAC_HAS_WINRT_GSMTC)
    Control::GlobalSystemMediaTransportControlsSessionManager manager{nullptr};
    Control::GlobalSystemMediaTransportControlsSession session{nullptr};
#endif
    QNetworkAccessManager *lyricsNetwork = nullptr;
    LyricsDocument lyrics;
    QString lyricsTrackKey;
    QString pendingLyricsTrackKey;
    bool externalLyricsEnabled = false;
    qreal audioLevel = 0.0;
    std::atomic_bool audioRunning{false};
    std::thread audioThread;
};

MusicSessionService::MusicSessionService(QObject *parent)
    : QObject(parent)
    , m_d(std::make_unique<MusicSessionServicePrivate>())
    , m_pollTimer(new QTimer(this))
    , m_refreshWatcher(new QFutureWatcher<NowPlayingSnapshot>(this))
{
    qRegisterMetaType<music::NowPlayingSnapshot>("music::NowPlayingSnapshot");
    m_d->lyricsNetwork = new QNetworkAccessManager(this);
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, &MusicSessionService::refreshNowPlaying);
    connect(m_refreshWatcher, &QFutureWatcher<NowPlayingSnapshot>::finished, this, [this]() {
        publishSnapshot(m_refreshWatcher->result());
    });
}

MusicSessionService::~MusicSessionService()
{
    stopAudioMonitor();
}

void MusicSessionService::start()
{
#if defined(DOCK_WMAC_HAS_WINRT_GSMTC)
    publishUnavailable(QStringLiteral("Loading Windows media session"));
    refreshNowPlaying();
    m_pollTimer->start();
#else
    publishUnavailable(QStringLiteral("GSMTC requires the MSVC WinRT build"));
#endif
    startAudioMonitor();
}

void MusicSessionService::setExternalLyricsEnabled(bool enabled)
{
    m_d->externalLyricsEnabled = enabled;
}

void MusicSessionService::refreshNowPlaying()
{
#if defined(DOCK_WMAC_HAS_WINRT_GSMTC)
    if (m_refreshWatcher->isRunning())
        return;
    m_refreshWatcher->setFuture(QtConcurrent::run(fetchNowPlayingSnapshot));
#else
    publishUnavailable(QStringLiteral("GSMTC requires the MSVC WinRT build"));
#endif
}

void MusicSessionService::togglePlayPause()
{
#if defined(DOCK_WMAC_HAS_WINRT_GSMTC)
    if (!m_snapshot.canPlayPause)
        return;
    QThreadPool::globalInstance()->start([]() {
        runPlaybackCommand(PlaybackCommand::TogglePlayPause);
    });
    QTimer::singleShot(250, this, &MusicSessionService::refreshNowPlaying);
#endif
}

void MusicSessionService::previous()
{
#if defined(DOCK_WMAC_HAS_WINRT_GSMTC)
    if (!m_snapshot.canPrevious)
        return;
    QThreadPool::globalInstance()->start([]() {
        runPlaybackCommand(PlaybackCommand::Previous);
    });
    QTimer::singleShot(250, this, &MusicSessionService::refreshNowPlaying);
#endif
}

void MusicSessionService::next()
{
#if defined(DOCK_WMAC_HAS_WINRT_GSMTC)
    if (!m_snapshot.canNext)
        return;
    QThreadPool::globalInstance()->start([]() {
        runPlaybackCommand(PlaybackCommand::Next);
    });
    QTimer::singleShot(250, this, &MusicSessionService::refreshNowPlaying);
#endif
}

void MusicSessionService::publishUnavailable(const QString &source)
{
    NowPlayingSnapshot snapshot;
    snapshot.source = source;
    snapshot.status = PlaybackStatus::Unavailable;
    publishSnapshot(snapshot);
}

void MusicSessionService::publishSnapshot(const NowPlayingSnapshot &snapshot)
{
    m_snapshot = snapshot;
    m_snapshot.audioLevel = m_d->audioLevel;
    applyLyrics(&m_snapshot);
    emit snapshotChanged(m_snapshot);
    refreshLyricsIfNeeded(m_snapshot);
}

void MusicSessionService::publishAudioLevel(qreal level)
{
    m_d->audioLevel = qBound(0.0, level, 1.0);
    if (!m_snapshot.available)
        return;

    m_snapshot.audioLevel = m_d->audioLevel;
    applyLyrics(&m_snapshot);
    emit snapshotChanged(m_snapshot);
}

void MusicSessionService::refreshLyricsIfNeeded(const NowPlayingSnapshot &snapshot)
{
    if (!m_d->externalLyricsEnabled || !m_d->lyricsNetwork || !snapshot.available)
        return;
    if (snapshot.title.trimmed().isEmpty() || snapshot.artist.trimmed().isEmpty())
        return;

    const QString key = trackKey(snapshot);
    if (key == m_d->lyricsTrackKey || key == m_d->pendingLyricsTrackKey)
        return;

    m_d->pendingLyricsTrackKey = key;

    QUrl url(QStringLiteral("https://lrclib.net/api/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("track_name"), snapshot.title);
    query.addQueryItem(QStringLiteral("artist_name"), snapshot.artist);
    if (!snapshot.album.isEmpty())
        query.addQueryItem(QStringLiteral("album_name"), snapshot.album);
    if (snapshot.durationMs > 0)
        query.addQueryItem(QStringLiteral("duration"), QString::number(snapshot.durationMs / 1000));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Dock_WMac/0.2.5"));
    QNetworkReply *reply = m_d->lyricsNetwork->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, durationMs = snapshot.durationMs]() {
        reply->deleteLater();
        if (key != m_d->pendingLyricsTrackKey)
            return;
        m_d->pendingLyricsTrackKey.clear();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "LRCLIB request failed:" << reply->errorString();
            m_d->lyrics = {};
            m_d->lyricsTrackKey = key;
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray())
            return;

        LyricsDocument best;
        const QJsonArray results = doc.array();
        for (const QJsonValue &value : results) {
            const QJsonObject obj = value.toObject();
            const QString synced = obj.value(QStringLiteral("syncedLyrics")).toString();
            if (!synced.trimmed().isEmpty()) {
                best = LrcParser::parse(synced);
                break;
            }
            if (best.lines.isEmpty()) {
                best = plainLyricsDocument(obj.value(QStringLiteral("plainLyrics")).toString(), durationMs);
            }
        }

        m_d->lyrics = best;
        m_d->lyricsTrackKey = key;
        applyLyrics(&m_snapshot);
        emit snapshotChanged(m_snapshot);
    });
}

void MusicSessionService::applyLyrics(NowPlayingSnapshot *snapshot) const
{
    if (!snapshot)
        return;

    snapshot->hasLyrics = !m_d->lyrics.lines.isEmpty()
        && trackKey(*snapshot) == m_d->lyricsTrackKey;
    snapshot->previousLyric.clear();
    snapshot->currentLyric.clear();
    snapshot->nextLyric.clear();
    if (!snapshot->hasLyrics || trackKey(*snapshot) != m_d->lyricsTrackKey)
        return;

    const int position = static_cast<int>(qBound(qint64(0), snapshot->positionMs, qint64(INT_MAX)));
    int currentIndex = -1;
    for (int i = 0; i < m_d->lyrics.lines.size(); ++i) {
        const auto &line = m_d->lyrics.lines.at(i);
        if (position >= line.startMs && position <= line.endMs) {
            currentIndex = i;
            break;
        }
        if (position < line.startMs) {
            currentIndex = qMax(0, i - 1);
            break;
        }
    }
    if (currentIndex < 0)
        currentIndex = m_d->lyrics.lines.size() - 1;

    snapshot->currentLyric = m_d->lyrics.lines.at(currentIndex).text;
    if (currentIndex > 0)
        snapshot->previousLyric = m_d->lyrics.lines.at(currentIndex - 1).text;
    if (currentIndex + 1 < m_d->lyrics.lines.size())
        snapshot->nextLyric = m_d->lyrics.lines.at(currentIndex + 1).text;
}

void MusicSessionService::startAudioMonitor()
{
#if defined(DOCK_WMAC_HAS_WASAPI)
    if (m_d->audioRunning.exchange(true))
        return;

    m_d->audioThread = std::thread([this]() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool shouldUninitialize = SUCCEEDED(hr);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            m_d->audioRunning = false;
            return;
        }

        WAVEFORMATEX *mixFormat = nullptr;
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> device;
        ComPtr<IAudioClient> audioClient;
        ComPtr<IAudioCaptureClient> captureClient;

        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(&enumerator));
        if (SUCCEEDED(hr))
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (SUCCEEDED(hr))
            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void **>(audioClient.GetAddressOf()));
        if (SUCCEEDED(hr))
            hr = audioClient->GetMixFormat(&mixFormat);
        if (SUCCEEDED(hr)) {
            hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_LOOPBACK,
                                         1000000, 0, mixFormat, nullptr);
        }
        if (SUCCEEDED(hr))
            hr = audioClient->GetService(IID_PPV_ARGS(&captureClient));
        if (SUCCEEDED(hr))
            hr = audioClient->Start();

        if (FAILED(hr)) {
            qWarning() << "WASAPI loopback failed:" << QString::number(static_cast<unsigned long>(hr), 16);
            if (mixFormat)
                CoTaskMemFree(mixFormat);
            if (shouldUninitialize)
                CoUninitialize();
            m_d->audioRunning = false;
            return;
        }

        qreal smoothed = 0.0;
        while (m_d->audioRunning.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(35));

            UINT32 packetLength = 0;
            qreal packetLevel = 0.0;
            if (FAILED(captureClient->GetNextPacketSize(&packetLength)))
                continue;

            while (packetLength > 0) {
                BYTE *data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                if (FAILED(captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
                    break;

                const qreal level = (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    ? 0.0
                    : rmsLevel(data, frames, mixFormat);
                packetLevel = qMax(packetLevel, level);
                captureClient->ReleaseBuffer(frames);

                if (FAILED(captureClient->GetNextPacketSize(&packetLength)))
                    break;
            }

            smoothed = smoothAudioLevel(smoothed, packetLevel);
            QMetaObject::invokeMethod(this, [this, smoothed]() {
                publishAudioLevel(smoothed);
            }, Qt::QueuedConnection);
        }

        audioClient->Stop();
        if (mixFormat)
            CoTaskMemFree(mixFormat);
        if (shouldUninitialize)
            CoUninitialize();
    });
#endif
}

void MusicSessionService::stopAudioMonitor()
{
#if defined(DOCK_WMAC_HAS_WASAPI)
    m_d->audioRunning = false;
    if (m_d->audioThread.joinable())
        m_d->audioThread.join();
#endif
}

} // namespace music
