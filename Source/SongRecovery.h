#pragma once
#include "SongStudio.h"
#include "NativeDictation.h"

// Dedicated disk worker. Recovery never overwrites a user-selected song file.
class SongRecovery final : private juce::Thread {
public:
    struct Entry {juce::File folder;juce::String label;juce::Time modified;};
    struct Writing {SongStudio::Lyrics lyrics;juce::String draft;bool previous=false;};
    SongRecovery(SongStudio& song,NativeDictation& speech,juce::File location=defaultRoot(),bool automatic=true)
        :juce::Thread("IDW song recovery"),engine(song),dictation(speech),root(std::move(location)),session(juce::Uuid().toString()){
        if(automatic)startThread();
    }
    ~SongRecovery() override{
        if(isThreadRunning()){dictation.finish();signalThreadShouldExit();notify();waitForThreadToExit(-1);}
    }
    static juce::File defaultRoot(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IDW Voice MIDI Studio/Recovery");}
    juce::File directory()const{return root;}
    juce::String status()const{const juce::ScopedLock guard(statusLock);return message;}
    void requestCheckpoint(){requested=true;notify();}
    // Public for deterministic tests with automatic=false; never call alongside worker.
    juce::Result checkpoint(bool includeSong=true){
        const auto writing=engine.recoveryWriting();const auto speech=dictation.snapshot();
        const auto folder=root.getChildFile(session).getChildFile(writing.document);
        const bool changed=writing.document!=lastDocument||writing.lyrics.title!=lastTitle||writing.lyrics.text!=lastText||speech.text!=lastDraft;
        const bool meaningful=!writing.lyrics.title.isEmpty()||!writing.lyrics.text.isEmpty()||!speech.text.isEmpty()||engine.audioRevision()!=0||!lastDocument.isEmpty();
        if(changed&&meaningful){
            if(folder.createDirectory().failed())return failed("Recovery folder is not writable. Use Save song.");
            auto* object=new juce::DynamicObject();juce::var payload(object);
            object->setProperty("version",1);object->setProperty("title",writing.lyrics.title);object->setProperty("lyrics",writing.lyrics.text);object->setProperty("draft",speech.text);
            const auto destination=folder.getChildFile("writing-current.json");juce::TemporaryFile temp(destination);
            if(!temp.getFile().replaceWithText(juce::JSON::toString(payload))||!rotate(destination,folder.getChildFile("writing-previous.json"))||!temp.overwriteTargetFileWithTemporary())return failed("Writing recovery failed. Previous checkpoint kept; use Save song.");
            lastDocument=writing.document;lastTitle=writing.lyrics.title;lastText=writing.lyrics.text;lastDraft=speech.text;
            setStatus("Writing checkpoint saved "+juce::Time::getCurrentTime().formatted("%H:%M:%S")+". Save song for your permanent project.");
        }
        if(includeSong&&engine.audioRevision()!=lastAudioRevision&&!engine.isPlaying()){
            std::uint64_t stamp=0;auto snapshot=engine.recoverySnapshot(stamp);
            if(!snapshot)return juce::Result::ok();
            const auto songFolder=root.getChildFile(session).getChildFile(snapshot->recoveryWriting().document);
            if(songFolder.createDirectory().failed())return failed("Song recovery folder is not writable. Use Save song.");
            const auto destination=songFolder.getChildFile("song-current.idwsong");juce::TemporaryFile temp(destination);
            const auto saved=snapshot->save(temp.getFile());
            if(saved.failed()||!rotate(destination,songFolder.getChildFile("song-previous.idwsong"))||!temp.overwriteTargetFileWithTemporary())return failed("Song recovery failed. Previous checkpoint kept; use Save song.");
            lastAudioRevision=stamp;
            setStatus("Song checkpoint saved "+juce::Time::getCurrentTime().formatted("%H:%M:%S")+". Writing is checkpointed separately.");
        }
        return juce::Result::ok();
    }
    std::vector<Entry> entries()const{
        std::vector<Entry> list;
        for(const auto& sessionFolder:root.findChildFiles(juce::File::findDirectories,false)){
            if(sessionFolder.isSymbolicLink())continue;
            for(const auto& folder:sessionFolder.findChildFiles(juce::File::findDirectories,false)){
                if(folder.isSymbolicLink())continue;
                juce::Time latest;bool exists=false;
                for(const char* name:{"writing-current.json","writing-previous.json","song-current.idwsong","song-previous.idwsong"}){
                    const auto file=folder.getChildFile(name);if(file.existsAsFile()){exists=true;latest=std::max(latest,file.getLastModificationTime());}
                }
                if(!exists)continue;
                Writing words;const auto read=readWriting(folder,words);
                const auto title=read.wasOk()&&!words.lyrics.title.isEmpty()?words.lyrics.title:"Untitled song";
                list.push_back({folder,latest.formatted("%Y-%m-%d %H:%M:%S")+" / "+title.replaceCharacters("\r\n\t","   ").substring(0,60)+" / "+sessionFolder.getFileName().substring(0,8),latest});
            }
        }
        std::sort(list.begin(),list.end(),[](const Entry& a,const Entry& b){return a.modified>b.modified;});return list;
    }
    static juce::Result readWriting(const juce::File& folder,Writing& result){
        for(bool previous:{false,true}){
            const auto file=folder.getChildFile(previous?"writing-previous.json":"writing-current.json");
            if(!file.existsAsFile()||file.getSize()>524288)continue;
            const auto payload=juce::JSON::parse(file);
            if(!payload.isObject()||payload.getProperty("version",0)!=juce::var(1))continue;
            const auto title=payload.getProperty("title",{}),lyrics=payload.getProperty("lyrics",{}),draft=payload.getProperty("draft",{});
            if(!title.isString()||!lyrics.isString()||!draft.isString())continue;
            if(title.toString().length()>256||lyrics.toString().length()>32768||draft.toString().length()>32768)continue;
            result={{title.toString(),lyrics.toString()},draft.toString(),previous};return juce::Result::ok();
        }
        return juce::Result::fail("No readable writing checkpoint. Current writing kept.");
    }
    juce::Result restoreWriting(const juce::File& folder,bool& previous){
        if(dictation.busy())return juce::Result::fail("Stop dictation before restoring writing.");
        Writing writing;const auto read=readWriting(folder,writing);if(read.failed())return read;
        if(!engine.setLyrics(writing.lyrics))return juce::Result::fail("Invalid recovered lyrics. Current writing kept.");
        dictation.restoreDraft(writing.draft);engine.forkRecoveryDocument();previous=writing.previous;return juce::Result::ok();
    }
    juce::Result restoreSong(const juce::File& folder,bool& previous){
        if(engine.isPlaying()||dictation.busy()||!dictation.snapshot().text.isEmpty())return juce::Result::fail("Stop transport/dictation, then append or discard the speech draft before restoring a song.");
        for(bool fallback:{false,true}){
            const auto result=engine.load(folder.getChildFile(fallback?"song-previous.idwsong":"song-current.idwsong"));
            if(result.wasOk()){previous=fallback;return result;}
        }
        return juce::Result::fail("No readable song checkpoint. Current song kept. Try restoring writing separately.");
    }
private:
    static bool rotate(const juce::File& current,const juce::File& previous){
        if(!current.existsAsFile())return true;
        juce::TemporaryFile temp(previous);return current.copyFileTo(temp.getFile())&&temp.overwriteTargetFileWithTemporary();
    }
    void setStatus(const juce::String& text){const juce::ScopedLock guard(statusLock);message=text;}
    juce::Result failed(const juce::String& text){setStatus(text);return juce::Result::fail(text);}
    void run()override{
        auto lastWriting=juce::Time::getMillisecondCounterHiRes(),lastSong=lastWriting;
        while(!threadShouldExit()){
            wait(1000);const auto now=juce::Time::getMillisecondCounterHiRes();const bool force=requested.exchange(false);
            if(force||now-lastWriting>=10000){
                const bool full=force||now-lastSong>=30000;
                try{checkpoint(full);}catch(const std::exception&){setStatus("Recovery could not complete. Use Save song.");}
                lastWriting=now;if(full)lastSong=now;
            }
        }
        try{checkpoint(true);}catch(const std::exception&){setStatus("Final recovery checkpoint failed.");}
    }
    SongStudio& engine;NativeDictation& dictation;juce::File root;juce::String session;
    juce::String lastDocument,lastTitle,lastText,lastDraft;std::uint64_t lastAudioRevision=0;
    mutable juce::CriticalSection statusLock;juce::String message="Recovery on: writing every 10 seconds; stopped songs every 30 seconds.";
    std::atomic<bool> requested{false};
};
