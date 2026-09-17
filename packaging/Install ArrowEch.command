#!/bin/bash
# Copies ArrowEch into your user plug-in folders and clears the download quarantine flag.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"

echo "Installing ArrowEch... (the blob is very excited)"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/ArrowEch.vst3" "$AU/ArrowEch.component"
cp -R "$HERE/ArrowEch.vst3" "$VST3/"
cp -R "$HERE/ArrowEch.component" "$AU/"
xattr -dr com.apple.quarantine "$VST3/ArrowEch.vst3" "$AU/ArrowEch.component" 2>/dev/null || true

# Make Logic / GarageBand notice the new Audio Unit.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Done!"
echo "  VST3 -> $VST3/ArrowEch.vst3"
echo "  AU   -> $AU/ArrowEch.component"
echo ""
echo "Rescan plug-ins in your DAW. Look for 'ArrowEch' by 'Arrow'."
echo "You can close this window."
