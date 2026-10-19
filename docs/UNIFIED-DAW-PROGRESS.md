# Unified DAW progress

The first integration step adds the existing vocal rack's de-esser, compressor,
and saturation to audio tracks inside the V14 timeline.

Select an audio track, stop playback, and choose **VOCAL FX** beside the master
volume controls. Off is the default; Gentle uses light compression and de-essing,
Warm adds saturation, and Forward uses stronger compression and de-essing.
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
