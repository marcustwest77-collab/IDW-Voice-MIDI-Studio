#include "ProjectPackaging.h"
#include <stdexcept>
#include <iostream>
int main(){
    using namespace idw::daw;
    Session s;s.lyrics=u8"[Hook]\nIn Da Wind \u2014 we rise \U0001F3B5\n\n[Verse 1]\nLine two\n";
    auto data=sessionToJson(s);Session loaded;
    if(!sessionFromJson(juce::JSON::parse(juce::JSON::toString(data)),loaded) || loaded.lyrics!=s.lyrics)
        throw std::runtime_error("Unicode/newlines round trip");
    auto* root=data.getDynamicObject();root->removeProperty("lyrics");
    if(!sessionFromJson(data,loaded) || !loaded.lyrics.empty())throw std::runtime_error("legacy defaults");
    root->setProperty("lyrics",42);
    loaded.lyrics="keep existing";
    if(sessionFromJson(data,loaded) || loaded.lyrics!="keep existing")throw std::runtime_error("invalid load is atomic");
    root->setProperty("lyrics",juce::String::repeatedString("x",32769));
    if(sessionFromJson(data,loaded))throw std::runtime_error("oversized lyrics accepted");
    root->setProperty("lyrics",juce::String::repeatedString("x",32768));
    if(!sessionFromJson(data,loaded))throw std::runtime_error("maximum lyrics rejected");
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-lyrics-test","",false);
    juce::String error;
    if(!exportPortableProject(s,folder,error) || !loadSession(folder.getChildFile("Session.idwproj"),loaded) || loaded.lyrics!=s.lyrics)
        throw std::runtime_error("portable project lyrics");
    folder.deleteRecursively();
    std::cout<<"PASS: lyrics persistence, legacy defaults, validation and portable project\n";
}
