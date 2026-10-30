#include "../RemoteTLSOfferWire.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
    LocalTLSOffer offer{45,std::string(64,'a'),std::string(64,'b'),13},decoded;
    LocalTLSOfferBytes bytes{};assert(encodeTLSOffer(offer,bytes));
    assert(bytes.size()==142 && bytes[4]==1 && bytes[12]==45 && bytes[141]==13);
    assert(decodeTLSOffer(bytes,decoded));assert(decoded.epoch==45 && decoded.tlsVersion==13);
    assert(decoded.peerSha256==offer.peerSha256 && decoded.connectionId==offer.connectionId);
    for(const auto offset:{0,4,13,77,141}) {
        auto bad=bytes;bad[static_cast<std::size_t>(offset)]=255;
        assert(!decodeTLSOffer(bad,decoded));assert(decoded.epoch==45);
    }
    auto bad=bytes;for(std::size_t i=5;i<13;++i) bad[i]=0;
    assert(!decodeTLSOffer(bad,decoded));
    offer.epoch=~std::uint64_t{0};offer.tlsVersion=12;
    assert(encodeTLSOffer(offer,bytes) && decodeTLSOffer(bytes,decoded));assert(decoded.epoch==offer.epoch);
    offer.peerSha256="bad";assert(!encodeTLSOffer(offer,bytes));
}
