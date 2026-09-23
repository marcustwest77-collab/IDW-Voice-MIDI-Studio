#pragma once
#include "MidiExport.h"
// Owns the take for the processor lifetime, independently of any editor window.
// All methods and the timer run on the message thread; the audio thread only uses capture's SPSC FIFO.
class TakeArchive final : private juce::Timer {
public:
    explicit TakeArchive(PerformanceCapture& source):capture(source){startTimer(100);}
    ~TakeArchive() override {stopTimer();const juce::ScopedLock guard(lock);capture.stop();drain();checkpoint();}
    static juce::File directory(){auto d=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IDW Voice MIDI Studio/Recovered Takes");d.createDirectory();return d;}
    bool start(double tempo){const juce::ScopedLock guard(lock);
        if(capture.isRecording())return false;
        drain();if(!checkpoint())return false; // Never replace an unsaved take after a disk error.
        events.clear();truncated=false;bpm=juce::jlimit(40.0,240.0,tempo);dirty=false;
        recoveryFile=directory().getChildFile("IDW-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")+"-"+juce::Uuid().toString()+".mid");
        capture.start();return true;
    }
    void drain(){const juce::ScopedLock guard(lock);
        PerformanceCapture::Event event;
        while(capture.pop(event)){
            if(events.size()<250000){events.push_back(event);dirty=true;}
            else {truncated=true;capture.stop();}
        }
    }
    bool checkpoint(){const juce::ScopedLock guard(lock);
        if(!dirty||events.empty())return true;
        if(recoveryFile==juce::File{})recoveryFile=directory().getChildFile("IDW-recovered-"+juce::Uuid().toString()+".mid");
        juce::TemporaryFile temporary(recoveryFile);
        const bool ok=writePerformanceMidi(temporary.getFile(),events,bpm)&&temporary.overwriteTargetFileWithTemporary();
        saveFailed=!ok;if(ok){dirty=false;lastSave=juce::Time::getMillisecondCounterHiRes();}return ok;
    }
    bool exportTo(const juce::File& file){const juce::ScopedLock guard(lock);drain();if(capture.isRecording()||events.empty())return false;juce::TemporaryFile temp(file);return writePerformanceMidi(temp.getFile(),events,bpm)&&temp.overwriteTargetFileWithTemporary();}
    bool recover(const juce::File& file){const juce::ScopedLock guard(lock);
        if(capture.isRecording())return false;
        drain();if(!checkpoint())return false;
        if(file.getSize()>16*1024*1024)return false;
        juce::FileInputStream stream(file);juce::MidiFile midi;
        if(!stream.openedOk()||!midi.readFrom(stream))return false;
        double tempo=120;
        for(int t=0;t<midi.getNumTracks();++t)for(int n=0;n<midi.getTrack(t)->getNumEvents();++n){const auto& m=midi.getTrack(t)->getEventPointer(n)->message;if(m.isTempoMetaEvent()&&m.getTimeStamp()==0)tempo=60.0/m.getTempoSecondsPerQuarterNote();}
        midi.convertTimestampTicksToSeconds();std::vector<PerformanceCapture::Event> recovered;
        for(int t=0;t<midi.getNumTracks();++t)for(int n=0;n<midi.getTrack(t)->getNumEvents();++n){const auto& m=midi.getTrack(t)->getEventPointer(n)->message;
            if(m.isMetaEvent()||m.isSysEx()||m.getRawDataSize()>3)continue;
            if(recovered.size()>=250000)return false;
            PerformanceCapture::Event e;e.seconds=m.getTimeStamp();if(!std::isfinite(e.seconds)||e.seconds<0)return false;e.size=m.getRawDataSize();std::copy_n(m.getRawData(),e.size,e.data);recovered.push_back(e);
        }
        if(recovered.empty())return false;
        std::stable_sort(recovered.begin(),recovered.end(),[](const auto& a,const auto& b){return a.seconds<b.seconds;});
        events=std::move(recovered);bpm=juce::jlimit(40.0,240.0,tempo);truncated=false;dirty=false;saveFailed=false;recoveryFile=juce::File{};return true;
    }
    size_t size() const{const juce::ScopedLock guard(lock);return events.size();}
    double duration() const{const juce::ScopedLock guard(lock);return capture.isRecording()?capture.duration():(events.empty()?0:events.back().seconds);}
    bool isTruncated() const{const juce::ScopedLock guard(lock);return truncated||capture.wasTruncated();}
    bool diskError() const{const juce::ScopedLock guard(lock);return saveFailed;}
    double tempo() const{const juce::ScopedLock guard(lock);return bpm;}
private:
    void timerCallback() override {const juce::ScopedLock guard(lock);drain();const bool recording=capture.isRecording();if(dirty&&(!recording||juce::Time::getMillisecondCounterHiRes()-lastSave>=5000))checkpoint();}
    mutable juce::CriticalSection lock;
    PerformanceCapture& capture;
    std::vector<PerformanceCapture::Event> events;
    juce::File recoveryFile;
    double bpm=120,lastSave=0;
    bool dirty=false,truncated=false,saveFailed=false;
};
