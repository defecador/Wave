// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    initialPage: Component { PlayerPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
    _defaultPageOrientations: defaultAllowedOrientations

    // Poll often while Wave is on screen and slowly while it is only a cover,
    // to stay well inside Spotify's development mode quota.
    Binding {
        target: spotifyPlayer
        property: "polling"
        value: spotifyAuth.loggedIn
    }
    Binding {
        target: spotifyPlayer
        property: "pollInterval"
        value: Qt.application.state === Qt.ApplicationActive ? 5000 : 30000
    }
}
