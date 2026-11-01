#include "MultiTakeRecorder.h"
#include "IsolatedAudioCallback.h"
#include "ClipPlayback.h"
#include "SessionPersistence.h"
#include "OfflineBounce.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static float inputSample(int frame,int channel) {
    return static_cast<float>((frame*3+frame/257*11+channel*29)%128-64)/1024.0f;
}
static float backingSample(int frame,int rate,int channel) {
    const float level=(frame/(rate/4))%2==0?0.125f:0.0625f;
    return channel==0?level:-level*0.5f;
}
static void makeBacking(const juce::File& file,int rate) {
    juce::WavAudioFormat wav;
    auto stream=std::unique_ptr<juce::FileOutputStream>(file.createOutputStream());
    require(stream!=nullptr,"backing stream");
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(),rate,2,24,{},0));
    require(writer!=nullptr,"backing writer");stream.release();
    juce::AudioBuffer<float> buffer(2,4096);
    for(int position=0;position<3*rate;position+=4096) {
        const int count=std::min(4096,3*rate-position);
        for(int channel=0;channel<2;++channel)
            for(int i=0;i<count;++i)buffer.setSample(channel,i,backingSample(position+i,rate,channel));
        require(writer->writeFromFloatArrays(buffer.getArrayOfReadPointers(),2,count),"backing samples");
    }
}
static void verifyMono(const juce::File& file,int rate,int frames,int channel) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    require(reader && reader->numChannels==1 && reader->sampleRate==rate &&
            reader->lengthInSamples==frames,"simultaneous take length/rate/channel mismatch");
    juce::AudioBuffer<float> buffer(1,4096);
    for(int position=0;position<frames;position+=4096) {
        const int count=std::min(4096,frames-position);
        require(reader->read(&buffer,0,count,position,true,false),"take read");
        for(int i=0;i<count;++i)
            require(std::abs(buffer.getSample(0,i)-inputSample(position+i,channel))<0.000001f,
                    "simultaneous take lost alignment or routed the wrong input");
    }
}
static void run(const juce::File& root,int rate,int tracks) {
    using namespace idw::daw;
    require(root.createDirectory().wasOk(),"case directory");
    const auto backing=root.getChildFile("backing.wav");makeBacking(backing,rate);
    juce::MemoryBlock original;require(backing.loadFileAsData(original),"backing snapshot");
    std::vector<IDWMultiTakeRecorder::Input> destinations;
    for(int slot=0;slot<tracks;++slot)
        destinations.push_back({root.getChildFile("take-"+juce::String(slot)+".wav"),tracks-1-slot});
    IDWMultiTakeRecorder recorder;IDWIsolatedAudioCallback isolated(recorder);
    require(recorder.start(destinations,rate),"simultaneous recording start");
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(backing));
    require(reader!=nullptr,"backing reader");
    juce::AudioFormatReaderSource source(reader.release(),true);
    juce::AudioTransportSource transport;
    constexpr int block=257;
    transport.setSource(&source,0,nullptr,rate);transport.prepareToPlay(block,rate);transport.start();
    juce::AudioBuffer<float> input(tracks<8?tracks+1:tracks,block),captureOutput(2,block),playback(2,block);
    Clip clip{backing.getFullPathName().toStdString(),0,6,0};
    juce::int64 next=-1;juce::AudioIODeviceCallbackContext context{};
    const int frames=2*rate;
    const auto start=std::chrono::steady_clock::now();
    for(int position=0;position<frames;position+=block) {
        const int count=std::min(block,frames-position);
        for(int i=0;i<count;++i) {
            if(tracks<8)input.setSample(tracks,i,0.875f); // Unselected extra input.
            for(int channel=0;channel<tracks;++channel)
                input.setSample(channel,i,inputSample(position+i,channel));
            captureOutput.setSample(0,i,0.75f);captureOutput.setSample(1,i,-0.75f);
        }
        const auto window=readClipWindow(transport,playback,next,clip,120,rate,position,count);
        require(window.frames==count && window.offset==0,"backing playback window");
        isolated.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),input.getNumChannels(),
            captureOutput.getArrayOfWritePointers(),2,count,context);
        // Same independent-callback summation contract as AudioDeviceManager.
        // Monitoring is off: simultaneous capture must neither erase nor double playback.
        for(int channel=0;channel<2;++channel)
            for(int i=0;i<count;++i) {
                require(captureOutput.getSample(channel,i)==0,"unrequested capture monitoring/stale output");
                require(std::abs(playback.getSample(channel,i)+captureOutput.getSample(channel,i)-
                    backingSample(position+i,rate,channel))<0.000001f,"backing changed during simultaneous capture");
            }
        const auto due=start+std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(static_cast<double>(position+count)/rate));
        std::this_thread::sleep_until(due);
    }
    recorder.stop();transport.setSource(nullptr);transport.releaseResources();
    require(recorder.getDroppedBlocks()==0 && recorder.getRejectedWrites()==0 &&
            !recorder.wasDeviceInterrupted(),"simultaneous recording dropped/rejected/interrupted");
    Session session;session.setTempo(120);require(session.addTrack(TrackType::audio),"backing track");
    require(session.addClip(0,clip),"backing timeline clip");
    for(int slot=0;slot<tracks;++slot) {
        verifyMono(destinations[static_cast<std::size_t>(slot)].file,rate,frames,tracks-1-slot);
        require(session.addTrack(TrackType::audio),"take track");
        session.tracks.back().inputChannel=tracks-1-slot;
        require(session.addClip(static_cast<std::size_t>(slot+1),{
            destinations[static_cast<std::size_t>(slot)].file.getFullPathName().toStdString(),1,4,0}),"take placement");
        require(session.setTrackMix(static_cast<std::size_t>(slot+1),0.2f,slot%2==0?-1.0f:1.0f),"take pan");
    }
    const auto project=root.getChildFile("Overdub.idwproj");
    require(saveSession(session,project),"multitrack project save");
    const auto snapshot=juce::JSON::toString(sessionToJson(session));session=Session{};
    require(loadSession(project,session) && juce::JSON::toString(sessionToJson(session))==snapshot,
            "multitrack project reopen changed routing/placement");
    const auto bounce=root.getChildFile("Overdub.wav");juce::String error;
    require(renderSessionWav(session,bounce,error,rate),"multitrack bounce");
    std::unique_ptr<juce::AudioFormatReader> mix(formats.createReaderFor(bounce));
    require(mix && mix->numChannels==2 && mix->sampleRate==rate && mix->lengthInSamples==3*rate,"mix format/duration");
    juce::AudioBuffer<float> buffer(2,4096);
    for(int position=0;position<3*rate;position+=4096) {
        const int count=std::min(4096,3*rate-position);
        require(mix->read(&buffer,0,count,position,true,true),"mix read");
        for(int i=0;i<count;++i)for(int channel=0;channel<2;++channel) {
            const int timeline=position+i;
            float expected=backingSample(timeline,rate,channel);
            const int takeFrame=timeline-rate/2;
            if(takeFrame>=0 && takeFrame<frames)
                for(int slot=0;slot<tracks;++slot)if(slot%2==channel)
                    expected+=inputSample(takeFrame,tracks-1-slot)*0.2f;
            require(std::abs(buffer.getSample(channel,i)-expected)<0.000002f,
                    "reopened overdub mix lost timing, stereo separation or source samples");
        }
    }
    juce::MemoryBlock after;require(backing.loadFileAsData(after) && after==original,"backing source modified");
    std::cout << "PASS: " << tracks << " simultaneous mono takes at " << rate
              << " Hz; every take/mix sample verified; playback, save/reopen and stereo bounce preserved\n";
}
int main(int argc,char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI initialiser;
        require(argc<=2,"usage: MultitrackOverdubTests [fresh-evidence-directory]");
        const auto root=argc==2?juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]):
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getNonexistentChildFile("idw-multitrack-overdub","",false);
        require(!root.exists() && root.createDirectory().wasOk(),"evidence directory must be new");
        run(root.getChildFile("two-inputs"),44100,2);run(root.getChildFile("eight-inputs"),48000,8);
        if(argc==1)require(root.deleteRecursively(),"temporary test cleanup");
        else std::cout << "Synthetic evidence: " << root.getFullPathName() << '\n';
        return 0;
    } catch(const std::exception& error) {std::cerr << "FAIL: " << error.what() << '\n';return 1;}
}
