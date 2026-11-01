#include "TakeRecorder.h"
#include "IsolatedAudioCallback.h"
#include "SessionRecovery.h"
#include "OfflineBounce.h"
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>

// Wall-clock-paced synthetic callbacks, not physical-device/latency qualification.
static void require(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}
static float sample(std::int64_t frame,int channel) {
    // Exactly representable in PCM24. Changing frame/channel patterns expose
    // dropped, duplicated, reordered or swapped samples anywhere in a long take.
    const auto code=(frame*17+frame/997*31+channel*113)%1024;
    return static_cast<float>(code-512)/2048.0f;
}
static void verifyTake(const juce::File& file,int rate,std::int64_t frames) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    require(reader && reader->sampleRate==rate && reader->numChannels==2 &&
            reader->lengthInSamples==frames,"long take format/frame count mismatch");
    juce::AudioBuffer<float> data(2,4096);
    for(std::int64_t position=0;position<frames;position+=4096) {
        const int count=static_cast<int>(std::min<std::int64_t>(4096,frames-position));
        require(reader->read(&data,0,count,position,true,true),"long take read failed");
        for(int channel=0;channel<2;++channel)
            for(int i=0;i<count;++i)
                require(std::abs(data.getSample(channel,i)-sample(position+i,channel))<0.000001f,
                        "long take contains changed/missing/reordered/channel-swapped audio");
    }
}
struct DiskLoad {
    std::atomic<bool> canceled{false};
    std::exception_ptr failure;
    std::thread worker;
    ~DiskLoad() {canceled.store(true);if(worker.joinable())worker.join();}
    void finish() {worker.join();if(failure)std::rethrow_exception(failure);}
};
static void run(const juce::File& folder,int rate,int seconds) {
    require(folder.createDirectory().wasOk(),"rate directory");
    const auto seed=folder.getChildFile("disk-load-source.wav");
    {
        IDWTakeRecorder recorder;
        require(recorder.start(seed,rate,2),"disk-load seed start");
        juce::AudioBuffer<float> input(2,4096);
        juce::AudioIODeviceCallbackContext context{};
        for(int i=0;i<4096;++i) {
            input.setSample(0,i,0.125f);input.setSample(1,i,-0.0625f);
        }
        recorder.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),2,nullptr,0,4096,context);
        recorder.stop();require(recorder.getQueuedFrames()==4096,"disk-load seed capture");
    }
    const std::int64_t frames=static_cast<std::int64_t>(rate)*seconds;
    std::atomic<std::int64_t> produced{0};
    const auto destination=folder.getChildFile("extended-take.wav");
    IDWTakeRecorder recorder;
    IDWIsolatedAudioCallback isolated(recorder);
    require(recorder.start(destination,rate,2,1),"long recording start");
    DiskLoad load;
    load.worker=std::thread([&] {
        try {
            using namespace idw::daw;
            Session snapshot;snapshot.setTempo(120);require(snapshot.addTrack(TrackType::audio),"load track");
            require(snapshot.addClip(0,{seed.getFullPathName().toStdString(),0,0.125,0}),"load clip");
            const auto project=folder.getChildFile("Snapshot.idwproj");
            SessionRecovery recovery(folder.getChildFile("Recovery"));recovery.begin(snapshot,true);
            for(int revision=1;revision<=12;++revision) {
                while(produced.load()<frames*revision/12 && !load.canceled.load())juce::Thread::sleep(1);
                if(load.canceled.load())return;
                require(snapshot.setTrackMix(0,0.5f+static_cast<float>(revision)*0.025f,0),"snapshot mix");
                require(saveSession(snapshot,project),"snapshot save under disk load");
                Session reopened;
                require(loadSession(project,reopened) &&
                        juce::JSON::toString(sessionToJson(reopened))==juce::JSON::toString(sessionToJson(snapshot)),
                        "snapshot save/reopen under disk load");
                require(recovery.checkpoint(snapshot,project)==SessionRecovery::Result::saved,"recovery under disk load");
                const auto copies=recovery.copies();Session recovered;juce::File origin;
                require(!copies.empty() && copies.size()<=2 && SessionRecovery::read(copies.front(),recovered,origin) && origin==project &&
                        juce::JSON::toString(sessionToJson(recovered))==juce::JSON::toString(sessionToJson(snapshot)),
                        "recovery readability under disk load");
                juce::String error;
                const auto bounce=folder.getChildFile("snapshot-"+juce::String(revision)+".wav");
                require(renderSessionWav(reopened,bounce,error,rate),"independent snapshot bounce under disk load");
                juce::AudioFormatManager formats;formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(bounce));
                require(reader && reader->numChannels==2 && reader->sampleRate==rate &&
                        reader->lengthInSamples==static_cast<juce::int64>(std::ceil(rate*0.0625)),
                        "snapshot bounce format/duration");
                juce::AudioBuffer<float> bounced(2,64);
                require(reader->read(&bounced,0,64,0,true,true),"snapshot bounce read");
                for(int channel=0;channel<2;++channel)
                    for(int i=0;i<64;++i)
                        require(std::abs(bounced.getSample(channel,i)-
                            (channel==0?0.125f:-0.0625f)*snapshot.tracks[0].gain)<0.000001f,
                            "snapshot bounce signal under disk load");
            }
        } catch(...) {load.failure=std::current_exception();}
    });
    juce::AudioBuffer<float> input(3,4096),output(2,4096);
    juce::AudioIODeviceCallbackContext context{};
    const auto captureStart=std::chrono::steady_clock::now();
    for(std::int64_t position=0;position<frames;position+=4096) {
        const int count=static_cast<int>(std::min<std::int64_t>(4096,frames-position));
        for(int i=0;i<count;++i) {
            input.setSample(0,i,0.875f); // Unselected input must not leak into the recording.
            input.setSample(1,i,sample(position+i,0));input.setSample(2,i,sample(position+i,1));
            output.setSample(0,i,0.75f);output.setSample(1,i,-0.75f);
        }
        isolated.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),3,
            output.getArrayOfWritePointers(),2,count,context);
        for(int channel=0;channel<2;++channel)
            for(int i=0;i<count;++i)require(output.getSample(channel,i)==0,"capture output leaked under load");
        produced.store(position+count);
        // Respect the writer's actual time budget. Accelerating a producer can
        // overflow its bounded queue even when ordinary realtime capture is healthy.
        const auto due=captureStart+std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(static_cast<double>(position+count)/rate));
        std::this_thread::sleep_until(due);
    }
    load.finish();recorder.stop();
    std::cout << "Capture counters: received=" << recorder.getCapturedFrames()
              << " queued=" << recorder.getQueuedFrames() << " dropped=" << recorder.getDroppedBlocks()
              << " rejected=" << recorder.getRejectedWrites() << " missing=" << recorder.getMissingInputBlocks()
              << " interrupted=" << recorder.wasDeviceInterrupted() << '\n';
    require(recorder.getCapturedFrames()==static_cast<std::uint64_t>(frames) &&
            recorder.getQueuedFrames()==static_cast<std::uint64_t>(frames) &&
            recorder.getDroppedBlocks()==0 && recorder.getRejectedWrites()==0 &&
            recorder.getMissingInputBlocks()==0 && !recorder.wasDeviceInterrupted(),
            "long capture dropped/rejected/interrupted audio");
    verifyTake(destination,rate,frames);
    // Reuse the same recorder after a long take; counters and file ownership reset.
    const auto restart=folder.getChildFile("restart.wav");
    require(recorder.start(restart,rate,2,1),"recorder restart after long take");
    for(int i=0;i<257;++i) {
        input.setSample(1,i,sample(i,0));input.setSample(2,i,sample(i,1));
    }
    isolated.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),3,nullptr,0,257,context);
    recorder.stop();require(recorder.getQueuedFrames()==257 && recorder.getCapturedFrames()==257,"restart counters");
    verifyTake(restart,rate,257);
    std::cout << "PASS: " << seconds << " synthetic seconds at " << rate
              << " Hz stereo; " << frames << " frames verified, 12 disk-load snapshots/exports, restart clean\n";
}
int main(int argc,char** argv) {
    try {
        int seconds=10;juce::String evidence;
        for(int i=1;i<argc;++i) {
            const juce::String option(argv[i]);
            require(i+1<argc,"usage: ExtendedRecordingTests [--seconds 10..1800] [--evidence fresh-directory]");
            const juce::String value(argv[++i]);
            if(option=="--seconds") {
                require(value.isNotEmpty() && value.containsOnly("0123456789") && value.length()<=4,"invalid seconds");
                seconds=value.getIntValue();require(seconds>=10 && seconds<=1800,"seconds out of bounds");
            } else if(option=="--evidence") {require(value.isNotEmpty(),"empty evidence path");evidence=value;}
            else require(false,"unknown option");
        }
        const auto root=evidence.isEmpty() ? juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("idw-extended-recording","",false) :
            juce::File::getCurrentWorkingDirectory().getChildFile(evidence);
        require(!root.exists() && root.createDirectory().wasOk(),"evidence directory must be new");
        const auto start=std::chrono::steady_clock::now();
        run(root.getChildFile("48000"),48000,seconds);run(root.getChildFile("44100"),44100,seconds);
        const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout << "Clock-paced engine check: " << elapsed << " wall seconds; no hardware/latency guarantee\n";
        if(evidence.isEmpty())require(root.deleteRecursively(),"temporary test cleanup");
        else std::cout << "Evidence: " << root.getFullPathName() << '\n';
        return 0;
    } catch(const std::exception& error) {std::cerr << "FAIL: " << error.what() << '\n';return 1;}
}
