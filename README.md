# AntScopeZ

AntScopeZ supports various models of RigExpert antenna analyzers (plus a
few other brands) across Windows, Linux and macOS -- see
[SUPPORTED_DEVICES.md](SUPPORTED_DEVICES.md) for the full list. It is a
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
  6.4.2 and 6.11.1, but 6.11 is the version this project targets and
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

## License

AntScopeZ, as built and distributed, is licensed under the **GNU General
Public License v3.0 or later** (see [`COPYING`](COPYING)) -- it bundles
GPLv3-licensed components (QCustomPlot, HIDAPI), which makes the combined
application GPLv3 as a whole. The original RigExpert-authored source is
separately available under the MIT license ([`LICENSE.txt`](LICENSE.txt)).
Qt and libusb are used under their LGPL terms, and a Windows-only FTDI
driver package is bundled under FTDI's own proprietary redistribution
terms. Full per-component licenses and platform-specific notes:
[`THIRD-PARTY-LICENSES.md`](THIRD-PARTY-LICENSES.md).
