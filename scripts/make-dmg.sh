#!/bin/bash
# Builds a universal (Apple Silicon + Intel) release and packages it as dist/ArrowEch-<version>.dmg
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"
VERSION="$(sed -n 's/^project(ArrowEch VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)"
BUILD="$ROOT/build-release"
ARTEFACTS="$BUILD/ArrowEch_artefacts/Release"
STAGE="$BUILD/dmg-stage"
DMG="$ROOT/dist/ArrowEch-$VERSION.dmg"

echo "==> Building ArrowEch $VERSION (universal)"
cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DARROWECH_COPY_PLUGINS=OFF
cmake --build "$BUILD" --config Release --target ArrowEch_VST3 ArrowEch_AU ArrowEch_Standalone -j"$(sysctl -n hw.ncpu)"

echo "==> Staging"
rm -rf "$STAGE" && mkdir -p "$STAGE"
cp -R "$ARTEFACTS/VST3/ArrowEch.vst3" "$STAGE/"
cp -R "$ARTEFACTS/AU/ArrowEch.component" "$STAGE/"
cp -R "$ARTEFACTS/Standalone/ArrowEch.app" "$STAGE/"
cp "$ROOT/packaging/Install ArrowEch.command" "$STAGE/"
cp "$ROOT/packaging/READ ME FIRST.txt" "$STAGE/"
chmod +x "$STAGE/Install ArrowEch.command"

# Ad-hoc sign so Apple Silicon hosts will load the binaries.
for bundle in "$STAGE/ArrowEch.vst3" "$STAGE/ArrowEch.component" "$STAGE/ArrowEch.app"; do
    codesign --force --deep --sign - "$bundle"
done

echo "==> Creating DMG"
mkdir -p "$ROOT/dist"
rm -f "$DMG"
hdiutil create -volname "ArrowEch $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
echo "==> Done: $DMG"
