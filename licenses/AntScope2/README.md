# AntScope2 (RigExpert) License

`LICENSE.txt` in this directory is the MIT license Rig Expert Ukraine Ltd.
released the original AntScope2 codebase under -- the software AntScopeZ
is forked from. It's a plain copy of the MIT terms portion of the
repo-root `LICENSE.txt`, kept here for consistency with `licenses/Qt/` and
`licenses/FTDI/`.

This one is a different kind of entry than those two, though: Qt and FTDI
are third-party dependencies AntScopeZ *uses*; this is the license of the
codebase AntScopeZ itself *originated from* and still substantially
consists of. It applies to the RigExpert-authored source files considered
on their own.

The repo-root `LICENSE.txt` remains the canonical, GitHub-discoverable
copy -- it additionally explains how this MIT grant relates to the
project's overall GPLv3-or-later license (`/COPYING`), which this
directory's plain copy deliberately leaves out to stay a clean mirror of
the original terms. See `THIRD-PARTY-LICENSES.md` at the repo root for how
all of these pieces fit together.

## Provenance, and RigExpert's later switch to GPLv3

The MIT grant above applies specifically to the AntScope2 codebase as it
stood when AntScopeZ was forked from it, not to RigExpert's ongoing
development since. Confirmed directly from their own repository history
at `github.com/rigexpert/AntScope2`: commit `2053358` ("license added",
2020-07-02) added this exact MIT text, and it stood unchanged (only
reformatted in `04e856a`, 2020-08-31) through the version this project
forked from.

On 2026-10-15, RigExpert deleted that file (`a14302b`, "v2.0.4: QT
Licenses") and, five days later, added a new, unmodified GPLv3
`LICENSE.txt` in its place (`b58870d`, 2026-10-20) -- their repository is
licensed GPL-3.0 as of this writing, with no MIT file anywhere in its
current tree.

AntScopeZ has continued pulling individual fixes from their ongoing
development since the fork (see `CHANGELOG.md`'s "Ported from RigExpert
AntScope2 X.X.X" entries). That code comes from their current,
GPL-3.0-licensed repository, not the 2020 MIT snapshot above -- it was
never MIT, and crediting it as such would be wrong, not just outdated.

This doesn't change how AntScopeZ itself is licensed: the distributed
application is already GPLv3-as-a-whole regardless, because of its own
GPL dependencies (QCustomPlot, HIDAPI) -- see the repo-root `LICENSE.txt`
and `/COPYING`. Incorporating GPLv3-sourced fixes alongside the
MIT-licensed fork base doesn't create any new obligation beyond what
already applies.

Credit to Rig Expert Ukraine Ltd. for both: the original MIT-licensed
AntScope2 codebase this project forked from, and the GPLv3 license
governing their ongoing development since (full GPLv3 text already
carried in this repo at `/COPYING`; current upstream copy at
`github.com/rigexpert/AntScope2/blob/master/LICENSE.txt`).
