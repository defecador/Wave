// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBRESPOTCONTROLLER_H
#define LIBRESPOTCONTROLLER_H

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>

// Runs librespot as a Spotify Connect receiver so audio plays on the phone.
// Playback is still controlled through the Web API (SpotifyPlayer); this class
// only starts and stops the receiver and handles its one-time login.
class LibrespotController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString deviceName READ deviceName CONSTANT)
    Q_PROPERTY(QString loginUrl READ loginUrl NOTIFY loginUrlChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum State {
        Stopped,
        Starting,
        NeedsLogin,
        Running
    };
    Q_ENUM(State)

    explicit LibrespotController(QObject *parent = nullptr);
    ~LibrespotController();

    // False when the package was built without librespot (non-aarch64 builds).
    bool available() const;
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    State state() const { return m_state; }
    QString deviceName() const;
    QString loginUrl() const { return m_loginUrl; }
    QString lastError() const { return m_lastError; }

    // Forgets the receiver's cached Spotify login.
    Q_INVOKABLE void logout();

signals:
    void enabledChanged();
    void stateChanged();
    void loginUrlChanged();
    void lastErrorChanged();
    // The receiver is logged in after the user turned it on; playback can move to it.
    void readyToPlay(const QString &deviceName);
    // The receiver started loading a track, so Spotify's playback state changed.
    void trackLoaded();

private:
    void start();
    void stop();
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void setState(State state);
    void setLoginUrl(const QString &url);
    void setLastError(const QString &error);

    QProcess m_process;
    QByteArray m_buffer;
    bool m_enabled;
    State m_state;
    QString m_loginUrl;
    QString m_lastError;
    bool m_stopping;
    bool m_claimPending;
};

#endif // LIBRESPOTCONTROLLER_H
