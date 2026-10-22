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
| 39 | See every significant reflection in a scan, not just the single strongest one (e.g. two connectors plus the far end) | Mode > TDR events table: every reflection above the noise floor, nearest first, with distance, time, impedance, a plain-wording kind (open end, short end, High-Z, Low-Z...) and notes | TDR | ✅ shipped 2026-10-07. ⚠️ the detection thresholds are fixed starting values, not yet tuned against many real scans |
| 40 | Compare a TDR scan against an earlier one of the same cable (did a repair actually change anything) | Select multiple Measurements rows, eyeball both traces on the same chart | TDR (multi-select already works) | ⚠️ possible today by overlaying rows manually; no dedicated before/after diff (distance/amplitude deltas called out explicitly). Feature suggestion, 2026-08-21, not scoped -- would need a second-measurement picker UI. |
| 18 | Type "6ft"/"1.5M" into a field instead of the field's fixed unit | -- | -- | ❌ planned, not started (see the unit-shorthand-input plan) |

## 2-port / transmission measurement (`.s2p` import, plus live NanoVNA-family S11+S21 -- classic ASCII since 2026-08-31, validated against real NanoVNA-H4 hardware 2026-09-02; V2/binary since 2026-09-03, emulator-only so far)

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 19 | Verify a filter/attenuator's passband or insertion loss vs frequency | Import a `.s2p`, read S21 magnitude -- or, on NanoVNA-family hardware, a live scan now populates real S21 directly (unvalidated, see row 28) | S21 tab | ✅ (shipped 2026-08-19; live NanoVNA path added 2026-08-31) |
| 20 | Sanity-check S21≈S12 (reciprocity) for a passive device | View > Show S12 | S21 tab | ✅ for imported `.s2p` data. Live NanoVNA scans hardcode S12 to 0 -- see row 22. |
| 21 | Track a frequency point's S21/S12 across the Markers table / cursor hover | Marker Comparison table columns, Cursor Details | S21 tab, Markers | ✅ for imported `.s2p` data. Same live-scan caveat as row 20. |
| 22 | Full S12/S22 access -- output-port match, S11≈S22 symmetry check, output-side matching network design, and real (not hardcoded-0) S12 on a live NanoVNA scan | -- | -- | ❌ deferred pending hardware to test a full 4-parameter capture with (folded rows 22/23/24 together 2026-09-06) |
| 25 | Read group delay / phase linearity through a device | -- | -- | ❌ explicitly deferred in the 2-port plan |
| 28 | Capture 2-port data live from real hardware | NanoVNA-family (classic ASCII *and* V2/binary): `data 1` after the normal S11 pass (classic), or S11+S21 together per FIFO record (V2), plus an opportunistic ASCII/binary `scan` fast path where classic firmware supports it | any 1-port chart + S21 tab (NanoVNA only) | ⚠️ split by device family. **NanoVNA-family: ✅** classic ASCII shipped 2026-08-31, since exercised against real NanoVNA-H4 hardware (two real bugs found and fixed 2026-09-02); V2/binary (`nanovna_v2_analyzer.cpp`) shipped 2026-09-03, so far only tested against the companion NanoVNA emulator, not real V2/LiteVNA64 hardware. Both forward-only: S12/S22 are hardcoded to `(0,0)`, not a full 4-parameter capture. **RigExpert-family: ❌ (closed as a hardware/firmware limitation, 2026-10-21 -- nothing to do on our side).** `FDB` (S21) *and* `EFRX` (User Defined tab) both return "Error.Not recognized" on the only device tested (RigExpert Match RFE, confirmed 2026-08-19/20) -- the device's own firmware simply doesn't implement these commands. Not an AntScopeZ bug and not fixable here short of RigExpert shipping new firmware; accepted as a permanent per-device-family gap rather than an open item. (Segmented/stitched-sweep support still doesn't reach either path -- `on_measureUser()`/EFRX already routes through `startStitchedMeasure()`, `on_measureS21()`/FDB doesn't, and NanoVNA's own scan pipeline is separate again -- moot for RigExpert given the above, still relevant if NanoVNA-family stitching ever needs revisiting.) |
| 35 | Import a 2-port *Z-parameter* Touchstone file (`# ... Z RI ...`, 9-value rows) correctly | Import as usual -- `Measurements::zToSParam()` converts Z21/Z12/Z22 to S21/S12/S22 automatically | File > Open | ✅ (real gap found 2026-08-19, fixed 2026-09-06, #7 -- real Z-to-S 2-port matrix conversion, hand-verified against a known S-to-Z round-trip case) |
| 34 | Tell, at a glance, which Measurements-list rows are 2-port (have real S21/S12/S22) vs. plain 1-port | Points column now reads e.g. "100 (s1p)"/"300 (s2p)" | Measurements panel | ✅ (shipped 2026-08-19; column-width formatting for large point counts still to be revisited) |

## App reliability / diagnostics

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 30 | Understand why a scan is slow or appears hung | A scan that goes silent past Settings > Analyzer > "Analyzer timeout" (default 8s) now fails with a proper non-modal error dialog -- timestamped, with whatever diagnostic detail is available -- instead of leaving the busy indicator/wait cursor stuck forever. Stopping a scan that's still delivering data (neither NanoVNA protocol has a wire-level abort) now visibly shows "draining" progress in the status bar and disables scan controls until it's genuinely done, instead of looking stopped while still silently discarding incoming points -- bounded by the same watchdog | Settings > General, analyzer error dialog, status bar | ✅ (watchdog shipped 2026-09-01 -- `AnalyzerPro` scan-silence watchdog; USB/HID and Serial connections also now detect "device present but busy" specifically, both at launch and while polling. Drain-aware stop shipped 2026-09-03/04 -- `AnalyzerPro::beginDraining()`/`stopCommandAbortsDevice()`, optional "Use reconnect to drain unwanted data" in Settings > Analyzer, USB/serial only -- a Bluetooth device always drains by waiting; the drain timeout restarts on every leftover point, 2026-10-20). ⚠️ Genuinely aborting a *very* large (~10000-point) scan mid-flight is still unfixed -- draining/reconnect both wait out or discard what's already in transit, but a prior attempt at a true wire-level-independent early exit (processEvents()+early-exit in the NanoVNA parse loops) broke real hardware badly (wouldn't start, needed multiple reconnects, stop didn't drain) and was fully reverted 2026-09-05. Leading suspect, unconfirmed: `QEventLoop::ExcludeSocketNotifiers` may be blocking something in the connect/handshake/drain path that genuinely depends on socket-notifier events firing during that window. Needs a fresh root-cause pass before retrying. |
| 31 | Reliable, configurable auto-reconnect after a dropped/power-cycled analyzer | Settings > Analyzer tab (renamed from "Developer" 2026-09-06, #6) exists as the future home for this | Settings > Analyzer | ⚠️ known gap -- row 30's "device present but busy" detection helps diagnose a stuck connect attempt but doesn't make reconnect itself more reliable. Retry/reconnect controls (count, backoff, defaults, UI shape) not yet scoped (folded row 42 in here 2026-09-06). **Design concern: needs a hard retry limit -- endless unattended reconnect cycling against unresponsive/faulted hardware risks actual hardware damage, not just a UX annoyance.** |
| 38 | Get remote help from a more experienced ham -- let them drive your analyzer software over the network while you handle the physical antenna/hardware end | Two options, for different needs: **AntScopeZ-Web** (separate repo, `github.com/K4HEZ/AntScopeZ-Web`) -- a standalone bridge to AntScopeZ's own Remote API (row 48) giving basic remote scan control (start/stop/points, live SWR) from any browser, including a phone. LAN by default; reachable beyond the LAN the same way any other home-hosted service is, by forwarding a port on the router -- a deliberate tradeoff the user takes on themselves, since the bridge has no built-in auth/TLS of its own. **Screen-sharing software** (WebEx or equivalent) -- lets a genuinely remote ham drive the real AntScopeZ desktop app directly: full feature set (calibration, TDR, markers, every setting), not limited to Wi-Fi/LAN at all, no AntScopeZ-specific code involved since it's the same screen-share pattern used for anything else. | Settings > Graphs (Remote API), AntScopeZ-Web, third-party screen-share | ✅ (2026-10-21) -- supersedes the abandoned narrow UDP bridge previously mentioned here (see `BUILDINFO.md`'s findings). AntScopeZ-Web covers quick/basic remote scan triggering without installing anything on the helper's end; screen-share covers full-feature remote operation when that's what's actually needed instead. |

## Platform & external integration

| # | Use case | How today | Charts/tools | Status |
|---|---|---|---|---|
| 47 | Run AntScopeZ natively on Windows, not just Linux/macOS | Released in 2.2.7: `AntScopeZ-<version>-win64.exe` NSIS installer (Qt 6.11.2 MinGW; CI and presets now use Qt 6.12.0), built by GitHub Actions (`windows-build.yml`) | -- | ⚠️ builds, packages and launches in CI (screenshot-verified); none of `docs/windows-port-audit.md`'s "Verification checklist" items (real analyzer over HID/FTDI, `.asd` file association, device-change notifications, per-user settings path) have been exercised against real Windows hardware yet. Unsigned installer (SmartScreen warns). See issue #58. |
| 49 | Get a distributable macOS build, without an Apple Developer account | Released in 2.2.7: `AntScopeZ-<version>-macOS.dmg`, universal arm64+x86_64 (`macos-build.yml`), Qt 6.11's own macOS 13+ floor (the develop branch now targets Qt 6.12 and macOS 14.4) | -- | ⚠️ builds and launches -- confirmed via real CI-captured screenshots on both arm64 and Intel (x86_64), 2026-09-18. Unsigned, not notarized (Gatekeeper needs right-click > Open or `xattr -cr`); no real analyzer hardware reachable from CI. See issue #57. |
| 48 | Let an external tool observe/control a live scan programmatically (automation scripts, remote dashboards, etc.) | Settings > Graphs > Enable Remote API (NDJSON-over-TCP, loopback-only by default); status/devices/connect/disconnect/sweep/stop/subscribe/last commands, live point streaming | Settings > Graphs, `core/remoteapi/README.md` | ✅ core commands implemented (PR #11, merged 2026-09-06; 5 review-found bugs fixed same day, see CHANGELOG's 2.2.4 entry). ⚠️ known gaps: BLE devices explicitly rejected (not yet supported); can't target one specific device by port when more than one of the same type is attached (always connects to whichever port enumerates first); each `connect` command's device handshake runs synchronously on the GUI thread, so a slow/flaky device blocks the whole UI for the duration. Related to, but not a replacement for, the earlier "let a person drive the GUI remotely" idea (row 38) -- this is a machine-facing data/control API, not a screen-sharing mechanism. |
| 51 | Run the mobile app on an iPhone | `mobile/` builds for iOS with Qt 6.12's iOS kits; Bluetooth only | Mobile app | ⚠️ verified on a real iPhone with BLE (RigExpert Match, 2026-10-19). USB-only analyzers (most NanoVNAs) can't work on iOS -- hard platform limitation, not a bug. A BLE-to-USB bridge could close this; not scoped. |
| 53 | Get the iOS build onto the App Store | Icon, privacy manifest, launch screen already in place (2026-10-21) | -- | ❌ not started -- blocked on no Apple Developer Program account (needed for code-signing and an App Store Connect listing). Staying foreground-only for V1 (deliberate). iPad untested; decide iPhone-only vs. validate on iPad before release. |

## Notes

Row 22's gap (S22) is the single largest one left: nothing lets a
user look at a 2-port device's output port at all, not a
workaround-but-clunky case, a flat ❌. 2-port export and S21-in-Multi-view
(both already shipped and working) are worth keeping in view when S22
does get designed -- whatever data model it uses should stay export- and
Multi-view-compatible without another rework. Row 34 (the Points-column
s1p/s2p tag) turned out to be a real prerequisite for 2-port export, not
just adjacent to it -- `Export::updateDetails()` reads that tag to decide
whether to show the S2P button at all.

Related, still-open design question: whether a truly symmetric 2-port
device (real dual-source hardware, or a documented DUT-flip-and-combine
workflow) should get independent Port1(S11)/Port2(S22) toggles on
*every* chart (SWR/RL/Smith/Rs/Rp), not just the S21 tab. Renaming the
"S21" tab itself (e.g. to "2Port") was discussed and deliberately not
done 2026-09-05/06, pending that broader question -- a name change now
would likely need revisiting anyway once S22 lands.
