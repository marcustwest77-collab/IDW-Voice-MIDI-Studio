# Unified DAW progress

The first integration step adds the existing vocal rack's de-esser, compressor,
and saturation to audio tracks inside the V14 timeline.

Select an audio track, stop playback, and choose **VOCAL FX** beside the master
volume controls. Off is the default; Gentle uses light compression and de-essing,
Warm adds saturation, and Forward uses stronger compression and de-essing.
Use the adjacent **% FX** slider to blend the original take (0%) with the full
preset (100%). The amount is saved per track; switching Off retains it.
The insert runs before track volume/pan in playback and WAV export. Settings
save in `.idwproj` files and portable project copies. Original recordings and
live input monitoring remain dry. MIDI tracks do not use this insert.

This is the first subset of the existing rack, not the full Tune Studio:
pitch correction, harmony, reverb/delay, and Song Studio integration are still
pending. Effects are set while stopped, rather than automated during playback.
Old projects open with effects off. Older builds ignore the new preset field;
saving in an older build will discard the preset. Use the newer build to hear
and retain these inserts. The current playback UI supports one audio clip per
track; multi-clip track chains remain future work.

Verification includes exact stereo bypass, stereo channel isolation, deterministic
restart, preset validation, project round trips, legacy defaults, and rendered
WAV comparison against the same processor used by playback. Hardware listening
and Windows UI checks are still required after a successful native build.

## Lyrics notebook

The LYRICS button in the top header opens a notebook inside V14. Paste or type
hooks, verses and song notes; it stays available during playback and recording.
Use SAVE PROJECT while stopped to save the words. Portable projects carry them
along with the session. Opening a different project replaces the notebook with
that project's words; projects without lyrics open with a blank notebook.

This is a manually edited notebook, not automatic transcription, timed lyrics,
or import of the separate Song Studio project format. It allows 32,768
characters. Older V14 builds ignore this field and discard it on re-save.

## Built-in MIDI instrument WAV export

BOUNCE WAV now includes piano-roll notes with the same built-in Keys, Bass,
Lead, Organ and Pad instruments used in playback. MIDI-only projects can export,
and notes after the last audio clip extend the export. Track volume, pan,
mute/solo and the master tone/volume/ceiling apply. Muted material still retains
its timeline duration, matching the existing audio export behavior.

This renders the built-in instruments; it does not capture external MIDI
hardware or host third-party instrument plugins.
