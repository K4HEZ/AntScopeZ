# Changelog

All notable changes to this project are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the version number
below should track `project(VERSION ...)` in `CMakeLists.txt`.

## [Unreleased]

### Changed

- Renamed the project from AntScope2 to **AntScopeZ**, to stop sharing an
  identity/namespace with RigExpert's own AntScope2 -- this fork is not a
  RigExpert-published build. `CMakeLists.txt`'s `project()`/target name,
  the built executable, `ANTSCOPE2VER` -> `ANTSCOPEZ_VER`, window titles,
  the `.desktop` file (`antscope2.desktop` -> `antscopez.desktop`,
  `Name=`/`Exec=`/`Icon=` updated), app icons
  (`AntScope2.ico`/`.icns`/`.png` -> `AntScopeZ.*`), and user-facing strings
  (file-dialog filters, the Windows `.asd` file-association registration,
  the Touchstone-export header comment) all follow.
  `QCoreApplication::setOrganizationName("hz23116")` is gone entirely (was
  already deliberately not "RigExpert" -- see below) rather than becoming
  `"AntScopeZ"`, so `QStandardPaths::AppConfigLocation` no longer adds an
  extra directory level: settings/calibration move from
  `~/.config/hz23116/AntScope2/AntScope2.ini` to `~/.config/AntScopeZ/AntScopeZ.ini`.
  Also dropped "RigExpert" from the Mac/Windows/Raspbian data-path literals
  (`RigExpert/AntScope2/`), which was a direct use of the vendor's company
  name in our own path -- worse than the Linux org-name issue this
  supersedes. `Settings::setIniFile()`'s one-time migration logic now
  checks both the pre-2.1.4 "next to the binary" layout and the 2.1.4
  `hz23116/AntScope2` layout and folds either into the new location, so
  existing local settings/calibration data isn't lost.
  `AnalyzerPro::on_checkUpdatesBtn_clicked()`'s `app=antscope2` query
  parameter to `rigexpert.com` is deliberately left alone -- that identifies
  us to the *vendor's* API, which has no reason to know this fork's name.
  GitHub repo/username rename is a separate, manual step outside this
  commit's scope.

### Added

- `docs/user-guide.md`: new "Customized analyzer parameters" section
  documenting the hidden Settings → Customize tab (`-developer` flag),
  how the "prototype" selector relates to protocol vs. just default
  range/LCD values, how to use it for a recognized clone/updated-range
  analyzer, and that the saved preset only takes effect while launched
  with `-developer` (it's inert, not silently applied, on a normal
  launch).

### Added

- A real Light/Dark theme system. `style.h`/`style.cpp` now build every
  `Style::*()` stylesheet from a small `Theme` struct (named color roles)
  instead of hardcoded `rgb(...)`/hex literals, with `Dark` reproducing the
  app's original colors and a new hand-picked `Light` theme alongside it.
  Settings → General → Color theme gets a functional `Theme:` combo box
  (Light/Dark) in place of two radio buttons that were force-hidden right
  after being created. Switching it re-skins the open Settings dialog and
  the main window immediately, and persists via the existing
  `darkColorTheme` ini setting. `Theme` only covers the canvas (window
  background, text, muted text, borders) now; buttons, menus, the color
  dialog, and message-box buttons dropped the hand-picked navy skin they
  carried over both themes and render natively off `Style::palette()`
  instead, so selection highlighting and control chrome come from Fusion
  rather than an app-baked color.

### Changed

- Settings → General: the Metric/Imperial measurement system is now a single
  `measureSystemComboBox` combo box (matching the Language selector) instead
  of a pair of checkboxes that hand-rolled their own mutual-exclusivity logic
  (`measureSystemMetric`/`measureSystemAmerican`). Behavior and the persisted
  `measureSystemMetric` setting are unchanged.
- Removed the large RigExpert logo from the main window's right-hand control
  panel and let the group boxes below it use the freed space; the image
  asset is untouched for a possible future splash panel.

### Fixed

- Settings, the Connect Analyzer dialog, and the main window's central
  widget each had a custom `paintEvent()` that unconditionally painted a
  black-to-cyan radial gradient over themselves, regardless of theme --
  among other things, washing out the USB/COM/BLE label text in Connect
  Analyzer. All three now use the active theme's plain window background.
- `MainWindow::changeColorTheme()` hardcoded `_dark = true` as its first
  line, discarding whatever theme was actually requested -- Light mode was
  unreachable no matter what was selected or saved.
- `MainWindow::setStyles()` called `qApp->setStyleSheet()` four times in a
  row; since each call replaces rather than adds to the app-wide
  stylesheet, only the last one ever took effect, silently dropping the
  intended label/lineEdit/groupBox rules (and, as a side effect, wiping out
  the `QDialog{}` background rule `main.cpp` sets up before any dialog
  exists to reassert it locally). Combined into a single call.
- The Settings dialog's Theme combo fired its own change handler during
  construction (`setCurrentIndex()` before the constructor's outer
  `beginGroup("Settings")` had closed), corrupting the ini with a spurious
  nested `Settings\darkColorTheme` key on every open.
- 18 `QMetaObject::connectSlotsByName: No matching signal for ...` warnings
  at startup (issue #12) -- `MainWindow`'s `on_mouseWheel_*`/`on_mouseMove_*`/
  `on_replotY_swr`/`on_measurementsClearBtn_clicked` slots were all already
  wired up with explicit `connect()` calls, but their `on_<name>` naming
  made Qt's auto-connect scan also try (and fail) to match them at startup.
  Renamed all 18 to drop the `on_` prefix so the scan skips them; the
  explicit `connect()` calls are unchanged.
- `MainWindow::on_singleStart_clicked()`'s "stop an in-progress scan" branch
  set `singleStart` back to *checked* (green/busy) instead of unchecked
  (issue #7) -- clicking Single to interrupt a running scan left the button
  looking like a scan was still running. The sibling `continuousStartBtn`
  handler already did this correctly; `singleStart` now matches.
- `Export`, `FqSettings`, and the "Chart background" color-picker dialog
  each carried their own hardcoded dark background/colors (e.g.
  `#2F2F2F`) completely independent of the active theme. All three now use
  `Style::dialog()` / a new `Style::colorDialog()`.
- The "show graph hint" / "show brief params under cursor" / "show markers
  hint" checkboxes were persisted independently in three places (`Settings`,
  `Measurements`, `Markers`), each with its own `QSettings` group and key
  casing (issue #28). `Settings` no longer keeps its own copy; it displays
  whatever `Measurements`/`Markers` -- the classes that actually consult
  these flags -- report via `getGraphHintEnabled()`/
  `getGraphBriefHintEnabled()`/`getMarkersHintEnabled()` when the dialog
  opens, same as it already did for the measurement-system and color-theme
  settings.
- "Brief params under cursor" not tracking the mouse on plot tabs other than
  the one that was active while a scan was running (issue #33).
  `Measurements::replot()` only ever replots `m_currentTab` while a scan
  streams in, so every other tab's chart kept a stale pixel-to-frequency
  mapping until something replotted it -- moving the mouse there computed
  garbage (or nothing) until the user happened to trigger that tab's own
  replot. `MainWindow::on_measurementComplete()`/`on_measurementCompleteNano()`
  now replot every plot tab once when a scan finishes.
- The Smith chart's resistance/reactance arcs and number labels, and the
  TDR/S21 tabs' right-hand (`yAxis2`) scale, stayed hardcoded black instead
  of following the color theme -- unreadable (arcs/labels literally
  invisible against a black chart background). `drawSmithImage()` only
  ever painted those once in black, and `MainWindow::setChartBackground()`
  recolored `xAxis`/`yAxis` on every plot but never touched `yAxis2` or the
  Smith chart's curves/labels. Added `Measurements::setSmithForegroundColor()`
  and `setChartBackground()` now calls it (and also recolors `yAxis2`)
  alongside the existing background handling. `setSmithForegroundColor()`
  tracks the arcs/labels via new `m_smithArcs`/`m_smithLabels` members
  populated by `drawSmithImage()`, rather than
  `m_smithWidget->plottableCount()`/`itemCount()` -- those curves/items are
  never registered via `QCustomPlot::addPlottable()`/`addItem()` (they
  render anyway; a `QCPLayerable` adds itself to its layer independently of
  that list), so the first attempt at this fix silently recolored nothing.

### Removed

- `AntScope2.ini` is no longer tracked in the repo — it's a runtime settings
  file the app (re)creates on first launch, and the committed copy had
  accumulated one developer's machine-specific paths and window geometry.
- The bundled sample OSL calibration files (`Calibration/cal_load.s1p`,
  `cal_open.s1p`, `cal_short.s1p`) are no longer distributed. Calibration is
  analyzer-specific, so each user needs to run their own; the app recreates
  the `Calibration/` directory on demand. Both `AntScope2.ini` and
  `Calibration/` are now gitignored.
- The `ANTSCOPE_NEW_CONNECTION` and `ANTSCOPE_NEW_ANALYZER` CMake options,
  and the OFF-path dead code they guarded (which referred to arrays/types
  that no longer exist and hadn't compiled in a long time). Also deleted
  `analyzer/analyzer.cpp`/`.h`, the pre-`AnalyzerPro` analyzer class they
  belonged to -- unused by the build and, where still `#include`d, unused by
  the includer. `ANTSCOPE_DEBUG_BLE` is gone too; its raw BLE TX/RX
  `qDebug()` calls in `analyzer/ble_analyzer.cpp` are now plain commented-out
  code instead of a build option, to uncomment locally when needed.
- The `ANTSCOPE_OLD_TDR` CMake option and the OFF-path dead code it guarded
  (declared a `startTDRProgress(Analyzer*, ...)` overload against a type
  that doesn't exist, and referenced `TDRProgressDialog`/`ProgressDlg` in
  ways that didn't compile once the option was actually turned off). Also
  deleted `tdrprogressdialog.cpp`/`.h`/`.ui`, which only that dead path used.
  TDR progress now always uses the `ProgressDlg`-based flow the option had
  been forcing on anyway.
- The AntScope2 software self-update checker (`Updater`, `updater.cpp`/`.h`)
  and its dead-but-present auto-launch-a-downloaded-.exe code path. This
  build isn't vendor-distributed, so it shouldn't silently phone home to
  `rigexpert.com` on every launch and every analyzer connect to check for
  a newer AntScope2 build.
- The scrolling "marquee" ad/news banner's remote fetch
  (`MarqueeLabel::request()`), which hit `rigexpert.com/get.php?part=asscroll`
  on every analyzer connect and rendered whatever it returned in-app.
  `MarqueeLabel::load()` (from a local file or in-memory JSON) is untouched
  and still backs developerMode's local preview.
- `AnalyzerPro::applyAnalyzer()`, a pure telemetry ping (serial number, OS,
  CPU, language, software/firmware version) sent to `rigexpert.com` on
  *every* analyzer connect, unconditionally, with no throttling. Also
  removed `checkFirmwareUpdate()`/`needCheckForUpdate()`, the automatic
  (timer-triggered, once/day) firmware-version check that used to run
  alongside it -- see below for what firmware checking now looks like.

### Fixed

- Settings' "Check for firmware updates" button (`checkUpdatesBtn`, in the
  Updates tab's Analyzer group) was fully wired up in the `.ui` and emitted
  `Settings::checkUpdatesBtn()` on click, but nothing was ever connected to
  that signal -- clicking it did nothing. It's now connected to
  `AnalyzerPro::on_checkUpdatesBtn_clicked()`, the only way left to trigger a
  firmware-update check (see Removed, above). Renamed from "Check Updates"
  to "Check for firmware updates" for clarity. This check only queries
  `rigexpert.com` for the latest firmware version/download link -- it does
  not send the telemetry ping `applyAnalyzer()` used to send.
- Firmware update flow no longer tries to flash the downloaded file to the
  analyzer (`AnalyzerPro::updateFirmware()`/`BaseAnalyzer::update()`) --
  applying vendor firmware isn't something this non-vendor-distributed build
  should attempt unattended. `on_downloadFileComplete()` now saves the
  downloaded file instead, to the user's Downloads folder (falling back to
  `Settings::localDataFolder()` if that can't be resolved) as
  `AntScope2_firmware_<model>_<version>.bin`, and the update dialog reports
  the saved path so the user can find it (and apply it via the vendor's own
  tool, if desired).

- Language translations not loading in deeper build layouts (e.g. Qt
  Creator's default shadow-build directory) — `Settings::languageDataFolder()`
  was resolving one directory too high on Linux, landing on an empty folder
  instead of the `.qm` files staged next to the binary. Also the underlying
  cause of issue #32's crash when changing language. (#32)
- `Settings` and `Calibration` persisted to differently-cased ini filenames
  (`antscope2.ini` vs `AntScope2.ini`), which only stayed unified by
  accident on case-insensitive filesystems (Windows/macOS); on Linux they
  silently diverged into two separate files. `Calibration` now goes through
  `Settings::setIniFile()` instead of its own hardcoded filename, and
  `setIniFile()` does a one-time migration that folds any surviving old
  file's settings into the new one. (#43)

### Removed

- Settings' two "Auto check updates" checkboxes (`autoUpdatesCheckBox` next
  to the firmware-update button, `checkBox_AntScopeAutoUpdate` in the
  AntScope2 group) and everything backing them
  (`Settings::firmwareAutoUpdateStateChanged`/`antScopeAutoUpdateStateChanged`,
  `MainWindow::on_firmwareAutoUpdateStateChanged`/`on_antScopeAutoUpdateStateChanged`,
  `m_autoFirmwareUpdateEnabled`/`m_autoUpdateEnabled`, the `autoFirmwareUpdate`/
  `autoUpdate` ini keys). Neither checkbox ever actually gated anything --
  the automatic checks they implied control over fired unconditionally
  regardless of their state even before today's changes -- and now that
  those automatic checks are gone outright (see above), there was nothing
  left for them to control.

### Fixed

- mainwindow.ui layout: the "Run" groupbox (Single/Continuous) had started
  overlapping the "Frequency" groupbox above it, and its own frame would
  resize oddly as the main window resized. Moved `singleStart`/
  `continuousStartBtn` into `groupBox_Fq`'s form layout as a new row and
  deleted `groupBox_Run` outright, along with a duplicate/dead
  `ui->groupBox_Run->setStyleSheet(style)` call (it was being called twice)
  in `MainWindow::setStyles()`. `groupBox_Fq`'s vertical size policy is now
  `Fixed` (was `Preferred`, capped by a hardcoded `maximumSize` that was
  too small for the extra row) so it always sizes to its actual content
  and never grows to soak up extra window space, which belongs to
  `groupBox_Presets`/`groupBox_Measure`'s expanding tables instead.
- `spinBoxPoints` (the "Points" field in the Frequency box) had an explicit
  `Expanding` vertical size policy, unlike every sibling field in that box
  -- it (and its row) visibly stretched on resize while everything else
  stayed put. Changed to `Fixed`.
- `tableWidget_presets` (Presets box) always showed its vertical scrollbar
  and had a hardcoded 170px minimum height with an `Expanding` vertical
  policy, unlike `tableWidget_measurments` (Measurements box, which resizes
  correctly) -- the mismatch meant the Presets box's Add/Delete/Move-up
  buttons overlapped the bottom of the list once the main window got small
  enough that the table couldn't honor both its own floor and the button
  row's space. Mirrored Measurements' table settings onto it exactly:
  `Maximum` vertical policy, no hardcoded minimum height, vertical
  scrollbar `AsNeeded` instead of `AlwaysOn`.
- Settings dialog: the tab widget had a `Maximum` vertical size policy while
  the Close button row below it defaulted to `Preferred` -- with neither
  set to `Expanding`, Qt's box layout ended up visually centering the
  whole tab-widget-plus-button group in the dialog instead of anchoring it
  to the top. Set the tab widget to `Expanding` so it fills the dialog and
  the Close button naturally stays pinned to the bottom edge.
- Settings' General tab: two vertical spacers between the license buttons
  and the measurement-system groupbox were implicitly `Expanding` (Designer's
  default when a spacer's `sizeType` isn't set explicitly), so once the tab
  widget above started actually receiving extra vertical space, these two
  absorbed all of it and visibly spread the tab's contents apart. Pinned
  both to `Fixed`, and added one new trailing `Expanding` spacer at the
  bottom of each of the tab's two columns instead, so extra space collects
  there rather than in the gaps between existing controls.
- `MainWindow::setGeometry(rect)` restored the exact saved window geometry
  from `AntScope2.ini` on launch with no floor. A saved size from before
  today's layout changes (or just a window shrunk to its smallest and then
  closed) could force the window below what the current layout actually
  needs, leaving the right-hand groupboxes overlapping until some later
  resize forced Qt to redo the layout pass -- symptoms included controls
  needing several clicks before they'd register a selection, since clicks
  were landing on whichever wrongly-sized/positioned widget ended up on
  top. The restored geometry is now floored at `minimumSizeHint()` so a
  stale saved size can't do this.

### Removed

- The scrolling "marquee" widget (`MarqueeLabel`/`marqueelabel.cpp`/`.h`,
  `MarqueeString`, `ui->labelMarquee`) entirely. Its only reason to exist
  was rendering vendor-fetched ad/news content (removed earlier) and
  relaying `AnalyzerPro`'s (already-unemitted) `showNotification` signal --
  with both gone, the widget had no remaining purpose. Also removed
  `MainWindow::on_showNotification()` and `AnalyzerPro::showNotification`,
  its only caller/signal.

### Fixed

- Tab clicks in the main tab bar would sometimes not register --
  particularly around TDR/Smith -- needing several clicks, or a window
  move/resize, before a tab would actually switch. Root cause:
  `MainWindow::on_1secTimerTick()` (which shows/hides the "brief params
  under cursor" hint every 100ms based on cursor position) computed the
  active plot widget's global bounding box by manually adding
  `this->geometry()` + `ui->tabWidget`'s offset + `m_swrWidget`'s offset --
  skipping the offset of `m_swrWidget`'s own parent tab page *within*
  `tabWidget` (the tab bar's height and the tab widget's frame border).
  That missing offset shifted the computed box's top edge up into the tab
  bar itself, so the cursor still read as "inside the plot" while hovering
  the tab bar -- the hint (a real `Qt::Tool` floating window, not a child
  widget clipped to the plot) never got told to hide, and sat on top of
  the tab bar eating clicks meant for it until the cursor happened to land
  somewhere even that oversized box didn't cover. Replaced the manual
  arithmetic with `m_swrWidget->mapToGlobal(QPoint(0, 0))`, which walks the
  full parent chain correctly instead of re-deriving it by hand.

### Changed (docs, following up on the AntScopeZ rename)

- `README.md`/`BUILDINFO.md`: renamed throughout, kept the vendor
  attribution (this is a fork of RigExpert's AntScope2, not affiliated with
  or endorsed by them), and added a note that packaged releases bundle Qt
  6.11's shared libraries and that building from source should target Qt
  6.11 -- older Qt (e.g. distro-packaged 6.4.x) has shown real bugs (see
  below) that don't reproduce under 6.11.
- `BUILDINFO.md`'s "Build options" table still listed `ANTSCOPE_OLD_TDR`
  (removed earlier this session) and "Known issues" still mentioned
  `analyzer/analyzer.{h,cpp}` (deleted in a prior session) -- both stale,
  now fixed. Folded in the Qt-6.4-vs-6.11 rendering/connect bugs found
  during `.deb` testing (analyzer connect failures on fresh install,
  incomplete repaint after resizing larger, stale paint artifacts) as a
  proper "Known issues" entry instead of leaving them only in local notes.
- `docs/user-guide.md` renamed throughout.
- `build.sh` (`TARGET=`), `.gitignore` (the ignored ini filename/path
  comment), and `Info.plist` (macOS bundle name/executable/icon) updated to
  match. `Info.plist`'s `CFBundleIdentifier` was literally
  `com.rigexpert.AntScope2` -- changed to `io.github.hz23116.AntScopeZ`;
  its `CFBundleShortVersionString` was also stale (`1.4.11`) and is now
  `2.1.4`.
- Deleted `AntScope2_resource.rc` -- already confirmed unused (Qt generates
  the real `.rc` via `QT_TARGET_RC_ICONS`; this one hardcoded an absolute
  path that never matched this repo's layout) rather than renaming a dead
  file.

### Changed

- The `.deb` now actually bundles its own private copy of Qt 6.11, rather
  than just documenting that it should (see the "Changed (docs...)" entry
  above) -- `CMakeLists.txt`'s Linux install rules use
  `qt6_generate_deploy_script()`/`qt_deploy_runtime_dependencies()` (Qt's
  own CMake-integrated equivalent of `linuxdeployqt`) to copy the Qt 6.11
  libraries and plugins the built binary actually links against into
  `/usr/lib/x86_64-linux-gnu/antscopez/`, with an `INSTALL_RPATH` target
  property pointing there. `.deb` packaging now happens via the `release` preset
  (Qt 6.11, from `/opt/Qt`) instead of `system-qt`, which is kept only for
  reproducing the Qt-6.4-specific bugs documented in `BUILDINFO.md`.
  Getting there took working around three separate gaps: `qt_deploy_runtime_dependencies()`'s
  `EXECUTABLE` argument needs `${QT_DEPLOY_BIN_DIR}` left unexpanded until
  the generated deploy script actually runs, which requires calling
  `qt6_generate_deploy_script()` directly rather than the versionless
  `qt_generate_deploy_script()` macro (macro-argument forwarding via
  unquoted `${ARGV}` re-parses and silently empties it); by the time that
  script runs, `install(TARGETS)` has already rewritten the executable's
  RPATH away from the build tree's Qt install, so resolving what to bundle
  needs a temporary `file(RPATH_SET)` to the real Qt 6.11 lib dir first (a
  bug of omission, otherwise it silently falls back to system search and
  can bundle the wrong Qt) and back to the real `INSTALL_RPATH` value
  afterward; and CPack's `dpkg-shlibdeps` integration can't recognize the
  bundled libraries as self-satisfying without a generated
  `debian/shlibs.local`, so without one it both requires the very system
  Qt packages bundling was meant to avoid *and* pins an exact
  `qt6-base-abi (= 6.4.2)` version that could make the `.deb` uninstallable
  on a system with a different distro Qt.

### Fixed

- The bundled `.deb` from the entry above crashed on launch
  (`qxcb: ... undefined symbol ... version Qt_6_PRIVATE_API`, a
  `QEventDispatcherGlib` symbol Qt's own glib event-dispatcher support
  needs) -- `$ORIGIN`-relative `RPATH` alone isn't enough for Qt's plugin
  loader to reliably resolve it, even though every bundled file is
  byte-identical to the original Qt 6.11 install and every `RPATH` is
  correct. `LD_LIBRARY_PATH` fixes it; `CMakeLists.txt` now installs the
  real binary as `AntScopeZ.bin` and puts a thin wrapper script (from the
  new `cmake/antscopez-wrapper.sh.in`) at the original `AntScopeZ` path
  that exports `LD_LIBRARY_PATH` before exec'ing it -- the same fix
  linuxdeployqt-based AppImages typically ship as an `AppRun` script.

### Fixed

- The running app fell back to a generic window icon (taskbar, alt-tab,
  etc.) regardless of `AntScopeZ.png`/`.ico`/`.icns` on disk -- individual
  dialogs (`screenshot.ui`, `print.ui`, ...) already referenced the `.qrc`
  icon for themselves, but nothing set `QApplication::setWindowIcon()`.
  `main.cpp` now does, from the same `:/new/prefix1/AntScopeZ.png`
  resource, which is also bumped from a blurry 2.5KB placeholder to a
  proper 64x64 render.

### Added

- A band selector above the Presets list: a combo box (`presetsBandComboBox`)
  listing every named band in the currently active ITU region, formatted as
  `<name> (<start> - <stop> kHz)` and derived from the same
  `itu-regions.txt`/`itu-regions-defaults.txt` data `MainWindow::m_BandsMap`
  already loads for the plots' shaded band overlays. Picking an entry sets
  Start/Stop (or Center/Range, matching whichever mode is active), re-ranges
  every plot in one pass, and reverts the combo to its resting
  "Select a band" placeholder so it's ready for the next pick. Repopulates
  automatically whenever the active region changes (`on_bandChanged()`),
  including on startup and after editing bands via Settings. Region entries
  without a name (a custom 2-field line in a hand-edited `itu-regions.txt`)
  are skipped -- nothing to label them with.
  Settings → General gets a new "Enable band selector" checkbox (under
  "Show band name") controlling its visibility, persisted as
  `Settings/band-selector-enabled`. It's seeded once, the first time bands
  are loaded, to whether the active region actually has any named bands
  (so a fresh install shows a working control instead of an enabled-but-
  empty one); after that, region switches never touch the saved choice --
  an enabled selector with nothing but the placeholder in it is fine, not
  an error.

### Fixed

- `appendSpaces()` (the Start/Stop thousands-separator helper) counted
  every character from the end of the whole string, including the decimal
  point and fractional digits, to decide where to insert a grouping space
  -- so any Start/Stop value with a fractional kHz part (e.g. picking the
  new band selector's 2200m entry, `135.7 - 137.8 kHz`) rendered with the
  separator in the wrong place (`"13 5.7"`). Now groups only the integer
  part; the fractional part (if any) is left untouched.
- Typing an extremely small Start/Stop value (e.g. `0.000005`) could crash
  the app with a `QCheckedInt` "Overflow in operator-" assert once that
  value reached an integer frequency conversion further downstream --
  Start/Stop had no lower or upper bound at all before this. `setFqFrom()`/
  `setFqTo()` (the single choke point every Start/Stop write already goes
  through -- manual edits, device data, presets, and the new band
  selector) now clamp to this app's actual supported range, 1 Hz - 10 GHz,
  and round to the nearest 0.001 kHz (finer than that is beyond any
  analyzer's real resolution anyway). `on_tableWidget_presets_cellDoubleClicked()`
  and the band selector's plot-range/redraw path clamp the same way, so a
  stale or hand-edited out-of-range preset/band entry can't reach the
  plots unclamped either.
- `Settings::setConnectButtonText()` had the only two user-facing strings
  in the app still spelled the British "analyser" -- the rest of the UI
  already said "analyzer" throughout. Also fixed two `mainwindow.cpp`
  comments quoting that same button text. (Any existing translation keyed
  off the old "Connect analyser"/"Disconnect analyser" source strings
  falls back to the now-updated English source until it's revisited --
  not addressed here.)
- Translated the Russian comments in `markerspopup.cpp`/`.h` and `popup.h`
  to English (narration only, e.g. "disable window decoration" -- nothing
  was hiding logic beyond `markerspopup.cpp`'s `m_mapHeader` column
  comments, which clarify that the `R||`/`X||`/`Z||`/`L||`/`C||` columns
  are the parallel-equivalent-circuit-model counterparts to `R`/`X`/`Z`/
  `L`/`C`'s series model, not just decoration).

### Added

- Settings → General's Language combo box now discovers whatever
  `QtLanguage_<code>.qm` files actually exist instead of listing a fixed,
  compiled-in 3-language array (English/Ukrainian/Japanese, with a
  commented-out, never-actually-available Russian) -- adding a language is
  now "compile a `.qm` and put it where the app looks," not "edit
  `mainwindow.h` and rebuild the whole app." `Settings::setLanguages()`
  scans both `Settings::localDataFolder()` (per-user, e.g.
  `~/.config/AntScopeZ`) and `languageDataFolder()` (the shared/installed
  copy) for `QtLanguage_*.qm`, and `MainWindow::loadLanguage()` prefers a
  user-folder copy over the shared one if both exist for the same code --
  the same override convention `itu-regions.txt` already uses (see
  `loadBands()`), so a language can be added or replaced by dropping a
  `.qm` into the per-user folder, without needing write access to the
  shared install location or a repackage. Each entry's display name comes
  from `QLocale(code).nativeLanguageName()` (e.g. "日本語" for `ja`) --
  the `.ts`/`.qm` format has no display-name field of its own,
  `QTranslator::language()` just returns the same code back. The active
  language is now persisted as that code (`Settings/languageCode`)
  instead of an index into the old fixed array (`languageNumber`), with a
  one-time migration from the old key so upgrading doesn't silently reset
  an existing Ukrainian/Japanese install back to English.

### Fixed

- The `.qm` translation files the app actually loads were prebuilt
  binaries checked into the repository root, completely disconnected from
  the real `.ts` sources in `locales/` -- a comment in `CMakeLists.txt`
  already flagged that running `lrelease` on `locales/*.ts` writes its
  output back to `locales/`, not to the root files the app ships, so
  editing a translation and regenerating never actually reached the
  running app. `CMakeLists.txt` now compiles `locales/*.ts` to `.qm` at
  build time via Qt's `qt_add_translations()` (new `LinguistTools`
  component dependency), as loose files (not embedded in Qt resources,
  since loose files are what makes the per-user override above possible)
  staged/installed the same way `cables.txt`/`itu-regions-defaults.txt`
  already are. `AntScopeZ` depends on the new `release_translations`
  target so a parallel build can't race ahead of it. The prebuilt root
  `QtLanguage_*.qm` (and an orphaned, untranslated `locales/QtLanguage.ts`
  template with no `language=` attribute -- 360/360 messages still
  `unfinished`) are deleted; both are now build artifacts, not tracked
  files. `build.sh`'s now-redundant manual `lrelease` pre-step (writing
  into the source tree, the same root-vs-`locales/` mismatch from above)
  is removed -- the CMake build handles it.
- A repo-wide sweep for user-facing strings not wrapped in `tr()`, wrapped
  the clear-cut ones: `MainWindow`'s "Screenshot" window title, "Add
  multi-charts" tooltip, "Create marker" context-menu action, the new band
  selector's `"%1 (%2 - %3 kHz)"` format string; `Settings`' firmware
  "Update"/"Updating..." button text; and file-dialog captions
  ("Export"/"Export PDF"/"Export PNG"/"Save as BMP"/"Save file"/"Open
  file") across `export.cpp`/`screenshot.cpp`/`print.cpp`/`mainwindow.cpp`
  that were inconsistent with the ones `settings.cpp`/`analyzerdata.cpp`
  already wrapped. Also wrapped the print-document header builder
  (`MainWindow::on_printBtn_clicked()`'s `string += "SWR graph"` etc.,
  ~10 sites feeding `Print::setHead()`) -- `Print::setName("SWR")`/`("TDR")`
  right next to two of them looked like the same kind of thing but turned
  out to be an internal mode discriminator (`if (m_graphName == "SWR")` in
  `print.cpp`), not display text, so left alone; wrapping it would have
  silently broken that comparison. `measurements.cpp`'s on-canvas "brief
  hint" popup text, initially flagged by the same sweep, turned out to
  already be correctly wrapped throughout (both the static labels and the
  live `tr("...%1...").arg(...)`-built values) -- the sweep's plain-text
  scan just couldn't see past `tr()`'s multi-line adjacent-string-literal
  concatenation to realize every fragment after the first one was already
  covered by it.
  A few short EE-abbreviation chart legend names (`measurements.cpp`'s
  `setName("R")`/`"X"`/`"Rp"`/etc., `CustomPlot.cpp`'s generic "Graph "+N
  fallback) are left as judgment calls for now, marked
  `// Needs tr() attention` for the next pass.
- `EditBandsDialog`'s three `g_showMessageBox()` titles turned out not to
  be dead code, just rare error paths: `"loadDefaults"`/`"load"`/
  `"EditBandsDialog::save"` (the function names, not real titles) only
  ever showed up if the shipped `itu-regions-defaults.txt` and/or the
  user's own `itu-regions.txt` failed to open or save -- the "Restore
  Defaults"/dialog-open/"Save" paths respectively -- which is why they'd
  never actually been seen in normal use despite being live code. Now
  `tr("Couldn't load default bands")`/`tr("Couldn't load bands")`/
  `tr("Couldn't save bands")`, with a comment at each explaining exactly
  when it fires.

### Removed

- `checked.png`/`unchecked.png` (and their `res.qrc` entries) -- unused
  since `Style::checkBox()` moved to the native/Fusion checkbox indicator;
  the only remaining trace was a comment in `style.cpp` explaining why.

## [2.1.3]

Baseline — changelog tracking starts here. See `git log` for history prior
to this point.
