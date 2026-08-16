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

- Settings > Themes: edit any of the 5 built-in themes' colors (window
  background, text, muted text, border, chart background, marker) with a
  live preview, a Default button to restore a theme's original colors, and
  Save/Cancel. Themes can be freely renamed.

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
