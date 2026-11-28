# V9.2 tests

Build and run `ctest --test-dir build -C Release --output-on-failure`, or one target such as `IDWTests`.
Five C++ test programs are registered: `IDWTests` (processor regressions in ProcessorTests.cpp), `IDWSetupTests` (setup diagnostics), `IDWHarmonyTests` (harmony and scales), `IDWSynthTests` (Studio Instrument) and `IDWRollingTests` (rolling MIDI and retrospective capture). `IDWTests` emits a MIDI export sample and editor PNGs into the build directory.
The companion's Python tests are separate: `python -m unittest discover -s Tests -p "test_*.py" -v`.

See ../RELEASE_CHECKLIST.md for current build status and CI runs; ../docs/VALIDATION.md is kept as the historical V4 record.

Before using V9.2 for a live performance, test your microphone and preferred synth in FL Studio: sustained notes, slides, fast note changes, silence release, Panic, trained drum hits, and MIDI export/import. Check buffer sizes supported by your audio device and match synth bend range. Preserve a backup of the previous plugin and important sessions.
