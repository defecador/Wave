// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPOTIFYAUTH_H
#define SPOTIFYAUTH_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QTcpServer;

// Spotify login using the authorization code flow with PKCE, so no client
// secret is needed. Each user supplies the client ID of their own Spotify
// developer app. The browser redirects to a loopback server run by this class;
// pasting the redirect address into handleRedirect() works as a fallback.
class SpotifyAuth : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool libraryAccess READ libraryAccess NOTIFY libraryAccessChanged)
    // Whether this login may add songs to, and remove them from, Liked songs.
    Q_PROPERTY(bool libraryWrite READ libraryWrite NOTIFY libraryWriteChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString redirectUri READ redirectUri CONSTANT)
    Q_PROPERTY(QString authorizeUrl READ authorizeUrl NOTIFY authorizeUrlChanged)

public:
    explicit SpotifyAuth(QNetworkAccessManager *nam, QObject *parent = nullptr);

    QString clientId() const { return m_clientId; }
    void setClientId(const QString &clientId);
    bool loggedIn() const { return !m_refreshToken.isEmpty(); }
    // Whether Spotify granted the playlist and saved music scopes. A login made
    // before Wave asked for them keeps working for playback only.
    bool libraryAccess() const { return m_libraryAccess; }
    bool libraryWrite() const { return m_libraryWrite; }
    bool busy() const { return m_busy; }
    QString redirectUri() const;
    QString authorizeUrl() const { return m_authorizeUrl; }

    // Calls callback with a valid access token, refreshing it first if needed.
    // The token is empty if the user is not logged in or the refresh failed.
    void withAccessToken(std::function<void(const QString &)> callback);
    // Forces a refresh on the next request, e.g. after the API returned 401.
    void invalidateAccessToken() { m_accessToken.clear(); }

    Q_INVOKABLE bool startLogin();
    Q_INVOKABLE bool handleRedirect(const QString &url);
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void logout();

signals:
    void clientIdChanged();
    void loggedInChanged();
    void libraryAccessChanged();
    void libraryWriteChanged();
    void busyChanged();
    void authorizeUrlChanged();
    void loginFailed(const QString &message);

private:
    void onNewConnection();
    void refresh();
    void requestToken(const QList<QPair<QString, QString> > &params, bool isRefresh);
    void flushPending(const QString &token);
    void setBusy(bool busy);

    QNetworkAccessManager *m_nam;
    QTcpServer *m_server;
    QString m_clientId;
    QString m_accessToken;
    QString m_refreshToken;
    QDateTime m_expiresAt;
    QByteArray m_verifier;
    QByteArray m_state;
    QString m_authorizeUrl;
    bool m_libraryAccess = false;
    bool m_libraryWrite = false;
    bool m_busy;
    QList<std::function<void(const QString &)> > m_pending;
};

#endif // SPOTIFYAUTH_H
