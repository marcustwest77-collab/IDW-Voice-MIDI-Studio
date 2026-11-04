#pragma once
#include "SessionRecovery.h"

namespace idw::daw {
// Confirmation belongs to one exact document state. Recovery must succeed before
// replacing it; neither the saved project nor any source audio is overwritten.
class NewProjectPlan {
public:
    NewProjectPlan(const Session& session,const juce::File& project)
        : before(text(session)),origin(project) {}
    bool commit(Session& session,juce::File& project,SessionRecovery& recovery,
                bool confirmed,bool idle,juce::String& error) {
        error.clear();
        if(!confirmed)return false;
        if(!idle){error="Stop playback, recording, editing and export before starting a new project.";return false;}
        if(used || project!=origin || text(session)!=before){
            error="The project changed while confirmation was open. Choose New empty project again.";return false;
        }
        Session next;
        next.addTrack(TrackType::audio);next.addTrack(TrackType::midi);
        if(recovery.checkpoint(session,project)==SessionRecovery::Result::failed){
            error="Your current work could not be protected. Save your project manually before trying again.";return false;
        }
        recovery.begin(next);
        session=std::move(next);project=juce::File{};used=true;
        return true;
    }
private:
    static juce::String text(const Session& session){return juce::JSON::toString(sessionToJson(session),true);}
    juce::String before;
    juce::File origin;
    bool used=false;
};
}
