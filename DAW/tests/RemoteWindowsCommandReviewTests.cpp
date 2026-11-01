#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif
#include "../RemoteWindowsCommandReviewChannel.h"
#include "../RemoteLocalCommandConsent.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <thread>
using namespace idw::daw::remote;
int main(int argc,char** argv) {
    LocalTLSOffer expected{45,std::string(64,'a'),std::string(64,'b'),13};
    WindowsCommandReviewChannel channel;
#if defined(_WIN32)
    if(argc==4 && (std::string(argv[1])=="--test-host" || std::string(argv[1])=="--test-bound-host")) {
        if(_setmode(_fileno(stdout),_O_BINARY)==-1) return 2;
        if(std::string(argv[1])=="--test-bound-host") {
            if(_setmode(_fileno(stdin),_O_BINARY)==-1) return 2;
            LocalTLSOfferBytes offer{};
            std::cin.read(reinterpret_cast<char*>(offer.data()),static_cast<std::streamsize>(offer.size()));
            if(!std::cin || !decodeTLSOffer(offer,expected)) return 2;
        }
        // TEST ONLY: trusted driver supplies its own PID; production API uses
        // an already held independently trusted child process handle instead.
        const auto pid=std::stoul(argv[2]);const std::string mode=argv[3];
        const auto peer=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,static_cast<DWORD>(pid));
        if(!peer || !channel.create()) {if(peer) CloseHandle(peer);return 2;}
        const auto name=channel.name();std::string path;
        for(const auto c:name) path.push_back(static_cast<char>(c)); // Generated ASCII prefix/hex nonce only.
        std::cout << "PIPE " << path << '\n' << std::flush;
        std::thread canceler;
        if(mode=="cancel") canceler=std::thread([&]{Sleep(80);channel.cancel();});
        LocalCommandConsent commandConsent;std::atomic<bool> controllerFinished{false};std::thread controller;
        if(mode=="manual") controller=std::thread([&] {
            const auto input=GetStdHandle(STD_INPUT_HANDLE);
            while(!controllerFinished.load()) {
                const auto prompt=commandConsent.pending();
                if(prompt) {
                    DWORD available=0;
                    if(!PeekNamedPipe(input,nullptr,0,nullptr,&available,nullptr)){commandConsent.close();return;}
                    if(available) {
                        char command[6]{};DWORD read=0;
                        if(available!=6 || !ReadFile(input,command,6,&read,nullptr) || read!=6){commandConsent.close();return;}
                        const std::string decision(command,command+6);
                        if(decision!="ALLOW\n" && decision!="DENY!\n"){commandConsent.close();return;}
                        commandConsent.resolve(*prompt,decision=="ALLOW\n");return;
                    }
                }
                Sleep(1);
            }
        });
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(mode=="expire"?100:2000);
        const bool result=channel.reviewFrom(peer,expected,"guest",
            [&](const LocalCommandProposal& proposal,bool& allow,const std::atomic<bool>& canceled,WindowsCommandReviewChannel::Deadline until) {
                std::cout << "REVIEW\n" << std::flush;
                if(mode=="manual") {
                    return commandConsent.request(proposal,allow,canceled,until);
                }
                if(mode=="expire") std::this_thread::sleep_until(until+std::chrono::milliseconds(10));
                if(mode=="slow") Sleep(1000);
                if(mode=="throw") throw 1;
                if(canceled.load()) return false;
                allow=mode!="deny";return true;
            },2000,deadline);
        controllerFinished.store(true);commandConsent.close();
        if(controller.joinable()) controller.join();
        if(canceler.joinable()) canceler.join();
        const bool retired=channel.name().empty();channel.close();CloseHandle(peer);
        std::cout << (result?"DONE\n":"FAILED\n") << std::flush;
        return retired && result?0:3;
    }
    if(argc!=1) return 2;
    assert(channel.create());assert(!channel.create());
    bool called=false;
    assert(!channel.reviewFrom(GetCurrentProcess(),expected,"guest",
        [&](const auto&,bool&,const auto&,auto){called=true;return true;}));
    assert(!called && channel.name().empty());
    assert(channel.create());channel.cancel();
    assert(!channel.reviewFrom(nullptr,expected,"guest",{}));
    assert(channel.name().empty());channel.close();
#else
    (void)argc;(void)argv;
    assert(!channel.create());assert(channel.name().empty());
    assert(!channel.reviewFrom(nullptr,expected,"guest",{}));channel.close();
#endif
}
