# V13 FL Studio Acceptance Test — Windows

This is a **manual hardware/host acceptance test**, not a claim of FL Studio certification.
Use the latest successful build artifact from the `v13-midi-safety` workflow.
Back up the previously installed VST3 and save your FL Studio project before testing.

## Test A — plugin and audio path
1. Open FL Studio, open **Options > Manage plugins**, rescan, and confirm one IDW Voice MIDI Studio entry.
2. Select the correct ASIO interface and microphone input. Route the microphone to a dedicated Mixer insert.
3. Load IDW Voice MIDI Studio as an effect on that insert.
4. Turn on **Preview sound**; press **Test note**. Confirm an audible C4. Switch preview off.
5. In a silent room select **Calibrate noise**, then sing one steady note. Confirm pitch, level, and note indicators respond.
6. Sing softly and loudly. Verify gate/confidence behavior, absence of clipping, and acceptable latency.

## Test B — VST3 output into FL Studio
1. Set **MIDI output port 10** on the IDW plugin wrapper.
2. Set **MIDI input port 10** on the intended receiving instrument wrapper.
3. Disable preview and enable the receiving instrument's audio output.
4. Press **Test note** in IDW. Confirm the receiving instrument sounds C4.
5. Sing single notes, glides, and silence. Verify note starts, ends, and pitch bend.
6. Set the receiving instrument's bend range equal to IDW's configured bend range (default 2 semitones).
7. Record a short take, stop transport, play back, export a MIDI file and inspect note lengths and channel assignment.
8. Press PANIC during a held note. Verify silence and no stuck notes.

## Test C — separate standalone-to-FL router (optional)
Use this only when an existing virtual MIDI port is already installed and connected.
1. Place four unique Channel Rack instruments named exactly **IDW Lead**, **IDW Chords**, **IDW Bass**, **IDW Drums**.
2. Attach the opt-in IDW Performance controller script to the MIDI input carrying standalone output.
3. Check channel 1 -> Lead, channel 2 -> Chords, channel 3 -> Bass, channel 10 -> Drums.
4. Hold a lead note and a drum note. Send CC123 on drum channel 10; only the drum should stop.
5. Confirm a following lead Note Off stops the lead. Test global panic and restart.
6. Rename a target temporarily; confirm routing refuses duplicates or missing targets.
7. Save the project, restart FL Studio, and repeat the test.

**Important:** MIDI port 10 is not MIDI channel 10. The opt-in controller router does not automatically reroute the VST3 plugin's internal output; it is intended for standalone MIDI input.

## Evidence to record
- Windows version / FL Studio version / audio interface and ASIO driver
- Build artifact name, GitHub run ID, commit SHA
- Sample rate, buffer size, input channel, output/input port pair
- Each test PASS/FAIL, screenshots, logs, and stuck-note reproduction steps

## Release gate
Do not promote this branch to a general V13 installer until CI, plugin validation,
a real microphone, FL Studio, and both panic/routing acceptance tests have passed.
