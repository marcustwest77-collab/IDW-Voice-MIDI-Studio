#pragma once
#include "SessionModel.h"
#include <optional>
namespace idw::daw {
struct PianoNoteBounds {double x=0,width=0;int y=0;bool rightEdgeVisible=false;};
// View-only state; never changes session timing, pitch, duration or history.
struct PianoRollViewport {
    static constexpr int rowHeight=13,headerHeight=24,keyWidth=54;
    int topPitch=84,zoomLevel=1;
    double firstBeat=0;
    double pixelsPerBeat() const {
        constexpr double scales[]{8,16,24,32,48};return scales[std::clamp(zoomLevel,0,4)];
    }
    static int rows(int height){return std::clamp(height/rowHeight,1,128);}
    void constrain(int width,int height,double lastBeat){
        topPitch=std::clamp(topPitch,rows(height)-1,127);
        zoomLevel=std::clamp(zoomLevel,0,4);
        if(!std::isfinite(firstBeat))firstBeat=0;
        if(!std::isfinite(lastBeat) || lastBeat<0)lastBeat=0;
        const double maxScroll=std::max(0.0,lastBeat+4.0-std::max(1,width)/pixelsPerBeat());
        firstBeat=std::clamp(firstBeat,0.0,maxScroll);
    }
    void scrollPitch(double delta,int height){
        if(!std::isfinite(delta))return;
        topPitch=(int)std::round(std::clamp(topPitch+delta,(double)(rows(height)-1),127.0));
    }
    void scrollTime(double delta,int width,int height,double lastBeat){
        if(std::isfinite(delta))firstBeat+=delta;
        constrain(width,height,lastBeat);
    }
    void zoomAt(int delta,double x,int width,int height,double lastBeat){
        if(!std::isfinite(x))return;
        x=std::clamp(x,0.0,(double)std::max(1,width));
        const double anchor=firstBeat+x/pixelsPerBeat();
        zoomLevel=std::clamp(zoomLevel+std::clamp(delta,-4,4),0,4);
        firstBeat=anchor-x/pixelsPerBeat();constrain(width,height,lastBeat);
    }
    double beatAt(double x) const {return std::max(0.0,firstBeat+x/pixelsPerBeat());}
    std::optional<int> pitchAt(int y,int height) const {
        if(y<0 || y>=rows(height)*rowHeight)return {};
        return topPitch-y/rowHeight;
    }
    std::optional<PianoNoteBounds> bounds(const MidiNote& n,int width,int height) const {
        if(width<=0 || n.pitch<0 || n.pitch>127 || n.pitch>topPitch ||
           n.pitch<topPitch-rows(height)+1 || !std::isfinite(n.startBeat) ||
           !std::isfinite(n.lengthBeats) || n.startBeat<0 || n.lengthBeats<=0)return {};
        const double end=n.startBeat+n.lengthBeats,last=firstBeat+width/pixelsPerBeat();
        if(!std::isfinite(end) || end<=firstBeat || n.startBeat>=last)return {};
        const double x=(std::max(firstBeat,n.startBeat)-firstBeat)*pixelsPerBeat();
        const double right=(std::min(last,end)-firstBeat)*pixelsPerBeat();
        return PianoNoteBounds{x,std::min((double)width-x,std::max(3.0,right-x)),
                               (topPitch-n.pitch)*rowHeight,end<=last};
    }
};
}
