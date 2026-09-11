// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPOTIFYAPI_H
#define SPOTIFYAPI_H

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class SpotifyAuth;

// Sends authorized requests to the Spotify Web API, with token refresh,
// rate limiting and error reporting in one place.
class SpotifyApi : public QObject
{
    Q_OBJECT

public:
    // Called with the HTTP status and body. A status other than 200 or 204
    // means the request failed; errorOccurred() has already been emitted.
    typedef std::function<void(int status, const QByteArray &data)> Callback;

    explicit SpotifyApi(QNetworkAccessManager *nam, SpotifyAuth *auth, QObject *parent = nullptr);

    void get(const QString &path, Callback done);
    void send(const QByteArray &verb, const QString &path, const QByteArray &body, Callback done);

signals:
    void errorOccurred(const QString &message);

private:
    void handleReply(QNetworkReply *reply, const Callback &done);

    QNetworkAccessManager *m_nam;
    SpotifyAuth *m_auth;
    QDateTime m_rateLimitedUntil;
};

#endif // SPOTIFYAPI_H
