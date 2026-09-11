// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>

// Path of Wave's settings file, for QSettings with QSettings::IniFormat.
// Sailjail only allows writing inside ~/.config/<organization>/<application>/,
// so QSettings' default location cannot be used.
QString settingsFile();

#endif // SETTINGS_H
