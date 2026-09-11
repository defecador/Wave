// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QScopedPointer>
#include <QSocketNotifier>
#include <qqml.h>

#include <sailfishapp.h>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include "appservice.h"
#include "librespotcontroller.h"
#include "spotifyauth.h"
#include "spotifyplayer.h"

namespace {

int signalFds[2] = { -1, -1 };

void handleQuitSignal(int)
{
    // Only async-signal-safe work here; the event loop does the actual quitting.
    const char byte = 1;
    const ssize_t written = ::write(signalFds[0], &byte, sizeof(byte));
    Q_UNUSED(written);
}

// Quit through the event loop on SIGTERM, SIGINT and SIGHUP, so that
// LibrespotController's destructor stops librespot instead of leaving it
// playing on its own after Wave is gone.
void installQuitSignalHandlers(QCoreApplication *app)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalFds) != 0)
        return;

    QSocketNotifier *notifier = new QSocketNotifier(signalFds[1], QSocketNotifier::Read, app);
    QObject::connect(notifier, &QSocketNotifier::activated, app, [notifier]() {
        notifier->setEnabled(false);
        char byte;
        const ssize_t bytesRead = ::read(signalFds[1], &byte, sizeof(byte));
        Q_UNUSED(bytesRead);
        QCoreApplication::quit();
    });

    struct sigaction action;
    action.sa_handler = handleQuitSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    ::sigaction(SIGTERM, &action, nullptr);
    ::sigaction(SIGINT, &action, nullptr);
    ::sigaction(SIGHUP, &action, nullptr);
}

} // namespace

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match OrganizationName/ApplicationName in wave.desktop (Sailjail).
    app->setOrganizationName(QStringLiteral("io.github.wave"));
    app->setApplicationName(QStringLiteral("wave"));
    app->setApplicationVersion(QStringLiteral(APP_VERSION));

    AppService appService;
    if (!appService.registerInstance()) {
        // Wave is already running, possibly in the background: show that instance.
        AppService::activateRunningInstance();
        return 0;
    }

    installQuitSignalHandlers(app.data());

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

    // While Wave plays on the phone, closing the window keeps it (and librespot)
    // running in the background. Otherwise closing the window quits as usual.
    app->setQuitOnLastWindowClosed(false);
    const auto quitIfIdle = [&view, &librespot, &app]() {
        if (!view->isVisible()
                && (!librespot.enabled() || librespot.state() == LibrespotController::Stopped)) {
            app->quit();
        }
    };
    QObject::connect(app.data(), &QGuiApplication::lastWindowClosed, app.data(), quitIfIdle);
    QObject::connect(&librespot, &LibrespotController::enabledChanged, app.data(), quitIfIdle);
    QObject::connect(&librespot, &LibrespotController::stateChanged, app.data(), quitIfIdle);
    QObject::connect(view->engine(), &QQmlEngine::quit, app.data(), &QGuiApplication::quit);

    // No need to poll Spotify's playback state while there is no window.
    QObject::connect(view.data(), &QWindow::visibleChanged, &player, [&player, &auth](bool visible) {
        player.setPolling(visible && auth.loggedIn());
    });

    QObject::connect(&appService, &AppService::activateRequested, view.data(), [&view]() {
        view->show();
        view->raise();
        view->requestActivate();
    });

    return app->exec();
}
