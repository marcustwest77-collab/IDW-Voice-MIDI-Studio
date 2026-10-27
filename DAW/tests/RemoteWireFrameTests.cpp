#include "../RemoteWireFrame.h"
#ifdef NDEBUG
#undef NDEBUG // This executable uses assert as its test runner, even in Release.
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
    std::vector<std::uint8_t> b;
    std::string result="untouched";
    assert(encodeFrame("hello",b));
    assert(b.size()==9 && b[3]==5);
    assert(decodeFrame(b,result)==FrameResult::complete && result=="hello");
    b.pop_back(); result="sentinel";
    assert(decodeFrame(b,result)==FrameResult::incomplete && result=="sentinel");
    b={0,0,0,0};
    assert(decodeFrame(b,result)==FrameResult::invalid);
    b={0xff,0xff,0xff,0xff};
    assert(decodeFrame(b,result)==FrameResult::invalid);
    assert(!encodeFrame(std::string(maxControlPayload+1,'x'),b));
    assert(!encodeFrame("",b));
    assert(encodeFrame(std::string(maxControlPayload,'x'),b));
    assert(decodeFrame(b,result)==FrameResult::complete && result.size()==maxControlPayload);
    b.push_back(0);
    assert(decodeFrame(b,result)==FrameResult::invalid);
}
