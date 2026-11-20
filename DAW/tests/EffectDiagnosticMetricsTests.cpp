#include "EffectDiagnosticMetrics.h"
#include <iostream>
#include <limits>
using namespace idw::daw;
int main(){
    EffectDiagnosticMetrics m;const float a[]{0,0.25f},b[]{0.5f,0.75f};
    m.observe(a,a,2,0.01,1000);m.observe(b,b,2,3,1000);
    if(m.samples!=4 || m.blocks!=2 || m.peak!=0.75 || m.maxBoundaryJump!=0.25 ||
        m.maxInteriorJump!=0.25 || m.overDeadline!=1 || m.maxProcessMs!=3)return 1;
    const float c[]{-2,std::numeric_limits<float>::quiet_NaN()};m.observe(c,c,2,0,1000);
    if(m.nonfinite!=2 || m.overUnity!=2 || m.peak!=2 || m.maxBoundaryJump!=2.75)return 1;
    const auto count=m.blocks;m.observe(nullptr,a,2,0,1000);m.observe(a,a,0,0,1000);m.observe(a,a,2,0,0);
    if(m.blocks!=count)return 1;
    std::cout<<"Effect diagnostic metric tests passed\n";return 0;
}
