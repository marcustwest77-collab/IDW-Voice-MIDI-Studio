#include "IsolatedAudioCallback.h"
#include "TakeRecorder.h"
#include "MultiTakeRecorder.h"
#include "LoopbackCapture.h"
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char* message) {
    if(!ok)throw std::runtime_error(message);
}
static void silent(const juce::AudioBuffer<float>& output) {
    for(int channel=0;channel<output.getNumChannels();++channel)
        for(int frame=0;frame<output.getNumSamples();++frame)
            require(output.getSample(channel,frame)==0.0f,"stale callback output reached the mix");
}
static void dirty(juce::AudioBuffer<float>& output) {
    for(int channel=0;channel<output.getNumChannels();++channel)
        for(int frame=0;frame<output.getNumSamples();++frame)
            output.setSample(channel,frame,frame%2==0?0.7f:-0.6f);
}
int main() {
    try {
        IDWTakeRecorder recorder;IDWMultiTakeRecorder multi;IDWLoopbackCapture loopback;
        IDWIsolatedAudioCallback singleOutput(recorder),multiOutput(multi),loopOutput(loopback);
        juce::AudioIODeviceCallback* callbacks[]{&loopOutput,&multiOutput,&singleOutput};
        juce::AudioBuffer<float> output(2,64),input(1,64);
        for(int i=0;i<64;++i)input.setSample(0,i,0.25f);
        juce::AudioIODeviceCallbackContext context{};
        // JUCE reuses uncleared scratch across secondary callbacks and blocks.
        // A stopped workstation must remain silent regardless of scratch history.
        for(int block=0;block<512;++block) {
            for(auto* callback:callbacks) {
                dirty(output);
                callback->audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,
                    output.getArrayOfWritePointers(),2,64,context);
                silent(output);
            }
        }
        const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("idw-callback-isolation","",false);
        require(root.createDirectory().wasOk(),"test directory");
        require(recorder.start(root.getChildFile("single.wav"),48000),"single recording start");
        dirty(output);
        singleOutput.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,
            output.getArrayOfWritePointers(),2,64,context);
        silent(output);require(recorder.getCapturedFrames()==64,"single capture was not forwarded");
        singleOutput.audioDeviceStopped();
        require(recorder.wasDeviceInterrupted(),"single device stop was not forwarded");recorder.stop();

        require(multi.start({{root.getChildFile("multi.wav"),0}},48000),"multi recording start");
        dirty(output);
        multiOutput.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,
            output.getArrayOfWritePointers(),2,64,context);
        silent(output); // Monitoring is opt-in, even while recording.
        multi.setHeadphoneMonitor(true,0.1f);
        dirty(output);
        multiOutput.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,
            output.getArrayOfWritePointers(),2,64,context);
        for(int channel=0;channel<2;++channel)
            for(int i=0;i<64;++i)
                require(std::abs(output.getSample(channel,i)-0.025f)<0.000001f,
                        "monitor inherited stale scratch or lost its input");
        multiOutput.audioDeviceStopped();
        require(multi.wasDeviceInterrupted(),"multi device stop was not forwarded");
        dirty(output);
        multiOutput.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,
            output.getArrayOfWritePointers(),2,64,context);
        silent(output);multi.stop();

        require(loopback.prepare(8000,0,0) && loopback.start(),"loopback prepare/start");
        for(int position=0;position<6400;position+=64) {
            dirty(output);
            loopOutput.audioDeviceIOCallbackWithContext(input.getArrayOfReadPointers(),1,
                output.getArrayOfWritePointers(),2,64,context);
            for(int i=0;i<64;++i) {
                const int frame=position+i;
                const float pulse=frame>=800 && frame<=4000 && frame%800==0?0.25f:0.0f;
                require(output.getSample(0,i)==pulse && output.getSample(1,i)==0.0f,
                        "loopback leaked scratch instead of isolated pulses");
            }
        }
        require(loopback.isComplete(),"loopback completion was not forwarded");
        dirty(output);
        loopOutput.audioDeviceIOCallbackWithContext(nullptr,0,output.getArrayOfWritePointers(),2,64,context);
        silent(output); // No persistent pulse/tone after calibration completes.
        // Input-only callbacks and unavailable output slots must remain valid.
        singleOutput.audioDeviceIOCallbackWithContext(nullptr,0,nullptr,0,64,context);
        float* missingOutputs[]{nullptr};
        singleOutput.audioDeviceIOCallbackWithContext(nullptr,0,missingOutputs,1,64,context);
        require(root.deleteRecursively(),"test cleanup");
        std::cout << "PASS: idle/capture/interrupted callbacks are silent; monitor/pulses are isolated\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';return 1;
    }
}
