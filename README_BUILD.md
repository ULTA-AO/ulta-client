# Building ULTA AO

The client is built with CMake and Qt 5.15. Windows is the supported target.
macOS and Linux builds have not been tested.

## Dependencies

* Qt 5.15 with the Core, Gui, Network, Widgets, Concurrent, WebSockets and UiTools modules
* BASS, BASS MIDI and BASS Opus from https://www.un4seen.com (not included here; free for non-commercial use)
* Discord Rich Presence library (https://github.com/discord/discord-rpc/releases)
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

## Running it

The client reads the player's existing `base/` folder next to the executable.
The bundled effects pack and narrator content live in `ulta-content/base`. Copy
that folder to `ulta/base` next to the executable.
