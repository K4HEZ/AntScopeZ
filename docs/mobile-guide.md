---
layout: default
title: Mobile App Guide
---

# AntScopeZ Mobile Guide

The AntScopeZ mobile app is a pocket-sized companion to the desktop
program: connect an analyzer, run a scan, read SWR and impedance, look at
the Smith chart, watch a live single-frequency reading, find cable faults
with TDR, and share the result as a Touchstone file. It targets Android
phones; the same code also runs on a Linux desktop for testing.

It is deliberately smaller than the desktop app -- there are no markers,
calibration, saved measurement lists, multi-view or printing. For those,
use the [desktop guide](user-guide.md). Everything the mobile app can do,
it does with the same analyzer drivers and the same math as the desktop.

If you are trying a pre-release build, see
[Testing a pre-release build](mobile-beta-testing.md) first.

## Contents

- [Moving around](#moving-around)
- [Connection](#connection)
- [Scan](#scan)
- [SWR](#swr)
- [Smith](#smith)
- [TDR](#tdr)
- [Live Data](#live-data)
- [Impedance](#impedance)
- [Settings](#settings)
- [About](#about)
- [Frequency limits, explained](#frequency-limits-explained)
- [Differences from the desktop app](#differences-from-the-desktop-app)
- [Troubleshooting](#troubleshooting)

## Moving around

Tap the **☰** button at the top left to open the menu. The pages, in
order, are Connection, Scan, SWR, Smith, TDR, Live Data, Impedance,
Settings and About. The title bar shows the current page and the name of
the connected analyzer (or "Not connected"). The strip along the bottom is
the status line: it shows what the app is doing ("Scanning...", "Scan
complete: 50 points") and any error from the analyzer. Long messages wrap
instead of being cut off.

A typical session: **Connection** (connect) → **Scan** (pick a band) →
**SWR** or **Smith** (press Scan, read the result) → **Impedance** (share
the file).

## Connection

The top of the page, **Connected Device**, shows what the app knows about
the analyzer once it is connected:

| Field | Meaning |
|---|---|
| Name | The name the analyzer announced (for Bluetooth, the name you picked) |
| Model | The analyzer model AntScopeZ identified it as |
| Serial number | Serial number, when the analyzer reports one |
| Firmware | Firmware version, when reported |
| License | For RigExpert Match units, the license level the device reports (BASE, ADVANCED, RFE, PRO) |
| Interface | How you are connected: USB (HID), Bluetooth LE or Serial (COM) |
| Protocol | ASCII or Binary -- the command style the app is using with the device |
| Port | The serial port or Bluetooth address, where there is one |
| Range | The analyzer's frequency range, in MHz |

Some fields stay `--` when a device doesn't report them -- for example,
a Match connected over Bluetooth doesn't send its license level. A Match's
range shows `--` until the device has reported it, rather than showing a
guess.

Below that, **Connect via** has three choices. Pick one, pick the device,
and press **Connect**. **Disconnect** is always on this row.

### Bluetooth

Choose **Bluetooth** and press **Search**. Nearby analyzers appear in the
list. Tap one to highlight it, then **Connect**. The first time, Android
asks for Bluetooth permission. After connecting (or disconnecting) the
list is cleared; press Search again to get a fresh list.

### USB

Choose **USB** and connect the analyzer with a USB cable (a phone usually
needs a USB-OTG adapter). Press **Connect**. The first time, Android shows
a "Allow AntScopeZ to access the USB device?" dialog; the app retries
until you answer it. **Cancel** stops the attempt. USB currently means
the RigExpert Match's HID connection.

### Serial

Choose **Serial** and press **Search**. NanoVNA (classic and V2/LiteVNA)
and RigExpert serial analyzers that are plugged in appear in the list. Tap
one, then **Connect**. Android shows its USB permission dialog the first
time, as above.

## Scan

Everything that defines a scan lives here, with a **Scan** / **Stop**
button at the top.

**Band.** Pick an **ITU region** (the list is full width), then pick a
band from the list below it. The band list starts on "Select a band" and
each entry shows the band's name and its range, for example
`2m - (144000 - 148000 kHz)`. Picking a band fills in Start/Stop (or
Center/Span) and the list returns to "Select a band". Only bands that
overlap the analyzer's usable range are listed (see
[Frequency limits](#frequency-limits-explained)).

The scan range is the band **plus a margin on each side**, so the SWR dip
isn't pressed against the edge of the chart. The margin is set in
[Settings](#settings) ("Add margin to scans").

**Frequency Range.** Choose **Start / Stop** or **Center / Span** and type
values in kHz. Whatever you enter is kept inside the allowed range; the
fields show the value actually used.

**Sweep.** **Points** is how many frequency steps the scan takes.
**Z0** is the reference impedance in ohms (50 by default); SWR and the
Smith chart are computed against it.

All of these are remembered between runs.

## SWR

Press **Scan** at the top. The chart plots SWR (fixed vertical scale 1 to
10) against frequency, with the lowest-SWR point marked and a shaded
highlight for any amateur bands inside the plotted range.

**Reading a point.** Touch or drag on the plot: a cursor line and dot
follow your finger, and the table underneath shows that point's
frequency, SWR, return loss, series R + jX, |Z|, parallel R + jX, |Zp| and
phase. After a scan the cursor starts on the lowest-SWR point; if you scan
again, it stays at the same frequency.

**Dense scans.** If the scan has more points than fit comfortably across
the screen, the chart shows a section and a scrollbar appears under it.
Drag the scrollbar, or drag along the frequency labels under the plot, to
move along the sweep. Dragging the cursor towards either edge of the plot
scrolls the chart smoothly under it. How much room each point gets is the
"SWR chart density" setting.

## Smith

Press **Scan**, then read the trace on the Smith chart: constant-resistance
and constant-reactance circles, plus rings for SWR 1.5, 2, 3 and 5. The
red dot is the selected point. Move it with the **slider** under the
chart, or tap the chart. The same parameter table as on the SWR page shows
its values. The selected point is shared with the SWR page.

## TDR

TDR (time-domain reflectometry) turns a very wide sweep into a picture of
what is along a cable: it shows reflections -- connectors, damage, an open
or shorted end -- at a distance. It uses the same math as the desktop TDR
tool. See the [desktop guide's TDR section](user-guide.md#tdr-time-domain-reflectometry)
for the background; the controls are the same, in a phone layout.

**Scan setup**

- **Cable preset** fills in the velocity factor from the bundled cable list
  (over 100 cables). You can also type a velocity factor.
- **Top frequency** is how far up the sweep goes. It always starts near
  DC. A wider sweep resolves closer reflections but covers a shorter
  distance before the trace wraps around.
- **Points** (200 to 1000).
- **Unambiguous range** and **Resolution (estimate)** update as you change
  those, so you can check they suit your cable before scanning.

Press **TDR Scan**. A progress bar shows while the sweep runs; **Stop**
cancels it. A cancelled scan is discarded.

**The trace.** Choose **Impulse**, **Step** or **Impedance**. Touch the
chart to read the distance and value at a point. The **Window** picker
(Rectangular, Hamming, Hann, Blackman, Kaiser) reshapes the trace
instantly without a rescan -- see the desktop guide for what each trades
off. Kaiser also has a **Beta** field.

**Result**

- **Distance to strongest reflection** -- in metres or feet (see
  [Settings](#settings)).
- **Reflection** -- Open or Short, with an approximate impedance in
  ohms, or "None detected" if nothing rises above the noise floor. Treat
  the ohms figure as a rough estimate.
- A note appears if the peak is near the edge of the scan's range, or if
  you entered a known cable length and the peak is noticeably short of it
  (a possible fault partway along the cable).
- **Known cable length** -- type a length you have measured and the page
  shows the velocity factor that would make the reflection land at that
  length. **Use** copies it into the velocity factor.

## Live Data

A continuously updating single-frequency reading. Enter a frequency in kHz
(it starts at the middle of the Scan range) and press **Start**. The page
shows the frequency, SWR, return loss, series and parallel R + jX, |Z|,
|Zp| and phase, and a very large **SWR** readout that scales to fill the
rest of the screen. The number's colour runs from dark green at 1:1,
through yellow at 3:1 and orange at 5:1, to red toward 10:1 and above --
readable at a glance while you adjust an antenna.

Live Data uses its own reading, so it does not overwrite the last scan
shown on SWR, Smith and Impedance. It stops when you leave the page,
because the analyzer can only do one thing at a time. If the analyzer stops
answering, the status line says so after a few tries.

## Impedance

The last scan as a table -- frequency in MHz, SWR, and series R + jX in
ohms -- with a **Share Touchstone file** button. The button writes the
scan as a one-port Touchstone file (`.s1p`, real/imaginary format) and
opens Android's Share sheet, so you can send it to email, a messaging app,
a cloud drive, or save it. (On the desktop test build it just saves the
file and reports where.)

## Settings

- **Add margin to scans** -- 0 to 100%. When you pick a band on the Scan
  page, the scan range is widened on each side by this percentage of the
  band's own width. 0% scans exactly the band.
- **SWR chart density** -- the minimum number of pixels between points on
  the SWR chart, 1 to 30. Scans with more points than fit at this density
  scroll instead of crowding together. Larger values scroll sooner.
- **Distance units** -- metric (metres) or feet, for TDR.
- **Frequency limits** -- see below.

## About

The app icon, version, build stamp, and links to the source code and the
project web page.

## Frequency limits, explained

Three things decide which frequencies the app will let you scan:

1. **The analyzer's range** -- reported by the connected device, shown on
   the Connection page.
2. **Use device range** (Settings, on by default) -- when on, scan
   frequencies and the band list are kept to the analyzer's range. Turn it
   off to remove that limit: the band list then shows every band, and the
   Start/Stop fields are no longer held to the device's range.
3. **Absolute min / absolute max** (Settings, defaults 100 kHz and
   10,000,000 kHz) -- hard limits that always apply, even with "Use device
   range" off.

A band appears in the list if any part of it falls inside the allowed
range, and the scan is kept inside the range even if the band extends past
it. Entering a minimum that isn't below the maximum is rejected and the
field reverts.

## Differences from the desktop app

Not in the mobile app: markers and marker comparison, OSL calibration and
cable correction, saved measurement lists and `.asd` files, multi-view,
printing and screenshots, firmware updates, user-defined band presets,
S21/two-port measurements, and the remote API. Files you share from the
mobile app are plain one-port Touchstone; open them in the desktop app for
analysis.

## Troubleshooting

- **No devices in the Bluetooth list.** Check the analyzer's Bluetooth is
  on, that Android has granted AntScopeZ Bluetooth permission, and press
  Search again. If a device you tapped does nothing and the status line
  says the list is out of date, search again.
- **USB or Serial never connects.** Make sure you tapped Allow on Android's
  USB permission dialog, and that the cable and OTG adapter carry data (a
  charge-only cable won't). Press Cancel and try again.
- **A scan stops early or the analyzer stops answering.** Disconnect and
  reconnect. If it happens repeatedly, note what the status line says and
  [open an issue](https://github.com/K4HEZ/AntScopeZ/issues).
- **Range shows `--`.** The device hasn't reported its range (yet). Scanning
  still works; frequencies are then held only to the absolute limits.
- **TDR says the sweep must start near DC.** The analyzer's minimum
  frequency is too high for TDR (it must be 100 kHz or lower).
