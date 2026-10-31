#include "../RemoteLocalOfferConsent.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <thread>
using namespace idw::daw::remote;
using namespace std::chrono_literals;
LocalTLSOffer offer(){return {45,std::string(64,'a'),std::string(64,'b'),13};}
LocalTLSOffer awaitPrompt(LocalOfferConsent& consent) {
    const auto deadline=LocalOfferConsent::Clock::now()+1s;
    while(LocalOfferConsent::Clock::now()<deadline) {
        if(auto pending=consent.pending()) return *pending;
        std::this_thread::yield();
    }
    assert(false);return {};
}
int main() {
    {
        LocalOfferConsent consent;LocalHostConsent grant;std::atomic<bool> cancel{false};bool accepted=false;
        assert(!consent.pending());assert(!consent.resolve(offer(),true));
        std::thread worker([&]{accepted=consent.request(offer(),grant,cancel,1s);});
        const auto prompt=awaitPrompt(consent);
        for(unsigned field=0;field<4;++field) {
            auto wrong=prompt;
            if(field==0) ++wrong.epoch;
            if(field==1) wrong.peerSha256[0]='c';
            if(field==2) wrong.connectionId[0]='c';
            if(field==3) wrong.tlsVersion=12;
            assert(!consent.resolve(wrong,true));assert(consent.pending());
        }
        assert(consent.resolve(prompt,true));assert(!consent.resolve(prompt,true));
        worker.join();assert(accepted && grant.hostApproved && grant.trackControlGranted);
        assert(grant.epoch==prompt.epoch && grant.peerSha256==prompt.peerSha256 && grant.connectionId==prompt.connectionId);
        assert(!consent.pending());assert(!consent.request(offer(),grant,cancel,1s));
    }
    for(unsigned mode=0;mode<4;++mode) {
        LocalOfferConsent consent;LocalHostConsent grant;grant.epoch=99;
        std::atomic<bool> cancel{false};bool accepted=true;
        std::thread worker([&]{accepted=consent.request(offer(),grant,cancel,mode==3?100ms:1s);});
        const auto prompt=awaitPrompt(consent);
        if(mode==0) assert(consent.resolve(prompt,false));
        if(mode==1) cancel.store(true);
        if(mode==2) consent.close();
        worker.join();assert(!accepted && grant.epoch==99 && !grant.hostApproved);
        assert(!consent.pending());assert(!consent.resolve(prompt,true));
    }
    {
        LocalOfferConsent consent;LocalHostConsent grant;std::atomic<bool> cancel{true};
        assert(!consent.request(offer(),grant,cancel,1s));cancel.store(false);
        assert(!consent.request(offer(),grant,cancel,0ms));
        auto invalid=offer();invalid.connectionId="invalid";
        assert(!consent.request(invalid,grant,cancel,1s));
        consent.close();assert(!consent.request(offer(),grant,cancel,1s));
    }
    {
        LocalOfferConsent consent;LocalHostConsent grant;std::atomic<bool> cancel{false};
        assert(!consent.request(offer(),grant,cancel,5001ms)); // Legacy duration limit remains five seconds.
        assert(!consent.requestUntil(offer(),grant,cancel,LocalOfferConsent::Clock::now()-1ms));
        assert(!consent.requestUntil(offer(),grant,cancel,LocalOfferConsent::Clock::now()+31s));
        const auto deadline=LocalOfferConsent::Clock::now()+10s;
        bool accepted=false;
        std::thread worker([&]{accepted=consent.requestUntil(offer(),grant,cancel,deadline);});
        const auto prompt=awaitPrompt(consent);assert(consent.resolve(prompt,true));worker.join();assert(accepted);
    }
    {
        LocalOfferConsent consent;LocalHostConsent grant;std::atomic<bool> cancel{false};
        const auto deadline=LocalOfferConsent::Clock::now()+50ms;
        std::this_thread::sleep_for(60ms); // Budget consumed before entering the callback is never renewed.
        assert(!consent.requestUntil(offer(),grant,cancel,deadline));assert(!consent.pending());
    }
    {
        LocalOfferConsent consent;LocalHostConsent first,second;std::atomic<bool> cancel{false};
        bool accepted=true;
        std::thread worker([&]{accepted=consent.request(offer(),first,cancel,1s);});
        const auto prompt=awaitPrompt(consent);
        assert(!consent.request(offer(),second,cancel,1s));
        assert(consent.resolve(prompt,false));worker.join();assert(!accepted);
    }
}
