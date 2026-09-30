// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

// Manages the playlists Spotify makes for one person -- the daily mixes,
// Discover Weekly, Release Radar. They are not in the API, so each one is
// kept by its link and can only be added, removed and ordered here.
Page {
    id: page

    // While a row is being carried: the row that was picked up, where it would
    // land, and how far it has been dragged from its own place.
    property int dragIndex: -1
    property int dropIndex: -1
    property real dragOffset: 0
    property real dragStartY: 0

    readonly property real rowHeight: Theme.itemSizeMedium

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

    function startDrag(index, y) {
        dragIndex = index
        dropIndex = index
        dragStartY = y
        dragOffset = 0
    }

    function dragTo(y) {
        if (dragIndex < 0)
            return
        dragOffset = y - dragStartY
        var wanted = dragIndex + Math.round(dragOffset / rowHeight)
        dropIndex = Math.max(0, Math.min(listView.count - 1, wanted))
    }

    function endDrag() {
        if (dragIndex >= 0 && dropIndex !== dragIndex)
            spotifyBrowser.moveAddedPlaylist(dragIndex, dropIndex)
        dragIndex = -1
        dropIndex = -1
        dragOffset = 0
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        model: spotifyBrowser.addedPlaylists
        // A carried row must not scroll away under the finger.
        interactive: page.dragIndex < 0

        header: PageHeader {
            title: qsTr("Made for you")
            description: qsTr("Press and hold to rearrange")
        }

        PullDownMenu {
            enabled: page.dragIndex < 0
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

        // The delegate itself stays where the view puts it; only the visible row
        // inside it moves, so the drag never fights the view's own layout.
        delegate: Item {
            id: wrapper

            property bool dragging: page.dragIndex === index
            // The rows a carried one has passed slide into the gap it left.
            property real shift: {
                if (page.dragIndex < 0 || dragging)
                    return 0
                if (index > page.dragIndex && index <= page.dropIndex)
                    return -height
                if (index < page.dragIndex && index >= page.dropIndex)
                    return height
                return 0
            }

            width: listView.width
            height: page.rowHeight
            z: dragging ? 2 : 1

            Rectangle {
                id: row

                width: parent.width
                height: parent.height
                y: wrapper.dragging ? page.dragOffset : wrapper.shift
                color: Theme.rgba(Theme.highlightBackgroundColor,
                                  wrapper.dragging || mouse.pressed
                                  ? Theme.highlightBackgroundOpacity : 0)

                Behavior on y {
                    enabled: !wrapper.dragging
                    NumberAnimation { duration: 150; easing.type: Easing.InOutQuad }
                }

                Column {
                    anchors {
                        left: parent.left
                        right: handle.left
                        leftMargin: Theme.horizontalPageMargin
                        rightMargin: Theme.paddingMedium
                        verticalCenter: parent.verticalCenter
                    }

                    Label {
                        width: parent.width
                        text: modelData.name
                        color: mouse.pressed || wrapper.dragging ? Theme.highlightColor
                                                                 : Theme.primaryColor
                        truncationMode: TruncationMode.Fade
                    }

                    Label {
                        width: parent.width
                        // The name is whatever was typed, so show the link's own
                        // id as well to tell two mixes apart.
                        text: modelData.id
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: mouse.pressed || wrapper.dragging ? Theme.secondaryHighlightColor
                                                                 : Theme.secondaryColor
                        truncationMode: TruncationMode.Fade
                    }
                }

                IconButton {
                    id: handle

                    anchors {
                        right: parent.right
                        rightMargin: Theme.horizontalPageMargin
                        verticalCenter: parent.verticalCenter
                    }
                    icon.source: "image://theme/icon-m-clear"
                    enabled: page.dragIndex < 0
                    onClicked: remorse.execute(wrapper, qsTr("Removing"), function() {
                        spotifyBrowser.removeAddedPlaylist(modelData.id)
                    })
                }
            }

            RemorseItem { id: remorse }

            // Sits still while the row moves, so the finger's position stays
            // meaningful for the whole drag.
            MouseArea {
                id: mouse

                anchors.fill: parent
                onClicked: if (page.dragIndex < 0) page.play(modelData)
                onPressAndHold: page.startDrag(index, mapToItem(listView, 0, mouseY).y)
                onPositionChanged: if (wrapper.dragging) page.dragTo(mapToItem(listView, 0, mouseY).y)
                onReleased: if (wrapper.dragging) page.endDrag()
                onCanceled: if (wrapper.dragging) page.endDrag()
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
