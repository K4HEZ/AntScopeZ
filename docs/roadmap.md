---
layout: default
title: Use Cases & Outstanding Work
---

# AntScopeZ use cases & outstanding work (working doc)

Not end-user documentation -- that's `docs/user-guide.md`, organized by
*feature*. This is organized by *goal* ("I want to accomplish X"),
specifically to drive feature planning: for each real thing a user
might be trying to do, what's the procedure today, and does AntScopeZ
actually cover it yet. Also folds in (2026-09-06, merged from a
separate `todo.txt`) the handful of outstanding items that aren't
use-case-shaped -- open questions, unfinished QA passes -- so there's
one place to check for "what's not done yet" instead of two.

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
| 8 | Log/export a measurement for a report or later reference | File > Save (or right-click a row > Save as...) -- one dialog for every format, including AntScopeZ's own .asd; Print, Save Screenshot | File menu, right-click menu | ✅ (Import/Export renamed Open/Save 2026-09-06 -- one dialog per direction now, not a separate quick-.asd path plus a different multi-format one) |
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
| 20 | Sanity-check S21≈S12 (reciprocity) for a passive device | View > Show S12 (off by default -- S12 duplicates S21 exactly for any reciprocal device, so it's opt-in); both traces solid, distinct colors (dashed-vs-solid dropped 2026-09-06, no longer needed) | S21 tab | ✅ for imported `.s2p` data. Not meaningful on a live NanoVNA scan -- that path is forward-only and hardcodes S12 to 0 (see row 28), so it can never actually show S21≈S12, only S21 against a flat zero line. |
| 21 | Track a frequency point's S21/S12 across the Markers table / cursor hover | Marker Comparison table columns, Cursor Details | S21 tab, Markers | ✅ (shipped 2026-08-19). Same caveat as row 20 -- the S12 column reads real data from an imported `.s2p`, but is always 0 on a live NanoVNA scan. |
| 22 | Check the DUT's *output*-port match (S22) | -- | -- | ❌ deferred -- and now a *double* gap, not a single one: no live-capture path produces real S22 either (NanoVNA's row 28 addition is forward-only S11+S21, S22 hardcoded to 0 in `nanovna_analyzer.cpp`), and even an imported `.s2p` file's real S22 is parsed and re-exportable but never surfaced anywhere in the UI (no chart, no column) |
| 23 | Sanity-check a device is electrically symmetric (S11≈S22) | -- | -- | ❌ same gap as above |
| 24 | Design a matching network for a 2-port device's output side | -- | -- | ❌ same gap |
| 25 | Read group delay / phase linearity through a device | -- | -- | ❌ explicitly deferred in the 2-port plan |
| 26 | Compare S21 alongside another chart in Multi view | Right-click the S21 tab > "Move chart to the tab Multi", same as any other chart | S21 tab, Multi | ✅ (fixed 2026-09-06 -- S21 used to be excluded from Multi's join menu entirely, which also meant joining it via the *other* "move to Multi" menu, then using Multi's own "Close all", could permanently strand the tab hidden with no way back; both fixed together, S21 is a fully symmetric Multi-tab participant now) |
| 27 | Export imported 2-port data back out (S11/S21/S12/S22), in RI, MA, or DB | Export dialog's S2P RI/MA/DB buttons (only shown for a 2-port measurement) | File > Export | ✅ (shipped 2026-08-19, `exportSParamData()`) |
| 28 | Capture 2-port data live from real hardware | NanoVNA-family (classic ASCII *and* V2/binary): `data 1` after the normal S11 pass (classic), or S11+S21 together per FIFO record (V2), plus an opportunistic ASCII/binary `scan` fast path where classic firmware supports it | any 1-port chart + S21 tab (NanoVNA only) | ⚠️ split by device family. **NanoVNA-family: ✅** classic ASCII shipped 2026-08-31, since exercised against real NanoVNA-H4 hardware (two real bugs found and fixed 2026-09-02 -- see [[nanovna-two-port-work-deferred]]); V2/binary (`nanovna_v2_analyzer.cpp`) shipped 2026-09-03, so far only tested against the companion NanoVNA emulator, not real V2/LiteVNA64 hardware. Both forward-only: S12/S22 are hardcoded to `(0,0)`, not a full 4-parameter capture. **RigExpert-family: ❌** still a dead end on this user's hardware -- `FDB` (S21) *and* `EFRX` (User Defined tab) both return "Error.Not recognized" on the only device tested (RigExpert Match RFE, confirmed 2026-08-19/20). See [[s21-and-user-defined-live-capture-deferred]]. (Row 29's stitching still doesn't reach either path -- `on_measureUser()`/EFRX already routes through `startStitchedMeasure()`, `on_measureS21()`/FDB doesn't, and NanoVNA's own scan pipeline is separate again.) |
| 35 | Import a 2-port *Z-parameter* Touchstone file (`# ... Z RI ...`, 9-value rows) correctly | Import as usual -- `Measurements::zToSParam()` converts Z21/Z12/Z22 to S21/S12/S22 automatically | File > Open | ✅ (real gap found 2026-08-19, fixed 2026-09-06, #7 -- real Z-to-S 2-port matrix conversion, hand-verified against a known S-to-Z round-trip case) |
| 34 | Tell, at a glance, which Measurements-list rows are 2-port (have real S21/S12/S22) vs. plain 1-port | Points column now reads e.g. "100 (s1p)"/"300 (s2p)" | Measurements panel | ✅ (shipped 2026-08-19; column-width formatting for large point counts still to be revisited) |

## App reliability / diagnostics

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 30 | Understand why a scan is slow or appears hung | A scan that goes silent past Settings > General > "Analyzer timeout" (default 8s) now fails with a proper non-modal error dialog -- timestamped, with whatever diagnostic detail is available -- instead of leaving the busy indicator/wait cursor stuck forever. Stopping a scan that's still delivering data (neither NanoVNA protocol has a wire-level abort) now visibly shows "draining" progress in the status bar and disables scan controls until it's genuinely done, instead of looking stopped while still silently discarding incoming points -- bounded by the same watchdog | Settings > General, analyzer error dialog, status bar | ✅ (watchdog shipped 2026-09-01 -- `AnalyzerPro` scan-silence watchdog; USB/HID and Serial connections also now detect "device present but busy" specifically, both at launch and while polling. Drain-aware stop shipped 2026-09-03/04 -- `AnalyzerPro::beginDraining()`/`stopCommandAbortsDevice()`, optional "Use reconnect to drain unwanted data" in Settings > General). ⚠️ Genuinely aborting a *very* large (~10000-point) scan mid-flight is still unfixed -- draining/reconnect both wait out or discard what's already in transit, but a prior attempt at a true wire-level-independent early exit (processEvents()+early-exit in the NanoVNA parse loops) broke real hardware badly (wouldn't start, needed multiple reconnects, stop didn't drain) and was fully reverted 2026-09-05. Leading suspect, unconfirmed: `QEventLoop::ExcludeSocketNotifiers` may be blocking something in the connect/handshake/drain path that genuinely depends on socket-notifier events firing during that window. Needs a fresh root-cause pass before retrying. |
| 31 | Reliable auto-reconnect after a dropped/power-cycled analyzer | -- | -- | ⚠️ known gap, unchanged -- row 30's "device present but busy" detection helps diagnose a stuck connect attempt but doesn't make the reconnect itself more reliable |
| 38 | Get remote help from a more experienced ham -- let them drive your analyzer software over the network while you handle the physical antenna/hardware end | -- | -- | ❌ parked idea (2026-08-20), not scoped or designed -- see [[remote-network-control-idea]]. Would supersede, not extend, an existing but abandoned narrow UDP bridge (see [[s21-and-user-defined-live-capture-deferred]]-adjacent findings in `BUILDINFO.md`). |
| 42 | Configure how AntScopeZ handles analyzer errors (retry/reconnect behavior) | -- | -- | ⚠️ tab renamed "Developer" -> "Analyzer" 2026-09-06 (#6). What retry/reconnect controls actually belong on it (retry count, backoff, defaults, UI shape) still not scoped -- needs a design pass before implementation. Feeds directly into row 31's gap. |
| 43 | Manually disconnect from the analyzer without closing the app | -- | -- | ❌ planned, not started (2026-09-06) -- there's a Connect Analyzer... action but no matching Disconnect; `on_deviceDisconnected()` today only handles an unexpected drop, not a user-requested one. Add an explicit Connect/Disconnect mechanism. |

## Visual / UI

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 32 | Tell overlapping measurement traces apart at a glance | `getColor()` palette; on the S21 tab specifically, selecting a measurement brings its traces to the front (z-order) and every trace is fully opaque | any multi-trace chart | ✅ (getColor()'s red-collapse fixed 2026-09-04; S21-specific z-order-on-select and opacity fixes 2026-09-05/06 -- opacity replaced a permanent semi-transparency that had traded "one trace fully hides another" for "two overlapping traces blend into an unlabeled third color") |
| 41 | Know at a glance whether a measurement has unsaved changes | Points column shows a trailing " *" for a dirty measurement (scanned or renamed since last saved) | Measurements panel | ✅ (shipped 2026-09-06 -- `measurement::dirty`; deleting/clearing a dirty measurement also warns first, gated by a Settings > General checkbox, default on) |
| 33 | Keep the Markers table visible without it floating awkwardly over the chart | Docked into a resizable splitter under the plot tabs as a normal themed table; visibility follows the View > Markers Hint checkbox alone; columns auto-widen to fit real data on every refresh | Markers panel | ✅ (shipped 2026-09-01 -- `MarkersPopUp`, a translucent floating `Qt::Tool` window, retired in favor of `MarkersPanel`, a plain child widget styled for free by the app's existing Fusion/palette theming. Column auto-resize fixed 2026-09-06 -- previously only sized once, to the header label's width.) |
| 44 | Clear all markers at once, or just the ones that no longer land on real scan data (e.g. after switching to a narrower-range measurement, a marker's original frequency falls outside it) | Right-click the Markers table > "Clear All Markers" (removes all) or "Clear Empty Markers" (removes only those with no valid data across any measurement) | Markers panel | ✅ (shipped 2026-09-06 -- detection scans all data columns, identifies markers with no valid data across any/all measurements, and removes in reverse index order to avoid renumbering conflicts) |
| 45 | See the full ±180° phase swing on the Phase chart without the 180° line clipping right at the chart's edge | -- | Phase chart | ❌ planned, not started (2026-09-06) -- suggested fix: widen the vertical axis to ±190° so 180° sits inside the plot area instead of getting truncated at the boundary |
| 46 | Scale the Series/Parallel Z charts out far enough to see high-impedance points | -- | Z=R+jX, Z=R‖jX | ❌ planned, not started (2026-09-06) -- allow the vertical axis to scale up to ±5000 Ω |

## Other outstanding items

Things that don't fit the use-case table shape above, but still need
action:

- **Measurements file I/O rework needs a real live test pass.**
  Implemented 2026-09-06 (rows 8 and 41 above; see `CHANGELOG.md` for
  the full shape), but not yet exercised end-to-end by hand: rename,
  save in every format including the new .asd button, delete/clear both
  with and without dirty measurements present, the warn-before-discard
  checkbox itself, the new combined "All supported files" Open filter,
  and right-click on an empty row vs. a real one.

## Notes

Row 22-24's gap (S22) is the single largest one left: nothing lets a
user look at a 2-port device's output port at all, not a
workaround-but-clunky case, a flat ❌. Row 27 (2-port export) and row 26
(Multi view) are adjacent gaps worth keeping in view when S22 does get
designed -- whatever data model it uses should be export- and
Multi-view-compatible without another rework. Row 34 (the Points-column
s1p/s2p tag) turned out to be a real prerequisite for row 27, not just
adjacent to it -- `Export::updateDetails()` reads that tag to decide
whether to show the S2P button at all.

Related, still-open design question: whether a truly symmetric 2-port
device (real dual-source hardware, or a documented DUT-flip-and-combine
workflow) should get independent Port1(S11)/Port2(S22) toggles on
*every* chart (SWR/RL/Smith/Rs/Rp), not just the S21 tab. Renaming the
"S21" tab itself (e.g. to "2Port") was discussed and deliberately not
done 2026-09-05/06, pending that broader question -- a name change now
would likely need revisiting anyway once S22 lands.
