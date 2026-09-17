#pragma once
#include <JuceHeader.h>

class BeatboxClassifier
{
public:
    enum Kind { None, Kick, Snare, Hat };

    void prepare(double sampleRateToUse);
    Kind process(const float*, int, float);

private:
    double sampleRate = 44100.0;
    float envelope = 0.0f;
    int cooldownSamples = 0;
};
