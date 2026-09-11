// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Image {
        anchors.fill: parent
        source: spotifyPlayer.coverUrl
        fillMode: Image.PreserveAspectCrop
        opacity: 0.3
        visible: spotifyPlayer.active && source != ""
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: spotifyPlayer.active ? spotifyPlayer.trackName : "Wave"
            color: Theme.highlightColor
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: spotifyPlayer.active ? spotifyPlayer.artists : ""
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }

    CoverActionList {
        enabled: spotifyPlayer.active

        CoverAction {
            iconSource: spotifyPlayer.playing ? "image://theme/icon-cover-pause" : "image://theme/icon-cover-play"
            onTriggered: spotifyPlayer.togglePlay()
        }

        CoverAction {
            iconSource: "image://theme/icon-cover-next-song"
            onTriggered: spotifyPlayer.next()
        }
    }
}
