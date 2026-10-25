#include "SessionRecovery.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static juce::String json(const Session& session){return juce::JSON::toString(sessionToJson(session),true);}
int main(){
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("idw-recovery","",false);
    require(root.createDirectory().wasOk(),"create fixture");
    const auto folder=root.getChildFile("recovery");
    const auto project=root.getChildFile("original.idwproj");
    const auto audio=root.getChildFile("source.wav");
    require(audio.replaceWithText("source bytes must remain unchanged"),"source fixture");
    Session session;session.addTrack(TrackType::audio);
    session.addClip(0,{"source.wav",0,4,0.25});
    session.setClipFades(0,0,0.5,1,FadeCurve::equalPower,FadeCurve::linear);
    session.addTrack(TrackType::midi);session.addMidiNote(1,{64,1,2,95});
    require(saveSession(session,project),"manual fixture");
    const auto originalProject=project.loadFileAsString(),originalAudio=audio.loadFileAsString();
    SessionRecovery recovery(folder);recovery.begin(session);
    require(recovery.checkpoint(session,project)==SessionRecovery::Result::unchanged,"clean project written");
    require(!folder.exists(),"clean project should not create recovery folder");
    session.setTempo(93);
    require(recovery.checkpoint(session,project)==SessionRecovery::Result::saved,"first snapshot");
    auto files=recovery.copies();require(files.size()==1,"first generation");
    const auto first=files.front();
    Session restored;juce::File origin;
    require(SessionRecovery::read(first,restored,origin) && origin==project,"read recovery origin");
    require(restored.bpm==93 && restored.tracks[0].clips[0].fadeInCurve==FadeCurve::equalPower &&
        restored.tracks[0].clips[0].sourcePath==audio.getFullPathName().toStdString() &&
        restored.tracks[1].notes.size()==1,"preserve fades notes and relative media location");
    require(recovery.checkpoint(session,project)==SessionRecovery::Result::unchanged &&
        recovery.copies().size()==1,"unchanged edit should not create another generation");
    // Deterministic write failure: the next destination is a directory.
    const auto blocked=folder.getChildFile(first.getFileName().replace("-0.idwproj","-1.idwproj"));
    require(blocked.createDirectory().wasOk(),"block destination");
    session.setTempo(97);
    require(recovery.checkpoint(session,project)==SessionRecovery::Result::failed,"write failure accepted");
    require(SessionRecovery::read(first,restored,origin) && restored.bpm==93,"failure lost last good copy");
    require(blocked.deleteFile(),"remove empty blocker");
    require(recovery.checkpoint(session,project)==SessionRecovery::Result::saved,"failed checkpoint was not retryable");
    require(recovery.copies().size()==2,"second generation");
    session.setTempo(101);
    require(recovery.checkpoint(session,project)==SessionRecovery::Result::saved &&
        recovery.copies().size()==2,"rotation must retain two generations");
    // Corrupt the latest slot; discovery retains the preceding valid generation.
    require(first.replaceWithText("{truncated"),"corrupt newest fixture");
    const auto before=json(restored);const auto beforeOrigin=origin;
    require(!SessionRecovery::read(first,restored,origin) && json(restored)==before &&
        origin==beforeOrigin,"bad recovery mutated live state");
    files=recovery.copies();require(files.size()==1,"corrupt file included");
    require(SessionRecovery::read(files.front(),restored,origin) && restored.bpm==97,"previous generation missing");
    // An unrelated app instance must not overwrite or delete our recoverable copy.
    SessionRecovery other(folder);other.begin(session);session.setTempo(105);
    require(other.checkpoint(session,project)==SessionRecovery::Result::saved,"second instance snapshot");
    const auto surviving=files.front();
    require(other.markSaved(session) && surviving.existsAsFile(),"manual save removed another owner's copy");
    require(other.checkpoint(session,project)==SessionRecovery::Result::unchanged,"saved project still dirty");
    // Switching documents leaves the previous document's copy intact.
    recovery.begin(session);session.setTempo(109);
    require(recovery.checkpoint(session,project)==SessionRecovery::Result::saved &&
        surviving.existsAsFile(),"project switch removed older document");
    // Missing media is retained as an absolute reference; recovery never requires decoding it.
    Session missing;missing.addTrack(TrackType::audio);missing.addClip(0,{"Audio/missing.wav",0,2,0});
    SessionRecovery missingRecovery(root.getChildFile("missing-recovery"));missingRecovery.begin(missing,true);
    require(missingRecovery.checkpoint(missing,project)==SessionRecovery::Result::saved,"missing media snapshot");
    require(SessionRecovery::read(missingRecovery.copies().front(),restored,origin) &&
        restored.tracks[0].clips[0].sourcePath==root.getChildFile("Audio/missing.wav").getFullPathName().toStdString(),
        "missing media resolved relative to wrong directory");
    // Untitled absolute-path projects also recover, including a session restored but not saved.
    Session untitled=session;untitled.tracks[0].clips[0].sourcePath=audio.getFullPathName().toStdString();
    SessionRecovery unsaved(root.getChildFile("untitled"));unsaved.begin(untitled,true);
    require(unsaved.checkpoint(untitled,{})==SessionRecovery::Result::saved,"untitled recovery");
    require(SessionRecovery::read(unsaved.copies().front(),restored,origin) &&
        origin==juce::File{} && json(restored)==json(untitled),"untitled round trip");
    // Partial temporary files and ordinary projects are not recovery candidates.
    require(saveSession(session,folder.getChildFile("ordinary.idwproj")),"ordinary fixture");
    require(folder.getChildFile("partial.tmp").replaceWithText("{"),"temporary fixture");
    for(const auto& file:recovery.copies())require(file.getFileName()!="ordinary.idwproj","ordinary project offered as recovery");
    require(project.loadFileAsString()==originalProject && audio.loadFileAsString()==originalAudio,
        "recovery changed manual project or source audio");
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS: recovery rotation, failed writes, corrupt copy fallback, isolation, media references and manual-save protection\n";
}
