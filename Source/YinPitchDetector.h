#pragma once
#include <JuceHeader.h>
// The processor feeds fixed 5 ms hops; no allocation occurs after prepare().
class YinPitchDetector {
public:
    void prepare(double sampleRate, int);
    void reset();
    float process(const float*, int);
    float confidence() const { return conf; }
    float rms() const { return level; }
private:
    double sr = 48000;
    std::vector<float> ring, frame, diff, cmnd;
    int pos = 0, filled = 0, minTau = 0, maxTau = 0, window = 0;
    float conf = 0, level = 0;
    float detect();
};
