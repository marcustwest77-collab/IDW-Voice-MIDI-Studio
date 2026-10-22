#include "ReleaseSafety.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace { void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); } }

int main() {
    using namespace idw;
    try {
        for (float sample : {-0.94f, -0.5f, 0.0f, 0.5f, 0.94f})
            check(guardedOutput(sample, true) == sample, "Clip Guard changed audio below its knee");
        for (float sample : {-8.0f, -1.4f, 1.4f, 8.0f}) {
            const float guarded = guardedOutput(sample, true);
            check(std::isfinite(guarded) && std::abs(guarded) <= 0.9991f, "Clip Guard exceeded its ceiling");
            check((guarded < 0) == (sample < 0), "Clip Guard changed polarity");
        }
        check(guardedOutput(1.4f, false) == 1.4f, "Clip Guard bypass changed audio");
        check(guardedOutput(INFINITY, true) == 0.0f, "Clip Guard accepted non-finite audio");

        ConfidenceGate gate; gate.prepare(48000.0);
        float opened = 0.0f; for (int i = 0; i < 4800; ++i) opened = gate.process(true);
        check(opened > 0.98f, "Harmony confidence gate did not open");
        float released = opened; for (int i = 0; i < 24000; ++i) released = gate.process(false);
        check(released < 0.01f, "Harmony confidence gate did not close");

        const auto eco = qualityPlan(0), studio = qualityPlan(1), high = qualityPlan(2);
        check(!eco.formantEnabled && eco.maximumHarmonyVoices == 1, "Eco quality plan is not lightweight");
        check(studio.formantEnabled && studio.maximumHarmonyVoices == 2, "Studio quality plan is incomplete");
        check(high.formantAnalysisHop < studio.formantAnalysisHop, "High quality does not increase formant analysis rate");
        std::cout << "PASS V10.5 Clip Guard / harmony confidence gate / CPU quality plans\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
}
