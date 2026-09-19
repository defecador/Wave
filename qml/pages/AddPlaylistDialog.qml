// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

// Spotify's API does not hand out the "Made for you" playlists, so the only
// way to reach Discover Weekly and the daily mixes is to paste their link.
Dialog {
    id: dialog

    property alias link: linkField.text
    property alias name: nameField.text

    canAccept: spotifyBrowser.isPlaylistLink(linkField.text)

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            DialogHeader {
                acceptText: qsTr("Add")
                title: qsTr("Add a playlist")
            }

            TextField {
                id: linkField
                width: parent.width
                label: qsTr("Playlist link")
                placeholderText: qsTr("https://open.spotify.com/playlist/...")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhUrlCharactersOnly
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: nameField.focus = true
            }

            TextField {
                id: nameField
                width: parent.width
                label: qsTr("Name")
                placeholderText: qsTr("Discover Weekly")
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false
            }

            Item {
                width: 1
                height: Theme.paddingLarge
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("In the Spotify app, open the playlist, tap the three dots and choose "
                           + "Share, then Copy link. Paste it here and give it a name.\n\n"
                           + "Spotify does not let other apps read its own playlists, so Wave "
                           + "cannot list their songs. It can play them, and the link keeps "
                           + "working as the playlist changes every week.")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                wrapMode: Text.Wrap
            }
        }

        VerticalScrollDecorator { }
    }
}
