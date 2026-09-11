// SPDX-License-Identifier: GPL-3.0-or-later

#include "spotifybrowser.h"
#include "spotifyapi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QVariantMap>

namespace {

// Spotify caps development mode searches at 10 results per type.
const int SearchLimit = 10;
const int ListLimit = 50;

QVariantMap makeItem(const QString &kind, const QString &id, const QString &uri,
                     const QString &name, const QString &subtitle, const QString &image)
{
    QVariantMap item;
    item.insert("kind", kind);
    item.insert("id", id);
    item.insert("uri", uri);
    item.insert("name", name);
    item.insert("subtitle", subtitle);
    item.insert("image", image);
    return item;
}

QVariantMap makeHeader(const QString &title)
{
    return makeItem(QStringLiteral("header"), QString(), QString(), title, QString(), QString());
}

// Spotify lists images largest first.
QString imageUrl(const QJsonObject &object)
{
    const QJsonArray images = object.value("images").toArray();
    return images.isEmpty() ? QString() : images.at(0).toObject().value("url").toString();
}

QString artistNames(const QJsonObject &object)
{
    QStringList names;
    const QJsonArray artists = object.value("artists").toArray();
    for (const QJsonValue &artist : artists)
        names << artist.toObject().value("name").toString();
    return names.join(QStringLiteral(", "));
}

QVariantMap playlistItem(const QJsonObject &playlist)
{
    // The "tracks" field was renamed to "items" in February 2026.
    const QJsonObject items = playlist.value("items").toObject();
    const int total = items.isEmpty() ? playlist.value("tracks").toObject().value("total").toInt()
                                      : items.value("total").toInt();
    return makeItem(QStringLiteral("playlist"),
                    playlist.value("id").toString(),
                    playlist.value("uri").toString(),
                    playlist.value("name").toString(),
                    QObject::tr("%1 songs").arg(total),
                    imageUrl(playlist));
}

QVariantMap albumItem(const QJsonObject &album)
{
    return makeItem(QStringLiteral("album"),
                    album.value("id").toString(),
                    album.value("uri").toString(),
                    album.value("name").toString(),
                    artistNames(album),
                    imageUrl(album));
}

QVariantMap trackItem(const QJsonObject &track, const QString &image = QString())
{
    const QJsonObject album = track.value("album").toObject();
    return makeItem(QStringLiteral("track"),
                    track.value("id").toString(),
                    track.value("uri").toString(),
                    track.value("name").toString(),
                    artistNames(track),
                    image.isEmpty() ? imageUrl(album) : image);
}

// Playlist entries hold the track in "item"; older responses used "track".
QJsonObject entryTrack(const QJsonObject &entry)
{
    const QJsonObject item = entry.value("item").toObject();
    return item.isEmpty() ? entry.value("track").toObject() : item;
}

QVariantList tracksFrom(const QJsonArray &entries, bool nested, const QString &image = QString())
{
    QVariantList tracks;
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        const QJsonObject track = nested ? entryTrack(entry) : entry;
        if (track.value("uri").toString().isEmpty())
            continue; // Local files and unavailable tracks cannot be played.
        tracks.append(trackItem(track, image));
    }
    return tracks;
}

} // namespace

SpotifyBrowser::SpotifyBrowser(SpotifyApi *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_busy(false)
{
}

void SpotifyBrowser::refreshHome()
{
    setBusy(true);
    m_api->get(QStringLiteral("/me/playlists?limit=%1").arg(ListLimit),
               [this](int playlistStatus, const QByteArray &playlistData) {
        QVariantList playlists;
        if (playlistStatus == 200) {
            const QJsonArray items = QJsonDocument::fromJson(playlistData).object().value("items").toArray();
            for (const QJsonValue &value : items)
                playlists.append(playlistItem(value.toObject()));
        }

        m_api->get(QStringLiteral("/me/albums?limit=%1").arg(ListLimit),
                   [this, playlists](int albumStatus, const QByteArray &albumData) {
            QVariantList home;
            home.append(makeItem(QStringLiteral("liked"), QString(), QString(),
                                 tr("Liked songs"), tr("Songs you saved on Spotify"), QString()));

            if (!playlists.isEmpty()) {
                home.append(makeHeader(tr("Playlists")));
                home += playlists;
            }

            if (albumStatus == 200) {
                const QJsonArray items = QJsonDocument::fromJson(albumData).object().value("items").toArray();
                QVariantList albums;
                for (const QJsonValue &value : items)
                    albums.append(albumItem(value.toObject().value("album").toObject()));
                if (!albums.isEmpty()) {
                    home.append(makeHeader(tr("Albums")));
                    home += albums;
                }
            }

            m_home = home;
            emit homeChanged();
            setBusy(false);
        });
    });
}

void SpotifyBrowser::search(const QString &query)
{
    if (query.trimmed().isEmpty())
        return;

    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("q"), query.trimmed());
    parameters.addQueryItem(QStringLiteral("type"), QStringLiteral("track,album,playlist"));
    parameters.addQueryItem(QStringLiteral("limit"), QString::number(SearchLimit));

    setBusy(true);
    m_api->get(QStringLiteral("/search?") + parameters.toString(QUrl::FullyEncoded),
               [this](int status, const QByteArray &data) {
        setBusy(false);
        if (status != 200)
            return;

        const QJsonObject results = QJsonDocument::fromJson(data).object();
        QVariantList found;

        const QJsonArray tracks = results.value("tracks").toObject().value("items").toArray();
        if (!tracks.isEmpty()) {
            found.append(makeHeader(tr("Songs")));
            found += tracksFrom(tracks, false);
        }

        const QJsonArray albums = results.value("albums").toObject().value("items").toArray();
        if (!albums.isEmpty()) {
            found.append(makeHeader(tr("Albums")));
            for (const QJsonValue &value : albums)
                found.append(albumItem(value.toObject()));
        }

        const QJsonArray playlists = results.value("playlists").toObject().value("items").toArray();
        if (!playlists.isEmpty()) {
            found.append(makeHeader(tr("Playlists")));
            for (const QJsonValue &value : playlists) {
                const QJsonObject playlist = value.toObject();
                if (playlist.value("uri").toString().isEmpty())
                    continue;
                found.append(playlistItem(playlist));
            }
        }

        m_searchResults = found;
        emit searchResultsChanged();
    });
}

void SpotifyBrowser::loadPlaylist(const QString &id, const QString &name)
{
    setTracks(QVariantList(), name, QStringLiteral("spotify:playlist:") + id);
    setBusy(true);
    m_api->get(QStringLiteral("/playlists/%1/items?limit=%2&additional_types=track,episode").arg(id).arg(ListLimit),
               [this, name, id](int status, const QByteArray &data) {
        setBusy(false);
        if (status != 200)
            return;
        const QJsonArray entries = QJsonDocument::fromJson(data).object().value("items").toArray();
        setTracks(tracksFrom(entries, true), name, QStringLiteral("spotify:playlist:") + id);
    });
}

void SpotifyBrowser::loadAlbum(const QString &id, const QString &name)
{
    setTracks(QVariantList(), name, QStringLiteral("spotify:album:") + id);
    setBusy(true);
    m_api->get(QStringLiteral("/albums/%1/tracks?limit=%2").arg(id).arg(ListLimit),
               [this, name, id](int status, const QByteArray &data) {
        setBusy(false);
        if (status != 200)
            return;
        // Album tracks carry no album object, so they have no cover of their own.
        const QJsonArray entries = QJsonDocument::fromJson(data).object().value("items").toArray();
        setTracks(tracksFrom(entries, false), name, QStringLiteral("spotify:album:") + id);
    });
}

void SpotifyBrowser::loadLikedSongs()
{
    setTracks(QVariantList(), tr("Liked songs"), QString());
    setBusy(true);
    m_api->get(QStringLiteral("/me/tracks?limit=%1").arg(ListLimit), [this](int status, const QByteArray &data) {
        setBusy(false);
        if (status != 200)
            return;
        const QJsonArray entries = QJsonDocument::fromJson(data).object().value("items").toArray();
        // Liked songs have no playlist context, so they are played as loose tracks.
        setTracks(tracksFrom(entries, true), tr("Liked songs"), QString());
    });
}

QStringList SpotifyBrowser::trackUris() const
{
    QStringList uris;
    for (const QVariant &value : m_tracks) {
        const QVariantMap track = value.toMap();
        if (track.value("kind").toString() == QLatin1String("track"))
            uris << track.value("uri").toString();
    }
    return uris;
}

void SpotifyBrowser::setTracks(const QVariantList &tracks, const QString &title, const QString &context)
{
    m_tracks = tracks;
    m_tracksTitle = title;
    m_tracksContext = context;
    emit tracksChanged();
}

void SpotifyBrowser::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}
