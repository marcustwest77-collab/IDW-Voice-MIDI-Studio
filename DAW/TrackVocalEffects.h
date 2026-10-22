#pragma once
#include "../Source/VocalEffects.h"

namespace idw::daw {
// Shared playback/export insert. These presets intentionally use only zero-delay
// modules: no pitch latency, tail truncation or stereo-to-mono summing.
class TrackVocalEffects {
public:
    static bool validPreset(int preset) { return preset >= 0 && preset <= 3; }
    void reset() { left.reset(); right.reset(); }
    StereoSample process(float l, float r, int preset, float mix=1.0f) {
        if (preset == 0 || !validPreset(preset)) return {l, r};
        VocalEffectsSettings settings;
        settings.enabled = true;
        settings.mix = std::isfinite(mix) ? std::clamp(mix,0.0f,1.0f) : 0.0f;
        settings.deEsserEnabled = true;
        settings.compressorEnabled = true;
        settings.deEsser = preset == 3 ? 0.60f : 0.35f;
        settings.compressor = preset == 1 ? 0.20f : 0.45f;
        settings.saturationEnabled = preset == 2;
        settings.saturation = 0.15f;
        // Separate state preserves stereo input; no delayed modules need buffers.
        return {left.process(l, settings).left, right.process(r, settings).right};
    }
private:
    VocalEffects left, right;
};
}
