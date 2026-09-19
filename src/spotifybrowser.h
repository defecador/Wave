// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPOTIFYBROWSER_H
#define SPOTIFYBROWSER_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class QByteArray;
class SpotifyApi;

// Reads the user's playlists, saved albums and liked songs, and searches
// Spotify. Every list holds QVariantMaps with a "kind" of "header", "playlist",
// "album", "track" or "liked", so QML can render them with one delegate.
class SpotifyBrowser : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList home READ home NOTIFY homeChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(QString tracksTitle READ tracksTitle NOTIFY tracksChanged)
    // Playlist or album URI the loaded tracks belong to; empty for loose tracks.
    Q_PROPERTY(QString tracksContext READ tracksContext NOTIFY tracksChanged)
    // Why the track list is empty, if Spotify refused it. Empty otherwise.
    Q_PROPERTY(QString tracksError READ tracksError NOTIFY tracksChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit SpotifyBrowser(SpotifyApi *api, QObject *parent = nullptr);

    QVariantList home() const { return m_home; }
    QVariantList searchResults() const { return m_searchResults; }
    QVariantList tracks() const { return m_tracks; }
    QString tracksTitle() const { return m_tracksTitle; }
    QString tracksContext() const { return m_tracksContext; }
    QString tracksError() const { return m_tracksError; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void refreshHome();
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void loadPlaylist(const QString &id, const QString &name);
    Q_INVOKABLE void loadAlbum(const QString &id, const QString &name);
    Q_INVOKABLE void loadLikedSongs();
    // Spotify's own "Made for you" playlists, Discover Weekly above all, are
    // not in /me/playlists and cannot be searched for, so they are added by
    // pasting their link. Returns an error message, or an empty string.
    Q_INVOKABLE QString addPlaylistLink(const QString &link, const QString &name);
    Q_INVOKABLE bool isPlaylistLink(const QString &link) const;
    Q_INVOKABLE void removeAddedPlaylist(const QString &id);
    // URIs of the loaded tracks, for playing a list that has no context.
    Q_INVOKABLE QStringList trackUris() const;

signals:
    void homeChanged();
    void searchResultsChanged();
    void tracksChanged();
    void busyChanged();

private:
    struct AddedPlaylist {
        QString id;
        QString name;
    };

    void loadHome();
    void loadAddedPlaylists();
    void saveAddedPlaylists();
    void fetchAddedPlaylistName(const QString &id);
    QVariantList addedItems() const;
    void setTracks(const QVariantList &tracks, const QString &title, const QString &context,
                   const QString &error = QString());
    void setBusy(bool busy);

    SpotifyApi *m_api;
    QString m_userId;
    QList<AddedPlaylist> m_added;
    QVariantList m_home;
    QVariantList m_searchResults;
    QVariantList m_tracks;
    QString m_tracksTitle;
    QString m_tracksContext;
    QString m_tracksError;
    bool m_busy;
};

#endif // SPOTIFYBROWSER_H
