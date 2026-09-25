# IDW V6 — implementation and remaining work

Status: locally tested source candidate, 23 September 2026. Windows V6 binaries have not been built. V5 remains the available runnable build. No GitHub changes were made for this candidate.

## What is implemented

### Built-in Studio Instrument
Open **Studio instrument** at the top of either view. Enable the instrument and press Test note. It replaces the simple preview while enabled.

- 32 simultaneous voices in a fixed pool; no voice allocations during rendering.
- Lead, chord and bass each select Sub/sine, Saw lead, Pulse, Warm keys or FM bell.
- MIDI channel 1 lead, 2 chords, 3 bass; channel 10 synthesized kick (35/36), snare (38/40) and hats/other percussion.
- Attack/release, brightness, volume, 250 ms feedback echo, MIDI sustain and pitch-bend range via RPN.
- Incoming MIDI keyboard notes can play the instrument; outgoing voice MIDI is still generated.
- Panic silences voices and echo. Legacy sessions default the new instrument to off.
- MPE uses the lead patch on member channels; IDW reserves channel 10 for drums. Harmony remains disabled with MPE.

This is an original compact synthesizer, not a sample library or an emulation of commercial instruments. It is not a voice clone. Internal synthesis does not change the monophonic live pitch detector. Chord voicing comes from the existing arrangement engine.

### Audio Lab companion
Run `Companion/Run-Audio-Lab.cmd` with Python 3.10 installed (include Tkinter and Python launcher). It is a separate application in this candidate. To add the local model, run `Companion/Setup-Transcription.cmd` once with internet access. This downloads dependencies, not recordings.

1. Export a 16-bit PCM WAV from your DAW, mono/stereo, 8–96 kHz, up to 10 minutes and 25 MiB.
2. Choose WAV; inspect measured duration, levels and clipped samples.
3. On Local recording tools, choose Transcribe WAV to MIDI locally and an output folder.
4. A unique take folder contains transcription.mid, notes.csv and report.json. Import the MIDI into your DAW and correct any mistaken notes.

Basic Pitch supports overlapping pitches and works best with a single instrument. This integration processes a recorded file, not a live microphone stream. Stereo is analyzed by the model, not separated into independent instrument tracks. No local model inference has executed in this environment; the proposed Windows workflow includes a real inference gate using a synthetic triad. Contract tests with a mock model are separately identified.

### Optional cloud voice conversion
Audio Lab implements the documented Kits API, with an API key held in process memory, numeric model IDs, explicit approval before every upload, job receipts and manual job-status checks. No startup network call or automatic audio upload occurs. The provider may charge credits and retain uploaded files under its account terms.

1. Create your own voice model in Kits or choose a voice you are authorized to use. Training currently happens in the provider's application.
2. Enter your API key directly into Audio Lab's masked field; do not paste it into chat or a project file.
3. List available models (first page), copy the model ID, select your WAV and press Upload WAV and convert voice.
4. Review the upload confirmation. On submission, copy the returned job ID into the Job ID field.
5. Check the job until it succeeds. Copy its outputFileUrl into your browser to download the result. Kits documents that result URLs expire; request job status again if needed.
6. Import the converted audio into your DAW.

Network requests have timeouts and are not retried automatically. A submission timeout may still have created a billed job: check provider history before retrying. Closing Audio Lab does not cancel an accepted cloud job. The API key is not persisted; receipts contain only job ID, status and model ID. Result URLs appear in the UI but are not written to receipts.

**Still required:** a user account, trained/authorized model and a live roundtrip test. No cloud inference or voice cloning was performed during development. This is an implemented integration with pending service validation, not a bundled offline clone trainer. It does not offer general-purpose cloud synchronization, collaboration or GPU hosting.

### FL Studio Performance Router
This is an opt-in MIDI controller script for **standalone IDW output through an existing MIDI port**. It does not receive VST3 wrapper MIDI directly.

1. Create four instrument channels in FL Studio. Rename them exactly `IDW Lead`, `IDW Chords`, `IDW Bass`, `IDW Drums`. Each name must be unique.
2. Copy the entire `Integrations/FL-Studio/IDW-Performance` directory under the Hardware directory in your FL Studio user-data Settings folder.
3. Start/restart FL Studio. Enable the MIDI input receiving IDW and choose **IDW Performance Router** as its Controller type.
4. Select the matching MIDI output in standalone IDW. A virtual MIDI port must already exist; this package does not install a driver.
5. Turn MPE off. Press Test note: the script directs channel 1 to IDW Lead. Chords on 2, bass on 3 and drums on 10 target the named channels.
6. The script skips missing/duplicate targets and prints a diagnostic. MIDI Panic releases held notes. Stop playing and use Panic before renaming, deleting or rearranging instrument channels.

This candidate routes **notes only**. It consumes pitch bends/CC on its four mapped channels to avoid unintentionally controlling FL's selected channel; use the documented VST wrapper route for expressive bends and MPE. It does not instantiate instruments, select audio drivers, change wrapper ports, arm tracks or edit project files. Mock API tests passed; actual FL Studio testing remains necessary. Session edits while notes are held can defeat index-based note release; use FL's stop/panic if needed.

## Validation

Passed locally:
- C++ synth tests: five patches, 44.1/48/96 kHz, polyphony, channel isolation, sustain, voice stealing, Panic and drums.
- Existing independent harmony and setup diagnostic tests.
- 13 Python tests covering WAV validation, upload approval, request format/errors, redirect rejection, transcription export contract and FL note routing.
- C++17 syntax checks of processor, editor and processor regression tests with JUCE 9.0.2 headers.

Prepared, not yet executed:
- Full Windows compile/link, existing processor regressions plus sample-positioned incoming MIDI, synth Panic and instrument state persistence.
- Actual minimum/default interface rendering including the Instrument panel.
- Real Basic Pitch inference against a synthetic chord.
- pluginval strictness 5 on the V6 VST3.

Still requires a human/device session: microphone tracking, audible synth quality, CPU load/latency, FL routing and recording, provider/account voice conversion, and recovery after restart.

## Next differentiators, not implemented yet

1. **Retrospective capture:** retrieve the last 30–60 seconds of a performance after inspiration strikes, with explicit memory/disk limits.
2. **Editable song scenes:** verse/hook/bridge arrangements with saved sound choices, chord inversions, bass patterns and drum grooves.
3. **Voice gesture mapping:** learn vowels, breath and consonant attacks as instrument controls, with per-user calibration and visible confidence.
4. **Better takes:** editable piano roll with uncertain notes highlighted; keep raw and corrected versions side by side.
5. **Own-voice workspace:** dataset management, consent records, training jobs, A/B listening and versioned voice models. Requires choosing an engine and testing quality/hardware/cost.
6. **DAW connection checks:** a bidirectional companion handshake proving that a test note reached the expected destination, with templates for supported DAWs. A plugin cannot infer this from its outgoing MIDI counter.

Prioritize a reliable path from voice → audible arrangement → editable take. These ideas are product directions, not claims that competing products lack them.

## Primary references checked

- Spotify Basic Pitch: https://github.com/spotify/basic-pitch
- Inference function: https://github.com/spotify/basic-pitch/blob/main/basic_pitch/inference.py
- Kits conversion API: https://docs.kits.ai/api-reference/api-endpoints/voice-conversion-api/create-new-voice-conversion-job
- Kits result/status: https://docs.kits.ai/api-reference/types-limits/inference-job
- Kits voice models: https://docs.kits.ai/api-reference/api-endpoints/voice-model-api/fetch-voice-models
- FL MIDI scripting: https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/midi_scripting.htm
