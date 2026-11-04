#pragma once
#include "SessionPersistence.h"

namespace idw::daw {
inline juce::File projectSaveDestination(const juce::File& selected){
    if(selected==juce::File{})return {};
    auto name=selected.getFileName().trimCharactersAtEnd(".");
    // File pickers can append another extension to a typed full filename.
    // Remove repeated project suffixes and separator dots, preserving the stem.
    while(name.endsWithIgnoreCase(".idwproj")){
        name=name.dropLastCharacters(8).trimCharactersAtEnd(".");
    }
    if(name.isEmpty())return {};
    return selected.getSiblingFile(name+".idwproj");
}
inline juce::File suggestedProjectSaveDestination(const juce::File& current,const juce::File& documents){
    return current==juce::File{}?documents.getChildFile("IDW Session.idwproj"):projectSaveDestination(current);
}
class ProjectSavePlan {
public:
    ProjectSavePlan(const Session& session,const juce::File& origin,const juce::File& selected)
        : snapshot(session),before(juce::JSON::toString(sessionToJson(session),true)),project(origin),
          destination(projectSaveDestination(selected)) {}
    const juce::File& target() const {return destination;}
    bool commit(const Session& live,const juce::File& origin,bool idle,bool replaceApproved,juce::String& error){
        error.clear();
        if(used){error="This save request has already completed.";return false;}
        if(!idle){error="Stop playback, recording and export before saving.";return false;}
        if(origin!=project || juce::JSON::toString(sessionToJson(live),true)!=before){
            error="The project changed while the save request was open. Choose SAVE PROJECT again.";return false;
        }
        if(destination==juce::File{} || destination.isDirectory()){
            error="Choose a project filename, not a folder.";return false;
        }
        if(destination.exists() && !replaceApproved){error="Confirm replacement of the existing project first.";return false;}
        if(!saveProjectSession(snapshot,destination)){
            error="Could not write the project. Your current project and save status are unchanged.";return false;
        }
        used=true;return true;
    }
private:
    Session snapshot;
    juce::String before;
    juce::File project,destination;
    bool used=false;
};
}
