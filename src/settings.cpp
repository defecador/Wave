// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <QDir>
#include <QStandardPaths>

QString settingsFile()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/wave.conf");
}
