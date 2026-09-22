# V4 tests

Build and run `IDWTests` or `ctest --test-dir build -C Release --output-on-failure`.
The automated suite is in ProcessorTests.cpp. It emits a MIDI export sample and editor PNGs into the build directory.

See ../docs/VALIDATION.md for actual test results and limits.

Before using V4 for a live performance, test your microphone and preferred synth in FL Studio: sustained notes, slides, fast note changes, silence release, Panic, trained drum hits, and MIDI export/import. Check buffer sizes supported by your audio device and match synth bend range. Preserve a backup of the previous plugin and important sessions.
