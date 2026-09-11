// SPDX-License-Identifier: GPL-3.0-or-later

#include "librespotcontroller.h"
#include "settings.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

#include <sailfishapp.h>

namespace {

const char *const ReceiverName = "Wave";
// librespot serves its own OAuth redirect on http://127.0.0.1:<port>/login.
const int OAuthPort = 5588;

QString binaryPath()
{
    return SailfishApp::pathTo(QStringLiteral("bin/librespot")).toLocalFile();
}

QString cacheDir()
{
    // Holds librespot's credentials.json after the first login.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/librespot");
    QDir().mkpath(dir);
    return dir;
}

} // namespace

LibrespotController::LibrespotController(QObject *parent)
    : QObject(parent)
    , m_enabled(false)
    , m_state(Stopped)
    , m_stopping(false)
    , m_claimPending(false)
{
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyRead, this, &LibrespotController::onReadyRead);
    connect(&m_process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &LibrespotController::onFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        setLastError(tr("Could not start librespot: %1").arg(m_process.errorString()));
        setState(Stopped);
    });

    QSettings settings(settingsFile(), QSettings::IniFormat);
    m_enabled = available() && settings.value(QStringLiteral("librespot/enabled"), false).toBool();
    if (m_enabled)
        start();
}

LibrespotController::~LibrespotController()
{
    stop();
}

bool LibrespotController::available() const
{
    return QFileInfo(binaryPath()).isExecutable();
}

void LibrespotController::setEnabled(bool enabled)
{
    if (enabled == m_enabled || (enabled && !available()))
        return;

    m_enabled = enabled;
    QSettings settings(settingsFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("librespot/enabled"), m_enabled);
    emit enabledChanged();

    // The user just asked to play here, so move playback once the receiver is up.
    m_claimPending = m_enabled;
    if (m_enabled)
        start();
    else
        stop();
}

QString LibrespotController::deviceName() const
{
    return QLatin1String(ReceiverName);
}

void LibrespotController::logout()
{
    stop();
    QFile::remove(cacheDir() + QStringLiteral("/credentials.json"));
    if (m_enabled)
        start();
}

void LibrespotController::start()
{
    if (m_process.state() != QProcess::NotRunning)
        return;

    QStringList args;
    args << QStringLiteral("--name") << QLatin1String(ReceiverName)
         << QStringLiteral("--device-type") << QStringLiteral("smartphone")
         << QStringLiteral("--backend") << QStringLiteral("pulseaudio")
         << QStringLiteral("--bitrate") << QStringLiteral("320")
         << QStringLiteral("--initial-volume") << QStringLiteral("100")
         << QStringLiteral("--disable-discovery")
         << QStringLiteral("--system-cache") << cacheDir()
         // Cached credentials take precedence; OAuth only runs for the first login.
         << QStringLiteral("--enable-oauth")
         << QStringLiteral("--oauth-port") << QString::number(OAuthPort);

    // Name the PulseAudio stream after Wave instead of librespot. Sailfish's volume
    // keys only adjust streams with the "x-maemo" media role, which is what
    // libaudioresource sets for native media players.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PULSE_PROP_application.name"), QStringLiteral("Wave"));
    env.insert(QStringLiteral("PULSE_PROP_media.role"), QStringLiteral("x-maemo"));
    m_process.setProcessEnvironment(env);

    m_buffer.clear();
    setLastError(QString());
    setState(Starting);
    m_process.start(binaryPath(), args);
}

void LibrespotController::stop()
{
    if (m_process.state() == QProcess::NotRunning)
        return;

    m_stopping = true;
    m_process.terminate();
    if (!m_process.waitForFinished(3000)) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    m_stopping = false;

    setLoginUrl(QString());
    setState(Stopped);
}

void LibrespotController::onReadyRead()
{
    m_buffer += m_process.readAll();

    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_buffer.left(newline)).trimmed();
        m_buffer.remove(0, newline + 1);
        if (line.isEmpty())
            continue;
        qDebug().noquote() << "librespot:" << line;

        const QLatin1String browsePrefix("Browse to: ");
        const int browse = line.indexOf(browsePrefix);
        if (browse >= 0) {
            setLoginUrl(line.mid(browse + browsePrefix.size()).trimmed());
            setState(NeedsLogin);
        } else if (line.contains(QLatin1String("Authenticated as"))) {
            setLoginUrl(QString());
            setState(Running);
            if (m_claimPending) {
                m_claimPending = false;
                emit readyToPlay(deviceName());
            }
        } else if (line.contains(QLatin1String(" ms) loaded"))) {
            // "<Track name> (215000 ms) loaded"
            emit trackLoaded();
        } else if (line.contains(QLatin1String(" ERROR "))) {
            // Log lines look like "[2026-09-11T20:00:00Z ERROR librespot] message".
            const int end = line.indexOf(QLatin1String("] "));
            setLastError(end >= 0 ? line.mid(end + 2) : line);
        }
    }
}

void LibrespotController::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_stopping)
        return;

    setLoginUrl(QString());
    if (m_lastError.isEmpty()) {
        setLastError(exitStatus == QProcess::CrashExit
                     ? tr("librespot crashed.")
                     : tr("librespot stopped (exit code %1).").arg(exitCode));
    }
    setState(Stopped);
}

void LibrespotController::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void LibrespotController::setLoginUrl(const QString &url)
{
    if (m_loginUrl == url)
        return;
    m_loginUrl = url;
    emit loginUrlChanged();
}

void LibrespotController::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}
