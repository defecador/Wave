// SPDX-License-Identifier: GPL-3.0-or-later

#include "spotifyauth.h"
#include "settings.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace {

// Must match the redirect URI registered in the user's Spotify developer app.
// Spotify does not accept "localhost"; loopback redirects must use 127.0.0.1.
const quint16 RedirectPort = 8898;
const char *const RedirectPath = "/callback";
const char *const Scopes =
        "user-read-playback-state user-modify-playback-state user-read-currently-playing "
        "playlist-read-private playlist-read-collaborative user-library-read "
        "user-library-modify";

QByteArray base64Url(const QByteArray &data)
{
    return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray randomBytes(int count)
{
    QFile urandom(QStringLiteral("/dev/urandom"));
    if (!urandom.open(QIODevice::ReadOnly))
        return QByteArray();
    const QByteArray bytes = urandom.read(count);
    return bytes.size() == count ? bytes : QByteArray();
}

QString jsonString(const QJsonObject &object, const char *key)
{
    return object.value(QLatin1String(key)).toString();
}

} // namespace

SpotifyAuth::SpotifyAuth(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
    , m_server(new QTcpServer(this))
    , m_busy(false)
{
    // TODO: keep the refresh token in Sailfish Secrets instead of a plain config file.
    QSettings settings(settingsFile(), QSettings::IniFormat);
    m_clientId = settings.value(QStringLiteral("auth/clientId")).toString();
    m_refreshToken = settings.value(QStringLiteral("auth/refreshToken")).toString();

    connect(m_server, &QTcpServer::newConnection, this, &SpotifyAuth::onNewConnection);
}

void SpotifyAuth::setClientId(const QString &clientId)
{
    const QString trimmed = clientId.trimmed();
    if (trimmed == m_clientId)
        return;

    // Tokens only work with the client ID that issued them.
    logout();
    m_clientId = trimmed;
    QSettings settings(settingsFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("auth/clientId"), m_clientId);
    emit clientIdChanged();
}

QString SpotifyAuth::redirectUri() const
{
    return QStringLiteral("http://127.0.0.1:%1%2").arg(RedirectPort).arg(QLatin1String(RedirectPath));
}

void SpotifyAuth::withAccessToken(std::function<void(const QString &)> callback)
{
    if (!loggedIn()) {
        callback(QString());
        return;
    }
    if (!m_accessToken.isEmpty() && QDateTime::currentDateTimeUtc().secsTo(m_expiresAt) > 60) {
        callback(m_accessToken);
        return;
    }

    m_pending.append(callback);
    if (m_pending.size() == 1)
        refresh();
}

bool SpotifyAuth::startLogin()
{
    if (m_clientId.isEmpty()) {
        emit loginFailed(tr("Enter the client ID of your Spotify app first."));
        return false;
    }

    const QByteArray verifier = randomBytes(48);
    const QByteArray state = randomBytes(12);
    if (verifier.isEmpty() || state.isEmpty()) {
        emit loginFailed(tr("Could not generate a secure random value."));
        return false;
    }

    cancelLogin();
    m_verifier = base64Url(verifier);
    m_state = base64Url(state);

    // Not fatal: the user can still paste the redirect address by hand.
    if (!m_server->listen(QHostAddress::LocalHost, RedirectPort))
        qWarning() << "Cannot listen on" << redirectUri() << m_server->errorString();

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), m_clientId);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri());
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("code_challenge"),
                       QString::fromLatin1(base64Url(QCryptographicHash::hash(m_verifier, QCryptographicHash::Sha256))));
    query.addQueryItem(QStringLiteral("state"), QString::fromLatin1(m_state));
    query.addQueryItem(QStringLiteral("scope"), QLatin1String(Scopes));

    QUrl url(QStringLiteral("https://accounts.spotify.com/authorize"));
    url.setQuery(query);
    m_authorizeUrl = url.toString(QUrl::FullyEncoded);
    emit authorizeUrlChanged();
    return true;
}

void SpotifyAuth::onNewConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            if (!socket->canReadLine())
                return;
            // Only the request line matters: "GET /callback?code=...&state=... HTTP/1.1"
            socket->disconnect(this);
            const QList<QByteArray> parts = socket->readLine().trimmed().split(' ');
            const QByteArray target = parts.size() >= 2 ? parts.at(1) : QByteArray();

            bool handled = false;
            if (parts.at(0) == "GET" && target.startsWith(RedirectPath))
                handled = handleRedirect(QStringLiteral("http://127.0.0.1:%1").arg(RedirectPort) + QString::fromLatin1(target));

            const QByteArray body = handled
                    ? QByteArray("<html><body><h1>Wave</h1><p>Logged in. You can go back to Wave.</p></body></html>")
                    : QByteArray("<html><body><h1>Wave</h1><p>Login failed. Go back to Wave and try again.</p></body></html>");
            socket->write(QByteArray("HTTP/1.1 ") + (handled ? "200 OK" : "400 Bad Request")
                          + "\r\nContent-Type: text/html; charset=utf-8"
                          + "\r\nContent-Length: " + QByteArray::number(body.size())
                          + "\r\nConnection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
        });
    }
}

bool SpotifyAuth::handleRedirect(const QString &url)
{
    if (m_verifier.isEmpty()) {
        emit loginFailed(tr("No login in progress. Tap \"Log in with Spotify\" first."));
        return false;
    }

    const QUrlQuery query(QUrl(url.trimmed()));
    const QString error = query.queryItemValue(QStringLiteral("error"));
    const QString code = query.queryItemValue(QStringLiteral("code"));
    if (!error.isEmpty()) {
        emit loginFailed(tr("Spotify refused the login: %1").arg(error));
        return false;
    }
    if (code.isEmpty()) {
        emit loginFailed(tr("That address does not contain a login code."));
        return false;
    }
    if (query.queryItemValue(QStringLiteral("state")) != QString::fromLatin1(m_state)) {
        emit loginFailed(tr("That address belongs to a different login attempt."));
        return false;
    }

    const QString verifier = QString::fromLatin1(m_verifier);
    cancelLogin();

    QList<QPair<QString, QString> > params;
    params << qMakePair(QStringLiteral("grant_type"), QStringLiteral("authorization_code"))
           << qMakePair(QStringLiteral("code"), code)
           << qMakePair(QStringLiteral("redirect_uri"), redirectUri())
           << qMakePair(QStringLiteral("client_id"), m_clientId)
           << qMakePair(QStringLiteral("code_verifier"), verifier);
    requestToken(params, false);
    return true;
}

void SpotifyAuth::cancelLogin()
{
    if (m_server->isListening())
        m_server->close();
    m_verifier.clear();
    m_state.clear();
}

void SpotifyAuth::logout()
{
    cancelLogin();
    const bool wasLoggedIn = loggedIn();
    m_accessToken.clear();
    m_refreshToken.clear();
    m_expiresAt = QDateTime();

    QSettings settings(settingsFile(), QSettings::IniFormat);
    settings.remove(QStringLiteral("auth/refreshToken"));

    if (wasLoggedIn)
        emit loggedInChanged();
}

void SpotifyAuth::refresh()
{
    QList<QPair<QString, QString> > params;
    params << qMakePair(QStringLiteral("grant_type"), QStringLiteral("refresh_token"))
           << qMakePair(QStringLiteral("refresh_token"), m_refreshToken)
           << qMakePair(QStringLiteral("client_id"), m_clientId);
    requestToken(params, true);
}

void SpotifyAuth::requestToken(const QList<QPair<QString, QString> > &params, bool isRefresh)
{
    QUrlQuery form;
    form.setQueryItems(params);

    QNetworkRequest request(QUrl(QStringLiteral("https://accounts.spotify.com/api/token")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    setBusy(true);
    QNetworkReply *reply = m_nam->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, isRefresh]() {
        reply->deleteLater();
        setBusy(false);

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        const QString accessToken = jsonString(json, "access_token");

        if (accessToken.isEmpty()) {
            QString reason = jsonString(json, "error_description");
            if (reason.isEmpty())
                reason = reply->errorString();
            qWarning() << "Token request failed:" << status << reason;

            // A rejected refresh token (access revoked, app deleted) needs a new login.
            // Network errors keep it so the next request can try again.
            if (isRefresh && (status == 400 || status == 401))
                logout();
            else if (!isRefresh)
                emit loginFailed(tr("Could not log in: %1").arg(reason));
            flushPending(QString());
            return;
        }

        const bool wasLoggedIn = loggedIn();
        m_accessToken = accessToken;
        m_expiresAt = QDateTime::currentDateTimeUtc().addSecs(json.value(QStringLiteral("expires_in")).toInt(3600));

        // Spotify reports the scopes it actually granted. Logins made before Wave
        // asked for library access keep working for playback only, so the user has
        // to log in again to browse playlists and saved music.
        const QString scopes = jsonString(json, "scope");
        const bool libraryAccess = scopes.contains(QLatin1String("playlist-read-private"))
                && scopes.contains(QLatin1String("user-library-read"));
        if (libraryAccess != m_libraryAccess) {
            m_libraryAccess = libraryAccess;
            emit libraryAccessChanged();
        }

        const bool libraryWrite = scopes.contains(QLatin1String("user-library-modify"));
        if (libraryWrite != m_libraryWrite) {
            m_libraryWrite = libraryWrite;
            emit libraryWriteChanged();
        }

        // Spotify may rotate the refresh token on every refresh.
        const QString refreshToken = jsonString(json, "refresh_token");
        if (!refreshToken.isEmpty() && refreshToken != m_refreshToken) {
            m_refreshToken = refreshToken;
            QSettings settings(settingsFile(), QSettings::IniFormat);
            settings.setValue(QStringLiteral("auth/refreshToken"), m_refreshToken);
        }

        if (loggedIn() != wasLoggedIn)
            emit loggedInChanged();
        flushPending(m_accessToken);
    });
}

void SpotifyAuth::flushPending(const QString &token)
{
    const QList<std::function<void(const QString &)> > pending = m_pending;
    m_pending.clear();
    for (const auto &callback : pending)
        callback(token);
}

void SpotifyAuth::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}
