#pragma once
#include "SessionPersistence.h"
namespace idw::daw {
// Project metadata only. Audio files stay in their original locations.
// Each document instance owns two alternating files; other instances are never overwritten.
class SessionRecovery {
public:
    enum class Result { unchanged, saved, failed };
    explicit SessionRecovery(juce::File directory):folder(std::move(directory)) {}
    void begin(const Session& session,bool needsSave=false) {
        owner=juce::Uuid().toString();nextSlot=0;
        lastText=needsSave?juce::String{}:text(session);
    }
    Result checkpoint(const Session& session,const juce::File& project) {
        const auto current=text(session);
        if(current==lastText)return Result::unchanged;
        if(!folder.createDirectory().wasOk())return Result::failed;
        Session snapshot=session;
        for(auto& track:snapshot.tracks)for(auto& clip:track.clips){
            const juce::String path(clip.sourcePath);
            if(!juce::File::isAbsolutePath(path)){
                // Recovery lives elsewhere: never resolve missing media relative to it.
                if(project==juce::File{})return Result::failed;
                clip.sourcePath=project.getParentDirectory().getChildFile(path).getFullPathName().toStdString();
            }
        }
        auto data=sessionToJson(snapshot);
        auto* root=data.getDynamicObject();
        root->setProperty("recoveryVersion",1);
        root->setProperty("recoveryProject",project==juce::File{}?juce::String{}:project.getFullPathName());
        const auto target=slot(nextSlot);
        if(target.isDirectory())return Result::failed;
        juce::TemporaryFile temporary(target);
        if(!temporary.getFile().replaceWithText(juce::JSON::toString(data,true)))return Result::failed;
        Session checked;juce::File checkedProject;
        if(!read(temporary.getFile(),checked,checkedProject) ||
           text(checked)!=text(snapshot) || checkedProject!=project)return Result::failed;
        if(!temporary.overwriteTargetFileWithTemporary())return Result::failed;
        lastText=current;nextSlot=1-nextSlot;
        return Result::saved;
    }
    // Call only after a successful manual save. Failed cleanup leaves a harmless copy.
    bool markSaved(const Session& session) {
        lastText=text(session);
        const bool a=!slot(0).exists() || slot(0).deleteFile();
        const bool b=!slot(1).exists() || slot(1).deleteFile();
        nextSlot=0;return a && b;
    }
    static bool read(const juce::File& file,Session& output,juce::File& project) {
        if(!file.existsAsFile() || file.getSize()>32*1024*1024)return false;
        const auto data=juce::JSON::parse(file);
        auto* root=data.getDynamicObject();
        if(!root || !root->getProperty("recoveryVersion").isInt() ||
           (int)root->getProperty("recoveryVersion")!=1)return false;
        const auto origin=root->getProperty("recoveryProject");
        if(!origin.isString() || (origin.toString().isNotEmpty() &&
           !juce::File::isAbsolutePath(origin.toString())))return false;
        Session next;if(!sessionFromJson(data,next))return false;
        for(const auto& track:next.tracks)for(const auto& clip:track.clips)
            if(!juce::File::isAbsolutePath(juce::String(clip.sourcePath)))return false;
        output=std::move(next);
        project=origin.toString().isEmpty()?juce::File{}:juce::File(origin.toString());
        return true;
    }
    std::vector<juce::File> copies() const {
        std::vector<juce::File> result;
        for(const auto& file:folder.findChildFiles(juce::File::findFiles,false,"*.idwproj")){
            Session ignored;juce::File project;
            if(read(file,ignored,project))result.push_back(file);
        }
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){
            return a.getLastModificationTime()>b.getLastModificationTime();
        });
        return result;
    }
    const juce::File& directory() const {return folder;}
private:
    static juce::String text(const Session& session){return juce::JSON::toString(sessionToJson(session),true);}
    juce::File slot(int number) const {return folder.getChildFile(owner+"-"+juce::String(number)+".idwproj");}
    juce::File folder;
    juce::String owner=juce::Uuid().toString(),lastText;
    int nextSlot=0;
};
}
