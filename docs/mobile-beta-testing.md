---
layout: default
title: Testing a Pre-release Mobile Build
---

# Testing a pre-release mobile build

Thanks for testing. This page is for people trying a pre-release
(beta) build of the AntScopeZ mobile app. For how the app works once it's
running, see the [mobile guide](mobile-guide.md).

Pre-release builds are development snapshots. They are published on the
[Releases page](https://github.com/K4HEZ/AntScopeZ/releases) marked
**Pre-release**; the first one is
[`android-beta-1`](https://github.com/K4HEZ/AntScopeZ/releases/tag/android-beta-1).

## Installing

1. On the phone, open the release page and download the `.apk` file (arm64
   phones -- nearly every Android phone from the last several years).
2. Open the downloaded file. Android will ask you to allow installing apps
   from your browser (or file manager); allow it for this install.
3. Open **AntScopeZ**.

These builds are signed with a debug key. To move to a newer pre-release
you may need to uninstall the old one first (settings are lost).

## What to try

Pick whatever you have; you don't need all of it.

- **Bluetooth analyzer** -- Connection > Bluetooth > Search > pick > Connect.
  Run a scan on the SWR page. Check the Connected Device fields.
- **USB analyzer** (RigExpert Match, with an OTG adapter) -- Connection >
  USB > Connect, and allow the USB permission dialog.
- **Serial analyzer** (NanoVNA classic or V2/LiteVNA, RigExpert COM units,
  with an OTG adapter) -- Connection > Serial > Search > pick > Connect.
- **Live Data**, **Smith** and **TDR** against a real antenna or cable.
- **Share Touchstone file** from the Impedance page.

## What's known to be checked, and what isn't

| Area | State |
|---|---|
| Bluetooth connect and scan (RigExpert Match) | Checked on a desktop Linux build; not yet on an Android phone |
| USB-HID connect and scan (RigExpert Match) | Checked on a desktop Linux build; the Android USB path is new and unproven |
| Serial (NanoVNA, RigExpert COM) on Android | New, built but never run on a phone or a device |
| NanoVNA scans and Live Data | Not tested on real hardware from the mobile app |
| TDR | The math is the desktop's and was checked with simulated cable data; not yet run on real hardware from the mobile app |
| Share Touchstone file (Android Share sheet) | Built, not yet run on a phone |
| Bluetooth connection staying up | Reports of the Match disconnecting after a while are still being looked into |
| Live Data over USB | A fix for it stopping after a few readings is in, unconfirmed |

## What to tell us

The most useful reports include:

- the phone model and Android version
- the analyzer model and how it was connected
- the exact text on the status line at the bottom when something fails
- what you were doing just before (idle, scanning, Live Data...)

Report problems at
[github.com/K4HEZ/AntScopeZ/issues](https://github.com/K4HEZ/AntScopeZ/issues).

If you are comfortable with a computer, `adb logcat` while reproducing the
problem is very helpful -- the app logs under the tag `AntScopeZ`.
