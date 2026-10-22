#pragma once
#include <algorithm>
#include <cmath>
namespace idw {
// Editor-only time window. Audio and project coordinates remain absolute seconds.
struct SongTimelineView {
    double duration=10,zoom=1,start=0;
    double span()const{return duration/zoom;}
    void setDuration(double value){if(!std::isfinite(value)||value<=0)return;duration=value;setStart(start);}
    void setStart(double value){if(std::isfinite(value))start=std::clamp(value,0.,std::max(0.,duration-span()));}
    void setZoom(double value,double anchor){if(!std::isfinite(value)||!std::isfinite(anchor))return;zoom=std::clamp(value,1.,32.);setStart(anchor-span()*.5);}
    void fit(){zoom=1;start=0;}
    double timeAt(double fraction)const{return start+std::clamp(fraction,0.,1.)*span();}
    double fractionAt(double time)const{return (time-start)/span();}
    bool contains(double time)const{return time>=start&&time<=start+span();}
};
}
