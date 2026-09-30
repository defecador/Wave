// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {

// Where wave.conf lived before the package was renamed harbour-wave, to keep
// its OrganizationName/ApplicationName (and Sailjail id) from colliding with
// an unrelated "wave" package elsewhere in the repos. QStandardPaths reads
// those two names live, so this has to be built by hand instead.
QString legacySettingsFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/io.github.wave/wave/wave.conf");
}

} // namespace

QString settingsFile()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    const QString file = dir + QStringLiteral("/harbour-wave.conf");

    // Carry over the Spotify login and added playlists from before the rename,
    // the first time the renamed package runs.
    if (!QFile::exists(file) && QFile::exists(legacySettingsFile()))
        QFile::copy(legacySettingsFile(), file);

    return file;
}
