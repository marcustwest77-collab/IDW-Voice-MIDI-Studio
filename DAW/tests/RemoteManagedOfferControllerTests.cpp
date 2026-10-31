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
    bool pauseAtEntry=false,throwOnStart=false,expiredOffer=false;
    unsigned readyMode=0;
    std::atomic<bool> readyPublished{false},releaseReady{false};
    bool startWithTimedOffer(const std::wstring&,const std::string&,const LocalSocketReceiver::Options&,
                        std::uint64_t epoch,const std::string& peer,const WindowsBridgeSession::TimedOfferConsent& consent,
                        unsigned timeoutMs,const std::atomic<bool>* ownerCanceled) {
        const auto deadline=expiredOffer ? std::chrono::steady_clock::now()-1ms
            : std::chrono::steady_clock::now()+std::chrono::milliseconds(timeoutMs);
        ++calls;entered.store(true);
        if(pauseAtEntry) while(!ownerCanceled->load()) std::this_thread::yield();
        {
            std::lock_guard<std::mutex> lock(mutex);
            if(ownerCanceled->load()) return false;
            canceled.store(false);
        }
        if(throwOnStart) throw 1;
        LocalHostConsent grant;
        const bool accepted=consent({epoch,peer,std::string(64,'b'),13},grant,canceled,deadline);
        if(!accepted || canceled.load() || ownerCanceled->load()) return false;
        assert(grant.epoch==epoch && grant.peerSha256==peer && grant.connectionId==std::string(64,'b'));
        assert(grant.hostApproved && grant.trackControlGranted);++admitted;return true;
    }
    void stop(){std::lock_guard<std::mutex> lock(mutex);++stops;canceled.store(true);}
    bool startWithReadyOffer(const std::wstring& file,const std::string& image,const LocalSocketReceiver::Options& options,
                            std::uint64_t epoch,const std::string& peer,const WindowsBridgeSession::ReadyCallback& ready,
                            const WindowsBridgeSession::TimedOfferConsent& consent,unsigned timeoutMs,
                            const std::atomic<bool>* ownerCanceled) {
        const auto deadline=std::chrono::steady_clock::now()+(readyMode==1 ? -1ms :
            std::chrono::milliseconds(readyMode==4 ? 200 : timeoutMs));
        if(!ready({readyMode==2 ? epoch+1 : epoch,static_cast<std::uint16_t>(readyMode==3 ? 0 : 12345)},
                  *ownerCanceled,deadline)) return false;
        readyPublished.store(true);
        if(readyMode==4 || readyMode==5) {
            while(!releaseReady.load() && !ownerCanceled->load()) std::this_thread::yield();
            return false;
        }
        return startWithTimedOffer(file,image,options,epoch,peer,consent,timeoutMs,ownerCanceled);
    }
};
template<class Test> void await(Test test) {
    const auto deadline=std::chrono::steady_clock::now()+2s;
    while(!test()) {assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
}
int main() {
    LocalSocketReceiver::Options options;
    const auto image=std::string(64,'a'),peer=std::string(64,'c');
    for(unsigned mode=1;mode<=5;++mode) {
        FakeManager manager;manager.readyMode=mode;
        ManagedOfferController<FakeManager> controller(manager);
        assert(controller.start(L"test.exe",image,options,45,peer,1000,true));
        std::optional<bool> result;
        if(mode>=4) {
            await([&]{return manager.readyPublished.load();});
            if(mode==4) {
                // Keep the worker pending past the original readiness budget.
                assert(controller.takeReadiness() && controller.pendingReadiness());
                std::this_thread::sleep_for(250ms);
                assert(!controller.takeReadiness() && !controller.pendingReadiness());
            }
            manager.releaseReady.store(true);
        }
        await([&]{result=controller.takeStartupResult();return bool(result);});
        assert(!*result && !controller.takeReadiness() && !controller.pendingReadiness() && !controller.pending());
        assert(manager.admitted.load()==0);
        controller.stop();
        // A replacement operation cannot inherit the old port or deadline.
        manager.readyMode=0;manager.readyPublished.store(false);
        assert(controller.start(L"test.exe",image,options,46,peer,1000,true));
        std::optional<LocalBridgeReady> replacement;
        await([&]{replacement=controller.takeReadiness();return bool(replacement);});
        assert(replacement->epoch==46 && !controller.takeReadiness());
        controller.stop();
    }
    {
        FakeManager manager;ManagedOfferController<FakeManager> controller(manager);
        assert(controller.start(L"test.exe",image,options,45,peer,1000,true));
        await([&]{return bool(controller.pending());});
        assert(controller.resolve(*controller.pending(),true));
        std::optional<bool> result;
        await([&]{result=controller.takeStartupResult();return bool(result);});
        // Discovery is no longer pending after admission, either.
        assert(*result && manager.admitted.load()==1 && !controller.takeReadiness() && !controller.pendingReadiness());
        controller.stop();
    }
    {
        FakeManager manager;ManagedOfferController<FakeManager> controller(manager);
        assert(controller.start(L"test.exe",image,options,45,peer,1000,true));
        std::optional<LocalBridgeReady> ready;
        await([&]{ready=controller.takeReadiness();return bool(ready);});
        assert(ready->epoch==45 && ready->tlsPort==12345 && !controller.takeReadiness() && manager.admitted.load()==0);
        assert(controller.pendingReadiness() && controller.pendingReadiness()->tlsPort==12345);
        await([&]{return bool(controller.pending());});assert(controller.resolve(*controller.pending(),false));
        controller.stop();assert(!controller.takeReadiness() && !controller.pendingReadiness());
    }
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
        FakeManager manager;manager.expiredOffer=true;
        ManagedOfferController<FakeManager> controller(manager);
        assert(controller.start(L"test.exe",image,options,45,peer,1000));
        std::optional<bool> result;
        await([&]{
            // Renewing the relative timeout would expose a prompt that this
            // eager local controller would approve. An expired deadline cannot.
            if(const auto prompt=controller.pending()) controller.resolve(*prompt,true);
            result=controller.takeStartupResult();return bool(result);
        });
        assert(!*result && manager.admitted.load()==0);
    }
    {
        FakeManager manager;ManagedOfferController<FakeManager> controller(manager);
        assert(!controller.start(L"test.exe","bad",options,45,peer));
        assert(!controller.start(L"test.exe",image,options,0,peer));
        assert(!controller.start(L"test.exe",image,options,45,peer,0));
        assert(!controller.start(L"test.exe",image,options,45,peer,30001));
        assert(manager.calls.load()==0);
    }
}
