// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPOTIFYPLAYER_H
#define SPOTIFYPLAYER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class QJsonObject;
class SpotifyApi;
class SpotifyAuth;

// Controls Spotify Connect playback through the Web API. Audio is played by
// whichever device is active (a computer, a speaker, or librespot on the
// phone), not by this process.
class SpotifyPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool polling READ polling WRITE setPolling NOTIFY pollingChanged)
    Q_PROPERTY(int pollInterval READ pollInterval WRITE setPollInterval NOTIFY pollIntervalChanged)
    Q_PROPERTY(bool active READ active NOTIFY playbackChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(QString trackName READ trackName NOTIFY playbackChanged)
    Q_PROPERTY(QString artists READ artists NOTIFY playbackChanged)
    Q_PROPERTY(QString albumName READ albumName NOTIFY playbackChanged)
    Q_PROPERTY(QString coverUrl READ coverUrl NOTIFY playbackChanged)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY playbackChanged)
    Q_PROPERTY(int progressMs READ progressMs NOTIFY playbackChanged)
    // Spotify refuses to seek in adverts, and in a few podcast episodes.
    Q_PROPERTY(bool seekable READ seekable NOTIFY playbackChanged)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY playbackChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY playbackChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    // Whether the current song is in Liked songs. Only meaningful for songs,
    // not podcast episodes, and only known once Spotify has been asked.
    Q_PROPERTY(bool saved READ saved NOTIFY savedChanged)
    Q_PROPERTY(bool savable READ savable NOTIFY savedChanged)

public:
    explicit SpotifyPlayer(SpotifyApi *api, SpotifyAuth *auth, QObject *parent = nullptr);

    bool polling() const { return m_pollTimer.isActive(); }
    void setPolling(bool polling);
    int pollInterval() const { return m_pollTimer.interval(); }
    void setPollInterval(int ms);

    bool active() const { return m_active; }
    bool playing() const { return m_playing; }
    QString trackName() const { return m_trackName; }
    QString artists() const { return m_artists; }
    QString albumName() const { return m_albumName; }
    QString coverUrl() const { return m_coverUrl; }
    int durationMs() const { return m_durationMs; }
    int progressMs() const { return m_progressMs; }
    bool seekable() const { return m_seekable; }
    bool shuffle() const { return m_shuffle; }
    QString deviceName() const { return m_deviceName; }
    QVariantList devices() const { return m_devices; }
    bool saved() const { return m_saved; }
    bool savable() const { return !m_trackId.isEmpty(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(int positionMs);
    // Seeks forwards, or backwards for a negative amount.
    Q_INVOKABLE void seekBy(int deltaMs);
    // Where the song would be now, counted from the clock since the last time
    // Spotify said. Polling is seconds apart, so anything drawing a progress
    // bar of its own -- a car stereo, over MPRIS -- has to be told this instead
    // of progressMs, which only moves when Spotify is asked.
    Q_INVOKABLE int livePositionMs() const;
    Q_INVOKABLE void setShuffle(bool enabled);
    // Adds the current song to Liked songs, or takes it out again.
    Q_INVOKABLE void toggleSaved();
    Q_INVOKABLE void transferTo(const QString &deviceId);
    // Waits for a device to appear in the device list, then moves playback to it.
    Q_INVOKABLE void transferToDeviceNamed(const QString &name, int attempts = 5);
    // Plays a playlist or album, optionally starting at one of its tracks.
    Q_INVOKABLE void playContext(const QString &contextUri, const QString &trackUri = QString());
    // Plays loose tracks, e.g. liked songs or search results.
    Q_INVOKABLE void playTracks(const QStringList &trackUris, const QString &startUri = QString());

signals:
    void pollingChanged();
    void pollIntervalChanged();
    void playbackChanged();
    void devicesChanged();
    void savedChanged();
    // The song jumped rather than played on, so listeners have to catch up.
    void seeked(int positionMs);
    void errorOccurred(const QString &message);

private:
    void command(const QByteArray &verb, const QString &path, const QByteArray &body = QByteArray());
    void applyState(const QJsonObject &state);
    void applyDevices(const QByteArray &data);
    void checkSaved();
    void setSaved(bool saved);
    // Records where the song is and when that was true.
    void setProgress(int positionMs);

    SpotifyApi *m_api;
    SpotifyAuth *m_auth;
    QTimer m_pollTimer;

    bool m_active;
    bool m_playing;
    QString m_trackName;
    QString m_artists;
    QString m_albumName;
    QString m_coverUrl;
    int m_durationMs;
    int m_progressMs;
    // Restarted whenever m_progressMs is, so that the two together say where
    // the song is at any later moment.
    QElapsedTimer m_sinceProgress;
    bool m_seekable;
    bool m_shuffle;
    QString m_deviceName;
    QString m_trackId;
    bool m_saved;
    QVariantList m_devices;
};

#endif // SPOTIFYPLAYER_H
