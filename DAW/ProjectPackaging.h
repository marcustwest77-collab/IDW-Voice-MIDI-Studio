#pragma once
#include "SessionPersistence.h"
namespace idw::daw {
// Export a self-contained folder: Session.idwproj and Audio/*. Files are copied,
// never moved. Do not overwrite an existing package directory.
inline bool exportPortableProject(const Session& source,const juce::File& folder,juce::String& error){
    if(folder.exists()){error="Destination already exists.";return false;}
    if(!folder.createDirectory()){error="Could not create package directory.";return false;}
    const auto audio=folder.getChildFile("Audio");
    if(!audio.createDirectory()){error="Could not create Audio directory.";return false;}
    Session portable=source;
    int fileIndex=0;
    for(auto& track:portable.tracks){
        for(auto& clip:track.clips){
            const juce::File original(juce::String(clip.sourcePath));
            if(!original.existsAsFile()){
                error="Audio file missing: "+original.getFullPathName();
                folder.deleteRecursively();
                return false;
            }
            auto safeName=juce::String(++fileIndex).paddedLeft('0',4)+"_"+original.getFileName();
            const auto target=audio.getChildFile(safeName);
            if(!original.copyFileTo(target)){
                error="Cannot copy audio: "+original.getFullPathName();
                folder.deleteRecursively();
                return false;
            }
            clip.sourcePath=("Audio/"+safeName).toStdString();
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
