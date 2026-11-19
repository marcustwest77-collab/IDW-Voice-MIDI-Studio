# Experimental effect crackle diagnostic

Run the separate IDWVst3EffectProbe with one installed VST3 path and an optional
new output-folder path. With no folder, the earlier synthetic smoke check remains.
With a new folder, it creates 5-second synthetic 227 Hz stereo renders at block
sizes 32, 512 and 1024, plus Report.json. Existing destinations are refused.

Depth/Speed parameters are set to normalized maximum only when their full names
match; Report.json lists which were found. No personal vocal is read. This does
not write plugin state, install plugins or change the DAW/audio-device setup.

Metrics include peak, samples beyond unity, nonfinite samples, maximum adjacent
sample change at block boundaries/interiors, processing duration and blocks
exceeding their nominal real-time deadline. Offline timing is not a measured
device xrun; a boundary jump alone is not proof of crackle. Successful renders
need inspection/listening and comparison to real-time behavior.

Motivation: #263 user reported mild crackle with MAutoPitch Depth/Speed at maximum;
bypass clean. Initial AudioBox buffer was 32 samples; after changing to 512 and
reloading once the device settled, active-effect crackle persisted. Cause remains
unestablished; do not blame the microphone or claim this diagnostic fixes it.

Local strict metric tests pass; probe translation unit compile checked. Linked
build, real-plugin diagnostic execution and listening remain pending CI artifact.
