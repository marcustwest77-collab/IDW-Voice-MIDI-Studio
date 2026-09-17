#include "BeatboxClassifier.h"
#include <cmath>

void BeatboxClassifier::prepare(double sampleRateToUse)
{
    sampleRate = sampleRateToUse > 0.0 ? sampleRateToUse : 44100.0;
    envelope = 0.0f;
    cooldownSamples = 0;
}

BeatboxClassifier::Kind BeatboxClassifier::process(const float* input, int numSamples, float threshold)
{
    if (input == nullptr || numSamples < 8)
        return None;

    double energy = 0.0;
    double differenceEnergy = 0.0;
    int zeroCrossings = 0;
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sample = input[i];
        energy += (double) sample * (double) sample;
        peak = juce::jmax(peak, std::abs(sample));

        if (i > 0)
        {
            const float difference = sample - input[i - 1];
            differenceEnergy += (double) difference * (double) difference;
            if ((sample >= 0.0f) != (input[i - 1] >= 0.0f))
                ++zeroCrossings;
        }
    }

    const float rms = (float) std::sqrt(energy / (double) numSamples);
    const float previousEnvelope = envelope;
    envelope = 0.94f * envelope + 0.06f * rms;
    const float onsetRise = rms - previousEnvelope;

    cooldownSamples = juce::jmax(0, cooldownSamples - numSamples);
    if (cooldownSamples > 0 || onsetRise < threshold)
        return None;

    const float zcr = (float) zeroCrossings / (float) juce::jmax(1, numSamples - 1);
    const float brightness = (float) std::sqrt(differenceEnergy / juce::jmax(1.0e-12, energy));
    const float crest = peak / juce::jmax(1.0e-6f, rms);

    cooldownSamples = juce::jmax(1, (int) std::lround(sampleRate * 0.055));

    if (zcr < 0.08f && brightness < 0.75f)
        return Kick;

    if (zcr > 0.20f || brightness > 1.05f || crest > 7.0f)
        return Hat;

    return Snare;
}
