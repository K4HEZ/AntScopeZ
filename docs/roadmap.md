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
| 18 | Type "6ft"/"1.5M" into a field instead of the field's fixed unit | -- | -- | ❌ planned, not started (see the unit-shorthand-input plan) |
| 55 | See a measurement's actual stored values (not a chart) without exporting it to a file first, to tell a real data problem from a chart-rendering or export-format one | File > Save dialog, alongside the existing Save button: a new **Preview** button, scoped to the tabular/text formats (Touchstone `.s1p`/`.s2p`, `.asd`) -- hidden/disabled for image/PDF-style exports (screenshot, Print), which don't have the same meaning. Reuses the exact same per-format rendering code Save already runs (Touchstone RI/MA/DB, `.asd`'s own serialization), just renders to an on-screen table instead of writing to disk -- guarantees Preview can never drift out of sync with what Save actually produces, since it's the same code path with a different destination. Doubles as the verification tool for row 54's combine feature (e.g. confirming a synthesized measurement's S22 column is real, not accidentally zero). | File > Save dialog | ❌ designed 2026-10-22, not started |

## 2-port / transmission measurement (`.s2p` import, plus live NanoVNA-family S11+S21 -- classic ASCII since 2026-08-31, validated against real NanoVNA-H4 hardware 2026-09-02; V2/binary since 2026-09-03, emulator-only so far)

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 22 | Full S12/S22 access -- output-port match, S11≈S22 symmetry check, output-side matching network design, and real (not hardcoded-0) S12 on a live NanoVNA scan | -- | -- | ❌ two genuinely separate paths here, not one blocker (folded rows 22/23/24 together 2026-09-06). **Real dual-source 2-port hardware**: blocked on procurement, not a data-format question -- none of our supported devices have a second RF source/bridge (NanoVNA-family's port 2 is receive-only; RigExpert-family is single-port and doesn't even do S21). **DUT-flip-and-combine**: needs zero new hardware -- reverse a reversible 2-port DUT (filter, attenuator, amp), re-measure S11 from the other end, that *is* its S22; just needs a documented workflow plus UI to pair two separately-captured sweeps into one synthetic 2-port measurement. Either way, S22 is mathematically identical to S11 (reflection coefficient at the other port) -- once either path produces real data, displaying it is mostly reusing the existing S11 chart code (see Notes), not new rendering work. |
| 54 | Get real (not hardcoded-0) S12/S22 from two 1-port scans of a DUT, taken with it physically reversed between them | **Tools menu, always enabled**: a dialog reads whatever's currently checked in the Measurements panel. Not exactly two matching (same Start/Stop/Points, both plain 1-port) measurements checked -> shows guidance instead of controls. Exactly two valid -> shows both names (with a suggested combined name), a **Swap** button (which is Port 1 vs. Port 2-reversed, defaulting to scan order), and **Create .s2p Measurement** -- synthesizes one new measurement (Port 1's S11/S21 + Port 2's S11->S22/S21->S12) and adds it to the Measurements panel like any normal scan, not straight to a file save. Feeds the rest of the existing 2-port pipeline for free (S21 tab's real S12 trace, Export's real S2P output, the `(s2p)` Points tag, Multi-view, Marker Comparison). Also needs new **Show S11/Show S22** View-menu toggles (mirroring the existing Show S21/Show S12 pattern -- additive, both traces visible at once, not a mode switch) on SWR/Phase/Z=R+jX/Z=R‖jX/RL/Smith, defaulting off; a dashed-variant color convention for a measurement's S22 trace (same precedent as Smith's dashed SWR guide circles); disabled/meaningless for a live-NanoVNA 2-port scan where S22 is still hardcoded zero. TDR needs no new mechanism -- a port-2 view is already just overlaying the original, uncombined reversed scan (row 40's existing mechanism, already closed). **Scope split, 2026-10-22**: ship S11+S22-only pairing first -- it works on *any* hardware, including RigExpert-family, which can never capture S21 at all (row 28) and so can never get real S12/S21 from this technique no matter what; that's the only benefit RigExpert users ever get from row 54. Treat "also pull real S12/S21 when the paired scan genuinely captured S21 (NanoVNA-family)" as a layered enhancement on top, not a prerequisite. Also avoids a data-integrity problem: a `.s2p` file's S21/S12 columns can't distinguish "measured and genuinely zero" from "never measured," so always writing a full 4-parameter file (faking S12/S21 as zero when the hardware simply can't capture them) would silently mislead anyone -- or any other tool -- opening that file later. | SWR, Phase, Z=R+jX, Z=R‖jX, RL, Smith, S21 tab, Export, Tools menu | ❌ designed 2026-10-22, not started |
| 25 | Read group delay / phase linearity through a device | -- | -- | ❌ explicitly deferred in the 2-port plan |
| 28 | Capture 2-port data live from real hardware | NanoVNA-family: S11+S21, classic ASCII and V2/binary | any 1-port chart + S21 tab (NanoVNA only) | ⚠️ split by device family. **NanoVNA-family: ✅** classic ASCII validated on real NanoVNA-H4 hardware (2026-08-31/09-02); V2/binary shipped 2026-09-03, emulator-only so far, not real V2/LiteVNA64 hardware. Both forward-only (S12/S22 hardcoded 0) -- row 54's two-pass (flip-and-combine) approach is the path to real 4-parameter data here until/unless true dual-source hardware shows up. **RigExpert-family: ❌ permanent, not an open item** -- the only device tested (Match RFE) returns "Error.Not recognized" for both `FDB` (S21) and `EFRX`; firmware doesn't implement them, nothing to fix here. |

## App reliability / diagnostics

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 30 | Understand why a scan is slow or appears hung | A scan that goes silent past Settings > Analyzer > "Analyzer timeout" (default 8s) now fails with a proper non-modal error dialog -- timestamped, with whatever diagnostic detail is available -- instead of leaving the busy indicator/wait cursor stuck forever. Stopping a scan that's still delivering data (neither NanoVNA protocol has a wire-level abort) now visibly shows "draining" progress in the status bar and disables scan controls until it's genuinely done, instead of looking stopped while still silently discarding incoming points -- bounded by the same watchdog | Settings > General, analyzer error dialog, status bar | ✅ (watchdog shipped 2026-09-01 -- `AnalyzerPro` scan-silence watchdog; USB/HID and Serial connections also now detect "device present but busy" specifically, both at launch and while polling. Drain-aware stop shipped 2026-09-03/04 -- `AnalyzerPro::beginDraining()`/`stopCommandAbortsDevice()`, optional "Use reconnect to drain unwanted data" in Settings > Analyzer, USB/serial only -- a Bluetooth device always drains by waiting; the drain timeout restarts on every leftover point, 2026-10-20). ⚠️ Genuinely aborting a *very* large (~10000-point) scan mid-flight is still unfixed -- draining/reconnect both wait out or discard what's already in transit, but a prior attempt at a true wire-level-independent early exit (processEvents()+early-exit in the NanoVNA parse loops) broke real hardware badly (wouldn't start, needed multiple reconnects, stop didn't drain) and was fully reverted 2026-09-05. Leading suspect, unconfirmed: `QEventLoop::ExcludeSocketNotifiers` may be blocking something in the connect/handshake/drain path that genuinely depends on socket-notifier events firing during that window. Needs a fresh root-cause pass before retrying. |
| 31 | Reliable, configurable auto-reconnect after a dropped/power-cycled analyzer | Settings > Analyzer tab (renamed from "Developer" 2026-09-06, #6) exists as the future home for this | Settings > Analyzer | ⚠️ known gap -- row 30's "device present but busy" detection helps diagnose a stuck connect attempt but doesn't make reconnect itself more reliable. Retry/reconnect controls (count, backoff, defaults, UI shape) not yet scoped (folded row 42 in here 2026-09-06). **Design concern: needs a hard retry limit -- endless unattended reconnect cycling against unresponsive/faulted hardware risks actual hardware damage, not just a UX annoyance.** |

## Platform & external integration

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 47 | Run AntScopeZ natively on Windows, not just Linux/macOS | Released in 2.2.7: `AntScopeZ-<version>-win64.exe` NSIS installer (Qt 6.11.2 MinGW; CI and presets now use Qt 6.12.0), built by GitHub Actions (`windows-build.yml`) | -- | ⚠️ builds, packages and launches in CI (screenshot-verified); none of `docs/windows-port-audit.md`'s "Verification checklist" items (real analyzer over HID/FTDI, `.asd` file association, device-change notifications, per-user settings path) have been exercised against real Windows hardware yet. Unsigned installer (SmartScreen warns). See issue #58. |
| 49 | Get a distributable macOS build, without an Apple Developer account | Released in 2.2.7: `AntScopeZ-<version>-macOS.dmg`, universal arm64+x86_64 (`macos-build.yml`), Qt 6.11's own macOS 13+ floor (the develop branch now targets Qt 6.12 and macOS 14.4) | -- | ⚠️ builds and launches -- confirmed via real CI-captured screenshots on both arm64 and Intel (x86_64), 2026-09-18. Unsigned, not notarized (Gatekeeper needs right-click > Open or `xattr -cr`); no real analyzer hardware reachable from CI. See issue #57. |
| 48 | Let an external tool observe/control a live scan programmatically (automation scripts, remote dashboards, etc.) | Settings > Graphs > Enable Remote API (NDJSON-over-TCP, loopback-only by default); status/devices/connect/disconnect/sweep/stop/subscribe/last commands, live point streaming | Settings > Graphs, `core/remoteapi/README.md` | ✅ core commands implemented (PR #11, merged 2026-09-06; 5 review-found bugs fixed same day, see CHANGELOG's 2.2.4 entry). ⚠️ known gaps: BLE devices explicitly rejected (not yet supported); can't target one specific device by port when more than one of the same type is attached (always connects to whichever port enumerates first); each `connect` command's device handshake runs synchronously on the GUI thread, so a slow/flaky device blocks the whole UI for the duration. Related to, but not a replacement for, letting a person drive the GUI remotely (AntScopeZ-Web/screen-share, already shipped) -- this is a machine-facing data/control API, not a screen-sharing mechanism. |
| 51 | Run the mobile app on an iPhone | `mobile/` builds for iOS with Qt 6.12's iOS kits; Bluetooth only | Mobile app | ⚠️ verified on a real iPhone with BLE (RigExpert Match, 2026-10-19). USB-only analyzers (most NanoVNAs) can't work on iOS -- hard platform limitation, not a bug. A BLE-to-USB bridge could close this; not scoped. |
| 53 | Get the iOS build onto the App Store | Icon, privacy manifest, launch screen already in place (2026-10-21) | -- | ❌ not started -- blocked on no Apple Developer Program account (needed for code-signing and an App Store Connect listing). Staying foreground-only for V1 (deliberate). iPad untested; decide iPhone-only vs. validate on iPad before release. |

## Notes

Row 22's gap (S22) is the single largest one left: nothing lets a
user look at a 2-port device's output port at all, not a
workaround-but-clunky case, a flat ❌. It's purely an acquisition gap,
not a rendering one -- S22 is the same reflection-coefficient math as
S11, just at the other port, so it'd reuse existing single-port chart
code once real data exists via either path described in row 22. 2-port
export and S21-in-Multi-view
(both already shipped and working) are worth keeping in view when S22
does get designed -- whatever data model it uses should stay export- and
Multi-view-compatible without another rework. The Points-column s1p/s2p
tag turned out to be a real prerequisite for 2-port export, not just
adjacent to it -- `Export::updateDetails()` reads that tag to decide
whether to show the S2P button at all.

Resolved 2026-10-22 (see row 54): yes, independent Show S11/Show S22
toggles on *every* reflection chart (SWR/Phase/Z=R+jX/Z=R‖jX/RL/Smith),
not just the S21 tab -- additive (both traces visible together), same
pattern as the existing Show S21/Show S12 toggles, not a mode switch.
Renaming the "S21" tab itself (e.g. to "2Port") was discussed and
deliberately not done 2026-09-05/06 -- still an open question, but a
smaller one now that the toggle design is settled; a name change would
still need revisiting once row 54 actually ships.
