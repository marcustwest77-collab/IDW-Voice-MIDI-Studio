#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool near(float a,float b){return std::abs(a-b)<.000002f;}
static juce::AudioBuffer<float> render(SongStudio& song){check(song.start(false),"Start");juce::AudioBuffer<float> buffer(2,96000);buffer.clear();song.process(buffer);song.stop();return buffer;}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-takes-"+juce::Uuid().toString());root.createDirectory();try{
    const auto wav=root.getChildFile("source.wav"),project=root.getChildFile("takes.idwsong"),bad=root.getChildFile("bad.idwsong");
    {juce::WavAudioFormat format;std::unique_ptr<juce::OutputStream> output=wav.createOutputStream();auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));check(writer!=nullptr,"Writer");juce::AudioBuffer<float> audio(2,48000);for(int ch=0;ch<2;++ch)for(int i=0;i<48000;++i)audio.setSample(ch,i,.1f);check(writer->writeFromAudioSampleBuffer(audio,0,48000),"Fixture");}

    SongStudio song;song.prepare(48000);auto pattern=song.pattern();pattern.bpm=240;pattern.bars=2;pattern.rows.fill(0);pattern.trackGain=.5;song.setPattern(pattern);
    for(int lane=0;lane<4;++lane){check(song.addWav(lane,wav,0).wasOk(),"Meter fixture");auto settings=song.clips(lane)[0].settings;settings.gain=.5;settings.pan=lane*.1f;song.setClipSettings(lane,0,settings);}
    auto audio=render(song);auto peaks=song.consumeMeterPeaks();for(int lane=0;lane<4;++lane)check(near(peaks[(size_t)lane],.025f),"Track meter post gain/pan");check(near(peaks[4],0)&&near(peaks[5],.1f),"Drum/mix meter");for(float value:song.consumeMeterPeaks())check(value==0,"Meter exchange did not reset");
    check(song.setClipMuted(0,0,true),"Mute");auto solo=song.clips(1)[0].settings;solo.solo=true;song.setClipSettings(1,0,solo);render(song);peaks=song.consumeMeterPeaks();check(peaks[0]==0&&peaks[2]==0&&peaks[3]==0&&near(peaks[1],.025f)&&near(peaks[5],.025f),"Mute/solo meter");
    auto revision=song.audioRevision();song.exportWav(wav);for(float value:song.consumeMeterPeaks())check(value==0,"Offline export polluted live meters");check(song.audioRevision()==revision,"Meters changed project revision");
    SongStudio live;live.prepare(48000);pattern.trackGain=1;live.setPattern(pattern);check(live.armLane(3)&&live.start(true),"Record start");juce::AudioBuffer<float> input(1,128);for(int i=0;i<128;++i)input.setSample(0,i,.2f);live.process(input);live.stop();peaks=live.consumeMeterPeaks();check(near(peaks[3],.2f)&&near(peaks[5],.2f),"Mono live recording meter");
    SongStudio count;count.prepare(48000);count.setPattern(pattern);count.setCountInBars(1);check(count.start(true),"Count-in start");input.clear();count.process(input);count.stop();peaks=count.consumeMeterPeaks();check(peaks[0]==0&&peaks[4]==0&&peaks[5]>0,"Count-in output cue meter");
    SongStudio loud;loud.prepare(48000);loud.setPattern(pattern);check(loud.addWav(0,root.getChildFile("source.wav"),0).wasOk(),"Loud fixture"); // source was overwritten with exported mix; use its current measured signal.
    for(int i=0;i<50;++i)check(loud.duplicateClip(0,0,0),"Loud copies");audio=render(loud);peaks=loud.consumeMeterPeaks();check(peaks[0]>1&&peaks[5]>1&&audio.getMagnitude(0,audio.getNumSamples())<1,"Meters must measure before output limiter");
    std::cout<<"PASS track/master meter gain, pan, mute/solo, interval reset, offline isolation, mono recording, count-in cues and pre-limiter overload\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
