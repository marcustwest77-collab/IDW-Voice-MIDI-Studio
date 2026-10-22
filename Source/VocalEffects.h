#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace idw {

struct VocalEffectsSettings {
    bool enabled = false;
    bool deEsserEnabled = false;
    bool compressorEnabled = false;
    bool saturationEnabled = false;
    bool doublerEnabled = false;
    bool reverbEnabled = false;
    bool delayEnabled = false;
    bool autoGain = false;
    float deEsser = 0.45f;
    float compressor = 0.45f;
    float saturation = 0.20f;
    float doubler = 0.20f;
    float reverb = 0.15f;
    float delay = 0.12f;
    float delaySeconds = 0.375f;
    float mix = 1.0f;
};

struct StereoSample { float left = 0.0f, right = 0.0f; };

// A deliberately small, causal vocal chain for live tracking. Every module has a
// true bypass path and the complete rack defaults off so older sessions are unchanged.
class VocalEffects {
public:
    void prepare(double newRate) {
        sampleRate = std::max(8000.0, newRate);
        doublerLine.assign((std::size_t)std::ceil(sampleRate * 0.060), 0.0f);
        delayLine.assign((std::size_t)std::ceil(sampleRate * 3.100), 0.0f);
        reverbLeft.assign((std::size_t)std::ceil(sampleRate * 0.089), 0.0f);
        reverbRight.assign((std::size_t)std::ceil(sampleRate * 0.097), 0.0f);
        reset();
    }

    void reset() {
        std::fill(doublerLine.begin(), doublerLine.end(), 0.0f);
        std::fill(delayLine.begin(), delayLine.end(), 0.0f);
        std::fill(reverbLeft.begin(), reverbLeft.end(), 0.0f);
        std::fill(reverbRight.begin(), reverbRight.end(), 0.0f);
        doublerWrite = delayWrite = reverbLeftWrite = reverbRightWrite = 0;
        previousInput = deEsserEnvelope = compressorEnvelope = 0.0f;
        compressorGain = matchedGain = 1.0f;dryEnvelope=wetEnvelope=0.0f;
    }

    StereoSample process(float input, const VocalEffectsSettings& s) {
        const bool anyModule=s.deEsserEnabled||s.compressorEnabled||s.saturationEnabled||s.doublerEnabled||s.reverbEnabled||s.delayEnabled;
        if (!s.enabled||!anyModule||s.mix<=0.0f) return {input, input};
        const float dry = input;
        float mono = input;

        if (s.deEsserEnabled) mono = processDeEsser(mono, clamp01(s.deEsser));
        else previousInput = mono;
        if (s.compressorEnabled) mono = processCompressor(mono, clamp01(s.compressor));
        if (s.saturationEnabled) mono = processSaturation(mono, clamp01(s.saturation));

        float left = mono, right = mono;
        if (s.doublerEnabled && !doublerLine.empty()) {
            const float amount = clamp01(s.doubler);
            const auto leftTap = tap(doublerLine, doublerWrite, secondsToSamples(0.017));
            const auto rightTap = tap(doublerLine, doublerWrite, secondsToSamples(0.023));
            doublerLine[doublerWrite] = mono;
            advance(doublerWrite, doublerLine);
            left += leftTap * amount * 0.55f;
            right += rightTap * amount * 0.55f;
        }

        if (s.delayEnabled && !delayLine.empty()) {
            const float amount = clamp01(s.delay);
            const float echo = tap(delayLine, delayWrite, secondsToSamples(std::clamp(s.delaySeconds,0.04f,3.0f)));
            delayLine[delayWrite] = mono + echo * (0.18f + 0.32f * amount);
            advance(delayWrite, delayLine);
            left += echo * amount * 0.45f;
            right += echo * amount * 0.36f;
        }

        if (s.reverbEnabled && !reverbLeft.empty() && !reverbRight.empty()) {
            const float amount = clamp01(s.reverb);
            const float wetL = reverbLeft[reverbLeftWrite];
            const float wetR = reverbRight[reverbRightWrite];
            reverbLeft[reverbLeftWrite] = mono + (wetL * 0.42f + wetR * 0.16f) * (0.65f + 0.25f * amount);
            reverbRight[reverbRightWrite] = mono + (wetR * 0.39f + wetL * 0.19f) * (0.65f + 0.25f * amount);
            advance(reverbLeftWrite, reverbLeft); advance(reverbRightWrite, reverbRight);
            left += wetL * amount * 0.32f;
            right += wetR * amount * 0.32f;
        }

        if(s.autoGain){
            const float coefficient=static_cast<float>(1.0-std::exp(-1.0/(sampleRate*0.180)));
            dryEnvelope+=coefficient*(std::abs(dry)-dryEnvelope);wetEnvelope+=coefficient*((std::abs(left)+std::abs(right))*.5f-wetEnvelope);
            const float target=dryEnvelope>.001f&&wetEnvelope>.001f?std::clamp(dryEnvelope/wetEnvelope,.5f,2.0f):1.0f;
            matchedGain+=(target-matchedGain)*coefficient*.35f;left*=matchedGain;right*=matchedGain;
        }
        const float mix = clamp01(s.mix);
        return {dry + (softLimit(left) - dry) * mix, dry + (softLimit(right) - dry) * mix};
    }

private:
    static float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
    static float softLimit(float value) { return std::tanh(value); }
    int secondsToSamples(double seconds) const { return std::max(1, (int)std::lround(sampleRate * seconds)); }
    static void advance(std::size_t& index, const std::vector<float>& line) { if (++index >= line.size()) index = 0; }
    static float tap(const std::vector<float>& line, std::size_t write, int delay) {
        if (line.empty()) return 0.0f;
        const auto offset = (std::size_t)std::clamp(delay, 1, (int)line.size() - 1);
        return line[(write + line.size() - offset) % line.size()];
    }
    float processDeEsser(float input, float amount) {
        const float high = input - previousInput; previousInput = input;
        const float coefficient = std::abs(high) > deEsserEnvelope ? 0.16f : 0.012f;
        deEsserEnvelope += coefficient * (std::abs(high) - deEsserEnvelope);
        const float threshold = 0.015f + (1.0f - amount) * 0.055f;
        const float reduction = amount * std::clamp((deEsserEnvelope - threshold) / std::max(0.001f, threshold * 3.0f), 0.0f, 0.72f);
        return input - high * reduction;
    }
    float processCompressor(float input, float amount) {
        const float level = std::abs(input);
        const float coefficient = level > compressorEnvelope ? 0.025f : 0.0015f;
        compressorEnvelope += coefficient * (level - compressorEnvelope);
        const float threshold = std::pow(10.0f, (-10.0f - amount * 22.0f) / 20.0f);
        const float ratio = 1.5f + amount * 6.5f;
        float target = 1.0f;
        if (compressorEnvelope > threshold)
            target = std::pow(threshold / std::max(threshold, compressorEnvelope), 1.0f - 1.0f / ratio);
        compressorGain += (target - compressorGain) * (target < compressorGain ? 0.035f : 0.0012f);
        const float makeup = 1.0f + amount * 0.65f;
        return input * compressorGain * makeup;
    }
    static float processSaturation(float input, float amount) {
        const float drive = 1.0f + amount * 7.0f;
        const float shaped = std::tanh(input * drive) / std::tanh(drive);
        return input + (shaped - input) * amount;
    }

    double sampleRate = 48000.0;
    std::vector<float> doublerLine, delayLine, reverbLeft, reverbRight;
    std::size_t doublerWrite = 0, delayWrite = 0, reverbLeftWrite = 0, reverbRightWrite = 0;
    float previousInput = 0.0f, deEsserEnvelope = 0.0f;
    float compressorEnvelope = 0.0f, compressorGain = 1.0f;
    float dryEnvelope=0.0f,wetEnvelope=0.0f,matchedGain=1.0f;
};

} // namespace idw
