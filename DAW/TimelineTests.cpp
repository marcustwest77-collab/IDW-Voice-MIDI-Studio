#include "TimelineMath.h"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cfenv>
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
 // Malformed project values must not reach llround out of range or overflow
 // a block/source cursor. Invalid windows stay silent.
 std::feclearexcept(FE_ALL_EXCEPT);
 const auto maxFrame=std::numeric_limits<int64_t>::max();
 for(double bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),1.0e300}){
    require(overlap(0,512,48000,120,bad,1).frames==0,"invalid start");
    require(overlap(0,512,48000,120,0,bad).frames==0,"invalid length");
    require(overlap(0,512,48000,120,0,1,bad).frames==0,"invalid source offset");
 }
 require(overlap(maxFrame-1,512,48000,120,0,1).frames==0,"block cursor overflow");
 require(overlap(0,2048,1,60,0,2048,std::nextafter((double)maxFrame,0.0)).frames==0,
         "source cursor overflow");
 int64_t converted=123;
 require(!idw::daw::roundedSampleFrame((double)maxFrame,converted) && converted==123,"conversion upper bound");
 require(!idw::daw::roundedSampleFrame(-1,converted),"negative frame");
 require(idw::daw::roundedSampleFrame(0.5,converted) && converted==1,"half frame rounding");
 require((std::fetestexcept(FE_INVALID)&FE_INVALID)==0,"invalid float-to-frame conversion executed");
 std::cout<<"PASS: shared-clock clip window tests\n";
}
