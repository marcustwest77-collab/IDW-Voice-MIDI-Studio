#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

IDWVoiceMIDIStudioAudioProcessor::IDWVoiceMIDIStudioAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      // Keep the V9 state tree identifier so existing sessions and presets load in V3.
      apvts(*this, nullptr, "IDW_V9", layout()),
      presets(apvts)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout IDWVoiceMIDIStudioAudioProcessor::layout()
{
    using F = juce::AudioParameterFloat;
    using I = juce::AudioParameterInt;
    using B = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<F>("gate", "Gate", .001f, .2f, .008f));
    p.add(std::make_unique<F>("confidence", "Confidence", .5f, 1.f, .75f));
    p.add(std::make_unique<I>("bend", "Bend", 1, 24, 2));
    p.add(std::make_unique<F>("tuneCents", "Pitch Calibration", -100.f, 100.f, 0.f));
    p.add(std::make_unique<B>("scaleLock", "Scale Lock", false));
    p.add(std::make_unique<I>("root", "Root", 0, 11, 0));
    p.add(std::make_unique<I>("scaleMask", "Scale Mask", 1, 4095, 2741));

    p.add(std::make_unique<B>("beatbox", "Beatbox", true));
    p.add(std::make_unique<F>("beatThreshold", "Beat Threshold", .005f, .2f, .035f));
    p.add(std::make_unique<I>("kick", "Kick", 0, 127, 36));
    p.add(std::make_unique<I>("snare", "Snare", 0, 127, 38));
    p.add(std::make_unique<I>("hat", "Hat", 0, 127, 42));

    p.add(std::make_unique<B>("gestureCC", "Gesture CC", true));
    p.add(std::make_unique<I>("cc", "CC", 0, 127, 74));
    p.add(std::make_unique<F>("ccSense", "CC Sense", .25f, 3.f, 1.f));
    p.add(std::make_unique<B>("mpe", "MPE", false));
    p.add(std::make_unique<I>("mpeFirst", "MPE First", 2, 16, 2));
    p.add(std::make_unique<I>("mpeLast", "MPE Last", 2, 16, 16));
    p.add(std::make_unique<I>("latencyMs", "Latency", 0, 100, 0));
    return p;
}

void IDWVoiceMIDIStudioAudioProcessor::prepareToPlay(double sampleRate, int blockSize)
{
    pitch.prepare(sampleRate, blockSize);
    beats.prepare(sampleRate);
    mpe.reset();
    active = -1;
    candidate = -1;
    candidateFrames = 0;
    silentSamples = 0;
    ccFrameCounter = 0;
    lastHz = 0.0f;
    smoothedRawNote = -1.0f;
}

void IDWVoiceMIDIStudioAudioProcessor::releaseResources()
{
    pitch.reset();
}

bool IDWVoiceMIDIStudioAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto input = layout.getMainInputChannelSet();
    const auto output = layout.getMainOutputChannelSet();
    return input == juce::AudioChannelSet::mono()
        && (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo());
}

void IDWVoiceMIDIStudioAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiOut)
{
    juce::MidiBuffer incoming = midiOut;
    learn.process(incoming, apvts);
    midiOut.clear();

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() == 0 || numSamples == 0)
        return;

    const auto* input = buffer.getReadPointer(0);
    const float detectedHz = pitch.process(input, numSamples);
    const float rms = pitch.rms();
    const float quality = pitch.confidence();

    freq = detectedHz;
    confidence = quality;
    inputRms = rms;

    scale.setMask((uint16_t) apvts.getRawParameterValue("scaleMask")->load());
    mpe.setZone((int) apvts.getRawParameterValue("mpeFirst")->load(),
                (int) apvts.getRawParameterValue("mpeLast")->load());

    const bool mpeOn = apvts.getRawParameterValue("mpe")->load() > .5f;
    const float gate = apvts.getRawParameterValue("gate")->load();
    const float minConfidence = apvts.getRawParameterValue("confidence")->load();

    if (detectedHz > 0.0f && rms >= gate && quality >= minConfidence)
    {
        const float calibrationCents = apvts.getRawParameterValue("tuneCents")->load();
        const float measuredNote = 69.0f + 12.0f * std::log2(detectedHz / 440.0f)
                                   + calibrationCents / 100.0f;

        // Smooth in musical (semitone) space. This is intentionally light so
        // vibrato and slides remain expressive while octave glitches are tamed.
        if (smoothedRawNote < 0.0f || std::abs(measuredNote - smoothedRawNote) > 7.0f)
            smoothedRawNote = measuredNote;
        else
            smoothedRawNote += 0.35f * (measuredNote - smoothedRawNote);

        const float rawNote = smoothedRawNote;
        int noteNumber = (int) std::lround(rawNote);

        if (apvts.getRawParameterValue("scaleLock")->load() > .5f)
            noteNumber = scale.quantize(noteNumber, (int) apvts.getRawParameterValue("root")->load());

        noteNumber = juce::jlimit(0, 127, noteNumber);

        // Require a new note to win two consecutive analysis frames. The first
        // note still starts immediately, keeping live response fast.
        if (noteNumber != active)
        {
            if (active >= 0)
            {
                if (candidate != noteNumber)
                {
                    candidate = noteNumber;
                    candidateFrames = 1;
                }
                else
                    ++candidateFrames;

                if (candidateFrames < 2)
                    noteNumber = active;
            }

            if (noteNumber != active)
            {
                if (active >= 0)
                {
                    const int oldChannel = mpeOn ? mpe.channelFor(active) : 1;
                    midiOut.addEvent(juce::MidiMessage::pitchWheel(oldChannel, 8192), 0);
                    midiOut.addEvent(juce::MidiMessage::noteOff(oldChannel, active), 0);
                    mpe.release(active);
                }

                const int newChannel = mpeOn ? mpe.allocate(noteNumber) : 1;
                const float velocityNorm = juce::jlimit(0.0f, 1.0f, (rms - gate) / juce::jmax(0.001f, 0.2f - gate));
                const int velocity = juce::jlimit(1, 127, (int) std::lround(35.0f + velocityNorm * 92.0f));
                midiOut.addEvent(juce::MidiMessage::noteOn(newChannel, noteNumber, (juce::uint8) velocity), 0);
                active = noteNumber;
                candidate = -1;
                candidateFrames = 0;
            }
        }

        midi = noteNumber;
        silentSamples = 0;

        const int channel = mpeOn ? mpe.channelFor(noteNumber) : 1;
        const int bendRange = juce::jmax(1, (int) apvts.getRawParameterValue("bend")->load());
        const float semitoneOffset = juce::jlimit(-(float) bendRange, (float) bendRange, rawNote - (float) noteNumber);
        const int wheel = juce::jlimit(0, 16383,
                                      (int) std::lround(8192.0 + (semitoneOffset / (float) bendRange) * 8192.0));
        midiOut.addEvent(juce::MidiMessage::pitchWheel(channel, wheel), 0);

        // Limit gesture CC to roughly every other frame to reduce MIDI traffic.
        if (apvts.getRawParameterValue("gestureCC")->load() > .5f && lastHz > 0.0f
            && (++ccFrameCounter % 2) == 0)
        {
            const float movement = std::abs(12.0f * std::log2(detectedHz / lastHz));
            const int ccValue = juce::jlimit(0, 127,
                                             (int) (movement * 64.0f * apvts.getRawParameterValue("ccSense")->load()));
            midiOut.addEvent(juce::MidiMessage::controllerEvent(
                                 channel,
                                 (int) apvts.getRawParameterValue("cc")->load(),
                                 ccValue),
                             0);
        }

        lastHz = detectedHz;
    }
    else
    {
        silentSamples += numSamples;
        const int releaseSamples = juce::jmax(1, (int) std::lround(getSampleRate() * 0.075));
        if (silentSamples >= releaseSamples && active >= 0)
        {
            const int channel = mpeOn ? mpe.channelFor(active) : 1;
            midiOut.addEvent(juce::MidiMessage::pitchWheel(channel, 8192), 0);
            midiOut.addEvent(juce::MidiMessage::noteOff(channel, active), 0);
            mpe.release(active);
            active = -1;
            midi = -1;
            lastHz = 0.0f;
            smoothedRawNote = -1.0f;
            candidate = -1;
            candidateFrames = 0;
        }
    }

    const auto beat = apvts.getRawParameterValue("beatbox")->load() > .5f
                        ? beats.process(input, numSamples, apvts.getRawParameterValue("beatThreshold")->load())
                        : BeatboxClassifier::None;

    if (beat)
    {
        const char* id = beat == BeatboxClassifier::Kick ? "kick"
                        : beat == BeatboxClassifier::Snare ? "snare"
                                                          : "hat";
        const int drumNote = (int) apvts.getRawParameterValue(id)->load();
        midiOut.addEvent(juce::MidiMessage::noteOn(10, drumNote, (juce::uint8) 115), 0);
        midiOut.addEvent(juce::MidiMessage::noteOff(10, drumNote), juce::jmin(numSamples - 1, 10));
    }

    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
        buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor* IDWVoiceMIDIStudioAudioProcessor::createEditor()
{
    return new IDWVoiceMIDIStudioAudioProcessorEditor(*this);
}

void IDWVoiceMIDIStudioAudioProcessor::getStateInformation(juce::MemoryBlock& data)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, data);
}

void IDWVoiceMIDIStudioAudioProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IDWVoiceMIDIStudioAudioProcessor();
}
