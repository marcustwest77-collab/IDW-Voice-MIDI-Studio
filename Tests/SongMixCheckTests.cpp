#include "SongExportJob.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool near(float a,float b){return std::abs(a-b)<.000002f;}
static juce::AudioBuffer<float> render(SongStudio& song){check(song.start(false),"Start");juce::AudioBuffer<float> buffer(2,96000);buffer.clear();song.process(buffer);song.stop();return buffer;}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-takes-"+juce::Uuid().toString());root.createDirectory();try{
    const auto wav=root.getChildFile("source.wav"),project=root.getChildFile("takes.idwsong"),bad=root.getChildFile("bad.idwsong");
    {juce::WavAudioFormat format;std::unique_ptr<juce::OutputStream> output=wav.createOutputStream();auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));check(writer!=nullptr,"Writer");juce::AudioBuffer<float> audio(2,48000);for(int ch=0;ch<2;++ch)for(int i=0;i<48000;++i)audio.setSample(ch,i,.1f);check(writer->writeFromAudioSampleBuffer(audio,0,48000),"Fixture");}

    SongStudio song;song.prepare(48000);auto pattern=song.pattern();pattern.bpm=240;pattern.bars=2;pattern.rows.fill(0);pattern.trackGain=1;song.setPattern(pattern);song.setMetronomeEnabled(true);song.setCountInBars(2);
    SongStudio::MixCheck report;check(song.checkMix(report).wasOk()&&report.peaks[5]==0&&report.totalFrames==96000&&report.summary().contains("mix is silent"),"Silent check/cue exclusion");
    for(int lane=0;lane<4;++lane)check(song.addWav(lane,wav,.25).wasOk(),"Sources");check(song.checkMix(report).wasOk(),"Check");for(int lane=0;lane<4;++lane)check(near(report.peaks[(size_t)lane],.1f),"Per-lane peak");check(near(report.peaks[5],.4f)&&report.limitedFrames==0&&report.firstLimitSeconds==-1,"Unrestricted mix");
    check(song.exportWav(root.getChildFile("mix.wav")).wasOk(),"Export");{juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(root.getChildFile("mix.wav").createInputStream().release(),true));juce::AudioBuffer<float> audio(2,96000);check(reader&&reader->read(&audio,0,96000,0,true,true)&&near(audio.getMagnitude(0,96000),report.peaks[5]),"Export peak parity below limiting");}
    for(int i=0;i<6;++i)check(song.duplicateClip(0,0,.25),"Overload copies");check(song.checkMix(report).wasOk()&&report.peaks[5]>.94f&&report.limitedFrames==48000&&std::abs(report.firstLimitSeconds-.25)<.000001,"Limiter count or first location");
    auto mute=song.clips(0)[0].settings;mute.mute=true;song.setClipSettings(0,0,mute);check(song.checkMix(report).wasOk()&&report.peaks[0]==0&&near(report.peaks[5],.3f)&&report.limitedFrames==0,"Track mute scan");
    std::atomic<bool> cancelled{true};std::atomic<float> progress{0};const auto oldPeak=report.peaks[5];check(song.checkMix(report,&cancelled,&progress).failed()&&report.peaks[5]==oldPeak,"Cancelled check published partial result");
    const auto revision=song.audioRevision();for(float peak:song.consumeMeterPeaks())check(peak==0,"Scan polluted live meters");std::uint64_t stamp=0;{SongExportJob job(song.recoverySnapshot(stamp),{},false,true);check(job.start(),"Check worker");song.removeLane(1);const auto deadline=juce::Time::getMillisecondCounter()+10000;while(!job.finished()&&juce::Time::getMillisecondCounter()<deadline)juce::Thread::sleep(1);check(job.finished()&&job.result().wasOk()&&job.fraction()==1&&job.levelSummary().contains("Backing: -20.0 dBFS"),"Worker snapshot/report");}
    check(song.audioRevision()==revision+1,"Scan changed revision");check(song.start(false)&&song.checkMix(report).failed(),"Transport lock");song.stop();
    std::cout<<"PASS full-song scan silence, cues excluded, per-track peaks, export parity, limiter frames/time, mute, cancellation preservation, worker snapshot and meter isolation\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
