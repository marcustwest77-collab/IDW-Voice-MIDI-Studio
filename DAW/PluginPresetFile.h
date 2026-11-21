#pragma once
#include "PluginStateData.h"
namespace idw::daw {
inline std::optional<PluginStateData> readPluginPreset(const juce::File& file,juce::String& error){
    error.clear();
    juce::FileInputStream stream(file);
    constexpr int maximumBytes=maximumPluginStateText+65536;
    if(!stream.openedOk() || stream.getTotalLength()>maximumBytes){error="Preset cannot be read or is too large.";return {};}
    juce::MemoryBlock bytes;
    stream.readIntoMemoryBlock(bytes,maximumBytes+1);
    if(bytes.getSize()>(size_t)maximumBytes || stream.getStatus().failed()){
        error="Preset cannot be read or is too large.";return {};
    }
    auto state=decodePluginState(juce::String::fromUTF8((const char*)bytes.getData(),(int)bytes.getSize()));
    if(!state)error="This is not a valid IDW effect preset.";
    return state;
}
inline bool writePluginPreset(const juce::File& file,const PluginStateData& state,juce::String& error){
    error.clear();const auto encoded=encodePluginState(state);
    if(!encoded){error="The plugin settings cannot be saved.";return false;}
    juce::TemporaryFile staged(file);
    if(!staged.getFile().replaceWithText(*encoded) || !staged.overwriteTargetFileWithTemporary()){
        error="Could not write the preset. Choose a writable file location.";return false;
    }
    return true;
}
}
