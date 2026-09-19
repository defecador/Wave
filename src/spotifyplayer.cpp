// SPDX-License-Identifier: GPL-3.0-or-later

#include "spotifyplayer.h"
#include "spotifyapi.h"
#include "spotifyauth.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

namespace {

// Spotify reports the new playback state a moment after a command succeeds.
const int CommandSettleMs = 700;

} // namespace

SpotifyPlayer::SpotifyPlayer(SpotifyApi *api, SpotifyAuth *auth, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_auth(auth)
    , m_active(false)
    , m_playing(false)
    , m_durationMs(0)
    , m_progressMs(0)
    , m_shuffle(false)
    , m_saved(false)
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
    m_api->get(QStringLiteral("/me/player?additional_types=track,episode"),
               [this](int status, const QByteArray &data) {
        // 204 means there is no active playback session.
        if (status == 204)
            applyState(QJsonObject());
        else if (status == 200)
            applyState(QJsonDocument::fromJson(data).object());
    });
}

void SpotifyPlayer::refreshDevices()
{
    m_api->get(QStringLiteral("/me/player/devices"), [this](int status, const QByteArray &data) {
        if (status == 200)
            applyDevices(data);
    });
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
    m_api->send("PUT", QStringLiteral("/me/player"), QJsonDocument(body).toJson(QJsonDocument::Compact),
                [this](int, const QByteArray &) {
        QTimer::singleShot(CommandSettleMs, this, &SpotifyPlayer::refresh);
        QTimer::singleShot(CommandSettleMs, this, &SpotifyPlayer::refreshDevices);
    });
}

void SpotifyPlayer::transferToDeviceNamed(const QString &name, int attempts)
{
    m_api->get(QStringLiteral("/me/player/devices"), [this, name, attempts](int status, const QByteArray &data) {
        if (status != 200)
            return;
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

void SpotifyPlayer::playContext(const QString &contextUri, const QString &trackUri)
{
    QJsonObject body { { "context_uri", contextUri } };
    if (!trackUri.isEmpty())
        body.insert("offset", QJsonObject { { "uri", trackUri } });
    command("PUT", QStringLiteral("/me/player/play"), QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_playing = true;
    emit playbackChanged();
}

void SpotifyPlayer::playTracks(const QStringList &trackUris, const QString &startUri)
{
    if (trackUris.isEmpty())
        return;

    QJsonArray uris;
    for (const QString &uri : trackUris)
        uris.append(uri);

    QJsonObject body { { "uris", uris } };
    if (!startUri.isEmpty())
        body.insert("offset", QJsonObject { { "uri", startUri } });
    command("PUT", QStringLiteral("/me/player/play"), QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_playing = true;
    emit playbackChanged();
}

void SpotifyPlayer::command(const QByteArray &verb, const QString &path, const QByteArray &body)
{
    m_api->send(verb, path, body, [this](int, const QByteArray &) {
        QTimer::singleShot(CommandSettleMs, this, &SpotifyPlayer::refresh);
    });
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

    // Podcast episodes cannot be liked, so they keep an empty track id.
    const QString trackId = item.value("type").toString() == QLatin1String("episode")
            ? QString() : item.value("id").toString();
    if (trackId != m_trackId) {
        m_trackId = trackId;
        m_saved = false;
        emit savedChanged();
        checkSaved();
    }
}

void SpotifyPlayer::toggleSaved()
{
    if (m_trackId.isEmpty())
        return;

    const bool wanted = !m_saved;
    const QString id = m_trackId;
    // Show the new state at once; Spotify is told in the background.
    setSaved(wanted);
    m_api->send(wanted ? "PUT" : "DELETE",
                QStringLiteral("/me/tracks?ids=%1").arg(id), QByteArray(),
                [this, id, wanted](int status, const QByteArray &) {
        if (status == 200 || status == 204)
            return;
        if (id == m_trackId)
            setSaved(!wanted); // Put the heart back the way it was.
    });
}

void SpotifyPlayer::checkSaved()
{
    if (m_trackId.isEmpty())
        return;

    const QString id = m_trackId;
    m_api->get(QStringLiteral("/me/tracks/contains?ids=%1").arg(id),
               [this, id](int status, const QByteArray &data) {
        if (status != 200 || id != m_trackId)
            return;
        const QJsonArray answer = QJsonDocument::fromJson(data).array();
        setSaved(!answer.isEmpty() && answer.at(0).toBool());
    });
}

void SpotifyPlayer::setSaved(bool saved)
{
    if (m_saved == saved)
        return;
    m_saved = saved;
    emit savedChanged();
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
