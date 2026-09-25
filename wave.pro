# SPDX-License-Identifier: GPL-3.0-or-later

TARGET = wave

CONFIG += sailfishapp c++11

isEmpty(VERSION): VERSION = 0.2.6

# The version reaches the code through a generated header rather than a define,
# so that changing it recompiles what uses it. A define lives in the Makefile,
# which object files do not depend on, so a bumped version would go unnoticed.
# qmake treats # as a comment and cannot quote ", hence LITERAL_HASH and the
# raw version, which main.cpp turns into a string.
APP_VERSION_HEADER = $$OUT_PWD/appversion.h
APP_VERSION_CONTENT = "$${LITERAL_HASH}define APP_VERSION_RAW $$VERSION"
write_file($$APP_VERSION_HEADER, APP_VERSION_CONTENT)|error("Cannot write $$APP_VERSION_HEADER")
INCLUDEPATH += $$OUT_PWD

QT += dbus network

SOURCES += \
    src/appservice.cpp \
    src/librespotcontroller.cpp \
    src/main.cpp \
    src/settings.cpp \
    src/spotifyapi.cpp \
    src/spotifyauth.cpp \
    src/spotifybrowser.cpp \
    src/spotifyplayer.cpp

HEADERS += \
    src/appservice.h \
    src/librespotcontroller.h \
    src/settings.h \
    src/spotifyapi.h \
    src/spotifyauth.h \
    src/spotifybrowser.h \
    src/spotifyplayer.h

DISTFILES += \
    qml/wave.qml \
    qml/cover/CoverPage.qml \
    qml/pages/AboutPage.qml \
    qml/pages/AccountPage.qml \
    qml/pages/AddPlaylistDialog.qml \
    qml/pages/DevicesPage.qml \
    qml/pages/LibraryPage.qml \
    qml/pages/MadeForYouPage.qml \
    qml/pages/PlayerPage.qml \
    qml/pages/TrackListPage.qml \
    rpm/wave.spec \
    scripts/build-librespot.sh \
    wave.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

license.files = LICENSE
license.path = /usr/share/licenses/$$TARGET
INSTALLS += license

# librespot plays audio on the phone. It is built separately with
# scripts/build-librespot.sh <arch>; pass LIBRESPOT_BIN=<path> to use another
# binary. Without it Wave still works as a remote control.
isEmpty(LIBRESPOT_BIN) {
    contains(QT_ARCH, arm64): \
        LIBRESPOT_BIN = $$PWD/build/deps/librespot/target/aarch64-unknown-linux-gnu/release/librespot
    else:contains(QT_ARCH, arm): \
        LIBRESPOT_BIN = $$PWD/build/deps/librespot/target/armv7-unknown-linux-gnueabihf/release/librespot
}
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
