#!/bin/bash
set -e
JUCE_PATH="${1:-}"
if [ -n "$JUCE_PATH" ]; then
  CMAKE_JUCE_ARG="-DIDW_JUCE_PATH=$JUCE_PATH"
elif [ -f ThirdParty/JUCE/CMakeLists.txt ]; then
  CMAKE_JUCE_ARG=""
else
  echo "No local ThirdParty/JUCE checkout found; CMake will fetch JUCE 9.0.2 automatically (requires internet access)."
  CMAKE_JUCE_ARG=""
fi
cmake -S . -B build-mac -G Xcode -DCMAKE_BUILD_TYPE=Release $CMAKE_JUCE_ARG
cmake --build build-mac --config Release --target IDWVoiceMIDIStudio_VST3 IDWVoiceMIDIStudio_Standalone
echo "macOS VST3/Standalone build complete. AU target should be enabled in the macOS release CMake after host validation."
