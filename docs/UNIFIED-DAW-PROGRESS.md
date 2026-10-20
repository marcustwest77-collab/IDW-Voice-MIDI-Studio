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

## Consistent MIDI levels across audio buffers

Polyphony normalization now counts notes at each sample. Previously, every note
that touched a callback buffer affected the volume of that entire buffer, so
staggered notes could sound quieter in a large-buffer WAV export than in playback.
The correction preserves the existing audio mix and uses fixed stack buffers.
Regression checks cover staggered notes, fractional start times, five instruments,
stereo pan, mute, and buffer sizes 1, 64, 127, 256, 1024 and 4096.

## Master mix project recall

SAVE PROJECT and EXPORT PORTABLE now retain master volume, bass/treble tone,
and output ceiling. Opening a project restores both the controls and the audio
settings. WAV export takes the complete mix from one session snapshot.

Older projects have no saved master values, so they open at unity volume,
flat tone, and a -1 dB ceiling instead of inheriting the previous song's mix.
Older app builds ignore the new fields and discard them if the project is
re-saved there. This stores mix settings, not automation over time.

## Ten factory skins in the V14 workspace

The skin dropdown now includes the six Voice MIDI Studio palettes: IDW Gold,
Midnight Studio, Electric Blue, Vocal Heat, Platinum, and High Contrast, alongside
Neo Cyan, Royal Gold, Emerald Circuit, and Purple Night. These adapt the original
palette colors to V14's existing layout; they do not change recording or routing.
The four original preset IDs remain stable. New selections persist across app
restarts and work with V14's custom skin save/load/import/export tools.

Older V14 builds do not understand the six added factory IDs. Custom skin files
from the separate Voice MIDI Studio app still use a different format; this update
brings over its factory palettes, not its custom-skin file importer.

## Identifying the installed desktop beta

Use BUILD INFO in the top header to see the app version and full source revision.
That revision distinguishes updates which still share the V14 version number.
It is embedded when the app is configured for compilation, including CI merge
builds. Source archives without Git metadata display an unknown revision, and
tracked local changes are marked. Reconfigure local CMake builds after changing
revisions. No network connection or diagnostic upload is involved.

### Docked desktop layout

Vocal FX and lyrics now sit beside the arrangement, with the existing mixer below. Arrangement painting and pointer selection stop at the dock boundary. Lyrics open by default; timeline shortcuts are suppressed only while the notebook has keyboard focus. Expanding the mixer also resizes the notebook. Master controls now fit above the arrangement, and voice commands and peak meters sit in the footer. This is the first structural pass toward the approved concept, not a claim of complete visual parity. Native compilation and Windows visual review remain pending.
