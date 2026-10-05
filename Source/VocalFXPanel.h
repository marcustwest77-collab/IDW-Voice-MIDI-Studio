#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
#include "AutoKeyDetector.h"
#include "SkinTheme.h"

class VocalFXPanel : public juce::Component {
public:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;
    using ButtonAttachment = APVTS::ButtonAttachment;
    using ComboAttachment = APVTS::ComboBoxAttachment;

    VocalFXPanel(APVTS& state, std::function<void()> close, std::function<void()> startKey,
                 std::function<void()> stopKey, std::function<idw::KeySuggestion()> getKey)
        : apvts(state), closeAction(std::move(close)), startKeyAction(std::move(startKey)),
          stopKeyAction(std::move(stopKey)), keySuggestion(std::move(getKey)) {
        title.setText("VOCAL FX / TUNE + PRODUCTION RACK", juce::dontSendNotification);
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
                                                                       &enable, &mode, &adaptive, &formant, &audioHarmony, &safeTracking, &outputGuard, &quality,
                                                                       &rackEnable, &levelMatch, &learnKey, &applyKey, &keyLabel, &harmonyStyle, &delayDivision,
                                                                       &storeA, &recallA, &storeB, &recallB, &closeButton})
            addAndMakeVisible(component);
        enable.setButtonText("Enable Vocal Tune");
        enable.setTooltip("Processes microphone audio into a corrected vocal. Use headphones to prevent feedback.");
        adaptive.setButtonText("Adaptive Tune");adaptive.setTooltip("Speeds up note changes while slowing small sustained-note corrections to preserve vibrato.");
        formant.setButtonText("Formant Preserve (Beta)");formant.setTooltip("LPC envelope transfer reduces chipmunk/boomy character on larger corrections. Compare carefully on your voice.");
        audioHarmony.setButtonText("Audio Harmony");audioHarmony.setTooltip("Adds scale-aware sung harmony voices to the audio output. Use headphones.");
        safeTracking.setButtonText("Safe Tracking");safeTracking.setTooltip("Temporarily bypasses formant processing, audio harmonies, doubler, reverb and delay while keeping tuning and core dynamics active.");
        outputGuard.setButtonText("Clip Guard");outputGuard.setTooltip("Softly catches peaks above -0.5 dBFS at the final output, including the Studio Instrument.");
        quality.addItem("CPU: Eco",1);quality.addItem("CPU: Studio",2);quality.addItem("CPU: High",3);
        quality.setTooltip("Eco bypasses LPC formants and limits harmony to one voice. Studio is the default. High doubles LPC envelope analysis frequency.");
        rackEnable.setButtonText("Enable FX Rack");
        rackEnable.setTooltip("Turns on the production effects after the vocal tuner. The rack defaults off in existing sessions.");
        levelMatch.setButtonText("Level Match");levelMatch.setTooltip("Slowly matches processed loudness to the incoming vocal for fairer A/B comparisons.");
        mode.addItem("Chromatic - nearest note", 1);
        mode.addItem("Song scale - use root/scale", 2);
        harmonyStyle.addItem("Harmony: upper 3rd",1);harmonyStyle.addItem("Harmony: low + high 3rd",2);harmonyStyle.addItem("Harmony: 3rd + 5th",3);harmonyStyle.addItem("Harmony: octaves",4);
        learnKey.setButtonText("Learn key");learnKey.setTooltip("Listen to a clear vocal phrase and estimate the song key locally.");
        applyKey.setButtonText("Apply key");applyKey.setEnabled(false);applyKey.setTooltip("Apply the suggested root and major/minor scale to Vocal Tune.");
        keyLabel.setText("AUTO-KEY: ready",juce::dontSendNotification);keyLabel.setColour(juce::Label::textColourId,juce::Colour(0xff68dfbd));
        learnKey.onClick=[this]{const auto current=getKeySuggestion();if(current.learning){if(stopKeyAction)stopKeyAction();}else if(startKeyAction)startKeyAction();refreshKey();};
        applyKey.onClick=[this]{const auto key=getKeySuggestion();if(!key.valid)return;set("root",(float)key.root);set("scaleMask",(float)(key.mode==0?idw::AutoKeyDetector::majorMask:idw::AutoKeyDetector::minorMask));set("vocalTuneMode",1);refreshKey();};
        closeButton.setButtonText("Close");
        closeButton.onClick = [this] { if (closeAction) closeAction(); };
        storeA.setButtonText("Store A");recallA.setButtonText("Recall A");storeB.setButtonText("Store B");recallB.setButtonText("Recall B");
        storeA.onClick=[this]{capture(snapshotA);recallA.setEnabled(true);};storeB.onClick=[this]{capture(snapshotB);recallB.setEnabled(true);};
        recallA.onClick=[this]{recall(snapshotA);};recallB.onClick=[this]{recall(snapshotB);};recallA.setEnabled(false);recallB.setEnabled(false);

        configure(speed, speedLabel, "RETUNE SPEED", "Fast = hard tune; slow = smoother transitions.", " ms", 0);
        configure(amount, amountLabel, "TUNE AMOUNT", "How strongly the voice moves toward the target note.", " %", 0, 100.0);
        configure(humanize, humanizeLabel, "HUMANIZE", "Keeps small natural pitch movement near the note.", " %", 0, 100.0);
        configure(mix, mixLabel, "WET MIX", "Blend between original and corrected vocal.", " %", 0, 100.0);
        configure(output, outputLabel, "OUTPUT", "Corrected vocal output trim.", " dB", 1);
        configure(vibrato,vibratoLabel,"VIBRATO","How strongly Adaptive Tune protects small sustained-note pitch movement."," %",0,100.0);
        configure(harmonyMix,harmonyMixLabel,"HARMONY MIX","Level of the scale-aware audio harmony voices."," %",0,100.0);
        configure(harmonyConfidence,harmonyConfidenceLabel,"HARMONY GATE","Confidence required before harmony voices fade in."," %",0,100.0);

        enableAttachment = std::make_unique<ButtonAttachment>(apvts, "vocalTuneEnabled", enable);
        adaptiveAttachment = std::make_unique<ButtonAttachment>(apvts,"vocalTuneAdaptive",adaptive);
        formantAttachment=std::make_unique<ButtonAttachment>(apvts,"vocalFormantPreserve",formant);
        audioHarmonyAttachment=std::make_unique<ButtonAttachment>(apvts,"vocalAudioHarmony",audioHarmony);
        safeTrackingAttachment=std::make_unique<ButtonAttachment>(apvts,"safeTracking",safeTracking);
        outputGuardAttachment=std::make_unique<ButtonAttachment>(apvts,"outputGuard",outputGuard);
        qualityAttachment=std::make_unique<ComboAttachment>(apvts,"vocalQuality",quality);
        rackEnableAttachment = std::make_unique<ButtonAttachment>(apvts, "vocalFxEnabled", rackEnable);
        levelMatchAttachment = std::make_unique<ButtonAttachment>(apvts,"vocalFxAutoGain",levelMatch);
        modeAttachment = std::make_unique<ComboAttachment>(apvts, "vocalTuneMode", mode);
        harmonyStyleAttachment=std::make_unique<ComboAttachment>(apvts,"vocalHarmonyStyle",harmonyStyle);
        attach("vocalTuneSpeed", speed); attach("vocalTuneAmount", amount);
        attach("vocalTuneHumanize", humanize); attach("vocalTuneMix", mix);
        attach("vocalTuneOutput", output);attach("vocalTuneVibrato",vibrato);attach("vocalHarmonyMix",harmonyMix);attach("vocalHarmonyConfidence",harmonyConfidence);

        const std::array<const char*, 6> moduleNames{{"DE-ESSER", "COMPRESSOR", "SATURATION", "DOUBLER", "REVERB", "DELAY"}};
        const std::array<const char*, 6> moduleIds{{"deEsserEnabled", "compressorEnabled", "saturationEnabled", "doublerEnabled", "reverbEnabled", "delayEnabled"}};
        const std::array<const char*, 6> amountIds{{"deEsserAmount", "compressorAmount", "saturationAmount", "doublerAmount", "reverbAmount", "delayAmount"}};
        const std::array<const char*, 6> tips{{"Softens harsh S and T sounds.", "Levels loud and quiet vocal phrases.", "Adds controlled warmth and edge.", "Adds stereo vocal width.", "Adds a compact vocal room.", "Adds a tempo-friendly vocal echo."}};
        for (size_t i=0;i<moduleButtons.size();++i) {
            moduleButtons[i].setButtonText(moduleNames[i]); moduleButtons[i].setTooltip(tips[i]); addAndMakeVisible(moduleButtons[i]);
            moduleAttachments.push_back(std::make_unique<ButtonAttachment>(apvts,moduleIds[i],moduleButtons[i]));
            configure(moduleAmounts[i],moduleLabels[i],"AMOUNT",tips[i]," %",0,100.0); attach(amountIds[i],moduleAmounts[i]);
        }
        moduleLabels[5].setVisible(false);delayDivision.addItem("1/8",1);delayDivision.addItem("1/8 dotted",2);delayDivision.addItem("1/4",3);delayDivision.addItem("1/2",4);
        delayDivision.setTooltip("Delay time synchronized to the BPM shown on the main screen.");delayDivisionAttachment=std::make_unique<ComboAttachment>(apvts,"delayDivision",delayDivision);
        configure(fxMix,fxMixLabel,"RACK MIX","Blends the complete production rack with the tuned or dry vocal."," %",0,100.0);
        attach("vocalFxMix",fxMix);

        const std::array<const char*, 6> labels{{"Natural", "Smooth R&B", "Tight", "Memphis Hard", "Singing Rap", "Robot"}};
        for (size_t i = 0; i < presetButtons.size(); ++i) {
            presetButtons[i].setButtonText(labels[i]);
            addAndMakeVisible(presetButtons[i]);
            presetButtons[i].onClick = [this, i] { applyPreset((int)i); };
        }
    }

    void update(float correctionSemitones, int targetNote, float latencyMs, bool signalValid, float outputLevel, unsigned int guardBlocks) {
        refreshKey();
        const auto outputDb=juce::Decibels::gainToDecibels(outputLevel,-100.0f);
        const auto outputText="  /  OUT "+juce::String(outputDb,1)+" dB"+(guardBlocks>0?"  /  GUARD "+juce::String(guardBlocks):juce::String());
        if (!signalValid || targetNote < 0) {
            live.setText("Sing a clear note to see correction  /  latency ~" + juce::String(latencyMs, 1) + " ms"+outputText, juce::dontSendNotification);
            return;
        }
        const auto note = juce::MidiMessage::getMidiNoteName(targetNote, true, true, 4);
        live.setText("TARGET " + note + "   /   CORRECTION " + juce::String(correctionSemitones >= 0 ? "+" : "")
                     + juce::String(correctionSemitones, 2) + " st   /   latency ~" + juce::String(latencyMs, 1) + " ms"+outputText,
                     juce::dontSendNotification);
    }

    void applySkin(const idw::SkinPalette& next) {
        skin=&next;
        title.setColour(juce::Label::textColourId,juce::Colour(skin->accent));
        subtitle.setColour(juce::Label::textColourId,juce::Colour(skin->muted));
        safety.setColour(juce::Label::textColourId,juce::Colour(skin->safety));
        live.setColour(juce::Label::textColourId,juce::Colour(skin->highlight));
        keyLabel.setColour(juce::Label::textColourId,juce::Colour(skin->highlight));
        for(auto* label:{&speedLabel,&amountLabel,&humanizeLabel,&vibratoLabel,&harmonyMixLabel,&harmonyConfidenceLabel,&mixLabel,&outputLabel,&fxMixLabel})
            label->setColour(juce::Label::textColourId,juce::Colour(skin->muted));
        for(auto& label:moduleLabels)label.setColour(juce::Label::textColourId,juce::Colour(skin->muted));
        repaint();
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(skin->background));
        const auto panel = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(skin->card)); g.fillRoundedRectangle(panel, 12.0f);
        g.setColour(juce::Colour(skin->accent).withAlpha(0.4f)); g.drawRoundedRectangle(panel, 12.0f, 1.0f);
        g.setColour(juce::Colour(skin->muted)); g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("VOCAL CHARACTER PRESETS", 26, 226, getWidth() - 52, 22, juce::Justification::centredLeft);
        g.drawText("MANUAL CONTROLS", 26, 325, getWidth() - 52, 22, juce::Justification::centredLeft);
        g.drawText("VOCAL PRODUCTION RACK", 26, 536, getWidth() - 52, 22, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawLine(26.0f, 217.0f, (float)getWidth() - 26.0f, 217.0f);
        g.drawLine(26.0f, 315.0f, (float)getWidth() - 26.0f, 315.0f);
        g.drawLine(26.0f, 526.0f, (float)getWidth() - 26.0f, 526.0f);
    }

    void resized() override {
        const int w = getWidth();
        title.setBounds(26, 20, w - 190, 32); subtitle.setBounds(27, 50, w - 190, 24);
        closeButton.setBounds(w - 120, 24, 92, 30);
        enable.setBounds(30,95,180,34);mode.setBounds(220,96,220,32);learnKey.setBounds(450,96,95,32);applyKey.setBounds(550,96,90,32);keyLabel.setBounds(650,96,w-830,32);harmonyStyle.setBounds(w-170,96,140,32);
        live.setBounds(30,139,w-60,35);safety.setBounds(30,168,345,25);quality.setBounds(385,168,135,26);safeTracking.setBounds(530,168,145,25);outputGuard.setBounds(680,168,125,25);
        adaptive.setBounds(30,195,145,25);formant.setBounds(185,195,175,25);audioHarmony.setBounds(370,195,150,25);
        storeA.setBounds(w-424,194,92,27);recallA.setBounds(w-327,194,92,27);storeB.setBounds(w-230,194,92,27);recallB.setBounds(w-133,194,103,27);
        const int gap = 10, left = 30;
        const int presetW = (w - 2 * left - 5 * gap) / 6;
        for (int i = 0; i < 6; ++i) presetButtons[(size_t)i].setBounds(left + i * (presetW + gap), 255, presetW, 42);
        const int knobW = (w - 60) / 8;
        juce::Slider* knobs[]{&speed, &amount, &humanize, &vibrato, &harmonyMix, &harmonyConfidence, &mix, &output};
        juce::Label* labels[]{&speedLabel, &amountLabel, &humanizeLabel, &vibratoLabel, &harmonyMixLabel, &harmonyConfidenceLabel, &mixLabel, &outputLabel};
        for (int i = 0; i < 8; ++i) {
            labels[i]->setBounds(30 + i * knobW, 352, knobW, 24);
            knobs[i]->setBounds(30 + i * knobW, 376, knobW, 150);
        }
        rackEnable.setBounds(30,562,150,28);levelMatch.setBounds(30,594,150,28);fxMixLabel.setBounds(w-165,544,135,22);fxMix.setBounds(w-160,564,125,96);
        const int rackLeft=190,rackRight=175,rackGap=5;
        const int moduleW=(w-rackLeft-rackRight-5*rackGap)/6;
        for(int i=0;i<6;++i){
            const int x=rackLeft+i*(moduleW+rackGap);
            moduleButtons[(size_t)i].setBounds(x,562,moduleW,28);
            moduleLabels[(size_t)i].setBounds(x,594,moduleW,20);
            moduleAmounts[(size_t)i].setBounds(x,614,moduleW,108);
            if(i==5)delayDivision.setBounds(x,594,moduleW,22);
        }
    }

private:
    struct Snapshot { std::vector<float> values; bool valid=false; };
    static const std::vector<const char*>& snapshotIds(){
        static const std::vector<const char*> ids{
            "vocalTuneEnabled","vocalTuneMode","vocalTuneSpeed","vocalTuneAmount","vocalTuneHumanize","vocalTuneMix","vocalTuneOutput",
            "vocalTuneAdaptive","vocalTuneVibrato","vocalFormantPreserve","vocalAudioHarmony","vocalHarmonyStyle","vocalHarmonyMix","vocalHarmonyConfidence",
            "vocalQuality","safeTracking","outputGuard","vocalFxEnabled","vocalFxAutoGain","deEsserEnabled","compressorEnabled","saturationEnabled",
            "doublerEnabled","reverbEnabled","delayEnabled","deEsserAmount","compressorAmount","saturationAmount","doublerAmount","reverbAmount","delayAmount","vocalFxMix","delayDivision"
        };return ids;
    }
    void capture(Snapshot& snapshot){snapshot.values.clear();for(const auto* id:snapshotIds())snapshot.values.push_back(apvts.getRawParameterValue(id)->load());snapshot.valid=true;}
    void recall(const Snapshot& snapshot){if(!snapshot.valid||snapshot.values.size()!=snapshotIds().size())return;for(std::size_t i=0;i<snapshot.values.size();++i)set(snapshotIds()[i],snapshot.values[i]);}
    idw::KeySuggestion getKeySuggestion() const{return keySuggestion?keySuggestion():idw::KeySuggestion{};}
    void refreshKey(){
        const auto key=getKeySuggestion();learnKey.setButtonText(key.learning?"Stop learning":"Learn key");applyKey.setEnabled(key.valid);
        if(!key.valid){keyLabel.setText(key.learning?"AUTO-KEY: sing a phrase":"AUTO-KEY: ready",juce::dontSendNotification);return;}
        static const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        keyLabel.setText(juce::String("AUTO-KEY: ")+names[key.root]+(key.mode==0?" major  ":" minor  ")+juce::String((int)std::lround(key.confidence*100.0f))+"%",juce::dontSendNotification);
    }
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
        struct Values { float mode, speed, amount, humanize, vibrato, formant, mix, output; float deEss,comp,sat,doubleAmount,reverb,delay; };
        static constexpr Values presets[] = {
            {1,95,.72f,.72f,.85f,1,.90f,0,.32f,.34f,.08f,0,.08f,0}, {1,70,.82f,.62f,.75f,1,1,0,.42f,.50f,.18f,.18f,.24f,.10f},
            {0,25,.95f,.22f,.35f,1,1,0,.50f,.58f,.20f,0,.04f,0}, {0,8,1,.03f,.05f,1,1,0,.38f,.70f,.55f,.32f,.12f,.26f},
            {1,18,.96f,.15f,.28f,1,1,0,.48f,.62f,.30f,.22f,.16f,.12f}, {0,5,1,0,0,0,1,-1,.15f,.75f,.78f,.55f,.08f,.35f}
        };
        const auto& v = presets[juce::jlimit(0, 5, index)];
        set("vocalTuneEnabled", 1); set("vocalTuneMode", v.mode); set("vocalTuneSpeed", v.speed);
        set("vocalTuneAmount", v.amount); set("vocalTuneHumanize", v.humanize);
        set("vocalTuneMix", v.mix); set("vocalTuneOutput", v.output);
        set("vocalTuneAdaptive",1);set("vocalTuneVibrato",v.vibrato);
        set("vocalFormantPreserve",v.formant);set("vocalAudioHarmony",0);set("vocalHarmonyStyle",2);set("vocalHarmonyMix",.25f);
        set("vocalFxEnabled",1);set("deEsserEnabled",v.deEss>0);set("compressorEnabled",v.comp>0);
        set("saturationEnabled",v.sat>0);set("doublerEnabled",v.doubleAmount>0);set("reverbEnabled",v.reverb>0);set("delayEnabled",v.delay>0);
        set("deEsserAmount",v.deEss);set("compressorAmount",v.comp);set("saturationAmount",v.sat);
        set("doublerAmount",v.doubleAmount);set("reverbAmount",v.reverb);set("delayAmount",v.delay);set("vocalFxMix",1);set("vocalFxAutoGain",1);
    }

    APVTS& apvts;
    std::function<void()> closeAction;
    std::function<void()> startKeyAction,stopKeyAction;
    std::function<idw::KeySuggestion()> keySuggestion;
    juce::Label title, subtitle, safety, live, keyLabel;
    juce::ToggleButton enable, adaptive, formant, audioHarmony, safeTracking, outputGuard, rackEnable, levelMatch;
    juce::ComboBox mode,harmonyStyle,delayDivision,quality;
    juce::TextButton closeButton,learnKey,applyKey,storeA,recallA,storeB,recallB;
    juce::Slider speed, amount, humanize, vibrato, harmonyMix, harmonyConfidence, mix, output, fxMix;
    juce::Label speedLabel, amountLabel, humanizeLabel, vibratoLabel, harmonyMixLabel, harmonyConfidenceLabel, mixLabel, outputLabel, fxMixLabel;
    std::array<juce::ToggleButton,6> moduleButtons;
    std::array<juce::Slider,6> moduleAmounts;
    std::array<juce::Label,6> moduleLabels;
    std::array<juce::TextButton, 6> presetButtons;
    std::unique_ptr<ButtonAttachment> enableAttachment;
    std::unique_ptr<ButtonAttachment> adaptiveAttachment;
    std::unique_ptr<ButtonAttachment> formantAttachment,audioHarmonyAttachment;
    std::unique_ptr<ButtonAttachment> safeTrackingAttachment,outputGuardAttachment;
    std::unique_ptr<ButtonAttachment> rackEnableAttachment;
    std::unique_ptr<ButtonAttachment> levelMatchAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;
    std::unique_ptr<ComboAttachment> harmonyStyleAttachment;
    std::unique_ptr<ComboAttachment> qualityAttachment;
    std::unique_ptr<ComboAttachment> delayDivisionAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> moduleAttachments;
    Snapshot snapshotA,snapshotB;
    const idw::SkinPalette* skin=&idw::skins.front();
};
