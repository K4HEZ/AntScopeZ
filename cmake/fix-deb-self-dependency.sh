#!/bin/sh
# Strip any self-referential entry (the package depending on itself) from a
# .deb's Depends: field, then repack.
#
# Why this exists: CPack's CPACK_DEBIAN_PACKAGE_SHLIBDEPS runs dpkg-shlibdeps
# against the bundled-Qt libraries under usr/lib/<triplet>/antscopez (see the
# "Packaging (.deb)" comment in CMakeLists.txt). On build machines that also
# have distro Qt6 packages installed (common for Qt version-comparison
# testing -- see BUILDINFO.md's "system-qt preset"), dpkg-shlibdeps has been
# observed to non-deterministically misattribute some of those bundled
# libraries back to the antscopez package itself, producing a literal
# "Depends: antscopez (>= <version>)" self-dependency in the control file.
# That's not just cosmetic: a package that depends on itself is uninstallable
# via apt/dpkg on any machine that doesn't already have it installed. Root
# cause traced into dpkg-shlibdeps's own path-resolution internals (not
# something fixable via CPack config); this fixup guarantees a clean control
# file regardless of whether/how that misattribution recurs.
#
# Usage: fix-deb-self-dependency.sh path/to/package.deb

set -e

if [ -z "$1" ]; then
    echo "usage: $0 path/to/package.deb" >&2
    exit 1
fi
DEB="$1"

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

dpkg-deb -R "$DEB" "$WORKDIR"

PKGNAME=$(sed -n 's/^Package: *//p' "$WORKDIR/DEBIAN/control")
if [ -z "$PKGNAME" ]; then
    echo "fix-deb-self-dependency.sh: couldn't find Package: field in $DEB" >&2
    exit 1
fi

BEFORE=$(sed -n 's/^Depends: //p' "$WORKDIR/DEBIAN/control")
# Drop a "<pkgname>" or "<pkgname> (>= ...)" entry, plus its trailing/leading
# comma-space, from the Depends: line only.
AFTER=$(printf '%s' "$BEFORE" | sed -E "s/(^|, )${PKGNAME}( \([^)]*\))?(, |$)/\3/g; s/, $//; s/^, //")

if [ "$BEFORE" != "$AFTER" ]; then
    echo "fix-deb-self-dependency.sh: stripped self-dependency on '$PKGNAME' from Depends:"
    echo "  before: $BEFORE"
    echo "  after:  $AFTER"
    sed -i "s/^Depends: .*/Depends: ${AFTER}/" "$WORKDIR/DEBIAN/control"
    dpkg-deb -b "$WORKDIR" "$DEB" >/dev/null
else
    echo "fix-deb-self-dependency.sh: no self-dependency found, $DEB left untouched"
fi
