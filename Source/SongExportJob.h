#pragma once
#include "SongStudio.h"

// Owns a stopped immutable snapshot. Never accesses the live engine or UI.
class SongExportJob final : private juce::Thread {
public:
    SongExportJob(std::unique_ptr<SongStudio> source,juce::File output,bool separate,bool checkLevels=false)
        :juce::Thread("IDW audio export"),destination(std::move(output)),stems(separate),levelCheck(checkLevels),snapshot(std::move(source)){}
    ~SongExportJob()override{cancel();stopThread(-1);}
    bool start(){return snapshot&&startThread();}
    void cancel(){cancelled.store(true);}
    bool finished()const{return complete.load();}
    bool cancellationRequested()const{return cancelled.load();}
    float fraction()const{return progress.load();}
    // Read only after finished(): publication through complete synchronizes result.
    juce::Result result()const{return complete.load()?outcome:juce::Result::fail("Export still running.");}
    const juce::File destination;
    const bool stems,levelCheck;
    juce::String levelSummary()const{return complete.load()&&outcome.wasOk()&&levelCheck?check.summary():juce::String{};}
private:
    void run()override{
        try{outcome=levelCheck?snapshot->checkMix(check,&cancelled,&progress):stems?snapshot->exportStems(destination,&cancelled,&progress):snapshot->exportWav(destination,&cancelled,&progress);}
        catch(const std::exception& e){outcome=juce::Result::fail("Export failed: "+juce::String(e.what()));}
        catch(...){outcome=juce::Result::fail("Export failed unexpectedly. Check the destination for partial files.");}
        complete.store(true);
    }
    SongStudio::MixCheck check;
    std::unique_ptr<SongStudio> snapshot;
    std::atomic<bool> cancelled{false},complete{false};std::atomic<float> progress{0};
    juce::Result outcome=juce::Result::fail("Export not started.");
};
