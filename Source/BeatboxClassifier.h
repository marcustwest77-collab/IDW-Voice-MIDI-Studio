#pragma once
#include <JuceHeader.h>
class BeatboxClassifier{public:enum Kind{None,Kick,Snare,Hat};void prepare(double);Kind process(const float*,int,float);
private:float prev=0;int cooldown=0;};
