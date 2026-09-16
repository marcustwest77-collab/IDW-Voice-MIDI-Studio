#include "PluginEditor.h"
IDWVoiceMIDIStudioAudioProcessorEditor::IDWVoiceMIDIStudioAudioProcessorEditor(IDWVoiceMIDIStudioAudioProcessor&x):AudioProcessorEditor(&x),p(x){setSize(1040,680);
title.setText("IDW VOICE MIDI STUDIO • V7",juce::dontSendNotification);title.setFont(juce::Font(32,juce::Font::bold));title.setJustificationType(juce::Justification::centred);addAndMakeVisible(title);
readout.setFont(juce::Font(38,juce::Font::bold));readout.setJustificationType(juce::Justification::centred);addAndMakeVisible(readout);status.setJustificationType(juce::Justification::centred);addAndMakeVisible(status);
const char*n[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};for(int i=0;i<12;i++){scaleButtons[i].setButtonText(n[i]);addAndMakeVisible(scaleButtons[i]);scaleButtons[i].onClick=[this,i]{p.scale.toggle(i);if(auto*v=p.apvts.getParameter("scaleMask"))v->setValueNotifyingHost(p.scale.getMask()/4095.f);syncScale();};}
auto d=p.learn.destinations();for(int i=0;i<d.size();i++)learnTarget.addItem(d[i],i+1);learnTarget.setSelectedId(1);addAndMakeVisible(learnTarget);addAndMakeVisible(learn);addAndMakeVisible(save);
learn.onClick=[this]{p.learn.arm(learnTarget.getText());};save.onClick=[this]{p.presets.save("IDW V7 "+juce::Time::getCurrentTime().formatted("%H%M%S"));};syncScale();startTimerHz(30);}
void IDWVoiceMIDIStudioAudioProcessorEditor::syncScale(){for(int i=0;i<12;i++)scaleButtons[i].setToggleState(p.scale.enabled(i),juce::dontSendNotification);}
void IDWVoiceMIDIStudioAudioProcessorEditor::paint(juce::Graphics&g){g.fillAll(juce::Colour::fromRGB(5,5,8));g.setColour(juce::Colours::gold);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(9),20,2);g.setFont(18);
g.drawFittedText("YIN PITCH ENGINE • MPE • MIDI LEARN MATRIX FOUNDATION • CUSTOM SCALE • BEATBOX MIDI",45,280,getWidth()-90,70,juce::Justification::centred,2);
g.setColour(juce::Colours::white.withAlpha(.5f));g.setFont(14);g.drawFittedText("IDW PERFORMANCE ENGINE • V7",30,620,getWidth()-60,25,juce::Justification::centred,1);}
void IDWVoiceMIDIStudioAudioProcessorEditor::resized(){title.setBounds(20,25,getWidth()-40,50);readout.setBounds(20,105,getWidth()-40,70);status.setBounds(20,180,getWidth()-40,30);
int x=110;for(int i=0;i<12;i++){scaleButtons[i].setBounds(x+i*68,390,60,36);}learnTarget.setBounds(280,470,170,34);learn.setBounds(465,470,130,34);save.setBounds(610,470,140,34);}
void IDWVoiceMIDIStudioAudioProcessorEditor::timerCallback(){float h=p.hz();readout.setText(h>0?juce::String(h,1)+" Hz • MIDI "+juce::String(p.note())+" • CONF "+juce::String(p.conf(),2):"Waiting for vocal input...",juce::dontSendNotification);
auto s=p.learn.mappingText();status.setText(s.isNotEmpty()?s:"Performance engine ready",juce::dontSendNotification);}
