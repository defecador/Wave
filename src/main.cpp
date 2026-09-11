// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include <qqml.h>

#include <sailfishapp.h>

#include "librespotcontroller.h"
#include "spotifyauth.h"
#include "spotifyplayer.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match OrganizationName/ApplicationName in wave.desktop (Sailjail).
    app->setOrganizationName(QStringLiteral("io.github.wave"));
    app->setApplicationName(QStringLiteral("wave"));
    app->setApplicationVersion(QStringLiteral(APP_VERSION));

    // Exposes the LibrespotController::State enum to QML as Librespot.Running etc.
    qmlRegisterUncreatableType<LibrespotController>("Wave", 1, 0, "Librespot",
                                                    QStringLiteral("Use the librespot context property"));

    QNetworkAccessManager nam;
    SpotifyAuth auth(&nam);
    SpotifyPlayer player(&nam, &auth);
    LibrespotController librespot;
    QObject::connect(&librespot, &LibrespotController::readyToPlay, &player, [&player](const QString &name) {
        player.transferToDeviceNamed(name);
    });

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("spotifyAuth"), &auth);
    view->rootContext()->setContextProperty(QStringLiteral("spotifyPlayer"), &player);
    view->rootContext()->setContextProperty(QStringLiteral("librespot"), &librespot);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
