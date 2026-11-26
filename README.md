# ULTA AO2

The Attorney Online client for the ULTA community, based on [AO2-Client](https://github.com/AttorneyOnline/AO2-Client) 2.10.1 (C++/Qt 5). It works with the [ULTA server](https://github.com/ULTA-AO/tsuserverULTA).

### What's new

* Trio pairing: a second partner on screen, with front, behind or middle order.
* Custom text colors per player.
* Narrator mode, with scene tools for fades and menu overlays.
* Character fade in and out.
* Flashback looks in sepia or black and white.
* A built-in effects pack.

### Building

The client builds with CMake and Qt 5.15. The qmake project file and `README_BUILD.md` are from upstream and don't include the ULTA files.

```bash
cmake -S . -B build -DAO_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

You also need the BASS audio libraries and headers from [un4seen.com](https://www.un4seen.com/), which are not included here. To build a client locked to one server, add `-DULTA_LOCKED_SERVER=<host>:<port>` and `-DULTA_LOCKED_NAME="<name>"`.

The built-in effects and narrator content are not in this repository (see Credits). Without them the client still runs, it just lacks those extras.

## Credits

* AO2-Client by the Attorney Online developers.
* Designed to be used with a Case Cafe base install (content by jooj11 and guyfugs).
* Effects pack: "Zutra's Vanilla Effects Pack v2 (CC Edition)" by Zutra (Discord: _threedott).
* Narrator content compiled by Michael Notback (Discord: michael_notback).
* ULTA logo and title art by Marilith Cribaum (Discord: marilithcribaum01).

## License

This client is based on AO2-Client, which is licensed under the GPLv3 and MIT licenses. See `LICENSE.GPLv3` and `LICENSE.MIT`.
