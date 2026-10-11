#include "NativeDictation.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    check(argc==4,"Usage: native-speech-smoke runtime model-folder speech-test.wav");
    const auto file=[](const char* path){return juce::File::getCurrentWorkingDirectory().getChildFile(path);};
    juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file(argv[3]).createInputStream().release(),true));
    check(reader&&reader->numChannels==1&&reader->lengthInSamples>0,"Invalid speech fixture");
    NativeDictation speech;check(speech.start(file(argv[1]),file(argv[2]),reader->sampleRate),"Cannot start native Vosk");
    for(int i=0;i<6000&&!speech.snapshot().listening&&speech.busy();++i)juce::Thread::sleep(10);
    check(speech.snapshot().listening,"Speech model did not become ready");
    juce::AudioBuffer<float> audio(1,1024);
    for(juce::int64 offset=0;offset<reader->lengthInSamples;offset+=1024){
        const int count=(int)std::min<juce::int64>(1024,reader->lengthInSamples-offset);
        check(reader->read(&audio,0,count,offset,true,false),"Cannot read speech samples");
        speech.push(audio.getReadPointer(0),nullptr,count,0,reader->sampleRate);
        juce::Thread::sleep((int)std::ceil(count*1000.0/reader->sampleRate));
        check(speech.busy(),"Recognition stopped during fixture");
    }
    speech.stop();for(int i=0;i<6000&&speech.busy();++i)juce::Thread::sleep(10);
    check(!speech.busy(),"Recognition finalization timeout");
    const auto result=speech.snapshot();std::cout<<result.status<<"\n"<<result.text<<"\n";
    check(result.status.contains("draft ready")&&result.text.containsWholeWord("zero")&&juce::StringArray::fromTokens(result.text," \n","").size()>=4,"Official speech fixture not recognized");
    std::cout<<"PASS native Vosk runtime/model/PCM integration on official fixture; physical mic not tested\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
