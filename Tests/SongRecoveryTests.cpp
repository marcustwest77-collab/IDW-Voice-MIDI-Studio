#include "SongRecovery.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-recovery-test-"+juce::Uuid().toString());
    try{
        SongStudio studio;NativeDictation speech;SongRecovery recovery(studio,speech,root,false);
        check(recovery.checkpoint().wasOk()&&recovery.entries().empty(),"Blank instance created recovery files");
        check(studio.setLyrics({juce::String::fromUTF8("Café"),"first draft"}),"Cannot set writing");
        speech.restoreDraft("unappended words");check(recovery.checkpoint().wasOk(),"Writing checkpoint failed");
        auto entries=recovery.entries();check(entries.size()==1,"Writing checkpoint not listed");const auto original=entries[0].folder;
        auto pattern=studio.pattern();pattern.bpm=240;pattern.bars=1;pattern.rows.fill(0);studio.setPattern(pattern);studio.prepare(48000);
        check(studio.start(true),"Cannot record test take");juce::AudioBuffer<float> block(2,480);for(int ch=0;ch<2;++ch)for(int i=0;i<480;++i)block.setSample(ch,i,.25f);studio.process(block);
        studio.setLyrics({"new title","second draft"});check(recovery.checkpoint().wasOk(),"Writing checkpoint during recording failed");
        check(!original.getChildFile("song-current.idwsong").existsAsFile(),"Checkpoint captured active recording");
        studio.stop();check(recovery.checkpoint().wasOk(),"Stopped song checkpoint failed");
        check(original.getChildFile("song-current.idwsong").existsAsFile(),"Completed take was not checkpointed");
        studio.clearTake();check(recovery.checkpoint().wasOk(),"Second song checkpoint failed");
        check(original.getChildFile("song-previous.idwsong").existsAsFile(),"Previous take checkpoint missing");
        original.getChildFile("song-current.idwsong").replaceWithText("corrupt");
        bool previous=false;speech.clearDraft();check(recovery.restoreSong(original,previous).wasOk()&&previous&&studio.hasTake(),"Corrupt song fallback lost take");
        check(studio.start(false),"Recovered take cannot play");block.clear();studio.process(block);check(std::abs(block.getSample(0,30)-.25f)<.0001f,"Recovered audio differs");studio.stop();
        original.getChildFile("writing-current.json").replaceWithText("corrupt");
        check(recovery.restoreWriting(original,previous).wasOk()&&previous,"Corrupt writing fallback failed");
        check(studio.lyrics().text=="first draft"&&speech.snapshot().text=="unappended words","Writing/dictation recovery differs");
        original.getChildFile("writing-previous.json").replaceWithText("{\"version\":1,\"title\":[],\"lyrics\":null}");
        check(recovery.restoreWriting(original,previous).failed()&&studio.lyrics().text=="first draft","Invalid checkpoints destroyed current writing");
        check(recovery.checkpoint().wasOk()&&recovery.entries().size()==2,"Recovered document overwrote original recovery folder");
        const auto fileInsteadOfFolder=root.getChildFile("blocked");fileInsteadOfFolder.replaceWithText("keep");
        SongRecovery blocked(studio,speech,fileInsteadOfFolder,false);check(blocked.checkpoint().failed()&&fileInsteadOfFolder.loadFileAsString()=="keep","Unwritable destination was not preserved/reported");
        {SongRecovery other(studio,speech,root,false);check(other.checkpoint().wasOk(),"Second instance checkpoint failed");check(other.entries().size()==3,"Instances overwrote each other");}
        const auto automaticRoot=root.getChildFile("automatic");
        {SongRecovery automatic(studio,speech,automaticRoot,true);automatic.requestCheckpoint();for(int i=0;i<200&&automatic.entries().empty();++i)juce::Thread::sleep(10);check(!automatic.entries().empty(),"Background recovery did not run");studio.setLyrics({"final","normal close"});}
        SongRecovery afterClose(studio,speech,automaticRoot,false);const auto after=afterClose.entries();SongRecovery::Writing saved;
        check(!after.empty()&&SongRecovery::readWriting(after[0].folder,saved).wasOk()&&saved.lyrics.text=="normal close","Normal close lost final writing checkpoint");
        std::cout<<"PASS recovery: writing during recording, stopped take, audio round trip, previous-copy fallback, corrupt data preservation, separate documents/instances, disk failure and background/close checkpoint\n";
        root.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}
}
