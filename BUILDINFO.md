---
layout: default
title: Build Instructions
---

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

## Qt Linguist tools (for translations)

Needed to build the app: `.qm` translation files are compiled from
`locales/*.ts` as part of the normal CMake build (see
[Translations](#translations)), not checked in. Install **Qt Linguist
tools** (provides `lrelease`/`lupdate`), listed under the installer's
*Developer and Designer Tools* section, or the `qt6-l10n-tools` distro
package. `find_package(Qt6 ... LinguistTools)` fails the configure step if
it's missing.

## Linux-only

- `pkg-config` (used by `CMakeLists.txt` to locate libusb; `find_package(PkgConfig REQUIRED)` fails the configure step without it)
- `libusb-1.0` development headers (e.g. `libusb-1.0-0-dev` on Debian/Ubuntu)

## Build options

`ANTSCOPEZ_SANITIZE` (off by default) builds with AddressSanitizer +
UndefinedBehaviorSanitizer -- catches out-of-bounds reads/writes and
similar live, with an exact file/line/stack, instead of by code audit.
Configure a separate build dir (`CMAKE_PREFIX_PATH` must be passed
explicitly, or CMake silently resolves the system's older bundled Qt6
instead of the one under `/opt/Qt`):

```sh
cmake -B build-debug-asan -DCMAKE_BUILD_TYPE=Debug -DANTSCOPEZ_SANITIZE=ON \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.11.2/gcc_64
cmake --build build-debug-asan --target AntScopeZ --parallel
```

Run with `ASAN_OPTIONS="suppressions=../asan-suppressions.txt:detect_leaks=0" ./AntScopeZ`
from `build-debug-asan/` -- the suppressions file (repo root) covers a
real bug in `libxcb-cursor` (not ours) and turns off LeakSanitizer's
exit-time report, which for a Qt/GTK GUI app is almost entirely
third-party-library noise rather than anything actionable.

`.clang-tidy` (repo root) is also available for static analysis --
`bugprone-*`/`clang-analyzer-*` checks, no separate install needed since
both it and `clang-tidy` itself already ship with Qt Creator's bundled
clang toolchain. Point it at `build-debug/.qtc_clangd/compile_commands.json`
(already generated for clangd).

Otherwise none currently -- `ANTSCOPE_NEW_CONNECTION`, `ANTSCOPE_NEW_ANALYZER`,
and `ANTSCOPE_OLD_TDR` used to gate old code paths they replaced; all
three were always `ON`, and the flags and their dead OFF-path code are
gone. `ANTSCOPE_DEBUG_BLE`'s raw TX/RX `qDebug()` calls are commented
out at their call sites in `analyzer/ble_analyzer.cpp` instead of a
build option -- uncomment locally when actually debugging Bluetooth.

## macOS packaging

`build.sh` runs a release build (which compiles the translations as part of
the normal CMake build -- see [Translations](#translations)) and produces a
`.dmg` via `macdeployqt`:

```sh
./build.sh [build-dir]
```

## Build performance

Always build with `--parallel` (or `-j<N>`) -- `cmake --build` on the Unix
Makefiles generator (the default here; no generator is pinned in
`CMakePresets.json`) defaults to serial, one file at a time, regardless of
how many cores are available. Confirmed (2026-08-16): touching a
widely-included header and rebuilding took 3m07s plain vs 51s with
`--parallel 16` on a 16-core box -- same build, same everything else. Qt
Creator's own Build button is a separate question -- check Projects > Build
Settings > Build Steps for a jobs override if it also feels serial.

This is unrelated to translations (`.ts`/`.qm`, see
[Translations](#translations) below) and unrelated to the `.deb`
self-dependency fix (see "Known issues") -- that fix only touches the CPack
packaging step (`cpack`/`fix-deb-self-dependency.sh`), never `cmake --build`,
so it structurally can't affect ordinary build times.

## Linux packaging (.deb)

```sh
cmake --preset release
cmake --build --preset release --parallel
cd build-release && cpack
../cmake/fix-deb-self-dependency.sh antscopez_<version>_amd64.deb
```

Produces `antscopez_<version>_amd64.deb`. The `fix-deb-self-dependency.sh`
step is required, not optional -- see "Known issues" below (self-dependency
bug). Uses the `release` preset (Qt 6.11
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

Source `.ts` files live in `locales/`. `qt_add_translations()` in
`CMakeLists.txt` compiles them to `.qm` at build time (target
`release_translations`, built by default -- `AntScopeZ` depends on it, so a
parallel build can't race ahead of it) and leaves them as loose files (not
embedded in the Qt resource system): `QTranslator` loads them from disk at
runtime, checking a per-user override folder before the shared/installed
copy (`MainWindow::loadLanguage()`; see `Settings::localDataFolder()` /
`languageDataFolder()`). Nothing needs to be checked in or regenerated by
hand — building the app regenerates them from whatever's currently in
`locales/*.ts`.

The View → Language menu is populated by scanning both of
those folders for `QtLanguage_<code>.qm` files (`Settings::availableLanguages()`,
called from `MainWindow` -- Settings itself no longer has a language control of its
own, moved to the View menu along with Theme/Bands highlighting/Band Selector),
not from a fixed list -- adding a language is "add `locales/QtLanguage_<code>.ts`,
rebuild" (or, without a rebuild, drop a `.qm` compiled elsewhere into either
folder). The combo's display name for each comes from `QLocale(code).nativeLanguageName()`,
since the `.ts`/`.qm` format has no display-name field of its own.

To add or update a translation: edit/create `locales/QtLanguage_<code>.ts`
(Qt Linguist, or by hand) and rebuild -- or run the `update_translations`
target first (manual/opt-in, since it rewrites `.ts` file contents) to have
`lupdate` refresh `locales/*.ts` from the current source strings before
translating:

```sh
cmake --build --preset debug --target update_translations --parallel
```

`qt_add_translations()` passes `-no-obsolete` to `lupdate` (see
`CMakeLists.txt`), so a string no longer found in source is dropped
outright rather than left behind marked `obsolete`/`vanished` -- keeps
`locales/*.ts` from accumulating dead entries across releases.

Before assuming a "translated" string just needs updating, check whether
its `<translation>` is actually just a copy of the English `<source>` --
`lupdate`'s same-text heuristic can reuse a match from elsewhere in the
file for a *new* entry, but older entries translated by hand or by an
earlier pass can also just be an untouched English copy with no visual
indication besides that. Found repeatedly (2026-08-16) across all three
languages this way, well after they'd otherwise seemed complete.

## Build timestamp

`cmake/generate-build-timestamp.cmake`, invoked via an `add_custom_target()`
with no tracked `OUTPUT` (so it reruns on every build, not just on
reconfigure), regenerates `build-timestamp.h` in the build directory with
`ANTSCOPEZ_BUILD_TIMESTAMP` (`yymmdd-hhmmss`, local time) fresh each time.
Shown in Help → About AntScopeZ, below the version. A plain
`target_compile_definitions()` value (like `ANTSCOPEZ_VER`) would only be
recomputed at configure time, going stale across ordinary incremental
rebuilds -- not useful for actually identifying which build you're
looking at.

## Platform notes

Developed on Linuxmint. Using a RigExpert Match RFE (BLE and hidusb):

- **Linux** — builds and runs. Uses the `hidapi` Linux backend and `libusb-1.0`.
  Bluetooth works on Linux.
- **Windows** — uses the `hidapi` Windows backend, `setupapi`, and the bundled
  FTDI DLLs in `ftdi/`. OpenSSL link flags are currently *not* applied; see the
  note in `CMakeLists.txt` -- confirmed 2026-08-30 this is correct as-is, not
  an oversight: Qt's prebuilt Windows kit ships a Schannel TLS backend by
  default and has no OpenSSL 3.x runtime to fall back to, so the one live
  HTTPS caller (`src/licenseagent.cpp`) works with no extra linkage. `CMakePresets.json`
  has a `windows-mingw`/`windows-mingw-release` pair (Qt Online Installer's
  Qt 6.11.2 MinGW kit under `C:/Qt`) and `CMakeLists.txt` has an NSIS
  packaging block (`cpack -G NSIS`). Merged into `develop` 2026-09-06 (PR #11,
  `windows-dev-port`, plus a follow-up commit fixing 5 bugs the merge review
  turned up). A real `windows-mingw` build has been done and verified: it
  compiles cleanly, `cmake --install` deploys a self-contained `bin/` folder
  via Qt's `qt_deploy_runtime_dependencies()`, and the deployed `.exe`
  launches and runs. What's *not* yet verified is anything needing real
  Windows hardware -- device enumeration/connection (HID, FTDI), `.asd` file
  association, `WM_DEVICECHANGE` hot-plug detection, per-user settings paths.
  See `docs/windows-port-audit.md` for the full checklist and findings.
- **macOS** — uses the `hidapi` mac backend; `build.sh` drives `macdeployqt`. 
  This project has not been tested on macOS due to not owning the hardware.

## Known issues

- **`.deb` self-dependency bug in `dpkg-shlibdeps`.** On build machines that
  also have distro Qt6 packages installed alongside `/opt/Qt` (e.g. this
  project's own dev box, kept that way on purpose for the `system-qt` preset
  comparisons below), `CPACK_DEBIAN_PACKAGE_SHLIBDEPS`'s call to
  `dpkg-shlibdeps` against the bundled-Qt libraries under
  `usr/lib/<triplet>/antscopez` has been observed to non-deterministically
  misattribute some of those libraries back to the antscopez package itself,
  producing a literal `Depends: antscopez (>= <version>)` in the control
  file -- a package can't depend on itself; `apt`/`dpkg` refuse to install it
  on any machine that doesn't already have it. `CMakeLists.txt` sets
  `CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS` as a partial mitigation, but
  root cause traces into `dpkg-shlibdeps`'s own path-resolution internals
  (relative vs. absolute paths change its `$ORIGIN`/package-root detection),
  not something fully fixable via CPack config. `cmake/fix-deb-self-dependency.sh`
  (run as the last step of the packaging recipe above) guarantees a clean
  result by stripping any such self-reference from the built `.deb` after
  the fact, regardless of whether/how the misattribution recurs.
  **Separately known and left as-is (cosmetic, not install-blocking):** on a
  machine with system Qt6 packages installed, the same mechanism can also
  attribute bundled Qt libraries to *those* real packages (e.g.
  `libqt6core6t64`) instead of treating them as private -- extra, unneeded
  `Depends:` entries rather than a missing/broken one, since the app still
  loads the bundled Qt 6.11 at runtime via RPATH regardless. Not fixed here.
- `analyzer/updater/downloader.cpp` uses `QDomDocument::ParseResult`, which is
  Qt 6.5+. A version guard keeps it building on 6.2–6.4.
- ~~`mainwindow.cpp` (~234 KB) and `measurements.cpp` (~192 KB) are very
  large and are the main candidates for being split up.~~ **Done
  (2026-08-10):** both split into topical files sharing the same class
  (`mainwindow_shortcuts.cpp`, `_mouse.cpp`, `_tabs.cpp`, `_multitab.cpp`,
  `_analyzer.cpp`, `_scan.cpp`, `_frequency.cpp`, `_measurements_io.cpp`,
  `_presets_bands.cpp`, `_markers.cpp`, `_settings.cpp`;
  `measurements_popups.cpp`, `_io.cpp`, `_tdr.cpp`, `_redraw.cpp`,
  `_farend.cpp`, `_autocal.cpp`, `_onefq.cpp`) -- pure code motion, no
  behavior change, ~1,200 lines of confirmed-dead code (old
  `/* */`-commented and `#if 0`'d implementations, found interleaved with
  live code during the split) removed at the same time. `mainwindow.cpp`
  itself is down to ~1,200 lines, `measurements.cpp` to ~1,600.
- **Build/run against Qt 6.11, not an older system Qt (e.g. distro-packaged
  6.4.x).** A `.deb` built and run against system Qt 6.4.2 showed real bugs
  that don't reproduce under Qt 6.11: the analyzer sometimes refuses to
  connect on a fresh install even after several manual attempts (works again
  after restarting the app once its ini file exists, not yet root-caused),
  the main window doesn't fully repaint after being resized larger, and
  plot/paint redraws leave stale artifacts behind until something forces a
  repaint (e.g. minimize/restore). All three were confirmed to be Qt-version
  differences, not something wrong with the packaging/install path itself.
  Packaged releases bundle the Qt 6.11 shared libraries specifically to
  avoid this.
- **PDF output (all three paths) was silently coming out as A4 regardless
  of an explicit Letter page size in code -- fixed 2026-08-11.** Affected:
  Print dialog's "Save as .pdf" (`Print::on_pdfPrintBtn_clicked()`), the
  "Screenshot from AA" dialog's "Export to PDF" (`Screenshot::savePDF()`),
  and Print dialog's "Print" &rarr; "Print to File (PDF)"
  (`Print::on_printBtn_clicked()`). Root cause: all three rendered through
  `QPrinter`, which simulates a physical printer/driver -- its own
  driver-default resolution logic was silently overriding an explicitly-set
  page size even when nothing else touched it afterward. Fixed by switching
  actual PDF-file output to `QPdfWriter` (a direct `QPagedPaintDevice` for
  PDF, no driver emulation) in all three, with the page size set via the
  `QPageLayout` round-trip form (`layout = device.pageLayout();
  layout.setPageSize(...); device.setPageLayout(layout);`) rather than the
  bare `setPageSize()` shorthand. A genuine physical-printer job from
  `on_printBtn_clicked()` (still `QPrinter::NativeFormat`) is untouched and
  still goes through `QPrinter` as before -- whether *that* specific case's
  `QPrintDialog` "Properties" widget correctly shows Letter (it was
  originally seen defaulting to A4 there too, screenshots 2026-08-08) is
  still unverified against real printer hardware, separately from the now-
  fixed PDF-file case. **Verified 2026-08-11** via `pdfinfo` (reads the
  file's actual `MediaBox`, not a viewer's guess) on real exported files:
  confirmed `612 x 792 pts (letter)`. Note: qpdfview (a PDF viewer,
  unrelated to this app) displays every PDF checked as A4 regardless of its
  actual size, confirmed by it mislabeling an unrelated third-party PDF
  (a RigExpert manual made by Acrobat Distiller) the same way -- that's a
  qpdfview bug/quirk, not this app's output; use `pdfinfo` or a different
  viewer to actually check a PDF's page size.
- **`Screenshot::savePDF()`'s device-screenshot image placement was
  off-center or flush against the page edges -- fixed 2026-08-11.** Two
  separate device-size branches, both wrong in different ways: the small/
  square-LCD branch (e.g. the RigExpert Match/MATCH U used for day-to-day
  testing here, 480x480) drew the image at a fixed offset
  (`m_lcdWidth*0.7`) that happened to sit flush against the right page
  edge with a large empty gap on the left; now centered horizontally using
  the image's actual current width, so it stays correct if the image is
  scaled before drawing here in the future. The large-landscape-LCD branch
  (`AA-2000 ZOOM`/`AA-3000 ZOOM`/`AA-1500 ZOOM SE`, 746x480 declared in
  `AnalyzerParameters::fill()`) stretched the image to fill the entire page
  width with 0 margin on any side; now scaled to fit within a 50px margin
  on all sides, preserving aspect ratio. The small-LCD fix was confirmed
  against a real exported PDF (`pdfinfo`/rendered preview); the large-LCD
  fix was confirmed only via a standalone repro using the declared 746x480
  geometry (offscreen `QPdfWriter` + rendered preview) -- none of those
  three models are available to test against here, so this needs
  re-checking against real hardware if one becomes available.
- **Some of Qt's own built-in dialog strings stay in English even though
  `qtbase_<code>.qm` is loaded and working.** Confirmed (a headless
  `QT_QPA_PLATFORM=offscreen` probe against the real `qtbase_es.qm`,
  2026-08-09) that this is a mismatch inside Qt's own shipped translation,
  not this app's loading of it: the file dialog's actual live strings are
  `"File &name:"` / `"Files of &type:"` / `"&Look in:"`, but
  `qtbase_es.qm` only has translations keyed to `"File &name:"` (matches,
  hence *that* one does show translated) and the accelerator-less
  `"Files of type:"` / `"Look in:"` (don't match, so those two silently
  fall back to English). `QTranslator` lookups are exact-string, mnemonic
  ampersand included -- whoever last updated Qt's own `.ts` for this
  translated an older/different source string. Not `es`-specific in
  principle; any language where Qt's own catalog has drifted from the
  current `qfiledialog.ui` mnemonics would show the same gap.
  **Both `"Files of &type:"` (2026-08-24) and `"&Look in:"` (2026-09-01)
  are now fixed the same way:** rather than waiting on upstream Qt,
  `locales/qtbase_override_<code>.ts` (`es`/`ja`/`uk`, two messages each
  now) supplies just those two corrected mnemonic strings, re-using each
  language's own existing (correct) wording for the rest -- compiled and
  staged alongside the regular `qtbase_<code>.qm` (see
  `ANTSCOPE_QTBASE_OVERRIDE_*` in `CMakeLists.txt`), loaded after it so
  it wins on the messages it covers without needing a full retranslate of
  Qt's own catalog. Confirmed working (not just file-inspected) by
  loading both translators the same order/way the app does and checking
  `qApp->translate("QFileDialog", "&Look in:")` resolves correctly for
  all three languages. `"&Look in:"`'s translation had no letter shared
  with English "Look" for `es`/`uk` (unlike `"type"`'s lucky match on
  "t"), so those two use the first letter of the translated word's
  operative term instead -- a defensible but not Qt-upstream-verified
  choice of accelerator letter, same caveat as any translation this
  project supplies itself rather than sourcing from Qt. If a similar gap
  turns up in some *other* qtbase string later, this is the pattern to
  repeat: confirm the real live string via `strings` against
  `libQt6Widgets.so.6` (or wherever it actually lives), then add one
  `<message>` per affected language here.
- **The S21 tab is import-only -- there's still no live S21/S12 capture
  from real hardware.** As of the 2-port `.s2p` import work, the tab is
  no longer unconditionally hidden: `MainWindow::on_importFinished()`
  shows it as soon as an import populates `dataSParam`
  (`mainwindow_measurements_io.cpp`), and it's fully documented as a
  user-facing feature in `docs/user-guide.md`. What's described below is
  a *separate*, still-unfinished path: measuring S21 live from a
  connected analyzer, rather than reading it from a file.
  - A real protocol command exists for it: `BaseAnalyzer::startMeasure()`
    sends `"FDB<dots>"` instead of the normal `"FRX"`/`"EFRX"` scan
    command when S21 mode is active (`baseanalyzer.cpp`).
  - The full signal chain is wired end-to-end:
    `AnalyzerPro::on_measureS21()` -> `BaseAnalyzer::setIsS21Mode(true)`
    + `startMeasure()` -> device reply parsed -> `newS21Data` signal ->
    `Measurements::on_newS21Data()` -> plotted on `m_s21Widget`.
    `Markers` already fully supports S21 (its own line/label objects,
    its own branch in `redraw()`). `MainWindow`'s own scan-start
    handler already branches correctly on the active tab: `if
    (currentTab == "tab_s21") emit measureS21(...)`.
  - What's actually missing: **no capability gating exists anywhere.**
    `AnalyzerParameters` (the model table, `analyzer/analyzerparameters.h`)
    has no "supports S21 / two-port" flag, and nothing checks the
    connected device's model before offering live S21 capture -- there's
    no UI path to it at all right now (the tab's visibility is driven
    solely by imported data, per above), which reads as a placeholder
    for a detection step that was never built. Also, **response parsing
    is HID-only**: the `WAIT_S21_DATA` parser state that actually
    extracts an S21 value from the device's reply is implemented only in
    `hid_analyzer.cpp`; `com_analyzer.cpp` (serial) and `ble_analyzer.cpp`
    (Bluetooth) have no S21-handling code at all, even though the
    command-sending logic they'd inherit from `BaseAnalyzer` is
    transport-agnostic.
  - To actually finish this: add a per-model (or runtime-detected)
    two-port capability flag, a UI path to trigger live capture on the
    S21 tab when connected to a capable device over HID, and add
    `WAIT_S21_DATA` handling to `com_analyzer.cpp`/`ble_analyzer.cpp` if
    S21 should also work over serial/BLE. See also the
    `nanovna-two-port-work-deferred` note -- this is folded into the same
    future pass, blocked on owning real 2-port-capable hardware to test
    against.
  - **Confirmed rejected on this session's test hardware:** `FDB10\r`
    against a RigExpert Match RFE (firmware `FT810`, per its own
    `CDIC`/`SN` reply) gets `Error.Not recognized` back over HID
    (2026-08-19). Not proof it's rejected on every model -- just the only
    data point that exists right now. See the `EFRX` finding below: the
    same device rejects that command too, and this codebase has no record
    anywhere of which RigExpert model/firmware, if any, actually accepts
    either one.
- **Developer mode (`-developer` flag, `g_developerMode`) -- inert from
  2026-08-10, removed entirely 2026-09-07.** `main.cpp` stopped setting
  `g_developerMode` on 2026-08-10 even when `-developer` was passed; as of
  2026-08-20 it gated exactly two things: `CustomAnalyzer::load(m_settings)`
  (startup preset loading, `mainwindow.cpp`) and what a since-removed
  comment called an "abandoned UDP remote-control bridge" in
  `onefqwidget.cpp` -- already gone from that file by then, actually
  (removed by commit `d27827e`, "Remove OneFqWidget's dead UDP
  remote-control stub, superseded by json-tcp-api" -- see the
  `remoteapi/`-module note further down; this paragraph's older wording
  didn't know that yet). By 2026-09-07, `CustomAnalyzer::load()` had been
  unhooked from the flag too (Custom Analyzer is meant to be a live,
  user-facing feature, not something needing `-developer`), leaving
  `g_developerMode` with no live runtime call site anywhere -- so the flag,
  the global, every `extern bool g_developerMode;` declaration (15 files),
  and the `-developer`-gated `-comserial`/`-usbhid`/`-nanovna`/`-ble` CLI
  shortcut for pre-enabling Debug Logging (never the only way to reach
  it -- Settings > Developer's four checkboxes always worked regardless)
  were all deleted outright that day rather than left inert. It did
  **not** gate a points ceiling -- that claim was true of a since-replaced
  mechanism (`MAX_DOTS`/`spinBoxPoints`, neither of which exist in the code
  anymore) and was stale as of the scan-stitching rework: the actual
  ceiling today is `g_pointsMax`/`g_pointsWarnThreshold`/
  `g_analyzerMaxPoints` (Settings > General), independently user-editable,
  bounded 50-`POINTS_MAX` (`mainwindow.h`, currently 10000), with no
  `g_developerMode` check ever in that path.
  **As of 2026-08-13, it no longer gates Settings' Custom Analyzer group
  box** (renamed from "Customize"; as of 2026-08-14 it lives inside the
  renamed "Developer" tab, alongside the unrelated "Debug Logging" group
  box added the same day -- see `CHANGELOG.md`). **As of 2026-09-07 the
  group box is no longer force-disabled either** -- "Use customized
  analyzer" moved out to its own item directly above the group box and
  now genuinely enables/disables it (`Settings::on_enableCustomizeControls()`
  cascades via `groupBoxCustomAnalyzer->setEnabled()`), and
  `Settings::initCustomizeTab()` seeds both the checkbox and the group
  box's enabled state from the real saved `CustomAnalyzer::customized()`
  value on open, instead of always forcing both off. The two STILL BROKEN
  items below are unfixed as of this date, so turning the feature on for
  real use still runs into them.
  - **What it's for:** define a named preset that overrides a real,
    already-detected model's min/max frequency and LCD width/height --
    aimed at a clone or updated-range unit that AntScopeZ already
    identifies correctly (via the device's own reported version string)
    but whose real frequency range differs from what AntScopeZ assumes for
    that model. Picking a "prototype" only seeds sensible defaults; it
    never changes which protocol/commands are used to talk to the device.
  - **Architecture:** `CustomAnalyzer` (`analyzer/customanalyzer.h/.cpp`)
    holds the persisted presets (`m_map` of alias -> preset,
    `m_currentAlias`, `m_useCustomized`) and saves/loads them under the
    `AntScopeZ.ini` `[CustomAnalyzers]` group. `Settings::initCustomizeTab()`
    (`settings.cpp`) wires up the Customize tab UI. Three
    `AnalyzerPro` methods -- `getModelString()`, `getMinFq()`, `getMaxFq()`
    (`analyzerpro.cpp`) -- are the actual integration points: each checks
    `CustomAnalyzer::customized()` and substitutes the custom value in place
    of the real device's `AnalyzerParameters` entry (the fixed table of
    ~35 real RigExpert models, `analyzer/analyzerparameters.h`).
  - **FIXED:** `AnalyzerPro::slotFullInfo()` (`analyzerpro.cpp`) null-derefed
    `AnalyzerParameters::byName(getModelString())` -- `getModelString()`
    returns `CustomAnalyzer::currentPrototype()` while customized, which is
    never a real model name (defaults to the literal placeholder
    `"Custom"`), so `byName()` reliably returned `nullptr` and the very next
    line crashed on it (confirmed via `coredumpctl`: SIGSEGV, null `this` +
    member offset, on a RigExpert Match RFE reporting its license level
    mid-scan). Now reads `AnalyzerParameters::current()` instead -- the
    real, physically connected device, already resolved by serial-number
    prefix at connection time
    (`SelectDeviceDialog::onApply()` -> `AnalyzerParameters::setCurrent()`).
    License-level bookkeeping describes the real hardware, not whatever
    override is configured for display/range purposes.
  - **FIXED:** `Settings::initCustomizeTab()` unconditionally `.hide()`'d
    `comboBoxPrototype` and its label, despite correctly populating it with
    every `AnalyzerParameters` model name right below -- so the one control
    that lets you pick a valid reference model literally couldn't be used.
    Unhidden; confirmed it populates.
  - **FIXED 2026-09-07:** `AnalyzerPro::getMinFq()`/`getMaxFq()`
    (`analyzerpro.cpp`) returned `CustomAnalyzer::currentPrototype()` while
    customized -- a copy-paste from `getModelString()` right above, which
    correctly wants a model-name string; these two want a frequency
    instead. Never actually called from anywhere (`AnalyzerPro`'s own
    `getMinFq()`/`getMaxFq()` have no live callers -- everything else on
    this list reimplements the same `CustomAnalyzer::customized() ? ... :
    AnalyzerParameters::getMinFq()/getMaxFq()` check inline at its own call
    site instead), so harmless in practice, but wrong: any future caller
    doing `.toULongLong()` on a non-numeric string like `"AA-230 ZOOM"`
    silently gets `0`. Now returns `CustomAnalyzer::getCurrent()->minFq()/
    maxFq()`, falling back to `AnalyzerParameters::getMinFq()/getMaxFq()`
    if there's no current alias.
  - **FIXED 2026-09-07:** `Settings::on_addButton()` ("New") set the
    (no longer hidden) `comboBoxPrototype`'s current text to the literal
    string `"names[0]"` -- dead placeholder, never actually indexed into a
    real list. Now `setCurrentIndex(0)`, selecting the combo's actual first
    entry (populated in `initCustomizeTab()`).
  - **STILL BROKEN:** the custom min/max frequency override doesn't survive
    a scan even with a valid prototype picked. `AnalyzerParameters::
    normalizeFq()`/`normalizeFqRange()` (`analyzerparameters.h`)
    unconditionally clamp to `AnalyzerParameters::current()`'s real stock
    range and have no concept of `CustomAnalyzer` at all;
    `MainWindow::on_dataChanged()` calls `normalizeFqRange()` on every range
    change, so both "Full Range" and a typed Stop value get silently
    clamped straight back down to the real device's limit. Called from
    roughly 8 sites total in `mainwindow.cpp`, not just the one -- fixing
    this means making those two static methods customization-aware (same
    `CustomAnalyzer::customized() ? ... : ...` pattern the individual
    `mainwindow_scan.cpp`/`mainwindow_frequency.cpp` call sites already use
    inline), not patching call sites individually.
  - **STILL BROKEN, not diagnosed:** running an actual scan against a real
    device (RigExpert Match RFE) with "Use customized analyzer" checked
    gets the outgoing command rejected at the protocol level --
    `HidAnalyzer::sendData()` logs
    `***** ERROR:  "Error.Not recognized"` in response to sending
    `07046f66660d0000...` (zero-padded to the fixed HID report size).
    Root cause not chased this pass -- worth checking whether the command
    encodes a frequency value that becomes malformed once it's built from a
    custom range instead of a real model's, but that's a guess, not a
    finding.
  - **Screenshot width/height (investigated 2026-09-07): client-side only,
    by design -- not a bug, but a hard limit worth knowing.**
    `MainWindow::on_actionScreenshotAA_triggered()` correctly substitutes a
    custom profile's width/height for the real model's when customized,
    and `Screenshot` (`screenshot.cpp`) consistently uses whatever it's
    given throughout buffer allocation, pixel decoding, and preview
    scaling -- no dimension bugs found there. But
    `HidAnalyzer::makeScreenshot()`/`ComAnalyzer::makeScreenshot()`
    (`analyzer/hid_analyzer.cpp`, `analyzer/com_analyzer.cpp`) send the
    bare `screenshot\r` command with no width/height encoded in it at all
    -- the real device streams its own native, fixed-resolution pixel data
    regardless of what's configured here. A custom profile's width/height
    only decode correctly if they're set to match a real connected
    device's actual native screen resolution; there's no protocol-level
    way to make the device produce a different one, so this is really the
    same class of problem as the "Error.Not recognized" scan rejection
    above, not something fixable purely in `Screenshot`.
  - Reported, not yet diagnosed: the Customize tab's controls looking "not
    laid out cleanly" at runtime -- no screenshot yet to compare against
    the `.ui` markup, which looks like a structurally normal form layout
    on its own.
  - Separate feature living on the same tab, not part of Custom Analyzer
    itself: the six "Auto-calibration" length/resistance fields
    (`cable_length_min/max/steps`, `cable_res_min/max/steps`) feed
    `Measurements::autoCalibrate()` (`measurements.cpp`), a brute-force
    grid search for the best-fit cable length + characteristic resistance
    against a reference measurement, whose result gets pushed to the
    analyzer firmware as a `calrl<R>,<L>` calibration command
    (`MainWindow::autoCalibrate()`, `mainwindow.cpp`). `measurements.h`'s
    own comment says what it's for: `// 0-NONE, 1-R,L(old AA-1400),
    2-C,L(new AA-230 ZOOM)` -- a legacy calibration routine for the AA-1400
    specifically (only the R,L path is actually implemented). Only reachable
    via the Ctrl+Alt+Shift+M shortcut
    (`MainWindow::on_presssCtrlAltShiftM()`), gated behind the
    `CALIBRATION_DEBUG_TOOLS` compile-time constant as of 2026-08-20 (see
    "Compile-time feature gates" below) rather than `g_developerMode`; both
    real trigger sites (`on_measurementComplete()`,
    `on_measurementCompleteNano()`) have an `#if 0`-disabled older variant
    sitting right next to the live one, suggesting this was left
    mid-refactor.
  - **User Defined tab (live `EFRX` capture) -- separate feature from
    Custom Analyzer, gated independently since 2026-08-20 (see
    "Compile-time feature gates" below).** Investigated 2026-08-20 while
    `g_developerMode` was flipped on locally for inspection. The plotting
    pipeline itself is real and fully wired end-to-end: scanning while
    this tab is active sends `EFRX<dots>` instead of `FRX`
    (`BaseAnalyzer::startMeasure()`), the device's reply is a `#field,
    field,...` header line followed by `freq,r,x,value,value,...` rows,
    parsed identically on both HID (`hid_analyzer.cpp`) and serial
    (`com_analyzer.cpp`) -- unlike S21, not HID-only -- and
    `Measurements::on_newUserDataHeader()`/`on_newUserData()`
    (`measurements.cpp`) auto-creates one colored, legended `QCPGraph` per
    field and plots it live. A fixed legend-setup ordering bug found and
    fixed the same session (`mainwindow.cpp`'s `setWidgetsSettings()` had
    `setAutoAddPlottableToLegend(false)` before `addGraph()` instead of
    after, unlike every sibling widget) was the only code defect turned up
    -- the mechanism itself works as designed.
    - **No field-selection or scale UI exists.** Whatever named fields the
      device's `EFRX` reply happens to report get auto-plotted, unscaled
      -- there's no way to pick, hide, rename, or rescale individual
      fields from the app side. "User defined" describes the *device's*
      arbitrary-field protocol capability, not a user-facing
      customization panel.
    - **Confirmed rejected on this session's test hardware:** the same
      RigExpert Match RFE (firmware `FT810`) that rejects `FDB` also
      rejects `EFRX10\r` outright -- `Error.Not recognized` over HID
      (2026-08-20).
    - **No commentary anywhere in this codebase, or in the user guide's
      Supported Devices section, identifies which RigExpert model or
      firmware, if any, actually accepts `EFRX` or `FDB`.**
      `AnalyzerParameters` (`analyzer/analyzerparameters.h`) has no
      per-model capability flags for either command -- nothing gates
      which devices offer these features because nothing in this
      codebase distinguishes them. Both may be vestigial from AntScope2's
      original codebase (aimed at manufacturer/debug firmware never
      present in retail units) rather than a documented feature of any
      currently-shipping analyzer -- unconfirmed either way, and not
      discoverable from source alone.
  - **"One Fq" mode (Start==Stop or Range==0, then Single/Continuous) --
    three real bugs found and fixed 2026-08-20, confirmed working
    end-to-end afterward (floating live readout, updating every few
    seconds, clean Esc-to-stop) against a freshly power-cycled Match RFE.**
    Not itself gated by `g_developerMode` -- reachable in the shipped
    build today via that Start/Stop trick, just undocumented (see
    `docs/user-guide.md` gap). Only the two-way UDP bridge layered on top
    of it is developer-only; see the separate bullet below.
    - `onefqwidget.cpp`'s constructor built its `QLabel` stylesheet by
      concatenating a hex color directly against `"margin-top: 6px;..."`
      with no separating `;` -- e.g. `#ffffffmargin-top`, an invalid CSS
      token, logged as `QCssParser::parseHexColor: Unknown color name`
      every time the widget opened. Fixed (`onefqwidget.cpp`).
    - `AnalyzerPro::on_measureOneFq()` (`analyzerpro.cpp`) hardcoded
      `m_dotsNumber = 100000` and discarded its own `dotsNumber` parameter
      (commented out as unused), sending `FRX100000\r` -- 10x past this
      app's own `POINTS_MAX` (10000) ceiling anywhere else. Root cause of
      an `Error.Not recognized` from the Match RFE in this session's
      testing. Fixed to use the caller's real value (the normal
      Points/Speed-Accuracy slider, 10-1000) instead.
    - **Hard crash, `SIGABRT`, confirmed via `coredumpctl`/`gdb` backtrace
      (not guessed):** `Measurements::updateOneFqWidget()`
      (`measurements_onefq.cpp`) ran `_data.ptX`/`_data.ptY` -- already
      Smith-chart *plot* coordinates (`Measurements::NormRXtoSmithPoint()`,
      range roughly ±6) -- through `QCPAxis::pixelToCoord()`, which treats
      its argument as a *pixel* position. Backwards regardless of outcome;
      confirmed via the core dump to occasionally divide by an axis rect
      height of 0 (a transient layout state right as this floating widget
      first opens), producing `inf` -> `NaN` once
      `QCPItemEllipse::draw()`'s `QPointF::toPoint()` tried to round it,
      aborting on Qt's own `qSaturateRound()` assert. Fixed by removing
      the erroneous `pixelToCoord()` calls entirely -- `setCoords()`
      already wants plot coordinates, exactly what `_data.ptX`/`ptY`
      already were. `NormRXtoSmithPoint()` itself also hardened
      (`measurements_redraw.cpp`) to guard NaN/Inf inputs and a
      zero-denominator degenerate case, as defense in depth -- not the
      confirmed crash site here, but a real, separate latent gap (the
      calibrated path a few lines away in `measurements.cpp` already
      guarded the equivalent values, this shared helper never did).
    - Along the way: the Match RFE spent part of this session's testing
      stuck in a state where it autonomously streamed
      `"144.000000,15.XX,-38.XX\r\n"`-style telemetry unprompted --
      interleaved into unrelated replies (even `VER`/`FULLINFO` right
      after connecting), rejected new measurement commands with
      `Error.Measurer busy...`, and didn't visibly stop on `off\r`. A full
      power cycle cleared it. Not a code bug -- flagged here since it
      produced confusing symptoms (including a false lead on the crash
      above) that looked code-related until the device state was ruled
      out.
  - **UDP remote-control bridge (`OneFqWidget`) -- removed, not just
    disabled.** Investigated 2026-08-20: two plain-text UDP sockets
    (ports 6050/6051, `AA1`/`AA2`/`SETFQ` text protocol), gated behind
    `g_developerMode`, present since this repo's very first commit
    (`eb1cce9`) and inherited from AntScope2. Found not viable as-is (the
    receive socket's lifetime was backwards for real remote control -- it
    only existed while a One Fq session a human had already started was
    open -- plus a likely unit-mismatch bug on the reply side, never
    exercised end-to-end to confirm). Decided 2026-08-20 not worth fixing
    in place; deleted outright by commit `d27827e` ("Remove OneFqWidget's
    dead UDP remote-control stub, superseded by json-tcp-api") rather than
    left dormant -- `onefqwidget.cpp`/`.h` have no socket code left at all.
    See the `remote-network-control-idea` memory note for the direction
    actually worth pursuing if remote control comes back up; the
    `remoteapi/` module (TCP JSON API, `json-tcp-api` branch) is the
    real, current remote-control mechanism.

- **Compile-time feature gates -- `USER_DEFINED_FEATURE` and
  `CALIBRATION_DEBUG_TOOLS` (both `CMakeLists.txt`, default `0`).** Added
  2026-08-20 while auditing every `g_developerMode` use case-by-case
  (~40 sites across a dozen files) and deciding, for each, whether it
  actually belonged on a *runtime* flag at all. `-developer` is
  documented in this file, and until today, in `docs/user-guide.md` too; a
  `.deb` user can read about it and pass it. That's an acceptable
  exposure for something meant to eventually ship live (Custom Analyzer),
  and not acceptable for things that either need real hardware nobody has
  yet, or send commands that alter a connected device's own internal
  calibration state. Compile-time gating means the code doesn't exist in
  a normal build at all -- reaching it requires editing a source line and
  rebuilding, not just knowing a documented flag.

  - **`USER_DEFINED_FEATURE`** -- the User Defined tab and everything
    that supports it (~32 sites: tab visibility, Multi-view join/restore
    exclusion, every `m_userWidget` axis-sync duplicate across zoom/pan/
    preset/band code, the widget setup/connect block, print pen-width
    handling, measurement-delete cleanup, plus the dead `.csv`
    User-Data-reload shortcut in `measurements_io.cpp`, left
    uncommented-and-wrapped exactly as it was rather than fixed -- its
    extra `g_developerMode` inner gate was dropped 2026-09-07 along with
    that flag, so it's reachable on `USER_DEFINED_FEATURE` alone now,
    still unconfirmed-working either way). "Come back to this if/when
    `EFRX`-capable hardware turns up" -- see the
    `s21-and-user-defined-live-capture-deferred` memory note.
  - **`CALIBRATION_DEBUG_TOOLS`** -- both Ctrl+Alt+Shift+M/N shortcuts
    (`mainwindow_scan.cpp`) and the `WAIT_CALFIVEKOHM`/
    `WAIT_CALFIVEKOHM_START` response parsing they depend on
    (`analyzer/hid_analyzer.cpp`). Never meant to be end-user-reachable at
    all, regardless of what happened to `g_developerMode` (removed
    entirely 2026-09-07, see above) -- see `CMakeLists.txt`'s own warning
    comment for the full reasoning (short version: these alter the
    device's *own* internal calibration, not this app's software-side OSL
    data, using undocumented commands RigExpert has never published
    anywhere this project has access to).

  Three smaller things audited the same pass turned out not to need a
  flag *at all*, runtime or compile-time, and were ungated outright:
  - **"Don't restrict frequency"** (Settings > Developer > Custom
    Analyzer, directly under "Use customized analyzer") -- relocated
    there from Settings > General purely for placement (it's a
    developer-facing setting, living on the Developer tab is gate enough
    on its own), with no flag dependency left at all -- `m_fqRestrict`
    always reads/writes the real saved value now. Fixed a real,
    previously-invisible bug along the way: `Settings::initCustomizeTab()`
    displayed the checkbox with inverted polarity on first open (checked
    == restricting, when checked is supposed to mean "don't restrict") --
    masked for years by the old `!g_developerMode -> setChecked(true)`
    fallback always overriding it. One click self-corrected it (the click
    handler's polarity was always right), but the initial display was
    backwards; fixed to match.
  - **"Allow extended chart zoom"** (Settings > General, new
    `g_extendedChartZoom`, default off) -- six Y-axis zoom presets on the
    SWR/Rs/Rp/RL charts (mouse wheel in `mainwindow_mouse.cpp`, keyboard
    in `mainwindow_shortcuts.cpp`) that used to unlock past their normal
    floor/ceiling only under `g_developerMode`. Purely a "let me zoom
    further" preference with no safety angle, so it got a real persisted
    Settings toggle instead of either gate.
  - **Ctrl+0 on the SWR tab** (`mainwindow_shortcuts.cpp`,
    `on_pressCtrlZero()`) -- was the *only* tab whose Ctrl+0 Y-axis reset
    was gated at all (Rs/Rp/RL's equivalent resets never were), and had
    no `else` branch, so without the flag it silently did nothing. No
    device commands involved, just a chart zoom reset -- ungated to match
    every other tab.

  Also found and fixed in passing, unrelated to any of the above:
  `mainwindow_shortcuts.cpp`'s `tab_rl` keyboard-zoom handler was missing
  its `m_rpWidget` sync entirely, and had two lines referencing
  `m_userWidget->xAxis->range()` where every other tab's equivalent block
  references its own just-changed widget -- copy-paste error, found while
  converting the `USER_DEFINED_FEATURE` gate sitting right next to it,
  fixed to reference `m_rlWidget` and add the missing `m_rpWidget` sync.

  `docs/user-guide.md` deliberately does **not** mention
  `USER_DEFINED_FEATURE` or `CALIBRATION_DEBUG_TOOLS` (or anything gated
  by them) anywhere -- it's user-facing documentation for a shipped
  `.deb`, and nothing behind either constant is reachable from one. This
  file is the right place for that detail; the user guide only documents
  what a normal install can actually do.
