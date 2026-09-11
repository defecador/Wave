// SPDX-License-Identifier: GPL-3.0-or-later

#include "spotifyplayer.h"
#include "spotifyauth.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

namespace {

const char *const ApiBase = "https://api.spotify.com/v1";
// Spotify reports the new playback state a moment after a command succeeds.
const int CommandSettleMs = 700;

} // namespace

SpotifyPlayer::SpotifyPlayer(QNetworkAccessManager *nam, SpotifyAuth *auth, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
    , m_auth(auth)
    , m_active(false)
    , m_playing(false)
    , m_durationMs(0)
    , m_progressMs(0)
    , m_shuffle(false)
{
    m_pollTimer.setInterval(5000);
    connect(&m_pollTimer, &QTimer::timeout, this, &SpotifyPlayer::refresh);

    connect(m_auth, &SpotifyAuth::loggedInChanged, this, [this]() {
        if (m_auth->loggedIn()) {
            refresh();
        } else {
            applyState(QJsonObject());
            m_devices.clear();
            emit devicesChanged();
        }
    });
}

void SpotifyPlayer::setPolling(bool polling)
{
    if (polling == m_pollTimer.isActive())
        return;
    if (polling) {
        m_pollTimer.start();
        refresh();
    } else {
        m_pollTimer.stop();
    }
    emit pollingChanged();
}

void SpotifyPlayer::setPollInterval(int ms)
{
    if (ms == m_pollTimer.interval())
        return;
    m_pollTimer.setInterval(ms);
    emit pollIntervalChanged();
}

void SpotifyPlayer::refresh()
{
    if (!m_auth->loggedIn())
        return;
    send("GET", QStringLiteral("/me/player?additional_types=track,episode"), QByteArray(),
         [this](int status, const QByteArray &data) {
        // 204 means there is no active playback session.
        applyState(status == 204 ? QJsonObject() : QJsonDocument::fromJson(data).object());
    });
}

void SpotifyPlayer::refreshDevices()
{
    send("GET", QStringLiteral("/me/player/devices"), QByteArray(), [this](int, const QByteArray &data) {
        applyDevices(data);
    });
}

void SpotifyPlayer::transferToDeviceNamed(const QString &name, int attempts)
{
    send("GET", QStringLiteral("/me/player/devices"), QByteArray(), [this, name, attempts](int, const QByteArray &data) {
        applyDevices(data);
        const QVariantList devices = m_devices;
        for (const QVariant &value : devices) {
            const QVariantMap device = value.toMap();
            if (device.value("name").toString() == name && !device.value("restricted").toBool()) {
                transferTo(device.value("id").toString());
                return;
            }
        }

        // A receiver that has just logged in takes a few seconds to show up.
        if (attempts > 1) {
            QTimer::singleShot(2000, this, [this, name, attempts]() {
                transferToDeviceNamed(name, attempts - 1);
            });
        } else {
            emit errorOccurred(tr("\"%1\" did not show up in your Spotify devices.").arg(name));
        }
    });
}

void SpotifyPlayer::applyDevices(const QByteArray &data)
{
    m_devices.clear();
    const QJsonArray devices = QJsonDocument::fromJson(data).object().value("devices").toArray();
    for (const QJsonValue &value : devices) {
        const QJsonObject device = value.toObject();
        QVariantMap map;
        map.insert("id", device.value("id").toString());
        map.insert("name", device.value("name").toString());
        map.insert("type", device.value("type").toString());
        map.insert("active", device.value("is_active").toBool());
        map.insert("restricted", device.value("is_restricted").toBool());
        m_devices.append(map);
    }
    emit devicesChanged();
}

void SpotifyPlayer::play()
{
    command("PUT", QStringLiteral("/me/player/play"));
    m_playing = true;
    emit playbackChanged();
}

void SpotifyPlayer::pause()
{
    command("PUT", QStringLiteral("/me/player/pause"));
    m_playing = false;
    emit playbackChanged();
}

void SpotifyPlayer::togglePlay()
{
    if (m_playing)
        pause();
    else
        play();
}

void SpotifyPlayer::next()
{
    command("POST", QStringLiteral("/me/player/next"));
}

void SpotifyPlayer::previous()
{
    command("POST", QStringLiteral("/me/player/previous"));
}

void SpotifyPlayer::seek(int positionMs)
{
    command("PUT", QStringLiteral("/me/player/seek?position_ms=%1").arg(positionMs));
    m_progressMs = positionMs;
    emit playbackChanged();
}

void SpotifyPlayer::setShuffle(bool enabled)
{
    command("PUT", QStringLiteral("/me/player/shuffle?state=%1").arg(enabled ? "true" : "false"));
    m_shuffle = enabled;
    emit playbackChanged();
}

void SpotifyPlayer::transferTo(const QString &deviceId)
{
    const QJsonObject body {
        { "device_ids", QJsonArray { deviceId } },
        { "play", true }
    };
    send("PUT", QStringLiteral("/me/player"), QJsonDocument(body).toJson(QJsonDocument::Compact),
         [this](int, const QByteArray &) {
        QTimer::singleShot(CommandSettleMs, this, &SpotifyPlayer::refresh);
        QTimer::singleShot(CommandSettleMs, this, &SpotifyPlayer::refreshDevices);
    });
}

void SpotifyPlayer::command(const QByteArray &verb, const QString &path, const QByteArray &body)
{
    send(verb, path, body, [this](int, const QByteArray &) {
        QTimer::singleShot(CommandSettleMs, this, &SpotifyPlayer::refresh);
    });
}

void SpotifyPlayer::send(const QByteArray &verb, const QString &path, const QByteArray &body, Callback done)
{
    if (m_rateLimitedUntil.isValid() && QDateTime::currentDateTimeUtc() < m_rateLimitedUntil) {
        if (verb != "GET")
            emit errorOccurred(tr("Spotify asked Wave to slow down. Try again in a moment."));
        return;
    }

    m_auth->withAccessToken([this, verb, path, body, done](const QString &token) {
        if (token.isEmpty())
            return; // Logged out; the UI shows the login flow.

        QNetworkRequest request(QUrl(QLatin1String(ApiBase) + path));
        request.setRawHeader("Authorization", "Bearer " + token.toUtf8());

        QNetworkReply *reply = nullptr;
        if (verb == "GET") {
            reply = m_nam->get(request);
        } else {
            if (!body.isEmpty())
                request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            // Spotify answers 411 to body-less PUT/POST requests without an explicit length.
            request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
            reply = verb == "PUT" ? m_nam->put(request, body) : m_nam->post(request, body);
        }

        connect(reply, &QNetworkReply::finished, this, [this, reply, done]() {
            handleReply(reply, done);
        });
    });
}

void SpotifyPlayer::handleReply(QNetworkReply *reply, const Callback &done)
{
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();

    if (status == 401) {
        m_auth->invalidateAccessToken();
        return;
    }
    if (status == 429) {
        int seconds = reply->rawHeader("Retry-After").toInt();
        if (seconds <= 0)
            seconds = 30;
        m_rateLimitedUntil = QDateTime::currentDateTimeUtc().addSecs(seconds);
        emit errorOccurred(tr("Spotify rate limit reached. Pausing for %1 seconds.").arg(seconds));
        return;
    }
    if (status >= 400 || (status == 0 && reply->error() != QNetworkReply::NoError)) {
        // Spotify errors look like {"error": {"status": 404, "message": "...", "reason": "..."}}
        const QString message = QJsonDocument::fromJson(data).object()
                .value("error").toObject().value("message").toString();
        emit errorOccurred(message.isEmpty() ? reply->errorString() : message);
        return;
    }

    if (done)
        done(status, data);
}

void SpotifyPlayer::applyState(const QJsonObject &state)
{
    const QJsonObject item = state.value("item").toObject();
    const QJsonObject album = item.value("album").toObject();

    QStringList artists;
    const QJsonArray artistArray = item.value("artists").toArray();
    for (const QJsonValue &artist : artistArray)
        artists << artist.toObject().value("name").toString();
    // Podcast episodes have a show instead of artists.
    const QString showName = item.value("show").toObject().value("name").toString();
    if (artists.isEmpty() && !showName.isEmpty())
        artists << showName;

    QJsonArray images = album.value("images").toArray();
    if (images.isEmpty())
        images = item.value("images").toArray();

    m_active = !state.isEmpty();
    m_playing = state.value("is_playing").toBool();
    m_trackName = item.value("name").toString();
    m_artists = artists.join(QStringLiteral(", "));
    m_albumName = album.value("name").toString();
    // Spotify lists images largest first.
    m_coverUrl = images.isEmpty() ? QString() : images.at(0).toObject().value("url").toString();
    m_durationMs = item.value("duration_ms").toInt();
    m_progressMs = state.value("progress_ms").toInt();
    m_shuffle = state.value("shuffle_state").toBool();
    m_deviceName = state.value("device").toObject().value("name").toString();
    emit playbackChanged();
}
