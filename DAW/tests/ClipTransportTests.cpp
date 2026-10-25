#include "ClipPlayback.h"
#include "OfflineBounce.h"
#include "SessionHistory.h"
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
    juce::MemoryBlock originalSource;
    require(source.loadFileAsData(originalSource),"read original source bytes");
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
    // Exercise history through actual audio, not just metadata. Each curve
    // starts from a dry session, with independent outside fades on the pair.
    const auto bounceAudio=[&](const Session& session,const juce::File& output){
        juce::String error;require(renderSessionWav(session,output,error),"history bounce");
        juce::AudioFormatManager formats;formats.registerBasicFormats();
        auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(output));
        require(reader!=nullptr && reader->lengthInSamples==160,"history bounce length");
        juce::AudioBuffer<float> samples(2,160);
        require(reader->read(&samples,0,160,0,true,true),"history bounce samples");
        return samples;
    };
    for(auto curve:{FadeCurve::linear,FadeCurve::equalPower}){
        const auto historyRoot=root.getChildFile(curve==FadeCurve::linear?"history-linear":"history-power");
        require(historyRoot.createDirectory().wasOk(),"history fixture directory");
        Session session;session.bpm=60;session.addTrack(TrackType::audio);
        session.addClip(0,{path,0,128.0/44100,0});
        session.addClip(0,{path,64.0/44100,96.0/44100,16.0/44100});
        const auto dry=session;
        const auto baseline=bounceAudio(dry,historyRoot.getChildFile("history-dry.wav"));
        ClipEditHistory history;
        require(session.setClipFades(0,0,16.0/44100,0,curve,curve),"outside fade in");
        require(session.setClipFades(0,1,0,16.0/44100,curve,curve),"outside fade out");
        require(session.crossfadeClips(0,0,1,curve),"history crossfade");
        require(history.record(dry,session),"record audio edit");
        const auto faded=bounceAudio(session,historyRoot.getChildFile("history-faded.wav"));
        require(std::abs(faded.getSample(0,80)-baseline.getSample(0,80))>0.01f,
                "fade must audibly change fixture");
        require(faded.getSample(0,0)==0 && faded.getSample(0,159)==0,"outside fade endpoints");
        for(int block:{1,17,128,256})
            compare(playback(session,source,0,160,block),faded,0.00001f);
        require(history.undo(session)==ClipEditHistory::Result::applied,"audio undo");
        compare(bounceAudio(session,historyRoot.getChildFile("history-undo.wav")),baseline,0.00001f);
        compare(playback(session,source,0,160,17),baseline,0.00001f);
        require(history.redo(session)==ClipEditHistory::Result::applied,"audio redo");
        compare(bounceAudio(session,historyRoot.getChildFile("history-redo.wav")),faded,0.00001f);
        compare(playback(session,source,0,160,31),faded,0.00001f);
        juce::MemoryBlock currentSource;
        require(source.loadFileAsData(currentSource) && currentSource==originalSource,
                "fade/history/playback/export modified source WAV");
    }
    // An independent numeric oracle catches a gain omitted or applied twice in
    // both shared playback/export paths. The adjacent clip must remain unchanged.
    for(double db:{-60.0,-6.0,6.0,12.0}){
        const auto gainRoot=root.getChildFile("gain-"+juce::String(db,0));
        require(gainRoot.createDirectory().wasOk(),"gain fixtures");
        Session session;session.bpm=60;session.addTrack(TrackType::audio);
        session.addClip(0,{path,0,64.0/44100,0});
        session.addClip(0,{path,64.0/44100,96.0/44100,0});
        const auto dry=session;
        ClipEditHistory history;
        require(session.setClipGain(0,0,db) && history.record(dry,session),"gain edit");
        const auto exported=bounceAudio(session,gainRoot.getChildFile("gain.wav"));
        for(int ch=0;ch<2;++ch)for(int i=0;i<160;++i){
            const int sourceFrame=i<64?i:i-64;
            const float sourceValue=ch==0?0.1f+sourceFrame/1024.0f:-0.05f-sourceFrame/2048.0f;
            const double factor=i<64?std::pow(10.0,db/20.0):1.0;
            require(std::abs(exported.getSample(ch,i)-sourceValue*factor)<0.00001,
                    "clip gain numeric oracle or unaffected neighbor");
        }
        for(int block:{1,17,64,256})compare(playback(session,source,0,160,block),exported,0.00001f);
        require(history.undo(session)==ClipEditHistory::Result::applied,"gain undo");
        compare(bounceAudio(session,gainRoot.getChildFile("undo.wav")),
                playback(dry,source,0,160,17),0.00001f);
        require(history.redo(session)==ClipEditHistory::Result::applied,"gain redo");
        compare(bounceAudio(session,gainRoot.getChildFile("redo.wav")),exported,0.00001f);
        juce::MemoryBlock unchanged;
        require(source.loadFileAsData(unchanged) && unchanged==originalSource,"gain modified source file");
    }
    // Gains compose with both crossfade laws in the actual WAV readers.
    for(auto curve:{FadeCurve::linear,FadeCurve::equalPower}){
        auto gained=overlapSession;
        require(gained.crossfadeClips(0,0,1,curve) &&
                gained.setClipGain(0,0,-6) && gained.setClipGain(0,1,3),"gained crossfade");
        const auto expected=bounceAudio(gained,root.getChildFile(curve==FadeCurve::linear?"gain-linear.wav":"gain-power.wav"));
        for(int block:{1,17,64,256})compare(playback(gained,source,0,160,block),expected,0.00001f);
    }
    // Duplicating repeats exactly the same trimmed, faded, gained source segment.
    Session duplicate;duplicate.bpm=60;duplicate.addTrack(TrackType::audio);
    duplicate.addClip(0,{path,0,96.0/44100,32.0/44100,16.0/44100,16.0/44100,
                        FadeCurve::equalPower,FadeCurve::linear,-6.0});
    require(duplicate.duplicateClip(0,0),"duplicate audio clip");
    const auto duplicateWav=root.getChildFile("duplicate.wav");juce::String duplicateError;
    require(renderSessionWav(duplicate,duplicateWav,duplicateError),"duplicate export");
    {
        juce::AudioFormatManager formats;formats.registerBasicFormats();
        auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(duplicateWav));
        require(reader!=nullptr,"duplicate reader");
        juce::AudioBuffer<float> expected(2,192);
        require(reader->read(&expected,0,192,0,true,true),"duplicate export samples");
        require(expected.getMagnitude(0,192)>0.03f,"duplicate fixture silent");
        for(int ch=0;ch<2;++ch)for(int i=0;i<96;++i)
            require(std::abs(expected.getSample(ch,i)-expected.getSample(ch,96+i))<0.00001f,
                    "duplicate changed source segment or envelope");
        for(int block:{1,17,64,256})compare(playback(duplicate,source,0,192,block),expected,0.00001f);
        juce::MemoryBlock unchanged;
        require(source.loadFileAsData(unchanged) && unchanged==originalSource,"duplicate modified source file");
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
