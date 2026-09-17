#!/usr/bin/env bash
set -euo pipefail

BUILD_ROOT="${1:-build/IDWVoiceMIDIStudio_artefacts/Release}"
OUT_DIR="${2:-release/Installer}"
VERSION="0.8.2"
PKG_NAME="IDW-Voice-MIDI-Studio-Setup-${VERSION}-macOS-arm64.pkg"
DMG_NAME="IDW-Voice-MIDI-Studio-${VERSION}-macOS-arm64.dmg"

rm -rf release/pkgroot release/dmgroot
mkdir -p \
  release/pkgroot/Applications \
  release/pkgroot/Library/Audio/Plug-Ins/VST3 \
  release/pkgroot/Library/Audio/Plug-Ins/Components \
  release/dmgroot \
  "$OUT_DIR"

cp -R "$BUILD_ROOT/Standalone/IDW Voice MIDI Studio.app" release/pkgroot/Applications/
cp -R "$BUILD_ROOT/VST3/IDW Voice MIDI Studio.vst3" release/pkgroot/Library/Audio/Plug-Ins/VST3/
cp -R "$BUILD_ROOT/AU/IDW Voice MIDI Studio.component" release/pkgroot/Library/Audio/Plug-Ins/Components/

pkgbuild \
  --root release/pkgroot \
  --identifier com.idwent.idwvoicemidistudio.pkg \
  --version "$VERSION" \
  --install-location / \
  "$OUT_DIR/$PKG_NAME"

cp "$OUT_DIR/$PKG_NAME" release/dmgroot/
cat > release/dmgroot/README.txt <<EOF
IDW Voice MIDI Studio ${VERSION}
In Da Wind Entertainment

Real-time voice-to-MIDI performance engine.

This installer places:
- Standalone app in /Applications
- VST3 in /Library/Audio/Plug-Ins/VST3
- Audio Unit in /Library/Audio/Plug-Ins/Components

This CI-generated package is unsigned until Developer ID certificates and notarization credentials are configured.
EOF

hdiutil create \
  -volname "IDW Voice MIDI Studio ${VERSION}" \
  -srcfolder release/dmgroot \
  -ov \
  -format UDZO \
  "$OUT_DIR/$DMG_NAME"

pkgutil --check-signature "$OUT_DIR/$PKG_NAME" || true
hdiutil verify "$OUT_DIR/$DMG_NAME"
