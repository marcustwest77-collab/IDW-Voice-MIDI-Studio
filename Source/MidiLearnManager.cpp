#include "MidiLearnManager.h"
bool MidiLearnManager::process(const juce::MidiBuffer&m,juce::AudioProcessorValueTreeState&s){for(auto meta:m){auto x=meta.getMessage();if(!x.isController())continue;int cc=x.getControllerNumber();
if(armed&&!target.isEmpty()){map[cc]=target;last="CC "+juce::String(cc)+" → "+target;armed=false;}auto i=map.find(cc);if(i!=map.end())if(auto*p=s.getParameter(i->second)){p->beginChangeGesture();p->setValueNotifyingHost(x.getControllerValue()/127.f);p->endChangeGesture();}return true;}return false;}
