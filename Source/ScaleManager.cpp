#include "ScaleManager.h"
int ScaleManager::quantize(int n,int root)const{int b=n,d=99;for(int x=juce::jmax(0,n-6);x<=juce::jmin(127,n+6);x++){int pc=(x-root+120)%12;if(enabled(pc)&&abs(x-n)<d){b=x;d=abs(x-n);}}return b;}
