// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0
import Wave 1.0

CoverBackground {
    id: cover

    // A cover holds at most two actions, so each one does a second thing on a
    // double tap. Cover actions only report single taps, so two taps inside
    // DoubleTapMs count as a double tap.
    readonly property int doubleTapMs: 300
    readonly property bool canTransfer: librespot.available && librespot.state !== Librespot.Running
    readonly property bool canLike: spotifyPlayer.savable && spotifyAuth.libraryWrite

    function hintText() {
        if (canLike && canTransfer)
            return qsTr("double tap: quit")
        if (canLike)
            return qsTr("double tap: next · quit")
        if (canTransfer)
            return ""
        return qsTr("double tap: next")
    }

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

        Row {
            // The heart on the cover cannot change shape, so the song says
            // for itself whether it is already saved.
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.paddingSmall
            visible: spotifyPlayer.saved

            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "image://theme/icon-s-favorite?" + Theme.highlightColor
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Liked")
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.highlightColor
            }
        }
    }

    Label {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: Theme.itemSizeSmall + Theme.paddingSmall
        }
        text: cover.hintText()
        visible: text !== ""
        font.pixelSize: Theme.fontSizeTiny
        color: Theme.secondaryColor
    }

    // Started by the first tap; while either runs, the next tap is a double tap.
    Timer {
        id: playTap
        interval: cover.doubleTapMs
    }

    Timer {
        id: likeTap
        interval: cover.doubleTapMs
        onTriggered: spotifyPlayer.toggleSaved()
    }

    CoverActionList {
        enabled: true

        CoverAction {
            iconSource: cover.canTransfer
                        ? "image://theme/icon-cover-transfers"
                        : (spotifyPlayer.playing ? "image://theme/icon-cover-pause"
                                                 : "image://theme/icon-cover-play")
            onTriggered: {
                if (cover.canTransfer) {
                    librespot.enabled = true
                    return
                }
                // Play and pause must not wait for a second tap, so a double
                // tap takes back the first tap before skipping.
                if (playTap.running) {
                    playTap.stop()
                    spotifyPlayer.togglePlay()
                    spotifyPlayer.next()
                } else {
                    spotifyPlayer.togglePlay()
                    playTap.restart()
                }
            }
        }

        CoverAction {
            iconSource: cover.canLike ? "image://theme/icon-cover-favorite"
                                      : "image://theme/icon-cover-cancel"
            onTriggered: {
                if (!cover.canLike) {
                    Qt.quit()
                    return
                }
                if (likeTap.running) {
                    likeTap.stop()
                    Qt.quit()
                } else {
                    likeTap.restart()
                }
            }
        }
    }
}
