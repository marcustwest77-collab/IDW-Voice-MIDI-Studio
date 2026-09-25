#pragma once
#include <JuceHeader.h>
#include <array>
// Local spectral templates. No neural model, downloads, or cloud audio processing.
class BeatboxClassifier {
public:
    struct Hit { int pad=-1; int velocity=0; };
    BeatboxClassifier();
    void prepare(double);
    Hit process(const float*,int,float);
    void train(int pad) { command.store(juce::jlimit(0,7,pad)); }
    void cancel() { command.store(-2); }
    void clearModels() { command.store(-3); }
    int trainingPad() const { return training.load(); }
    int examplesRemaining() const { return remaining.load(); }
    int examples(int pad) const { return counts[(size_t)pad].load(); }
    juce::ValueTree save() const;
    void restore(const juce::ValueTree&);
private:
    using Features=std::array<float,8>;
    Features features() const;
    std::array<std::array<std::atomic<float>,8>,8> models;
    std::array<std::atomic<int>,8> counts;
    std::vector<float> ring;
    mutable std::array<float,2048> fftData{};
    mutable juce::dsp::FFT fft{10};
    std::atomic<int> command{-1},training{-1},remaining{0};
    Features trainingSum{};
    int trainingCount=0,pos=0,cooldown=0,pending=0;
    float previous=0,peak=0;
    double sr=48000;
};
