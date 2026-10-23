#include "ClipPlayback.h"
#include "OfflineBounce.h"
#include <stdexcept>
#include <iostream>
#include <vector>
using namespace idw::daw;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void makeSource(const juce::File& file,double rate){
    juce::WavAudioFormat wav;
    auto stream=std::unique_ptr<juce::FileOutputStream>(file.createOutputStream());
    require(stream!=nullptr,"fixture stream");
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(),rate,2,24,{},0));
    require(writer!=nullptr,"fixture writer");stream.release();
    juce::AudioBuffer<float> audio(2,128);
    for(int i=0;i<128;++i){audio.setSample(0,i,0.1f+i/1024.0f);audio.setSample(1,i,-0.05f-i/2048.0f);}
    const float* channels[]{audio.getReadPointer(0),audio.getReadPointer(1)};
    require(writer->writeFromFloatArrays(channels,2,128),"fixture samples");
}
// Actual JUCE WAV reader and transport, with the same resampling setup as the app.
static juce::AudioBuffer<float> playback(const Session& session,const juce::File& source,
                                        int start,int frames,int blockSize){
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(source));
    require(reader!=nullptr,"playback reader");const auto sourceRate=reader->sampleRate;
    juce::AudioFormatReaderSource readerSource(reader.release(),true);
    juce::AudioTransportSource transport;
    transport.setSource(&readerSource,0,nullptr,sourceRate);
    transport.prepareToPlay(blockSize,44100);transport.start();
    juce::AudioBuffer<float> scratch(2,blockSize),result(2,frames);result.clear();
    juce::int64 next=-1;
    for(int pos=0;pos<frames;pos+=blockSize){
        const auto count=std::min(blockSize,frames-pos);
        for(const auto& clip:session.tracks[0].clips){
            const auto window=readClipWindow(transport,scratch,next,clip,session.bpm,44100,start+pos,count);
            for(int ch=0;ch<2;++ch)
                result.addFrom(ch,pos+window.offset,scratch,ch,0,window.frames);
        }
    }
    transport.setSource(nullptr);transport.releaseResources();return result;
}
static void compare(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,float tolerance){
    require(a.getNumSamples()==b.getNumSamples(),"different render lengths");
    for(int ch=0;ch<2;++ch)for(int i=0;i<a.getNumSamples();++i)
        if(!std::isfinite(a.getSample(ch,i)) || std::abs(a.getSample(ch,i)-b.getSample(ch,i))>tolerance)
            throw std::runtime_error("transport sample mismatch at "+std::to_string(i)+" channel "+std::to_string(ch));
}
int main(){
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("idw-clip-transport","",false);
    require(root.createDirectory().wasOk(),"fixture directory");
    const auto source=root.getChildFile("source.wav");makeSource(source,44100);
    Session repeated;repeated.bpm=60;repeated.addTrack(TrackType::audio);
    const auto path=source.getFullPathName().toStdString();
    require(repeated.addClip(0,{path,0,128.0/44100,0}),"first clip");
    require(repeated.addClip(0,{path,192.0/44100,128.0/44100,0}),"repeated clip");
    for(int size:{8,31,64,256}){
        auto actual=playback(repeated,source,0,320,size);
        for(int ch=0;ch<2;++ch){
            for(int i=0;i<128;++i){
                const float expected=ch==0?0.1f+i/1024.0f:-0.05f-i/2048.0f;
                require(std::abs(actual.getSample(ch,i)-expected)<0.00001f,"first clip/tail missing");
                require(std::abs(actual.getSample(ch,192+i)-expected)<0.00001f,"repeat after EOF missing");
            }
            for(int i=128;i<192;++i)require(actual.getSample(ch,i)==0,"gap not silent");
        }
    }
    // Explicit stop is distinct from EOF: a subsequent callback must not
    // restart the reader behind the user's back. No device callback runs here,
    // so JUCE stop may wait for its bounded timeout before this final read.
    {
        juce::AudioFormatManager formats;formats.registerBasicFormats();
        juce::AudioFormatReaderSource reader(formats.createReaderFor(source),true);
        juce::AudioTransportSource transport;
        transport.setSource(&reader,0,nullptr,44100);transport.prepareToPlay(8,44100);transport.start();
        juce::AudioBuffer<float> scratch(2,8);juce::int64 next=-1;
        const auto clip=repeated.tracks[0].clips[0];
        readClipWindow(transport,scratch,next,clip,60,44100,0,8);
        require(!transport.hasStreamFinished(),"stop test must precede EOF");
        transport.stop();
        readClipWindow(transport,scratch,next,clip,60,44100,8,8);
        require(!transport.isPlaying(),"explicit stop restarted");
        readClipWindow(transport,scratch,next,clip,60,44100,16,8);
        require(scratch.getMagnitude(0,8)==0,"stopped reader not silent");
        transport.setSource(nullptr);transport.releaseResources();
    }
    // Both clips share one transport. The outgoing clip reaches EOF during the
    // crossfade; the incoming clip must still seek/restart/read its own offset.
    Session overlapSession;overlapSession.bpm=60;overlapSession.addTrack(TrackType::audio);
    overlapSession.addClip(0,{path,0,128.0/44100,0});
    overlapSession.addClip(0,{path,64.0/44100,96.0/44100,16.0/44100});
    for(auto curve:{FadeCurve::linear,FadeCurve::equalPower}){
        require(overlapSession.crossfadeClips(0,0,1,curve),"crossfade fixture");
        const auto output=root.getChildFile(curve==FadeCurve::linear?"linear.wav":"power.wav");
        juce::String error;require(renderSessionWav(overlapSession,output,error),"crossfade bounce");
        juce::AudioFormatManager formats;formats.registerBasicFormats();
        auto bounceReader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(output));
        require(bounceReader!=nullptr,"bounce reader");
        juce::AudioBuffer<float> expected(2,160);
        require(bounceReader->read(&expected,0,160,0,true,true),"bounce samples");
        for(int size:{8,31,64,256})compare(playback(overlapSession,source,0,160,size),expected,0.00001f);
        juce::AudioBuffer<float> expectedSeek(2,67);
        for(int ch=0;ch<2;++ch)expectedSeek.copyFrom(ch,0,expected,ch,83,67);
        compare(playback(overlapSession,source,83,67,17),expectedSeek,0.00001f);
    }
    // Repeat at a different source rate. Compare repeated sections, not against
    // export's different resampling algorithm; both reads should be identical.
    const auto resampled=root.getChildFile("48k.wav");makeSource(resampled,48000);
    Session differentRate;differentRate.bpm=60;differentRate.addTrack(TrackType::audio);
    differentRate.addClip(0,{resampled.getFullPathName().toStdString(),0,128.0/48000,0});
    differentRate.addClip(0,{resampled.getFullPathName().toStdString(),192.0/44100,128.0/48000,0});
    const auto data=playback(differentRate,resampled,0,320,16);
    for(int ch=0;ch<2;++ch)for(int i=0;i<118;++i)
        require(std::abs(data.getSample(ch,i)-data.getSample(ch,192+i))<0.00001f,"resampled repeat differs");
    require(data.getMagnitude(0,192,100)>0.05f,"resampled repeat silent");
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS: actual clip transport EOF, overlap, seek and export parity tests\n";
}
