# ULTA AO2

The Attorney Online client for the ULTA community, based on [AO2-Client](https://github.com/AttorneyOnline/AO2-Client) 2.10.1 (C++/Qt). It works with the [ULTA server](https://github.com/ULTA-AO/tsuserverULTA).

### What's new

* Trio pairing: a second partner on screen, with front, behind or middle order.
* Custom text colors per player.
* Narrator mode, with scene tools for fades and menu overlays.
* Character fade in and out.
* Flashback looks in sepia or black and white.
* A built-in effects pack.
* Discord Rich Presence.
* Automatic updates on Windows and macOS.
* A native macOS build for Apple Silicon.

### Installing

Download the files from the [latest release](https://github.com/ULTA-AO/ulta-client/releases/latest). On Windows take `ULTA-AO-Windows.zip`, on a Mac with Apple Silicon take `ULTA-AO-Mac.zip`. You also need a Case Cafe base install, which is not included.

**Windows:** put the `ULTA` folder from the zip inside your AO folder, next to `Attorney_Online.exe`, then run `ULTA\ULTA_AO.exe`. Your content stays in the normal `base` folder. Do not create a `base` folder inside `ULTA`.

**Mac:** put `ULTA AO.app` in the folder that holds your `base` folder, then open it. If macOS blocks it, allow it once under System Settings, Privacy & Security.

ULTA AO updates itself after the first download.

### Building

The client builds with CMake. Windows uses Qt 5.15 and macOS (Apple Silicon) uses Qt 6. See `README_BUILD.md` for the details.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

You also need the BASS audio libraries and headers from [un4seen.com](https://www.un4seen.com/), which are not included here. To build a client locked to one server, add `-DULTA_LOCKED_SERVER=<host>:<port>` and `-DULTA_LOCKED_NAME="<name>"`.

The built-in effects and narrator content are not in this repository (see Credits). Without them the client still runs, it just lacks those extras.

## Credits

* AO2-Client by the Attorney Online developers.
* Designed to be used with a Case Cafe base install (content by jooj11 and guyfugs).
* Effects pack: Zutra's Vanilla Effects Pack v2 (CC Edition) by Zutra (Discord: _threedott).
* Narrator content compiled by Michael Notback (Discord: michael_notback).
* ULTA logo and title art by Marilith Cribaum (Discord: marilithcribaum01).
* Discord Rich Presence uses [discord-rpc](https://github.com/discord/discord-rpc) by Discord, Inc. (MIT).

## License

This client is based on AO2-Client, which is licensed under the GPLv3 and MIT licenses. See `LICENSE.GPLv3` and `LICENSE.MIT`.

## AI (Artificial Intelligence) Disclaimer

Since AI is a very convoluted topic right now in the coding hemisphere, I will attach this brief memo to be transparent. I did not rely on AI to code nor to explicitly write mass waves of data. AI was only used in one of two ways: (1) to help me learn some complex topics that my prior coding experience did not teach me, or (2) to fix extremely small (e.g. complicated handshake problems between server and client) issues that otherwise would've taken me ages to resolve on my own.
