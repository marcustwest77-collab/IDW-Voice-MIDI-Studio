# V7 Validation Plan
1. Build Standalone/VST3 on Windows and Standalone/VST3/AU on macOS.
2. Sine tests: 110, 220, 440, 880 Hz; verify A2/A3/A4/A5.
3. Sweep 70–1000 Hz and measure pitch error/cents and CPU.
4. Confirm no heap allocation is introduced by YIN frame buffers after prepareToPlay.
5. Verify note-on/off pairing and pitch wheel bounds.
6. Verify MPE channels stay inside configured member zone.
7. MIDI Learn: arm each destination, send CC, confirm mapping and normalized parameter update.
8. Toggle all 12 custom-scale notes and verify quantization.
9. Beatbox test corpus: kick/snare/hat precision/recall.
10. DAWs: Ableton Live, FL Studio, Studio One, Logic Pro.
11. Test 44.1/48/96 kHz and 64/128/256/512 sample buffers.
12. Save/reload plugin state and named presets.
