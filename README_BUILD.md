# Building ULTA AO

The client is built with CMake. Windows (Qt 5.15) and macOS on Apple Silicon
(Qt 6) are supported. Linux has not been tested.

## Dependencies

* Qt 5.15 (Windows) or Qt 6 (macOS) with the Core, Gui, Network, Widgets, Concurrent, WebSockets and UiTools modules
* BASS, BASS MIDI and BASS Opus from https://www.un4seen.com (not included here; free for non-commercial use)
* Discord Rich Presence library, version 3.4.0 (https://github.com/discord/discord-rpc/releases)
* QtApng plugin (https://github.com/jurplel/QtApng), built for the same Qt version

Put the BASS and Discord headers and libraries in `lib/`. The BASS files are
ignored by git on purpose. Copy the QtApng plugin into an `imageformats` folder
next to the executable.

## Configure and build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

If your CMake complains about old policy versions, add
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.

## ULTA options

* `ULTA_VERSION`: version shown in the lobby and About box (default `1.0.0`).
* `ULTA_LOCKED_SERVER`: lock the client to one server, as `host:port`. The lobby then shows only that server. Empty builds the open client.
* `ULTA_LOCKED_NAME`: display name of the locked server.
* `ULTA_UPDATE_URL`: https address of `latest.json` for the update check. Empty turns the update check off.
* `ULTA_UPDATE_ALLOW_HTTP`: allow an http update address. For local tests only.

A Windows build also produces `ulta-updater.exe`, the helper that installs
updates.

## macOS

Install Qt 6 (for example with Homebrew) and build the same way, pointing CMake at it:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build -j8
```

This produces `ULTA AO.app`. A few extra steps make it a standalone app:

* Build the QtApng plugin against Qt 6 (`-DAPNG_QT6=ON`) and copy it into `Contents/PlugIns/imageformats`.
* Copy `libbass.dylib` and `libbassopus.dylib` into `Contents/Frameworks`. BASS MIDI is not used on macOS.
* Run `macdeployqt` on the app, then copy `ulta-content/base` to `Contents/Resources/ulta/base`.
* Sign the app. An ad-hoc signature is enough (`codesign --force --deep -s - "ULTA AO.app"`).

The client reads the player's `base/` folder next to the `.app`.

For Discord presence, build discord-rpc 3.4.0 as a static library and pass
`-DULTA_DISCORD_DIR=<folder>`. The folder needs `include/discord-rpc.h` and
`libdiscord-rpc.a`. Use the 3.4.0 release: newer versions change the layout of the
presence data and crash the client.

## Updates

Both platforms update themselves when `ULTA_UPDATE_URL` is set. The update
information (`latest.json`) is signed with Ed25519 and the client only trusts it
if the signature matches the key built into it. It lists one download per
platform, each with its size and SHA-256. On Windows the helper `ulta-updater.exe`
installs the update. On macOS a short script replaces the app bundle and puts the
old one back if anything fails.

## Running it

The client reads the player's existing `base/` folder next to the executable.
The bundled effects pack and narrator content live in `ulta-content/base`. Copy
that folder to `ulta/base` next to the executable (inside the app bundle on macOS,
at `Contents/Resources/ulta/base`).
