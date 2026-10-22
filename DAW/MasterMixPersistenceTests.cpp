#include "ProjectPackaging.h"
#include <stdexcept>
#include <limits>
#include <iostream>
int main(){
 using namespace idw::daw;
 Session source;source.masterMix={0.63f,3.5f,-4.0f,-2.1f};
 auto same=[](const MasterMix& a,const MasterMix& b){return a.gain==b.gain && a.lowDb==b.lowDb && a.highDb==b.highDb && a.ceilingDb==b.ceilingDb;};
 Session loaded;
 if(!sessionFromJson(juce::JSON::parse(juce::JSON::toString(sessionToJson(source))),loaded) || !same(source.masterMix,loaded.masterMix))
    throw std::runtime_error("master settings round trip");
 auto legacy=sessionToJson(source);legacy.getDynamicObject()->removeProperty("masterMix");
 if(!sessionFromJson(legacy,loaded) || !same(loaded.masterMix,MasterMix{}))throw std::runtime_error("legacy mix defaults");
 for(const char* key:{"gain","lowDb","highDb","ceilingDb"}){
    for(const juce::var bad:{juce::var("1"),juce::var(true),juce::var(99.0),juce::var(-99.0),juce::var(std::numeric_limits<double>::quiet_NaN())}){
        auto data=sessionToJson(source);data.getDynamicObject()->getProperty("masterMix").getDynamicObject()->setProperty(key,bad);
        loaded.masterMix=source.masterMix;
        if(sessionFromJson(data,loaded) || !same(loaded.masterMix,source.masterMix))throw std::runtime_error("invalid mix load must be rejected atomically");
    }
    auto partial=sessionToJson(source);partial.getDynamicObject()->getProperty("masterMix").getDynamicObject()->removeProperty(key);
    if(sessionFromJson(partial,loaded))throw std::runtime_error("partial mix accepted");
 }
 const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-master-test","",false);
 juce::String error;
 if(!exportPortableProject(source,folder,error) || !loadSession(folder.getChildFile("Session.idwproj"),loaded) || !same(loaded.masterMix,source.masterMix))
    throw std::runtime_error("portable project mix recall");
 folder.deleteRecursively();
 std::cout<<"PASS: master mix recall, legacy defaults, invalid values and portable export\n";
}
