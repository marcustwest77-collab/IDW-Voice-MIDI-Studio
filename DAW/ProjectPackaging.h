#pragma once
#include "SessionPersistence.h"
#include <map>
namespace idw::daw {
// Export a self-contained folder: Session.idwproj and Audio/*. Files are copied,
// never moved. Do not overwrite an existing package directory.
inline bool exportPortableProject(const Session& source,const juce::File& folder,juce::String& error){
    error.clear();
    if(folder==juce::File{}){error="Choose a destination folder.";return false;}
    if(folder.exists()){error="Destination already exists.";return false;}
    if(!folder.createDirectory()){error="Could not create package directory.";return false;}
    const auto audio=folder.getChildFile("Audio");
    if(!audio.createDirectory()){error="Could not create Audio directory.";folder.deleteRecursively();return false;}
    Session portable=source;
    // Path identity, never basename/content identity: separate recordings with
    // the same name must stay separate, but split/repeated clips share one copy.
    std::map<std::string,std::string> copied;
    int fileIndex=0;
    for(auto& track:portable.tracks){
        for(auto& clip:track.clips){
            const juce::String savedPath(clip.sourcePath);
            if(!juce::File::isAbsolutePath(savedPath)){
                error="Resolve project audio paths before portable export.";
                folder.deleteRecursively();return false;
            }
            const juce::File original(savedPath);
            if(!original.existsAsFile()){
                error="Audio file missing: "+original.getFullPathName();
                folder.deleteRecursively();
                return false;
            }
            const auto key=original.getFullPathName().toStdString();
            const auto existing=copied.find(key);
            if(existing!=copied.end()){clip.sourcePath=existing->second;continue;}
            auto safeName=juce::String(++fileIndex).paddedLeft('0',4)+"_"+original.getFileName();
            const auto target=audio.getChildFile(safeName);
            if(!original.copyFileTo(target) || !original.hasIdenticalContentTo(target)){
                error="Cannot copy or verify audio: "+original.getFullPathName();
                folder.deleteRecursively();
                return false;
            }
            clip.sourcePath=("Audio/"+safeName).toStdString();
            copied.emplace(key,clip.sourcePath);
        }
    }
    if(!saveSession(portable,folder.getChildFile("Session.idwproj"))){
        error="Could not write packaged project.";
        folder.deleteRecursively();
        return false;
    }
    return true;
}
}
