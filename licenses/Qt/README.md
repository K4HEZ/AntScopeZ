# Qt License Files

Qt's LGPLv3 option is defined as the GNU Lesser General Public License v3
text (`LGPL-3.0.txt`), which itself incorporates the GNU General Public
License v3 (`GPL-3.0.txt`) by reference plus a short list of additional
permissions. Both files are required together to state the full terms; this
directory is self-contained on purpose so it can be copied as a unit into
packaged output (see `THIRD-PARTY-LICENSES.md` at the repo root for the
per-platform bundling details and rationale).

Both texts fetched verbatim from:
- https://www.gnu.org/licenses/lgpl-3.0.txt
- https://www.gnu.org/licenses/gpl-3.0.txt

## Which Qt license option AntScopeZ elects

Every Qt module AntScopeZ links against (Core, Gui, Widgets, PrintSupport,
SerialPort, Network, Xml, Concurrent, OpenGL, Bluetooth) is dual-licensed
by The Qt Company as **LGPLv3 or GPLv2** (confirmed against each module's
own doc page for Qt 6.11, e.g. `doc.qt.io/qt-6/qtserialport-index.html`,
`.../qtbluetooth-index.html`, `.../qtcore-index.html`, etc. -- commercial
licensing is a third option not relevant here).

AntScopeZ elects the **LGPLv3** option, not GPLv2, for a specific reason:
GPLv2-only code is not license-compatible with a GPLv3(-or-later) combined
work (no "or later" clause carries it forward), whereas LGPLv3 explicitly
permits relicensing the covered library into a GPLv3-or-later combined
work (LGPLv3 section 4, "Combined Works"). Since AntScopeZ as a whole is
GPLv3-or-later (see `/COPYING`), LGPLv3 is the only one of Qt's two
copyleft options that's actually compatible -- electing GPLv2 here would
create the exact same kind of conflict this project went through the
QCustomPlot/GPLv3 analysis to resolve.
