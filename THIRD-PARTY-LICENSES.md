# Third-Party Components

AntScopeZ's own code is Copyright 2016-2020 Rig Expert Ukraine Ltd., licensed
under the MIT terms in `LICENSE.txt` (also mirrored standalone at
`licenses/AntScope2/`, alongside the other component license folders below,
since this is the license of the AntScope2 codebase AntScopeZ was forked
from). The AntScopeZ *application*, as compiled and distributed, is
licensed as a whole under the GNU General Public License v3 or later
(`COPYING`) because it statically incorporates GPLv3 components listed
below.

This file lists every third-party component that ends up inside the
distributed AntScopeZ package -- whether its source lives in this
repository (QCustomPlot, HIDAPI, the FTDI driver package) or it's pulled in
as prebuilt binaries at packaging time (Qt) -- grouped by which platform
build actually uses it, so that anyone building AntScopeZ for a platform
other than the one the original authors targeted knows which license files
must travel with that build.

---

## Qt (bundled on Linux and macOS; Windows mechanism unconfirmed)

- **Not present as source in this repository** -- Qt is consumed as
  official prebuilt binaries (Core, Gui, Widgets, PrintSupport, SerialPort,
  Network, Xml, Concurrent, OpenGL, Bluetooth per `CMakeLists.txt`'s
  `find_package(Qt6 ...)`/`target_link_libraries`), but those binaries ARE
  physically copied into the distributed package, not just dynamically
  linked against whatever Qt the target system happens to have installed.
  This corrects the earlier draft of this file, which incorrectly described
  Qt as an unbundled runtime dependency.
- **Linux (.deb):** `CMakeLists.txt`'s install-rules section privately
  bundles the Qt 6.11 build pinned by the `release` CMake preset
  (`/opt/Qt`, not the distro's Qt) via `qt6_generate_deploy_script()` /
  `qt_deploy_runtime_dependencies()`, copying `libQt6*.so.*` and the Qt
  plugins actually needed into `${CMAKE_INSTALL_LIBDIR}/antscopez/` inside
  the package, with `$ORIGIN`-relative RPATHs so the installed binary loads
  those copies instead of any system Qt. See the "Known issues" bundling
  rationale referenced in that section (system Qt 6.4.2 exhibited real
  connect/repaint bugs not present under 6.11).
- **macOS:** `build.sh` runs `macdeployqt "$APP" -dmg`, which copies the Qt
  frameworks into `AntScopeZ.app/Contents/Frameworks` before producing the
  `.dmg`.
- **Windows:** no `windeployqt` (or equivalent) step was found in
  `CMakeLists.txt` or any script in this repo as of this review --
  confirm separately how/whether Qt DLLs are bundled into the Windows
  build before treating this section as complete for that platform.
- **License:** verified directly against each bundled module's own doc page
  for Qt 6.11 (`doc.qt.io/qt-6/qt{core,gui,widgets,network,xml,concurrent,
  opengl,printsupport,serialport,bluetooth}-index.html`) rather than assumed
  -- every one of them is dual-licensed **LGPLv3 or GPLv2** (plus a
  commercial option, not relevant here). This is *not* the same pairing as
  QCustomPlot/HIDAPI above (GPLv3-or-later) -- notably, **GPLv2-only is not
  compatible with a GPLv3(-or-later) combined work**, so AntScopeZ elects
  the **LGPLv3** branch of Qt's license, not GPLv2. LGPLv3 explicitly
  permits relicensing into a GPLv3-or-later combined work (LGPLv3 section
  4); GPLv2-only does not carry an "or later" clause that would make it
  compatible. See `licenses/Qt/README.md` for the full reasoning.
- **License files added to the repo:** `licenses/Qt/LGPL-3.0.txt` and
  `licenses/Qt/GPL-3.0.txt` (LGPLv3 incorporates GPLv3 by reference, so both
  are needed together), fetched verbatim from gnu.org -- see
  `licenses/Qt/README.md`. These are not yet wired into any `install()`
  rule in `CMakeLists.txt`, so they don't travel with the packaged output
  yet; that's a follow-up build-system change, not done here.
- **Action needed (partially done):** because actual Qt binaries are
  shipped -- not merely linked against a system package whose own
  `/usr/share/doc/.../copyright` would normally travel with it via apt --
  this package is responsible for carrying Qt's own copyright/license
  notices itself. `CMakeLists.txt` already notes that `dpkg-shlibdeps`
  cannot attribute the private bundled copies to a system Qt package (hence
  the `debian/shlibs.local` workaround nearby), which is the same gap:
  nothing yet actually ships Qt's own `LICENSE`/copyright text alongside the
  bundled `.so`/framework files. `licenses/Qt/` (added to this repo, see
  above) is meant to be that text; remaining work is an `install()` rule
  that copies it into the packaged output (e.g. `lib/antscopez/licenses/Qt/`
  on Linux, alongside `Frameworks/` in the macOS bundle) so it actually
  ships with the binaries it covers.

---

## Bundled on every platform (Windows, Linux, macOS)

### QCustomPlot
- **Files:** `qcustomplot.h`, `qcustomplot.cpp`
- **Copyright:** 2011-2015 Emanuel Eichhammer
- **License:** GNU General Public License v3, or (at your option) any later
  version. See `COPYING`.
- **Notes:** Compiled directly into the AntScopeZ binary on all platforms.
  This is the component that requires the whole application to be
  distributed under GPLv3. No commercial QCustomPlot license has been
  purchased; if one ever is, this section and the project's overall license
  choice need to be revisited.

### HIDAPI
- **Files:** `analyzer/usbhid/hidapi/hidapi.h`,
  `analyzer/usbhid/hidapi/linux/hid.c`,
  `analyzer/usbhid/hidapi/mac/hid.c`,
  `analyzer/usbhid/hidapi/windows/hid.c`
- **Copyright:** 2009-2010 Alan Ott, Signal 11 Software
- **License:** Triple-licensed, at the licensee's discretion: GPLv3, a
  BSD-style license, or the original HIDAPI license (see
  `github.com/signal11/hidapi`). **AntScopeZ elects the GPLv3 option**, to
  match the project's overall license. See `COPYING`.
- **Notes:** All three platform-specific `hid.c` files are bundled in this
  repository for cross-platform completeness, but only the one matching the
  target OS is compiled into any given build (Linux build compiles
  `linux/hid.c`, Windows compiles `windows/hid.c`, macOS compiles
  `mac/hid.c`).

---

## Windows-only

### FTDI D2XX driver package
- **Files:** `ftdi/ftd2xx.h`, `ftdi/amd64/ftd2xx.dll` (shipped next to the
  executable by the build), plus the rest of the FTDI CDM driver package
  bundled alongside it: `ftdi/amd64/ftser2k.sys`, `ftdi/amd64/ftdibus.sys`,
  `ftdi/amd64/ftbusui.dll`, `ftdi/amd64/ftserui2.dll`,
  `ftdi/amd64/ftcserco.dll`, `ftdi/amd64/ftlang.dll`
- **Copyright:** 2001-2011 Future Technology Devices International Limited
  (FTDI)
- **License:** FTDI's own proprietary redistribution terms, embedded in
  `ftd2xx.h` and now also extracted verbatim to
  `licenses/FTDI/LICENSE.txt` (see `licenses/FTDI/README.md`) for the same
  reason `licenses/Qt/` exists -- so the notice travels as its own file
  rather than only inline in a header. **Not GPL-compatible and not part of
  the GPLv3 grant above.** Key conditions: usable only in conjunction with
  FTDI-chip-based products; redistributable in any form as long as the
  license text is not modified; no warranty.
- **Why this doesn't conflict with GPLv3:** none of this is linked into the
  AntScopeZ binary at compile or link time. `ftdiinfo.cpp` loads
  `ftd2xx.dll` at runtime via `QLibrary`/`GetProcAddress`, and the driver
  `.sys`/`.dll` files are separate, unmodified FTDI binaries installed
  alongside the application. This is "mere aggregation" under GPLv3 section
  5 -- independent programs distributed on the same medium, not a combined
  work.
- **Why it's needed only on Windows:** Windows has no built-in driver for
  FTDI's USB-serial chips, so RigExpert bundles FTDI's redistributable
  driver package. Do not modify these files or their embedded license
  notices when redistributing.
- **Housekeeping note (not a licensing issue):** `ftdi/windows/win32/` and
  `ftdi/windows/win64/` also contain `ftd2xx.dll`/`ftd2xx.lib` copies that
  are not referenced anywhere in `CMakeLists.txt` -- only
  `ftdi/amd64/ftd2xx.dll` is actually shipped. These look like unused
  leftovers and are candidates for removal, independent of licensing.

---

## Linux-only

### libusb-1.0
- **Not bundled in this repository.** Required as a system dependency (see
  `pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)` in
  `CMakeLists.txt`), dynamically linked against the distribution's
  `libusb-1.0` package.
- **License:** GNU Lesser General Public License v2.1 (upstream project).
  Ordinary dynamic linking against a system package; no bundling or
  additional notice obligations beyond what the distro's own libusb package
  already carries.

### FTDI USB-serial support
- **Not bundled in this repository.** On Linux, FTDI USB-serial chips are
  handled by the mainline kernel's `ftdi_sio` driver (GPLv2, part of the
  Linux kernel itself) exposing standard `/dev/ttyUSB*` devices, read via
  Qt's `QSerialPortInfo`/`QSerialPort`. AntScopeZ ships no FTDI code on
  Linux; the `#ifdef Q_OS_WIN` guard around the FTDI-specific logic in
  `ftdi/ftdiinfo.cpp` confirms this.

---

## macOS-only

### FTDI USB-serial support
- **Not bundled in this repository.** `ftdiinfo.cpp`'s `Q_OS_MAC` branch
  also just enumerates `QSerialPortInfo::availablePorts()` and filters by
  description string; it relies on macOS's own built-in serial driver
  support (or a user-installed FTDI VCP driver) rather than anything
  bundled here.
- HIDAPI's `mac/hid.c` (see above) is the only third-party component
  actually compiled into a macOS build.

---

*Maintainers: if a future change swaps out any of the above (e.g., dropping
QCustomPlot, buying a commercial license for it, or bundling a different
FTDI package on another platform), update this file and re-evaluate whether
the project's overall GPLv3 license is still the correct choice.*
