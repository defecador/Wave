// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPSERVICE_H
#define APPSERVICE_H

#include <QObject>

// Keeps Wave to a single process. Wave stays running without a window while it
// plays on the phone, so launching it again has to show that instance instead
// of starting a second one.
class AppService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.wave.Wave")

public:
    explicit AppService(QObject *parent = nullptr);

    // Claims Wave's D-Bus name. Returns false if another instance already owns it.
    bool registerInstance();
    // Asks the instance that owns the name to show its window.
    static bool activateRunningInstance();

public slots:
    Q_SCRIPTABLE void activate();

signals:
    void activateRequested();
};

#endif // APPSERVICE_H
