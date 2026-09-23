#pragma once
#include <array>
#include <deque>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace idw {
struct HistoryEvent { double seconds=0;std::array<unsigned char,3> data{};int size=0; };
// Message-thread history. No audio or files; retains at most 30 seconds / 30000 events.
class RollingMidiHistory {
public:
    explicit RollingMidiHistory(size_t limit=30000):capacity(std::max(size_t(1),limit)){}
    void clear(double now=0){events.clear();base={};start=std::max(0.0,now);trimmedTo=start;clock=start;limited=false;}
    void append(const HistoryEvent& e){
        if(!std::isfinite(e.seconds)||e.seconds<0||e.size<1||e.size>3)return;
        if(!events.empty()&&e.seconds<events.back().seconds)return;
        events.push_back(e);clock=std::max(clock,e.seconds);
        while(events.size()>capacity){trimmedTo=std::max(trimmedTo,events.front().seconds);base.apply(events.front());events.pop_front();limited=true;}
        advance(clock);
    }
    void advance(double now){
        if(!std::isfinite(now))return;clock=std::max(clock,now);
        const double cutoff=begin();
        while(!events.empty()&&events.front().seconds<cutoff){base.apply(events.front());events.pop_front();}
    }
    double duration() const{return std::max(0.0,clock-begin());}
    bool capacityLimited() const{return limited;}
    size_t retained() const{return events.size();}
    std::vector<HistoryEvent> snapshot() const {
        std::vector<HistoryEvent> result;const double origin=begin();
        auto emit=[&](int status,int a,int b,int size=3){HistoryEvent e;e.data={(unsigned char)status,(unsigned char)a,(unsigned char)b};e.size=size;result.push_back(e);};
        for(int ch=0;ch<16;++ch){const auto& c=base.channels[(size_t)ch];
            if(c.program>=0)emit(0xc0+ch,c.program,0,2);
            for(int cc=0;cc<120;++cc)if(c.cc[(size_t)cc]>=0&&cc!=6&&cc!=38&&cc!=100&&cc!=101)emit(0xb0+ch,cc,c.cc[(size_t)cc]);
            if(c.range>=0){emit(0xb0+ch,101,0);emit(0xb0+ch,100,0);emit(0xb0+ch,6,c.range);emit(0xb0+ch,38,c.cents);emit(0xb0+ch,101,127);emit(0xb0+ch,100,127);}
            if(c.touched)emit(0xe0+ch,c.wheel&127,(c.wheel>>7)&127);
            for(int n=0;n<128;++n)if(c.velocity[(size_t)n]){emit(0x90+ch,n,c.velocity[(size_t)n]);if(!c.key[(size_t)n])emit(0x80+ch,n,0);}
        }
        for(auto e:events)if(e.seconds>=origin&&e.seconds<=clock){e.seconds-=origin;result.push_back(e);}
        bool music=false;std::array<bool,16>used{};
        for(const auto& e:result){if((e.data[0]&0xf0)==0x90&&e.size==3&&e.data[2]>0)music=true;if(e.data[0]>=0x80&&e.data[0]<0xf0)used[e.data[0]&15]=true;}
        if(!music)return {};
        for(int ch=0;ch<16;++ch)if(used[(size_t)ch]){HistoryEvent e;e.seconds=duration();e.size=3;e.data={(unsigned char)(0xb0+ch),64,0};result.push_back(e);e.data[1]=123;result.push_back(e);}
        return result;
    }
private:
    struct Channel {
        std::array<int,128>cc;std::array<int,128>velocity{};std::array<bool,128>key{};
        int wheel=8192,program=-1,rpnMSB=127,rpnLSB=127,range=-1,cents=0;bool touched=false;
        Channel(){cc.fill(-1);}
    };
    struct State {
        std::array<Channel,16>channels;
        void apply(const HistoryEvent& e){
            const int type=e.data[0]&0xf0,ch=e.data[0]&15,a=e.data[1]&127,b=e.data[2]&127;
            if(e.data[0]<0x80||e.data[0]>=0xf0)return;auto& c=channels[(size_t)ch];c.touched=true;
            if(type==0xc0&&e.size>=2)c.program=a;
            if(e.size<3)return;
            if(type==0x90&&b>0){c.key[(size_t)a]=true;c.velocity[(size_t)a]=b;}
            if(type==0x80||(type==0x90&&b==0)){c.key[(size_t)a]=false;if(c.cc[64]<64)c.velocity[(size_t)a]=0;}
            if(type==0xe0)c.wheel=a+128*b;
            if(type!=0xb0)return;
            if(a==120){c.key.fill(false);c.velocity.fill(0);return;}
            if(a==123){c.key.fill(false);if(c.cc[64]<64)c.velocity.fill(0);return;}
            if(a==121){c.cc.fill(-1);c.wheel=8192;c.rpnMSB=c.rpnLSB=127;for(int n=0;n<128;++n)if(!c.key[(size_t)n])c.velocity[(size_t)n]=0;return;}
            c.cc[(size_t)a]=b;
            if(a==64&&b<64)for(int n=0;n<128;++n)if(!c.key[(size_t)n])c.velocity[(size_t)n]=0;
            if(a==101)c.rpnMSB=b;if(a==100)c.rpnLSB=b;
            if(c.rpnMSB==0&&c.rpnLSB==0){if(a==6)c.range=b;if(a==38)c.cents=b;}
        }
    };
    double begin() const{return std::max({start,clock-30.0,trimmedTo});}
    size_t capacity;std::deque<HistoryEvent>events;State base;double start=0,trimmedTo=0,clock=0;bool limited=false;
};
}
