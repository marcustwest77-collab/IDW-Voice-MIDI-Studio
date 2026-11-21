#include "PluginStateData.h"
#include "PluginPresetFile.h"
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
    const auto presetFile=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("idw-effect-preset-test",".idwfx",false);
    struct Cleanup {juce::File file;~Cleanup(){file.deleteFile();}} cleanup{presetFile};
    juce::String error;
    require(writePluginPreset(presetFile,state,error) && error.isEmpty(),"write preset file");
    const auto fromFile=readPluginPreset(presetFile,error);
    require(fromFile && fromFile->identifier==state.identifier && fromFile->bytes==state.bytes && fromFile->bypassed,"preset file roundtrip");
    const auto original=presetFile.loadFileAsString();
    require(!writePluginPreset(presetFile,PluginStateData{},error) && error.isNotEmpty() &&
        presetFile.loadFileAsString()==original,"invalid capture replaced existing preset");
    auto replacement=state;replacement.bypassed=false;replacement.bytes.reset();
    require(writePluginPreset(presetFile,replacement,error),"replace preset");
    const auto replaced=readPluginPreset(presetFile,error);
    require(replaced && !replaced->bypassed && replaced->bytes.getSize()==0,"replacement state mismatch");
    require(presetFile.replaceWithText("{bad json"),"create corrupt preset");
    require(!readPluginPreset(presetFile,error) && error.isNotEmpty(),"corrupt preset accepted");
    require(presetFile.replaceWithText(juce::String::repeatedString("x",maximumPluginStateText+65537)),"create oversized preset");
    require(!readPluginPreset(presetFile,error),"oversized file accepted");
    require(!readPluginPreset(presetFile.getSiblingFile("missing-"+juce::Uuid().toString()+".idwfx"),error),"missing file accepted");
    std::cout<<"Plugin state data tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
