#include "RecentProjects.h"
#include "SessionPersistence.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-recent-projects","",false);
    try{
        require(root.createDirectory().wasOk(),"fixture folder");
        const auto preferences=root.getChildFile("prefs/RecentProjects.json");
        RecentProjects recent(preferences);
        require(!recent.load() && recent.entries().empty(),"missing preferences");
        require(!recent.remember({}) && recent.entries().empty(),"empty path accepted");
        Session s;s.addTrack(TrackType::midi);s.addMidiNote(0,{60,0,1,70});
        const auto project=root.getChildFile(juce::String::fromUTF8("Piano \xce\xb2.idwproj"));
        require(saveSession(s,project),"project fixture");const auto projectBytes=project.loadFileAsString();
        require(recent.remember(project) && recent.entries().size()==1,"first remember");
        RecentProjects loaded(preferences);require(loaded.load() && loaded.entries()==recent.entries(),"preference roundtrip");
        require(recent.remember(project) && recent.entries().size()==1,"duplicate entry");
        const auto another=root.getChildFile("other/Piano.idwproj");
        require(recent.remember(another) && recent.entries()[0]==another,"missing project path metadata");
        require(!another.exists() && recent.remember(project) && recent.entries()[0]==project && recent.entries()[1]==another,"reopen ordering or implicit project creation");
        for(int i=0;i<12;++i)require(recent.remember(root.getChildFile("Song"+juce::String(i)+".idwproj")),"bounded list save");
        require(recent.entries().size()==8 && recent.entries().front().getFileName()=="Song11.idwproj" &&
            recent.entries().back().getFileName()=="Song4.idwproj","recency cap");
        require(recent.remember(recent.entries().back()) && recent.entries().front().getFileName()=="Song4.idwproj" && recent.entries().size()==8,"move old entry to front");
        const auto before=recent.entries();const auto validPreferences=preferences.loadFileAsString();
        for(int failure=0;failure<3;++failure){
            require(!recent.saveWithWriter([&](const juce::File& target,const juce::String& text){
                if(failure==0)return false;
                return target.replaceWithText(failure==1?juce::String("{partial"):text.replace("Song4","SongX"));
            }),"bad staged write accepted");
            require(preferences.loadFileAsString()==validPreferences && recent.entries()==before,"write failure lost good list");
        }
        // Invalid metadata never replaces the existing in-memory list.
        const juce::String invalid[]={"{partial","[]","{}","{\"version\":2,\"paths\":[]}",
            "{\"version\":true,\"paths\":[]}","{\"version\":1,\"paths\":[42]}",
            "{\"version\":1,\"paths\":[\"relative.idwproj\"]}","{\"version\":1,\"paths\":[\"\"]}"};
        for(const auto& data:invalid){require(preferences.replaceWithText(data),"corrupt fixture");require(!recent.load() && recent.entries()==before,"invalid load mutated list");}
        require(preferences.replaceWithText(juce::String::repeatedString("[",1000)+"0"+juce::String::repeatedString("]",1000)) && !recent.load(),"deep JSON accepted");
        auto makeData=[&](const juce::Array<juce::var>& paths){auto data=std::make_unique<juce::DynamicObject>();data->setProperty("version",1);data->setProperty("paths",paths);return juce::JSON::toString(juce::var(data.release()));};
        juce::Array<juce::var> duplicates;duplicates.add(project.getFullPathName());duplicates.add(project.getFullPathName());
        require(preferences.replaceWithText(makeData(duplicates)) && !recent.load() && recent.entries()==before,"duplicate metadata accepted");
        juce::Array<juce::var> tooMany;for(int i=0;i<9;++i)tooMany.add(root.getChildFile("Extra"+juce::String(i)).getFullPathName());
        require(preferences.replaceWithText(makeData(tooMany)) && !recent.load(),"oversized count accepted");
        juce::Array<juce::var> badPaths;badPaths.add(root.getFullPathName()+"/line\nbreak.idwproj");
        require(preferences.replaceWithText(makeData(badPaths)) && !recent.load(),"control character accepted");
        badPaths.clear();badPaths.add(root.getFullPathName()+"/"+juce::String::repeatedString("x",4097));
        require(preferences.replaceWithText(makeData(badPaths)) && !recent.load(),"long path accepted");
        require(preferences.replaceWithText(juce::String::repeatedString("x",static_cast<int>(RecentProjects::maximumBytes+1))) && !recent.load() && recent.entries()==before,"byte bound accepted");
        require(preferences.replaceWithText(validPreferences) && recent.load() && recent.entries()==before,"recovery from corrupt preference");
        // Saving/opening a project must remain possible if preference storage fails.
        const auto blocker=root.getChildFile("blocked");require(blocker.replaceWithText("not a folder"),"blocked parent");
        RecentProjects failed(blocker.getChildFile("RecentProjects.json"));
        require(!failed.remember(project) && failed.entries().size()==1 && project.loadFileAsString()==projectBytes,"preference failure affected project");
        require(blocker.deleteFile() && blocker.createDirectory().wasOk() && failed.remember(project),"failed preference retry");
        RecentProjects repaired(blocker.getChildFile("RecentProjects.json"));require(repaired.load() && repaired.entries().size()==1,"retry readback");
        Session checked;require(loadSession(project,checked) && checked.tracks[0].notes[0].velocity==70 && project.loadFileAsString()==projectBytes,"project content touched");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"Recent project tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
