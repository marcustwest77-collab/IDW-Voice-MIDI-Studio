#pragma once
#include <juce_core/juce_core.h>
#include "DictationAudioQueue.h"
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct VoskModel;
struct VoskRecognizer;

// Optional Vosk runtime; no microphone device is opened here. All recognition and
// model loading run on this worker. The audio callback only writes bounded PCM.
class NativeDictation final : private juce::Thread {
public:
    struct Snapshot {juce::String status,partial,text;bool busy=false,listening=false;};
    NativeDictation():juce::Thread("IDW offline dictation"){}
    ~NativeDictation() override {finish();}
    void finish(){stop();waitForThreadToExit(-1);}
    bool restoreDraft(const juce::String& text){if(busy()||text.length()>32768)return false;const juce::ScopedLock guard(stateLock);transcript=text;partial.clear();status="Recovered draft — review and append to lyrics.";return true;}
    bool busy()const{return isThreadRunning();}
    Snapshot snapshot()const{const juce::ScopedLock guard(stateLock);return {status,partial,transcript,busy(),accepting.load()};}
    bool start(const juce::File& library,const juce::File& model,double rate){
        if(busy())return false;
        if(rate<16000||rate>192000){setStatus("Start audio at 16–192 kHz before dictating.");return false;}
        if(!library.existsAsFile()||!model.getChildFile("am/final.mdl").existsAsFile()){
            setStatus("Speech assets missing. Choose the Vosk runtime and extracted model folder.");return false;
        }
        runtimePath=library;modelPath=model;sessionRate=rate;stopRequested=false;failure=0;
        {const juce::ScopedLock guard(stateLock);partial.clear();status="Loading local speech model — wait for MIC ON.";}
        if(!startThread()){setStatus("Could not start recognition worker.");return false;}return true;
    }
    void stop() noexcept {accepting=false;stopRequested=true;}
    void audioStopped() noexcept {if(accepting.load())failure=2;stop();}
    // Called once per block BEFORE effects, monitoring, synthesis and song playback.
    void push(const float* left,const float* right,int count,int mode,double rate) noexcept {
        if(!accepting.load(std::memory_order_acquire))return;
        writers.fetch_add(1);
        if(accepting.load(std::memory_order_acquire)&&!stopRequested.load()){
            if(rate!=sessionRate){failure=2;stop();}
            else if(!queue.push(left,right,count,mode)){failure=1;stop();}
        }
        writers.fetch_sub(1);
    }
    bool clearDraft(){if(busy())return false;const juce::ScopedLock guard(stateLock);transcript.clear();partial.clear();return true;}
    static juce::File installedAssets(){
        const auto exe=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        for(const auto& root:{exe.getChildFile("Audio-Lab/_internal"),exe.getParentDirectory().getChildFile("Audio-Lab/_internal")})
            if(root.getChildFile("speech-model/vosk-model-small-en-us-0.15/am/final.mdl").existsAsFile())return root;
       #if JUCE_WINDOWS
        const juce::File installed("C:/Program Files/IDW Voice MIDI Studio/Audio-Lab/_internal");
        if(installed.getChildFile("speech-model/vosk-model-small-en-us-0.15/am/final.mdl").existsAsFile())return installed;
       #endif
        return {};
    }
private:
    class SpeechRuntime {
    public:
       #if JUCE_WINDOWS
        ~SpeechRuntime(){if(handle)FreeLibrary(handle);}
        bool open(const juce::String& path){handle=LoadLibraryExW(path.toWideCharPointer(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);return handle!=nullptr;}
        template<class Fn> Fn* getFunction(const char* name){return reinterpret_cast<Fn*>(GetProcAddress(handle,name));}
    private:
        HMODULE handle=nullptr;
       #else
        bool open(const juce::String& path){return library.open(path);}
        template<class Fn> Fn* getFunction(const char* name){return library.getFunction<Fn>(name);}
    private:
        juce::DynamicLibrary library;
       #endif
    };
    void setStatus(const juce::String& value){const juce::ScopedLock guard(stateLock);status=value;}
    void addResult(const char* json,bool final){
        if(json==nullptr)return;
        const auto value=juce::JSON::parse(juce::String::fromUTF8(json));
        const auto text=value.getProperty(final?"text":"partial",{}).toString().trim();
        const juce::ScopedLock guard(stateLock);
        if(!final){partial=text;return;}
        partial.clear();if(text.isEmpty())return;
        const auto next=transcript+(transcript.isEmpty()?"":"\n")+text;
        if(next.length()>32768){failure=3;stop();return;}
        transcript=next;
    }
    void run() override {
        // Consume any residual samples while capture is disabled; no index reset races.
        std::array<short,4096> pcm{};while(queue.pop(pcm.data(),(int)pcm.size())>0){}
        SpeechRuntime runtime;
        if(!runtime.open(runtimePath.getFullPathName())){setStatus("Cannot load Vosk runtime. Check its architecture and companion DLLs.");return;}
        const auto modelNew=runtime.getFunction<VoskModel*(const char*)>("vosk_model_new");
        const auto modelFree=runtime.getFunction<void(VoskModel*)>("vosk_model_free");
        const auto recognizerNew=runtime.getFunction<VoskRecognizer*(VoskModel*,float)>("vosk_recognizer_new");
        const auto recognizerFree=runtime.getFunction<void(VoskRecognizer*)>("vosk_recognizer_free");
        const auto accept=runtime.getFunction<int(VoskRecognizer*,const short*,int)>("vosk_recognizer_accept_waveform_s");
        const auto result=runtime.getFunction<const char*(VoskRecognizer*)>("vosk_recognizer_result");
        const auto partialResult=runtime.getFunction<const char*(VoskRecognizer*)>("vosk_recognizer_partial_result");
        const auto finalResult=runtime.getFunction<const char*(VoskRecognizer*)>("vosk_recognizer_final_result");
        if(!modelNew||!modelFree||!recognizerNew||!recognizerFree||!accept||!result||!partialResult||!finalResult){setStatus("Incompatible Vosk runtime: required speech functions are missing.");return;}
        std::unique_ptr<VoskModel,decltype(modelFree)> model(modelNew(modelPath.getFullPathName().toRawUTF8()),modelFree);
        if(!model){setStatus("Cannot load speech model. Choose the extracted model folder.");return;}
        if(stopRequested.load()){setStatus("MIC OFF — dictation cancelled.");return;}
        std::unique_ptr<VoskRecognizer,decltype(recognizerFree)> recognizer(recognizerNew(model.get(),(float)sessionRate),recognizerFree);
        if(!recognizer){setStatus("Could not create speech recognizer.");return;}
        accepting=true;
        setStatus("MIC ON — local dictation uses IDW's selected input. Stop to finish your draft.");
        const auto started=juce::Time::getMillisecondCounterHiRes();
        auto lastAudio=started;
        while(!stopRequested.load()){
            const int count=queue.pop(pcm.data(),(int)pcm.size());
            if(count){lastAudio=juce::Time::getMillisecondCounterHiRes();const int answer=accept(recognizer.get(),pcm.data(),count);if(answer<0){failure=4;break;}addResult(answer?result(recognizer.get()):partialResult(recognizer.get()),answer!=0);}
            else wait(10);
            if(juce::Time::getMillisecondCounterHiRes()-lastAudio>3000){failure=6;break;}
            if(juce::Time::getMillisecondCounterHiRes()-started>600000){failure=5;break;}
        }
        accepting=false;while(writers.load()!=0)wait(1);
        for(int count;(count=queue.pop(pcm.data(),(int)pcm.size()))>0;){const int answer=accept(recognizer.get(),pcm.data(),count);if(answer<0){failure=4;break;}if(answer)addResult(result(recognizer.get()),true);}
        if(failure.load()!=4)addResult(finalResult(recognizer.get()),true);
        switch(failure.load()){
            case 1:setStatus("MIC OFF — recognition fell behind. Review the partial draft; reduce CPU load.");break;
            case 2:setStatus("MIC OFF — audio device changed/stopped. Review draft, then restart dictation.");break;
            case 3:setStatus("MIC OFF — draft limit reached. Review and append before continuing.");break;
            case 4:setStatus("MIC OFF — speech engine error. Review the partial draft.");break;
            case 6:setStatus("MIC OFF — no audio callbacks. Start IDW audio or enable host processing, then retry.");break;
            case 5:setStatus("MIC OFF — ten-minute session ended. Review and append your draft.");break;
            default:setStatus("MIC OFF — draft ready. Append to lyrics, then Save song.");break;
        }
    }
    idw::DictationAudioQueue queue;
    std::atomic<bool> accepting{false},stopRequested{false};std::atomic<int> writers{0},failure{0};
    double sessionRate=48000;juce::File runtimePath,modelPath;
    mutable juce::CriticalSection stateLock;
    juce::String status="MIC OFF — choose speech assets or use installed Audio Lab assets.",partial,transcript;
};
