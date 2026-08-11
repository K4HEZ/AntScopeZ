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

- "Scan Mode" combo box (Start/Stop / Center/Range) replaces the old
  Limits/Center-Range toggle buttons.
- A "Speed/Accuracy" slider now sits directly under Points; the separate
  "Measurement speed..." dialog is gone.

### Changed

- Points is now a plain text field instead of a spinner, capped at 1000 to
  match the Speed/Accuracy slider.
- Reworked keyboard tab order across the main window to follow the visual
  layout (tabs, toolbar buttons, Frequency/Presets/Measurements groups in
  order, ending at Clear).

### Fixed

- Single/Continuous/Full range buttons' disabled state was a hardcoded
  dark gray that ignored the Light/Dark theme; now theme-correct like
  every other disabled control.
- Some fields (e.g. Save dialogs' "File name" box) had zero fill contrast
  against their own dialog background in Dark mode -- `QPalette::Base` was
  identical to `Window`.
- Speed/Accuracy slider couldn't be adjusted by keyboard -- arrow keys
  were being intercepted by the chart pan/zoom shortcuts first.
- Presets/Measurements tables trapped Tab-key focus inside instead of
  moving to the next control; added Enter as a keyboard equivalent for
  the double-click-to-load-a-row action.
- Presets table columns now auto-size to content instead of allowing a
  horizontal scrollbar.
- `.asd` file-type registration wrote a literal `HKEY_CLASSES_ROOT` junk
  file on non-Windows platforms instead of being skipped -- now guarded
  with `#ifdef Q_OS_WIN`.
- PDF export (Print dialog's "Save as .pdf", the "Screenshot from AA"
  dialog's "Export to PDF", and Print dialog's "Print" &rarr; "Print to
  File (PDF)") was silently coming out as A4 even where the page size was
  explicitly set to Letter in code. All three were routed through
  `QPrinter`, which simulates a physical printer/driver and was overriding
  the explicit page size; switched to `QPdfWriter` (writes PDF directly, no
  driver involved) for the actual PDF-producing case in each. Verified via
  `pdfinfo` on real exported files (confirmed Letter); the A4 that still
  showed up afterward in one PDF viewer (qpdfview) turned out to be that
  viewer mis-displaying page size generally, confirmed by it also
  mislabeling an unrelated third-party PDF as A4. A genuine physical-
  printer job (as opposed to a PDF file) is unaffected by this fix and
  still goes through `QPrinter` as before -- see `BUILDINFO.md` for the
  still-open question of whether that path's own Properties dialog is
  affected by the same underlying default-page-size issue.
- "Screenshot from AA"'s "Export to PDF" was positioning the device
  screenshot image off-center (small/square-LCD models) or stretching it
  edge-to-edge with no margin (the large-landscape-LCD `AA-2000 ZOOM`/
  `AA-3000 ZOOM`/`AA-1500 ZOOM SE` models). Both now center the image
  properly, the latter with a 50px margin instead of 0. See `BUILDINFO.md`
  -- the large-LCD fix couldn't be tested against real hardware.

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
