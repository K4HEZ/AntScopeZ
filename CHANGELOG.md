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

- Print dialog: "Print Band Highlighting" checkbox (toner saver, off by
  default) -- see the Fixed entry below for why this needed a default.
- Settings > Updates > "Check for firmware updates" (the internet-facing
  check-and-download half of firmware update) is live now, gated behind
  the new "Use TLS" checkbox (see Fixed, below). Downloaded firmware saves
  to the configured data folder (Settings > General), not the OS Downloads
  folder. Every check/download request logs its URL, HTTP status, and any
  error to the console. Browse (file picker, also live now) lets you look
  at a chosen file's info, but the Update button -- actually flashing it to
  the connected analyzer -- stays deliberately disabled; its own greyed-out
  state is the only notice needed.

### Fixed

- Print/PDF/copy-image output: band highlighting (SWR/Phase/Rs/Rp/RL/S21/
  TDR/User tabs) was silently drawing nothing, for every region, always --
  `Print::drawBands()` only handled a 2-field band-data shape that no real
  band entry actually has (they're all 3 fields, `freq1,freq2,name`). The
  live chart already handled this correctly; print now matches it.
- Remote API: `sweep` now rejects `start_hz == stop_hz` instead of passing a
  zero-span sweep through to the device -- known to hang at least one real
  device (RigExpert Match). The GUI already avoided this via its own
  single-frequency mode; the Remote API has no equivalent yet (tracked
  separately).
- BLE: a notification shorter than a full packet (e.g. from mismatched/older
  firmware) caused an out-of-bounds read while checking its CRC --
  `QByteArray`'s own bounds check is a `Q_ASSERT`, compiled out entirely in
  release builds. Now rejected explicitly before any parsing touches it.
- HID: a device reporting a payload length larger than what it actually sent
  (raw stack array, no bounds check possible in any build mode) caused a
  stack over-read of up to ~193 bytes. Now clamped to both the buffer size
  and the actual bytes received.
- Every RigExpert network request (app registration, and now firmware
  update) unconditionally disabled TLS certificate verification, regardless
  of scheme. New "Use TLS" checkbox (Settings > Updates, on by default)
  drives both the scheme and certificate verification now; off is an
  escape hatch for a vendor-side certificate problem, not routine.
- HID firmware-update integrity check (currently unreachable from the UI --
  applying firmware isn't exposed as a feature yet) silently reported
  success regardless of whether the device actually verified the write, due
  to a stale-read HIDAPI queue. Now checks every chunk's response, not just
  the first -- in both `HidFirmwareUpdater::update()` and a second,
  independent copy of the same write loop in `HidAnalyzer::update()`.
- HID firmware update (same unreachable-today feature) would hang forever
  waiting to enter the device's bootloader after sending RESET --
  `m_bootMode`, the condition the wait loop was blocking on, was never set
  `true` anywhere in the codebase. Replaced with a bounded poll for the
  device re-enumerating under its bootloader VID:PID, matched by serial
  number.
- Settings > Cable: subtract/add cable's result was off by ~3.28x
  (attenuation and phase both) specifically when "dB/100feet" -- the
  default -- was the selected loss unit, relative to the other three
  (equivalent) choices. A leftover unit correction, made redundant once
  cable length was normalized to feet at the input boundary, was still
  applying itself only for that one dropdown selection.
- Settings > Cable: the cable length field now matches the width of the
  other numeric fields on that tab (was unconstrained).
- AA-230 firmware-info read: a slow or partial reply shorter than the
  expected fixed-size response caused an out-of-bounds heap read. Now
  clamped to the actual bytes received.
- Settings > Updates > "Check for firmware updates": the "Checking..."
  button animation ran on its own fixed 5s timer regardless of when the
  request actually finished, so a fast failure/error dialog could appear
  while "Checking..." kept cycling for several more seconds afterward. Now
  stops the moment the real result arrives.
- "Check for firmware updates" with no analyzer connected sent RigExpert a
  request with blank model/serial fields and showed a generic failure
  dialog. Now checked upfront with a clear "connect an analyzer first"
  message instead.
- HID devices that report full info as `serial,version,LICx` (e.g. Match)
  instead of the older `MODEL REV x vYYY` string never had their version/
  revision recorded anywhere -- `AnalyzerPro::slotFullInfo()` only read the
  `LICx` part and threw the rest away, leaving the update-check URL's
  `fw=`/`revision=` permanently blank for these devices. Now parsed.
- Some BLE devices' firmware (confirmed on a real Match unit) never sends
  the full-info field that carries serial number/version at all over BLE
  -- not a parsing bug, the data isn't on the wire. "Check for firmware
  updates" now fails fast with a message pointing at USB instead of
  sending a request that can never identify the device.
- "Check for firmware updates" showed "Please try later" even when the
  request succeeded outright (HTTP 200, no network error) but RigExpert's
  server had nothing to report -- confirmed by hand against the live
  endpoint that it currently returns an empty result for every input,
  real or fake, so that's not a transient condition worth retrying. Now
  says so plainly instead.

### Documentation

- README.md and the user guide's Updates tab / troubleshooting sections
  rewritten to match this cycle's actual firmware-update state (Check for
  firmware updates and Browse are live now; Update stays disabled) instead
  of the older entirely-disabled description.
- BUILDINFO.md: documented the new `ANTSCOPEZ_SANITIZE` build option and
  `.clang-tidy` config.
- Translations: `update_translations` run, 15 new strings translated
  (uk/ja/es). All three `.ts` files fully finished, 0 unfinished.

## [2.2.5] - 2026-09-07

### Changed

- File > "Data from AA" moved to the Analyzer menu, under "Screenshot...".
- Settings > Analyzer: "Use reconnect to drain unwanted data" moved from
  the General tab, placed under "Analyzer timeout:".
- Settings > Analyzer > Custom Analyzer: "Use customized analyzer"
  checkbox moved out of the group box to directly above it, and now
  enables/disables every control inside. The "under development" label
  and the group box's permanent forced-disable are gone -- Custom
  Analyzer is a live feature now, no longer gated behind the (removed)
  `-developer` flag, and its on/off state and saved presets persist
  across restarts.
- Spin boxes, combo boxes, and sliders throughout the app no longer
  react to the mouse wheel unless already focused (click or Tab in
  first) -- fixes values silently changing while scrolling past a
  control on a long settings page.
- Settings dialog: labels/fields grid-aligned and oversized/mis-sized
  text boxes fixed across General, Markers, Analyzer, and Graphs tabs
  (several fields were stretching to fill the dialog width, or sitting
  far from their label, instead of sizing to their actual content).
- Settings > Graphs > "Chart Y-Axis Ranges" reworked into a 3-column
  grid (chart name | min | max) instead of 4 separate min/max-labeled
  rows.
- "Edit ITU Bands..." moved off its own Edit menu (now removed) onto a
  new Settings > **ITU Bands** tab, between Graphs and Updates -- same
  text box and Cancel/Restore Defaults/Save buttons, just living on a
  tab instead of a standalone dialog. The Analyzer menu now sits where
  Edit used to (right after File).

### Fixed

- Custom Analyzer: `AnalyzerPro::getMinFq()`/`getMaxFq()` returned a
  model-name string instead of a frequency (copy-paste bug); had no live
  caller, so no observed symptom.
- Settings > Custom Analyzer's "New" button set the prototype combo box
  to the literal text "names[0]" instead of selecting a real entry.
- Pressing Esc in the Settings dialog closed it but left File >
  Settings... permanently disabled -- a latent bug present since this
  fork's first commit, never previously exercised/reported. `reject()`
  (Esc's actual entry point) now routes through the same cleanup the
  Close button already used correctly.
- Band Selector: dropped the issue #23 stopgap that force-enabled it on
  every startup regardless of what was saved -- it's a plain persisted
  bool again (default `true` only when unset), so turning it off
  actually survives a restart now (#13).
- View > Band Highlighting had no region checked on first launch (or
  after deleting the ini) even though bands were correctly shown --
  the menu-builder and every other reader of the same `current_band`
  ini key disagreed on its default. Both now go through one shared,
  self-healing resolver that also persists its choice immediately.
- Editing bands in Settings' ITU Bands tab and clicking Save now
  rebuilds the View > Band Highlighting menu immediately instead of
  requiring a restart to see a renamed/added/removed region.
- Charts (and printed output) no longer fall back to a hardcoded
  18-entry ham-band list when the current region has no band data --
  they draw nothing, matching an intentionally-empty `itu-regions.txt`.
  Also fixes `itu-regions.txt`/Settings' ITU Bands tab edits not
  clearing already-drawn bands when the new region has none.
- Ported 3 Smith-chart fixes from RigExpert AntScope2 2.0.3 that never
  made it across the QCustomPlot 1.x->2.x rewrite (#10): point
  indexing that diverged across a Continuous "continue" (could corrupt
  point ordering); the Smith trace losing its assigned color on every
  "continue" (pen never reapplied after the curve was recreated); an
  out-of-range crash risk hovering the Smith chart cursor (a bounds
  clamp was already written in the code but commented out).
- `Measurements::saveData()` silently did nothing (no file, no
  warning) if the save path didn't contain ".asd" -- not guaranteed,
  since the file dialog never calls `setDefaultSuffix()`. Appends the
  extension instead of refusing to save (also from AntScope2 2.0.3, #10).
- BLE analyzer: writing a new command (start scan, screenshot, get
  analyzer data) while a ping response or another transfer was still
  outstanding risked colliding with it on the wire. New commands now
  queue and dispatch once the outstanding one actually completes;
  Stop now sends a real wire abort too (still treated as best-effort,
  not guaranteed) instead of only a local flag. Ported from RigExpert
  AntScope2 2.0.3, #10.
- Follow-up to the BLE queueing fix above: the Screenshot dialog's
  completion never actually reached the BLE analyzer at all
  (`AnalyzerPro::on_screenshotComplete()` was missing the call), so its
  queueing state could stay stuck after a screenshot; a per-record
  reset in the analyzer-data list fetch could clear it mid-fetch
  instead of only once the whole list was done; FULLINFO's last field
  and an empty-queue ping response weren't resetting it either.
- License tier "BASE"'s max frequency was still 70MHz; RigExpert
  AntScope2 2.0.3 corrected it to 75MHz.
- AA-650 ZOOM screenshots: the device can report a different pixel-
  compression mode over BLE (`FULLINFO`'s `screenCompression` field);
  `Screenshot` only ever decoded the one format, so screenshots came
  out wrong/garbled on the other mode. Now picks the right decode
  based on what the device actually reports. Ported from RigExpert
  AntScope2 2.0.3, #10.

### Removed

- The `-developer` CLI flag and `g_developerMode`, along with the
  `-comserial`/`-usbhid`/`-nanovna`/`-ble` CLI shortcut for pre-enabling
  Debug Logging -- both already inert/redundant (Debug Logging is always
  reachable via Settings > Developer's checkboxes regardless). The
  already-deleted UDP remote-control bridge's leftover documentation in
  BUILDINFO.md was trimmed to match.
- The standalone "Edit ITU Bands..." dialog (`EditBandsDialog`) -- its
  functionality now lives on Settings' new ITU Bands tab instead.

### Documentation

- Ran `update_translations` and translated this cycle's 14 new/changed
  source strings (Settings' reworked General/Analyzer/Graphs tabs, the
  new ITU Bands tab, and the About box's updated text) for Ukrainian,
  Japanese, and Spanish. All 3 `.ts` files are 765/765 finished, 0
  unfinished.
- User Guide, roadmap, and BUILDINFO.md updated to match this cycle's
  menu/Settings reorg (Data from AA's new home, General/Analyzer tabs'
  actual current contents, Custom Analyzer no longer described as
  disabled/under development) and to reflect the Windows port merge
  (build/deploy verified working; real-hardware verification still
  outstanding -- see `docs/windows-port-audit.md`).

## [2.2.4] - 2026-09-06

### Changed

- Settings dialog: "Developer" tab renamed "Analyzer". (#6)
- Measurements: Open/Save replace Import/Export as both the File menu's
  wording and the actual model -- there's one Open dialog (File > Open,
  every supported format including AntScopeZ's own, "All supported
  files" listed first) and one Save dialog (File > Save, or the
  measurements list's right-click "Save as...") for every format
  including AntScopeZ's own now, not a separate quick native-format Save
  button plus a different multi-format Export dialog.
- Measurements list: the 4 buttons below the table (Open/Save/Delete/
  Clear) are gone. Open was already fully covered by File > Open; the
  other 3 (plus Rename and a new Select Color...) are a right-click menu
  on the table now -- Select Color..., Rename..., Save as..., Delete,
  Clear All. The Delete key still deletes the selected measurement.
- Markers list: selection mode changed from cell-select to row-select
  (so right-click context menu picks an entire marker row, not an
  individual cell). Right-click menu added with "Clear All Markers"
  (removes all) and "Clear Empty Markers" (removes only those with no
  valid data across any measurement).
- Settings dialog: tooltips in the General and Scanning tabs wrapped
  for better readability (Analyzer timeout, Report Detailed Errors,
  Use reconnect to drain, Warn before deleting, Points ceiling, Analyzer
  max points, Allow extended chart zoom, Base color). Affects 9 translatable
  strings; see language files if retranslating.
- A measurement now tracks whether it's "dirty" (scanned or renamed
  since it was last saved, or never saved at all) -- shown as a trailing
  " *" in the Points column. Deleting or clearing a dirty measurement
  warns first (one combined warning, not one per item); a new Settings >
  General checkbox, "Warn before deleting or clearing dirty
  measurements" (on by default), controls this.

### Fixed

- Merged from RigExpert's AntScope2 2.0.3 (#10): RigExpert "Match"
  analyzer's max frequency corrected 690MHz -> 750MHz; screenshot
  completion now resets state right away instead of only on dialog
  close; a stale license-request cancel flag could wrongly short-circuit
  a new request.
- Band names could fail to show on a fresh profile's first run despite
  View > "Show Band Name" being checked, until toggled off and back on --
  a QSettings default mismatch. (#8)
- 2-port Z-parameter Touchstone imports (`# ... Z RI ...`) now convert to
  S-parameters correctly instead of being silently skipped. (#7)
- Phase chart: widened Y-axis to +/-190 deg so the 180 deg line isn't
  clipped at the edge. (#4)
- Z=R+jX / Z=R‖jX charts: Y-axis ceiling raised to +/-5000 Ohm to show
  high-impedance points. (#5)
- ITU region band data: fixed 70cm/23cm typo (Region 1/3), added missing
  33cm (Region 2). (#2)
- Windows: the app icon wasn't actually embedded in the built .exe --
  fixed.
- Classic/V2 NanoVNA: status bar no longer gets stuck on "Scanning..."
  after a scan finishes.
- Markers list: columns never widened to fit real data, only their
  header label -- resizeColumnsToContents() only ever ran once, before
  any values existed. Now also runs every time the values actually
  refresh, so a column stays sized to whatever's currently showing
  instead of needing a manual resize each time.
- S21 tab: 3+ measurements loaded at once used to make later ones' traces
  collapse to identical red (`getColor()`'s out-of-range fallback was
  sized for 1 color/measurement, not the S21 tab's 4). Legend now shows
  only the currently-selected measurement's 4 traces (S21/S12 dB+deg),
  not every measurement's at once -- `Measurements::updateS21Legend()`
  follows table-row selection the same way trace pen width already did.
  Selecting a measurement (table click, or a just-finished scan) also
  used to only thicken 1 of its 4 S21 graphs, in the wrong color (copied
  from the SWR chart's own pen) -- now correctly thickens/thins all 4,
  each keeping its own color.
- S21/S12 traces are always solid now, not S21 dashed vs. S12 solid --
  that distinction existed to keep an exactly-overlapping reciprocal
  network's S21 from being fully hidden under S12, back when their
  colors could collapse to the same value (see the red-collapse fix
  above); now that each of the 4 traces reliably gets its own color, a
  thick dashed line was just reading poorly for no remaining benefit.
- Calibrating the Short or Load standard alone (Settings' "Calibrate
  Short"/"Calibrate Load" buttons) never showed the busy indicator (the
  red dot in the plot area's corner) for any of its capture -- only
  "Calibrate Open" did. All 3 now show it the same way.
- Renaming a measurement (the pencil icon) didn't rename it on the S21
  tab's legend -- that measurement's 4 graphs (S21/S12 dB+deg) kept
  showing the name it was scanned with.
- S21 tab: two different measurements' overlapping traces (e.g. one
  green, one red) blended into an unlabeled third color (e.g. orange) --
  every trace was semi-transparent so a reciprocal device's identical
  S21/S12 wouldn't fully hide each other. Traces are fully opaque now
  that S12 defaults off (see below), so the common overlap case mostly
  doesn't reach the chart at the same time to begin with.
- Joining the S21 tab into the Multi tab (right-click it directly ->
  "Move chart to the tab Multi") then using the Multi tab's own "Close
  all"/"Close S21" could permanently strand the S21 tab hidden with no
  way to get it back. S21 is now a fully, symmetrically supported
  Join/Close target, and is always visible to begin with (matching
  every other chart tab) rather than hidden until first used.

### Added

- Screenshot capture for classic NanoVNA, moved into the Analyzer menu
  (Connect / Screenshot / separator / Disconnect). Not yet verified
  against real hardware. (#9)
- New "Analyzer" menu with Connect/Disconnect actions. (#3)
- Windows packaging: NSIS installer, Qt runtime bundling, MinGW build
  presets. Not yet built or verified on real Windows hardware.
- Save dialog: "AntScopeZ" (.asd) is now one of its format buttons
  (listed first) instead of having its own separate quick-save path.
  Same frequency/R/X fields as the other 1-port formats here, just as
  JSON numbers instead of rounded text (not that it matters in
  practice -- real measurement precision is below where that would
  show up). Doesn't store 2-port S21/S12/S22 data at all, so S2P
  actually has more data for a 2-port measurement, not the other way
  around.
- File > Open's dialog offers an "All supported files" combined filter
  covering every format (.asd/.s1p/.s2p/.csv/.nwl), listed first so it's
  the default shown -- previously defaulted to whichever single format
  (S1p) happened to be listed first.
- View menu: independent "Show S21"/"Show S12" toggles for the S21
  tab (default S21 on, S12 off) -- S12 duplicates S21 exactly for any
  reciprocal (passive) device, the common case for this instrument, so
  showing it is opt-in now instead of a permanent transparent overlay.

- Measurements list: the rename pencil column existed only briefly
  (moved left of Name earlier this cycle) before being removed entirely
  -- rename now lives on the right-click menu (see below), same as
  everything else that used to be a column icon or a button.
- S21 tab: selecting a measurement now brings its 4 traces to the front
  of the chart, not just thickening their pen -- previously an older
  measurement's traces stayed underneath every measurement scanned
  after it even while selected. Applies even if the measurement is
  currently hidden, so its traces are already on top the moment its
  visibility checkbox is turned on.
- Settings > General: line-width spinboxes for the selected measurement
  vs. every other loaded one (S21 tab and elsewhere), 1-10px.

### Documentation

- `docs/use-cases.md` and `docs/user-guide.md` brought back in sync with
  everything above (Open/Save wording, right-click measurements menu,
  dirty-flag warning, Show S21/S12 toggles, line-width settings, S21 tab
  always visible/joinable, markers list auto-resize).
- `todo.txt` merged into `docs/use-cases.md` and removed -- one place
  for outstanding work instead of two; resolved items dropped rather
  than kept as pointers (CHANGELOG.md/git log already have the history),
  genuinely open ones folded into the relevant row or a new "Other
  outstanding items" section for the few that aren't use-case-shaped.
  That merged doc renamed `docs/roadmap.md` (it's no longer just a
  use-case catalog) and gained two new planned items: a Settings dialog
  rework (rename "Developer" tab to "Analyzer", add retry/reconnect
  error-handling settings) and an explicit Connect/Disconnect Analyzer
  mechanism (today there's only Connect).
- Translations: ran `update_translations` and filled in this cycle's ~65
  new/changed source strings for Ukrainian, Japanese, and Spanish.
  Separately cleared a much older backlog inherited from this repo's
  first commit -- 310/388/537 (uk/ja/es) strings had sat flagged
  "unfinished" since day one. Most already had solid translation text
  (apparently RigExpert's own original work, just never marked reviewed
  in Linguist's metadata) and were already being compiled into the
  `.qm`/shown at runtime regardless of that flag -- lrelease only drops
  genuinely *empty* translations, of which there were a real handful
  (5/20/20). Wrote those, reviewed the rest for placeholder/newline
  consistency (all clean), and marked everything reviewed. All 3
  `.ts` files are now 742/742 finished, 0 unfinished.

- Settings > General: "Selected measurement line width" / "Other
  measurements' line width" spinboxes (1-10px) -- was hardcoded (5/2).
- A V2/binary NanoVNA scan placed two "lowest SWR" auto-markers per scan
  instead of one: `MainWindow::on_measurementComplete()`'s NanoVNA-family
  early-return only ever checked the classic `ReDeviceInfo::NANO` enum
  value, not `NANOV2` (added later, as its own value), so a V2 scan fell
  through to that function's own marker placement *and* the separate
  `on_measurementCompleteNano()` handler's. Classic ASCII never showed
  this -- it actually matched the check.
- Settings dialog reset every Developer-tab checkbox (Debug Logging,
  Report Detailed Errors, reconnect-to-drain) to unchecked on every
  single reopen, not just once per launch, since the dialog itself is
  reconstructed fresh each time it's opened -- discarding whatever you'd
  just set the moment you closed and reopened it. One of them (Report
  Detailed Errors) additionally force-reset the real underlying flag,
  not just the checkbox -- a functional regression, not just cosmetic.
  Now reads current state instead of hardcoding it off.
- BLE Pings' actual default was `true`, contradicting its own "defaults
  unchecked (hidden)" comment -- it now genuinely starts unchecked.

- Draining always timed out on HID/Serial devices (e.g. Match): the
  default `BaseAnalyzer::stopMeasure()` actually sends a real `"off\r"`
  wire-level abort, so the device genuinely stops producing more data --
  draining then waited for points that were never coming, by design,
  guaranteed to time out. Only `NanovnaAnalyzer`/`NanovnaV2Analyzer`/
  `BleAnalyzer` have no real abort and genuinely need to drain; new
  `BaseAnalyzer::stopCommandAbortsDevice()` (default true, overridden
  false on those three) lets `on_stopMeasure()` skip draining entirely
  for everything else and go straight to "Ready". Confirmed live against
  a real Match device.
- Status bar never returned to "Ready" after a normal (uninterrupted)
  scan completion -- it just sat on the last "Scanning..." message
  until the next scan started.

- Draining after Stop always timed out instead of completing normally:
  `AnalyzerPro::on_stopMeasure()` computed "how many points are still
  outstanding" *after* already zeroing the counter it needed to read,
  so it always waited for the full original point count instead of
  whatever was genuinely still in flight -- confirmed live, a 400-point
  scan stopped partway drained 146/400 and then timed out waiting for
  points that were never coming. Also: a clean drain now shows "Ready"
  in the status bar instead of the confusingly identical wording a
  timeout would show.
- Connect Analyzer always highlighted the first detected device row on
  reopen, even while already connected to a different one -- Connect
  could silently reconnect to the wrong device if clicked without
  checking closely. Now highlights whichever row matches the currently
  selected connection.

### Changed

- "Report Detailed Errors" and "Use reconnect to drain unwanted data"
  moved from Settings > Developer to Settings > General (under Analyzer
  timeout) -- ordinary user-facing preferences, not debug-only ones. Both
  now persist to the ini like the rest of the General tab, instead of
  being session-only.

### Added

- Status bar's scan-progress label now updates when a scan *starts* too
  (all scan types: Single, S21, Continuous, User, One Fq, TDR), not just
  while stopping/draining, and now shows a live "Scanning (N/Total
  points)..." count as each point actually arrives, not just a static
  message shown once at the start.
- New connection-info status-bar label (left side): model, connection
  type (USB/Serial/NanoVNA ASCII/NanoVNA Binary/Bluetooth/BLE), classic
  NanoVNA's own ASCII-vs-binary `scan` capability
  (`NanovnaAnalyzer::scanCapabilityDescription()`), and device serial
  where available. Updates on every connect/disconnect.

- Stopping a scan that's still delivering data (e.g. a large point-count
  sweep) no longer leaves the app looking stopped while it silently keeps
  discarding incoming points in the background: scan-triggering controls
  now stay disabled and a new permanent status-bar label shows draining
  progress ("Stopping — draining remaining data (N/Total points)...")
  until it's genuinely done, bounded by the existing analyzer-timeout
  watchdog so a device that goes silent mid-drain doesn't spin forever.
  Applies uniformly to every stop trigger (Esc, re-clicking Single, closing
  a dialog mid-scan, ...), since they already all funnel through one place
  (`AnalyzerPro::on_stopMeasure()`). Neither the classic ASCII nor the
  V2/LiteVNA64 binary protocol has a wire-level "abort" command, so this is
  fundamentally a wait -- Settings > General's new "Use reconnect to
  drain unwanted data" checkbox (off by default) switches to closing and
  reopening the connection instead, often faster for a large scan, though
  not guaranteed to make every device discard what it already queued.
  Status bar is general-purpose (`MainWindow::m_statusLabel`), not
  draining-specific -- room for more fields later (analyzer type, protocol,
  points in progress, idle).

- NanoVNA V2 / SAA-2 / LiteVNA64 support (`NanovnaV2Analyzer`): the binary
  register+FIFO protocol, distinct from classic NanoVNA/H/H4's ASCII shell.
  Detected via VID/PID `04B4:0008` in Connect Analyzer alongside the
  existing classic NanoVNA entries; distinguishes V2 from LiteVNA64 at
  connect time via a hardware/firmware version register read. Implemented
  independently from protocol facts cross-verified against two real
  clients (NanoVNASaver, libxavna/NanoVNA-QT) -- not a port of either; see
  the class comment in `analyzer/nanovna_v2_analyzer.h` for the licensing
  reasoning. Not yet validated end-to-end against anything -- next step is
  testing against the companion NanoVNA emulator's own binary profile,
  built earlier for exactly this. Connect Analyzer's dev-emulator
  convenience row (see above) is now offered twice, once per protocol,
  since the emulator's pty has no VID/PID to detect which one it's
  currently speaking.
- Stitched (multi-segment, >`g_analyzerMaxPoints`) sweeps against any
  NanoVNA-family device only ever showed the first segment's data: each
  analyzer backend fires its completion signal once per individual segment
  on the way to being stitched into one result, not just on the truly last
  one, and MainWindow's completion handlers unconditionally finalized
  (`setIsMeasuring(false)`, UI resets, ...) on every one of them --
  discarding every later segment's points as "stale leftover data" the
  moment `AnalyzerPro::on_newData()`'s own guard saw measuring had already
  stopped. New `AnalyzerPro::isStitchedSweepComplete()` lets MainWindow
  tell an intermediate segment boundary apart from the real end.
- Reconnecting to a different analyzer type at the same port path could
  fail every write with "device not open": `AnalyzerPro::createDevice()`
  left the previous analyzer's serial port to close itself via its
  deferred `deleteLater()` destructor, but the new analyzer's own
  `connectAnalyzer()` tries to open a port synchronously, in the same call
  -- a race invisible with real hardware (never the same port path back to
  back under a different protocol) but trivial to hit with a single
  multi-protocol dev target at a fixed path. Now closes the old port
  synchronously first.
- Connect Analyzer silently reopened on the USB tab instead of COM whenever
  the last connection was to any NanoVNA-family device (classic or V2) --
  the "restore last-used tab" switch had no case for `NANO`/`NANOV2`, so
  both fell through to USB's default. Pre-existing gap for classic `NANO`,
  not something the V2 work introduced, just newly exposed by actually
  reopening the dialog after a NanoVNA-family connection.
- Point-count lock ("100 points, field disabled") that's meant to be
  classic-NanoVNA-specific was keyed on a `name.contains("NanoVNA")`
  substring check, which incorrectly also caught "NanoVNA V2" (and missed
  "LiteVNA64") the moment those names existed. Now keyed on
  `connectionType() == ReDeviceInfo::NANO` instead.

### Fixed

- TDR Measurement dialog run against a NanoVNA-type device could get stuck
  permanently: the progress dialog (frameless, window-modal, Esc disabled)
  never closed and the panel's Scan button never re-enabled, whether the
  scan finished normally or was stopped early -- both NanoVNA completion
  paths skipped the TDR finalize step entirely. No UI recovery once hit;
  had to kill the process.
- TDR Measurement's Result groupbox (peak distance/reflection type/reverse-
  solve) never refreshed after a normally-completed NanoVNA TDR scan --
  only after one stopped early -- because it only listened for the signal
  variant NanoVNA's normal completion never emits.

### Added

- Dev-only: Connect Analyzer's COM list offers a "NanoVNA (dev emulator)"
  entry, pointing at a companion emulator app
  (`~/QT6Projects/NanoVnaEmulator`) over a local pty -- shown only while
  that emulator is actually running (checks for its symlink at
  `/tmp/nanovna-emulator`), invisible otherwise.

### Added

- Native Windows support: AntScopeZ now builds and runs natively on
  Windows (Qt 6.11.2, MinGW), packaged as an NSIS installer
  (`AntScopeZ-<version>-win64.exe`).
- File > "Disconnect Analyzer" menu action, next to Connect Analyzer.
- Settings > Themes: an Apply button to activate the selected theme
  immediately, instead of only loading it into the editor.
- About dialog: credits K4HEZ (creator) and KC5CD, with a link to
  RigExpert's AntScope2.
- `-developer`-gated CLI flags (`-comserial`/`-usbhid`/`-nanovna`/`-ble`)
  to enable Debug Logging at launch instead of only via Settings.

### Changed

- NanoVNA: no longer hard-locked to 100 scan points -- uses the same
  configurable points field/slider (and app-wide maximum) as every
  other analyzer.
- Band Selector now defaults to enabled at startup (a deliberate
  stopgap while the real cross-restart configuration behavior is
  decided).
- Analyzer connections: consolidated "device busy" and scan-timeout-
  watchdog handling across HID, Serial, and NanoVNA connections into
  shared logic, closing a few gaps where a stuck scan could leave the
  watchdog un-armed.

### Fixed

- Windows: taskbar/Start-menu icon was missing (a Qt 6.11.2 quirk with
  the public build API silently skipping icon generation); now shows
  correctly.
- Amateur band definitions: 60m corrected in all three ITU regions (US:
  5 channelized lines instead of one inaccurate wide range; IARU:
  5351.5-5366.5kHz), and the previously-missing 33cm band added for
  Region 2 (US/Canada). Also fixed a 70cm/23cm label typo in Region 1/3.
- Band preset dropdown wasn't updating the S21 chart's X-axis range
  along with every other chart.
- TDR: a NanoVNA TDR scan left the dialog's Cable Type/Velocity
  Factor/Points controls disabled after one measurement, with no way to
  change settings or re-scan without closing the dialog.
- Smith chart: the cursor position dot stayed at its last position from
  a previous scan or marker click instead of resetting on a new scan or
  Clear.
- Start/Stop frequency fields didn't apply their value when clicking
  elsewhere (only Enter or moving to another field worked); also fixed
  a duplicate-connection bug that ran the update logic twice per edit.
- NanoVNA: fixed a 0Hz first point and a race on the first Single-click
  right after connecting.
- Various NanoVNA edge cases: leftover S-parameter points leaking into
  a measurement after Stop, marker auto-placement using stale data
  after an empty/cancelled scan, a binary-scan-mismatch error skipping
  completion cleanup, and a stale-row bug in the device selection
  dialog's Connect button.
- SWR chart: vertical click-drag disabled (only Ctrl+scroll zooms the Y
  axis) to avoid accidental rescaling.
- Installer: fixed a missing `ftd2xx.dll` and a broken `ProgramData`
  data-file path in the Windows installer.

### Fixed

- NanoVNA: the "scan"-command point-count clamp (50-401, see above) was
  applied before picking between the "scan" and classic "sweep" device
  commands, so it also silently truncated fallback-protocol requests --
  clamp now only applies to the "scan" path.
- Markers list: "Clear Empty Markers" had become unreachable -- the new
  right-click "Clear All" put a `CustomContextMenu` policy on the table,
  which swallows the context-menu event before the panel's own
  `contextMenuEvent()` (where "Clear Empty Markers" lived) ever sees it.
  Folded into the same menu instead.
- Band Selector's "always start enabled" override (issue #23) only lasted
  until the next band-region switch -- it never wrote the forced value
  back to the ini, so `populateBandSelector()` re-hid it on the next
  region change using the still-stale persisted value.
- Remote API: connecting to a HID analyzer sent a duplicate "connected"
  event to every subscribed client -- `cmdConnect()`'s manual broadcast
  raced HID's own synchronous `analyzerFound` emission.
- Settings dialog silently turned BLE "Show Pings" back off every time it
  was reopened, even while actively enabled, contradicting its own
  "not a hardcoded reset" comment; now only forced off when BLE/Bluetooth
  logging itself is off, matching the checkbox's actual state.
- About dialog: the blue/yellow flag labels had been recentered and given
  a hardcoded pixel size, losing their original left/right-justified
  banner look -- restored, and now sized to 80% of the credits text
  box's width instead of a fixed size, so they scale with the dialog.

## [2.2.3] - 2026-09-01

### Added

- NanoVNA: real 2-port S11+S21 on every scan (was S11-only), plus an
  opportunistic ASCII/binary `scan` fast path when the firmware supports
  it. Not yet validated against real hardware.
- Analyzer scan-timeout watchdog: a scan that goes silent for too long
  (device unreachable, or busy -- already held open by another program or
  another AntScopeZ window) now fails with an error instead of leaving
  the busy indicator/wait cursor stuck forever. Timeout is configurable,
  Settings > General > "Analyzer timeout" (default 8s).
  USB/HID and Serial connections also now detect "device present but
  busy" specifically, both at launch and while polling for a device.
- Analyzer errors now show in a proper non-modal dialog (was a small
  banner too short to hold a real sentence) -- timestamped, includes
  whatever diagnostic detail is available (OS error string, Qt error
  enum, elapsed-silence duration, raw bytes on a CRC mismatch). BLE's
  own connection/protocol errors, previously compiled out entirely
  outside a debug build, are now available at runtime behind Settings >
  Developer > Error Reporting & Logging > "Report Detailed Errors" (off
  by default).
- Help > User Guide: a lightweight, non-modal in-app viewer for
  `docs/user-guide.md` (no HTML rendering engine) -- the guide's own
  self-links (jumping to a section) work.

### Changed

- `SUPPORTED_DEVICES.md` merged into `docs/user-guide.md` as its own
  section, so it's reachable from the in-app viewer above; the
  standalone file/page is gone (index.md, README.md, and the GitHub
  Pages nav all point at the merged section instead).

- Markers table: docked under the plot tabs (a resizable splitter) as a
  normal themed table, instead of a floating semi-transparent popup.
  Visibility now follows the View > Markers Hint checkbox alone --
  toggling it on shows the table (headers only, if empty) immediately,
  rather than only once a marker exists.
- Measurements list: a scan that ends with zero points (cancelled,
  errored, or timed out before anything arrived) is no longer left
  behind as an empty entry.

### Fixed

- File dialogs: "Look in:" now shows translated (es/ja/uk) instead of
  falling back to English -- same class of gap, and same fix, as the
  "Files of type:" label fixed earlier (a corrected-mnemonic override
  shipped alongside Qt's own translation, since Qt's own upstream
  catalog has the same exact-string mismatch for this one too).
- Esc / re-clicking Single or Continuous now actually stops the scan:
  previously, data still arriving from a device with no real wire-abort
  command (e.g. BLE) kept getting processed as if the scan were still
  running -- corrupting whichever measurement was still open, and, for
  Single, repeatedly re-placing the "lowest SWR" auto-marker for every
  leftover point (up to one per free marker slot).
- Continuous mode: stopping it now updates the Measurements list's
  Points column immediately, instead of only catching up whenever the
  next scan happened to rebuild the table.
- A marker placed right as a chart's X axis is still unscaled (e.g. just
  after connecting/reconnecting, before a scan has plotted anything)
  could crash the app on the next click anywhere on that chart.
- App could crash on close after having connected to an analyzer (double
  free of the docked markers table).
- The "Image added to clipboard" notification (Screenshot dialog) was
  unreadable in every theme -- a missing `;` in its stylesheet silently
  dropped the intended text color entirely. Also now follows the active
  theme instead of a fixed dark box.

## [2.2.2] - 2026-08-28

### Added

- Measurements table: Points column now tags each row "(s1p)"/"(s2p)" so
  1-port vs. 2-port measurements are distinguishable at a glance.
- Export dialog: shows the selected measurement's name/points/type, and
  offers new S2P RI/MA/DB Touchstone exports -- real S11/S21/S12/S22,
  only shown for a measurement that actually has 2-port data. The
  existing 1-port S-parameter export also gains a matching S,DB option
  (previously RI/MA only, same as S2P).
- Tools > Marker Comparison: compare two markers' frequency/SWR/return
  loss/R/X, estimated Q and equivalent L/C off the Current marker, and
  estimate how much to trim (or add) to a simple 1/4-wave vertical,
  1/2-wave dipole or full-wave antenna to move resonance from one marker
  to the other.
- Settings > Markers: "Automatically set a marker at the lowest SWR" (Marker
  behavior group, on by default) places a marker at the swept trace's
  lowest-SWR point right after a single/full scan finishes -- never during
  a Continuous scan, and only if a marker slot is free.
- Tools > TDR Measurement: one dialog for both setting up and reading a
  TDR scan (replaces the standalone Single-button-on-the-TDR-tab trigger
  and the separate Tools > TDR Analysis dialog, both retired). Scan
  setup: cable type/velocity factor (genuinely drives that scan's own
  distance calculation, but only for that one scan -- doesn't touch
  Settings > Cable unless applied explicitly, see below), top frequency,
  points, a live unambiguous-range/resolution estimate shown before
  scanning, and a window-function picker (Rectangular/Hamming/Hann/
  Blackman/Kaiser, Hamming default) that re-plots an already-captured
  scan live with no rescan needed. Result (after scanning): distance to
  the strongest reflection, open/short with an approximate impedance in
  Ohms (a rough estimate, not a precision figure), an automatic note when
  a peak falls short of an entered known cable length (possible fault
  partway along vs. just the far end), and a reverse calculator that
  solves for velocity factor given a known physical cable length. "Use
  this velocity factor" copies the solved value into Scan setup's
  velocity factor field and applies it to Settings > Cable as Custom
  (resetting R0/loss to the "Ideal 50-Ohm cable" convention -- 50 Ohm, no
  loss -- instead of leaving whatever a previously-selected Preset's real
  figures were showing), refreshing the TDR chart's distance axis
  immediately. If Settings > Cable happens to already be open, it updates
  live instead of only taking effect the next time the dialog is opened.
  No Continuous mode -- TDR is one scan at a time.
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
- Measurements table: new "Points" column shows the actual number of
  points received for each scan ("--" until it finishes) -- a device
  silently returning fewer points than requested is now visible at a
  glance instead of requiring a debug-log read.
- Settings > General: new "Scanning" group -- "Scanning points maximum"
  sets the Points field/slider's real practical ceiling (50-10000,
  replacing what used to be a fixed, recompile-only limit); "Warn for
  scans above" pops a Cancel-able confirmation before starting a scan that
  requests more points than this; "Analyzer maximum number of points"
  caps how many points a single sweep actually sends to the device --
  requesting more than that transparently splits the scan into several
  sequential sweeps ("stitching") and concatenates the results into one
  continuous dataset. Default 1000/1000/1000 -- the first two match prior
  behavior exactly, but a scan above 1000 points now stitches by default
  rather than needing to be turned on. Applies to Single/Continuous/User
  scans; TDR, Calibration, and S21 are unaffected for now.

- Settings > General: "Allow extended chart zoom" checkbox (off by
  default) lets Ctrl+scroll/Ctrl+`+`/`-` zoom the SWR, Z=R+jX, Z=R‖jX,
  and RL charts' Y-axis past their normal preset floor/ceiling.
- One Fq mode's floating readout gains a second style: double-click it
  (or its alternate) to swap between the original packed 11-field
  technical view and a new plain, resizable dialog showing just SWR as
  one giant bold number ("x.xx:1") with a small "SWR" caption, auto-sized
  to fill on resize -- a glanceable tuning aid readable from across a
  yard. Both styles stay live-updated the whole session regardless of
  which is shown, so toggling never displays a stale value. The chosen
  style is remembered across restarts and tracks the main window's
  minimize/restore state.

### Changed

- Settings > General: "Register application" button and the "Match
  license" groupbox (Register device/Update license/Device info) moved to
  the Updates tab -- they're update/registration-related, not general
  settings. No behavior change, just relocated.
- Settings > General: removed the "Connect analyzer" button (redundant
  with the main window's own Connect Analyzer entry points) and "Open
  'Connect Analyzer' on launch" checkbox (moved to the Connect Analyzer
  dialog itself, next to "Use same selection for future connections" --
  both are about that dialog's own behavior, not general app settings;
  same underlying setting/ini key, so an existing choice carries over
  unchanged).
- Settings > Cable's locked (Preset) fields now show dimmed text instead
  of full-contrast -- the intent was always "flatten into the background
  to read as locked", but keeping full-contrast text while an actually-
  editable field only gets a mild fill tint made locked fields look more
  prominent than editable ones, backwards from what "locked" vs.
  "editable" should communicate. Plain muted text undershot the other
  way (tuned for disabled hint text where illegibility is fine, not a
  value the user still needs to read) and came out too close to its own
  background, so this is a blend 70% back toward full-contrast text
  instead of the theme's muted color as-is.
- Settings dialog's minimum size is now computed from its own layout
  (every tab's real minimum) instead of a hand-picked 550x320 that
  predated several tabs growing past it -- dragging the dialog down to
  its old stated minimum could squash controls unreadable instead of
  actually stopping there.
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
- Consolidated all theme-driven styling into one `Style::globalStyleSheet()`
  applied to the whole app from exactly two places (startup, theme change)
  instead of ~150 scattered per-dialog `setStyleSheet()` calls that could
  silently go stale while a dialog was open across a theme switch. No
  behavior change for a theme picked before opening a dialog; open dialogs
  now restyle live instead of only on next open.

### Fixed

- 2-port Touchstone import: a `Z, RI` 2-port file no longer gets its
  Z21/Z12/Z22 columns silently mislabeled as S21/S12/S22 (different
  physical quantity, not converted) -- now just skipped, same as before
  2-port import existed, rather than shown as wrong data.
- Settings > General's Data Folder "Browse..." button opened a plain file
  picker (Open/Cancel, files selectable, clicking a folder navigated into
  it instead of choosing it) rather than a proper directory chooser --
  FileDialog::getExistingDirectory() never actually set FileMode::Directory
  despite its name. Also folded in ShowDirsOnly/DontUseNativeDialog via
  the same setOptions() call that sets it now, since setOptions() replaces
  the whole flags set rather than merging with prior setOption() calls --
  DontUseNativeDialog was silently getting wiped out by it too.
- Settings > Cable's Export button crashed with no measurement data yet --
  it always passed size()-1 (-1, wrapping to 4294967295 through a quint32
  parameter) as the measurement index to export, an out-of-bounds access
  as soon as Export needed a suggested filename. Now shows a "run a scan
  first" notification instead of opening Export at all in that case.
- m_settingsDialog could go dangling if Settings was closed via the native
  window decoration/Alt+F4 instead of its own Close button -- only that
  button's handler ever nulled the pointer; WA_DeleteOnClose's deferred
  deletion from any other close path left it pointing at a freed object.
  Added the same destroyed()-nulls-the-pointer safety net the newer
  Marker Comparison/TDR Analysis dialogs already had.
- Opening Settings (or pressing Esc) while no scan was running dropped a
  spurious second "lowest SWR" auto-marker on top of the real one --
  both send a blanket "stop measuring, just in case" signal that
  AnalyzerPro::on_stopMeasure() turned into a measurementComplete()
  emission unconditionally, even when nothing was actually measuring.
  That signal is wired to the same handler a real scan's completion uses,
  auto-marker placement included. Now only emitted if a measurement was
  genuinely in progress.
- Presets table (main window) didn't fill its groupbox the way the
  Measurements table beside it does -- AdjustToContents sized it off its
  own column widths (360px) regardless of the ~314px the box actually had
  to give it, and horizontalHeaderStretchLastSection was off, so it fought
  its container instead of filling it. Matched Measurements' proven config
  (AdjustIgnored, stretch-last-section, cascading resizes) and dropped an
  explicit minimumSize that was fighting the new stretch behavior once
  added. Also removed an extra leftMargin/rightMargin layer one level of
  nested layout added around the Presets column that neither the
  Frequency nor Measurements groupbox has, which was the actual source of
  the visibly wider whitespace around Presets' controls.
- Measurements table's "Points" column reverted a finished scan's point
  count back to "--" after closing Settings. Closing it always signals a
  system-impedance change, which rebuilds every existing measurement's
  graphs/data -- the rebuild never restored the point count it briefly
  cleared, even though the underlying data was intact throughout.
- Markers table's Z (Ohm) column occasionally showed a marker's impedance
  doubled up, e.g. "42.93-j11.1942.93-j11.19". Whenever a marker sat
  exactly on a swept frequency point -- always true for an auto-placed
  lowest-SWR marker -- the value got appended twice instead of once.
- Tools > Marker Comparison: pressing Enter in "Current length" closed the
  whole dialog instead of just committing the field -- Close was the
  dialog's only button, so it defaulted to acting as the Enter target.
- "One Fq" mode (set Start equal to Stop, or Range to 0, then Single or
  Continuous) -- a live single-frequency readout that's been reachable
  this whole time, just undocumented: could abort the app outright, could
  silently restart itself in a loop instead of stopping after one
  reading, and left the Measurements panel's Save/Delete/Clear buttons
  disabled even after stopping. Single and Continuous now behave
  consistently (one reading vs. live-until-Esc, respectively) and stop
  cleanly either way.
- Settings > Developer > Custom Analyzer's "Don't restrict frequency"
  checkbox displayed with inverted polarity the first time Settings
  opened (showed checked when frequency restriction was actually still
  active) -- self-corrected after one click, but the initial state was
  backwards.
- Ctrl+0 did nothing on the SWR chart tab specifically -- every other
  chart tab's Ctrl+0 (reset Y-axis zoom to default) already worked.
- Settings > Developer's "BLE Pings" checkbox defaulted to checked, adding
  BLE's once-a-second keepalive noise to every BLE debug-log capture
  unless manually turned off first.

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
