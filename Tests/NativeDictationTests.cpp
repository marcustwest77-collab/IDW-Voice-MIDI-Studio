#include "NativeDictation.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>static void until(F condition){for(int i=0;i<500;++i){if(condition())return;juce::Thread::sleep(10);}throw std::runtime_error("Worker timeout");}
int main(int argc,char** argv){
    const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-speech-test-"+juce::Uuid().toString());
    try{
        check(argc==2,"Provide fake speech runtime path");const juce::File library=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
        dir.getChildFile("am").createDirectory();dir.getChildFile("am/final.mdl").replaceWithText("contract fixture");
        NativeDictation speech;
        check(!speech.start({},dir,48000)&&!speech.busy(),"Missing runtime accepted");
        check(!speech.start(library,dir,8000),"Invalid audio rate accepted");
        check(speech.start(library,dir,48000),"Worker start failed");
        check(!speech.start(library,dir,48000),"Duplicate session accepted");
        until([&]{return speech.snapshot().listening;});
        std::array<float,480> audio;audio.fill(.25f);speech.push(audio.data(),nullptr,480,0,48000);
        until([&]{return speech.snapshot().text.contains("contract phrase");});
        check(!speech.clearDraft(),"Cleared active draft");
        speech.stop();until([&]{return !speech.busy();});
        check(speech.snapshot().text.contains("final phrase"),"Stop lost final result");
        check(speech.clearDraft()&&speech.snapshot().text.isEmpty(),"Draft clear failed");
        check(speech.start(library,dir,48000),"Restart failed");speech.stop();until([&]{return !speech.busy();});
        check(!speech.snapshot().listening&&speech.snapshot().text.isEmpty(),"Cancel while loading captured audio");
        check(speech.start(library,dir,48000),"Rate test start failed");until([&]{return speech.snapshot().listening;});
        speech.push(audio.data(),nullptr,480,0,44100);until([&]{return !speech.busy();});
        check(speech.snapshot().status.contains("device changed"),"Changed device rate was not rejected");
        std::cout<<"PASS native speech runtime contract, PCM feed, final phrase, cancellation and device change (fake recognizer, not speech accuracy)\n";
        dir.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';dir.deleteRecursively();return 1;}
}
