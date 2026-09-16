# IDW Voice MIDI Studio V8 — Release Preparation Build

This package moves V7 toward actual platform builds.

## What V8 adds
- Windows, macOS and Linux build scripts.
- GitHub Actions build workflow.
- Release/build/host validation checklist.
- CMake metadata updated to V8.
- Release-preparation structure for VST3 and Standalone.
- Existing V7 YIN pitch engine, MIDI Learn, custom scales, MPE, presets and beatbox MIDI remain.

## Dependency
JUCE is intentionally not redistributed in this ZIP. Put a compatible JUCE checkout in:

    ThirdParty/JUCE

JUCE's current official release is 9.0.2 as of September 2026. JUCE's official CMake plugin example requires CMake 3.22+ and demonstrates VST3/AU/Standalone targets.

## Build
Windows:
    powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1

macOS:
    chmod +x scripts/build-macos.sh
    ./scripts/build-macos.sh

Linux:
    chmod +x scripts/build-linux.sh
    ./scripts/build-linux.sh

## Status
This package is release-prep source. It is NOT claimed as a compiled, signed, DAW-validated commercial binary. Final Windows/macOS binaries require the corresponding native toolchain, JUCE dependency, host testing, signing and (for macOS distribution) notarization.

Clean-room implementation: no Vochlea Dubler proprietary source, models, UI, or assets.
