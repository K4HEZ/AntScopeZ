---
layout: default
title: User Guide
---

# AntScopeZ User Guide

This is the closest thing AntScopeZ has to real end-user documentation --
the root `README.md` is build/developer instructions only. It grew out of
a handful of stub sections into most of what's below; if something's
missing or wrong, that's more likely this guide being incomplete than the
app -- open an issue.

See [SUPPORTED_DEVICES.md](../SUPPORTED_DEVICES.md) for the full list of
supported analyzer models and brands.

## Table of contents

- [Installing and uninstalling](#installing-and-uninstalling)
- [First-time setup checklist](#first-time-setup-checklist)
- [Getting started](#getting-started)
- [Controls reference](#controls-reference)
- [Settings](#settings)
- [Interpreting your data](#interpreting-your-data)
- [Scan modes: Single vs. Continuous](#scan-modes-single-vs-continuous)
- [Calibration (OSL)](#calibration-osl)
- [Presets and bands](#presets-and-bands)
- [Markers](#markers)
- [Multi view](#multi-view)
- [Data from AA](#data-from-aa)
- [Import / Export](#import--export)
- [Print and screenshots](#print-and-screenshots)
- [TDR (Time Domain Reflectometry)](#tdr-time-domain-reflectometry)
- [Customized analyzer parameters](#customized-analyzer-parameters)
- [Files and directories](#files-and-directories)
- [Troubleshooting](#troubleshooting)

## Installing and uninstalling

### Linux (`.deb`)

Download `antscopez_<version>_amd64.deb` from the
[latest release](https://github.com/K4HEZ/AntScopeZ/releases/latest),
then install it with `apt` so it resolves dependencies automatically:

```sh
sudo apt install ./antscopez_<version>_amd64.deb
```

(A plain `sudo dpkg -i antscopez_<version>_amd64.deb` also works, but
won't pull in anything missing on its own -- run
`sudo apt --fix-broken install` afterward if it complains about unmet
dependencies.)

Released `.deb`s are built with a fixup step
(`cmake/fix-deb-self-dependency.sh`) that strips a `dpkg-shlibdeps`
quirk which otherwise makes the package list itself,
`antscopez (>= <version>)`, as one of its own dependencies -- not
cosmetic if it's present: a package can't satisfy a dependency on
itself on a machine that doesn't already have it installed, so an
un-fixed-up build is genuinely uninstallable, not just noisy. See
`BUILDINFO.md`'s Known Issues if you're building your own `.deb` and
hit this.

**Upgrading:** install a newer `.deb` the same way -- it replaces the
current install in place. Your own settings and calibration data live
entirely outside what the package touches (see
[Files and directories](#files-and-directories)), so they're untouched
by installing, upgrading, or removing the package.

**Uninstalling:**

```sh
sudo apt remove antscopez
```

This removes everything the package itself installed -- the binary,
bundled Qt libraries, shared data, desktop entry, and icon -- but
deliberately leaves `~/.config/AntScopeZ/` alone. If you want a
completely clean removal (settings, calibration data, any per-user
translation/band overrides), delete that folder yourself too:

```sh
rm -rf ~/.config/AntScopeZ
```

### Windows, macOS, or other Linux distros

No installer package for Windows or macOS yet, and Linux distributions
that aren't Debian/Ubuntu-based won't have a native package either --
build from source instead. See [BUILDINFO.md](../BUILDINFO.md) for
requirements and build steps. "Uninstalling" a from-source build is
just deleting the build directory and (if you want a clean slate)
whatever per-user config folder it wrote to (see
[Files and directories](#files-and-directories) for the Windows/macOS
equivalents).

## First-time setup checklist

A fast path through one-time setup, before your first real scan.
Everything here is covered in more detail elsewhere -- this is just the
order to do it in.

1. [Install AntScopeZ](#installing-and-uninstalling).
2. [Connect your analyzer](#connecting-to-your-analyzer).
3. Pick your measurement units (Metric/Imperial) in
   [Settings → General](#general-tab); Light/Dark theme and language
   (if not English) are set from the **View** menu instead.
4. *(Optional but recommended)* [Run OSL calibration](#calibration-osl)
   -- Open/Short/Load, once per analyzer.
5. If you want the [band selector](#presets-and-bands) shortcut, enable
   it from the **View** menu ("Band Selector").
6. Run [your first scan](#your-first-scan).

That's it -- everything past this point in the guide is reference
material for a specific feature, not more setup.

## Getting started

### Connecting to your analyzer

Click **Connect Analyzer** on the menu bar (or Settings → General →
**Connect analyzer**, which does the same thing) to open the "Connect
Analyzer" dialog. Pick a connection type (USB, COM, or BLE), click
**Scan**, select your device from the list, and **Connect**. "Use same
selection for future connections" saves that choice so AntScopeZ can
silently reconnect on its own next time, instead of asking again.

By default, this dialog also pops up automatically ~500 ms after launch
if there's no valid saved device to silently reconnect to. If you'd
rather it not do that -- say, you're just reviewing saved `.s1p`/`.asd`
files with no analyzer connected -- uncheck Settings → General → "Open
'Connect Analyzer' on launch". The manual **Connect Analyzer** menu item
is unaffected either way.

Once connected, the window's title bar shows the device's model/name
instead of "Analyzer not connected".

### Your first scan

1. **Set a frequency range.** Type Start/Stop directly into the
   Frequency panel, switch the **Scan Mode** combo box to Center/Range
   for that pair of fields instead, or -- if enabled (View menu →
   "Band Selector") -- pick a ham band from the selector above the
   Presets list, which fills in Start/Stop for you.
2. **Set the point count.** Type a number into Points directly, or drag
   the **Speed/Accuracy** slider just below it (10–1000 points; arrow
   keys move it in steps of 10) -- Fast end fewer points, Accurate end
   more. More points = finer resolution across your range, at the cost
   of a slower sweep.
3. **Run it.** Click **Single** (or press F9) for one sweep, or
   **Continuous** (F10) to keep sweeping until you stop it -- see
   [Scan modes](#scan-modes-single-vs-continuous) below for why you'd
   pick one over the other.
4. **Watch it draw.** The active chart tab (SWR by default) fills in
   point by point as data arrives.
5. Once a sweep finishes, it's already sitting in the **Measurements**
   list on the right, auto-named with an incrementing `NN>` prefix.
   Double-click the name to rename it, or use **Save** to write it out
   as an AntScopeZ `.asd` file if you want to keep it outside the app's
   own settings storage.

## Controls reference

Brief description of each control, grouped the way they're laid out in
the main window.

**Menu bar**

There's no toolbar of buttons any more -- everything below lives in the
menu bar instead (File / Edit / View / Connect Analyzer / Help).

*File*

| Control | What it does |
|---|---|
| Import Data... | Loads an external file: Touchstone (.s1p), CSV, NWL, or AntScopeZ's own `.asd` |
| Export Data... | Exports the *selected* measurement to CSV, NWL, or Touchstone (.s1p) -- select a row in Measurements first |
| Settings... | Opens the [Settings dialog](#settings) |
| Print... | Opens the [Print dialog](#print-and-screenshots) for the current chart |
| Save Screenshot... | Saves the *current chart* (not the whole window) straight to a PNG file you pick -- same image Ctrl+C copies, just written to disk instead of the clipboard |
| Screenshot from AA | Captures the *analyzer's own* on-device screen (not every model supports this -- see [Supported Devices](../SUPPORTED_DEVICES.md)) -- see [Print and screenshots](#print-and-screenshots) |
| Data from AA | Loads measurement results already stored in the analyzer's own memory -- see [Data from AA](#data-from-aa) |
| Exit | Closes AntScopeZ |

*Edit*

| Control | What it does |
|---|---|
| Edit ITU Bands... | Opens the [band editor](#editing-band-definitions) |

*View*

| Control | What it does |
|---|---|
| Cursor Details / Markers Hint / Cursor Params | Toggle the various hover/cursor readout popups on the charts (Cursor Details is docked in the main window; the other two float) |
| Show Band Name | Labels the shaded bands on the charts with their names, not just color |
| Band Selector | Shows/hides the band-selector dropdown above the Presets list -- see [Presets and bands](#presets-and-bands) |
| Band Highlighting | Submenu picking which region's band data to shade on the charts |
| Language | UI language -- auto-discovered from whatever `QtLanguage_*.qm` files are installed, not a fixed list |
| Theme | Light or Dark -- see [CHANGELOG.md](../CHANGELOG.md) for what it does and doesn't cover |

*Tools*

| Control | What it does |
|---|---|
| Marker Comparison... | Compare two placed markers and estimate an antenna trim -- see [Markers](#markers) |
| TDR Analysis... | Distance/open-short readout and a velocity-factor calculator for the last TDR scan -- see [TDR](#tdr-time-domain-reflectometry) |

*Connect Analyzer* -- opens the [device-connection dialog](#connecting-to-your-analyzer) directly, same as Settings → General's own button.

*Help*

| Control | What it does |
|---|---|
| About AntScopeZ... | Shows the running app's version number and build timestamp |

**Frequency panel**

| Control | What it does |
|---|---|
| Scan Mode | Two ways to define the same swept range -- Start/Stop (absolute), or Center/Range (a center frequency ± a range) |
| Start, Stop (or Center, Range) | The actual sweep bounds, in kHz |
| Points | Number of measurement points across the range |
| Speed/Accuracy (slider) | Sets Points for you -- Fast (fewer points) end to Accurate (more points) end, 10–1000 |
| Calibration (checkbox) | Applies OSL calibration correction to scans -- has no effect until you've actually performed a calibration in Settings (see [Calibration](#calibration-osl)) |
| Full range | Resets Start/Stop to the connected analyzer's own default range |

**Presets panel**

| Control | What it does |
|---|---|
| Band selector (if enabled) | Pick a ham band to set Start/Stop instantly |
| Add | Saves the *current* Start/Stop/Points as a new preset row |
| Delete | Removes the selected preset |
| Move up | Reorders the selected preset up one row |
| (double-click a row) | Applies that preset's Start/Stop/Points and re-ranges every chart |

**Scan buttons**

| Control | What it does |
|---|---|
| Single (F9) | Runs one sweep across the current range, then stops |
| Continuous (F10) | Sweeps repeatedly, updating the same trace in place, until you stop it |

**Measurements panel**

| Control | What it does |
|---|---|
| Open / Save | Load or save a single measurement as AntScopeZ's own `.asd` format |
| Delete | Removes the selected measurement |
| Clear | Removes *every* measurement in the list |
| Row checkbox | Shows/hides that measurement's trace on the charts |
| Row pencil icon | Renames the measurement |

**Chart tabs**: SWR, Phase, Z=R+jX, Z=R‖+jX, RL, Smith, TDR, Multi. A
"User defined" tab and an S21 tab also exist in the code but aren't
currently reachable -- both are unfinished/disabled features, not
something you're missing in the UI; see `BUILDINFO.md`'s Known Issues
for the full detail on either.

### Keyboard and mouse shortcuts, in the plot area

| Key / gesture | What it does |
|---|---|
| F1 – F7 | Jump to the SWR / Phase / Z=R+jX / Z=R‖+jX / RL / Smith / TDR tab (Multi has no shortcut of its own) |
| F9 / F10 | Single / Continuous scan -- same as the Single/Continuous buttons |
| Esc | Stop/interrupt the current scan |
| Delete | Delete the selected measurement |
| Mouse scroll | Zoom the current chart's frequency (X) range in/out |
| `+`, `=`, ↑ | Zoom the current chart's frequency (X) range in |
| `-`, ↓ | Zoom the current chart's frequency (X) range out |
| ←, → | Pan the current chart's frequency range left/right |
| Control (Command on macOS) + Mouse scroll | Zoom the Y-axis scale in/out |
| Ctrl + `+` / Ctrl + ↑ | Zoom the Y-axis scale in (same as Ctrl+scroll) |
| Ctrl + `-` / Ctrl + ↓ | Zoom the Y-axis scale out |
| Ctrl + 0 | Reset the Y-axis scale to default |
| Ctrl + C | Copy the current chart to the clipboard as an image |

These are the two gestures a hint overlay used to draw directly on every
chart (turned off -- it collided with the axis' own tick labels in the
corners where it was drawn); worth knowing since there's currently no
on-screen reminder that scroll/Ctrl+scroll do anything at all.

*(Ctrl+Alt+Shift+M and Ctrl+Alt+Shift+N trigger internal auto-calibration
debug routines gated behind an internal developer flag that's currently
disabled in the shipped build -- not reachable, and not meant for normal
use even when it is.)*

## Settings

The Settings dialog has seven tabs: **General**, **Markers**,
**OSL Calibration**, **Cable**, **Themes**, **Developer**, and
**Updates**.

OSL Calibration has its own section -- see
[Calibration (OSL)](#calibration-osl).

### General tab

<!-- SCREENSHOT: Settings dialog, General tab -->

Language, Band Highlighting, Show Band Name, and Band Selector used to
live here too -- they moved to the **View** menu (see
[Menu bar](#controls-reference) above) and aren't duplicated in Settings
any more. Theme also moved to the **View** menu, but that only picks
*which* of the 5 built-in themes is active -- to actually edit a
theme's colors, see the new [Themes tab](#themes-tab) below.

| Control | What it does |
|---|---|
| Connect analyzer | Opens the device-connection dialog -- see [Connecting to your analyzer](#connecting-to-your-analyzer) |
| Register application / Match license / Register device / Update license / Device info | RigExpert's own registration and licensing system. This talks to RigExpert's servers and isn't something this fork tests or supports -- see the disclaimer in [README.md](https://github.com/K4HEZ/AntScopeZ#readme). Use the vendor's own software for anything licensing-related. |
| Measurement system | Metric or Imperial units |
| Max measurements | Cap on how many measurements can be displayed at once |
| Don't restrict frequency | Would disable Start/Stop range clamping entirely -- currently hidden, gated behind the disabled internal developer flag (see `BUILDINFO.md`'s Known Issues) |
| System impedance | The reference impedance (default 50Ω) everything -- SWR, Smith chart center, RL -- is calculated against |
| Open 'Connect Analyzer' on launch | See [Connecting to your analyzer](#connecting-to-your-analyzer) |
| Data folder (with Browse...) | Where save/export/screenshot dialogs across the app default to -- see [Files and directories](#files-and-directories) |
| Save actions update this folder | Off by default. When on, completing a *save* (not Open/Import) somewhere else moves Data folder there too, so it follows you; when off, Data folder only changes when you set it here yourself |

### Markers tab

<!-- SCREENSHOT: Settings dialog, Markers tab -->

**Marker behavior**

| Control | What it does |
|---|---|
| Maximum number of markers | Cap on how many markers can be placed at once (1–5) |
| Automatically set a marker at the lowest SWR | On by default. Right after a single/full scan finishes (never during Continuous), places a marker at the swept trace's lowest-SWR point -- only if a marker slot is still free, otherwise it's a silent no-op. It's an ordinary marker once placed; deleting it later doesn't change this setting or stop it firing again next scan. |

**Columns**

| Control | What it does |
|---|---|
| Available / Selected lists | Choose which data columns the [Markers](#markers) popup shows, and in what order -- move columns between the two lists (or reorder within Selected) with the arrow buttons. Del/Marker/#/FQ are pinned at the top of Selected and can't be removed or reordered; everything else is up to you. |

### Cable tab

<!-- SCREENSHOT: Settings dialog, Cable tab -->

Lets you tell AntScopeZ about your feedline, so it can account for cable
loss/length in what it shows you -- useful when your analyzer is some
distance from the antenna through lossy coax. Also independently feeds
the TDR chart's distance axis (velocity factor only -- see
[TDR](#tdr-time-domain-reflectometry)).

**Preset vs. Custom**

A radio pair controls whether the cable-specification fields below are
locked or hand-editable:

| Control | What it does |
|---|---|
| Preset | The cable dropdown is enabled; velocity factor, R0, conductive/dielectric loss, loss units, and frequency are locked to whatever cable you've selected there (shown, but not editable) -- picking a different cable re-applies its numbers immediately |
| Custom | The cable dropdown is disabled; all of those fields become hand-editable instead -- use this for a cable not in the list (e.g. off a manufacturer datasheet), or after using Tools → TDR Analysis's "Use this velocity factor" (see below), which always switches to Custom rather than pretending a reverse-solved number matches some named cable |

Locked (Preset) fields are styled distinctly from Qt's normal "disabled"
dimming -- full-contrast text on a flattened background, so they read as
"showing a fixed value" rather than "unavailable".

**Cable specifications**

| Control | What it does |
|---|---|
| Cable dropdown | Pick a built-in ideal cable (50/75/25/37.5Ω) or one of the ~150 real-world cables from `cables.txt` (Belden part numbers, sourced from ac6la.com) -- only usable in Preset mode |
| Velocity factor, Cable R0 | Your feedline's velocity factor and characteristic impedance |
| Conductive loss, Dielectric loss | Loss figures for the cable, in dB/100ft, dB/ft, dB/100m, or dB/m (pick the unit from the dropdown next to them), specified either "at" a given frequency or as "any frequency" |

**Transmission line options**

Cable length and the three mode buttons live together here, separately
from Cable specifications above, because length isn't a property *of* a
particular cable the way R0/loss/velocity factor are -- it's how much of
it you actually have -- and because it (along with R0/loss/units/
frequency) only affects anything once Subtract or Add cable is selected.
Both stay editable regardless of Preset/Custom.

| Control | What it does |
|---|---|
| Cable length | Your feedline's physical length, in ft or m following the app's Metric/Imperial setting (Settings → General → Measurement system) |
| Do nothing | Use the measured impedance as-is -- no cable model applied (the default) |
| Subtract cable | De-embedding: removes this cable's modeled effect from the measurement, showing the antenna's true impedance at its own terminals. Use this when you measured *through* a known feedline and want to see past it. |
| Add cable | Embedding: the reverse -- projects a bare measurement forward through the modeled cable, showing what the radio end would actually see. |

The math behind Subtract/Add is a real lossy-transmission-line model
(`Measurements::calcFarEnd()`), using velocity factor, R0, conductive/
dielectric loss, and cable length together -- not just a cosmetic toggle.

| Control | What it does |
|---|---|
| Export | Exports the current/most recent measurement to a Touchstone file, with a comment block describing the active cable settings (Subtract/Add, velocity factor, length, R0, loss) embedded in it. Needs at least one scan first -- with none, it shows a notification instead of opening. |
| Update graphs | Applies whatever's currently in this tab immediately, without closing the dialog first |

### Themes tab

<!-- SCREENSHOT: Settings dialog, Themes tab -->

Edits any of the 5 fixed color themes (Light, Dark, Red, Green, Blue --
the same 5 View → Theme lists). Selecting one in the combo loads its
current colors into the form below; nothing here takes effect until you
hit Save.

| Control | What it does |
|---|---|
| Theme combo | Which of the 5 slots you're editing -- switching it discards any unsaved edits to the one you were on |
| Name | Freely renamable; shown as "N: Name" in both this combo and the View → Theme menu |
| Window Background / Text / Text Muted / Border / Chart Background / Marker | Six color swatches -- click one to open a color picker. The hex value is shown beside each. Chart Background and Marker apply to the plot area and marker lines; the other four are the general window canvas |
| Example panel | Live preview of the theme as you edit it -- not the saved version, whatever's currently in the form |
| Default | Restores this slot's original factory colors and name (not whatever's currently saved for it) |
| Cancel | Discards unsaved edits, reloads the slot as last saved |
| Save | Persists your edits. If you're editing the currently *active* theme (see View → Theme), the change applies immediately |

### Developer tab

<!-- SCREENSHOT: Settings dialog, Developer tab -->

Two group boxes:

**Custom Analyzer** -- everything in it is disabled; it's shown so you
can see it exists (and what it's meant to become), not because it
currently does anything. See
[Customized analyzer parameters](#customized-analyzer-parameters)
below for what it's for and why it's not safe to use yet.

**Debug Logging** -- four checkboxes, one per analyzer connection type:
Com/Serial, USB/HID, BLE/Bluetooth, and NanoVNA. Turning one on starts
dumping every raw byte sent and received over that connection --
timestamped, hex and ASCII side by side (traditional `hexdump`-style,
16 bytes/line), each line tagged `>>` for a byte the app sent or `<<`
for one it received -- into a shared log file. See
[Files and directories](#files-and-directories) for where that file
lives and what it looks like.

A fifth, indented checkbox under BLE/Bluetooth, **Show ping/keepalive
traffic**, is only enabled while BLE logging itself is on. BLE sends a
small keepalive packet once a second to detect a dropped connection;
useful to confirm it's alive, but it drowns out everything else in a
longer capture. Checked (shown) by default -- uncheck it to filter
just the pings out, without turning BLE logging off entirely. Serial,
USB/HID, and NanoVNA don't have an equivalent filter: their traffic
(including their own periodic keepalives) is always logged in full.

These checkboxes are session-only by design -- they always start
unchecked when you open AntScopeZ, regardless of how you left them
last time, so logging never keeps running silently in the background
across restarts. Turn them back on each time you actually want to
capture something.

### Updates tab

<!-- SCREENSHOT: Settings dialog, Updates tab -->

Your installed AntScopeZ version sits at the top, next to a
**Check for Software Updates** button -- there's no update-checking
mechanism built yet, so that button is a disabled placeholder for now.
Below it, the rest of the tab (analyzer info, "Update from file",
"Check for firmware updates") is disabled too, with a warning
explaining why: "Check for firmware updates" would contact RigExpert's
own servers directly, which this fork deliberately doesn't do. Get
firmware updates from RigExpert's own site/software instead.

## Interpreting your data

### SWR and Return Loss: what "good" looks like

SWR and RL (Return Loss) describe the same mismatch, in two different
units -- RL is logarithmic (dB), SWR is a ratio. Higher RL is better;
lower SWR is better. Rough conversion, for reference:

| SWR | RL (dB) | Roughly |
|---|---|---|
| 1.0 : 1 | ∞ | Perfect match (never actually happens) |
| 1.5 : 1 | ≈ 14 dB | Very good |
| 2.0 : 1 | ≈ 9.5 dB | Good, commonly cited as "acceptable" for most rigs |
| 3.0 : 1 | ≈ 6 dB | Marginal -- many radios start reducing power or refusing to transmit here |

Most modern transceivers tolerate up to somewhere around 2:1-3:1 before
their internal protection kicks in; check your radio's actual spec
rather than assuming.

The **RL** tab plots this same data on its own chart, in dB instead of
as a ratio. Worth switching to when comparing two *already-good*
matches against each other -- SWR's ratio scale compresses everything
below about 1.3:1 together near the bottom of the chart, where the
dB scale still spreads it out.

### Reading the dip: is my antenna too long or too short?

For a simple resonant antenna (a dipole or vertical cut for a specific
band), the SWR curve typically has one clear minimum -- the "dip" -- at
its actual resonant frequency. Where that dip sits relative to your
*target* frequency tells you which way to trim:

- **Dip to the left of (below) your target frequency** -- the antenna is
  resonating lower than you want, which for a simple wire/vertical
  usually means it's **electrically too long**. Shortening it raises the
  resonant frequency, moving the dip to the right, toward your target.
- **Dip to the right of (above) your target frequency** -- the opposite:
  the antenna is **electrically too short**. Lengthening it (or adding
  loading) moves the dip left.

The same read is available from `Z = R + jX` at your target frequency,
without needing to eyeball a chart: a small **positive X (inductive)**
at your target frequency means the resonant dip is below it (too long);
a small **negative X (capacitive)** means the dip is above it (too
short). At the dip itself, X is at or near zero.

This is the classic behavior of a simple resonant dipole/vertical --
it's a solid starting heuristic, not a universal law. Multi-band,
loaded, or otherwise non-resonant antenna designs (verticals with
matching networks, off-center-fed designs, etc.) don't necessarily
follow it the same way.

Trimming rule of thumb, for a simple dipole/vertical: the percentage
change in length needed is roughly the percentage change in frequency
you're trying to achieve (e.g. moving a dip up by 2% typically means
shortening by roughly 2%) -- treat this as a starting estimate and
re-measure after each cut, not an exact formula. Cut a little at a time;
wire you've already cut off doesn't grow back.

### Smith chart basics

The Smith chart plots impedance as a point (or, across a sweep, a
curve) on a circle. The very center of the chart is a perfect 50Ω match
(or whatever system impedance you've set in Settings → General →
"System impedance"); the further a point sits from center, the worse
the mismatch at that frequency. Points in the upper half are inductive
(+X), the lower half capacitive (−X). A sweep that traces a tight loop
close to center across your band of interest is a well-matched antenna
over that range; a curve that swings wide is not.

### Z = R + jX: resistance and reactance

`R` is the resistive part of impedance -- power delivered here actually
radiates (or is lost as heat). `X` is the reactive part -- energy
stored and returned, not radiated. At true resonance, X = 0 and the
antenna looks purely resistive; R at that point (ideally close to your
system impedance, commonly 50Ω) is what actually determines how good
the match is once X is out of the way. See
[Reading the dip](#reading-the-dip-is-my-antenna-too-long-or-too-short)
above for what the sign of X tells you off-resonance.

### Z = R ‖ jX: the parallel-equivalent view

Same underlying measurement as `Z = R + jX` above, just recomputed into
its parallel-equivalent-circuit form (`Rp`/`Xp`) instead of the series
form (`R`/`X`) -- two different, mathematically-equivalent ways to
model the same impedance as a simple two-component circuit. Series
(`R + jX`) is usually the more intuitive one to read for a plain
series-fed dipole or vertical; the parallel view earns its keep if
you're working with a parallel matching network or tuner, where
component values are easier to reason about directly in parallel form.

### Phase

Plots the reflection coefficient's phase angle against frequency -- the
same "phase" value shown in the Smith chart's cursor readout, in
degrees. It crosses through (or near) zero around resonance, mirroring
the same too-long/too-short read that [the sign of
X](#reading-the-dip-is-my-antenna-too-long-or-too-short) gives. How
*steep* that crossing is says something about Q: a fast swing across a
narrow frequency range points to a high-Q (narrowband) antenna or
match; a gentle, gradual slope points to a broader, lower-Q one.

### Q factor

"Q" is short for **Quality factor** -- same term as in general RF/filter
theory, just derived here from your actual swept data instead of a lab
Q-meter: `Q = center frequency ÷ 2:1-SWR bandwidth` (see
[Markers](#markers)' Tools > Marker Comparison "Q factor (Current)"
field, or the informal read from the Phase chart above).

- **Higher Q → narrower usable bandwidth.** SWR climbs back above 2:1
  close to resonance on either side, so you'll notice re-tuning (or a
  worse match) moving even a modest distance across the band. Typical
  of electrically short/loaded antennas -- mobile whips, loaded
  verticals with a coil, trap antennas -- and extreme for magnetic
  loops (Q often in the hundreds, tunable in kHz steps).
- **Lower Q → broader, more forgiving match.** Typical of a full-size
  resonant antenna (a proper half-wave dipole or quarter-wave vertical
  cut close to the actual operating frequency) -- SWR stays under 2:1
  across much more of the band without touching anything.
- **What it doesn't tell you:** whether a high reading is costing you
  efficiency. A lossy loading coil can produce the same SWR-bandwidth
  signature as a "clean" high-Q small antenna -- Q alone can't
  distinguish the two. It's a bandwidth/tuning-sensitivity indicator,
  not an efficiency meter.

Rule of thumb: single-digit-to-teens is a comfortably broad, close-to-
full-size match; several tens starts to mean noticeable re-tuning
across the band; hundreds means essentially a single-frequency device
(loop territory).

## Scan modes: Single vs. Continuous

**Single (F9)** runs exactly one sweep across the current range and
stops. Good for a one-off check.

**Continuous (F10)** keeps sweeping the same range repeatedly, updating
the *same* trace in place each pass rather than adding a new entry to
Measurements every time. This is the mode to use while physically
adjusting an antenna (trimming a wire, tuning a matcher) -- start
Continuous, watch the SWR dip move in real time as you adjust, and stop
it once you're happy. Only when it's stopped (or you run a fresh Single
scan) does the result settle as one finished entry in the Measurements
list.

## Calibration (OSL)

<!-- SCREENSHOT: Settings dialog, OSL Calibration tab / Calibration Wizard -->

OSL (Open/Short/Load) calibration corrects for the analyzer's own
measurement error, using three known reference standards. It's
per-device -- calibration data is stored under the connected analyzer's
own serial number, so switching analyzers doesn't mix up calibration
data between them.

Settings → OSL Calibration has two ways to run it:

- **Calibration Wizard** -- one **Start** button walks you through all
  three standards in order: connect Open and click OK, then Short, then
  Load, with each step confirmed by a dialog before proceeding.
- **Individually** -- each of the Open/Short/Load sections has its own
  "Start _ Calibration" button, for redoing just one standard without
  repeating all three. Each section also has an "Open file" button, to
  load a previously-saved calibration standard from disk instead of
  re-measuring it live.

Performing a calibration and applying it are two separate steps. Each
standard writes its own file (`cal_open.s1p`, `cal_short.s1p`,
`cal_load.s1p`) under that analyzer's calibration folder; the
**Calibration** checkbox in the main Frequency panel applies that
correction to your scans, but only once all three files actually exist.

If you check that box before all three are present, AntScopeZ shows a
"Calibration Required" prompt and unchecks it again -- it's literally
checking for those three files, not tracking calibration status any
other way. Running the wizard (or the three individual standards) is
what creates them; once they exist, the checkbox works.

## Presets and bands

**Presets** are saved Start/Stop/Points combinations, shown as a table
above the Measurements list. Click **Add** to save whatever range is
currently entered, or double-click an existing row to jump straight to
it (updates Start/Stop/Points and re-ranges every chart in one step).
**Delete**/**Move up** manage the list from there.

The **band selector** (View menu → "Band Selector") is a faster
shortcut for the common case: instead of building your own preset, pick
a named ham band from the dropdown above Presets and Start/Stop are set
for you immediately, formatted as `<name> (<start> - <stop> kHz)`.
Which bands show up depends on the region picked in View menu → "Band
Highlighting" (backed by `itu-regions.txt`/`itu-regions-defaults.txt`)
-- Edit menu → "Edit ITU Bands..." opens the band editor if you need to
add or adjust one for that region.

### Editing band definitions

<!-- SCREENSHOT: Edit Bands dialog -->

The band editor is a plain text editor over the region data, not a
structured form -- each line is
`start kHz, stop kHz, band name` (a trailing name is optional; an
unnamed 2-field line still defines a highlighted range, just without a
label). One `[Region Name]` header line groups the bands under it, e.g.:

```
[ITU Region 1 - Europe, Africa]

	135.7, 137.8, 2200m
	1810, 2000, 160m
	14000, 14350, 20m
```

**Restore Defaults** reloads the shipped `itu-regions-defaults.txt`
(discarding your edits in this dialog, not saving over anything until
you click Save). **Save** writes your edited text to your own
`itu-regions.txt`, which is what actually gets read from then on --
the shipped defaults file itself is never modified. **Cancel** discards
whatever you typed.

## Markers

Double-click anywhere on a frequency-domain chart (any tab except Smith
and TDR, where a marker wouldn't mean the same thing) to drop a
numbered marker at that frequency -- or right-click and choose **Create
marker** from the context menu. Markers appear at the same frequency
across every chart at once (SWR, Phase, Rs, Rp, RL, S21), each labeled
with a matching number, so you can track one frequency point across
multiple views simultaneously. You can place up to
[Settings → Markers → "Maximum number of markers"](#markers-tab) at once
(5 by default); once you hit that cap, double-clicking to add another
shows a brief notification instead of placing one.

Hovering shows a readout of that marker's values (frequency, SWR, RL,
R/X/Z, and more, depending on the chart) in a popup table. The View
menu's **Markers Hint** controls whether that readout pops up
automatically; which columns it shows, and in what order, is set from
[Settings → Markers](#markers-tab).

By default, AntScopeZ also places one for you: right after a
single/full scan finishes, a marker drops at the trace's lowest-SWR
point automatically (never during Continuous, and only if a slot is
free) -- turn this off at
[Settings → Markers → "Automatically set a marker at the lowest
SWR"](#markers-tab).

### Tools > Marker Comparison

Compares two already-placed markers -- **Current (dip)** and **Target
(desired)** -- and estimates how much to trim an antenna to move its
resonance from one to the other.

| Field | What it shows |
|---|---|
| Q factor, Equiv. L, Equiv. C (Current) | Derived from the Current marker alone -- Q is the classic bandwidth definition (center frequency ÷ 2:1-SWR bandwidth, walked from your actual swept data, not a lab Q-meter reading), read up on what it means at [Interpreting your data](#interpreting-your-data) |
| ΔFrequency, ΔSWR, ΔRL, ΔR, ΔX | Target minus Current -- Δ Frequency works off marker placement alone (no scan needed yet); the rest need real measurement data from both markers |
| Antenna type / Current length | Feeds the trim estimate below -- length is optional; leave it blank and a nominal half/quarter-wave formula stands in |
| Calculated trim, Suggested first trim, Per leg | The full calculated adjustment, and a conservative first cut -- shortening suggests half the calculated amount (cutting can't be undone); lengthening suggests 1.5× it (added wire can always be trimmed back down later) |

Estimate only -- velocity factor and end effects aren't modeled. Cut the
suggested amount, re-measure, and repeat rather than cutting the full
calculated trim at once.

## Multi view

The **Multi** tab lets you stack two or more charts for the *same* measurement, 
or compare markers across them, in one 
view -- useful for eyeballing return loss and SWR together instead of
flipping between tabs. Right-click a chart's tab and choose "Move chart
to the tab Multi" (or "Add multi-charts") to populate it.

You can add and remove tabs to Multi View using the "+" button to join charts.

## Data from AA

<!-- SCREENSHOT: Data from AA dialog (the stored-measurements list) -->

Loads measurement results that already exist in the *analyzer's own*
on-device memory (not files on your PC -- see
[Import / Export](#import--export) for that). **File → Data from AA**
opens a list of everything currently stored on the device;
double-click an entry (or select it and click OK) to load just that one
into AntScopeZ as a new measurement.

**Read and Save all** instead walks the *entire* list automatically:
pick a destination folder, and it loads and saves every stored entry in
turn as its own `.asd` file (zero-padded index + the device's own name
for each), with a progress dialog you can Abort partway through.

## Import / Export

These are two different File menu items, doing related but distinct
things:

- **File → Export Data...** opens a dialog for the measurement
  currently *selected* in the Measurements list, offering:
  - **CSV** -- comma-separated values
  - **NWL** -- APAK-EL format
  - **Z, RI** / **S, RI** / **S, MA** -- Touchstone (`.s1p`), as
    impedance or S-parameters, in rectangular (real/imaginary) or polar
    (magnitude/angle) form
- **File → Import Data...** is the general "bring external data in"
  action -- accepts Touchstone (`.s1p`), CSV, NWL, or AntScopeZ's own
  `.asd`.

Separately, the **Measurements panel's own Open/Save** buttons are
narrower: they only read/write AntScopeZ's native `.asd` format, for one
measurement at a time.

Export and Save both default to your [Data folder](#files-and-directories),
suggesting a filename built from the measurement's own name (Save) or
description (Export) rather than whatever you last typed. Import and
Open default to the same folder but don't move it -- browsing somewhere
else to import a one-off file doesn't change where your own saves land
afterward.

## Print and screenshots

<!-- SCREENSHOT: Print dialog -->
<!-- SCREENSHOT: Screenshot from AA dialog (the comment/export controls, not just the captured image already on the Pages site) -->

Three related but different ways to get a chart out of AntScopeZ as an
image or document:

- **File → Save Screenshot...** saves the *current chart tab* straight
  to a PNG file you pick -- the same image Ctrl+C copies to the
  clipboard, just written to disk instead. Not available for Multi.
- **File → Print...** opens a dedicated dialog: a preview of the current
  chart, the markers table beneath it, an auto-generated header (e.g.
  "SWR graph") that isn't user-editable in this dialog, a free-text
  Comment box, and a Line width slider affecting the printed/exported
  trace thickness. From there:
  - **Print** sends it to your system's print dialog.
  - **Export PDF** / **Export PNG** save it directly to a file instead,
    with the same header/chart/markers/comment layout.

  The Print button/dialog isn't available while the Multi tab is
  active -- clicking it does nothing in that case.
- **File → Screenshot from AA** captures the *analyzer's own* on-device
  screen (not every model supports this -- see
  [Supported Devices](../SUPPORTED_DEVICES.md)) and opens its own small
  dialog: add an optional comment, then **Export to PDF**,
  **Export to BMP**, or **To clipboard**. **Refresh** re-captures the
  device's screen again without closing the dialog, in case it's
  changed since it was first captured.

All of the above default to your [Data folder](#files-and-directories),
with a timestamped suggested filename (`Screenshot_yyyyMMdd-hhmmss.png`
for Save Screenshot, `AnalyzerScreen_yyyyMMdd-hhmmss.pdf`/`.bmp` for
Screenshot from AA) rather than reusing whatever was typed last time.

## TDR (Time Domain Reflectometry)

### What a TDR scan actually measures

A TDR run in AntScopeZ is not a separate kind of measurement -- it's a normal
frequency sweep, just an unusually wide one. Switch to the TDR tab *before*
clicking Single/Full -- that's what tells AntScopeZ to scan near-DC to your
connected analyzer's own maximum supported frequency (or a customized
analyzer's max, if you're using one), instead of whatever Start/Stop range is
in the Frequency panel; the number of points is clamped to a 200–1000 range
regardless of your usual Points/Speed-Accuracy setting. Continuous doesn't
work on this tab -- TDR is single/full-scan only. That sweep then runs
through the exact same measurement pipeline as a regular Single/Continuous
scan -- a genuine, real complex-impedance sweep across that whole span,
commonly on the order of 100 kHz up to several hundred MHz depending on your
analyzer.

The TDR tab then runs an inverse FFT over that near-DC-to-wideband sweep to
turn it into a time-domain impulse/step response -- which is what lets you see
reflections (bad connectors, cable damage, impedance bumps) at a distance
along the cable, instead of as a function of frequency.

### Why the same scan shows up in the other charts too

Because a TDR scan is just a regular sweep under the hood, every other
frequency-domain chart (SWR, Z=R+jX, Z=R‖jX, Return Loss, Phase, S21) is
simply a different view of that same raw data, so they populate right along
with the TDR chart. This is expected, not a bug.

The reverse doesn't happen: a normal band-limited scan (say, just your 20m
band) never shows up in the TDR chart, because the inverse-FFT math behind
TDR requires the sweep to start near DC. A narrowband scan doesn't satisfy
that, so TDR is correctly left empty in that case.

### Reading the data with that in mind

The wideband sweep behind a TDR run is real, valid data -- but keep two things
in mind before treating it like a normal scan of your operating band:

- The scan uses a fixed number of points spread across the *entire* span (near
  DC up to potentially hundreds of MHz), so resolution within any one narrow
  band of interest is much coarser than a dedicated scan of just that band
  would give you.
- Most antennas only behave meaningfully near their design frequencies. The
  part of the curve well above your antenna's intended range is real
  data, but it's typically just showing genuine out-of-design-range behavior
  (noisy, reactive, not meaningful for tuning) rather than a second usable
  band.

So: if you run a TDR scan and then flip to SWR/Multi and see a curve running
all the way from ~100 kHz to ~500 MHz, that's expected -- it's the same sweep
TDR needed, just viewed through a different chart.

### Tools > TDR Analysis

Reads the results of your last TDR scan and answers three questions
directly instead of making you eyeball the chart:

| Field | What it shows |
|---|---|
| Cable type / Velocity factor | A preset from `cables.txt` (~150 real cables), or an editable custom value -- local to this dialog by default, doesn't change Settings → Cable unless you click "Use this velocity factor" below |
| Distance to strongest reflection | The single biggest reflection in the scan, at whatever velocity factor is currently set above -- recalculates live as you change it, no re-scan needed (distance is exactly linear in velocity factor) |
| Reflection | Open / high impedance, or Short / near 0Ω, based on the sign of that reflection -- or "None detected" if nothing crosses the noise floor |
| Known cable length → Calculated velocity factor | The reverse direction: type in a length you've actually measured, get the velocity factor that makes the two agree -- exact, not trial-and-error |
| Use this velocity factor | Applies the solved value to Settings → Cable as Custom (resetting R0/loss to "no loss modeled" rather than keeping whichever preset's real numbers happened to be showing), and updates the TDR chart's own distance axis immediately |

**Workflows this answers:**

- **Bad SWR, feedline or antenna?** Compare "Distance to strongest
  reflection" against your feedline's actual physical length. Close
  match → that's just the antenna feedpoint, the problem is the antenna
  itself. Reflection shows up *short* of the real length → a fault
  partway along the cable (bad connector, damage, water intrusion) --
  now you know roughly where to look.
- **Unmarked/unknown cable -- is it open, shorted, or damaged?** Reliable
  regardless of velocity factor -- leave the far end open or shorted as
  a deliberate test (or see what the mystery termination gives you) and
  read "Reflection" directly. This is the one question here that doesn't
  need you to know anything about the cable first.
- **Unmarked cable -- how long is it?** Circular unless you know one of
  {length, velocity factor} already. If you can physically measure it
  (even coiled), use the reverse calculator to solve for velocity
  factor, then eyeball `cables.txt`'s presets for a plausible match by
  VF + R0. If you can't measure it at all, you're limited to guessing a
  plausible preset and accepting the length reading is only as good as
  that guess.
- **Spool of wire -- how much is left?** Same technique as above, but
  only if it's actually coax (or another real two-conductor
  transmission line) -- this doesn't apply to bare single-conductor
  antenna wire, which has no meaningful velocity factor without pairing
  it against a ground return. For that, a tape measure is the real
  answer.

A maximum unambiguous distance is set by the scan's own resolution (point
count) -- a long run may exceed what one scan can resolve; the dialog
flags this when the peak sits near the edge of that range.

## Customized analyzer parameters

Settings' **Developer** tab's **Custom Analyzer** group box is intended
to let you define a named analyzer preset -- a custom minimum/maximum
frequency range plus an LCD width/height -- for a unit AntScopeZ
already recognizes correctly (a clone, or a newer hardware revision of
a known model) whose real frequency range differs from what AntScopeZ
assumes for that model.

It's visible (a "This feature is currently under development"
notice sits at the top of it), but every control on it -- "Use
customized analyzer", Apply, Auto calibration -- is disabled, so
there's nothing to actually interact with yet. It's shown rather than
hidden so it isn't forgotten about, not because it's ready to use:
exercising the underlying feature previously turned up real problems --
a crash on some paths, a silently-ignored custom range on others, and
an outright device-protocol rejection when actually scanning with it
enabled. See `BUILDINFO.md`'s Known Issues for the full technical
writeup (what's fixed, what's still broken, and where in the code) if
you're looking to pick this back up.

## Files and directories

Where AntScopeZ actually keeps things, on Linux (the platform this was
verified against -- see the note at the end of each subsection for
Windows/macOS). Everything below is either read directly from an
installed `.deb`, or from a real config file generated during this
session's own testing.

### What the `.deb` installs

| Path | What's there |
|---|---|
| `/usr/bin/AntScopeZ` | A thin wrapper script -- sets `LD_LIBRARY_PATH` to the bundled Qt below, then execs the real binary |
| `/usr/bin/AntScopeZ.bin` | The actual executable |
| `/usr/bin/qt.conf` | Points Qt's own plugin/library lookup at the bundled copies instead of any system Qt |
| `/usr/lib/x86_64-linux-gnu/antscopez/` | AntScopeZ's own private copy of the Qt 6.11 libraries and plugins it was built/packaged against -- see [Qt version](https://github.com/K4HEZ/AntScopeZ#qt-version) for why it's bundled rather than linked against whatever Qt the system has |
| `/usr/share/antscopez/` | Read-only shared data: `cables.txt`, `itu-regions-defaults.txt`, and every `QtLanguage_<code>.qm` / `qtbase_<code>.qm` translation file |
| `/usr/share/applications/antscopez.desktop` | The desktop entry (app menu listing) |
| `/usr/share/icons/hicolor/64x64/apps/antscopez.png` | The app icon |

(Different install prefix than `/usr`? Everything under `/usr/...` above
follows that prefix instead -- `sharedDataFolder()`/`ANTSCOPE_SHARED_DATA_DIR`
is computed from it at build time, not hardcoded.)

**Windows/macOS:** no installer package yet, so this table doesn't apply
-- both keep the simpler "everything sits next to the executable" layout
a dev build uses on every platform (see the next section for where
*your own* files still live either way).

### Your own files: `~/.config/AntScopeZ/`

This is the one directory you actually own -- back it up, sync it,
whatever you like. Nothing the `.deb` installs is ever written to.

| Path | What's there |
|---|---|
| `AntScopeZ.ini` | Every setting -- see [AntScopeZ.ini reference](#antscopezini-reference) below |
| `Calibration/<analyzer serial number>/` | `cal_open.s1p`, `cal_short.s1p`, `cal_load.s1p` -- one subfolder per analyzer, see [Calibration (OSL)](#calibration-osl) |
| `itu-regions.txt` (only if you've edited bands) | Your own edited band data, created the first time you click Save in the [band editor](#editing-band-definitions) -- overrides the shipped `itu-regions-defaults.txt` entirely, not merged with it |
| `QtLanguage_<code>.qm` / `qtbase_<code>.qm` (optional) | Drop a `.qm` here to add a language AntScopeZ doesn't ship, or override a shipped one -- picked up automatically, no reinstall needed. See View menu → Language in [Controls reference](#controls-reference). |

**Windows:** the equivalent per-user folder is wherever Qt's
`GenericConfigLocation` resolves to (typically
`%APPDATA%\AntScopeZ\`). **macOS:** your home folder directly
(`QStandardPaths::HomeLocation`), not a dotfile -- look for an
`AntScopeZ` folder there.

### Your Data folder: `~/Documents/AntScopeZ/` (by default)

Separate from the config folder above -- this is where the *files you
actually work with* land: exported measurements, `.asd` saves,
screenshots, PDF/PNG prints, and (see below) debug logs. Every
save/export/screenshot dialog across the app defaults here, created
automatically the first time it's needed.

It's not fixed at that path -- change it any time from Settings →
General → Data folder (Browse...). Everything landing in one place by
default, rather than each dialog remembering its own separate folder
independently, is deliberate (see CHANGELOG.md if you're curious what
that replaced). Whether *saving* somewhere else should relocate this
folder for next time, or leave it where you set it, is up to the
adjacent "Save actions update this folder" checkbox -- off by default.
Opening/importing a file from elsewhere never relocates it either way.

Filenames are generated for you rather than reused from last time: a
measurement's own name for `.asd` Save and Export, the Print dialog's
title field for Print, and a timestamp (`yyyyMMdd-hhmmss`, sorts
correctly regardless of locale) for screenshots -- see
[Import / Export](#import--export) and
[Print and screenshots](#print-and-screenshots) above for specifics.

#### Debug logs: `Debug-yyyyMMdd.log`

Written here too, when you turn on one or more of Settings → Developer
→ Debug Logging's checkboxes (see [Developer tab](#settings)) -- one
shared file per calendar day, appended to across the day (including
across restarts), interleaving whichever of Serial/USB-HID/BLE/NanoVNA
you had logging turned on for so the order things actually happened in
is preserved. Every line is flushed to disk immediately, so the file
is still useful even if the app crashes right after something's
logged.

Format is a traditional hex+ASCII dump, 16 bytes per line, each line
ending `>>` (the app sent this) or `<<` (the app received this):

```
2026-08-14 14:32:07.123 BLE TX (20 bytes)
00000000  5A 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |Z...............| >>
00000010  00 00 00 3A                                       |...:| >>
```

This is raw wire traffic, not an interpretation of it -- exactly the
bytes sent/received, nothing decoded or summarized. Handy to attach if
you're reporting a connection problem with a specific analyzer.

### `AntScopeZ.ini` reference

This is a real, working config from actual use -- not a synthetic
example -- lightly trimmed of pure window-geometry noise. Groups you'd
actually want to hand-edit or just recognize:

```ini
[General]
UserDataDir=/home/you/Documents/AntScopeZ
UserDataDirFollowsSaves=false

[MainWindow]
languageCode=es
measureSystemMetric=true
rangeLower=143970
rangeUpper=147970
systemImpedance=50
dotsNumber=50
isRange=false

[Settings]
band-selector-enabled=true
activeTheme=0
current_band=ITU Region 2 - Americas
maxMarkers=5
maxMeasurements=5
open-connect-analyzer-at-launch=false
restrictFq=true
show-band-name=true

[Theme0]
name=Light
windowBackground=#f6f6f6
text=#1e1e1e
textMuted=#6e6e6e
border=#c3c3c8
chartBackground=#ffffff
marker=#ff0000

[Connection]
id=180000756
name=Match
same=false
type=0

[Markers]
header=0,1,2,3,4,5,6,7,8,9
markersHintEnabled=true

[Cable]
R0=50
VelFactor=0.66
ConductiveLoss=0
DielectricLoss=0
LossUnits=0
LossFrequencyMHz=1
LossAtAnyFrequency=0
Length=0
CableIndex=0
FarEndMeasurement=0

[Calibration]
Z0=50
DotsNumber=500
Performed=false
Enabled=false
OpenPath=/home/you/.config/AntScopeZ/Calibration/<serial>/cal_open.s1p
ShortPath=/home/you/.config/AntScopeZ/Calibration/<serial>/cal_short.s1p
LoadPath=/home/you/.config/AntScopeZ/Calibration/<serial>/cal_load.s1p

[CustomAnalyzers]
use_customized=false
current_alias=
```

Notes on specific keys:

- **`languageCode`** -- an ISO code (`es`, `ja`, `uk`, ...) matching a
  `QtLanguage_<code>.qm` filename, not an index. Delete this line (or
  the whole ini) to fall back to English.
- **`activeTheme` / `[Theme0]`-`[Theme4]`** -- which of the 5 fixed
  theme slots is active, and each slot's own colors -- see
  [Themes tab](#themes-tab). A slot missing here just falls back to its
  compiled-in factory default; only shows up once you've hit Save on it
  at least once. Hand-editing works (hex strings, same format shown
  above), but the Themes tab is the supported way to change these.
- **`Connection`** -- the last-connected device, used for silent
  auto-reconnect at launch (see
  [Connecting to your analyzer](#connecting-to-your-analyzer)). `same`
  tracks the "Use same selection for future connections" checkbox.
- **`Calibration`**'s `Performed`/`Enabled` here are just what gets
  written back out on exit -- the app's actual live check is whether the
  three `*Path` files exist on disk, not this flag (see
  [Calibration (OSL)](#calibration-osl)).
- **`[Markers]header`** -- the Markers popup's column list and order,
  same value Settings → Markers' Available/Selected lists edit. Not
  bookkeeping -- hand-editing it works, but the Settings tab is the
  supported way to change it.
- **`[General]UserDataDir`/`UserDataDirFollowsSaves`** -- the Data
  folder shown in Settings → General and the "Save actions update this
  folder" checkbox next to it; see
  [Files and directories](#files-and-directories) above. Safe to
  delete -- it just regenerates at the default location next launch.
  `[General]` also holds unrelated window-position bookkeeping (see
  below), sharing the section with these two isn't meaningful.
- Everything else not listed above (`[General]`'s other keys, `Hint`,
  `BriefHint`, `[Markers]`'s other keys (`x`/`y`/`mainX`/`mainY`/
  `mainBiasX`/`mainBiasY`/`markersHintEnabled`), per-tab `*ZoomState`,
  `mainX`/`mainY`/`geometry`, ...) is internal window-position/
  zoom-state bookkeeping. Harmless to delete individually if something
  looks stuck -- it just regenerates with defaults.
- **Developer tab's four "Enable ... debug logs" checkboxes (and BLE's
  "Show ping/keepalive traffic") are never written here at all** --
  deliberately session-only, always starting unchecked. See
  [Developer tab](#settings).

If your `.ini` has a leftover group named in another language (e.g.
`[Marcadores]` sitting next to `[Markers]`) from before this was fixed
(see CHANGELOG.md), it's an orphaned duplicate of the `Hint`/`Markers`/
`BriefHint` popup-position bookkeeping above -- harmless, safe to
delete.

## Troubleshooting

- **"Calibration Required" pops up, or the Calibration checkbox won't
  stay checked.** AntScopeZ can't find `cal_open.s1p`/`cal_short.s1p`/
  `cal_load.s1p` for this analyzer yet -- run the Calibration Wizard (or
  all three individually) first. See
  [Calibration (OSL)](#calibration-osl).
- **Clicking Print does nothing.** Print isn't available while the
  Multi tab is active -- switch to any other chart tab first. See
  [Print and screenshots](#print-and-screenshots).
- **Some file-dialog text (e.g. "Files of type:") stays in English no
  matter what language is selected.** That's a gap in Qt's own shipped
  translation, not something this app controls -- see `BUILDINFO.md`'s
  Known Issues.
- **The analyzer doesn't reconnect automatically at launch.** Check
  that Settings → General → "Open 'Connect Analyzer' on launch" is
  checked, and that "Use same selection for future connections" was
  checked the last time you connected. See
  [Connecting to your analyzer](#connecting-to-your-analyzer).
- **A language `.qm` file was dropped in but isn't showing up in the
  Language list.** Confirm the filename matches `QtLanguage_<code>.qm`
  exactly and it's in the right folder -- see
  [Files and directories](#files-and-directories).
- **"Check for firmware updates" is greyed out / does nothing.** That's
  deliberate -- it would contact RigExpert's own servers directly, which
  this fork doesn't do. See [Settings' Updates tab](#updates-tab). Get
  firmware updates from RigExpert's own site/software instead.
- **TDR chart is empty after a scan.** TDR only populates from a
  wideband, near-DC sweep -- a normal band-limited scan (e.g. just 20m)
  won't show anything there. See
  [TDR (Time Domain Reflectometry)](#tdr-time-domain-reflectometry).
- **Developer tab's Custom Analyzer controls are all greyed out.**
  Deliberate, not a bug -- the feature underneath is unfinished. See
  [Customized analyzer parameters](#customized-analyzer-parameters).
- **Debug logging was on, but the file is missing or empty.** The
  checkboxes reset to unchecked every time you open AntScopeZ (by
  design -- see [Developer tab](#settings)), so check they're still on;
  and a checkbox only logs traffic for *that* connection type, so
  nothing gets written unless something's actually connected and
  talking over it. See [Files and directories](#files-and-directories)
  for the exact file location.
- **Cable loss compensation ("Subtract cable"/"Add cable") doesn't
  seem to change anything.** Flagged as unverified in
  [the Cable tab reference](#cable-tab) -- it may not currently do
  anything at all; not confirmed either way.
- **There's no S21 tab anywhere, even though it's mentioned in this
  guide/the code.** The code's own comment says why: `// S21 not
  implemented yet` (`mainwindow_tabs.cpp`). Not something you're missing
  in the UI -- it's genuinely unfinished (not dead: the measurement
  pipeline is substantially built already). See `BUILDINFO.md`'s Known
  Issues for the full detail on what's there and what's still missing.

---

*This guide was drafted with [Claude](https://www.anthropic.com/claude)
(Anthropic's AI), grounded in AntScopeZ's actual source code -- not yet
verified line-by-line against real hardware. Found something wrong?
[Open an issue](https://github.com/K4HEZ/AntScopeZ/issues).*
