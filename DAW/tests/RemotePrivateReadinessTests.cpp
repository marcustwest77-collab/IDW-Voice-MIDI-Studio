// Real private-pipe processes; readiness and TLS evidence remain TEST metadata.
#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "../RemoteWindowsBootstrapChannel.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <thread>
using namespace idw::daw::remote;
#if defined(_WIN32)
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE) CloseHandle(value);}
};
struct Child {
    PROCESS_INFORMATION process{};
    Child(const std::wstring& pipe,const std::wstring& mode) {
        wchar_t self[32768]{};const auto size=GetModuleFileNameW(nullptr,self,32768);
        assert(size>0 && size<32768);
        auto command=std::wstring(L"\"")+self+L"\" --ready-child "+pipe+L" "+
            std::to_wstring(GetCurrentProcessId())+L" "+mode;
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
        assert(CreateProcessW(self,&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process));
    }
    ~Child(){
        if(WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT) TerminateProcess(process.hProcess,99);
        WaitForSingleObject(process.hProcess,2000);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }
    void expectSuccess(){
        assert(WaitForSingleObject(process.hProcess,3000)==WAIT_OBJECT_0);
        DWORD result=1;assert(GetExitCodeProcess(process.hProcess,&result) && result==0);
    }
};
int childMain(int argc,char** argv) {
    if(argc!=5) return 2;
    const std::string ascii(argv[2]),mode(argv[4]);const std::wstring name(ascii.begin(),ascii.end());
    if(mode=="stall") {Sleep(1500);return 0;}
    Handle parent{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,
                             static_cast<DWORD>(std::strtoul(argv[3],nullptr,10)))};
    if(!parent.value) return 3;
    if(mode=="ready-only") {
        Handle pipe;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(std::chrono::steady_clock::now()<until) {
            pipe.value=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
                                  SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr);
            if(pipe.value!=INVALID_HANDLE_VALUE) break;
            if(GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PIPE_BUSY) return 4;
            Sleep(10);
        }
        ULONG server=0;
        if(pipe.value==INVALID_HANDLE_VALUE || !GetNamedPipeServerProcessId(pipe.value,&server) ||
           server!=GetProcessId(parent.value) || WaitForSingleObject(parent.value,0)!=WAIT_TIMEOUT) return 4;
        LocalBridgeReadyBytes bytes{};if(!encodeBridgeReady({45,12345},bytes)) return 4;
        DWORD count=0;
        if(!WriteFile(pipe.value,bytes.data(),static_cast<DWORD>(bytes.size()),&count,nullptr) || count!=bytes.size()) return 4;
        // Receiving even one credential byte before a TLS offer is a failure.
        unsigned char credential[207]{};count=0;
        const auto read=ReadFile(pipe.value,credential,sizeof(credential),&count,nullptr);
        return !read && count==0 ? 0 : 5;
    }
    const LocalTLSOffer offer{45,std::string(64,'a'),std::string(64,'b'),13};
    const LocalBridgeReady ready{45,12345};bool consumed=false;
    const bool result=receiveWindowsBootstrap(name,mode=="wrong-server"?GetCurrentProcess():parent.value,
        [&](const LocalSessionBootstrap& value){consumed=true;return value.epoch==45 && value.port==54321;},
        2000,&offer,&ready);
    if(mode=="wrong-server" || mode=="expect-refused") return !result && !consumed ? 0 : 6;
    if(result) Sleep(50);
    return result && consumed ? 0 : 6;
}
#endif
int main(int argc,char** argv) {
#if defined(_WIN32)
    _set_error_mode(_OUT_TO_STDERR);
    if(argc>1) return childMain(argc,argv);
    const LocalSessionBootstrap bootstrap{54321,45,std::string(64,'1'),std::string(64,'a'),std::string(64,'b')};
    {
        WindowsBootstrapChannel channel;assert(channel.create());Child child(channel.name(),L"complete");
        LocalBridgeReady ready;std::chrono::steady_clock::time_point first{},second{};
        assert(channel.receiveReadyFrom(child.process.hProcess,ready,2000,&first));
        assert(ready.epoch==45 && ready.tlsPort==12345);
        LocalTLSOffer offer;
        assert(channel.receiveOfferFrom(child.process.hProcess,offer,30000,&second));
        assert(second==first); // A new phase cannot renew the readiness deadline.
        assert(channel.deliverTo(child.process.hProcess,bootstrap));child.expectSuccess();
    }
    {
        WindowsBootstrapChannel channel;assert(channel.create());Child child(channel.name(),L"ready-only");
        LocalBridgeReady ready;assert(channel.receiveReadyFrom(child.process.hProcess,ready));
        assert(!channel.deliverTo(child.process.hProcess,bootstrap));child.expectSuccess();
    }
    {
        WindowsBootstrapChannel channel;assert(channel.create());Child child(channel.name(),L"ready-only");
        LocalBridgeReady ready;assert(channel.receiveReadyFrom(child.process.hProcess,ready));
        bool accepted=true;LocalTLSOffer offer;
        std::thread waiting([&]{accepted=channel.receiveOfferFrom(child.process.hProcess,offer);});
        Sleep(50);const auto before=std::chrono::steady_clock::now();channel.cancel();waiting.join();
        assert(!accepted && std::chrono::steady_clock::now()-before<std::chrono::seconds(1));child.expectSuccess();
    }
    {
        WindowsBootstrapChannel channel;assert(channel.create());Child child(channel.name(),L"wrong-server");
        LocalBridgeReady ready{99,1};assert(!channel.receiveReadyFrom(child.process.hProcess,ready,200));
        assert(ready.epoch==99 && ready.tlsPort==1);child.expectSuccess();
    }
    {
        WindowsBootstrapChannel channel;assert(channel.create());Child expected(channel.name(),L"stall");
        Child wrong(channel.name(),L"expect-refused");LocalBridgeReady ready{99,1};
        assert(!channel.receiveReadyFrom(expected.process.hProcess,ready,200));
        assert(ready.epoch==99 && ready.tlsPort==1);wrong.expectSuccess();
    }
#else
    (void)argc;(void)argv;WindowsBootstrapChannel channel;LocalBridgeReady ready{99,1};
    assert(!channel.create() && !channel.receiveReadyFrom(nullptr,ready));assert(ready.epoch==99 && ready.tlsPort==1);
#endif
}
