// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    Connections {
        target: spotifyAuth
        onLoginFailed: errorLabel.text = message
        onLoggedInChanged: {
            if (spotifyAuth.loggedIn) {
                errorLabel.text = ""
                pageStack.pop()
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Account")
            }

            // Logged in

            Label {
                visible: spotifyAuth.loggedIn
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Wave is connected to Spotify.")
                color: Theme.highlightColor
                wrapMode: Text.Wrap
            }

            Button {
                visible: spotifyAuth.loggedIn
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Log out")
                onClicked: spotifyAuth.logout()
            }

            // Login steps

            Column {
                visible: !spotifyAuth.loggedIn
                width: parent.width
                spacing: Theme.paddingMedium

                SectionHeader {
                    text: qsTr("1. Create a Spotify app")
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("Wave connects through your own Spotify developer app. "
                               + "On a computer, open developer.spotify.com/dashboard, create an app, "
                               + "choose \"Web API\" and add this redirect URI:")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    wrapMode: Text.Wrap
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: spotifyAuth.redirectUri
                    color: Theme.highlightColor
                    wrapMode: Text.WrapAnywhere
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Copy redirect URI")
                    onClicked: Clipboard.text = spotifyAuth.redirectUri
                }

                SectionHeader {
                    text: qsTr("2. Enter its client ID")
                }

                TextField {
                    id: clientIdField
                    width: parent.width
                    label: qsTr("Client ID")
                    placeholderText: label
                    text: spotifyAuth.clientId
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    EnterKey.iconSource: "image://theme/icon-m-enter-close"
                    EnterKey.onClicked: focus = false
                }

                SectionHeader {
                    text: qsTr("3. Log in")
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Log in with Spotify")
                    enabled: clientIdField.text.trim().length > 0 && !spotifyAuth.busy
                    onClicked: {
                        errorLabel.text = ""
                        spotifyAuth.clientId = clientIdField.text
                        if (spotifyAuth.startLogin())
                            Qt.openUrlExternally(spotifyAuth.authorizeUrl)
                    }
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("After you approve access, the browser hands the login back to Wave. "
                               + "If it shows an error page instead, copy that page's address and paste it here.")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    wrapMode: Text.Wrap
                }

                TextField {
                    id: redirectField
                    width: parent.width
                    label: qsTr("Address from the browser")
                    placeholderText: label
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhUrlCharactersOnly
                    EnterKey.iconSource: "image://theme/icon-m-enter-close"
                    EnterKey.onClicked: focus = false
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Finish login")
                    enabled: redirectField.text.trim().length > 0 && !spotifyAuth.busy
                    onClicked: {
                        errorLabel.text = ""
                        spotifyAuth.handleRedirect(redirectField.text)
                    }
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Medium
                running: spotifyAuth.busy
                visible: running
            }

            Label {
                id: errorLabel
                visible: text !== ""
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                color: Theme.highlightColor
                wrapMode: Text.Wrap
            }
        }

        VerticalScrollDecorator { }
    }
}
