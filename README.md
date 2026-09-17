# IDW Voice MIDI Studio

**Current release candidate: 0.8.2**

IDW Voice MIDI Studio is a clean-room real-time voice-to-MIDI performance engine for Windows and macOS. It builds as Standalone and VST3 on both platforms and as Audio Unit on macOS.

## Core performance features
- Real-time monophonic voice-to-MIDI pitch tracking using a YIN-based detector.
- Voice level mapped to MIDI velocity.
- Pitch bend referenced to the final generated note, including scale-locked notes.
- 12-note custom scale mask and root selection.
- Gesture-driven MIDI CC.
- MPE-style channel allocation with MIDI channel 10 reserved for beatbox drums.
- Kick, snare and hi-hat beatbox classification using onset, zero-crossing, brightness and crest features.
- MIDI Learn foundation.
- Preset save/load and DAW state restoration.
- Black/gold IDW performance UI with live mic level meter and mic calibration control.

## Calibration
For a quick noise-floor setup, remain silent and press **CALIBRATE MIC**. The current room/microphone level is used to set a practical gate value. Fine-tune **MIC GATE** and **PITCH CONFIDENCE** afterward if needed.

## Build and validation
JUCE is fetched automatically with CMake when `ThirdParty/JUCE` is not present locally.

Requirements:
- CMake 3.22+
- JUCE 9.0.2
- Windows: current Visual Studio C++ toolchain
- macOS: current Xcode command-line toolchain

GitHub Actions builds and packages Windows and macOS, validates VST3 with pluginval, validates the Audio Unit with `auval`, and creates install-ready artifacts.

## Installer outputs
Windows:
- Standalone application
- VST3
- Versioned Inno Setup installer

macOS:
- Standalone application
- VST3
- Audio Unit
- Versioned PKG installer
- Versioned DMG

## Versioned releases
Pushing a tag such as `v0.8.2` runs the full Windows/macOS validation pipeline. If both platforms pass, GitHub Actions publishes the installer files to a GitHub Release automatically.

## Signing status
CI-generated packages are currently unsigned. Public commercial distribution should add:
- Windows Authenticode/code-signing certificate.
- Apple Developer ID Application and Installer certificates.
- Apple notarization credentials and notarization/stapling steps.

## Clean-room note
No Vochlea Dubler proprietary source code, models, UI assets, or private implementation details are used.
