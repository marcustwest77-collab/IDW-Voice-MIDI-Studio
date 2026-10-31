#include "../RemoteManagedOfferController.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
using namespace std::chrono_literals;
struct FakeManager {
    std::mutex mutex;
    std::atomic<bool> canceled{false},entered{false};
    std::atomic<unsigned> admitted{0},calls{0},stops{0};
    bool pauseAtEntry=false,throwOnStart=false;
    bool startWithOffer(const std::wstring&,const std::string&,const LocalSocketReceiver::Options&,
                        std::uint64_t epoch,const std::string& peer,const WindowsBridgeSession::OfferConsent& consent,
                        unsigned,const std::atomic<bool>* ownerCanceled) {
        ++calls;entered.store(true);
        if(pauseAtEntry) while(!ownerCanceled->load()) std::this_thread::yield();
        {
            std::lock_guard<std::mutex> lock(mutex);
            if(ownerCanceled->load()) return false;
            canceled.store(false);
        }
        if(throwOnStart) throw 1;
        LocalHostConsent grant;
        const bool accepted=consent({epoch,peer,std::string(64,'b'),13},grant,canceled);
        if(!accepted || canceled.load() || ownerCanceled->load()) return false;
        assert(grant.epoch==epoch && grant.peerSha256==peer && grant.connectionId==std::string(64,'b'));
        assert(grant.hostApproved && grant.trackControlGranted);++admitted;return true;
    }
    void stop(){std::lock_guard<std::mutex> lock(mutex);++stops;canceled.store(true);}
};
template<class Test> void await(Test test) {
    const auto deadline=std::chrono::steady_clock::now()+2s;
    while(!test()) {assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
}
int main() {
    LocalSocketReceiver::Options options;
    const auto image=std::string(64,'a'),peer=std::string(64,'c');
    {
        FakeManager manager;ManagedOfferController<FakeManager> controller(manager);
        assert(!controller.pending() && !controller.takeStartupResult());
        assert(controller.start(L"test.exe",image,options,45,peer,1000));
        assert(!controller.start(L"test.exe",image,options,46,peer,1000));
        await([&]{return bool(controller.pending());});const auto old=*controller.pending();
        assert(manager.admitted.load()==0 && !controller.takeStartupResult());
        auto wrong=old;++wrong.generation;assert(!controller.resolve(wrong,true));
        assert(controller.resolve(old,true));
        std::optional<bool> result;
        await([&]{result=controller.takeStartupResult();return bool(result);});
        assert(*result && manager.admitted.load()==1 && !controller.takeStartupResult());
        assert(!controller.resolve(old,true));controller.stop();assert(!controller.busy());
        // Even identical TLS metadata cannot authorize a replacement controller operation.
        assert(controller.start(L"test.exe",image,options,45,peer,1000));
        await([&]{return bool(controller.pending());});
        assert(controller.pending()->generation!=old.generation && !controller.resolve(old,true));
        assert(controller.resolve(*controller.pending(),false));
        await([&]{result=controller.takeStartupResult();return bool(result);});
        assert(!*result && manager.admitted.load()==1);controller.stop();
    }
    for(unsigned mode=0;mode<4;++mode) {
        FakeManager manager;manager.pauseAtEntry=mode==1;manager.throwOnStart=mode==3;
        ManagedOfferController<FakeManager> controller(manager);
        assert(controller.start(L"test.exe",image,options,45,peer,mode==2?50:1000));
        if(mode==0) {await([&]{return bool(controller.pending());});controller.stop();}
        else if(mode==1) {await([&]{return manager.entered.load();});controller.stop();}
        else {
            std::optional<bool> result;
            await([&]{result=controller.takeStartupResult();return bool(result);});assert(!*result);
        }
        assert(manager.admitted.load()==0 && !controller.pending());
    }
    {
        FakeManager manager;ManagedOfferController<FakeManager> controller(manager);
        assert(!controller.start(L"test.exe","bad",options,45,peer));
        assert(!controller.start(L"test.exe",image,options,0,peer));
        assert(!controller.start(L"test.exe",image,options,45,peer,0));
        assert(manager.calls.load()==0);
    }
}
