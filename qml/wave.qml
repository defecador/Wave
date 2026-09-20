// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0
import Amber.Mpris 1.0
import "pages"

ApplicationWindow {
    initialPage: Component { LibraryPage { } }
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

    // Lock screen, headset and Bluetooth controls. They go through the Web API,
    // so they work both for this phone and for other Spotify Connect devices.
    MprisPlayer {
        id: mprisPlayer

        serviceName: "wave"
        identity: "Wave"
        desktopEntry: "wave"

        // MPRIS clients read CanControl once and don't expect it to change, so keep
        // it true and enable the individual actions instead.
        canControl: true
        canPlay: spotifyAuth.loggedIn && spotifyPlayer.active
        canPause: canPlay
        canGoNext: canPlay
        canGoPrevious: canPlay
        canSeek: canPlay && spotifyPlayer.seekable
        canRaise: true

        playbackStatus: !spotifyPlayer.active ? Mpris.Stopped
                                              : spotifyPlayer.playing ? Mpris.Playing : Mpris.Paused

        metaData {
            title: spotifyPlayer.trackName
            contributingArtist: spotifyPlayer.artists
            albumTitle: spotifyPlayer.albumName
            artUrl: spotifyPlayer.coverUrl
            duration: spotifyPlayer.durationMs
        }

        // A car stereo draws a progress bar of its own and asks for the
        // position every time it redraws it. Spotify is only polled every few
        // seconds, so the answer comes from Wave's own clock instead.
        onPositionRequested: position = spotifyPlayer.livePositionMs()
        onSeekRequested: spotifyPlayer.seekBy(offset)
        // Here "position" is the one being asked for, not the property above.
        onSetPositionRequested: spotifyPlayer.seek(position)

        onPlayRequested: spotifyPlayer.play()
        onPauseRequested: spotifyPlayer.pause()
        onPlayPauseRequested: spotifyPlayer.togglePlay()
        onNextRequested: spotifyPlayer.next()
        onPreviousRequested: spotifyPlayer.previous()
        onRaiseRequested: appService.activate()
    }

    Connections {
        target: spotifyPlayer
        // Move the car's progress bar the moment the song jumps, rather than
        // leaving it wrong until the stereo next asks where the song is.
        onSeeked: mprisPlayer.seeked(positionMs)
    }
}
