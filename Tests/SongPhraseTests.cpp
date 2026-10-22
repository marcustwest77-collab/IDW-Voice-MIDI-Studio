#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool near(float a,float b){return std::abs(a-b)<.000002f;}
static juce::AudioBuffer<float> render(SongStudio& song){check(song.start(false),"Start");juce::AudioBuffer<float> buffer(2,96000);buffer.clear();song.process(buffer);song.stop();return buffer;}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-takes-"+juce::Uuid().toString());root.createDirectory();try{
    const auto wav=root.getChildFile("source.wav"),project=root.getChildFile("takes.idwsong"),bad=root.getChildFile("bad.idwsong");
    {juce::WavAudioFormat format;std::unique_ptr<juce::OutputStream> output=wav.createOutputStream();auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));check(writer!=nullptr,"Writer");juce::AudioBuffer<float> audio(2,48000);for(int ch=0;ch<2;++ch)for(int i=0;i<48000;++i)audio.setSample(ch,i,.1f+i*.000001f);check(writer->writeFromAudioSampleBuffer(audio,0,48000),"Fixture");}

    for(int lane=0;lane<4;++lane){SongStudio song;song.prepare(48000);auto pattern=song.pattern();pattern.bpm=240;pattern.bars=2;pattern.rows.fill(0);pattern.trackGain=1;song.setPattern(pattern);
        check(song.addWav(lane,wav,0).wasOk()&&song.duplicateClip(lane,0,.25),"Fixture clips");auto phrase=song.clips(lane)[1].settings;phrase.trimIn=.25;phrase.trimOut=.75;check(song.setClipSettings(lane,1,phrase),"Phrase trim");
        auto mixer=song.clips(lane)[0].settings;mixer.gain=.5f;mixer.pan=.25f;mixer.fadeIn=.1;mixer.fadeOut=.1;check(song.setClipSettings(lane,0,mixer),"Mixer/fades");
        check(song.setClipMuted(lane,1,true),"Mute selected fixture");auto expected=render(song);std::uint64_t stamp=0;auto recovery=song.recoverySnapshot(stamp);
        check(song.selectPhrase(lane,1),"Select phrase");auto clips=song.clips(lane);check(clips.size()==4&&!clips[0].settings.clipMuted&&!clips[1].settings.clipMuted&&clips[2].settings.clipMuted&&!clips[3].settings.clipMuted,"Phrase segment states");
        check(clips[0].settings.fadeIn==.1&&clips[3].settings.fadeOut==.1&&clips[0].settings.fadeOut==0&&clips[3].settings.fadeIn==0,"Outer fades preserved");
        check(clips[0].audio==clips[2].audio&&clips[2].audio==clips[3].audio&&clips[2].settings.start==.25&&clips[3].settings.start==.75,"Source sharing and timing");
        auto actual=render(song);for(int ch=0;ch<2;++ch)for(int i=0;i<96000;++i)check(near(actual.getSample(ch,i),expected.getSample(ch,i)),"Phrase mix lost surrounding audio or doubled overlap");
        check(recovery&&recovery->clips(lane).size()==2&&recovery->clips(lane)[1].settings.clipMuted,"Snapshot changed");check(song.undoTrackEdit()&&song.clips(lane).size()==2&&song.clips(lane)[1].settings.clipMuted&&song.redoTrackEdit()&&song.clips(lane).size()==4,"Atomic undo/redo");
        check(song.save(project).wasOk(),"Save comp");SongStudio loaded;loaded.prepare(48000);check(loaded.load(project).wasOk()&&loaded.clips(lane).size()==4,"Load comp");auto reopened=render(loaded);for(int i=0;i<96000;++i)check(near(actual.getSample(0,i),reopened.getSample(0,i)),"Comp roundtrip sound");
        check(song.start(false)&&!song.selectPhrase(lane,1),"Transport lock");song.stop();check(!song.selectPhrase(-1,0)&&!song.selectPhrase(lane,99),"Index validation");
        check(song.undoTrackEdit(),"Undo before rejection");mixer=song.clips(lane)[0].settings;mixer.fadeIn=.5;check(song.setClipSettings(lane,0,mixer),"Fade fixture");const auto revision=song.audioRevision();check(!song.selectPhrase(lane,1)&&song.clips(lane).size()==2&&song.audioRevision()==revision&&song.clips(lane)[1].settings.clipMuted,"Fade rejection partially applied");
    }
    SongStudio limits;limits.prepare(48000);check(limits.addWav(0,wav,0).wasOk()&&limits.duplicateClip(0,0,.25),"Limit fixture");auto p=limits.clips(0)[1].settings;p.trimOut=.5;limits.setClipSettings(0,1,p);for(int i=2;i<64;++i)check(limits.duplicateClip(0,0,2),"Limit copies");const auto revision=limits.audioRevision();check(!limits.selectPhrase(0,1)&&limits.clips(0).size()==64&&limits.audioRevision()==revision,"Clip limit partial edit");
    SongStudio primary;primary.prepare(48000);check(primary.addWav(0,wav,.25).wasOk()&&primary.duplicateClip(0,0,0),"Primary fixture");p=primary.clips(0)[0].settings;p.trimOut=.5;primary.setClipSettings(0,0,p);check(primary.selectPhrase(0,0)&&primary.clips(0).size()==4&&!primary.clips(0)[0].settings.clipMuted,"Primary phrase");
    SongStudio tiny;tiny.prepare(48000);tiny.addWav(0,wav,0);tiny.duplicateClip(0,0,.000001);p=tiny.clips(0)[1].settings;p.trimOut=.5;tiny.setClipSettings(0,1,p);const auto tinyRevision=tiny.audioRevision();check(!tiny.selectPhrase(0,1)&&tiny.audioRevision()==tinyRevision&&tiny.clips(0).size()==2,"Subsample rejection changed song");
    std::cout<<"PASS phrase selection on four tracks, preserved surrounding audio/fades/mixer, shared sources, atomic undo/recovery, persistence, primary selection and rejection rollback\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
