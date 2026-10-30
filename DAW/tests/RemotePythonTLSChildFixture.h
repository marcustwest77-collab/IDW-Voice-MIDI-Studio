#pragma once
// Test-only Python child launcher. Never use as the production artifact policy.
// Only stdout readiness and NUL stdin are inherited; bootstrap stays on its
// process-verified named pipe. An owned job kills the child on host exit.
#if defined(_WIN32)
#include <shellapi.h>
#include <chrono>
#include <string>
#include <vector>
struct FixtureHandle {
    HANDLE value=nullptr;
    ~FixtureHandle(){if(value && value!=INVALID_HANDLE_VALUE) CloseHandle(value);}
};
inline std::wstring fixtureQuote(const std::wstring& text) {
    std::wstring result=L"\"";unsigned slashes=0;
    for(wchar_t c:text) {
        if(c==L'\\') {++slashes;continue;}
        if(c==L'"') result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');
        slashes=0;result.push_back(c);
    }
    result.append(slashes*2,L'\\');result.push_back(L'"');return result;
}
inline bool launchPythonTLSFixture(const std::wstring& pipe,HANDLE& process,FixtureHandle& job,std::string& ready) {
    int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
    struct FreeArgs { wchar_t** value;~FreeArgs(){if(value) LocalFree(value);} } freeArgs{args};
    if(!args || count!=5) return false;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    FixtureHandle read,write,nul;
    if(!CreatePipe(&read.value,&write.value,&security,0) ||
       !SetHandleInformation(read.value,HANDLE_FLAG_INHERIT,0)) return false;
    nul.value=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
    if(nul.value==INVALID_HANDLE_VALUE) return false;
    SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);
    std::vector<unsigned char> storage(size);
    auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if(!InitializeProcThreadAttributeList(attributes,1,0,&size)) return false;
    struct FreeAttributes { LPPROC_THREAD_ATTRIBUTE_LIST value;~FreeAttributes(){DeleteProcThreadAttributeList(value);} } cleanup{attributes};
    HANDLE inherited[]={write.value,nul.value};
    if(!UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)) return false;
    job.value=CreateJobObjectW(nullptr,nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job.value || !SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) return false;
    const std::wstring python(args[2]);
    auto command=fixtureQuote(python)+L" "+fixtureQuote(args[3])+L" --runtime-child "+fixtureQuote(pipe)+L" "+
        std::to_wstring(GetCurrentProcessId())+L" 45 "+fixtureQuote(args[4]);
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributes;
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;startup.StartupInfo.hStdInput=nul.value;
    startup.StartupInfo.hStdOutput=write.value;startup.StartupInfo.hStdError=write.value;
    PROCESS_INFORMATION child{};
    if(!CreateProcessW(python.c_str(),&command[0],nullptr,nullptr,TRUE,
        CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&child)) return false;
    process=child.hProcess;FixtureHandle thread{child.hThread};
    if(!AssignProcessToJobObject(job.value,process) || ResumeThread(thread.value)==static_cast<DWORD>(-1)) {
        TerminateProcess(process,1);WaitForSingleObject(process,2000);return false;
    }
    CloseHandle(write.value);write.value=nullptr;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(std::chrono::steady_clock::now()<deadline) {
        DWORD available=0;
        if(!PeekNamedPipe(read.value,nullptr,0,nullptr,&available,nullptr)) return false;
        if(available) {
            char c=0;DWORD received=0;
            if(!ReadFile(read.value,&c,1,&received,nullptr) || received!=1) return false;
            if(c=='\n') return ready.rfind("TLS_PORT ",0)==0;
            if(ready.size()>=127) return false;
            if(c!='\r') ready.push_back(c);
        } else if(WaitForSingleObject(process,10)!=WAIT_TIMEOUT) return false;
    }
    return false;
}
#endif
