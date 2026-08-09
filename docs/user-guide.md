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

You may notice `antscopez (>= <version>)` listed as one of the
package's *own* dependencies -- that's a known, harmless cosmetic
byproduct of how the bundled Qt libraries get their dependency
declared (see `BUILDINFO.md`'s packaging notes), not a sign of a broken
build. It doesn't block installation; apt/dpkg treat a package as
trivially satisfying a dependency on itself.

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

## Getting started

### Connecting to your analyzer

Settings → General → **Connect analyzer** opens the "Select device"
dialog. Pick a connection type (USB, COM, or BLE), click **Scan**, select
your device from the list, and **Connect**. "Use same selection for
future connections" saves that choice so AntScopeZ can silently
reconnect on its own next time, instead of asking again.

By default, this dialog also pops up automatically ~500 ms after launch
if there's no valid saved device to silently reconnect to. If you'd
rather it not do that -- say, you're just reviewing saved `.s1p`/`.asd`
files with no analyzer connected -- uncheck Settings → General → "Open
'Connect Analyzer' on launch". The manual Connect button is unaffected
either way.

Once connected, the window's title bar shows the device's model/name
instead of "Analyzer not connected".

### Your first scan

1. **Set a frequency range.** Either type Start/Stop directly into the
   Frequency panel, switch to Center/Range mode (same panel, different
   pair of fields), or -- if enabled (Settings → General → "Enable band
   selector") -- pick a ham band from the selector above the Presets
   list, which fills in Start/Stop for you.
2. **Set the point count.** Type a number into Points directly, or open
   **Measurement speed...** for a Fast ⟷ Accurately slider (5–1000
   points, snapped to the nearest 10) that sets it for you. More points
   = finer resolution across your range, at the
   cost of a slower sweep.
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

**Top toolbar**

| Control | What it does |
|---|---|
| Settings | Opens the Settings dialog (connection, calibration, theme, language, cable settings, and more) |
| Export | Exports the *selected* measurement to CSV, NWL, or Touchstone (.s1p) -- select a row in Measurements first |
| Import | Loads an external file: Touchstone (.s1p), CSV, NWL, or AntScopeZ's own `.asd` |
| Print | Opens the [Print dialog](#print-and-screenshots) for the current chart |
| Screenshot | Saves the *current chart* (not the whole window) straight to a PNG file you pick -- same image Ctrl+C copies, just written to disk instead of the clipboard |
| Screenshot from AA | Captures the *analyzer's own* on-device screen (not every model supports this -- see [Supported Devices](../SUPPORTED_DEVICES.md)) -- see [Print and screenshots](#print-and-screenshots) |
| Data from AA | Loads measurement results already stored in the analyzer's own memory -- see [Data from AA](#data-from-aa) |

**Frequency panel**

| Control | What it does |
|---|---|
| Limits / Center / Range | Two ways to define the same swept range -- absolute Start/Stop, or a Center frequency ± a Range |
| Start, Stop (or Center, Range) | The actual sweep bounds, in kHz |
| Points | Number of measurement points across the range |
| Calibration (checkbox) | Applies OSL calibration correction to scans -- has no effect until you've actually performed a calibration in Settings (see [Calibration](#calibration-osl)) |
| Full range | Resets Start/Stop to the connected analyzer's own default range |
| Measurement speed... | Opens the Fast ⟷ Accurately points slider (5–1000, snapped to the nearest 10) |

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

**Chart tabs**: SWR, Phase, Z=R+jX, Z=R‖+jX, RL, Smith, TDR, Multi (plus
a "User defined" tab if launched with `-developer`).

### Keyboard shortcuts

| Key | What it does |
|---|---|
| F1 – F7 | Jump to the SWR / Phase / Z=R+jX / Z=R‖+jX / RL / Smith / TDR tab (Multi has no shortcut of its own) |
| F9 / F10 | Single / Continuous scan -- same as the toolbar buttons |
| Esc | Stop/interrupt the current scan |
| Delete | Delete the selected measurement |
| `+`, `=`, ↑ | Zoom the current chart's frequency (X) range in |
| `-`, ↓ | Zoom the current chart's frequency (X) range out |
| ←, → | Pan the current chart's frequency range left/right |
| Ctrl + `+` / Ctrl + ↑ | Zoom the Y-axis scale in (same as Ctrl+scroll) |
| Ctrl + `-` / Ctrl + ↓ | Zoom the Y-axis scale out |
| Ctrl + 0 | Reset the Y-axis scale to default |
| Ctrl + C | Copy the current chart to the clipboard as an image |

*(Developer mode only: Ctrl+Alt+Shift+M and Ctrl+Alt+Shift+N trigger
internal auto-calibration debug routines -- not meant for normal use.)*

## Settings

The Settings dialog normally has three tabs: **General**, **OSL
Calibration**, and **Cable**. (Two more, Customize and Updates, only
appear under `-developer` -- see
[Customized analyzer parameters](#customized-analyzer-parameters). The
Updates tab is additionally removed unconditionally regardless of that
flag, due to a known bug (#2247), so "Check for firmware updates" isn't
currently reachable at all.)

OSL Calibration has its own section -- see
[Calibration (OSL)](#calibration-osl).

### General tab

| Control | What it does |
|---|---|
| Connect analyzer | Opens the device-selection dialog -- see [Connecting to your analyzer](#connecting-to-your-analyzer) |
| Register application / Match license / Register device / Update license / Device info | RigExpert's own registration and licensing system. This talks to RigExpert's servers and isn't something this fork tests or supports -- see the disclaimer in [README.md](https://github.com/K4HEZ/AntScopeZ#readme). Use the vendor's own software for anything licensing-related. |
| Measurement system | Metric or Imperial units |
| Max measurements | Cap on how many measurements can be displayed at once |
| Chart background | Opens a color picker for the plot background |
| Theme | Light or Dark -- see [CHANGELOG.md](../CHANGELOG.md) for what it does and doesn't cover |
| Show graph hint / Show markers hint / Show brief params under cursor | Toggle the various hover/cursor readout popups on the charts |
| Don't restrict frequency *(developer mode only)* | Disables Start/Stop range clamping entirely -- hidden unless launched with `-developer` |
| System impedance | The reference impedance (default 50Ω) everything -- SWR, Smith chart center, RL -- is calculated against |
| Bands highlighting | A dropdown picking which region's band data to shade on the charts, plus a **...** button opening the band editor (add/edit/remove bands for that region) |
| Show band name | Labels the shaded bands on the charts with their names, not just color |
| Enable band selector | Shows/hides the band-selector dropdown above the Presets list -- see [Presets and bands](#presets-and-bands) |
| Open 'Connect Analyzer' on launch | See [Connecting to your analyzer](#connecting-to-your-analyzer) |
| Language | UI language -- auto-discovered from whatever `QtLanguage_*.qm` files are installed, not a fixed list |

### Cable tab

Lets you tell AntScopeZ about your feedline, so it can account for
cable loss/length in what it shows you -- useful when your analyzer is
some distance from the antenna through lossy coax.

| Control | What it does |
|---|---|
| Cable dropdown | Pick a built-in ideal cable (50/75/25/37.5Ω), a saved custom one, or "Change parameters or choose from list..." to enter your own |
| Cable R0, Cable length, Velocity factor | Your feedline's characteristic impedance, physical length, and velocity factor |
| Conductive loss, Dielectric loss | Loss figures for the cable, in dB/100ft, dB/ft, dB/100m, or dB/m (pick the unit from the dropdown next to them), specified either "at" a given frequency or as "any frequency" |
| Do nothing / Subtract cable / Add cable | Selects whether cable loss is factored out of, into, or ignored in your readings |
| Export | Exports the current cable settings |
| Update graphs | Re-applies the current cable settings to already-plotted data |

**Not independently verified:** while auditing this tab, "Do nothing /
Subtract cable / Add cable" only appear to toggle each other and
enable/disable the fields above them -- no code path was found that
reads which one is selected to actually transform displayed or exported
data. It may do nothing currently, or it may be wired through a path
this pass didn't find. Worth confirming against real hardware before
relying on it; flagging here rather than asserting a behavior that
wasn't actually confirmed.

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

The **band selector** (Settings → General → "Enable band selector") is
a faster shortcut for the common case: instead of building your own
preset, pick a named ham band from the dropdown above Presets and
Start/Stop are set for you immediately, formatted as
`<name> (<start> - <stop> kHz)`. Which bands show up depends on the
region picked in Settings → General → "Bands highlighting" (backed by
`itu-regions.txt`/`itu-regions-defaults.txt`) -- its **...** button
opens the band editor if you need to add or adjust one for that region.

### Editing band definitions

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
multiple views simultaneously.

Hovering shows a readout of that marker's values (frequency, SWR, RL,
R/X/Z, and more, depending on the chart). Settings → General → "Show
markers hint" controls whether that readout pops up automatically.

## Multi view

The **Multi** tab lets you stack two or more charts for the *same* measurement, 
or compare markers across them, in one 
view -- useful for eyeballing return loss and SWR together instead of
flipping between tabs. Right-click a chart's tab and choose "Move chart
to the tab Multi" (or "Add multi-charts") to populate it.

You can add and remove tabs to Multi View using the "+" button to join charts.

## Data from AA

Loads measurement results that already exist in the *analyzer's own*
on-device memory (not files on your PC -- see
[Import / Export](#import--export) for that). The toolbar's **Data from
AA** button opens a list of everything currently stored on the device;
double-click an entry (or select it and click OK) to load just that one
into AntScopeZ as a new measurement.

**Read and Save all** instead walks the *entire* list automatically:
pick a destination folder, and it loads and saves every stored entry in
turn as its own `.asd` file (zero-padded index + the device's own name
for each), with a progress dialog you can Abort partway through.

## Import / Export

These are two different toolbar buttons, doing related but distinct
things:

- **Export** (toolbar) opens a dialog for the measurement currently
  *selected* in the Measurements list, offering:
  - **CSV** -- comma-separated values
  - **NWL** -- APAK-EL format
  - **Z, RI** / **S, RI** / **S, MA** -- Touchstone (`.s1p`), as
    impedance or S-parameters, in rectangular (real/imaginary) or polar
    (magnitude/angle) form
- **Import** (toolbar) is the general "bring external data in" action --
  accepts Touchstone (`.s1p`), CSV, NWL, or AntScopeZ's own `.asd`.

Separately, the **Measurements panel's own Open/Save** buttons are
narrower: they only read/write AntScopeZ's native `.asd` format, for one
measurement at a time.

## Print and screenshots

Three related but different ways to get a chart out of AntScopeZ as an
image or document:

- **Screenshot** (toolbar) saves the *current chart tab* straight to a
  PNG file you pick -- the same image Ctrl+C copies to the clipboard,
  just written to disk instead. Not available for Multi.
- **Print** (toolbar) opens a dedicated dialog: a preview of the current
  chart, the markers table beneath it, an auto-generated header (e.g.
  "SWR graph") that isn't user-editable in this dialog, a free-text
  Comment box, and a Line width slider affecting the printed/exported
  trace thickness. From there:
  - **Print** sends it to your system's print dialog.
  - **Export PDF** / **Export PNG** save it directly to a file instead,
    with the same header/chart/markers/comment layout.

  The Print button/dialog isn't available while the Multi tab is
  active -- clicking it does nothing in that case.
- **Screenshot from AA** captures the *analyzer's own* on-device screen
  (not every model supports this -- see
  [Supported Devices](../SUPPORTED_DEVICES.md)) and opens its own small
  dialog: add an optional comment, then **Export to PDF**,
  **Export to BMP**, or **To clipboard**. **Refresh** re-captures the
  device's screen again without closing the dialog, in case it's
  changed since it was first captured.

## TDR (Time Domain Reflectometry)

### What a TDR scan actually measures

A TDR run in AntScopeZ is not a separate kind of measurement -- it's a normal
frequency sweep, just an unusually wide one. When you start a TDR scan, the
software picks a start frequency near the analyzer's lowest supported
frequency (close to DC) and a stop frequency computed from your cable-length
and velocity-factor settings (capped at the analyzer's maximum), then runs
that sweep through the exact same measurement pipeline as a regular
Single/Continuous scan. The result is a genuine, real complex-impedance sweep
across that whole span -- commonly on the order of 100 kHz up to several
hundred MHz, depending on your analyzer and cable settings.

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

## Customized analyzer parameters

AntScopeZ has a hidden "Customize" tab in Settings that lets you define your
own named analyzer presets -- a custom minimum/maximum frequency range plus an
LCD width/height used for screenshot layout. It's aimed at one specific
situation: you have a unit that AntScopeZ already recognizes correctly (a
clone, or a newer hardware revision of a model AntScopeZ knows about), but its
real frequency range differs from what AntScopeZ assumes for that model.

### Enabling the tab

The Customize tab (and the "Updates" tab next to it) are removed from the
Settings dialog by default. Customize only appears when AntScopeZ is launched
with the `-developer` command-line flag, which also unlocks a few other
developer-only controls (see below). There's no in-app toggle for this --
it's a launch argument. The Updates tab stays hidden unconditionally, in the
current build; it's disabled outright due to a known bug (#2247) rather than
gated by developer mode.

### What "prototype" actually means here

When you create a custom analyzer, you first pick a "prototype" from a list
of AntScopeZ's built-in models (AA-30, AA-55, AA-230, AA-1500, etc.). Picking
a prototype does **not** select a different communication protocol -- it just
seeds the new entry's default frequency range and LCD dimensions from that
model, as a starting point you then edit.

The actual protocol AntScopeZ uses to talk to your analyzer is chosen purely
by what the connected device reports about itself during the initial
handshake (its version string over serial, or its HID identification). If
your device's firmware reports itself as, say, an AA-55, AntScopeZ talks to it
exactly like a genuine AA-55 -- same commands, same parsing -- regardless of
any custom analyzer you've defined. Customizing doesn't change *how* commands
are sent; it only changes *what frequency range* those commands are allowed to
sweep.

### Using it for a clone or an updated-range unit

This is the practical use case:

1. Confirm AntScopeZ already detects your device correctly (it identifies
   itself as an existing model during connection).
2. Open Settings → Customize (requires `-developer`), pick that model as the
   prototype, give the preset an alias, and set the min/max frequency to what
   your actual hardware supports.
3. Check "Use customized analyzer."

From that point on, the custom min/max frequency you entered is used as the
sweep bounds for Single/Continuous scans on every chart tab, instead of the
stock model's built-in range -- so you can sweep wider (or narrower) than the
official spec for that model without any code changes.

### It only takes effect when launched with `-developer`, every time

Your custom presets and the "Use customized analyzer" checkbox are saved to
`AntScopeZ.ini`, but they are only *loaded back* into the running app when
AntScopeZ is started with `-developer`. Launch it normally and the saved data
just sits there, unread -- the app behaves exactly like stock, with no custom
range applied and no way to reach the Customize tab to turn it on. Nothing is
silently overridden in a normal launch.

Conversely, the reverse is also true: relaunching with `-developer` brings the
override straight back automatically, using whatever was last saved, even if
you don't open Settings or touch the checkbox that session. So the flag alone
fully controls whether the customization is live -- it isn't something you
have to re-enable by hand each time, and it isn't something that leaks into a
non-developer launch either.

### Limits and things to watch for

- **No capability negotiation happens.** AntScopeZ does not ask the device
  what frequencies it can actually produce -- it just sends whatever range you
  configured, using the same command format as the real model. If you set a
  range your hardware can't actually sweep, you won't get an error; you'll get
  meaningless or garbage results for the out-of-range portion.
- **Detection still depends on the device's own reported name.** If your
  clone's firmware reports a version string AntScopeZ doesn't recognize at
  all, it won't be identified as any model, and a custom analyzer preset can't
  help -- there's nothing to attach it to.
- **This is a developer-mode feature.** It's not intended as a
  polished end-user workflow, and settings entered here are not validated
  against real hardware limits the way the built-in model list is.
- A related developer-only control, "Restrict frequency" (also gated by
  `-developer`), can be unchecked to disable range clamping entirely instead
  of defining a custom preset -- a blunter alternative if you just want to
  type any frequency without setting up an alias.

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
| `QtLanguage_<code>.qm` / `qtbase_<code>.qm` (optional) | Drop a `.qm` here to add a language AntScopeZ doesn't ship, or override a shipped one -- picked up automatically, no reinstall needed. See [Language](#general-tab) in the Settings reference. |

**Windows:** the equivalent per-user folder is wherever Qt's
`GenericConfigLocation` resolves to (typically
`%APPDATA%\AntScopeZ\`). **macOS:** your home folder directly
(`QStandardPaths::HomeLocation`), not a dotfile -- look for an
`AntScopeZ` folder there.

### `AntScopeZ.ini` reference

This is a real, working config from actual use -- not a synthetic
example -- lightly trimmed of pure window-geometry noise. Groups you'd
actually want to hand-edit or just recognize:

```ini
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
chart-background=#505050
current_band=ITU Region 2 - Americas
darkColorTheme=true
maxMeasurements=5
open-connect-analyzer-at-launch=false
restrictFq=true
show-band-name=false

[Connection]
id=180000756
name=Match
same=false
type=0

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
- **`Connection`** -- the last-connected device, used for silent
  auto-reconnect at launch (see
  [Connecting to your analyzer](#connecting-to-your-analyzer)). `same`
  tracks the "Use same selection for future connections" checkbox.
- **`Calibration`**'s `Performed`/`Enabled` here are just what gets
  written back out on exit -- the app's actual live check is whether the
  three `*Path` files exist on disk, not this flag (see
  [Calibration (OSL)](#calibration-osl)).
- Everything else not listed above (`General`, `Hint`, `BriefHint`,
  `Markers`, per-tab `*ZoomState`, `mainX`/`mainY`/`geometry`, ...) is
  internal window-position/zoom-state bookkeeping. Harmless to delete
  individually if something looks stuck -- it just regenerates with
  defaults.

If your `.ini` has a leftover group named in another language (e.g.
`[Marcadores]` sitting next to `[Markers]`) from before this was fixed
(see CHANGELOG.md), it's an orphaned duplicate of the `Hint`/`Markers`/
`BriefHint` popup-position bookkeeping above -- harmless, safe to
delete.

---

*This guide was drafted with [Claude](https://www.anthropic.com/claude)
(Anthropic's AI), grounded in AntScopeZ's actual source code -- not yet
verified line-by-line against real hardware. Found something wrong?
[Open an issue](https://github.com/K4HEZ/AntScopeZ/issues).*
