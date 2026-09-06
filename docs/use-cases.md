---
layout: default
title: Use Case Catalog
---

# AntScopeZ use-case catalog (working doc)

Not end-user documentation -- that's `docs/user-guide.md`, organized by
*feature*. This is organized by *goal* ("I want to accomplish X"),
specifically to drive feature planning: for each real thing a user
might be trying to do, what's the procedure today, and does AntScopeZ
actually cover it yet. Pulled together 2026-08-19 as prep work before
planning S22 exposure, so gaps get spotted deliberately instead of
being discovered one bug report at a time.

Status legend: ✅ works today -- ⚠️ partially/awkwardly -- ❌ not
possible today.

## Antenna / one-port measurement

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 1 | Check SWR/return loss at my operating frequency | Set Start/Stop, Single scan, read the dip/marker | SWR, RL | ✅ |
| 2 | Find an antenna's actual resonant frequency | Read the SWR dip, or where X crosses zero | SWR, Z=R+jX | ✅ |
| 3 | Trim an antenna to a target frequency | Place Current + Target markers, read suggested trim | Tools > Marker Comparison | ✅ |
| 4 | Design a matching network for a mismatched antenna | Read R/X (series) or Rp/Xp (parallel) at target frequency, or trace shape on Smith | Z=R+jX, Z=R‖jX, Smith | ✅ |
| 5 | Determine antenna Q / usable bandwidth | Marker Comparison's Q field, or eyeball Phase-crossing steepness | Phase, Marker Comparison | ✅ |
| 6 | Compare two antennas/configurations side by side | Check multiple Measurements rows visible on one chart | any chart, Multi | ✅ |
| 7 | Watch a match improve in real time while physically adjusting it | Continuous scan mode | SWR (or any chart) | ✅ |
| 8 | Log/export a measurement for a report or later reference | Export (CSV/NWL/Touchstone), Save (.asd), Print, Save Screenshot | File menu | ✅ |
| 9 | Retrieve scans already stored on the analyzer itself | Data from AA | File > Data from AA | ✅ |
| 10 | Correct for the analyzer's own measurement error | OSL calibration, then check the Calibration box | Settings > OSL Calibration | ✅ |
| 11 | Target scans to a specific ham band quickly | Band selector fills Start/Stop | Presets panel | ✅ |
| 12 | Find a cable fault or verify cable integrity | Run a TDR scan, read the reflection (distance + impedance in Ohms, not just open/short) | TDR, Tools > TDR Measurement | ✅ distance/open-short reliable; ⚠️ the Ohms figure is a rough estimate, not precision -- confirmed 2026-08-21 against a real open 13ft cable (101Ω, then 771Ω after a real bug fix, still not fully convincing) -- real cable loss, the settling-point calculation, and window-function choice all affect the number independently. See [[tdr-scan-rework-plan]]. |
| 13 | Determine an unknown cable's length or velocity factor | Reverse-solve calculator (known length -> velocity factor) | Tools > TDR Measurement | ✅ |
| 14 | Figure out if bad SWR is the antenna or the feedline | Enter the known feedline length -- automatically flags "peak is short of that length" (possible fault partway along) vs. it just being the far end | Tools > TDR Measurement | ✅ (automated 2026-08-21 -- was a manual eyeball comparison before) |
| 15 | Share/document a chart as an image or PDF | Print, Save Screenshot, Screenshot from AA | File menu | ✅ |
| 16 | Overlay markers directly on the TDR (distance) chart | -- | -- | ❌ deferred -- see [[tdr-analysis-roadmap-deferred]] |
| 17 | Pick a TDR window function (Hamming/Hann/etc.) instead of the fixed default | Window combobox (Rectangular/Hamming/Hann/Blackman/Kaiser), live re-plot, no rescan | Tools > TDR Measurement | ✅ (shipped 2026-08-21) |
| 39 | See every significant reflection in a scan, not just the single strongest one (e.g. two connectors plus the far end) | -- | -- | ❌ feature suggestion, 2026-08-21, not scoped -- needs real peak-finding (local maxima above the noise floor), not just a global max. See [[tdr-scan-rework-plan]]. |
| 40 | Compare a TDR scan against an earlier one of the same cable (did a repair actually change anything) | Select multiple Measurements rows, eyeball both traces on the same chart | TDR (multi-select already works) | ⚠️ possible today by overlaying rows manually; no dedicated before/after diff (distance/amplitude deltas called out explicitly). Feature suggestion, 2026-08-21, not scoped -- would need a second-measurement picker UI. See [[tdr-scan-rework-plan]]. |
| 18 | Type "6ft"/"1.5M" into a field instead of the field's fixed unit | -- | -- | ❌ planned, not started (see the unit-shorthand-input plan) |
| 29 | Scan a range wider than one sweep/device supports (segmented/stitched sweep) | Settings > General: set "Analyzer maximum number of points" to the device's real limit; requesting more points than that transparently splits the scan into several sequential sweeps and concatenates them into one continuous dataset | any 1-port chart | ✅ (shipped -- `g_analyzerMaxPoints`, `AnalyzerPro::buildStitchSegments()`; confirmed 2026-08-18 against a real RigExpert Match at 1000/2000/10000 points). Real bug found+fixed 2026-09-03 against NanoVNA-family devices specifically: each segment's completion signal was treated as *the* completion, so only the first segment's data ever showed -- see `AnalyzerPro::isStitchedSweepComplete()`. |
| 36 | Watch SWR/Z settle at one specific frequency live, without a full sweep redrawing around it | Set Start=Stop (or Range=0), Single or Continuous | Smith chart tracer + a floating readout box | ✅ (undocumented until 2026-08-20 despite being reachable all along; three real bugs -- a crash, a silent restart loop, disabled panel buttons -- found and fixed the same day) |
| 37 | Get a large, glanceable SWR readout visible from a distance (e.g. up a ladder, tweaking an antenna, reading the screen across a yard) | Double-click the One Fq floating box (or its alternate) to swap to a plain, resizable dialog showing just SWR as one giant bold number ("x.xx:1") with a small "SWR" caption, auto-sized to fill on resize | One Fq mode | ✅ (shipped in 2.2.2, released 2026-08-28 -- `OneFqDisplayStyle::BigReadout` finally rendered. Both styles stay live-updated regardless of which is shown; the choice is remembered across restarts and tracks the main window's minimize/restore state.) |

## 2-port / transmission measurement (`.s2p` import, plus live NanoVNA-family S11+S21 -- classic ASCII since 2026-08-31, validated against real NanoVNA-H4 hardware 2026-09-02; V2/binary since 2026-09-03, emulator-only so far)

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 19 | Verify a filter/attenuator's passband or insertion loss vs frequency | Import a `.s2p`, read S21 magnitude -- or, on NanoVNA-family hardware, a live scan now populates real S21 directly (unvalidated, see row 28) | S21 tab | ✅ (shipped 2026-08-19; live NanoVNA path added 2026-08-31) |
| 20 | Sanity-check S21≈S12 (reciprocity) for a passive device | Both traced on the same S21 tab (dashed/solid) | S21 tab | ✅ for imported `.s2p` data. Not meaningful on a live NanoVNA scan -- that path is forward-only and hardcodes S12 to 0 (see row 28), so it can never actually show S21≈S12, only S21 against a flat zero line. |
| 21 | Track a frequency point's S21/S12 across the Markers table / cursor hover | Marker Comparison table columns, Cursor Details | S21 tab, Markers | ✅ (shipped 2026-08-19). Same caveat as row 20 -- the S12 column reads real data from an imported `.s2p`, but is always 0 on a live NanoVNA scan. |
| 22 | Check the DUT's *output*-port match (S22) | -- | -- | ❌ deferred -- and now a *double* gap, not a single one: no live-capture path produces real S22 either (NanoVNA's row 28 addition is forward-only S11+S21, S22 hardcoded to 0 in `nanovna_analyzer.cpp`), and even an imported `.s2p` file's real S22 is parsed and re-exportable but never surfaced anywhere in the UI (no chart, no column) |
| 23 | Sanity-check a device is electrically symmetric (S11≈S22) | -- | -- | ❌ same gap as above |
| 24 | Design a matching network for a 2-port device's output side | -- | -- | ❌ same gap |
| 25 | Read group delay / phase linearity through a device | -- | -- | ❌ explicitly deferred in the 2-port plan |
| 26 | Compare S21 alongside another chart in Multi view | -- | -- | ❌ S21 is explicitly excluded from Multi's join menu (`mainwindow_multitab.cpp`) |
| 27 | Export imported 2-port data back out (S11/S21/S12/S22), in RI, MA, or DB | Export dialog's S2P RI/MA/DB buttons (only shown for a 2-port measurement) | File > Export | ✅ (shipped 2026-08-19, `exportSParamData()`) |
| 28 | Capture 2-port data live from real hardware | NanoVNA-family (classic ASCII *and* V2/binary): `data 1` after the normal S11 pass (classic), or S11+S21 together per FIFO record (V2), plus an opportunistic ASCII/binary `scan` fast path where classic firmware supports it | any 1-port chart + S21 tab (NanoVNA only) | ⚠️ split by device family. **NanoVNA-family: ✅** classic ASCII shipped 2026-08-31, since exercised against real NanoVNA-H4 hardware (two real bugs found and fixed 2026-09-02 -- see [[nanovna-two-port-work-deferred]]); V2/binary (`nanovna_v2_analyzer.cpp`) shipped 2026-09-03, so far only tested against the companion NanoVNA emulator, not real V2/LiteVNA64 hardware. Both forward-only: S12/S22 are hardcoded to `(0,0)`, not a full 4-parameter capture. **RigExpert-family: ❌** still a dead end on this user's hardware -- `FDB` (S21) *and* `EFRX` (User Defined tab) both return "Error.Not recognized" on the only device tested (RigExpert Match RFE, confirmed 2026-08-19/20). See [[s21-and-user-defined-live-capture-deferred]]. (Row 29's stitching still doesn't reach either path -- `on_measureUser()`/EFRX already routes through `startStitchedMeasure()`, `on_measureS21()`/FDB doesn't, and NanoVNA's own scan pipeline is separate again.) |
| 35 | Import a 2-port *Z-parameter* Touchstone file (`# ... Z RI ...`, 9-value rows) correctly | -- | -- | ❌ real gap found 2026-08-19 -- Z21/Z12/Z22 are a different quantity than S21/S12/S22 (would need actual Z-to-S 2-port matrix conversion); guarded against silent mislabeling (skipped, not shown as wrong data) rather than fixed. S-parameter 2-port files (the overwhelmingly common case) are unaffected. |
| 34 | Tell, at a glance, which Measurements-list rows are 2-port (have real S21/S12/S22) vs. plain 1-port | Points column now reads e.g. "100 (s1p)"/"300 (s2p)" | Measurements panel | ✅ (shipped 2026-08-19; column-width formatting for large point counts still to be revisited) |

## App reliability / diagnostics

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 30 | Understand why a scan is slow or appears hung | A scan that goes silent past Settings > General > "Analyzer timeout" (default 8s) now fails with a proper non-modal error dialog -- timestamped, with whatever diagnostic detail is available -- instead of leaving the busy indicator/wait cursor stuck forever. Stopping a scan that's still delivering data (neither NanoVNA protocol has a wire-level abort) now visibly shows "draining" progress in the status bar and disables scan controls until it's genuinely done, instead of looking stopped while still silently discarding incoming points -- bounded by the same watchdog | Settings > General, analyzer error dialog, status bar | ✅ (watchdog shipped 2026-09-01 -- `AnalyzerPro` scan-silence watchdog; USB/HID and Serial connections also now detect "device present but busy" specifically, both at launch and while polling. Drain-aware stop shipped 2026-09-03/04 -- `AnalyzerPro::beginDraining()`/`stopCommandAbortsDevice()`, optional "Use reconnect to drain unwanted data" in Settings > General) |
| 31 | Reliable auto-reconnect after a dropped/power-cycled analyzer | -- | -- | ⚠️ known gap, unchanged -- row 30's "device present but busy" detection helps diagnose a stuck connect attempt but doesn't make the reconnect itself more reliable |
| 38 | Get remote help from a more experienced ham -- let them drive your analyzer software over the network while you handle the physical antenna/hardware end | -- | -- | ❌ parked idea (2026-08-20), not scoped or designed -- see [[remote-network-control-idea]]. Would supersede, not extend, an existing but abandoned narrow UDP bridge (see [[s21-and-user-defined-live-capture-deferred]]-adjacent findings in `BUILDINFO.md`). |

## Visual / UI

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 32 | Tell overlapping measurement traces apart at a glance | `getColor()` palette | any multi-trace chart | ⚠️ known collision bug, task #11 -- "not ready to fix it" |
| 33 | Keep the Markers table visible without it floating awkwardly over the chart | Docked into a resizable splitter under the plot tabs as a normal themed table; visibility follows the View > Markers Hint checkbox alone | Markers panel | ✅ (shipped 2026-09-01 -- `MarkersPopUp`, a translucent floating `Qt::Tool` window, retired in favor of `MarkersPanel`, a plain child widget styled for free by the app's existing Fusion/palette theming) |

## Notes

Rows 22-24 (S22) are the reason this doc exists -- nothing lets a user
look at a 2-port device's output port at all, not a workaround-but-
clunky case, a flat ❌. Row 27 (2-port export) and row 26 (Multi view)
are adjacent gaps worth keeping in view when S22 does get designed --
whatever data model it uses should be export- and Multi-view-compatible
without another rework. Row 34 (the Points-column s1p/s2p tag) turned
out to be a real prerequisite for row 27, not just adjacent to it --
`Export::updateDetails()` reads that tag to decide whether to show the
S2P button at all.

## Changelog cross-reference

Rows above get their "shipped"/"fixed" status kept in sync with
`CHANGELOG.md` as work lands; this section is just a quick index into
*why* a given row changed, for anything not obvious from the row itself.

- **2026-08-21, TDR rework:** dedicated `Tools > TDR Measurement` dialog
  (`TdrScanPanel`/`TdrScanDialog`), window-function picker (row 17),
  automatic fault-vs-known-cable-length flag (row 14), impedance (Ohms)
  readout (row 12 -- accuracy is a still-open question, not a quick win).
  Full history, including two hardware-confirmed bugs found along the
  way, in the `tdr-scan-rework-plan` memory.
- **2026-08-31, NanoVNA live 2-port:** real S11+S21 requested on every
  NanoVNA-family scan (`data 1`, plus an opportunistic ASCII/binary
  `scan` fast path), row 28's NanoVNA half. Shipped ahead of hardware
  validation -- see [[nanovna-two-port-work-deferred]].
- **2026-09-01, released as 2.2.3:** scan-silence watchdog + a proper
  error dialog (row 30); Markers table docked into the main window
  (row 33); plus assorted reliability fixes not tied to a specific row
  -- Esc/Stop actually stopping a scan, a marker-placement crash, a
  double-free on close, Continuous mode's Points column updating on
  stop, empty scans no longer left in the Measurements list -- and an
  in-app Help > User Guide viewer.
- **2026-09-02, NanoVNA-H4 hardware arrives:** the classic ASCII NanoVNA
  path exercised against real hardware for the first time; two real bugs
  found and fixed (0 Hz reported for the first swept point; a race on the
  first Single-click right after connecting, root-caused live via gdb).
  See [[nanovna-two-port-work-deferred]].
- **2026-09-03, NanoVNA V2/LiteVNA64 + assorted NanoVNA-family fixes:**
  binary register+FIFO protocol support (`nanovna_v2_analyzer.cpp`, row
  28's V2 half), built and verified against a companion NanoVNA emulator
  (ASCII and binary, plus fault injection) rather than real V2 hardware --
  Connect Analyzer gained a dev-only "(dev emulator)" row for this, shown
  only while the emulator's actually running. Testing against it surfaced
  several real, pre-existing bugs: a stitched-sweep race that only ever
  showed the first segment's data on any NanoVNA-family device (row 29);
  a TDR scan against a NanoVNA-family device that could get permanently
  stuck (Stop unresponsive); TDR's Result groupbox not refreshing after a
  normally-completed NanoVNA scan; a port-reuse race reconnecting to a
  different analyzer type at the same port path.
- **2026-09-03/04, drain-aware scan stop:** stopping a scan mid-flight
  now visibly drains instead of looking hung (row 30's second half).
  Several real bugs found testing it against the emulator: draining
  always timing out on devices that actually do have a wire-level abort
  (HID/Serial, e.g. Match); a status-bar ordering bug that always
  computed "how many points remain" as the full original count instead
  of what was genuinely still in flight; Settings dialog force-resetting
  every Developer-tab checkbox (and, for one of them, the real
  underlying flag too) on every single reopen, not just once per launch.
- **2026-09-04, Settings reorg + a real 2-marker bug:** "Report Detailed
  Errors" and "Use reconnect to drain unwanted data" moved from
  Developer to General tab, now persisted to the ini instead of
  session-only; BLE Pings' own default fixed (was `true`, contradicting
  its own "defaults unchecked" comment). Also found: a V2/binary scan
  placed two "lowest SWR" auto-markers per scan instead of one --
  `MainWindow::on_measurementComplete()`'s NanoVNA-family early-return
  only ever checked the classic `NANO` enum value, never the newer
  `NANOV2`.
