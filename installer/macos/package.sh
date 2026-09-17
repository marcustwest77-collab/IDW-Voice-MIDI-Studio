#!/usr/bin/env bash
set -euo pipefail

BUILD_ROOT="${1:-build/IDWVoiceMIDIStudio_artefacts/Release}"
OUT_DIR="${2:-release/Installer}"
VERSION="0.9.1"
PKG_NAME="IDW-Voice-MIDI-Studio-Setup-macOS-arm64.pkg"
DMG_NAME="IDW-Voice-MIDI-Studio-macOS-arm64.dmg"

rm -rf release/pkgroot release/dmgroot
mkdir -p \
  release/pkgroot/Applications \
  release/pkgroot/Library/Audio/Plug-Ins/VST3 \
  release/pkgroot/Library/Audio/Plug-Ins/Components \
  "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio" \
  release/dmgroot \
  "$OUT_DIR"

cp -R "$BUILD_ROOT/Standalone/IDW Voice MIDI Studio.app" release/pkgroot/Applications/
cp -R "$BUILD_ROOT/VST3/IDW Voice MIDI Studio.vst3" release/pkgroot/Library/Audio/Plug-Ins/VST3/
cp -R "$BUILD_ROOT/AU/IDW Voice MIDI Studio.component" release/pkgroot/Library/Audio/Plug-Ins/Components/
cp docs/USER_MANUAL.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/USER_MANUAL.md"

pkgbuild \
  --root release/pkgroot \
  --identifier com.idwent.idwvoicemidistudio.pkg \
  --version "$VERSION" \
  --install-location / \
  "$OUT_DIR/$PKG_NAME"

cp "$OUT_DIR/$PKG_NAME" release/dmgroot/
cp docs/USER_MANUAL.md release/dmgroot/USER_MANUAL.md
cat > release/dmgroot/README.txt <<'EOF'
IDW Voice MIDI Studio V9.1

This installer places:
- Standalone app in /Applications
- VST3 in /Library/Audio/Plug-Ins/VST3
- Audio Unit in /Library/Audio/Plug-Ins/Components
- User manual in /Library/Application Support/IDW Voice MIDI Studio

The plugin also includes a built-in HELP / QUICK START guide and factory preset bank.

This CI-generated package is unsigned until Developer ID certificates and notarization credentials are configured.
EOF

hdiutil create \
  -volname "IDW Voice MIDI Studio" \
  -srcfolder release/dmgroot \
  -ov \
  -format UDZO \
  "$OUT_DIR/$DMG_NAME"

pkgutil --check-signature "$OUT_DIR/$PKG_NAME" || true
hdiutil verify "$OUT_DIR/$DMG_NAME"
