$ErrorActionPreference = "Stop"
if (!(Test-Path "ThirdParty/JUCE/CMakeLists.txt")) {
  Write-Error "JUCE is missing. Put JUCE 9.0.2 in ThirdParty/JUCE."
}
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --config Release --target IDWVoiceMIDIStudio_VST3 IDWVoiceMIDIStudio_Standalone
Write-Host "Windows build complete. Check build-win/IDWVoiceMIDIStudio_artefacts/Release/"
