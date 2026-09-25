# V4.1 validation status

This is an upgraded source delivery, not a compiled Windows release.

Performed in this session:
- Compiled and ran SetupDiagnosticsTests.cpp with g++ C++17, -Wall -Wextra -Werror: all nine scenarios passed.
- C++ syntax checks against the locally available JUCE headers: see SYNTAX-CHECK.txt in the package.

Not performed: full application link/build; Windows or macOS execution; pluginval; live microphone, listening, MIDI driver or FL Studio tests; UI screenshot rendering.
The environment has no Windows toolchain, and Linux audio/GUI development dependencies were unavailable. Dependency installation failed due to environment permissions.

The older VALIDATION.md describes the supplied V4.0 build, not this upgrade. It is retained as historical evidence only.
The Windows build launcher requires a successful compilation and both CTest suites before creating a new portable ZIP. It does not replace any installed plugin.

Before release: build on Windows, run pluginval, test the live setup scenarios in START-HERE.html, and check all controls at default and minimum window sizes.
