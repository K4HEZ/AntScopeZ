---
layout: default
title: Changelog
---

# Changelog

All notable changes to this project are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the version number
below should track `project(VERSION ...)` in `CMakeLists.txt`.

## [Unreleased]

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
