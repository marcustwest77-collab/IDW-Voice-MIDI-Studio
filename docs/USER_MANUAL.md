# IDW Voice MIDI Studio 4.0

IDW VOICE MIDI STUDIO / VERSION 4

FIRST SOUND
1. In the standalone Audio/MIDI settings, choose your microphone and speakers/headphones.
2. Enable Preview sound. Test note plays a short C4 tone and also sends MIDI.
3. Stay quiet and Calibrate noise. Sing a steady note. The green readout shows accepted MIDI notes.
4. Preview sound is a simple setup instrument. Turn it off when listening through a DAW synth.

FL STUDIO / VST3
1. Load IDW as an effect on the Mixer insert receiving your microphone.
2. Open IDW's wrapper Settings and set MIDI Output port to 10 (or another unused port).
3. Load your destination instrument and set its wrapper MIDI Input port to the same number.
4. Press Test note. It sends MIDI 60 on channel 1, independently of vocal detection.
5. If the MIDI counter increases but your synth is silent, check routing and the synth's channel.
6. Match Bend Range in your synth. IDW sends RPN pitch-bend sensitivity; some instruments ignore it.
7. Start with Clean Vocal. Calibrate after loading a preset, since presets change the noise gate.

STANDALONE MIDI
Select a MIDI output in the standalone Audio/MIDI settings. For FL Studio on the same computer,
use an existing virtual MIDI port (such as loopMIDI), enable that port as an input in FL Studio,
and select a receiving instrument. The standalone app does not install a virtual MIDI driver.

SCALE MODES
Scale lock OFF: free pitch, with MIDI note changes plus pitch bends.
Strict scale: both note selection and sounding pitch stay on the selected scale.
Natural vibrato: scale note plus up to 45 cents of within-note vocal expression.
The twelve buttons are relative to the selected root and display the resulting note names.
At least one scale note must remain enabled.

DRUM LAB
Enable Beatbox for drum output on MIDI channel 10. Change each pad's MIDI note below its button.
With no trained pads, the engine uses basic kick/snare/hat spectral rules.
Select a pad, press Train 5 hits, and make five distinct examples with pauses between them.
Training suppresses melody and drum output while it learns. Repeat for additional pads.
Once profiles exist, only trained pads are classified. Ambiguous/unfamiliar hits are rejected.
These are local spectral templates, not a neural model. Accuracy depends on microphone,
background noise and distinct sounds. Save a preset to retain your profiles.
Reset pads removes all learned profiles and restores basic three-sound detection.

CAPTURE / EXPORT
Set BPM to your DAW tempo before recording. Record MIDI, perform, then Stop recording.
Export MIDI saves notes, drums, bends and controller expression. Drag the .mid file into your DAW.
A take is limited to ten minutes / 250,000 displayed events. An overflow is visibly marked.
The current take lives in the editor: export it before closing the plugin window.
Capture starts from new MIDI events; begin before singing. Notes held when stopping are closed
in the exported file, without stopping your live instrument. Tempo is fixed for each take.

SAFETY / TROUBLESHOOTING
PANIC stops sounding MIDI notes. Notes resume after a short suppression interval.
Hear microphone passes raw microphone audio to the output; use headphones to prevent feedback.
MPE reserves channel 10 for drums. Choose an MPE synth and match its bend range.
MIDI Learn is channel-aware. Mappings and drum profiles save in user presets and DAW sessions.
Changing input devices stops an active capture. Export the take before switching devices.
The displayed LOAD is callback time as a percentage of the audio block budget, not total CPU.
Lower host buffers reduce device latency; pitch estimation still needs a short window of sound.

LIMITS
Monophonic voice tracking, 65-1000 Hz. This version does not provide polyphonic transcription,
AI voice cloning, cloud processing, a full synthesizer, or automatic access to DAW routing.
