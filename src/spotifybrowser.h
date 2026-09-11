// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPOTIFYBROWSER_H
#define SPOTIFYBROWSER_H

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
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit SpotifyBrowser(SpotifyApi *api, QObject *parent = nullptr);

    QVariantList home() const { return m_home; }
    QVariantList searchResults() const { return m_searchResults; }
    QVariantList tracks() const { return m_tracks; }
    QString tracksTitle() const { return m_tracksTitle; }
    QString tracksContext() const { return m_tracksContext; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void refreshHome();
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void loadPlaylist(const QString &id, const QString &name);
    Q_INVOKABLE void loadAlbum(const QString &id, const QString &name);
    Q_INVOKABLE void loadLikedSongs();
    // URIs of the loaded tracks, for playing a list that has no context.
    Q_INVOKABLE QStringList trackUris() const;

signals:
    void homeChanged();
    void searchResultsChanged();
    void tracksChanged();
    void busyChanged();

private:
    void setTracks(const QVariantList &tracks, const QString &title, const QString &context);
    void setBusy(bool busy);

    SpotifyApi *m_api;
    QVariantList m_home;
    QVariantList m_searchResults;
    QVariantList m_tracks;
    QString m_tracksTitle;
    QString m_tracksContext;
    bool m_busy;
};

#endif // SPOTIFYBROWSER_H
