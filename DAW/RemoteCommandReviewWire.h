#pragma once
// Data-only records for a FUTURE process-verified private approval channel.
// No authentication, freshness, deadline or host authority is supplied by decoding.
#include "RemoteTLSOfferWire.h"
#include "RemoteControlCommandParser.h"
#include <vector>
#include <algorithm>

namespace idw::daw::remote {
struct LocalCommandProposal {
    std::uint64_t generation=0;
    std::string reviewOwner,peerId;
    LocalTLSOffer offer;
    std::string wire;
};
constexpr std::size_t maxCommandReviewBytes=382, maxCommandDecisionBytes=388;
using CommandReviewBytes=std::vector<unsigned char>;
inline bool validReviewOwner(const std::string& owner) {
    if(owner.size()!=32) return false;
    for(const char c:owner) if(!((c>='0' && c<='9') || (c>='a' && c<='f'))) return false;
    return true;
}
inline bool encodeCommandProposal(const LocalCommandProposal& value,CommandReviewBytes& out) {
    LocalTLSOfferBytes offer{};CommandMessage command;
    if(value.generation==0 || !validReviewOwner(value.reviewOwner) || !validId(value.peerId) ||
       !encodeTLSOffer(value.offer,offer) || value.wire.size()>128 || !parseControlCommand(value.wire,command) ||
       command.sessionEpoch!=value.offer.epoch || command.trackIndex>=128 ||
       (command.action!=Action::select_track && command.action!=Action::arm_track)) return false;
    const auto canonical="1|"+std::to_string(command.sessionEpoch)+"|"+std::to_string(command.sequence)+"|"+
        command.requestId+"|"+(command.action==Action::select_track ? "select" : "arm")+"|"+std::to_string(command.trackIndex);
    if(canonical!=value.wire) return false;
    CommandReviewBytes bytes{'I','D','W','Q',1};bytes.reserve(maxCommandReviewBytes);
    for(unsigned i=0;i<8;++i) bytes.push_back(static_cast<unsigned char>(value.generation>>((7-i)*8)));
    bytes.insert(bytes.end(),value.reviewOwner.begin(),value.reviewOwner.end());
    bytes.push_back(static_cast<unsigned char>(value.peerId.size()));
    bytes.insert(bytes.end(),value.peerId.begin(),value.peerId.end());
    bytes.insert(bytes.end(),offer.begin(),offer.end());
    bytes.push_back(static_cast<unsigned char>(value.wire.size()>>8));
    bytes.push_back(static_cast<unsigned char>(value.wire.size()));
    bytes.insert(bytes.end(),value.wire.begin(),value.wire.end());out=std::move(bytes);return true;
}
inline bool decodeCommandProposal(const CommandReviewBytes& bytes,LocalCommandProposal& out) {
    if(bytes.size()<192 || bytes.size()>maxCommandReviewBytes ||
       bytes[0]!='I' || bytes[1]!='D' || bytes[2]!='W' || bytes[3]!='Q' || bytes[4]!=1) return false;
    const auto peerSize=static_cast<std::size_t>(bytes[45]);
    if(peerSize==0 || peerSize>64 || bytes.size()<191+peerSize) return false;
    const auto wireSize=(static_cast<std::size_t>(bytes[188+peerSize])<<8)|bytes[189+peerSize];
    if(wireSize==0 || wireSize>128 || bytes.size()!=190+peerSize+wireSize) return false;
    LocalCommandProposal value;
    for(unsigned i=0;i<8;++i) value.generation=(value.generation<<8)|bytes[5+i];
    value.reviewOwner.assign(bytes.begin()+13,bytes.begin()+45);
    value.peerId.assign(bytes.begin()+46,bytes.begin()+46+peerSize);
    LocalTLSOfferBytes offer{};
    std::copy_n(bytes.begin()+46+peerSize,offer.size(),offer.begin());
    if(!decodeTLSOffer(offer,value.offer)) return false;
    value.wire.assign(bytes.begin()+190+peerSize,bytes.end());
    CommandReviewBytes checked;if(!encodeCommandProposal(value,checked) || checked!=bytes) return false;
    out=std::move(value);return true;
}
inline bool encodeCommandDecision(const LocalCommandProposal& proposal,bool allow,CommandReviewBytes& out) {
    CommandReviewBytes encoded;if(!encodeCommandProposal(proposal,encoded)) return false;
    CommandReviewBytes bytes{'I','D','W','D',1,static_cast<unsigned char>(allow?1:0)};
    bytes.insert(bytes.end(),encoded.begin(),encoded.end());out=std::move(bytes);return true;
}
inline bool matchCommandDecision(const CommandReviewBytes& bytes,const LocalCommandProposal& expected,bool& allow) {
    if(bytes.size()<198 || bytes.size()>maxCommandDecisionBytes || bytes[0]!='I' || bytes[1]!='D' ||
       bytes[2]!='W' || bytes[3]!='D' || bytes[4]!=1 || bytes[5]>1) return false;
    CommandReviewBytes encoded;if(!encodeCommandProposal(expected,encoded) || bytes.size()!=6+encoded.size() ||
        !std::equal(encoded.begin(),encoded.end(),bytes.begin()+6)) return false;
    allow=bytes[5]==1;return true;
}
}
