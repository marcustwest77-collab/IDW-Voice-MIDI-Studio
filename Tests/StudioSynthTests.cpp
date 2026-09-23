#include "StudioSynth.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <cstdint>
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
double energy(idw::StudioSynth& s,int n){double e=0;for(int i=0;i<n;++i){double v=s.sample();check(std::isfinite(v)&&std::abs(v)<=1,"Invalid audio");e+=v*v;}return e/n;}
int main(){try{
 for(double sr:{44100.,48000.,96000.}){
  idw::StudioSynth s;s.prepare(sr);check(energy(s,1024)==0,"Idle not silent");
  for(int patch=0;patch<5;++patch){s.reset();idw::StudioSynth::Settings p;p.lead=patch;p.delay=0;s.configure(p);s.message(0x90,60,100);check(energy(s,(int)sr/2)>1e-5,"Patch silent");s.message(0x80,60,0);energy(s,(int)sr*3);check(s.activeVoices()==0&&energy(s,128)==0,"Note-off stuck");}
  s.reset();s.message(0x91,60,100);s.message(0x91,64,100);s.message(0x91,67,100);s.message(0x92,36,100);check(s.activeVoices()==4&&energy(s,2048)>1e-5,"Arrangement not polyphonic");
  s.message(0xb1,120,0);check(s.activeVoices()==1,"Channel isolation failed");
  s.reset();s.message(0xb0,64,127);s.message(0x90,60,100);energy(s,2048);s.message(0x80,60,0);energy(s,(int)sr);check(s.activeVoices()==1,"Sustain lost");s.message(0xb0,64,0);energy(s,(int)sr);check(s.activeVoices()==0,"Sustain stuck");
  for(int n=0;n<128;++n)s.message(0x90,n,100);check(s.activeVoices()==32,"Voice pool overflow");energy(s,1024);s.message(0xb0,120,0);check(energy(s,1024)==0,"Panic not silent");
  for(int note:{36,38,42,46}){s.message(0x99,note,100);check(energy(s,1024)>1e-5,"Drum silent");energy(s,(int)(sr*2));check(s.activeVoices()==0,"Drum stuck");}
 }
 std::cout<<"PASS five patches / three sample rates / polyphony / channel isolation / sustain / voice stealing / Panic / four drums\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
