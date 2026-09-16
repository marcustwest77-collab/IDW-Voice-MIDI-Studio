#include "BeatboxClassifier.h"
#include <cmath>
void BeatboxClassifier::prepare(double){prev=0;cooldown=0;}
BeatboxClassifier::Kind BeatboxClassifier::process(const float*x,int n,float th){if(!x||n<8)return None;double e=0,z=0;for(int i=0;i<n;i++)e+=x[i]*x[i];float r=(float)std::sqrt(e/n);
for(int i=1;i<n;i++)if((x[i]>=0)!=(x[i-1]>=0))z++;float q=(float)(z/n);if(cooldown>0)--cooldown;float rise=r-prev;prev=r;if(cooldown||rise<th)return None;cooldown=4;return q<.08f?Kick:q>.22f?Hat:Snare;}
