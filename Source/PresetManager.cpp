#include "PresetManager.h"
juce::File PresetManager::dir()const{auto d=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IDW Voice MIDI Studio/Presets");d.createDirectory();return d;}
bool PresetManager::save(const juce::String&n){auto x=state.copyState().createXml();return x&&x->writeTo(dir().getChildFile(n+".xml"));}
bool PresetManager::load(const juce::String&n){auto x=juce::XmlDocument::parse(dir().getChildFile(n+".xml"));if(!x)return false;state.replaceState(juce::ValueTree::fromXml(*x));return true;}
juce::StringArray PresetManager::list()const{juce::StringArray a;for(auto&f:dir().findChildFiles(juce::File::findFiles,false,"*.xml"))a.add(f.getFileNameWithoutExtension());a.sort(true);return a;}
