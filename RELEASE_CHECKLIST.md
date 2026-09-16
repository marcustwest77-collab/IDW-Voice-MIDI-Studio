# IDW Voice MIDI Studio V8 Release Checklist

## Build gate
- [ ] Add JUCE 9.0.2 at ThirdParty/JUCE
- [ ] Configure with CMake 3.22+
- [ ] Windows x64 VST3 compiles
- [ ] Windows Standalone compiles
- [ ] macOS arm64/x86_64 VST3 compiles
- [ ] macOS Standalone compiles
- [ ] Enable/build AU on macOS after VST3/Standalone pass

## Functional gate
- [ ] Microphone permission and device selection
- [ ] Voice -> note on/off
- [ ] Pitch bend
- [ ] Scale lock/custom scale
- [ ] MIDI Learn
- [ ] MPE member channels
- [ ] Kick/snare/hat triggers
- [ ] Preset save/load
- [ ] State restoration

## Host gate
- [ ] FL Studio
- [ ] Ableton Live
- [ ] Studio One
- [ ] Logic Pro (AU)
- [ ] 44.1/48/96 kHz
- [ ] 64/128/256/512 buffer sizes

## Distribution gate
- [ ] Windows code signing
- [ ] macOS Developer ID signing
- [ ] macOS notarization
- [ ] Installer packaging
- [ ] Versioned release notes
