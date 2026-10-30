#pragma once
// Data reported by the trusted TLS bridge over a process-verified private pipe.
// Decoding does not verify TLS or grant consent. Host must check its peer policy.
#include "RemoteLocalBootstrapWire.h"
namespace idw::daw::remote {
struct LocalTLSOffer {
    std::uint64_t epoch=0;
    std::string peerSha256, connectionId;
    unsigned tlsVersion=0; // 12 or 13, never a boolean verification claim.
};
using LocalTLSOfferBytes=std::array<unsigned char,142>;
inline bool encodeTLSOffer(const LocalTLSOffer& value,LocalTLSOfferBytes& out) {
    if(value.epoch==0 || !validBootstrapHex(value.peerSha256) ||
       !validBootstrapHex(value.connectionId) || (value.tlsVersion!=12 && value.tlsVersion!=13)) return false;
    LocalTLSOfferBytes bytes{};
    bytes[0]='I';bytes[1]='D';bytes[2]='W';bytes[3]='O';bytes[4]=1;
    for(unsigned i=0;i<8;++i) bytes[5+i]=static_cast<unsigned char>(value.epoch>>((7-i)*8));
    for(std::size_t i=0;i<64;++i) {bytes[13+i]=static_cast<unsigned char>(value.peerSha256[i]);
        bytes[77+i]=static_cast<unsigned char>(value.connectionId[i]);}
    bytes[141]=static_cast<unsigned char>(value.tlsVersion);out=bytes;return true;
}
inline bool decodeTLSOffer(const LocalTLSOfferBytes& bytes,LocalTLSOffer& out) {
    if(bytes[0]!='I' || bytes[1]!='D' || bytes[2]!='W' || bytes[3]!='O' || bytes[4]!=1) return false;
    LocalTLSOffer value;
    for(unsigned i=0;i<8;++i) value.epoch=(value.epoch<<8)|bytes[5+i];
    value.peerSha256.assign(bytes.begin()+13,bytes.begin()+77);
    value.connectionId.assign(bytes.begin()+77,bytes.begin()+141);value.tlsVersion=bytes[141];
    LocalTLSOfferBytes check{};if(!encodeTLSOffer(value,check)) return false;
    out=std::move(value);return true;
}
}
