#include "PluginStateData.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
    PluginStateData state;state.identifier="VST3-test-identity";
    const unsigned char binary[]{0,1,2,127,128,254,255};state.bytes.append(binary,sizeof(binary));state.bypassed=true;
    const auto before=state.bytes;const auto encoded=encodePluginState(state);require(encoded.has_value(),"encode");
    const auto decoded=decodePluginState(*encoded);
    require(decoded && decoded->identifier==state.identifier && decoded->bypassed &&
        decoded->bytes==before && state.bytes==before,"binary/bypass roundtrip or mutation");
    require(pluginStateMatches(*decoded,state.identifier) && !pluginStateMatches(*decoded,"different-plugin"),"identity gate");
    PluginStateData empty;empty.identifier="empty-state";
    const auto emptyText=encodePluginState(empty);require(emptyText && decodePluginState(*emptyText)->bytes.getSize()==0,"empty state");
    require(!encodePluginState(PluginStateData{}),"empty identity");
    auto object=juce::JSON::parse(*encoded);
    for(const auto* bad:{"!","AAAA=","A","AA==\n","AB=="}){
        object.getDynamicObject()->setProperty("state",bad);
        require(!decodePluginState(juce::JSON::toString(object)),"invalid/noncanonical base64");
    }
    object=juce::JSON::parse(*encoded);object.getDynamicObject()->setProperty("version",2);
    require(!decodePluginState(juce::JSON::toString(object)),"future version");
    object=juce::JSON::parse(*encoded);object.getDynamicObject()->setProperty("bypassed",1);
    require(!decodePluginState(juce::JSON::toString(object)),"non-boolean bypass");
    object=juce::JSON::parse(*encoded);object.getDynamicObject()->removeProperty("state");
    require(!decodePluginState(juce::JSON::toString(object)),"missing state");
    require(!decodePluginState("[]") && !decodePluginState("{"),"invalid document");
    PluginStateData maximum;maximum.identifier="maximum";maximum.bytes.setSize(maximumPluginStateBytes,true);
    const auto maximumText=encodePluginState(maximum);require(maximumText && decodePluginState(*maximumText)->bytes==maximum.bytes,"maximum state");
    maximum.bytes.setSize(maximumPluginStateBytes+1);
    require(!encodePluginState(maximum),"oversized binary");
    require(!decodePluginState(juce::String::repeatedString("x",maximumPluginStateText+65537)),"oversized document");
    std::cout<<"Plugin state data tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
