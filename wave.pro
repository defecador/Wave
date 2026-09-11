# SPDX-License-Identifier: GPL-3.0-or-later

TARGET = wave

CONFIG += sailfishapp c++11

isEmpty(VERSION): VERSION = 0.1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

QT += dbus network

SOURCES += \
    src/appservice.cpp \
    src/librespotcontroller.cpp \
    src/main.cpp \
    src/settings.cpp \
    src/spotifyauth.cpp \
    src/spotifyplayer.cpp

HEADERS += \
    src/appservice.h \
    src/librespotcontroller.h \
    src/settings.h \
    src/spotifyauth.h \
    src/spotifyplayer.h

DISTFILES += \
    qml/wave.qml \
    qml/cover/CoverPage.qml \
    qml/pages/AccountPage.qml \
    qml/pages/DevicesPage.qml \
    qml/pages/PlayerPage.qml \
    rpm/wave.spec \
    scripts/build-librespot.sh \
    wave.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

license.files = LICENSE
license.path = /usr/share/licenses/$$TARGET
INSTALLS += license

# librespot plays audio on the phone. It is built separately with
# scripts/build-librespot.sh (aarch64 only for now); pass LIBRESPOT_BIN=<path>
# to use another binary. Without it Wave still works as a remote control.
isEmpty(LIBRESPOT_BIN):contains(QT_ARCH, arm64): \
    LIBRESPOT_BIN = $$PWD/build/deps/librespot/target/aarch64-unknown-linux-gnu/release/librespot
!isEmpty(LIBRESPOT_BIN):exists($$LIBRESPOT_BIN) {
    librespot.files = $$LIBRESPOT_BIN
    librespot.path = /usr/share/$$TARGET/bin
    librespot.CONFIG += executable
    # librespot is MIT licensed; its notice has to ship with the binary.
    librespot_license.files = $$PWD/build/deps/librespot/LICENSE
    librespot_license.path = /usr/share/licenses/$$TARGET/librespot
    INSTALLS += librespot librespot_license
    message("Bundling librespot from $$LIBRESPOT_BIN")
} else {
    message("librespot not found for $$QT_ARCH; building without on-device playback")
}
