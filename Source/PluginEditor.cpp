#include "PluginEditor.h"

IDWVoiceMIDIStudioAudioProcessorEditor::IDWVoiceMIDIStudioAudioProcessorEditor(IDWVoiceMIDIStudioAudioProcessor& processor)
    : AudioProcessorEditor(&processor), p(processor)
{
    setSize(1080, 720);

    title.setText("IDW VOICE MIDI STUDIO • V9", juce::dontSendNotification);
    title.setFont(juce::Font(32.0f, juce::Font::bold));
    title.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(title);

    readout.setFont(juce::Font(36.0f, juce::Font::bold));
    readout.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(readout);

    status.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(status);

    levelReadout.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(levelReadout);

    configureSlider(gateSlider, gateLabel, "NOISE GATE");
    configureSlider(confidenceSlider, confidenceLabel, "CONFIDENCE");
    configureSlider(bendSlider, bendLabel, "BEND RANGE");
    configureSlider(tuneSlider, tuneLabel, "PITCH CAL ±CENTS");

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

    addAndMakeVisible(scaleLock);
    scaleLockAttachment = std::make_unique<ButtonAttachment>(p.apvts, "scaleLock", scaleLock);

    const char* roots[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        rootSelector.addItem(roots[i], i + 1);
    rootSelector.setSelectedId((int) p.apvts.getRawParameterValue("root")->load() + 1, juce::dontSendNotification);
    addAndMakeVisible(rootSelector);
    rootLabel.setText("ROOT", juce::dontSendNotification);
    rootLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(rootLabel);
    rootAttachment = std::make_unique<ComboAttachment>(p.apvts, "root", rootSelector);

    for (int i = 0; i < 12; ++i)
    {
        scaleButtons[i].setButtonText(roots[i]);
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
    addAndMakeVisible(learnTarget);

    addAndMakeVisible(learn);
    addAndMakeVisible(save);
    addAndMakeVisible(calibrate);

    learn.onClick = [this] { p.learn.arm(learnTarget.getText()); };
    save.onClick = [this]
    {
        p.presets.save("IDW V9 " + juce::Time::getCurrentTime().formatted("%H%M%S"));
    };
    calibrate.onClick = [this] { calibrateNoiseGate(); };

    syncScale();
    startTimerHz(30);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::configureSlider(juce::Slider& slider,
                                                              juce::Label& label,
                                                              const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::calibrateNoiseGate()
{
    const float ambient = p.level();
    const float threshold = juce::jlimit(.001f, .2f, juce::jmax(.002f, ambient * 2.5f));

    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter("gate")))
    {
        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost(ranged->convertTo0to1(threshold));
        ranged->endChangeGesture();
        status.setText("Noise gate calibrated to " + juce::String(threshold, 3), juce::dontSendNotification);
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
    g.drawFittedText("VOICE → MIDI • SCALE LOCK • PITCH BEND • MPE • BEATBOX • MIDI LEARN",
                     30, 660, getWidth() - 60, 24, juce::Justification::centred, 1);
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
        scaleButtons[i].setBounds(x + i * 70, 485, 62, 36);

    learnTarget.setBounds(250, 565, 175, 34);
    learn.setBounds(440, 565, 130, 34);
    save.setBounds(585, 565, 140, 34);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::timerCallback()
{
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
    else if (! status.getText().startsWith("Noise gate calibrated"))
        status.setText("Performance engine ready", juce::dontSendNotification);
}
