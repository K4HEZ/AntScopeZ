---
layout: default
title: Supported Devices
---

# Supported devices *

AntScopeZ's device support isn't limited to RigExpert's own antenna
analyzers. This list is generated from the app's actual model table --
`AnalyzerParameters::fill()` in `analyzer/analyzerparameters.h` -- which is
the single source of truth for what's recognized by name/serial-number
prefix. If this file and that function ever disagree, the code wins;
update this file to match.

* Caveat: I do not own all these devices and cannot confirm if they actually work.
It is for this reason I've also disabled the firmware update mechanism so
you don't 'brick' your device because of something I cannot test/reproduce or fix
myself.   **Use this software at your own risk.**

## RigExpert AA-series

AA-30, AA-30 ZERO, AA-30.ZERO, AA-35 ZOOM, AA-54, AA-55 ZOOM, AA-170,
AA-200, AA-230, AA-230 ZOOM, AA-230PRO, AA-500, AA-520, AA-600,
AA-650 ZOOM, AA-700 ZOOM, AA-1000, AA-1400, AA-1500 ZOOM, AA-1500 SE,
AA-1500 ZOOM SE, AA-2000 ZOOM, AA-3000 ZOOM

## RigExpert "Stick" series (handheld)

Stick 230, Stick 500, Stick Pro, Stick XPro

## RigExpert "Match" series (antenna matcher/tuner)

Match, MATCH U

## RigExpert (unconfirmed naming)

These use the same serial-number-prefix detection scheme as the rest of
the RigExpert lineup above, but the product names below haven't been
independently confirmed -- worth double-checking against RigExpert's
current catalog before publishing this list anywhere external.

- Zero II
- Touch
- Touch E-Ink

## Other brands

- **NanoVNA** -- the open-source/DIY VNA project. Handled by its own
  dedicated `analyzer/nanovna_analyzer.cpp` class, entirely separate
  connection/protocol handling from the RigExpert-oriented analyzer
  classes. Detected via USB VID:PID `0483:5740` -- STMicroelectronics'
  generic "Virtual COM Port" demo ID, not a NanoVNA-specific registered
  one, which nearly the whole NanoVNA ecosystem ships unmodified (few
  variants bothered registering their own USB VID), so this is a broad
  but imprecise family match rather than a specific-model check. Speaks
  the original ASCII shell protocol (`info`/`sweep`/`frequencies`/`data`
  over what's really a USB CDC-ACM serial port, not a distinct native-USB
  path) -- the lowest common denominator most of the family (NanoVNA,
  NanoVNA-H, NanoVNA-H4, DiSlord's fork, most clones) has stayed
  backward-compatible with, even where a firmware also offers a newer,
  faster binary protocol as an option (not used here). Only requests S11
  (1-port reflection) data; S21 (2-port through) isn't requested even
  though the real firmware supports it. The `info` response's `Board:`
  line reports the actual connected firmware/hardware string if you need
  to confirm exactly what's plugged in.
- **WilsonPro CAA** -- Wilson Electronics' own brand (cellular
  signal-booster company), not RigExpert. Detected via its own
  serial-number prefix alongside the RigExpert ones, which suggests a
  RigExpert-manufactured unit sold under WilsonPro's branding
  (OEM/white-label) rather than an independent protocol implementation --
  inferred from the code pattern, not confirmed.

## Anything else

Settings -> Developer -> Custom Analyzer lets you manually define a
"prototype" -- frequency range, screen size, protocol -- for a device
not in the table above, without a code change. Currently disabled (not
safe to use yet -- see `BUILDINFO.md`'s Known Issues). See
`docs/user-guide.md`'s "Customized analyzer parameters" section.
