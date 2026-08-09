---
layout: default
title: User Guide
---

# AntScopeZ User Guide (starter)

This is a starting point for real end-user documentation -- AntScopeZ currently
has none (the root `README.md` is build/developer instructions only). Sections
below get filled in over time; most of this file is still a stub.

See [SUPPORTED_DEVICES.md](../SUPPORTED_DEVICES.md) for the full list of
supported analyzer models and brands.

## Table of contents

- [TDR (Time Domain Reflectometry)](#tdr-time-domain-reflectometry)
- [Customized analyzer parameters](#customized-analyzer-parameters)
- Scan modes (Single vs. Continuous) -- TODO
- Calibration -- TODO
- Markers -- TODO
- Multi view -- TODO
- Import / Export -- TODO
- Presets and bands -- TODO

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
