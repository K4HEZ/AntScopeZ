# FTDI License

`LICENSE.txt` in this directory is copied verbatim from the header comment
at the top of `ftdi/ftd2xx.h`. It is Future Technology Devices
International Limited's (FTDI) own proprietary redistribution license, not
an OSI-approved open-source license, and **not** part of AntScopeZ's GPLv3
grant (see `/COPYING` and `THIRD-PARTY-LICENSES.md` at the repo root).

It covers the Windows-only FTDI D2XX driver package bundled under `ftdi/`:
`ftd2xx.h`, `ftd2xx.dll`, and the rest of the FTDI CDM driver files
(`ftser2k.sys`, `ftdibus.sys`, `ftbusui.dll`, `ftserui2.dll`,
`ftcserco.dll`, `ftlang.dll`). None of this applies to Linux or macOS
builds, which don't bundle any FTDI code -- see
`THIRD-PARTY-LICENSES.md`.

Per FTDI's own terms ("FTDI DRIVERS MAY BE DISTRIBUTED IN ANY FORM AS LONG
AS LICENSE INFORMATION IS NOT MODIFIED"), this file must stay byte-identical
to the notice in `ftd2xx.h` -- if that header ever changes (e.g. a newer
FTDI SDK drop), update this copy to match.
