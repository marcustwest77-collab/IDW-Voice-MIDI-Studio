#pragma once
#include <juce_core/juce_core.h>
#include <optional>
namespace idw::daw {
// Bounded state container, independent of plugin execution. Callers must capture
// from a stopped instance and restore only to a newly created matching candidate.
struct PluginStateData {
    juce::String identifier;
    bool bypassed=false;
    juce::MemoryBlock bytes;
};
inline constexpr size_t maximumPluginStateBytes=4u*1024u*1024u;
inline constexpr int maximumPluginStateText=(int)((maximumPluginStateBytes+2)/3*4);
inline bool validPluginState(const PluginStateData& state){
    return state.identifier.isNotEmpty() && state.identifier.length()<=4096 &&
        state.bytes.getSize()<=maximumPluginStateBytes;
}
inline std::optional<juce::String> encodePluginState(const PluginStateData& state){
    if(!validPluginState(state))return {};
    auto* object=new juce::DynamicObject();
    object->setProperty("kind","idw-vst3-state");object->setProperty("version",1);
    object->setProperty("identifier",state.identifier);object->setProperty("bypassed",state.bypassed);
    object->setProperty("state",juce::Base64::toBase64(state.bytes.getData(),state.bytes.getSize()));
    return juce::JSON::toString(juce::var(object),true);
}
inline std::optional<PluginStateData> decodePluginState(const juce::String& text){
    // Includes headroom for escaped identifier and JSON punctuation.
    if(text.length()>maximumPluginStateText+65536)return {};
    const auto value=juce::JSON::parse(text);const auto* object=value.getDynamicObject();
    if(!object)return {};
    const auto kind=object->getProperty("kind"),version=object->getProperty("version");
    const auto identifier=object->getProperty("identifier"),bypass=object->getProperty("bypassed");
    const auto encoded=object->getProperty("state");
    if(!kind.isString() || kind.toString()!="idw-vst3-state" || !version.isInt() || (int)version!=1 ||
       !identifier.isString() || !bypass.isBool() || !encoded.isString())return {};
    const auto payload=encoded.toString();if(payload.length()>maximumPluginStateText)return {};
    PluginStateData result;result.identifier=identifier.toString();result.bypassed=(bool)bypass;
    if(result.identifier.isEmpty() || result.identifier.length()>4096)return {};
    {
        juce::MemoryOutputStream stream(result.bytes,false);
        if(!juce::Base64::convertFromBase64(stream,payload))return {};
    }
    // Re-encoding rejects noncanonical/truncated data and keeps decoding strict.
    if(!validPluginState(result) ||
       juce::Base64::toBase64(result.bytes.getData(),result.bytes.getSize())!=payload)return {};
    return result;
}
inline bool pluginStateMatches(const PluginStateData& state,const juce::String& instanceIdentifier){
    return validPluginState(state) && state.identifier==instanceIdentifier;
}
}
