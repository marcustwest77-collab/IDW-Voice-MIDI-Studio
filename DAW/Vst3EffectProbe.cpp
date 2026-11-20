#include "StereoEffectSlot.h"
#include <iostream>
#include <juce_audio_formats/juce_audio_formats.h>
#include "EffectDiagnosticMetrics.h"
// Explicitly launched diagnostic process, never a startup scan in the DAW.
int main(int argc,char** argv){
    if(argc!=2 && argc!=3){std::cerr<<"Usage: IDWVst3EffectProbe <one installed .vst3 path> [new diagnostic output folder]\n";return 2;}
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File file(juce::String::fromUTF8(argv[1]));
    if(!file.exists() || !file.hasFileExtension("vst3")){std::cerr<<"Choose an existing VST3 file or bundle.\n";return 2;}
    juce::AudioPluginFormatManager formats;
    auto* vst3=new juce::VST3PluginFormat();formats.addFormat(vst3);
    juce::OwnedArray<juce::PluginDescription> descriptions;
    vst3->findAllTypesForFile(descriptions,file.getFullPathName());
    for(const auto* description:descriptions){
        if(description->isInstrument)continue;
        if(argc==3){
            const juce::File destination(juce::String::fromUTF8(argv[2]));
            if(destination.exists() || !destination.createDirectory().wasOk()){
                std::cerr<<"Choose a new diagnostic output folder.\n";return 2;
            }
            constexpr double rate=44100;constexpr int total=44100*5;
            juce::Array<juce::var> runs;
            for(int size:{32,512,1024}){
                juce::String error;auto instance=formats.createPluginInstance(*description,rate,size,error);
                if(!instance){std::cerr<<error<<'\n';return 1;}
                juce::Array<juce::var> parameters,parameterInventory;
                for(auto* parameter:instance->getParameters()){
                    const auto name=parameter->getName(128);
                    auto* entry=new juce::DynamicObject();entry->setProperty("index",parameter->getParameterIndex());
                    entry->setProperty("name",name);entry->setProperty("label",parameter->getLabel());
                    entry->setProperty("normalizedValue",parameter->getValue());
                    entry->setProperty("displayValue",parameter->getCurrentValueAsText());parameterInventory.add(juce::var(entry));
                    if(name.equalsIgnoreCase("Depth") || name.equalsIgnoreCase("Speed") ||
                       name.equalsIgnoreCase("Depth (Automatic tuning)") || name.equalsIgnoreCase("Speed (Automatic tuning)")){
                        parameter->setValueNotifyingHost(1.0f);parameters.add(name);
                    }
                }
                idw::daw::StereoEffectSlot effect;
                if(!effect.install(std::move(instance),rate,size,error)){std::cerr<<error<<'\n';return 1;}
                juce::Array<juce::var> confirmedSettings;bool maximumConfirmed=parameters.size()==2;
                for(auto* parameter:effect.processorForControlThread()->getParameters()){
                    if(!parameters.contains(juce::var(parameter->getName(128))))continue;
                    auto* setting=new juce::DynamicObject();setting->setProperty("name",parameter->getName(128));
                    setting->setProperty("normalizedValue",parameter->getValue());
                    setting->setProperty("displayValue",parameter->getCurrentValueAsText());confirmedSettings.add(juce::var(setting));
                    maximumConfirmed=maximumConfirmed && parameter->getValue()>=0.999f;
                }
                maximumConfirmed=maximumConfirmed && confirmedSettings.size()==2;
                const auto output=destination.getChildFile("MAutoPitch-"+juce::String(size)+".wav");
                auto stream=output.createOutputStream();juce::WavAudioFormat wav;
                auto writer=std::unique_ptr<juce::AudioFormatWriter>(stream?wav.createWriterFor(stream.get(),rate,2,24,{},0):nullptr);
                if(!writer){std::cerr<<"Could not create diagnostic WAV.\n";return 1;}stream.release();
                idw::daw::EffectDiagnosticMetrics metrics;juce::AudioBuffer<float> block(2,size);
                for(int cursor=0;cursor<total;cursor+=size){
                    const int count=std::min(size,total-cursor);
                    for(int i=0;i<count;++i){
                        const double t=(cursor+i)/rate;
                        const double envelope=std::min(1.0,std::min(t/0.05,(5.0-t)/0.05));
                        const float sample=(float)(0.1*envelope*std::sin(juce::MathConstants<double>::twoPi*227.0*t));
                        block.setSample(0,i,sample);block.setSample(1,i,sample);
                    }
                    float* pointers[]{block.getWritePointer(0),block.getWritePointer(1)};
                    juce::AudioBuffer<float> view(pointers,2,count);
                    const double start=juce::Time::getMillisecondCounterHiRes();
                    const bool ok=effect.process(view);
                    const double elapsed=juce::Time::getMillisecondCounterHiRes()-start;
                    metrics.observe(view.getReadPointer(0),view.getReadPointer(1),count,elapsed,rate);
                    if(!ok || !writer->writeFromFloatArrays(view.getArrayOfReadPointers(),2,count)){
                        std::cerr<<"Diagnostic processing/write failed.\n";return 1;
                    }
                }
                writer.reset();
                auto* result=new juce::DynamicObject();
                result->setProperty("blockSize",size);result->setProperty("sampleRate",rate);
                result->setProperty("parameterInventory",parameterInventory);
                result->setProperty("confirmedSettingsAfterPrepare",confirmedSettings);
                result->setProperty("requestedMaximumSettingsApplied",maximumConfirmed);
                result->setProperty("latencySamples",effect.latencySamples());result->setProperty("parametersSetToMaximum",parameters);
                result->setProperty("peak",metrics.peak);result->setProperty("maxBoundaryJump",metrics.maxBoundaryJump);
                result->setProperty("maxInteriorJump",metrics.maxInteriorJump);result->setProperty("maxProcessMs",metrics.maxProcessMs);
                result->setProperty("blocksOverRealtimeDeadline",(juce::int64)metrics.overDeadline);
                result->setProperty("samplesOverUnity",(juce::int64)metrics.overUnity);
                result->setProperty("nonfiniteSamples",(juce::int64)metrics.nonfinite);
                result->setProperty("wav",output.getFileName());runs.add(juce::var(result));
            }
            auto* report=new juce::DynamicObject();report->setProperty("plugin",description->name);
            report->setProperty("input","5 seconds, 227 Hz synthetic tone at amplitude 0.1; 50 ms edge ramps");
            report->setProperty("scope","Offline diagnostic with plugin real-time mode unchanged; no device callback. Timing and boundary metrics do not prove audible crackle or its cause. No user recording read.");
            report->setProperty("runs",runs);
            if(!destination.getChildFile("Report.json").replaceWithText(juce::JSON::toString(juce::var(report),true)))return 1;
            std::cout<<"Diagnostic renders and measurements: "<<destination.getFullPathName()<<'\n';return 0;
        }
        juce::String error;
        auto instance=formats.createPluginInstance(*description,44100,512,error);
        if(!instance){std::cerr<<error<<'\n';return 1;}
        idw::daw::StereoEffectSlot slot;
        if(!slot.install(std::move(instance),44100,512,error)){std::cerr<<error<<'\n';return 1;}
        juce::AudioBuffer<float> block(2,512);
        for(int n=0;n<100;++n){
            for(int i=0;i<512;++i){const float sample=0.1f*std::sin((float)(n*512+i)*0.062689377f);block.setSample(0,i,sample);block.setSample(1,i,sample);}
            if(!slot.process(block)){std::cerr<<"Effect processing failed.\n";return 1;}
        }
        std::cout<<"Prepared and processed synthetic stereo audio: "<<description->name
                 <<" | latency "<<slot.latencySamples()<<" samples\n"
                 <<"Diagnostic only: not DAW track integration, editor, state recall or export qualification.\n";
        return 0;
    }
    std::cerr<<"No audio effect found in the selected VST3.\n";return 1;
}
