#include "YinPitchDetector.h"
#include <cmath>
void YinPitchDetector::prepare(double s, int) {
    sr = s;
    minTau = juce::jmax(2, (int) (sr / 1000.0));
    maxTau = (int) std::ceil(sr / 65.0);
    window = maxTau; // two periods at the lowest supported pitch
    ring.assign((size_t) (window + maxTau + 2), 0);
    frame.resize(ring.size()); diff.resize((size_t) maxTau + 2); cmnd.resize(diff.size());
    reset();
}
void YinPitchDetector::reset() {
    std::fill(ring.begin(), ring.end(), 0.0f); pos = filled = 0; conf = level = 0;
}
float YinPitchDetector::process(const float* in, int n) {
    if (!in || ring.empty() || n <= 0) return 0;
    double energy = 0;
    for (int i = 0; i < n; ++i) {
        const float x = std::isfinite(in[i]) ? in[i] : 0.0f;
        ring[(size_t) pos] = x; pos = (pos + 1) % (int) ring.size(); energy += x*x;
    }
    level = (float) std::sqrt(energy / n);
    filled = juce::jmin((int) ring.size(), filled + n);
    if (filled < (int) ring.size() || level < 0.0001f) { conf = 0; return 0; }
    for (size_t i = 0; i < frame.size(); ++i) frame[i] = ring[((size_t) pos + i) % ring.size()];
    return detect();
}
float YinPitchDetector::detect() {
    double running = 0;
    cmnd[0] = 1;
    for (int tau = 1; tau <= maxTau + 1; ++tau) {
        // Float accumulation vectorises well; fixed upper frequency bounds avoid unused work.
        float sum = 0;
        for (int i = 0; i < window; ++i) {
            const float delta = frame[(size_t) i] - frame[(size_t) (i + tau)]; sum += delta * delta;
        }
        diff[(size_t) tau] = sum; running += sum;
        cmnd[(size_t) tau] = running > 1.0e-15 ? (float) (sum * tau / running) : 1.0f;
    }
    int selected = 0;
    for (int t = minTau; t <= maxTau; ++t) {
        if (cmnd[(size_t) t] < 0.15f) {
            while (t < maxTau && cmnd[(size_t) (t+1)] < cmnd[(size_t) t]) ++t;
            selected = t; break;
        }
    }
    if (!selected) { conf = 0; return 0; }
    conf = juce::jlimit(0.0f, 1.0f, 1.0f - cmnd[(size_t) selected]);
    const float a = cmnd[(size_t) selected-1], b = cmnd[(size_t) selected], c = cmnd[(size_t) selected+1];
    const float denom = a - 2*b + c;
    const float offset = std::abs(denom) > 1.0e-9f ? juce::jlimit(-0.5f, 0.5f, 0.5f*(a-c)/denom) : 0;
    return (float) (sr / (selected + offset));
}
