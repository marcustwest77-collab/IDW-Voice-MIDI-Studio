#pragma once
// Text wire message parser for the prototype control channel.
// Format: version|epoch|sequence|request-id|action|track-index
// Only parses data. It does not verify network identity or execute commands.
#include "RemoteCommandValidation.h"
#include <array>
#include <cstdint>
#include <limits>
#include <string>
namespace idw::daw::remote {
inline bool parseDecimal(const std::string& s, std::uint64_t& value) noexcept {
    if (s.empty() || s.size() > 20) return false;
    std::uint64_t n=0;
    for (char c : s) {
        if (c<'0'||c>'9') return false;
        const unsigned d=static_cast<unsigned>(c-'0');
        if(n>(std::numeric_limits<std::uint64_t>::max()-d)/10) return false;
        n=n*10+d;
    }
    value=n;
    return true;
}
constexpr std::size_t maxControlPayloadForParser=4096;
inline bool parseControlCommand(const std::string& payload, CommandMessage& out) {
    if(payload.empty() || payload.size()>maxControlPayloadForParser) return false;
    std::array<std::string,6> fields;
    std::size_t start=0;
    for(std::size_t i=0;i<fields.size();++i) {
        const auto end=payload.find('|',start);
        if((end==std::string::npos)!=(i==fields.size()-1)) return false;
        fields[i]=payload.substr(start,(end==std::string::npos?payload.size():end)-start);
        start=(end==std::string::npos?payload.size():end+1);
    }
    std::uint64_t version=0,epoch=0,sequence=0,index=0;
    if(!parseDecimal(fields[0],version)||version!=1||
       !parseDecimal(fields[1],epoch)||epoch==0||
       !parseDecimal(fields[2],sequence)||sequence==0||
       !validId(fields[3])||!parseDecimal(fields[5],index)||
       index>std::numeric_limits<std::size_t>::max()) return false;
    Action action;
    if(fields[4]=="view") action=Action::view;
    else if(fields[4]=="select") action=Action::select_track;
    else if(fields[4]=="arm") action=Action::arm_track;
    else if(fields[4]=="record") action=Action::start_recording;
    else if(fields[4]=="stop-record") action=Action::stop_recording;
    else if(fields[4]=="gain") action=Action::set_gain;
    else if(fields[4]=="pan") action=Action::set_pan;
    else return false; // explicitly disallow destructive commands on the wire
    CommandMessage candidate;
    candidate.protocolVersion=1;
    candidate.sessionEpoch=epoch;
    candidate.sequence=sequence;
    candidate.requestId=fields[3];
    candidate.action=action;
    candidate.trackIndex=static_cast<std::size_t>(index);
    out=candidate;
    return true;
}
} // namespace idw::daw::remote
