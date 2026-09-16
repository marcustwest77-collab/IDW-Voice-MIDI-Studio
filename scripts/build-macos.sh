#!/bin/bash
set -e
if [ ! -f ThirdParty/JUCE/CMakeLists.txt ]; then
  echo "JUCE is missing. Put JUCE 9.0.2 in ThirdParty/JUCE."
  exit 1
fi
cmake -S . -B build-mac -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build build-mac --config Release --target IDWVoiceMIDIStudio_VST3 IDWVoiceMIDIStudio_Standalone
echo "macOS VST3/Standalone build complete. AU target should be enabled in the macOS release CMake after host validation."
