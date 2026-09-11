# Wave

An unofficial Spotify client for Sailfish OS.

> **Status: early development.** Wave shows what is playing and controls
> Spotify Connect playback. On aarch64 phones it can also play Spotify on the
> phone itself through a bundled [librespot](https://github.com/librespot-org/librespot),
> without Android App Support.

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

### Playing on the phone

1. Pull down and open **Devices**, then turn on **Play on this phone**.
2. The first time, tap **Log in with Spotify** again. This login is for the
   player on the phone, which uses librespot's own Spotify client ID, not your
   developer app. It is only needed once.
3. When it says "Ready", Wave moves playback to the phone, where it appears as **Wave**.

## Building

Wave is built with the [Sailfish SDK](https://docs.sailfishos.org/Tools/Sailfish_SDK/).

### librespot (optional, aarch64)

The SDK's Rust is too old for librespot 0.8, so librespot is cross-built with
Rust from [rustup](https://rustup.rs) on the host. The SDK engine only does the
C compiling and linking:

```bash
rustup target add aarch64-unknown-linux-gnu && scripts/build-librespot.sh
```

The Wave build picks the binary up from `build/deps/` automatically. Without it,
Wave is built as a remote control only.

### Wave

Use a separate build directory:

```bash
mkdir -p build && cd build && sfdk -c target=SailfishOS-5.1.0.11-aarch64 build ..
```

The RPM ends up in `build/RPMS/`. Use the `armv7hl` target for 32-bit devices
and `i486` for the emulator (both without on-device playback for now).

## Project layout

| Path | Purpose |
| --- | --- |
| `src/spotifyauth.*` | Login (authorization code flow with PKCE), token refresh and storage |
| `src/spotifyplayer.*` | Playback state and Spotify Connect commands through the Web API |
| `src/librespotcontroller.*` | Runs librespot as the on-device Spotify Connect receiver |
| `qml/pages/PlayerPage.qml` | Now playing and playback controls |
| `qml/pages/AccountPage.qml` | Client ID setup and login |
| `qml/pages/DevicesPage.qml` | Play on this phone, and choosing the Spotify Connect device |
| `qml/cover/CoverPage.qml` | App cover with play/pause and next actions |
| `scripts/build-librespot.sh` | Cross-builds librespot for Sailfish OS aarch64 |

## Roadmap

- [x] Log in with PKCE (no client secret)
- [x] Now playing, play/pause, next/previous, seek, shuffle
- [x] Choose the Spotify Connect device
- [x] Play audio on the phone through librespot (aarch64)
- [ ] Keep playing when Wave is closed
- [ ] Lock screen and headset controls (MPRIS)
- [ ] Browse library and playlists, search
- [ ] librespot for armv7hl
- [ ] Store tokens in Sailfish Secrets. For now, the refresh token is kept in
      `~/.config/io.github.wave/wave/wave.conf`, readable by your user.

## Related projects

- [Hutspot](https://github.com/sailfish-spotify/hutspot), the Spotify controller
  for Sailfish OS that proved this approach
- [librespot](https://github.com/librespot-org/librespot), the open source
  Spotify Connect client

## License

Wave is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version. See
[LICENSE](LICENSE).

librespot is distributed under the MIT license.
