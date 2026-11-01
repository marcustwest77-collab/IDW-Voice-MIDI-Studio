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
#include "../RemoteWindowsCommandReviewOperation.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <thread>
using namespace idw::daw::remote;
int main(int argc,char** argv){
    LocalTLSOffer expected{45,std::string(64,'a'),std::string(64,'b'),13};
    WindowsCommandReviewOperation operation;
#if defined(_WIN32)
    if(argc==4 && (std::string(argv[1])=="--test-host" || std::string(argv[1])=="--test-bound-host")) {
        if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stdin),_O_BINARY)==-1) return 2;
        if(std::string(argv[1])=="--test-bound-host") {
            LocalTLSOfferBytes offer{};
            std::cin.read(reinterpret_cast<char*>(offer.data()),static_cast<std::streamsize>(offer.size()));
            if(!std::cin || !decodeTLSOffer(offer,expected)) return 2;
        }
        const auto pid=std::stoul(argv[2]);const std::string mode=argv[3];
        // TEST driver provides its PID; production start accepts a held trusted
        // process handle, never PID-based process discovery.
        const auto source=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,static_cast<DWORD>(pid));
        if(!source) return 2;
        const auto started=std::chrono::steady_clock::now();
        const auto deadline=started+std::chrono::milliseconds(mode=="expire"?250:2000);
        const bool scheduled=operation.start(source,expected,"guest",2000,deadline);
        CloseHandle(source); // Deliberately release borrowed handle before worker I/O.
        if(!scheduled) return 2;
        if(mode=="immediate") {
            operation.stop();
            assert(!operation.busy() && operation.endpoint().empty() && !operation.pending() && !operation.takeResult());
            assert(std::chrono::steady_clock::now()-started<std::chrono::seconds(1));
            std::cout << "STOPPED\n" << std::flush;return 0;
        }
        const auto endpoint=operation.endpoint();if(endpoint.empty()) return 2;
        std::string name;for(const auto c:endpoint) name.push_back(static_cast<char>(c));
        std::cout << "PIPE " << name << '\n' << std::flush;
        bool shown=false,stopped=false;std::optional<bool> result;
        while(!(result=operation.takeResult())) {
            if(mode=="cancel" && std::chrono::steady_clock::now()-started>=std::chrono::milliseconds(80)) {
                operation.stop();stopped=true;break;
            }
            if(const auto prompt=operation.pending()) {
                if(!shown){std::cout << "REVIEW\n" << std::flush;shown=true;}
                const auto liveEndpoint=operation.endpoint();
                assert(liveEndpoint.empty() || liveEndpoint==endpoint); // Cached; expiry may win this poll.
                if(mode=="manual") {
                    DWORD available=0;const auto input=GetStdHandle(STD_INPUT_HANDLE);
                    if(!PeekNamedPipe(input,nullptr,0,nullptr,&available,nullptr)){operation.stop();stopped=true;break;}
                    if(available) {
                        char command[6]{};DWORD count=0;
                        if(available!=6 || !ReadFile(input,command,6,&count,nullptr) || count!=6) return 2;
                        const std::string decision(command,command+6);
                        if(decision=="STOP!\n"){operation.stop();stopped=true;break;}
                        if(decision!="ALLOW\n" && decision!="DENY!\n") return 2;
                        if(!operation.resolve(*prompt,decision=="ALLOW\n")) return 2;
                    }
                } else if(mode=="allow" || mode=="deny" ||
                          (mode=="slow" && std::chrono::steady_clock::now()-started>=std::chrono::milliseconds(1000))) {
                    if(!operation.resolve(*prompt,mode!="deny")) return 2;
                }
            }
            if(std::chrono::steady_clock::now()>deadline+std::chrono::seconds(1)){operation.stop();stopped=true;break;}
            Sleep(1);
        }
        if(!stopped) {
            assert(operation.busy() && operation.endpoint().empty() && !operation.pending() && !operation.takeResult());
            const auto again=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,static_cast<DWORD>(pid));
            if(again){assert(!operation.start(again,expected,"guest"));CloseHandle(again);}
        }
        operation.stop();assert(!operation.busy() && operation.endpoint().empty() && !operation.takeResult());
        std::cout << (result && *result?"DONE\n":"FAILED\n") << std::flush;
        return result && *result?0:3;
    }
    if(argc!=1) return 2;
    assert(!operation.start(GetCurrentProcess(),expected,"guest"));
#else
    (void)argc;(void)argv;
#endif
    assert(!operation.start(nullptr,expected,"guest"));
    assert(!operation.busy() && operation.endpoint().empty() && !operation.pending() && !operation.takeResult());
    assert(!operation.resolve({},true));operation.stop();operation.stop();
}
