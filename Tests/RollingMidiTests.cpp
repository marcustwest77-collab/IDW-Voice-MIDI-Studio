#include "RollingMidiHistory.h"
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
idw::HistoryEvent event(double t,int status,int a,int b){return {t,{(unsigned char)status,(unsigned char)a,(unsigned char)b},3};}
bool has(const std::vector<idw::HistoryEvent>& events,int status,int a,int b,double t){for(auto& e:events)if(e.data[0]==status&&e.data[1]==a&&e.data[2]==b&&std::abs(e.seconds-t)<.00001)return true;return false;}
int main(){try{
 idw::RollingMidiHistory h;
 check(h.snapshot().empty(),"Empty buffer exported a take");
 h.append(event(1,0x90,60,100));h.append(event(2,0x80,60,0));h.advance(3);auto s=h.snapshot();
 check(has(s,0x90,60,100,1)&&has(s,0x80,60,0,2),"Timing changed");
 h.clear();h.append(event(0,0x90,60,90));h.append(event(35,0x80,60,0));h.advance(40);s=h.snapshot();
 check(has(s,0x90,60,90,0)&&has(s,0x80,60,0,25),"Boundary-held note was lost");check(h.duration()==30,"Window exceeds 30 seconds");
 h.clear();h.append(event(0,0x90,60,80));h.append(event(1,0x80,60,0));h.advance(40);check(h.snapshot().empty(),"Old released note resurrected");
 h.clear();h.append(event(0,0xb0,64,127));h.append(event(1,0x90,60,100));h.append(event(2,0x80,60,0));h.append(event(35,0xb0,64,0));h.advance(40);s=h.snapshot();
 check(has(s,0xb0,64,127,0)&&has(s,0x90,60,100,0)&&has(s,0x80,60,0,0),"Sustain boundary state lost");check(has(s,0xb0,64,0,25),"Sustain release lost");
 h.clear();h.append(event(0,0xb1,101,0));h.append(event(0,0xb1,100,0));h.append(event(0,0xb1,6,12));h.append(event(0,0xb1,101,127));h.append(event(0,0xb1,100,127));h.append(event(1,0xe1,0,96));h.append(event(1,0x91,67,110));h.advance(35);s=h.snapshot();
 check(has(s,0xb1,6,12,0)&&has(s,0xe1,0,96,0)&&has(s,0x91,67,110,0),"Pitch range/channel context lost");
 check(has(s,0xb1,64,0,30)&&has(s,0xb1,123,0,30),"Snapshot lacks closing messages");
 h.append(event(36,0xb1,120,0));h.advance(70);check(h.snapshot().empty(),"Panic resurrected old notes");
 idw::RollingMidiHistory small(4);for(int i=0;i<10;++i)small.append(event(i,0x90,60+i,90));small.advance(11);check(small.retained()==4&&small.capacityLimited(),"Memory bound failed");s=small.snapshot();check(!s.empty(),"Capacity trim lost held notes");
 h.clear();h.append(event(0,0x90,60,100));h.clear(10);h.advance(12);check(h.snapshot().empty(),"Reset retained old performance");
 h.append(event(12,0x90,62,100));h.advance(13);const auto before=h.retained();h.append(event(-1,0x90,60,100));check(h.retained()==before,"Negative time accepted");
 std::cout<<"PASS rolling MIDI timing / 30-second boundary / held notes / sustain / bend-range context / Panic / capacity / reset / invalid time\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
