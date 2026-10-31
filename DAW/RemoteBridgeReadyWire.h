#pragma once
// Non-secret discovery only. Even process-verified readiness is not TLS identity,
// local consent, native admission or evidence that a DAW action was executed.
#include <array>
#include <cstdint>
namespace idw::daw::remote {
struct LocalBridgeReady {
    std::uint64_t epoch=0;
    std::uint16_t tlsPort=0; // IPv4 loopback TLS listener, not the native command socket.
};
using LocalBridgeReadyBytes=std::array<unsigned char,15>;
inline bool encodeBridgeReady(const LocalBridgeReady& value,LocalBridgeReadyBytes& out) {
    if(value.epoch==0 || value.tlsPort==0) return false;
    LocalBridgeReadyBytes bytes{};
    bytes[0]='I';bytes[1]='D';bytes[2]='W';bytes[3]='R';bytes[4]=1;
    for(unsigned i=0;i<8;++i) bytes[5+i]=static_cast<unsigned char>(value.epoch>>((7-i)*8));
    bytes[13]=static_cast<unsigned char>(value.tlsPort>>8);
    bytes[14]=static_cast<unsigned char>(value.tlsPort);out=bytes;return true;
}
inline bool decodeBridgeReady(const LocalBridgeReadyBytes& bytes,LocalBridgeReady& out) {
    if(bytes[0]!='I' || bytes[1]!='D' || bytes[2]!='W' || bytes[3]!='R' || bytes[4]!=1) return false;
    LocalBridgeReady value;
    for(unsigned i=0;i<8;++i) value.epoch=(value.epoch<<8)|bytes[5+i];
    value.tlsPort=static_cast<std::uint16_t>((static_cast<unsigned>(bytes[13])<<8)|bytes[14]);
    LocalBridgeReadyBytes checked{};if(!encodeBridgeReady(value,checked)) return false;
    out=value;return true;
}
}
