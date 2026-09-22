#include "PluginEditor.h"

IDWVoiceMIDIStudioAudioProcessorEditor::IDWVoiceMIDIStudioAudioProcessorEditor(IDWVoiceMIDIStudioAudioProcessor& processor)
    : AudioProcessorEditor(&processor), p(processor)
{
    setSize(1080, 760);

    title.setText("IDW VOICE MIDI STUDIO • VERSION 3", juce::dontSendNotification);
    title.setFont(juce::Font(32.0f, juce::Font::bold));
    title.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(title);

    readout.setFont(juce::Font(36.0f, juce::Font::bold));
    readout.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(readout);

    status.setJustificationType(juce::Justification::centred);
    status.setText("Choose a preset, calibrate while quiet, then sing", juce::dontSendNotification);
    addAndMakeVisible(status);

    levelReadout.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(levelReadout);

    configureSlider(gateSlider, gateLabel, "NOISE GATE",
                    "Minimum vocal input level. Raise it in a noisy room; lower it for quiet or soft vocals.");
    configureSlider(confidenceSlider, confidenceLabel, "CONFIDENCE",
                    "How certain pitch detection must be before a MIDI note is accepted. Higher is stricter.");
    configureSlider(bendSlider, bendLabel, "BEND RANGE",
                    "Pitch-bend range in semitones. Set the receiving synth to the same bend range.");
    configureSlider(tuneSlider, tuneLabel, "PITCH CAL ±CENTS",
                    "Fine tuning offset in cents. Leave at 0 unless pitch needs calibration.");

    gateSlider.setRange(.001, .2, .001);
    gateSlider.setNumDecimalPlacesToDisplay(3);
    confidenceSlider.setRange(.5, 1.0, .01);
    confidenceSlider.setNumDecimalPlacesToDisplay(2);
    bendSlider.setRange(1, 24, 1);
    tuneSlider.setRange(-100, 100, 1);

    gateAttachment = std::make_unique<SliderAttachment>(p.apvts, "gate", gateSlider);
    confidenceAttachment = std::make_unique<SliderAttachment>(p.apvts, "confidence", confidenceSlider);
    bendAttachment = std::make_unique<SliderAttachment>(p.apvts, "bend", bendSlider);
    tuneAttachment = std::make_unique<SliderAttachment>(p.apvts, "tuneCents", tuneSlider);

    scaleLock.setTooltip("Quantize outgoing MIDI notes to the selected custom scale.");
    addAndMakeVisible(scaleLock);
    scaleLockAttachment = std::make_unique<ButtonAttachment>(p.apvts, "scaleLock", scaleLock);

    const char* roots[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        rootSelector.addItem(roots[i], i + 1);
    rootSelector.setSelectedId((int) p.apvts.getRawParameterValue("root")->load() + 1, juce::dontSendNotification);
    rootSelector.setTooltip("Root note used when Scale Lock is enabled.");
    addAndMakeVisible(rootSelector);
    rootLabel.setText("ROOT", juce::dontSendNotification);
    rootLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(rootLabel);
    rootAttachment = std::make_unique<ComboAttachment>(p.apvts, "root", rootSelector);

    for (int i = 0; i < 12; ++i)
    {
        scaleButtons[i].setButtonText(roots[i]);
        scaleButtons[i].setTooltip("Toggle this pitch class in the custom scale mask.");
        addAndMakeVisible(scaleButtons[i]);
        scaleButtons[i].onClick = [this, i]
        {
            p.scale.toggle(i);
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter("scaleMask")))
                ranged->setValueNotifyingHost(ranged->convertTo0to1((float) p.scale.getMask()));
            syncScale();
        };
    }

    auto destinations = p.learn.destinations();
    for (int i = 0; i < destinations.size(); ++i)
        learnTarget.addItem(destinations[i], i + 1);
    learnTarget.setSelectedId(1);
    learnTarget.setTooltip("Choose the IDW control you want to map before pressing MIDI Learn.");
    addAndMakeVisible(learnTarget);

    calibrate.setTooltip("Stay quiet, then press this to set the noise gate just above the room noise floor.");
    learn.setTooltip("Arm MIDI Learn, then move a hardware MIDI control to create the mapping.");
    save.setTooltip("Save the current settings as a user preset.");
    addAndMakeVisible(learn);
    addAndMakeVisible(save);
    addAndMakeVisible(calibrate);

    presetLabel.setText("PRESETS", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetLabel);
    presetSelector.setTooltip("Choose a built-in factory preset or one of your saved user presets.");
    loadPreset.setTooltip("Load the selected preset.");
    addAndMakeVisible(presetSelector);
    addAndMakeVisible(loadPreset);

    help.setTooltip("Open the built-in Quick Start, control guide, FL Studio routing and troubleshooting manual.");
    addAndMakeVisible(help);

    helpText.setMultiLine(true);
    helpText.setReadOnly(true);
    helpText.setScrollbarsShown(true);
    helpText.setCaretVisible(false);
    helpText.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(12, 12, 16));
    helpText.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    helpText.setColour(juce::TextEditor::outlineColourId, juce::Colours::gold);
    helpText.setFont(juce::Font(16.0f));
    helpText.setText(manualText(), false);
    addAndMakeVisible(helpText);
    addAndMakeVisible(closeHelp);

    learn.onClick = [this] { p.learn.arm(learnTarget.getText()); };
    save.onClick = [this]
    {
        const auto name = "IDW V3 " + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
        if (p.presets.save(name))
        {
            refreshPresets();
            status.setText("Saved user preset: " + name, juce::dontSendNotification);
        }
    };
    calibrate.onClick = [this] { calibrateNoiseGate(); };
    loadPreset.onClick = [this] { loadSelectedPreset(); };
    presetSelector.onChange = [this] { loadSelectedPreset(); };
    help.onClick = [this] { showHelp(true); };
    closeHelp.onClick = [this] { showHelp(false); };

    refreshPresets();
    syncScale();
    showHelp(false);
    startTimerHz(30);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::configureSlider(juce::Slider& slider,
                                                              juce::Label& label,
                                                              const juce::String& text,
                                                              const juce::String& tooltip)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setTooltip(tooltip);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::calibrateNoiseGate()
{
    calibratingNoise = true;
    calibrationTicks = 0;
    calibrationPeak = 0.0f;
    calibrate.setEnabled(false);
    status.setText("Measuring room noise for 1 second — stay quiet...", juce::dontSendNotification);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::finishNoiseCalibration()
{
    calibratingNoise = false;
    calibrate.setEnabled(true);
    const float threshold = juce::jlimit(.001f, .2f, juce::jmax(.002f, calibrationPeak * 2.5f));

    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter("gate")))
    {
        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost(ranged->convertTo0to1(threshold));
        ranged->endChangeGesture();
        status.setText("Noise gate calibrated to " + juce::String(threshold, 3), juce::dontSendNotification);
    }
}

void IDWVoiceMIDIStudioAudioProcessorEditor::refreshPresets()
{
    const int previousId = presetSelector.getSelectedId();
    presetSelector.clear(juce::dontSendNotification);

    const auto factory = p.presets.factoryPresetNames();
    factoryPresetCount = factory.size();
    for (int i = 0; i < factory.size(); ++i)
        presetSelector.addItem("FACTORY • " + factory[i], i + 1);

    userPresetNames = p.presets.list();
    if (! userPresetNames.isEmpty())
    {
        presetSelector.addSeparator();
        for (int i = 0; i < userPresetNames.size(); ++i)
            presetSelector.addItem("USER • " + userPresetNames[i], 1000 + i);
    }

    if (previousId > 0 && presetSelector.indexOfItemId(previousId) >= 0)
        presetSelector.setSelectedId(previousId, juce::dontSendNotification);
    else
        presetSelector.setSelectedId(1, juce::dontSendNotification);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::loadSelectedPreset()
{
    const int id = presetSelector.getSelectedId();
    bool loaded = false;
    juce::String displayName;

    if (id >= 1 && id <= factoryPresetCount)
    {
        const auto factory = p.presets.factoryPresetNames();
        displayName = factory[id - 1];
        loaded = p.presets.applyFactoryPreset(displayName);
    }
    else if (id >= 1000)
    {
        const int index = id - 1000;
        if (juce::isPositiveAndBelow(index, userPresetNames.size()))
        {
            displayName = userPresetNames[index];
            loaded = p.presets.load(displayName);
        }
    }

    if (loaded)
    {
        p.scale.setMask((uint16_t) p.apvts.getRawParameterValue("scaleMask")->load());
        syncScale();
        status.setText("Loaded preset: " + displayName, juce::dontSendNotification);
    }
}

void IDWVoiceMIDIStudioAudioProcessorEditor::showHelp(bool shouldShow)
{
    helpText.setVisible(shouldShow);
    closeHelp.setVisible(shouldShow);
    if (shouldShow)
    {
        helpText.toFront(false);
        closeHelp.toFront(false);
    }
}

void IDWVoiceMIDIStudioAudioProcessorEditor::syncScale()
{
    for (int i = 0; i < 12; ++i)
        scaleButtons[i].setToggleState(p.scale.enabled(i), juce::dontSendNotification);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(5, 5, 8));
    g.setColour(juce::Colours::gold);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(9), 20.0f, 2.0f);

    g.setColour(juce::Colours::gold.withAlpha(.18f));
    g.fillRoundedRectangle(45.0f, 230.0f, (float) getWidth() - 90.0f, 180.0f, 16.0f);

    g.setColour(juce::Colours::white.withAlpha(.55f));
    g.setFont(14.0f);
    g.drawFittedText("VOICE → MIDI • PRESETS • QUICK START • SCALE LOCK • MPE • BEATBOX • MIDI LEARN",
                     30, 714, getWidth() - 60, 24, juce::Justification::centred, 1);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::resized()
{
    title.setBounds(20, 20, getWidth() - 40, 48);
    readout.setBounds(20, 78, getWidth() - 40, 60);
    status.setBounds(20, 142, getWidth() - 40, 28);
    levelReadout.setBounds(20, 172, getWidth() - 40, 24);

    const int knobY = 245;
    const int knobW = 150;
    const int knobH = 145;
    const int gap = 20;
    const int startX = 150;

    gateLabel.setBounds(startX, 220, knobW, 24);
    gateSlider.setBounds(startX, knobY, knobW, knobH);
    confidenceLabel.setBounds(startX + (knobW + gap), 220, knobW, 24);
    confidenceSlider.setBounds(startX + (knobW + gap), knobY, knobW, knobH);
    bendLabel.setBounds(startX + 2 * (knobW + gap), 220, knobW, 24);
    bendSlider.setBounds(startX + 2 * (knobW + gap), knobY, knobW, knobH);
    tuneLabel.setBounds(startX + 3 * (knobW + gap), 220, knobW, 24);
    tuneSlider.setBounds(startX + 3 * (knobW + gap), knobY, knobW, knobH);

    calibrate.setBounds(50, 425, 150, 34);
    scaleLock.setBounds(220, 425, 125, 34);
    rootLabel.setBounds(355, 402, 80, 20);
    rootSelector.setBounds(355, 425, 80, 34);

    int x = 120;
    for (int i = 0; i < 12; ++i)
        scaleButtons[i].setBounds(x + i * 70, 480, 62, 36);

    presetLabel.setBounds(110, 532, 90, 20);
    presetSelector.setBounds(200, 526, 300, 34);
    loadPreset.setBounds(515, 526, 125, 34);
    save.setBounds(655, 526, 130, 34);
    help.setBounds(800, 526, 170, 34);

    learnTarget.setBounds(330, 590, 175, 34);
    learn.setBounds(520, 590, 130, 34);

    helpText.setBounds(35, 75, getWidth() - 70, getHeight() - 125);
    closeHelp.setBounds(getWidth() - 175, 88, 120, 32);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::timerCallback()
{
    if (calibratingNoise)
    {
        calibrationPeak = juce::jmax(calibrationPeak, p.level());
        if (++calibrationTicks >= 30)
            finishNoiseCalibration();
    }

    const float h = p.hz();
    readout.setText(h > 0.0f
                        ? juce::String(h, 1) + " Hz   •   MIDI " + juce::String(p.note())
                              + "   •   CONF " + juce::String(p.conf(), 2)
                        : "Waiting for vocal input...",
                    juce::dontSendNotification);

    const float level = p.level();
    const float db = level > 0.0f ? juce::Decibels::gainToDecibels(level, -100.0f) : -100.0f;
    levelReadout.setText("INPUT " + juce::String(db, 1) + " dBFS   •   CALIBRATE WHILE QUIET",
                         juce::dontSendNotification);

    const auto mapping = p.learn.mappingText();
    if (mapping.isNotEmpty())
        status.setText(mapping, juce::dontSendNotification);
}

juce::String IDWVoiceMIDIStudioAudioProcessorEditor::manualText()
{
    return R"MANUAL(IDW VOICE MIDI STUDIO — QUICK START & USER GUIDE

QUICK START
1. Put IDW Voice MIDI Studio on the FL Studio mixer insert receiving your microphone.
2. In the IDW plugin wrapper, set a MIDI Output Port such as 10.
3. On your destination synth wrapper, set MIDI Input Port to the same number.
4. Stay quiet and press CALIBRATE NOISE.
5. Choose CLEAN VOCAL or another preset.
6. Sing. Your destination instrument should follow your voice.

PRESETS
Clean Vocal — balanced everyday vocal tracking.
Tight Tracking — stricter pitch acceptance and less MIDI chatter.
Smooth Lead — easier tracking, wider bends and smoother expressive control.
Scale Locked Lead — turns on scale lock using a major-scale starting mask.
Wide Bend Performance — 12-semitone bend range for larger vocal slides.
Beatbox Drums — suppresses most pitched notes and emphasizes kick/snare/hat triggering.
Expressive MPE — 24-semitone bend, gesture CC and MPE enabled.
Live Responsive — responsive starting settings for live performance.

NOISE GATE
Minimum input level required before pitched notes are generated. Raise it in a noisy room; lower it for soft singing. CALIBRATE NOISE automatically sets this from the current quiet-room level.

CONFIDENCE
How certain the pitch detector must be before accepting a note. Higher values are cleaner but stricter. Lower values respond more easily.

BEND RANGE
Sets pitch-bend range in semitones. IMPORTANT: the receiving synth should use the same bend range or slides will sound wrong.

PITCH CAL ±CENTS
Fine-tuning offset. Leave at 0 unless your source needs tuning compensation.

SCALE LOCK / ROOT / NOTE BUTTONS
Enable SCALE LOCK to quantize outgoing MIDI. ROOT sets the key center. The 12 note buttons define which pitch classes are allowed, letting you build custom scales.

MIDI LEARN
Choose a destination in the MIDI Learn menu, press MIDI LEARN, then move a hardware MIDI control.

SAVE PRESET
Stores the current settings as a user preset. Saved presets appear in the PRESETS menu under USER.

FL STUDIO ROUTING
• Microphone → FL Studio Mixer Insert → IDW Voice MIDI Studio.
• IDW wrapper MIDI Output Port = example 10.
• Destination synth wrapper MIDI Input Port = the same number.
• Sing and confirm the synth plays.

BEATBOX
Beatbox detection outputs on MIDI channel 10. Defaults: Kick 36, Snare 38, Hi-hat 42. Start with the Beatbox Drums preset.

MPE
Use Expressive MPE only with an MPE-capable destination instrument. IDW allocates note channels and sends per-note expression across the configured MPE zone.

LATENCY
On Windows use an ASIO driver. Start at a 128-sample buffer. Try 64 for lower latency if stable, or 256 if you hear clicks/dropouts.

TROUBLESHOOTING
No vocal response:
• Make sure the mic is arriving on the mixer insert.
• Press CALIBRATE NOISE while quiet.
• Lower Noise Gate for soft input.
• Lower Confidence slightly if pitches are rejected.

Pitch moves but synth is silent:
• Check that IDW MIDI Output Port and synth MIDI Input Port match.

Wrong notes:
• Recalibrate noise.
• Raise Confidence.
• Turn on Scale Lock and choose the desired notes.

Slides sound wrong:
• Match the synth pitch-bend range to IDW Bend Range.

Too much delay:
• Lower the audio buffer.
• Use ASIO on Windows.
• Bypass high-latency effects while performing.

RECOMMENDED FIRST SESSION
Load Clean Vocal → Calibrate Noise → Route MIDI → Sing sustained notes → Test slides → Try Scale Lock → Save your own preset.
)MANUAL";
}
