# Wave

An unofficial Spotify client for Sailfish OS.
Source code: [github.com/defecador/Wave](https://github.com/defecador/Wave)

> **Status: early development.** Wave browses your playlists, saved albums and
> liked songs, searches Spotify, and controls Spotify Connect playback. It can
> also play Spotify on the phone itself through a bundled
> [librespot](https://github.com/librespot-org/librespot), without Android App
> Support, on both aarch64 and armv7hl.

Wave is not affiliated with, endorsed by, or sponsored by Spotify.
Spotify is a trademark of Spotify AB. librespot is an unofficial Spotify client,
and changes on Spotify's side can break it at any time.

## Requirements

- Sailfish OS 5.x
- A **Spotify Premium** account. Spotify only allows playback control, and librespot, with Premium.
- Your own (free) Spotify developer app. See below.

### Why do I need my own Spotify developer app?

Spotify only gives apps like Wave "development mode" access, which is limited
to five users per app. So instead of one client ID built into Wave, everyone
uses their own. Spotify also requires the owner of a development mode app to
have Premium, which Wave needs anyway.

## Setup

1. On a computer, open the [Spotify developer dashboard](https://developer.spotify.com/dashboard) and log in.
2. Create an app:
   - **Name and description:** anything, for example "Wave on my phone"
   - **Redirect URI:** `http://127.0.0.1:8898/callback` (exactly this)
   - **APIs used:** Web API
3. Copy the app's **Client ID**. Wave does not need the client secret.
4. In Wave, pull down, open **Account**, paste the client ID and tap **Log in with Spotify**.
5. Approve access in the browser. It hands the login back to Wave. If the
   browser shows an error page instead, copy that page's address and paste it
   into Wave.

Wave opens on your library. Use its pull-down menu for Devices, the player and
your account.

### Discover Weekly and other Spotify playlists

Spotify does not let apps read the playlists it makes itself, and its "Made for
you" playlists are not in the API at all. To reach Discover Weekly, Release
Radar or a daily mix, copy its link in the Spotify app (three dots ▸ Share ▸
Copy link) and add it in Wave with **Add playlist by link**. Wave cannot list
their songs, but it plays them, and the link keeps working as they change.

Playlists made by other people appear under **Followed lists**. Tapping one
plays it; press and hold to try to see its songs.

> **Upgrading from an older Wave?** Log out and log in again in **Account**.
> Reading playlists and saved music, and liking songs, need permissions that
> older logins do not have, and Wave says so where they are missing.

### The app cover

A Sailfish cover has room for two actions, so each one does a second thing when
tapped twice, and a line above them says which:

| | Tap | Double tap |
| --- | --- | --- |
| Left | Move playback to this phone, or play/pause once it plays here | Next song |
| Right | Like this song, or quit when there is nothing to like | Quit Wave |

### Playing on the phone

1. Pull down and open **Devices**, then turn on **Play on this phone**.
2. The first time, tap **Log in with Spotify** again. This login is for the
   player on the phone, which uses librespot's own Spotify client ID, not your
   developer app. It is only needed once.
3. When it says "Ready", Wave moves playback to the phone, where it appears as **Wave**.

## Building

Wave is built with the [Sailfish SDK](https://docs.sailfishos.org/Tools/Sailfish_SDK/).

### librespot (optional)

The SDK's Rust is too old for librespot 0.8, so librespot is cross-built with
Rust from [rustup](https://rustup.rs) on the host. The SDK engine only does the
C compiling and linking:

```bash
rustup target add aarch64-unknown-linux-gnu && scripts/build-librespot.sh aarch64
```

For 32-bit devices:

```bash
rustup target add armv7-unknown-linux-gnueabihf && scripts/build-librespot.sh armv7hl
```

If `sfdk` cannot reach the build engine (a Docker engine your user may not talk
to, for instance), set `ENGINE_SSH=1` and the script goes in over SSH instead.

The Wave build picks the binary up from `build/deps/` automatically. Without it,
Wave is built as a remote control only.

### Wave

Use a separate build directory:

```bash
mkdir -p build && cd build && sfdk -c target=SailfishOS-5.1.0.11EA-aarch64 build ..
```

Target names follow the release you installed, so check yours with
`sdk-assistant list` and use that name: the early access releases carry an `EA`
suffix, as above. The RPM ends up in `build/RPMS/`. Use the `armv7hl` target for
32-bit devices, and `i486` for the emulator, which has no on-device playback.

Where `sfdk` cannot reach the build engine, the same build runs inside the
engine itself, which has the source tree mounted:

```bash
ssh -i ~/SailfishOS/vmshare/ssh/private_keys/sdk -p 2222 mersdk@127.0.0.1
cd /path/to/Wave/build && mb2 -t SailfishOS-5.1.0.11EA-aarch64 build ..
```

## Project layout

| Path | Purpose |
| --- | --- |
| `src/spotifyauth.*` | Login (authorization code flow with PKCE), token refresh and storage |
| `src/spotifyapi.*` | Authorized Web API requests, rate limiting and error reporting |
| `src/spotifyplayer.*` | Playback state and Spotify Connect commands through the Web API |
| `src/spotifybrowser.*` | Playlists, saved albums, liked songs and search |
| `src/librespotcontroller.*` | Runs librespot as the on-device Spotify Connect receiver |
| `src/appservice.*` | Single instance and window activation over D-Bus |
| `qml/pages/LibraryPage.qml` | Library and search; the page Wave opens on |
| `qml/pages/TrackListPage.qml` | Songs of a playlist, album or liked songs |
| `qml/pages/PlayerPage.qml` | Now playing and playback controls |
| `qml/pages/AccountPage.qml` | Client ID setup and login |
| `qml/pages/AddPlaylistDialog.qml` | Adding a playlist Spotify's API does not list |
| `qml/pages/DevicesPage.qml` | Play on this phone, and choosing the Spotify Connect device |
| `qml/cover/CoverPage.qml` | App cover with play/pause and next actions |
| `scripts/build-librespot.sh` | Cross-builds librespot for Sailfish OS aarch64 and armv7hl |

## Roadmap

- [x] Log in with PKCE (no client secret)
- [x] Now playing, play/pause, next/previous, seek, shuffle
- [x] Choose the Spotify Connect device
- [x] Play audio on the phone through librespot (aarch64 and armv7hl)
- [x] Keep playing when Wave is closed
- [x] Lock screen, headset and car controls (MPRIS), including the
      progress bar and seeking on a car stereo over Bluetooth
- [x] Browse library and playlists, search (first 50 items per list; Spotify
      returns at most 10 search results of each kind in development mode)
- [ ] Store tokens in Sailfish Secrets. For now, the refresh token is kept in
      `~/.config/io.github.wave/wave/wave.conf`, readable by your user.

## Related projects

- [Hutspot](https://github.com/sailfish-spotify/hutspot), the Spotify controller
  for Sailfish OS that proved this approach
- [librespot](https://github.com/librespot-org/librespot), the open source
  Spotify Connect client

## Author

Guillermo Torres — [github.com/defecador](https://github.com/defecador)

Wave is free and always will be. If it is useful to you, you can buy its author
a coffee at [ko-fi.com/gallerman73107](https://ko-fi.com/gallerman73107).

## License

Wave is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version. See
[LICENSE](LICENSE).

librespot is distributed under the MIT license.
