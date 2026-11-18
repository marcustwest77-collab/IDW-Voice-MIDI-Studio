#include "StereoEffectSlot.h"
#include <iostream>
// Explicitly launched diagnostic process, never a startup scan in the DAW.
int main(int argc,char** argv){
    if(argc!=2){std::cerr<<"Usage: IDWVst3EffectProbe <one installed .vst3 path>\n";return 2;}
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File file(juce::String::fromUTF8(argv[1]));
    if(!file.exists() || !file.hasFileExtension("vst3")){std::cerr<<"Choose an existing VST3 file or bundle.\n";return 2;}
    juce::AudioPluginFormatManager formats;
    auto* vst3=new juce::VST3PluginFormat();formats.addFormat(vst3);
    juce::OwnedArray<juce::PluginDescription> descriptions;
    vst3->findAllTypesForFile(descriptions,file.getFullPathName());
    for(const auto* description:descriptions){
        if(description->isInstrument)continue;
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
