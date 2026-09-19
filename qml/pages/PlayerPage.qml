// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    // Advanced locally between polls so the slider moves smoothly.
    property int position: 0

    function formatTime(ms) {
        var seconds = Math.floor(ms / 1000)
        var minutes = Math.floor(seconds / 60)
        seconds = seconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function syncPosition() {
        position = spotifyPlayer.progressMs
        if (!slider.down)
            slider.value = position
    }

    Component.onCompleted: syncPosition()

    Connections {
        target: spotifyPlayer
        onPlaybackChanged: page.syncPosition()
        onErrorOccurred: errorLabel.show(message)
    }

    Timer {
        interval: 1000
        repeat: true
        running: spotifyPlayer.playing && Qt.application.state === Qt.ApplicationActive
        onTriggered: {
            page.position = Math.min(page.position + interval, spotifyPlayer.durationMs)
            if (!slider.down)
                slider.value = page.position
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                // Closing the window keeps Wave playing in the background while
                // it plays on the phone, so offer a way to stop completely.
                text: qsTr("Quit Wave")
                visible: librespot.enabled
                onClicked: Qt.quit()
            }
            MenuItem {
                text: qsTr("Account")
                onClicked: pageStack.push(Qt.resolvedUrl("AccountPage.qml"))
            }
            MenuItem {
                text: qsTr("Devices")
                visible: spotifyAuth.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("DevicesPage.qml"))
            }
            MenuItem {
                text: qsTr("Refresh")
                visible: spotifyAuth.loggedIn
                onClicked: spotifyPlayer.refresh()
            }
        }

        ViewPlaceholder {
            enabled: !spotifyAuth.loggedIn
            text: qsTr("Not connected")
            hintText: qsTr("Pull down and open Account to connect Wave to Spotify")
        }

        ViewPlaceholder {
            enabled: spotifyAuth.loggedIn && !spotifyPlayer.active
            text: qsTr("Nothing playing")
            hintText: qsTr("Go back to your library to pick something, or pull down to open Devices")
        }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: "Wave"
                description: spotifyPlayer.active ? qsTr("Playing on %1").arg(spotifyPlayer.deviceName) : ""
            }

            Item {
                visible: spotifyAuth.loggedIn && spotifyPlayer.active
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: Math.min(width, page.height / 2)

                Rectangle {
                    anchors.fill: parent
                    color: Theme.rgba(Theme.highlightBackgroundColor, 0.1)
                    visible: cover.status !== Image.Ready
                }

                Image {
                    id: cover
                    anchors.fill: parent
                    source: spotifyPlayer.coverUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }
            }

            Label {
                visible: spotifyAuth.loggedIn && spotifyPlayer.active
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                text: spotifyPlayer.trackName
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
                wrapMode: Text.Wrap
            }

            Label {
                visible: spotifyAuth.loggedIn && spotifyPlayer.active
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                text: spotifyPlayer.artists
                color: Theme.secondaryHighlightColor
                wrapMode: Text.Wrap
            }

            Slider {
                id: slider
                visible: spotifyAuth.loggedIn && spotifyPlayer.active
                width: parent.width
                minimumValue: 0
                maximumValue: Math.max(1, spotifyPlayer.durationMs)
                valueText: page.formatTime(value)
                label: page.formatTime(spotifyPlayer.durationMs)
                onDownChanged: if (!down) spotifyPlayer.seek(value)
            }

            Row {
                visible: spotifyAuth.loggedIn && spotifyPlayer.active
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: spotifyPlayer.saved ? "image://theme/icon-m-favorite-selected"
                                                     : "image://theme/icon-m-favorite"
                    // Liking needs a permission older logins do not have.
                    enabled: spotifyPlayer.savable && spotifyAuth.libraryWrite
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: spotifyPlayer.toggleSaved()
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: "image://theme/icon-m-shuffle"
                    highlighted: spotifyPlayer.shuffle
                    onClicked: spotifyPlayer.setShuffle(!spotifyPlayer.shuffle)
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: "image://theme/icon-m-previous"
                    onClicked: spotifyPlayer.previous()
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: spotifyPlayer.playing ? "image://theme/icon-l-pause" : "image://theme/icon-l-play"
                    onClicked: spotifyPlayer.togglePlay()
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: "image://theme/icon-m-next"
                    onClicked: spotifyPlayer.next()
                }
            }
        }

        VerticalScrollDecorator { }
    }

    Label {
        id: errorLabel

        function show(message) {
            text = message
            hideTimer.restart()
        }

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.horizontalPageMargin
        }
        visible: text !== ""
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.highlightColor

        Timer {
            id: hideTimer
            interval: 6000
            onTriggered: errorLabel.text = ""
        }
    }
}
