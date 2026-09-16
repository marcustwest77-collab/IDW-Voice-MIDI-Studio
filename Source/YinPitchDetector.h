#pragma once
#include <JuceHeader.h>
class YinPitchDetector{
public:void prepare(double,int);void reset();float process(const float*,int);float confidence()const{return conf;}float rms()const{return level;}
private:double sr=44100;std::vector<float>ring,frame,diff,cmnd;int pos=0;float conf=0,level=0;float threshold=.15f;float detect();
};
