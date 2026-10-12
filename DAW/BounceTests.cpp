#include "OfflineBounce.h"
#include <stdexcept>
#include <cmath>
#include <iostream>
int main(){
 using namespace idw::daw;
 auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-bounce-test","",false);
 if(!root.createDirectory())throw std::runtime_error("temp directory");
 const auto input=root.getChildFile("source.wav"),output=root.getChildFile("result.wav");
 juce::WavAudioFormat wav;
 auto stream=std::unique_ptr<juce::FileOutputStream>(input.createOutputStream());
 if(!stream)throw std::runtime_error("input stream");
 auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(),44100.0,1,16,{},0));
 if(!writer)throw std::runtime_error("input writer");
 stream.release();
 juce::AudioBuffer<float> audio(1,4410);audio.clear();
 for(int i=0;i<4410;++i)audio.setSample(0,i,0.25f);
 const float* ptrs[]{audio.getReadPointer(0)};
 if(!writer->writeFromFloatArrays(ptrs,1,4410))throw std::runtime_error("write input");
 writer.reset();
 Session session;session.setTempo(120);session.addTrack(TrackType::audio);
 if(!session.addClip(0,{input.getFullPathName().toStdString(),1.0,0.2,0.0}))
  throw std::runtime_error("clip");
 juce::String error;
 if(!renderSessionWav(session,output,error))throw std::runtime_error(error.toStdString());
 auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(output.createInputStream().release(),true));
 if(!reader || reader->numChannels!=2 || reader->sampleRate!=44100 || reader->lengthInSamples!=26460)
  throw std::runtime_error("output metadata");
 juce::AudioBuffer<float> rendered(2,26460);
 if(!reader->read(&rendered,0,26460,0,true,true))throw std::runtime_error("read output");
 if(std::abs(rendered.getSample(0,100))>0.00001f || std::abs(rendered.getSample(0,23000)-0.25f)>0.01f)
  throw std::runtime_error("timeline alignment or signal level");
 reader.reset();root.deleteRecursively();
 std::cout<<"PASS: stereo WAV bounce timeline and signal test\n";
}
