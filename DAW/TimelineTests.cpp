#include "TimelineMath.h"
#include <iostream>
#include <stdexcept>
void require(bool condition,const char* label){if(!condition)throw std::runtime_error(label);}
int main(){
 using idw::daw::overlap;
 auto a=overlap(0,512,48000,120,0,4);
 require(a.offset==0 && a.frames==512 && a.sourceFrame==0,"start");
 auto b=overlap(47000,2000,48000,120,2,4);
 require(b.offset==1000 && b.frames==1000 && b.sourceFrame==0,"late entry");
 auto c=overlap(48000,512,48000,120,2,4,1.25);
 require(c.frames==512 && c.sourceFrame==60000,"source offset");
 auto d=overlap(143000,2000,48000,120,2,4);
 require(d.frames==1000 && d.sourceFrame==95000,"clip end");
 require(overlap(0,512,48000,120,4,1).frames==0,"before clip");
 require(overlap(0,512,48000,0,0,1).frames==0,"invalid bpm");
 std::cout<<"PASS: shared-clock clip window tests\n";
}
