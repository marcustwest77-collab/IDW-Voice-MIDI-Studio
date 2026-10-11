#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool near(float a,float b){return std::abs(a-b)<.000002f;}
static juce::AudioBuffer<float> render(SongStudio& song){check(song.start(false),"Start");juce::AudioBuffer<float> buffer(2,96000);buffer.clear();song.process(buffer);song.stop();return buffer;}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-takes-"+juce::Uuid().toString());root.createDirectory();try{
    const auto wav=root.getChildFile("source.wav"),project=root.getChildFile("takes.idwsong"),bad=root.getChildFile("bad.idwsong");
    {juce::WavAudioFormat format;std::unique_ptr<juce::OutputStream> output=wav.createOutputStream();auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));check(writer!=nullptr,"Writer");juce::AudioBuffer<float> audio(2,48000);for(int ch=0;ch<2;++ch)for(int i=0;i<48000;++i)audio.setSample(ch,i,.1f);check(writer->writeFromAudioSampleBuffer(audio,0,48000),"Fixture");}

    SongStudio song;song.prepare(48000);auto pattern=song.pattern();pattern.bpm=240;pattern.bars=2;pattern.rows.fill(0);pattern.trackGain=.5;check(song.setPattern(pattern),"Pattern");
    for(int lane=0;lane<4;++lane){check(song.addWav(lane,wav,lane*.1).wasOk(),"Import lane");auto settings=song.clips(lane)[0].settings;settings.gain=.5;settings.pan=lane*.1f;settings.fadeIn=.02;settings.fadeOut=.03;check(song.setClipSettings(lane,0,settings),"Mixer");}
    check(song.duplicateClip(0,0,.5)&&song.setClipMuted(0,1,true),"Muted alternative");
    const auto folder=root.getChildFile("stems");const auto revision=song.audioRevision();check(song.exportStems(folder).wasOk()&&song.audioRevision()==revision,"Stem export mutated model");
    const char* names[]{"01-Lead.wav","02-Backing.wav","03-Double.wav","04-Ad-lib.wav","05-Drum-pattern.wav"};
    auto read=[&](const juce::File& file){juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(file.createInputStream().release(),true));check(reader&&reader->sampleRate==48000&&reader->bitsPerSample==24&&reader->numChannels==2&&reader->lengthInSamples==96000,"Stem format/alignment");juce::AudioBuffer<float> audio(2,96000);check(reader->read(&audio,0,96000,0,true,true),"Read WAV");return audio;};
    juce::AudioBuffer<float> sum(2,96000);sum.clear();for(int stem=0;stem<5;++stem){auto audio=read(folder.getChildFile(names[stem]));for(int ch=0;ch<2;++ch)sum.addFrom(ch,0,audio,ch,0,96000);if(stem<4){check(near(audio.getSample(1,(int)((.2+stem*.1)*48000)),.025f),"Wrong lane or gain");if(stem>0)check(audio.getSample(0,100)==0,"Lost initial silence");}else check(audio.getMagnitude(0,96000)==0,"Empty drums not silent");}
    check(song.exportWav(wav).wasOk(),"Mix export");auto mix=read(wav);for(int ch=0;ch<2;++ch)for(int i=0;i<96000;++i)check(near(sum.getSample(ch,i),mix.getSample(ch,i)),"Stems do not sum to unclipped mix");
    check(folder.getChildFile("IMPORT-INSTRUCTIONS.txt").loadFileAsString().contains("240 BPM"),"Missing tempo instructions");check(song.exportStems(folder).failed()&&folder.getChildFile(names[0]).existsAsFile(),"Existing export overwritten");check(song.exportStems(root.getChildFile("missing/child")).failed(),"Missing parent accepted");
    auto solo=song.clips(2)[0].settings;solo.solo=true;song.setClipSettings(2,0,solo);const auto soloFolder=root.getChildFile("solo");check(song.exportStems(soloFolder).wasOk(),"Solo export");for(int stem=0;stem<5;++stem){auto audio=read(soloFolder.getChildFile(names[stem]));check((audio.getMagnitude(0,96000)>0)==(stem==2),"Solo or beat suppression incorrect");}
    check(song.start(false)&&song.exportStems(root.getChildFile("active")).failed()&&!root.getChildFile("active").exists(),"Active transport export");song.stop();
    SongStudio drums;drums.prepare(48000);pattern.rows[0]=1;pattern.beatGain=.1;drums.setPattern(pattern);const auto drumFolder=root.getChildFile("drums");check(drums.exportStems(drumFolder).wasOk()&&drums.exportWav(wav).wasOk(),"Drum export");auto beat=read(drumFolder.getChildFile(names[4]));mix=read(wav);check(beat.getMagnitude(0,96000)>0,"Drums missing");for(int i=0;i<96000;++i)check(near(beat.getSample(0,i),mix.getSample(0,i)),"Drum timing differs from mix");
    std::cout<<"PASS aligned 24-bit stems, isolated tracks/drums, fades/pan/master levels, muted takes, solo, exact low-level mix sum, tempo notes and overwrite/transport protection\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
