#include "PluginEditor.h"

namespace
{
constexpr int meterX = 80;
constexpr int meterY = 196;
constexpr int meterH = 14;
}

IDWVoiceMIDIStudioAudioProcessorEditor::IDWVoiceMIDIStudioAudioProcessorEditor(IDWVoiceMIDIStudioAudioProcessor& processor)
    : AudioProcessorEditor(&processor), p(processor)
{
    setSize(1180, 760);

    title.setText("IDW VOICE MIDI STUDIO • V8.2 RC", juce::dontSendNotification);
    title.setFont(juce::Font(32.0f, juce::Font::bold));
    title.setJustificationType(juce::Justification::centred);
    title.setColour(juce::Label::textColourId, juce::Colour::fromRGB(212, 175, 55));
    addAndMakeVisible(title);

    subtitle.setText("REAL-TIME VOICE → MIDI PERFORMANCE ENGINE", juce::dontSendNotification);
    subtitle.setFont(juce::Font(15.0f, juce::Font::bold));
    subtitle.setJustificationType(juce::Justification::centred);
    subtitle.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.70f));
    addAndMakeVisible(subtitle);

    readout.setFont(juce::Font(34.0f, juce::Font::bold));
    readout.setJustificationType(juce::Justification::centred);
    readout.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(readout);

    status.setJustificationType(juce::Justification::centred);
    status.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.70f));
    addAndMakeVisible(status);

    styleCaption(gateCaption, "MIC GATE");
    styleCaption(confidenceCaption, "PITCH CONFIDENCE");
    styleCaption(bendCaption, "BEND RANGE");
    styleCaption(beatCaption, "BEATBOX SENSITIVITY");
    styleCaption(latencyCaption, "MIDI LATENCY");
    styleCaption(rootCaption, "ROOT");
    styleCaption(presetCaption, "PRESETS");
    styleCaption(learnCaption, "MIDI LEARN TARGET");

    styleSlider(gate);
    styleSlider(confidenceSlider);
    styleSlider(bendSlider, " st");
    styleSlider(beatThreshold);
    styleSlider(latencySlider, " ms");

    gate.setNumDecimalPlacesToDisplay(3);
    confidenceSlider.setNumDecimalPlacesToDisplay(2);
    beatThreshold.setNumDecimalPlacesToDisplay(3);
    bendSlider.setNumDecimalPlacesToDisplay(0);
    latencySlider.setNumDecimalPlacesToDisplay(0);

    gateAttachment = std::make_unique<SliderAttachment>(p.apvts, "gate", gate);
    confidenceAttachment = std::make_unique<SliderAttachment>(p.apvts, "confidence", confidenceSlider);
    bendAttachment = std::make_unique<SliderAttachment>(p.apvts, "bend", bendSlider);
    beatAttachment = std::make_unique<SliderAttachment>(p.apvts, "beatThreshold", beatThreshold);
    latencyAttachment = std::make_unique<SliderAttachment>(p.apvts, "latencyMs", latencySlider);

    for (auto* button : { &scaleLock, &beatbox, &gesture, &mpe, &calibrate, &learn, &save, &load })
    {
        styleButton(*button);
        addAndMakeVisible(*button);
    }

    scaleLock.setClickingTogglesState(true);
    beatbox.setClickingTogglesState(true);
    gesture.setClickingTogglesState(true);
    mpe.setClickingTogglesState(true);

    scaleLockAttachment = std::make_unique<ButtonAttachment>(p.apvts, "scaleLock", scaleLock);
    beatboxAttachment = std::make_unique<ButtonAttachment>(p.apvts, "beatbox", beatbox);
    gestureAttachment = std::make_unique<ButtonAttachment>(p.apvts, "gestureCC", gesture);
    mpeAttachment = std::make_unique<ButtonAttachment>(p.apvts, "mpe", mpe);

    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
    {
        scaleButtons[i].setButtonText(noteNames[i]);
        scaleButtons[i].setClickingTogglesState(false);
        styleButton(scaleButtons[i]);
        addAndMakeVisible(scaleButtons[i]);
        scaleButtons[i].onClick = [this, i]
        {
            const auto current = (uint16_t) p.apvts.getRawParameterValue("scaleMask")->load();
            uint16_t next = current ^ (uint16_t) (1u << i);
            if (next == 0)
                next = (uint16_t) (1u << i);

            if (auto* parameter = p.apvts.getParameter("scaleMask"))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1((float) next));
                parameter->endChangeGesture();
            }
            syncScale();
        };
    }

    for (int i = 0; i < 12; ++i)
        root.addItem(noteNames[i], i + 1);
    rootAttachment = std::make_unique<ComboBoxAttachment>(p.apvts, "root", root);
    addAndMakeVisible(root);

    auto destinations = p.learn.destinations();
    for (int i = 0; i < destinations.size(); ++i)
        learnTarget.addItem(destinations[i], i + 1);
    if (destinations.size() > 0)
        learnTarget.setSelectedId(1);
    addAndMakeVisible(learnTarget);

    addAndMakeVisible(presetList);
    refreshPresets();

    learn.onClick = [this]
    {
        p.learn.arm(learnTarget.getText());
        status.setText("MIDI Learn armed — move a controller now", juce::dontSendNotification);
    };

    save.onClick = [this]
    {
        const auto name = "IDW V8.2 " + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
        if (p.presets.save(name))
        {
            refreshPresets();
            presetList.setText(name, juce::dontSendNotification);
            status.setText("Preset saved: " + name, juce::dontSendNotification);
        }
    };

    load.onClick = [this]
    {
        const auto name = presetList.getText();
        if (name.isNotEmpty() && p.presets.load(name))
        {
            syncScale();
            status.setText("Preset loaded: " + name, juce::dontSendNotification);
        }
    };

    calibrate.onClick = [this]
    {
        const float measured = p.level();
        const float suggestedGate = juce::jlimit(0.001f, 0.080f,
                                                 juce::jmax(0.003f, measured * 2.5f));
        if (auto* parameter = p.apvts.getParameter("gate"))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(suggestedGate));
            parameter->endChangeGesture();
        }
        status.setText("Mic calibrated — gate set to " + juce::String(suggestedGate, 3)
                           + " (calibrate while silent)",
                       juce::dontSendNotification);
    };

    syncScale();
    startTimerHz(30);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::styleSlider(juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 92, 22);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB(212, 175, 55));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(50, 50, 54));
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(12, 12, 15));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(80, 70, 40));
    addAndMakeVisible(slider);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::styleButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 20, 24));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(212, 175, 55));
    button.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(232, 206, 120));
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::styleCaption(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(12.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(212, 175, 55));
    addAndMakeVisible(label);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::refreshPresets()
{
    const auto selected = presetList.getText();
    presetList.clear(juce::dontSendNotification);
    auto names = p.presets.list();
    for (int i = 0; i < names.size(); ++i)
        presetList.addItem(names[i], i + 1);

    if (selected.isNotEmpty())
        presetList.setText(selected, juce::dontSendNotification);
    else if (names.size() > 0)
        presetList.setSelectedId(1, juce::dontSendNotification);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::syncScale()
{
    const auto mask = (uint16_t) p.apvts.getRawParameterValue("scaleMask")->load();
    for (int i = 0; i < 12; ++i)
        scaleButtons[i].setToggleState((mask & (1u << i)) != 0, juce::dontSendNotification);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto gold = juce::Colour::fromRGB(212, 175, 55);
    const auto panel = juce::Colour::fromRGB(12, 12, 16);

    g.fillAll(juce::Colour::fromRGB(5, 5, 8));
    g.setColour(gold);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(9.0f), 20.0f, 2.0f);

    g.setColour(panel);
    g.fillRoundedRectangle(35.0f, 225.0f, (float) getWidth() - 70.0f, 165.0f, 16.0f);
    g.fillRoundedRectangle(35.0f, 410.0f, (float) getWidth() - 70.0f, 115.0f, 16.0f);
    g.fillRoundedRectangle(35.0f, 545.0f, (float) getWidth() - 70.0f, 120.0f, 16.0f);

    g.setColour(gold.withAlpha(0.45f));
    g.drawRoundedRectangle(35.0f, 225.0f, (float) getWidth() - 70.0f, 165.0f, 16.0f, 1.0f);
    g.drawRoundedRectangle(35.0f, 410.0f, (float) getWidth() - 70.0f, 115.0f, 16.0f, 1.0f);
    g.drawRoundedRectangle(35.0f, 545.0f, (float) getWidth() - 70.0f, 120.0f, 16.0f, 1.0f);

    const int meterWidth = getWidth() - (meterX * 2);
    const float db = juce::Decibels::gainToDecibels(p.level(), -60.0f);
    const float meter = juce::jmap(juce::jlimit(-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
    g.setColour(juce::Colour::fromRGB(30, 30, 34));
    g.fillRoundedRectangle((float) meterX, (float) meterY, (float) meterWidth, (float) meterH, 7.0f);
    g.setColour(gold);
    g.fillRoundedRectangle((float) meterX, (float) meterY, meterWidth * meter, (float) meterH, 7.0f);

    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.setFont(13.0f);
    g.drawFittedText("MIC LEVEL", meterX, meterY - 19, 100, 18, juce::Justification::left, 1);
    g.drawFittedText("YIN PITCH • SCALE LOCK • MPE • MIDI LEARN • BEATBOX MIDI • IDW PERFORMANCE ENGINE",
                     35, 700, getWidth() - 70, 22, juce::Justification::centred, 1);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::resized()
{
    title.setBounds(20, 20, getWidth() - 40, 42);
    subtitle.setBounds(20, 60, getWidth() - 40, 24);
    readout.setBounds(20, 96, getWidth() - 40, 55);
    status.setBounds(20, 155, getWidth() - 40, 26);

    const int xs[] = { 75, 290, 505, 720, 935 };
    juce::Label* captions[] = { &gateCaption, &confidenceCaption, &bendCaption, &beatCaption, &latencyCaption };
    juce::Slider* sliders[] = { &gate, &confidenceSlider, &bendSlider, &beatThreshold, &latencySlider };
    for (int i = 0; i < 5; ++i)
    {
        captions[i]->setBounds(xs[i], 236, 170, 20);
        sliders[i]->setBounds(xs[i], 258, 170, 118);
    }

    scaleLock.setBounds(78, 425, 150, 34);
    beatbox.setBounds(242, 425, 150, 34);
    gesture.setBounds(406, 425, 150, 34);
    mpe.setBounds(570, 425, 120, 34);
    calibrate.setBounds(704, 425, 180, 34);

    rootCaption.setBounds(78, 470, 100, 18);
    root.setBounds(78, 489, 100, 30);

    int scaleX = 205;
    for (int i = 0; i < 12; ++i)
        scaleButtons[i].setBounds(scaleX + i * 72, 483, 64, 34);

    presetCaption.setBounds(75, 560, 270, 18);
    presetList.setBounds(75, 582, 270, 32);
    save.setBounds(75, 620, 130, 32);
    load.setBounds(215, 620, 130, 32);

    learnCaption.setBounds(425, 560, 260, 18);
    learnTarget.setBounds(425, 582, 260, 32);
    learn.setBounds(425, 620, 260, 32);
}

void IDWVoiceMIDIStudioAudioProcessorEditor::timerCallback()
{
    const float h = p.hz();
    const float db = juce::Decibels::gainToDecibels(p.level(), -60.0f);

    if (h > 0.0f)
    {
        readout.setText(juce::String(h, 1) + " Hz  •  MIDI " + juce::String(p.note())
                            + "  •  CONF " + juce::String(p.conf(), 2)
                            + "  •  " + juce::String(db, 1) + " dB",
                        juce::dontSendNotification);
    }
    else
    {
        readout.setText("Waiting for vocal input...  •  " + juce::String(db, 1) + " dB",
                        juce::dontSendNotification);
    }

    const auto mapping = p.learn.mappingText();
    if (mapping.isNotEmpty())
        status.setText(mapping, juce::dontSendNotification);
    else if (status.getText().isEmpty())
        status.setText("Performance engine ready", juce::dontSendNotification);

    syncScale();
    repaint();
}
