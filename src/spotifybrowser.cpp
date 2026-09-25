// SPDX-License-Identifier: GPL-3.0-or-later

#include "spotifybrowser.h"
#include "spotifyapi.h"
#include "settings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
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

// Spotify keeps its own playlists (Discover Weekly, Release Radar, the daily
// mixes and the editorial charts) out of reach of apps in development mode:
// /me/playlists still lists them, but reading their songs answers 404. They can
// still be played as a whole, so say so instead of showing an empty list.
QString listError(int status, bool playable)
{
    if (status == 0)
        return QObject::tr("Could not reach Spotify");
    if (status == 403 || status == 404) {
        return playable
                ? QObject::tr("Spotify does not let apps read this list. Playlists made by "
                              "Spotify, such as Discover Weekly or the daily mixes, are private "
                              "to Spotify's own apps. Pull down to play it anyway.")
                : QObject::tr("Spotify does not let apps read this list.");
    }
    return QObject::tr("Spotify could not be read (error %1)").arg(status);
}

// Accepts https://open.spotify.com/playlist/<id>?si=..., spotify:playlist:<id>
// and a bare id, which is what people end up pasting from the Spotify app.
QString playlistIdFromLink(const QString &link)
{
    const QString text = link.trimmed();
    if (text.isEmpty())
        return QString();

    QRegularExpression inLink(QStringLiteral("playlist[:/]([A-Za-z0-9]+)"));
    const QRegularExpressionMatch match = inLink.match(text);
    if (match.hasMatch())
        return match.captured(1);

    QRegularExpression bareId(QStringLiteral("\\A[A-Za-z0-9]{16,}\\z"));
    return bareId.match(text).hasMatch() ? text : QString();
}

} // namespace

SpotifyBrowser::SpotifyBrowser(SpotifyApi *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_busy(false)
{
    loadAddedPlaylists();
}

void SpotifyBrowser::refreshHome()
{
    setBusy(true);
    if (m_userId.isEmpty()) {
        // Needed to tell your own playlists from the ones you follow.
        m_api->get(QStringLiteral("/me"), [this](int status, const QByteArray &data) {
            if (status == 200)
                m_userId = QJsonDocument::fromJson(data).object().value("id").toString();
            loadHome();
        });
        return;
    }
    loadHome();
}

void SpotifyBrowser::loadHome()
{
    m_api->get(QStringLiteral("/me/playlists?limit=%1").arg(ListLimit),
               [this](int playlistStatus, const QByteArray &playlistData) {
        QVariantList playlists;
        QVariantList followed;
        if (playlistStatus == 200) {
            const QJsonArray items = QJsonDocument::fromJson(playlistData).object().value("items").toArray();
            for (const QJsonValue &value : items) {
                const QJsonObject playlist = value.toObject();
                const QString owner = playlist.value("owner").toObject().value("id").toString();
                // Playlists someone else made, Spotify's own above all, often
                // cannot be read by apps, so they are played rather than opened.
                const bool isFollowed = !owner.isEmpty() && !m_userId.isEmpty() && owner != m_userId;
                QVariantMap item = playlistItem(playlist);
                item.insert("followed", isFollowed);
                (isFollowed ? followed : playlists).append(item);
            }
        }

        m_api->get(QStringLiteral("/me/albums?limit=%1").arg(ListLimit),
                   [this, playlists, followed](int albumStatus, const QByteArray &albumData) {
            QVariantList home;
            home.append(makeItem(QStringLiteral("liked"), QString(), QString(),
                                 tr("Liked songs"), tr("Songs you saved on Spotify"), QString()));

            if (!playlists.isEmpty()) {
                home.append(makeHeader(tr("Playlists")));
                home += playlists;
            }

            // Playlists Spotify makes for one person -- the daily mixes,
            // Discover Weekly -- are not in the API at all, so they are the
            // ones added by link. They are nobody's followed lists.
            const QVariantList added = addedItems();
            if (!added.isEmpty()) {
                home.append(makeHeader(tr("Made for you")));
                home += added;
            }

            if (!followed.isEmpty()) {
                home.append(makeHeader(tr("Followed lists")));
                home += followed;
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
        const QString context = QStringLiteral("spotify:playlist:") + id;
        if (status != 200) {
            setTracks(QVariantList(), name, context, listError(status, true));
            return;
        }
        const QJsonArray entries = QJsonDocument::fromJson(data).object().value("items").toArray();
        setTracks(tracksFrom(entries, true), name, context);
    });
}

void SpotifyBrowser::loadAlbum(const QString &id, const QString &name)
{
    setTracks(QVariantList(), name, QStringLiteral("spotify:album:") + id);
    setBusy(true);
    m_api->get(QStringLiteral("/albums/%1/tracks?limit=%2").arg(id).arg(ListLimit),
               [this, name, id](int status, const QByteArray &data) {
        setBusy(false);
        const QString context = QStringLiteral("spotify:album:") + id;
        if (status != 200) {
            setTracks(QVariantList(), name, context, listError(status, true));
            return;
        }
        // Album tracks carry no album object, so they have no cover of their own.
        const QJsonArray entries = QJsonDocument::fromJson(data).object().value("items").toArray();
        setTracks(tracksFrom(entries, false), name, context);
    });
}

void SpotifyBrowser::loadLikedSongs()
{
    setTracks(QVariantList(), tr("Liked songs"), QString());
    setBusy(true);
    m_api->get(QStringLiteral("/me/tracks?limit=%1").arg(ListLimit), [this](int status, const QByteArray &data) {
        setBusy(false);
        if (status != 200) {
            setTracks(QVariantList(), tr("Liked songs"), QString(), listError(status, false));
            return;
        }
        const QJsonArray entries = QJsonDocument::fromJson(data).object().value("items").toArray();
        // Liked songs have no playlist context, so they are played as loose tracks.
        setTracks(tracksFrom(entries, true), tr("Liked songs"), QString());
    });
}

QString SpotifyBrowser::addPlaylistLink(const QString &link, const QString &name)
{
    const QString id = playlistIdFromLink(link);
    if (id.isEmpty())
        return tr("That does not look like a Spotify playlist link");

    for (const AddedPlaylist &playlist : m_added) {
        if (playlist.id == id)
            return tr("That playlist is already in your library");
    }

    AddedPlaylist playlist;
    playlist.id = id;
    playlist.name = name.trimmed().isEmpty() ? tr("Added playlist") : name.trimmed();
    m_added.append(playlist);
    saveAddedPlaylists();

    // Spotify usually refuses to name its own playlists, so the typed name
    // stands unless the real one can be read.
    fetchAddedPlaylistName(id);
    emit addedPlaylistsChanged();
    refreshHome();
    return QString();
}

bool SpotifyBrowser::isPlaylistLink(const QString &link) const
{
    return !playlistIdFromLink(link).isEmpty();
}

void SpotifyBrowser::removeAddedPlaylist(const QString &id)
{
    for (int i = 0; i < m_added.count(); ++i) {
        if (m_added.at(i).id == id) {
            m_added.removeAt(i);
            saveAddedPlaylists();
            emit addedPlaylistsChanged();
            refreshHome();
            return;
        }
    }
}

void SpotifyBrowser::fetchAddedPlaylistName(const QString &id)
{
    m_api->get(QStringLiteral("/playlists/%1?fields=name").arg(id),
               [this, id](int status, const QByteArray &data) {
        if (status != 200)
            return;
        const QString name = QJsonDocument::fromJson(data).object().value("name").toString();
        if (name.isEmpty())
            return;
        for (int i = 0; i < m_added.count(); ++i) {
            if (m_added.at(i).id == id && m_added.at(i).name != name) {
                m_added[i].name = name;
                saveAddedPlaylists();
                emit addedPlaylistsChanged();
                refreshHome();
                return;
            }
        }
    });
}

QVariantList SpotifyBrowser::addedItems() const
{
    QVariantList items;
    for (const AddedPlaylist &playlist : m_added) {
        QVariantMap item = makeItem(QStringLiteral("playlist"), playlist.id,
                                    QStringLiteral("spotify:playlist:") + playlist.id,
                                    playlist.name, tr("Added by link"), QString());
        item.insert("followed", true);
        item.insert("added", true);
        items.append(item);
    }
    return items;
}

void SpotifyBrowser::loadAddedPlaylists()
{
    QSettings settings(settingsFile(), QSettings::IniFormat);
    const int count = settings.beginReadArray(QStringLiteral("addedPlaylists"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        AddedPlaylist playlist;
        playlist.id = settings.value(QStringLiteral("id")).toString();
        playlist.name = settings.value(QStringLiteral("name")).toString();
        if (!playlist.id.isEmpty())
            m_added.append(playlist);
    }
    settings.endArray();
}

void SpotifyBrowser::saveAddedPlaylists()
{
    QSettings settings(settingsFile(), QSettings::IniFormat);
    settings.remove(QStringLiteral("addedPlaylists"));
    settings.beginWriteArray(QStringLiteral("addedPlaylists"), m_added.count());
    for (int i = 0; i < m_added.count(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), m_added.at(i).id);
        settings.setValue(QStringLiteral("name"), m_added.at(i).name);
    }
    settings.endArray();
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

void SpotifyBrowser::setTracks(const QVariantList &tracks, const QString &title,
                               const QString &context, const QString &error)
{
    m_tracks = tracks;
    m_tracksTitle = title;
    m_tracksContext = context;
    m_tracksError = error;
    emit tracksChanged();
}

void SpotifyBrowser::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}
