// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    property string query: ""

    function open(item) {
        switch (item.kind) {
        case "liked":
            spotifyBrowser.loadLikedSongs()
            pageStack.push(Qt.resolvedUrl("TrackListPage.qml"))
            break
        case "playlist":
            spotifyBrowser.loadPlaylist(item.id, item.name)
            pageStack.push(Qt.resolvedUrl("TrackListPage.qml"))
            break
        case "album":
            spotifyBrowser.loadAlbum(item.id, item.name)
            pageStack.push(Qt.resolvedUrl("TrackListPage.qml"))
            break
        case "track":
            spotifyPlayer.playTracks([item.uri], item.uri)
            pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
            break
        }
    }

    Component.onCompleted: {
        if (spotifyBrowser.home.length === 0)
            spotifyBrowser.refreshHome()
    }

    Timer {
        id: searchTimer
        interval: 700
        onTriggered: spotifyBrowser.search(page.query)
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: page.query === "" ? spotifyBrowser.home : spotifyBrowser.searchResults

        header: Column {
            width: listView.width

            PageHeader {
                title: qsTr("Library")
            }

            SearchField {
                width: parent.width
                placeholderText: qsTr("Search Spotify")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false
                onTextChanged: {
                    page.query = text.trim()
                    if (page.query !== "")
                        searchTimer.restart()
                    else
                        searchTimer.stop()
                }
            }
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("About")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
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
                text: qsTr("Now playing")
                visible: spotifyAuth.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: page.query === "" ? spotifyBrowser.refreshHome() : spotifyBrowser.search(page.query)
            }
        }

        ViewPlaceholder {
            // Logins made before Wave asked for library access can only control playback.
            enabled: spotifyAuth.loggedIn && !spotifyAuth.libraryAccess
            text: qsTr("Log in to Spotify again")
            hintText: qsTr("Wave needs new permission to read your playlists and saved music. Pull down, open Account, log out and log in again.")
        }

        ViewPlaceholder {
            enabled: spotifyAuth.libraryAccess && listView.count === 0 && !spotifyBrowser.busy
            text: page.query === "" ? qsTr("Nothing here yet") : qsTr("Nothing found")
            hintText: page.query === ""
                      ? qsTr("Your playlists and saved albums show up here")
                      : qsTr("Spotify returns at most 10 results of each kind")
        }

        delegate: ListItem {
            id: row

            property bool isHeader: modelData.kind === "header"

            width: listView.width
            contentHeight: isHeader ? Theme.itemSizeExtraSmall : Theme.itemSizeMedium
            enabled: !isHeader
            onClicked: page.open(modelData)

            SectionHeader {
                visible: row.isHeader
                text: modelData.name
            }

            Row {
                visible: !row.isHeader
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

    Connections {
        target: spotifyPlayer
        onErrorOccurred: errorLabel.show(message)
    }

    Connections {
        // Wave opens here, so the library may still be waiting for the login.
        target: spotifyAuth
        onLibraryAccessChanged: {
            if (spotifyAuth.libraryAccess && spotifyBrowser.home.length === 0)
                spotifyBrowser.refreshHome()
        }
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
