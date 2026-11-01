#include "../RemoteLocalCommandConsent.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <thread>
using namespace idw::daw::remote;
using namespace std::chrono_literals;
LocalCommandProposal proposal(){return {7,std::string(32,'c'),"guest",
    {45,std::string(64,'a'),std::string(64,'b'),13},"1|45|1|review|arm|1"};}
LocalCommandPrompt awaitPrompt(LocalCommandConsent& consent) {
    const auto until=LocalCommandConsent::Clock::now()+1s;
    while(LocalCommandConsent::Clock::now()<until) {
        if(const auto prompt=consent.pending()) return *prompt;
        std::this_thread::yield();
    }
    assert(false);return {};
}
int main(){
    for(const bool decision:{false,true}) {
        LocalCommandConsent consent;std::atomic<bool> cancel{false};bool result=false,allow=!decision;
        std::thread worker([&]{result=consent.request(proposal(),allow,cancel,LocalCommandConsent::Clock::now()+1s);});
        const auto prompt=awaitPrompt(consent);
        for(unsigned field=0;field<9;++field) {
            auto wrong=prompt;
            if(field==0) wrong.owner=std::make_shared<const int>(0);
            if(field==1) ++wrong.proposal.generation;
            if(field==2) wrong.proposal.reviewOwner[0]='d';
            if(field==3) wrong.proposal.peerId="other";
            if(field==4) ++wrong.proposal.offer.epoch;
            if(field==5) wrong.proposal.offer.peerSha256[0]='d';
            if(field==6) wrong.proposal.offer.connectionId[0]='d';
            if(field==7) wrong.proposal.offer.tlsVersion=12;
            if(field==8) wrong.proposal.wire.back()='2';
            assert(!consent.resolve(wrong,true));assert(consent.pending());
        }
        assert(consent.resolve(prompt,decision));assert(!consent.resolve(prompt,!decision));
        worker.join();assert(result && allow==decision && !consent.pending());
        assert(!consent.request(proposal(),allow,cancel,LocalCommandConsent::Clock::now()+1s));
        // An identical wire proposal in a replacement mailbox has a different
        // local owner token, so a queued old UI callback cannot resolve it.
        LocalCommandConsent replacement;result=false;
        std::thread next([&]{result=replacement.request(proposal(),allow,cancel,LocalCommandConsent::Clock::now()+1s);});
        const auto fresh=awaitPrompt(replacement);
        assert(fresh.owner!=prompt.owner && !replacement.resolve(prompt,true));
        assert(replacement.resolve(fresh,false));next.join();assert(result && !allow);
    }
    for(unsigned mode=0;mode<3;++mode) {
        LocalCommandConsent consent;std::atomic<bool> cancel{false};bool result=true,allow=true;
        std::thread worker([&]{result=consent.request(proposal(),allow,cancel,
            LocalCommandConsent::Clock::now()+(mode==2?100ms:1s));});
        const auto prompt=awaitPrompt(consent);
        if(mode==0) cancel.store(true);
        if(mode==1) consent.close();
        worker.join();assert(!result && allow && !consent.pending() && !consent.resolve(prompt,true));
    }
    LocalCommandConsent consent;std::atomic<bool> cancel{false};bool allow=true;
    auto invalid=proposal();invalid.wire="1|45|1|review|record|1";
    assert(!consent.request(invalid,allow,cancel,LocalCommandConsent::Clock::now()+1s));
    assert(!consent.request(proposal(),allow,cancel,LocalCommandConsent::Clock::now()-1ms));
    assert(!consent.request(proposal(),allow,cancel,LocalCommandConsent::Clock::now()+6s));
    assert(allow && !consent.pending());
}
