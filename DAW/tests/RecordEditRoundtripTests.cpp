#include "TakeRecorder.h"
#include "SessionPersistence.h"
#include "SessionHistory.h"
#include "SessionRecovery.h"
#include "OfflineBounce.h"
#include <iostream>
#include <stdexcept>

// Synthetic callback input exercises production engines, not physical hardware
// or Workstation UI automation. Optional fresh output directory retains evidence.
static void require(bool ok,const char* message) {
    if(!ok) throw std::runtime_error(message);
}
static juce::AudioBuffer<float> readWav(const juce::File& file,int rate,int frames) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    require(reader && reader->sampleRate==rate && reader->numChannels==2 &&
            reader->lengthInSamples==frames,"WAV format/duration changed");
    juce::AudioBuffer<float> result(2,frames);
    require(reader->read(&result,0,frames,0,true,true),"cannot read WAV");
    return result;
}
static void near(float actual,float expected,const char* message) {
    require(std::isfinite(actual) && std::abs(actual-expected)<0.00001f,message);
}
int main(int argc,char** argv) {
    try {
        require(argc<=2,"usage: RecordEditRoundtripTests [fresh-evidence-directory]");
        const auto root=argc==2 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]) :
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getNonexistentChildFile("idw-record-edit-roundtrip","",false);
        require(!root.exists() && root.createDirectory().wasOk(),"evidence directory must be new");
        const auto take=root.getChildFile("synthetic-stereo-take.wav");
        {
            IDWTakeRecorder recorder;
            require(recorder.start(take,48000,2,1),"record start");
            juce::AudioBuffer<float> input(3,256);
            juce::AudioIODeviceCallbackContext context{};
            for(int position=0;position<96000;position+=256) {
                const int count=std::min(256,96000-position);
                for(int i=0;i<count;++i) {
                    input.setSample(0,i,0.875f); // Unselected input must never enter the take.
                    input.setSample(1,i,position+i<48000?0.25f:-0.125f);
                    input.setSample(2,i,position+i<48000?-0.0625f:0.1875f);
                }
                recorder.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),3,
                    nullptr,0,count,context);
                juce::Thread::sleep(1); // Let the real background disk writer drain.
            }
            recorder.stop();
            require(!recorder.isRecording() && recorder.getCapturedFrames()==96000 &&
                    recorder.getQueuedFrames()==96000 && recorder.getDroppedBlocks()==0 &&
                    recorder.getRejectedWrites()==0,"capture lost or rejected frames");
        }
        const auto captured=readWav(take,48000,96000);
        near(captured.getSample(0,12000),0.25f,"left input routing");
        near(captured.getSample(1,12000),-0.0625f,"right input routing");
        near(captured.getSample(0,72000),-0.125f,"late left capture");
        near(captured.getSample(1,72000),0.1875f,"late right capture");
        juce::MemoryBlock original;require(take.loadFileAsData(original),"take snapshot");

        using namespace idw::daw;
        Session session;session.setTempo(120);
        require(session.addTrack(TrackType::audio) &&
                session.addClip(0,{take.getFullPathName().toStdString(),0,4,0}),"import recorded take");
        session.tracks[0].name="Synthetic stereo workflow";
        session.tracks[0].inputChannel=1;
        require(session.setTrackMix(0,0.8f,-0.25f),"track mix");
        const auto before=session;
        require(session.splitClip(0,0,2) && session.trimClipLeft(0,0,0.5),"split/trim");
        require(session.moveClip(0,1,3),"move second half leaving a gap");
        require(session.setClipGain(0,1,-6) && session.setClipFades(0,1,0.1,0.2),"clip gain/fades");
        ClipEditHistory history;
        require(history.record(before,session),"record edit history");
        const auto edited=juce::JSON::toString(sessionToJson(session),true);
        require(history.undo(session)==ClipEditHistory::Result::applied &&
                sameClips(session.tracks[0].clips,before.tracks[0].clips),"undo roundtrip");
        require(history.redo(session)==ClipEditHistory::Result::applied &&
                juce::JSON::toString(sessionToJson(session),true)==edited,"redo roundtrip");

        const auto project=root.getChildFile("Workflow.idwproj");
        require(saveSession(session,project),"save project");
        session=Session{}; // Reopen into fresh state; do not reuse the edited object.
        require(loadSession(project,session) &&
                juce::JSON::toString(sessionToJson(session),true)==edited,"reopened project differs");
        SessionRecovery recovery(root.getChildFile("Recovery"));
        recovery.begin(session,true);
        require(recovery.checkpoint(session,project)==SessionRecovery::Result::saved,"recovery checkpoint");
        const auto copies=recovery.copies();Session recovered;juce::File origin;
        require(copies.size()==1 && SessionRecovery::read(copies.front(),recovered,origin) &&
                origin==project && juce::JSON::toString(sessionToJson(recovered),true)==edited,
                "recovery changed project or media references");

        juce::String error;std::atomic<float> progress{0};
        const auto output=root.getChildFile("Workflow-48k.wav");
        require(renderSessionWav(session,output,error,48000,1,-1,0,0,&progress),"48k bounce failed");
        require(progress.load()==1.0f,"bounce incomplete");
        const auto bounced=readWav(output,48000,120000);
        near(bounced.getSample(0,6000),0,"trimmed lead-in must be silent");
        near(bounced.getSample(0,24000),0.2f,"first clip timeline/gain");
        near(bounced.getSample(1,24000),-0.0375f,"first clip stereo/pan");
        near(bounced.getSample(0,60000),0,"split/move gap must be silent");
        const float clipGain=std::pow(10.0f,-6.0f/20.0f);
        near(bounced.getSample(0,84000),-0.1f*clipGain,"second clip source offset/gain");
        near(bounced.getSample(1,84000),0.1125f*clipGain,"second clip stereo/pan");
        near(bounced.getSample(0,72000),0,"fade-in endpoint");
        near(bounced.getSample(0,119999),0,"fade-out endpoint");
        const auto converted=root.getChildFile("Workflow-44k1.wav");
        require(renderSessionWav(session,converted,error,44100),"44k1 bounce failed");
        const auto resampled=readWav(converted,44100,110250);
        near(resampled.getSample(0,22050),0.2f,"resampled timeline/gain");
        near(resampled.getSample(1,77175),0.1125f*clipGain,"resampled stereo/source offset");
        juce::MemoryBlock after;require(take.loadFileAsData(after) && after==original,"source WAV modified");
        juce::MemoryBlock outputBefore;require(output.loadFileAsData(outputBefore),"bounce snapshot");
        require(!renderSessionWav(session,output,error),"existing bounce overwritten");
        juce::MemoryBlock outputAfter;
        require(output.loadFileAsData(outputAfter) && outputAfter==outputBefore,"existing bounce changed on rejection");
        std::cout << "PASS: synthetic capture -> split/trim/move/mix -> undo/redo -> save/reopen/recover"
                     " -> stereo 48k/44k1 WAV, source preserved\n";
        if(argc==2) std::cout << "Evidence: " << root.getFullPathName() << '\n';
        else require(root.deleteRecursively(),"temporary evidence cleanup");
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';return 1;
    }
}
