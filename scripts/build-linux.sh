#!/bin/bash
set -e
test -f ThirdParty/JUCE/CMakeLists.txt || { echo "JUCE missing: ThirdParty/JUCE"; exit 1; }
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --config Release --target IDWVoiceMIDIStudio_VST3 IDWVoiceMIDIStudio_Standalone -j2
