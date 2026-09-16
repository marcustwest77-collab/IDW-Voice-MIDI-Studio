#pragma once
#include <JuceHeader.h>
class ScaleManager{public:void setMask(uint16_t m){mask=m&0xfff;}uint16_t getMask()const{return mask;}bool enabled(int pc)const{return mask&(1u<<pc);}
void toggle(int pc){mask^=(1u<<juce::jlimit(0,11,pc));}int quantize(int,int)const;
private:uint16_t mask=0x0ab5;};
