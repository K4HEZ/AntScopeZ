#!/bin/sh
#
# macOS release build + .dmg packaging.
#
# Usage: ./build.sh [build-dir]
#
# Previously this script was invoked by qmake with four positional arguments
# ($$PWD, $$DESTDIR, $$TARGET, $$VER_ARG). It is now self-contained: the version
# comes from CMakeLists.txt and the paths are derived from the build directory.

set -e

SRC_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR=${1:-"$SRC_DIR/build-cmake"}
TARGET=AntScopeZ

VERSION=$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9][0-9.]*\).*/\1/p' \
    "$SRC_DIR/CMakeLists.txt" | head -1)

if [ -z "$VERSION" ]; then
    echo "error: could not read VERSION from CMakeLists.txt" >&2
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

APP="$BUILD_DIR/$TARGET.app"

if [ ! -d "$APP" ]; then
    echo "error: $APP not found -- did the build produce a bundle?" >&2
    exit 1
fi

echo "Deploying..."
macdeployqt "$APP" -dmg

DMG="$BUILD_DIR/${TARGET}_${VERSION}.dmg"
mv "$BUILD_DIR/$TARGET.dmg" "$DMG"

echo
echo "Done! $DMG"
echo
