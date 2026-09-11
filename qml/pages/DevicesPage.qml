// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0
import Wave 1.0

Page {
    id: page

    function receiverStatus() {
        switch (librespot.state) {
        case Librespot.Starting:
            return qsTr("Starting…")
        case Librespot.NeedsLogin:
            return qsTr("Log in once so this phone can play Spotify")
        case Librespot.Running:
            return qsTr("Ready. Listed below as \"%1\"").arg(librespot.deviceName)
        default:
            return librespot.lastError !== ""
                    ? librespot.lastError
                    : qsTr("Plays Spotify through this phone's speaker or headphones")
        }
    }

    onStatusChanged: {
        if (status === PageStatus.Active)
            spotifyPlayer.refreshDevices()
    }

    Connections {
        target: librespot
        onStateChanged: {
            if (librespot.state === Librespot.Running)
                spotifyPlayer.refreshDevices()
        }
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: spotifyPlayer.devices

        header: Column {
            width: listView.width

            PageHeader {
                title: qsTr("Devices")
            }

            TextSwitch {
                visible: librespot.available
                text: qsTr("Play on this phone")
                description: page.receiverStatus()
                checked: librespot.enabled
                busy: librespot.state === Librespot.Starting
                automaticCheck: false
                onClicked: librespot.enabled = !librespot.enabled
            }

            Button {
                visible: librespot.state === Librespot.NeedsLogin
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Log in with Spotify")
                onClicked: Qt.openUrlExternally(librespot.loginUrl)
            }

            SectionHeader {
                text: qsTr("Spotify Connect devices")
            }
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Forget phone playback login")
                visible: librespot.available
                onClicked: librespot.logout()
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: spotifyPlayer.refreshDevices()
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0
            text: qsTr("No devices found")
            hintText: librespot.available
                      ? qsTr("Turn on \"Play on this phone\", or open Spotify on a computer or speaker")
                      : qsTr("Open Spotify on a computer, speaker or phone")
        }

        delegate: ListItem {
            // Restricted devices cannot be controlled through the Web API.
            enabled: !modelData.restricted && modelData.id !== ""
            contentHeight: Theme.itemSizeMedium
            onClicked: {
                spotifyPlayer.transferTo(modelData.id)
                pageStack.pop()
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin

                Label {
                    width: parent.width
                    text: modelData.name
                    color: modelData.active || highlighted ? Theme.highlightColor : Theme.primaryColor
                    truncationMode: TruncationMode.Fade
                }

                Label {
                    width: parent.width
                    text: modelData.active ? qsTr("%1 · playing").arg(modelData.type) : modelData.type
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                }
            }
        }

        VerticalScrollDecorator { }
    }
}
