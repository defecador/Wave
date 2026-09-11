// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPOTIFYPLAYER_H
#define SPOTIFYPLAYER_H

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <functional>

class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;
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
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY playbackChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY playbackChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    explicit SpotifyPlayer(QNetworkAccessManager *nam, SpotifyAuth *auth, QObject *parent = nullptr);

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
    bool shuffle() const { return m_shuffle; }
    QString deviceName() const { return m_deviceName; }
    QVariantList devices() const { return m_devices; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(int positionMs);
    Q_INVOKABLE void setShuffle(bool enabled);
    Q_INVOKABLE void transferTo(const QString &deviceId);
    // Waits for a device to appear in the device list, then moves playback to it.
    Q_INVOKABLE void transferToDeviceNamed(const QString &name, int attempts = 5);

signals:
    void pollingChanged();
    void pollIntervalChanged();
    void playbackChanged();
    void devicesChanged();
    void errorOccurred(const QString &message);

private:
    typedef std::function<void(int status, const QByteArray &data)> Callback;

    void send(const QByteArray &verb, const QString &path, const QByteArray &body, Callback done);
    void command(const QByteArray &verb, const QString &path, const QByteArray &body = QByteArray());
    void handleReply(QNetworkReply *reply, const Callback &done);
    void applyState(const QJsonObject &state);
    void applyDevices(const QByteArray &data);

    QNetworkAccessManager *m_nam;
    SpotifyAuth *m_auth;
    QTimer m_pollTimer;
    QDateTime m_rateLimitedUntil;

    bool m_active;
    bool m_playing;
    QString m_trackName;
    QString m_artists;
    QString m_albumName;
    QString m_coverUrl;
    int m_durationMs;
    int m_progressMs;
    bool m_shuffle;
    QString m_deviceName;
    QVariantList m_devices;
};

#endif // SPOTIFYPLAYER_H
