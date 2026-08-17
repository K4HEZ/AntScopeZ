---
layout: default
title: Changelog
---

# Changelog

All notable changes to this project are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the version number
below should track `project(VERSION ...)` in `CMakeLists.txt`.

## [Unreleased]

### Added

- Tools > Marker Comparison: compare two markers' frequency/SWR/return
  loss/R/X, estimated Q and equivalent L/C off the Current marker, and
  estimate how much to trim (or add) to a simple 1/4-wave vertical,
  1/2-wave dipole or full-wave antenna to move resonance from one marker
  to the other.
- Settings > Markers: "Automatically set a marker at the lowest SWR" (Marker
  behavior group, on by default) places a marker at the swept trace's
  lowest-SWR point right after a single/full scan finishes -- never during
  a Continuous scan, and only if a marker slot is free.
- Tools > TDR Analysis: distance to the strongest reflection in the last
  TDR scan, whether it looks like an open or a short, and a velocity
  factor picker (cable-type presets from cables.txt, local to this dialog
  -- doesn't touch Settings > Cable) including a reverse calculator that
  solves for velocity factor given a known physical cable length. "Use
  this velocity factor" applies the solved value to Settings > Cable as
  Custom and refreshes the TDR chart's distance axis immediately -- also
  resets R0/loss to the "Ideal 50-Ohm cable" convention (50 Ohm, no loss)
  instead of leaving whatever a previously-selected Preset's real figures
  were showing, which would otherwise look like real data for the cable
  actually being solved for here. If Settings > Cable happens to already
  be open, it updates live instead of only taking effect the next time
  the dialog is opened.
- Settings > Cable: new Preset/Custom toggle. Preset locks velocity
  factor/R0/conductive+dielectric loss/loss units/frequency to whatever
  cableComboBox has selected (so the displayed numbers can never silently
  disagree with the cable name shown); Custom disables the combo and
  hand-edits those fields instead, same as before this existed. Locked
  fields get their own deliberate "read-only" styling (full-contrast text,
  flattened into the dialog background) instead of Qt's normal disabled
  dimming, so they read as "showing a fixed value" rather than "broken".
  This is now the *only* thing that ever disables those fields -- see the
  cableActionEnableButtons() removal below.

### Changed

- Settings > Cable tab reordered to match how it's actually used and how
  the fields are actually consumed by calcFarEnd() (measurements_farend.cpp):
  cable type picker first; velocity factor/R0 joined the former "Cable
  loss" groupbox (renamed "Cable specifications") alongside
  conductive/dielectric loss/units/frequency -- all seven lock together
  under Preset. Cable length moved into "Transmission line options"
  instead (R0/loss/length all only affect anything once Subtract or Add
  cable is selected, but length isn't a property *of* the cable the way
  the other six are, so it stays always-editable regardless of
  Preset/Custom) and now shows its own ft/m unit label that follows the
  app's Metric/Imperial setting, converting the displayed number rather
  than just relabeling it -- previously always feet with no indication of
  that. Transmission line options' three buttons are stacked vertically
  with a plain-English explanation next to each (Do nothing/Subtract
  cable/Add cable). Export/Update graphs left as-is.
- Removed Settings::cableActionEnableButtons() -- a second, older mechanism
  that also disabled cableR0/cableLossComboBox/cableLen/conductiveLoss/
  dielectricLoss/atFq/anyFq whenever Transmission line options was set to
  Do nothing (vs. Subtract/Add), fighting the new Preset/Custom lock for
  control of the same widgets. Editability is now solely a Preset/Custom
  question ("can I change this"), never a Do-nothing/Subtract/Add one
  ("does this currently matter") -- those turned out to be different
  questions that don't need the same answer.

### Fixed

- Settings > Cable's Export button crashed with no measurement data yet --
  it always passed size()-1 (-1, wrapping to 4294967295 through a quint32
  parameter) as the measurement index to export, an out-of-bounds access
  as soon as Export needed a suggested filename. Now shows a "run a scan
  first" notification instead of opening Export at all in that case.
- Settings dialog's Data Folder field and any Preset-locked Cable
  specifications fields kept showing the previous theme's background color
  if the active theme was changed (View > Theme) while Settings stayed
  open -- Style::readOnlyLock() bakes the active theme's colors into a
  stylesheet once (unlike most of the dialog's controls, which are native
  and repaint for free off the live application palette), and nothing was
  re-running that when the theme changed out from under an already-open
  dialog. MainWindow::changeColorTheme() now refreshes Settings' styles
  too when it's currently open.
- m_settingsDialog could go dangling if Settings was closed via the native
  window decoration/Alt+F4 instead of its own Close button -- only that
  button's handler ever nulled the pointer; WA_DeleteOnClose's deferred
  deletion from any other close path left it pointing at a freed object.
  Added the same destroyed()-nulls-the-pointer safety net the newer
  Marker Comparison/TDR Analysis dialogs already had.

## [2.2.1] - 2026-08-16

### Added

- Settings > Themes: edit any of the 5 built-in themes' colors (window
  background, text, muted text, border, chart background, marker) with a
  live preview, a Default button to restore a theme's original colors, and
  Save/Cancel. Themes can be freely renamed.
- Help > About AntScopeZ now shows a centered build timestamp below the
  version.

### Changed

- View > Theme now offers 5 built-in options (Light/Dark/Red/Green/Blue)
  instead of just Light/Dark; the default theme on first launch is now
  Light instead of Dark.
- Settings > General's old standalone "Chart background" swatch is gone --
  chart background is now part of each theme, set from Settings > Themes.
- "Show Band Name" now defaults to on.

### Fixed

- Switching the active theme from the View menu didn't update the chart
  background or marker colors, or markers already placed on the chart --
  only saving a theme from Settings > Themes did.
- Settings > Themes' color pickers now open with the swatch's current
  color pre-selected.
- The open Settings dialog's own text didn't pick up a newly-saved theme
  until it was closed and reopened.
- Marker table's "Del"/"Marker" column headers never translated, in any
  language, even after a live language switch -- now translate properly,
  and "Del" is a plain "x" (no translation needed) instead.
- Print dialog's three buttons were fixed-size and clipped translated
  text; now resize to fit.
- The window title's "- Analyzer not connected"/connected-device-name
  suffix could get stuck in English (or whatever language was active at
  first launch) forever, even after switching languages -- it was being
  saved and blindly restored verbatim around each language reload instead
  of actually being rebuilt in the new language.
- Brought Ukrainian, Japanese, and Spanish translations up to date with
  everything added since they were last refreshed, including several
  strings found silently untranslated in older, pre-existing entries.

## [2.2.0] - 2026-08-15

### Added

- Menu bar (File/Edit/View/Connect Analyzer/Help), replacing the old button
  row.
- Main window is now a resizable 3-pane layout with a new docked Cursor
  Details panel.
- Help > About AntScopeZ, showing the running app version.
- Settings > Markers: max-markers spinner and a column picker for the
  Markers popup.
- Speed/Accuracy slider under Points, replacing the separate "Measurement
  speed..." dialog.
- Settings > General: "Data folder" field controlling where Save/Export/
  Screenshot dialogs default to.
- Settings' Developer tab: Debug Logging section with per-interface
  (Serial/USB-HID/BLE/NanoVNA) raw TX/RX logging to a daily log file,
  including a filter for BLE keepalive traffic.

### Changed

- Settings > Updates tab: app version info moved to the top; firmware-
  update checks disabled (explained inline) over privacy/security
  concerns with that network call.
- Settings' developer-only tab (now "Custom Analyzer") is always visible,
  with its controls explicitly disabled and marked "under development"
  instead of hidden.
- Renamed the device-picker dialog from "Select device" to "Connect
  Analyzer" for consistency.
- Points is now a plain text field (capped at 1000) instead of a spinner.
- Reworked keyboard tab order to follow the visual layout.
- The Frequency/SWR hint box is now docked in the main window instead of a
  floating popup.
- Save, Export, Print, and Screenshot dialogs now share one default folder
  instead of independent, mostly-unused "last path" settings; default
  filenames improved.

### Fixed

- Duplicate file extensions sometimes appended in Save/Export dialogs
  (e.g. ".asd.asd").
- Measurements/Presets table columns weren't resizable.
- `.deb` packages could depend on themselves, making them uninstallable.
- Disabled Single/Continuous/Full Range buttons ignored the Light/Dark
  theme.
- Some fields had no fill contrast against the dialog background in Dark
  mode.
- Speed/Accuracy slider didn't respond to arrow keys.
- Tab key got trapped inside the Presets/Measurements tables; added Enter
  as a shortcut to load a row.
- Presets table columns now auto-size to content instead of showing a
  scrollbar.
- A stray Windows registry file was written on non-Windows platforms
  during `.asd` file-type registration.
- PDF export sometimes came out as A4 instead of the configured page size.
- Device-screenshot images were off-center or edge-to-edge in PDF export.
- Crosshairs and cursor hints could get stuck, disappear, or not track the
  chart theme across tabs (including TDR).
- The scan-position tick stayed drawn after a scan finished on NanoVNA-
  connected analyzers.
- Poor contrast on the Markers/graph hint boxes against some chart
  backgrounds.
- The graph-hint box flickered at the edge of scanned data and ignored
  its checkbox while Settings was open.
- App icon missing from the About dialog.
- Export dialog missing a Close button.

## [2.1.6] - 2026-08-10

### Changed

- Clarified project licensing: AntScopeZ is distributed under GPLv3-or-later
  overall; added `THIRD-PARTY-LICENSES.md` covering every bundled/linked
  third-party component, and credited AntScopeZ's own copyright in
  `LICENSE.txt` alongside RigExpert's original.
- Packaged builds now ship the license/attribution files above instead of
  leaving them source-tree-only.

### Fixed

- Save dialog no longer suggests a double `.asd.asd` filename.

## [2.1.5] - 2026-08-10

### Fixed

- Fixed a `.deb` install issue that could break other installed Qt
  applications.
- Fixed a crash on first launch after leaving the app on a hidden
  developer-mode tab.
- Fixed a crash when using a custom analyzer profile with a connected
  device.
- Fixed popup positions not saving correctly under non-English languages.
- Fixed the plot occasionally not responding to mouse input right after
  launch.
- Fixed Start/Delete/Clear occasionally becoming unresponsive after
  placing a marker.
- Fixed "Open 'Connect Analyzer' on launch" not fully disabling
  auto-connect when unchecked.

### Changed

- Development builds between releases now report a `-dev` version suffix.

## [2.1.4] - 2026-08-09

### Changed

- Renamed the project from AntScope2 to **AntScopeZ** throughout (executable,
  icons, window titles, file associations, and settings location).
- Added a real Light/Dark theme system with a functional theme selector in
  Settings.
- Replaced the Metric/Imperial checkboxes with a single combo box.
- Removed the RigExpert logo from the main window.
- The `.deb` package now bundles its own copy of Qt 6.11 instead of relying
  on the system's Qt.

### Added

- A band selector above the Presets list for quickly setting Start/Stop
  from a named amateur-radio band.
- Settings option to open Connect Analyzer automatically on launch.
- UI languages are now discovered automatically instead of being hardcoded;
  added a Spanish translation.
- User guide documentation for the Customize/custom-analyzer settings.

### Fixed

- Numerous theme and dialog rendering issues (unreadable text, unstyled
  dialogs, Light mode not applying).
- Smith chart and TDR/S21 axis colors now follow the active theme.
- Start/Stop value formatting and out-of-range input handling.
- Several layout issues in Settings and the main window causing overlapping
  controls.
- Translation loading in non-standard build layouts.
- Firmware-update button now works; the update flow saves the downloaded
  file instead of attempting to flash it automatically.
- Tab-click reliability issues near the tab bar.
- Connect Analyzer could open more than one copy of itself, or fail to
  block other windows while open.
- Screenshot dialog image no longer overlaps its buttons.
- Save dialog now suggests the measurement's own name as the filename.
- Minor spelling/translation fixes ("analyser" -> "analyzer").

### Removed

- Vendor auto-update and telemetry checks that contacted RigExpert's
  servers on every launch/connect.
- Dead code paths, unused build options, and unused image assets.
- Bundled sample calibration files (each user now creates their own).

### Known issues

- Print dialog page-size default and Properties layout glitch (Linux).
- Some Qt-provided translations are incomplete (e.g. Spanish file-dialog
  strings).

## [2.1.3]

Baseline — changelog tracking starts here. See `git log` for history prior
to this point.
