# Build Information

Details supplementing the quick-start build steps in [README.md](README.md).

## Qt modules

`core gui widgets printsupport serialport network xml concurrent opengl
bluetooth` (see the `find_package(Qt6 ...)` call in `CMakeLists.txt` for the
authoritative list).

`core`, `gui`, `widgets`, `printsupport`, `network`, `xml`, `concurrent`, and
`opengl` all ship with Qt's base "Desktop" component. **`serialport` and
`bluetooth` do not** — if you're installing Qt via the official Qt Online
Installer / Maintenance Tool, you must explicitly check them under your Qt
version's *Additional Libraries*: **Qt Serial Port** and **Qt Bluetooth**.
Building will fail at the `find_package(Qt6 ...)` step (missing component)
if either is skipped. On Linux distro packages (e.g. `apt`), these are
typically separate packages too — e.g. `qt6-serialport-dev` and
`qt6-connectivity-dev` (Bluetooth ships under "connectivity") on
Debian/Ubuntu.

## Qt Linguist tools (for translations only)

Not needed to build the app — the compiled `.qm` translation files are
checked into the repo and staged as-is (see [Translations](#translations)).
Only needed if you're regenerating `.qm` files from the `.ts` sources
yourself: install **Qt Linguist tools** (provides `lrelease`/`lupdate`),
listed under the installer's *Developer and Designer Tools* section, or the
`qt6-l10n-tools` distro package.

## Linux-only

- `pkg-config` (used by `CMakeLists.txt` to locate libusb; `find_package(PkgConfig REQUIRED)` fails the configure step without it)
- `libusb-1.0` development headers (e.g. `libusb-1.0-0-dev` on Debian/Ubuntu)

## Build options

None currently -- `ANTSCOPE_NEW_CONNECTION`, `ANTSCOPE_NEW_ANALYZER`, and
`ANTSCOPE_OLD_TDR` used to gate old code paths they replaced; all three were
always `ON`, and the flags and their dead OFF-path code are gone.
`ANTSCOPE_DEBUG_BLE`'s raw TX/RX `qDebug()` calls are commented out at their
call sites in `analyzer/ble_analyzer.cpp` instead of a build option --
uncomment locally when actually debugging Bluetooth.

## macOS packaging

`build.sh` compiles the translations, runs a release build and produces a
`.dmg` via `macdeployqt`:

```sh
./build.sh [build-dir]
```

## Linux packaging (.deb)

```sh
cmake --preset release
cmake --build --preset release
cd build-release && cpack
```

Produces `antscopez_<version>_amd64.deb`. Uses the `release` preset (Qt 6.11
from `/opt/Qt`), not `system-qt` -- the CMake install rules bundle that
build's own Qt 6.11 libraries and plugins into
`/usr/lib/x86_64-linux-gnu/antscopez/` rather than linking whatever Qt6 the
target's distro ships, so the package doesn't depend on a system Qt install
at all (see "Known issues" below for why). The `system-qt` preset still
exists for reproducing/comparing the system-Qt-specific bugs that motivated
this.

## Qt Creator

Open `CMakeLists.txt` as the project. The old `AntScope.pro` has been removed;
if Qt Creator still shows the qmake project, delete `.qtcreator/AntScope.pro.user`
and reopen.

## Translations

Source `.ts` files live in `locales/`. `QTranslator` loads the compiled `.qm`
files from disk at runtime (via `Settings::languageDataFolder()`), *not* from
the Qt resource system.

The build copies the `.qm` files from the **repository root** next to the
binary, but the `.ts` sources are in `locales/` — so a plain `lrelease` writes
its output where nothing reads it. Until that is reconciled, regenerate with an
explicit output path:

```sh
for ts in locales/*.ts; do
    lrelease "$ts" -qm "$(basename "$ts" .ts).qm"
done
```

`build.sh` does this automatically for the macOS package. Note that the CMake
build does *not* run `lrelease` — the `.qm` files are checked in and staged
as-is.

## Platform notes

Developed on Linuxmint. Using a RigExpert Match RFE (BLE and hidusb):

- **Linux** — builds and runs. Uses the `hidapi` Linux backend and `libusb-1.0`.
  Bluetooth works on Linux.
- **Windows** — uses the `hidapi` Windows backend, `setupapi`, and the bundled
  FTDI DLLs in `ftdi/`. OpenSSL link flags are currently *not* applied; see the
  note in `CMakeLists.txt`.  This project has not been tested on Windows.
- **macOS** — uses the `hidapi` mac backend; `build.sh` drives `macdeployqt`. 
  This project has not been tested on macOS due to not owning the hardware.

## Known issues

- `analyzer/updater/downloader.cpp` uses `QDomDocument::ParseResult`, which is
  Qt 6.5+. A version guard keeps it building on 6.2–6.4.
- `mainwindow.cpp` (~234 KB) and `measurements.cpp` (~192 KB) are very large and
  are the main candidates for being split up.
- **Build/run against Qt 6.11, not an older system Qt (e.g. distro-packaged
  6.4.x).** A `.deb` built and run against system Qt 6.4.2 showed real bugs
  that don't reproduce under Qt 6.11.1: the analyzer sometimes refuses to
  connect on a fresh install even after several manual attempts (works again
  after restarting the app once its ini file exists, not yet root-caused),
  the main window doesn't fully repaint after being resized larger, and
  plot/paint redraws leave stale artifacts behind until something forces a
  repaint (e.g. minimize/restore). All three were confirmed to be Qt-version
  differences, not something wrong with the packaging/install path itself.
  Packaged releases bundle the Qt 6.11 shared libraries specifically to
  avoid this.
