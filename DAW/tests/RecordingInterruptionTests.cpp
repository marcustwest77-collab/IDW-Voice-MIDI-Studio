#include "TakeRecorder.h"
#include "MultiTakeRecorder.h"
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void verifyWav(const juce::File& file,float value,int frames){
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    require(reader && reader->sampleRate==48000 && reader->numChannels==1 &&
            reader->lengthInSamples==frames,"partial take format or duration");
    juce::AudioBuffer<float> audio(1,frames);
    require(reader->read(&audio,0,frames,0,true,false),"read partial take");
    for(int i=0;i<frames;++i)
        require(std::abs(audio.getSample(0,i)-value)<0.000001f,"partial take sample changed");
}
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("idw-recording-interruption","",false);
    require(root.createDirectory().wasOk(),"fixture directory");
    float a[64],b[64];std::fill_n(a,64,0.125f);std::fill_n(b,64,-0.25f);
    const float* inputs[]{a,b};
    juce::AudioIODeviceCallbackContext context{};
    for(int event=0;event<2;++event){
        const auto dir=root.getChildFile(juce::String(event));
        require(dir.createDirectory().wasOk(),"case directory");
        {
            IDWMultiTakeRecorder recorder;
            recorder.audioDeviceAboutToStart(nullptr);
            require(!recorder.isRecording() && !recorder.wasDeviceInterrupted(),"idle multitrack interruption");
            const auto first=dir.getChildFile("first.wav"),second=dir.getChildFile("second.wav");
            require(recorder.start({{first,0},{second,1}},48000),"multitrack start");
            for(int i=0;i<2;++i)recorder.audioDeviceIOCallbackWithContext(inputs,2,nullptr,0,64,context);
            recorder.setHeadphoneMonitor(true);
            if(event==0)recorder.audioDeviceStopped();else recorder.audioDeviceAboutToStart(nullptr);
            require(recorder.isRecording() && recorder.wasDeviceInterrupted(),"partial multitrack take must remain pending");
            float out[64]{};float* outputs[]{out};
            recorder.audioDeviceIOCallbackWithContext(inputs,2,outputs,1,64,context);
            recorder.audioDeviceAboutToStart(nullptr);
            recorder.audioDeviceIOCallbackWithContext(inputs,2,outputs,1,64,context);
            for(float sample:out)require(sample==0,"monitor resumed after interruption");
            recorder.stop();require(!recorder.isRecording(),"multitrack stop");
            verifyWav(first,0.125f,128);verifyWav(second,-0.25f,128);
            const auto next=dir.getChildFile("multi-next.wav");
            require(recorder.start({{next,1}},48000) && !recorder.wasDeviceInterrupted(),"multitrack restart");
            recorder.audioDeviceIOCallbackWithContext(inputs,2,nullptr,0,64,context);recorder.stop();
            verifyWav(next,-0.25f,64);
        }
        {
            IDWTakeRecorder recorder;
            const auto first=dir.getChildFile("single.wav");
            require(recorder.start(first,48000,1,1),"single start");
            for(int i=0;i<2;++i)recorder.audioDeviceIOCallbackWithContext(inputs,2,nullptr,0,64,context);
            if(event==0)recorder.audioDeviceStopped();else recorder.audioDeviceAboutToStart(nullptr);
            require(recorder.isRecording() && recorder.wasDeviceInterrupted(),"partial single take must remain pending");
            recorder.audioDeviceAboutToStart(nullptr);
            recorder.audioDeviceIOCallbackWithContext(inputs,2,nullptr,0,64,context);
            require(recorder.getCapturedFrames()==128 && recorder.getQueuedFrames()==128,"single appended restarted device");
            recorder.stop();verifyWav(first,-0.25f,128);
            const auto next=dir.getChildFile("single-next.wav");
            require(recorder.start(next,48000) && !recorder.wasDeviceInterrupted(),"single restart");
            recorder.audioDeviceIOCallbackWithContext(inputs,2,nullptr,0,64,context);recorder.stop();
            verifyWav(next,0.125f,64);
        }
    }
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS: single and multitrack device interruption preserves partial WAVs\n";
}
