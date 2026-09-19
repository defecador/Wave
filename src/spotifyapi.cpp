// SPDX-License-Identifier: GPL-3.0-or-later

#include "spotifyapi.h"
#include "spotifyauth.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

const char *const ApiBase = "https://api.spotify.com/v1";

} // namespace

SpotifyApi::SpotifyApi(QNetworkAccessManager *nam, SpotifyAuth *auth, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
    , m_auth(auth)
{
}

void SpotifyApi::get(const QString &path, Callback done)
{
    send("GET", path, QByteArray(), done);
}

void SpotifyApi::send(const QByteArray &verb, const QString &path, const QByteArray &body, Callback done)
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
            if (verb == "PUT")
                reply = m_nam->put(request, body);
            else if (verb == "DELETE")
                reply = m_nam->deleteResource(request); // Wave never sends a body with one.
            else
                reply = m_nam->post(request, body);
        }

        connect(reply, &QNetworkReply::finished, this, [this, reply, done]() {
            handleReply(reply, done);
        });
    });
}

void SpotifyApi::handleReply(QNetworkReply *reply, const Callback &done)
{
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();

    if (status == 401) {
        // The token was revoked or expired early; the next request refreshes it.
        m_auth->invalidateAccessToken();
    } else if (status == 429) {
        int seconds = reply->rawHeader("Retry-After").toInt();
        if (seconds <= 0)
            seconds = 30;
        m_rateLimitedUntil = QDateTime::currentDateTimeUtc().addSecs(seconds);
        emit errorOccurred(tr("Spotify rate limit reached. Pausing for %1 seconds.").arg(seconds));
    } else if (status >= 400 || (status == 0 && reply->error() != QNetworkReply::NoError)) {
        // Spotify errors look like {"error": {"status": 404, "message": "...", "reason": "..."}}
        const QString message = QJsonDocument::fromJson(data).object()
                .value("error").toObject().value("message").toString();
        qWarning() << "Spotify refused" << reply->request().url().path()
                   << "with" << status << (message.isEmpty() ? reply->errorString() : message);
        emit errorOccurred(message.isEmpty() ? reply->errorString() : message);
    }

    if (done)
        done(status, data);
}
