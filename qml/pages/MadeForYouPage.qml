// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

// Manages the playlists Spotify makes for one person -- the daily mixes,
// Discover Weekly, Release Radar. They are not in the API, so each one is
// kept by its link and can only be added and removed here.
Page {
    id: page

    function addPlaylist() {
        var dialog = pageStack.push(Qt.resolvedUrl("AddPlaylistDialog.qml"))
        dialog.accepted.connect(function() {
            var error = spotifyBrowser.addPlaylistLink(dialog.link, dialog.name)
            if (error !== "")
                errorLabel.show(error)
        })
    }

    function play(item) {
        spotifyPlayer.playContext(item.uri, "")
        pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        model: spotifyBrowser.addedPlaylists

        header: PageHeader {
            title: qsTr("Made for you")
            description: qsTr("Playlists added by link")
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Add playlist by link")
                onClicked: page.addPlaylist()
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0
            text: qsTr("Nothing added yet")
            hintText: qsTr("Spotify does not let apps see the playlists it makes for you. "
                           + "In the Spotify app, open one, tap the three dots and copy its "
                           + "link, then pull down here to add it.")
        }

        delegate: ListItem {
            id: item

            contentHeight: Theme.itemSizeMedium

            function remove() {
                remorseAction(qsTr("Removing"), function() {
                    spotifyBrowser.removeAddedPlaylist(modelData.id)
                })
            }

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Play")
                    onClicked: page.play(modelData)
                }
                MenuItem {
                    text: qsTr("Remove")
                    onClicked: item.remove()
                }
            }

            onClicked: page.play(modelData)

            Column {
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }

                Label {
                    width: parent.width
                    text: modelData.name
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                    truncationMode: TruncationMode.Fade
                }

                Label {
                    width: parent.width
                    // The name is whatever was typed, so show the link's own
                    // id as well to tell two mixes apart.
                    text: modelData.id
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: item.highlighted ? Theme.secondaryHighlightColor
                                            : Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
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
