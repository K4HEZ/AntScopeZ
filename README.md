# AntScopeZ

AntScopeZ supports various models of RigExpert antenna analyzers (plus a
few other brands) across Windows, Linux and macOS -- see the
[User Guide's Supported Devices section](docs/user-guide.md#supported-devices)
for the full list. It is a
fork of RigExpert's own AntScope2 software, renamed to keep this project's
identity, config files, and installed package clearly separate from the
vendor's.

This project is NOT an official project from RigExpert (see: RigExpert's own
AntScope2). Do not contact them for support for this project. It is provided
AS-IS and I do not accept any responsibility for its use. If doing important
tasks (such as licensing and firmware updates), you should use the vendor's
code instead. I have no way of testing those features. The original AntScope2
software may be obtained directly from RigExpert at
[rigexpert.com/software/antscope2](https://rigexpert.com/software/antscope2/).

Some of the non-English UI translations are AI-generated and haven't been
reviewed by a native speaker yet -- expect rough edges until that review
happens.

## Requirements

- CMake 3.21+
- A C++17 compiler
- Qt 6.11 (see [Qt version](#qt-version) below; built and tested against
  6.4.2 and 6.11.2, but 6.11 is the version this project targets and
  packaged releases ship with)

See [BUILDINFO.md](BUILDINFO.md) for the required Qt modules, Linux-only
dependencies, and other setup details.

### Qt version

Packaged releases (e.g. the `.deb`) bundle the Qt 6.11 shared libraries the
app needs, so installing one doesn't require a matching system Qt install.
If you're building from source, build against Qt 6.11 -- older Qt versions
(e.g. distro-packaged Qt 6.4.x) have shown real bugs (analyzer connect
failures on fresh installs, resize/repaint artifacts) that don't reproduce
under 6.11. See [BUILDINFO.md](BUILDINFO.md)'s Known issues for detail.

## Building

CMake is the primary build system.

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j$(nproc)
```

The binary lands in `build-cmake/AntScopeZ`, with the runtime data files
(`cables.txt`, `itu-regions-defaults.txt`) and the `.qm` translations copied
next to it. Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build.

For build options, macOS packaging, Qt Creator setup, translations, platform
notes, and known issues, see [BUILDINFO.md](BUILDINFO.md).

## Remote API

AntScopeZ can expose a local, in-process control API for whatever analyzer is currently
connected -- device enumeration, connect/disconnect, sweeps, and live point streaming,
over a small NDJSON-over-TCP protocol. It's for external tools (scripts, an MCP server, a
web dashboard) that want to drive AntScopeZ programmatically; the app's own GUI doesn't
use it.

Off by default. Enable it in Settings > General ("Enable Remote API" + a port field), or
pass `-remote-api-port <n>` on the command line to force it on for a single run. It binds
to `127.0.0.1` only -- not reachable from other machines -- unless you reconfigure that
yourself. `-headless` runs AntScopeZ without showing its window (still a full GUI process
under the hood, just not displayed), useful when driving it purely through this API.

See [`remoteapi/`](remoteapi/) for the implementation and protocol details.

## Windows

Windows support is a beta effort (releases tagged `-win-beta`); see
[BUILDINFO.md](BUILDINFO.md)'s platform notes for what has and hasn't been
verified against real hardware yet.

### Installing a prebuilt release

Download the `AntScopeZ-<version>-win64-beta.zip` asset from the
[Releases page](https://github.com/kc5cd/AntScopeZ/releases), extract it
anywhere, and run `AntScopeZ.exe` inside the extracted `bin\` folder --
it's a fully self-contained build (Qt DLLs and plugins included), so no
separate Qt install is required.

### Building from source

Requires the Qt 6.11.2 MinGW kit installed via the
[Qt Online Installer](https://www.qt.io/download-qt-installer) under
`C:\Qt`, including the SerialPort and Bluetooth (under "Connectivity")
addon components -- both are optional and not installed by default, so
check `C:\Qt\6.11.2\mingw_64\lib\cmake\` for `Qt6SerialPort`/
`Qt6Bluetooth` first.

```
cmake --preset windows-mingw
cmake --build --preset windows-mingw --parallel
```

This is a native build against the Qt Online Installer's bundled
MinGW-w64 toolchain and Ninja (`C:\Qt\Tools`), not a cross-compile and
not a system compiler. Use the `windows-mingw-release` preset instead
for an optimized build.

### Running

Windows builds don't use rpath, so the `AntScopeZ.exe` produced by the
build above (in `build-windows-mingw\`) won't find its Qt DLLs on its
own. To produce a complete, runnable copy -- the same layout the
packaged releases ship, with `windeployqt` run automatically -- install
into a staging directory instead of running the build output directly:

```
cmake --install build-windows-mingw --prefix pkg-windows
```

Then run `pkg-windows\bin\AntScopeZ.exe`.

## License

AntScopeZ, as built and distributed, is licensed under the **GNU General
Public License v3.0 or later** (see [`COPYING`](COPYING)) -- it bundles
GPLv3-licensed components (QCustomPlot, HIDAPI), which makes the combined
application GPLv3 as a whole. AntScopeZ's own source -- both the original
RigExpert AntScope2 codebase it's forked from and everything added or
modified since -- is separately available under the MIT license
([`LICENSE.txt`](LICENSE.txt)). Qt and libusb are used under their LGPL
terms, and a Windows-only FTDI
driver package is bundled under FTDI's own proprietary redistribution
terms. Full per-component licenses and platform-specific notes:
[`THIRD-PARTY-LICENSES.md`](THIRD-PARTY-LICENSES.md).
