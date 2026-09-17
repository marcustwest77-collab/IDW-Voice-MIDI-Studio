# IDW Voice MIDI Studio — User Manual

## Quick Start
1. Insert **IDW Voice MIDI Studio** on a mixer/input track that receives your microphone.
2. In FL Studio, set the plugin wrapper MIDI output port to a number such as **10**.
3. Set the destination synth's MIDI input port to the same number.
4. Stay quiet and press **CALIBRATE NOISE**.
5. Choose a factory preset or adjust Gate / Confidence / Bend Range manually.
6. Enable **SCALE LOCK**, choose a root, and select allowed scale notes if desired.
7. Sing. The destination synth should follow your voice.

## Main Controls
### Noise Gate
Minimum input level required before vocal notes are generated. Raise it in a noisy room; lower it for a quiet microphone or soft singing.

### Confidence
How certain the pitch detector must be before a note is accepted. Higher values reject uncertain pitches but can feel stricter. Lower values respond more easily.

### Bend Range
Pitch-bend range in semitones. The destination synth should use the same bend range for accurate slides.

### Pitch Calibration
Offsets detected pitch by cents. Leave at 0 unless your source or instrument requires tuning compensation.

### Calibrate Noise
Measure the current quiet-room input level and automatically place the gate above the ambient noise floor. Stay silent while pressing this button.

### Scale Lock
Forces outgoing notes to allowed scale notes.

### Root
Sets the scale root used by scale quantization.

### Scale Note Buttons
Choose which pitch classes are allowed. These buttons define a custom scale mask.

### MIDI Learn
Choose a destination in the MIDI Learn menu, arm MIDI Learn, then move a hardware MIDI control to map it.

### Save Preset
Saves the current settings as a user preset in the IDW Voice MIDI Studio preset folder.

## Factory Presets
### Clean Vocal
Balanced general-purpose voice tracking.

### Tight Tracking
Higher confidence and tighter gate for cleaner note changes in controlled rooms.

### Smooth Lead
Lower confidence threshold, wider bend and gesture response for melodic lead performance.

### Scale Locked Lead
Turns on scale lock with a major-scale mask for safer melodic performance.

### Wide Bend Performance
Uses a larger pitch-bend range for expressive slides. Match the receiving synth bend range.

### Beatbox Drums
Prioritizes beatbox triggering and keeps standard kick/snare/hat MIDI notes.

### Expressive MPE
Enables MPE and gesture CC for expressive compatible instruments.

### Low-Latency Live
Uses responsive thresholds intended as a starting point for live performance.

## FL Studio Routing
1. Add IDW Voice MIDI Studio to the mixer insert receiving the microphone.
2. Open the plugin wrapper settings.
3. Assign a MIDI output port, for example 10.
4. Load a synth or sampler.
5. Set that instrument wrapper's MIDI input port to the same value.
6. Sing into the microphone.

## Pitch Bend Setup
If pitch slides sound too small or too large, make sure the destination synth's pitch-bend range matches the **BEND RANGE** shown in IDW Voice MIDI Studio.

## Beatbox Mode
Beatbox detection sends drum notes on MIDI channel 10. Default notes are:
- Kick: 36
- Snare: 38
- Hi-hat: 42

## MPE
Enable MPE only when the destination instrument supports it. IDW Voice MIDI Studio allocates notes across the configured MPE channel range and sends per-note expression on those channels.

## Latency
For live playing, use an ASIO driver on Windows. Start at a 128-sample buffer. Try 64 samples if the computer remains stable; use 256 samples if you hear pops or dropouts.

## Troubleshooting
### Plugin loads but no vocal response
- Confirm the mixer/input track is receiving the microphone.
- Confirm microphone permission is enabled.
- Press CALIBRATE NOISE while quiet.
- Lower Noise Gate if the input is too soft.
- Lower Confidence slightly if pitch is not being accepted.

### Vocal pitch moves but synth is silent
- Confirm MIDI output and input port numbers match.
- Confirm the destination instrument accepts MIDI.

### Notes trigger incorrectly
- Recalibrate the noise gate.
- Raise Confidence.
- Enable Scale Lock.
- Confirm the custom scale buttons match the desired scale.

### Slides sound wrong
- Match the destination synth pitch-bend range to IDW's Bend Range.

### Too much delay
- Reduce audio buffer size.
- Use ASIO on Windows.
- Disable unnecessary high-latency plugins while performing.

## Recommended First Session
1. Load **Clean Vocal**.
2. Press **CALIBRATE NOISE** while silent.
3. Route MIDI to a simple synth.
4. Sing sustained notes.
5. Test slides.
6. Enable Scale Lock if desired.
7. Save your own preset once the response feels right.
