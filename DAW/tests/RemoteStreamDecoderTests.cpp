#include "../RemoteStreamDecoder.h"
#include <cassert>
using namespace idw::daw::remote;
int main(){
    StreamDecoder decoder;
    std::vector<std::uint8_t> one, two;
    assert(encodeFrame("first",one)); assert(encodeFrame("second",two));
    std::string out="unchanged";
    assert(decoder.feed(one.data(),2,out)==StreamDecoder::Result::need_more);
    assert(out=="unchanged");
    assert(decoder.feed(one.data()+2,one.size()-2,out)==StreamDecoder::Result::message);
    assert(out=="first");
    std::vector<std::uint8_t> both=one; both.insert(both.end(),two.begin(),two.end());
    decoder.reset();
    assert(decoder.feed(both.data(),both.size(),out)==StreamDecoder::Result::message);
    assert(out=="first");
    assert(decoder.feed(nullptr,0,out)==StreamDecoder::Result::message);
    assert(out=="second");
    assert(decoder.feed(nullptr,0,out)==StreamDecoder::Result::need_more);
    unsigned char bad[]={0xff,0xff,0xff,0xff};
    assert(decoder.feed(bad,4,out)==StreamDecoder::Result::invalid);
    assert(decoder.failed());
    assert(decoder.feed(one.data(),one.size(),out)==StreamDecoder::Result::invalid);
    decoder.reset();
    assert(decoder.feed(one.data(),one.size(),out)==StreamDecoder::Result::message);
    std::vector<std::uint8_t> flood(9000,0);
    decoder.reset();
    assert(decoder.feed(flood.data(),flood.size(),out)==StreamDecoder::Result::invalid);
}
