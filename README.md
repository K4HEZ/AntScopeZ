# AntScope2

AntScope2 supports various models of RigExpert antenna analyzers across
Windows, Linux and macOS.

This project is NOT an official project from RigExpert (See: RigExpert/AntScope2).  Do not contact them for support for this project.  It is provided AS-IS and I do not accept any responsibility for its use.  If doing important tasks (such as licensing and firmware updates), you should use the vendor's code instead. I have no way of testing those features.

## Requirements

- CMake 3.21+
- A C++17 compiler
- Qt 6.2+ (built and tested against 6.4.2 and 6.11.1)

See [BUILDINFO.md](BUILDINFO.md) for the required Qt modules, Linux-only
dependencies, and other setup details.

## Building

CMake is the primary build system.

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j$(nproc)
```

The binary lands in `build-cmake/AntScope2`, with the runtime data files
(`cables.txt`, `itu-regions-defaults.txt`) and the `.qm` translations copied
next to it. Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build.

For build options, macOS packaging, Qt Creator setup, translations, platform
notes, and known issues, see [BUILDINFO.md](BUILDINFO.md).
