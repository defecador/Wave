// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    function playFrom(trackUri) {
        // Playlists and albums keep playing past the loaded tracks; loose
        // tracks (liked songs) are sent to Spotify as a list.
        if (spotifyBrowser.tracksContext !== "")
            spotifyPlayer.playContext(spotifyBrowser.tracksContext, trackUri)
        else
            spotifyPlayer.playTracks(spotifyBrowser.trackUris(), trackUri)
        pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: spotifyBrowser.tracks

        header: PageHeader {
            title: spotifyBrowser.tracksTitle
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Play all")
                visible: listView.count > 0
                onClicked: page.playFrom("")
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && !spotifyBrowser.busy
            text: qsTr("No songs")
            hintText: qsTr("Wave loads the first 50 songs of a list")
        }

        delegate: ListItem {
            id: row

            width: listView.width
            contentHeight: Theme.itemSizeMedium
            onClicked: page.playFrom(modelData.uri)

            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: parent.height
                spacing: Theme.paddingMedium

                Image {
                    id: cover
                    anchors.verticalCenter: parent.verticalCenter
                    width: visible ? height : 0
                    height: Theme.itemSizeSmall
                    source: modelData.image
                    visible: modelData.image !== ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - (cover.visible ? cover.width + parent.spacing : 0)

                    Label {
                        width: parent.width
                        text: modelData.name
                        color: row.highlighted ? Theme.highlightColor : Theme.primaryColor
                        truncationMode: TruncationMode.Fade
                    }

                    Label {
                        width: parent.width
                        text: modelData.subtitle
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: row.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                    }
                }
            }
        }

        VerticalScrollDecorator { }
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: spotifyBrowser.busy && listView.count === 0
    }
}
