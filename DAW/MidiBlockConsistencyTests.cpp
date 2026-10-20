#include "MidiSineSynth.h"
#include <vector>
#include <iostream>
#include <stdexcept>
int main(){
 using namespace idw::daw;
 constexpr int frames=4096;constexpr double rate=48000.0;
 for(int preset=0;preset<5;++preset){
  Session s;s.addTrack(TrackType::midi);s.addTrack(TrackType::midi);
  s.setTrackSynthPreset(0,preset);s.setTrackSynthPreset(1,(preset+1)%5);
  s.setTrackMix(0,0.8f,-0.3f);s.setTrackMix(1,0.6f,0.4f);
  s.addMidiNote(0,{60,0.0,0.16,100});
  s.addMidiNote(0,{67,257.4/24000.0,700.0/24000.0,90});
  s.addMidiNote(1,{72,901.6/24000.0,1300.0/24000.0,110});
  s.addMidiNote(1,{65,2300.0/24000.0,400.0/24000.0,80});
  auto render=[&](int block){
   std::vector<float> output(frames*2,0.07f); // Existing audio must remain untouched.
   for(int pos=0;pos<frames;pos+=block)
    renderMidiSine(s,output.data()+pos,output.data()+frames+pos,std::min(block,frames-pos),123+pos,rate);
   return output;
  };
  const auto reference=render(1);
  for(int block:{64,127,256,1024,4096})
   if(render(block)!=reference)throw std::runtime_error("MIDI level changes with callback/export block size");
  s.tracks[0].mute=true;s.tracks[1].mute=true;
  for(float sample:render(127))if(sample!=0.07f)throw std::runtime_error("muted MIDI changed audio bed");
 }
 std::cout<<"PASS: staggered MIDI notes match exactly across six buffer sizes and five presets\n";
}
