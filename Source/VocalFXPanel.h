#pragma once
#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

class VocalFXPanel : public juce::Component {
public:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;
    using ButtonAttachment = APVTS::ButtonAttachment;
    using ComboAttachment = APVTS::ComboBoxAttachment;

    VocalFXPanel(APVTS& state, std::function<void()> close)
        : apvts(state), closeAction(std::move(close)) {
        title.setText("VOCAL FX / REAL-TIME PITCH CORRECTION", juce::dontSendNotification);
        title.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, juce::Colour(0xffeac36c));
        subtitle.setText("Original local tuner - your audio stays on this computer", juce::dontSendNotification);
        subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff9aabc2));
        safety.setText("Use headphones. Vocal Tune outputs the corrected microphone signal; turn it off before using speakers.", juce::dontSendNotification);
        safety.setColour(juce::Label::textColourId, juce::Colour(0xffffd77c));
        live.setText("Sing a clear note to see correction", juce::dontSendNotification);
        live.setFont(juce::FontOptions(19.0f, juce::Font::bold));
        live.setColour(juce::Label::textColourId, juce::Colour(0xff68dfbd));
        for (auto* component : std::initializer_list<juce::Component*>{&title, &subtitle, &safety, &live,
                                                                       &enable, &mode, &closeButton})
            addAndMakeVisible(component);
        enable.setButtonText("Enable Vocal Tune");
        enable.setTooltip("Processes microphone audio into a corrected vocal. Use headphones to prevent feedback.");
        mode.addItem("Chromatic - nearest note", 1);
        mode.addItem("Song scale - use root/scale", 2);
        closeButton.setButtonText("Close");
        closeButton.onClick = [this] { if (closeAction) closeAction(); };

        configure(speed, speedLabel, "RETUNE SPEED", "Fast = hard tune; slow = smoother transitions.", " ms", 0);
        configure(amount, amountLabel, "TUNE AMOUNT", "How strongly the voice moves toward the target note.", " %", 0, 100.0);
        configure(humanize, humanizeLabel, "HUMANIZE", "Keeps small natural pitch movement near the note.", " %", 0, 100.0);
        configure(mix, mixLabel, "WET MIX", "Blend between original and corrected vocal.", " %", 0, 100.0);
        configure(output, outputLabel, "OUTPUT", "Corrected vocal output trim.", " dB", 1);

        enableAttachment = std::make_unique<ButtonAttachment>(apvts, "vocalTuneEnabled", enable);
        modeAttachment = std::make_unique<ComboAttachment>(apvts, "vocalTuneMode", mode);
        attach("vocalTuneSpeed", speed); attach("vocalTuneAmount", amount);
        attach("vocalTuneHumanize", humanize); attach("vocalTuneMix", mix);
        attach("vocalTuneOutput", output);

        const std::array<const char*, 6> labels{{"Natural", "Smooth R&B", "Tight", "Memphis Hard", "Singing Rap", "Robot"}};
        for (size_t i = 0; i < presetButtons.size(); ++i) {
            presetButtons[i].setButtonText(labels[i]);
            addAndMakeVisible(presetButtons[i]);
            presetButtons[i].onClick = [this, i] { applyPreset((int)i); };
        }
    }

    void update(float correctionSemitones, int targetNote, float latencyMs, bool signalValid) {
        if (!signalValid || targetNote < 0) {
            live.setText("Sing a clear note to see correction  /  latency ~" + juce::String(latencyMs, 1) + " ms", juce::dontSendNotification);
            return;
        }
        const auto note = juce::MidiMessage::getMidiNoteName(targetNote, true, true, 4);
        live.setText("TARGET " + note + "   /   CORRECTION " + juce::String(correctionSemitones >= 0 ? "+" : "")
                     + juce::String(correctionSemitones, 2) + " st   /   latency ~" + juce::String(latencyMs, 1) + " ms",
                     juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff0b0e14));
        const auto panel = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(0xff141a24)); g.fillRoundedRectangle(panel, 12.0f);
        g.setColour(juce::Colour(0xffeac36c).withAlpha(0.4f)); g.drawRoundedRectangle(panel, 12.0f, 1.0f);
        g.setColour(juce::Colour(0xff9aabc2)); g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("VOCAL CHARACTER PRESETS", 26, 205, getWidth() - 52, 22, juce::Justification::centredLeft);
        g.drawText("MANUAL CONTROLS", 26, 312, getWidth() - 52, 22, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawLine(26.0f, 194.0f, (float)getWidth() - 26.0f, 194.0f);
        g.drawLine(26.0f, 301.0f, (float)getWidth() - 26.0f, 301.0f);
    }

    void resized() override {
        const int w = getWidth();
        title.setBounds(26, 20, w - 190, 32); subtitle.setBounds(27, 50, w - 190, 24);
        closeButton.setBounds(w - 120, 24, 92, 30);
        enable.setBounds(30, 95, 190, 34); mode.setBounds(235, 96, 260, 32);
        live.setBounds(30, 139, w - 60, 35); safety.setBounds(30, 168, w - 60, 25);
        const int gap = 10, left = 30;
        const int presetW = (w - 2 * left - 5 * gap) / 6;
        for (int i = 0; i < 6; ++i) presetButtons[(size_t)i].setBounds(left + i * (presetW + gap), 238, presetW, 42);
        const int knobW = (w - 60) / 5;
        juce::Slider* knobs[]{&speed, &amount, &humanize, &mix, &output};
        juce::Label* labels[]{&speedLabel, &amountLabel, &humanizeLabel, &mixLabel, &outputLabel};
        for (int i = 0; i < 5; ++i) {
            labels[i]->setBounds(30 + i * knobW, 344, knobW, 24);
            knobs[i]->setBounds(30 + i * knobW, 368, knobW, 150);
        }
    }

private:
    void configure(juce::Slider& slider, juce::Label& label, const juce::String& text,
                   const juce::String& tip, const juce::String& suffix, int decimals, double displayScale = 1.0) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 24);
        slider.setTooltip(tip); slider.setNumDecimalPlacesToDisplay(decimals);
        slider.textFromValueFunction = [suffix, decimals, displayScale](double value) {
            return juce::String(value * displayScale, decimals) + suffix;
        };
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colour(0xff9aabc2));
        addAndMakeVisible(slider); addAndMakeVisible(label);
    }
    void attach(const char* id, juce::Slider& slider) { sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, id, slider)); }
    void set(const char* id, float value) {
        if (auto* parameter = apvts.getParameter(id)) {
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); parameter->endChangeGesture();
        }
    }
    void applyPreset(int index) {
        struct Values { float mode, speed, amount, humanize, mix, output; };
        static constexpr Values presets[] = {
            {1, 95, .72f, .72f, .90f, 0}, {1, 70, .82f, .62f, 1, 0},
            {0, 25, .95f, .22f, 1, 0}, {0, 8, 1, .03f, 1, 0},
            {1, 18, .96f, .15f, 1, 0}, {0, 5, 1, 0, 1, -1}
        };
        const auto& v = presets[juce::jlimit(0, 5, index)];
        set("vocalTuneEnabled", 1); set("vocalTuneMode", v.mode); set("vocalTuneSpeed", v.speed);
        set("vocalTuneAmount", v.amount); set("vocalTuneHumanize", v.humanize);
        set("vocalTuneMix", v.mix); set("vocalTuneOutput", v.output);
    }

    APVTS& apvts;
    std::function<void()> closeAction;
    juce::Label title, subtitle, safety, live;
    juce::ToggleButton enable;
    juce::ComboBox mode;
    juce::TextButton closeButton;
    juce::Slider speed, amount, humanize, mix, output;
    juce::Label speedLabel, amountLabel, humanizeLabel, mixLabel, outputLabel;
    std::array<juce::TextButton, 6> presetButtons;
    std::unique_ptr<ButtonAttachment> enableAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
};
