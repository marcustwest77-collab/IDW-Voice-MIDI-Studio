#include "../RemoteBridgeReadyWire.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
    LocalBridgeReady value{45,12345},decoded;LocalBridgeReadyBytes bytes{};
    assert(encodeBridgeReady(value,bytes));
    assert(bytes.size()==15 && bytes[4]==1 && bytes[12]==45 && bytes[13]==0x30 && bytes[14]==0x39);
    assert(decodeBridgeReady(bytes,decoded) && decoded.epoch==45 && decoded.tlsPort==12345);
    for(unsigned offset=0;offset<5;++offset) {
        auto bad=bytes;bad[offset]=255;
        assert(!decodeBridgeReady(bad,decoded) && decoded.epoch==45 && decoded.tlsPort==12345);
    }
    auto bad=bytes;for(unsigned i=5;i<13;++i) bad[i]=0;
    assert(!decodeBridgeReady(bad,decoded));
    bad=bytes;bad[13]=bad[14]=0;assert(!decodeBridgeReady(bad,decoded));
    assert(!encodeBridgeReady({0,1},bytes));assert(!encodeBridgeReady({1,0},bytes));
    value={~std::uint64_t{0},65535};
    assert(encodeBridgeReady(value,bytes) && decodeBridgeReady(bytes,decoded));
    assert(decoded.epoch==value.epoch && decoded.tlsPort==65535);
}
