#include "SongExportJob.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static void wait(SongExportJob& job){const auto deadline=juce::Time::getMillisecondCounter()+20000;while(!job.finished()&&juce::Time::getMillisecondCounter()<deadline)juce::Thread::sleep(1);check(job.finished(),"Worker timeout");}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-export-job-"+juce::Uuid().toString());root.createDirectory();try{
    SongStudio song;auto pattern=song.pattern();pattern.bpm=240;pattern.bars=1;pattern.rows.fill(0);song.setPattern(pattern);std::uint64_t revision=0;
    const auto wav=root.getChildFile("mix.wav");{SongExportJob job(song.recoverySnapshot(revision),wav,false);check(job.start(),"Worker start");pattern.bars=2;song.setPattern(pattern);wait(job);check(job.result().wasOk()&&job.fraction()==1,"Worker result/progress");}
    juce::WavAudioFormat format;{std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wav.createInputStream().release(),true));check(reader&&reader->lengthInSamples==48000,"Live edits changed snapshot export");}
    const auto stems=root.getChildFile("stems");{SongExportJob job(song.recoverySnapshot(revision),stems,true);check(job.start(),"Stem worker start");wait(job);check(job.result().wasOk()&&job.fraction()==1&&stems.getChildFile("05-Drum-pattern.wav").existsAsFile(),"Stem worker completion");}
    std::atomic<bool> cancelled{true};std::atomic<float> progress{0};const auto before=wav.getSize();check(song.exportWav(wav,&cancelled,&progress).failed()&&wav.getSize()==before,"Cancelled mix replaced original");check(song.exportStems(root.getChildFile("cancelled"),&cancelled,&progress).failed()&&!root.getChildFile("cancelled").exists(),"Cancelled stems published");
    pattern.bpm=40;pattern.bars=30;song.setPattern(pattern);const auto longFolder=root.getChildFile("long");{SongExportJob job(song.recoverySnapshot(revision),longFolder,true);check(job.start(),"Long start");const auto deadline=juce::Time::getMillisecondCounter()+10000;while(job.fraction()==0&&!job.finished()&&juce::Time::getMillisecondCounter()<deadline)juce::Thread::sleep(1);check(job.fraction()>0&&!job.finished(),"No incremental progress");job.cancel();wait(job);check(job.result().failed()&&!longFolder.exists(),"In-progress cancel published output");}
    {SongExportJob job(song.recoverySnapshot(revision),root.getChildFile("close"),true);check(job.start(),"Close worker start");}check(!root.getChildFile("close").exists(),"Destructor did not cancel/join");
    check(root.findChildFiles(juce::File::findDirectories,false,"IDW-export-*").isEmpty(),"Cancellation leaked staging folder");
    {SongExportJob job(song.recoverySnapshot(revision),stems,true);check(job.start(),"Failure worker start");wait(job);check(job.result().failed()&&stems.getChildFile("05-Drum-pattern.wav").existsAsFile(),"Worker overwrite protection");}
    std::cout<<"PASS background mix/stems, immutable snapshot, progress, cancellation before/during render, original-file protection, staging cleanup, worker failure and joined shutdown\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
