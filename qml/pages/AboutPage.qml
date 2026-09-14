// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("About Wave")
            }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: "/usr/share/icons/hicolor/172x172/apps/wave.png"
                width: Theme.iconSizeLarge
                height: width
                asynchronous: true
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Version %1").arg(Qt.application.version)
                color: Theme.highlightColor
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("An unofficial Spotify client for Sailfish OS. It browses your library, "
                           + "controls Spotify Connect devices, and can play Spotify on this phone "
                           + "without Android App Support. A Spotify Premium account is required.")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                wrapMode: Text.Wrap
            }

            SectionHeader {
                text: qsTr("Playing in the background")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("While \"Play on this phone\" is on, Wave keeps playing after you close "
                           + "it. That is on purpose, not a bug: the music would otherwise stop the "
                           + "moment you leave the app. Opening Wave again returns to the running app.\n\n"
                           + "To stop completely, use \"Quit Wave\" below or in the Now playing menu. "
                           + "Turning off \"Play on this phone\" in Devices also stops playback here.")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                wrapMode: Text.Wrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Quit Wave")
                visible: librespot.enabled
                onClicked: Qt.quit()
            }

            SectionHeader {
                text: qsTr("Author")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: "Guillermo Torres"
                color: Theme.highlightColor
            }

            SectionHeader {
                text: qsTr("License")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Wave is free software under the GNU General Public License, version 3 "
                           + "or later. Playback on this phone uses librespot, which is free "
                           + "software under the MIT license.")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                wrapMode: Text.Wrap
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Wave is not affiliated with, endorsed by or sponsored by Spotify. "
                           + "Spotify is a trademark of Spotify AB.")
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }
        }

        VerticalScrollDecorator { }
    }
}
