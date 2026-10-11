#!/usr/bin/env bash
set -euo pipefail

BUILD_ROOT="${1:-build/IDWVoiceMIDIStudio_artefacts/Release}"
OUT_DIR="${2:-release/Installer}"
VERSION="12.12.0"
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
cp docs/V10.1-UPGRADE.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V10.1-UPGRADE.md"
cp docs/V10.2-UPGRADE.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V10.2-UPGRADE.md"
cp docs/V10.3-UPGRADE.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V10.3-UPGRADE.md"
cp docs/V10.4-UPGRADE.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V10.4-UPGRADE.md"
cp docs/V10.5-UPGRADE.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V10.5-UPGRADE.md"
cp docs/V11-SONG-STUDIO.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V11-SONG-STUDIO.md"
cp docs/V11.1-UNIFIED-LYRICS.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V11.1-UNIFIED-LYRICS.md"
cp docs/V11.2-NATIVE-DICTATION.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V11.2-NATIVE-DICTATION.md"
cp docs/V11.3-SONG-RECOVERY.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V11.3-SONG-RECOVERY.md"
cp docs/V11.4-COUNT-IN.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V11.4-COUNT-IN.md"
cp docs/V12.6-RECORD-CLIPS.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.6-RECORD-CLIPS.md"
cp docs/V12.7-TAKE-SELECTION.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.7-TAKE-SELECTION.md"
cp docs/V12.8-PHRASE-SELECTION.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.8-PHRASE-SELECTION.md"
cp docs/V12.9-STEM-EXPORT.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.9-STEM-EXPORT.md"
cp docs/V12.10-BACKGROUND-EXPORT.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.10-BACKGROUND-EXPORT.md"
cp docs/V12.11-MIX-METERS.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.11-MIX-METERS.md"
cp docs/V12.12-EXPORT-CHECK.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.12-EXPORT-CHECK.md"
cp docs/V12.5-ZOOM-SEEK.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.5-ZOOM-SEEK.md"
cp docs/V12.4-SECTIONS-LOOP.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.4-SECTIONS-LOOP.md"
cp docs/V12.3-FADES-GRID.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.3-FADES-GRID.md"
cp docs/V12.2-CLIP-ARRANGEMENT.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.2-CLIP-ARRANGEMENT.md"
cp docs/V12.1-TRACK-ARMING.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12.1-TRACK-ARMING.md"
cp docs/V12-FOUR-TRACK-STUDIO.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V12-FOUR-TRACK-STUDIO.md"
cp docs/V10.7-UPGRADE.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V10.7-UPGRADE.md"
cp docs/V10.6-UPGRADE.md "release/pkgroot/Library/Application Support/IDW Voice MIDI Studio/V10.6-UPGRADE.md"

pkgbuild \
  --root release/pkgroot \
  --identifier com.idwent.idwvoicemidistudio.pkg \
  --version "$VERSION" \
  --install-location / \
  "$OUT_DIR/$PKG_NAME"

cp "$OUT_DIR/$PKG_NAME" release/dmgroot/
cp docs/USER_MANUAL.md release/dmgroot/USER_MANUAL.md
cp docs/V10.1-UPGRADE.md release/dmgroot/V10.1-UPGRADE.md
cp docs/V10.2-UPGRADE.md release/dmgroot/V10.2-UPGRADE.md
cp docs/V10.3-UPGRADE.md release/dmgroot/V10.3-UPGRADE.md
cp docs/V10.4-UPGRADE.md release/dmgroot/V10.4-UPGRADE.md
cp docs/V10.5-UPGRADE.md release/dmgroot/V10.5-UPGRADE.md
cp docs/V11-SONG-STUDIO.md release/dmgroot/V11-SONG-STUDIO.md
cp docs/V11.1-UNIFIED-LYRICS.md release/dmgroot/V11.1-UNIFIED-LYRICS.md
cp docs/V11.2-NATIVE-DICTATION.md release/dmgroot/V11.2-NATIVE-DICTATION.md
cp docs/V11.3-SONG-RECOVERY.md release/dmgroot/V11.3-SONG-RECOVERY.md
cp docs/V11.4-COUNT-IN.md release/dmgroot/V11.4-COUNT-IN.md
cp docs/V12.6-RECORD-CLIPS.md release/dmgroot/V12.6-RECORD-CLIPS.md
cp docs/V12.7-TAKE-SELECTION.md release/dmgroot/V12.7-TAKE-SELECTION.md
cp docs/V12.8-PHRASE-SELECTION.md release/dmgroot/V12.8-PHRASE-SELECTION.md
cp docs/V12.9-STEM-EXPORT.md release/dmgroot/V12.9-STEM-EXPORT.md
cp docs/V12.10-BACKGROUND-EXPORT.md release/dmgroot/V12.10-BACKGROUND-EXPORT.md
cp docs/V12.11-MIX-METERS.md release/dmgroot/V12.11-MIX-METERS.md
cp docs/V12.12-EXPORT-CHECK.md release/dmgroot/V12.12-EXPORT-CHECK.md
cp docs/V12.5-ZOOM-SEEK.md release/dmgroot/V12.5-ZOOM-SEEK.md
cp docs/V12.4-SECTIONS-LOOP.md release/dmgroot/V12.4-SECTIONS-LOOP.md
cp docs/V12.3-FADES-GRID.md release/dmgroot/V12.3-FADES-GRID.md
cp docs/V12.2-CLIP-ARRANGEMENT.md release/dmgroot/V12.2-CLIP-ARRANGEMENT.md
cp docs/V12.1-TRACK-ARMING.md release/dmgroot/V12.1-TRACK-ARMING.md
cp docs/V12-FOUR-TRACK-STUDIO.md release/dmgroot/V12-FOUR-TRACK-STUDIO.md
cp docs/V10.7-UPGRADE.md release/dmgroot/V10.7-UPGRADE.md
cp docs/V10.6-UPGRADE.md release/dmgroot/V10.6-UPGRADE.md
cat > release/dmgroot/README.txt <<'EOF'
IDW Voice MIDI Studio V12.12 Preview

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
