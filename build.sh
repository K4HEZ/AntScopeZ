#!/bin/sh
#
# macOS release build + .dmg packaging.
#
# Usage: ./build.sh [build-dir]
#
# Previously this script was invoked by qmake with four positional arguments
# ($$PWD, $$DESTDIR, $$TARGET, $$VER_ARG). It is now self-contained: the version
# comes from CMakeLists.txt and the paths are derived from the build directory.
#
# Packaging goes through CPack (the "package" target, i.e. cpack's DragNDrop
# generator -- see the if(APPLE) CPack section in CMakeLists.txt) instead of
# calling macdeployqt directly: cpack runs the project's own `cmake --install`
# rules first, which already bundle the Qt frameworks/plugins into the .app
# and strip the dev machine's absolute Qt rpath (verified 2026-10-19). This
# also means macdeployqt itself no longer needs to be on PATH -- CMake invokes
# it internally with the absolute path resolved at configure time.

set -e

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR=${1:-"$SRC_DIR/build-cmake"}
TARGET=AntScopeZ

VERSION=$(head -1 "$SRC_DIR/version.txt")

if [ -z "$VERSION" ]; then
    echo "error: could not read version from $SRC_DIR/version.txt" >&2
    exit 1
fi

echo "Building $TARGET $VERSION"
echo "  source: $SRC_DIR"
echo "  build:  $BUILD_DIR"
echo

# Translations (locales/*.ts -> QtLanguage_*.qm) are compiled by the CMake
# build itself now (qt_add_translations() in CMakeLists.txt, target
# release_translations), including into the .app bundle on this platform --
# no separate manual lrelease step needed here anymore.

echo "Configuring..."
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
echo

echo "Building..."
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo

echo "Packaging..."
cmake --build "$BUILD_DIR" --target package
echo

DMG="$BUILD_DIR/${TARGET}-${VERSION}-Darwin.dmg"

if [ ! -f "$DMG" ]; then
    echo "error: $DMG not found -- did cpack produce a differently-named package?" >&2
    exit 1
fi

echo
echo "Done! $DMG"
echo
