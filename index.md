---
layout: default
title: AntScopeZ
---

# AntScopeZ

A modern antenna-analyzer application for RigExpert & NanoVNA (and a few
other brands) -- built for, and maintained by, the ham radio community.

## Features

- Support of RigExport Analyzers and NanoVNA (ASCII & Binary)
- Autodetect most Analyzers
- Import/Export of Touchstone Files (.asd/.s1p/.s2p/.csv/.nwl)
- Full 2-Port S-Parameter Export (S11/S21/S12/S22)
- Customizable Band Selection and User Presets
- Named Measurements, (load/save/conversions)
- Multiview of plots, synced, ...with markers.
- Auto Stitch Feature (~10k Points)
- Dynamic Single Frequency Mode (live SWR/Z monitor)
- Big-Number SWR Readout for One Fq Mode
- Built in Color/Theme Editor
- Marker Comparison and Trim Calculator
- OSL Calibrations and Cable Add/Delete
- Debug Logging for Analyzer Communications
- USB Serial/HID, BLE/Bluetooth and NavoVNA
- TDR Analysis 
  - Cable Length <-> Velocity Factor Calculations w/ 150+ Cable Presets 
  - Adjustable frequency/points with umabiguous range / resolution indication
  - Selectable windows (Rectangular, Hamming, Hamm, Blackman, Kaiser)
- Language Support: English, Ukrainian, Japanese, Spanish (AI generated)
- ...and much more.

## Download

**[⬇ Download the latest release](https://github.com/K4HEZ/AntScopeZ/releases/latest)**
&nbsp;·&nbsp;
[Browse the source](https://github.com/K4HEZ/AntScopeZ)
&nbsp;·&nbsp;
[Report an issue](https://github.com/K4HEZ/AntScopeZ/issues)

## Screenshots

![SWR sweep across the 20m band with the menu bar, docked Cursor Details panel, and band highlighting, Light theme](assets/screenshots/SWR-Chart.png)
*An SWR sweep across the 20m band -- menu bar, 3-pane layout, docked
Cursor Details panel, and band highlighting -- Light theme.*

## Why AntScopeZ?

AntScopeZ is a fork of RigExpert's own AntScope2 software, renamed and
rebuilt to give it its own identity, separate from the vendor's. Along the
way it's had a steady stream of fixes and new features. -- see [CHANGELOG.md](CHANGELOG.md) for the full
detail.

**This is not an official RigExpert product.** It's an independent,
unaffiliated, community effort -- see the disclaimer in
[README.md](https://github.com/K4HEZ/AntScopeZ#readme) before relying on
it for anything that matters (licensing, firmware updates, warranty
support). For those, use RigExpert's own software -- the original
AntScope2 is available directly from RigExpert at
[rigexpert.com/software/antscope2](https://rigexpert.com/software/antscope2/).

## Supported devices

AntScopeZ isn't limited to RigExpert's own analyzers -- NanoVNA and a
few other brands are supported too. See the [User Guide's Supported
Devices section](docs/user-guide.md#supported-devices) for the full
list. (Disclaimer: most listed devices haven't been individually tested
against this fork.)

## Documentation

- **[User Guide](docs/user-guide.md)** -- how to use AntScopeZ (the
  full supported-device list, TDR scans, customized analyzer
  parameters, and more as it grows)
- **[Build Instructions](BUILDINFO.md)** -- building from source, Qt
  version notes, platform-specific detail
- **[Changelog](CHANGELOG.md)** -- what's changed, release by release
- **[Third-Party Licenses](THIRD-PARTY-LICENSES.md)** -- full
  component-by-component license breakdown

## License

AntScopeZ is licensed under the **GNU General Public License v3.0 or
later** ([COPYING](COPYING)) as a whole, since it bundles GPLv3-licensed
components (QCustomPlot, HIDAPI). AntScopeZ's own code -- both the original
RigExpert AntScope2 codebase it's forked from and everything added or
modified since -- remains separately available under the MIT license
([LICENSE.txt](LICENSE.txt)). Qt and libusb are used under their LGPL
terms, and a Windows-only FTDI driver package is bundled under FTDI's own
proprietary redistribution terms -- see
[THIRD-PARTY-LICENSES.md](THIRD-PARTY-LICENSES.md) for the full
per-component, per-platform breakdown.

## Getting help

This is a volunteer-maintained fork, not a supported product -- there's
no SLA and no guarantee of a fix or a timely reply. That said,
[GitHub Issues](https://github.com/K4HEZ/AntScopeZ/issues) is the right
place to report a bug or ask a question about this fork specifically. For
anything involving RigExpert's own hardware, firmware, or licensing,
contact RigExpert directly -- this project has no connection to them and
no way to help with those.

---

*Built with help from [Claude](https://www.anthropic.com/claude)
(Anthropic's AI) -- docs, this site, and a fair bit of the code itself.*
