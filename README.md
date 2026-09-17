# IDW Voice MIDI Studio V9.1

IDW Voice MIDI Studio is a clean-room voice-to-MIDI performance plugin and standalone application from In Da Wind Entertainment.

## Formats
- Windows: Standalone + VST3
- macOS: Standalone + VST3 + Audio Unit

## Core Features
- Real-time YIN pitch detection
- Voice-to-MIDI note generation
- Vocal-level MIDI velocity
- Scale-aware pitch bend
- Custom scale lock and root selection
- Beatbox-to-drum MIDI triggering
- Gesture-to-MIDI CC
- MPE support
- MIDI Learn
- User preset save/load
- Noise-floor calibration
- Pitch calibration

## V9.1 User Experience
- Built-in **HELP / QUICK START** manual
- Hover tooltips for the main performance controls
- Factory preset browser
- Saved user presets appear in the same browser
- Full written manual in `docs/USER_MANUAL.md` and release packages

### Factory Presets
- Clean Vocal
- Tight Tracking
- Smooth Lead
- Scale Locked Lead
- Wide Bend Performance
- Beatbox Drums
- Expressive MPE
- Live Responsive

## Quick Start in FL Studio
1. Put IDW Voice MIDI Studio on the mixer insert receiving your microphone.
2. Set the IDW wrapper MIDI Output Port to a value such as `10`.
3. Set the destination synth wrapper MIDI Input Port to the same value.
4. Press **CALIBRATE NOISE** while the room is quiet.
5. Load **Clean Vocal**.
6. Sing and confirm the destination instrument follows the voice.

See `docs/USER_MANUAL.md` or press **HELP / QUICK START** inside the plugin for the full guide.

## Build
The CMake project is pinned to JUCE `9.0.2` and supports an optional local checkout at `ThirdParty/JUCE`; otherwise CMake fetches JUCE automatically.

Windows:

    powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1

macOS:

    chmod +x scripts/build-macos.sh
    ./scripts/build-macos.sh

GitHub Actions builds, validates and packages Windows/macOS artifacts. VST3 validation uses pluginval and macOS Audio Unit validation uses `auval`.

## Distribution Status
CI produces Windows and macOS installer packages. These builds are suitable for development and testing. Public/commercial distribution should add platform code signing; macOS distribution should also add Apple notarization.

Clean-room implementation: no Vochlea Dubler proprietary source, models, UI, or assets are used.
