// SPDX-License-Identifier: GPL-3.0-or-later

#include "appservice.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

namespace {

// Sailjail only lets Wave own the name derived from wave.desktop.
const char *const ServiceName = "io.github.wave.wave";
const char *const ObjectPath = "/io/github/wave";
const char *const InterfaceName = "io.github.wave.Wave";

} // namespace

AppService::AppService(QObject *parent)
    : QObject(parent)
{
}

bool AppService::registerInstance()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    // Without a session bus (e.g. started over SSH) there is nothing to coordinate with.
    if (!bus.isConnected())
        return true;
    if (!bus.registerService(QLatin1String(ServiceName)))
        return false;
    bus.registerObject(QLatin1String(ObjectPath), this, QDBusConnection::ExportScriptableSlots);
    return true;
}

bool AppService::activateRunningInstance()
{
    const QDBusMessage call = QDBusMessage::createMethodCall(QLatin1String(ServiceName),
                                                             QLatin1String(ObjectPath),
                                                             QLatin1String(InterfaceName),
                                                             QStringLiteral("activate"));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 3000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qWarning() << "Could not activate the running Wave instance:" << reply.errorMessage();
        return false;
    }
    qDebug() << "Handed the launch over to the running Wave instance";
    return true;
}

void AppService::activate()
{
    qDebug() << "Activation requested by a new launch";
    emit activateRequested();
}
