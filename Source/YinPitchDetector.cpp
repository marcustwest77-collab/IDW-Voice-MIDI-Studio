#include "YinPitchDetector.h"
#include <cmath>
#include <algorithm>
void YinPitchDetector::prepare(double s,int){sr=s;int n=std::max(2048,(int)(s*.05));ring.assign(n,0);frame.assign(n,0);diff.assign(n/2,0);cmnd.assign(n/2,1);reset();}
void YinPitchDetector::reset(){std::fill(ring.begin(),ring.end(),0);pos=0;conf=level=0;}
float YinPitchDetector::process(const float*in,int n){if(!in||ring.empty())return 0;double e=0;for(int i=0;i<n;i++){ring[pos]=in[i];pos=(pos+1)%(int)ring.size();e+=in[i]*in[i];}
level=(float)std::sqrt(e/std::max(1,n));for(size_t i=0;i<frame.size();i++)frame[i]=ring[(pos+(int)i)%(int)ring.size()];return detect();}
float YinPitchDetector::detect(){int half=(int)diff.size();std::fill(diff.begin(),diff.end(),0);for(int tau=1;tau<half;tau++){double d=0;for(int i=0;i<half;i++){float v=frame[i]-frame[i+tau];d+=v*v;}diff[tau]=(float)d;}
cmnd[0]=1;double run=0;for(int tau=1;tau<half;tau++){run+=diff[tau];cmnd[tau]=run>0?diff[tau]*tau/(float)run:1;}
int minTau=std::max(2,(int)(sr/1000)),maxTau=std::min(half-2,(int)(sr/70)),tau=0;
for(int t=minTau;t<=maxTau;t++)if(cmnd[t]<threshold){while(t+1<=maxTau&&cmnd[t+1]<cmnd[t])t++;tau=t;break;}
if(!tau){conf=0;return 0;}conf=juce::jlimit(0.f,1.f,1.f-cmnd[tau]);float better=(float)tau;
if(tau>1&&tau+1<half){float a=cmnd[tau-1],b=cmnd[tau],c=cmnd[tau+1],den=a-2*b+c;if(std::abs(den)>1e-6f)better+=.5f*(a-c)/den;}
return (float)(sr/better);}
