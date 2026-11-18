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
    juce::String replacementSummary() const {
        size_t clips=0,notes=0;
        for(const auto& track:snapshot.tracks){clips+=track.clips.size();notes+=track.notes.size();}
        const auto source=project==juce::File{}?juce::String("Untitled project"):project.getFullPathName();
        return "Current session snapshot:\n"+source+
            "\n\nExisting file to replace:\n"+destination.getFullPathName()+
            "\n\nTracks: "+juce::String((juce::int64)snapshot.tracks.size())+
            " | Audio clips: "+juce::String((juce::int64)clips)+" | MIDI notes: "+juce::String((juce::int64)notes)+
            (project!=juce::File{} && project!=destination
                ? "\n\nThis replaces a different project file with the current session. It does not open or merge that file."
                : "\n\nThis saves the current session over the existing file.")+
            "\nAudio files are referenced, not copied. Use Export portable project for a copy with audio."
            "\n\nChoose Cancel to keep the existing file and select another filename.";
    }
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
