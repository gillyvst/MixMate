#!/bin/bash
# Installs MixMate for the current user (AU + VST3).
set -e
cd "$(dirname "$0")"

AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$AU_DIR" "$VST3_DIR"

echo "Installing MixMate..."
rm -rf "$AU_DIR/MixMate.component" "$VST3_DIR/MixMate.vst3"
cp -R "MixMate.component" "$AU_DIR/"
cp -R "MixMate.vst3" "$VST3_DIR/"

# Plugins downloaded from the internet are blocked by macOS until this flag is removed.
xattr -dr com.apple.quarantine "$AU_DIR/MixMate.component" "$VST3_DIR/MixMate.vst3" 2>/dev/null || true

# Make Logic / GarageBand notice the new AU straight away.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Done! MixMate is installed:"
echo "  AU:   $AU_DIR/MixMate.component"
echo "  VST3: $VST3_DIR/MixMate.vst3"
echo ""
echo "Restart your DAW (in Ableton: Settings > Plug-Ins > Rescan if it doesn't appear)."
