#pragma once
// Include after RemoteNativeLocalReceiver.h so winsock2 precedes Windows headers.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

namespace idw::daw::remote {
// Host-thread lifecycle; asynchronous pipe I/O. No socket or audio/UI callbacks.
class WindowsWorkerProcess {
public:
    enum class State { idle, starting, ready, succeeded, failed };
    ~WindowsWorkerProcess() { stop(); }
    WindowsWorkerProcess()=default;
    WindowsWorkerProcess(const WindowsWorkerProcess&)=delete;
    WindowsWorkerProcess& operator=(const WindowsWorkerProcess&)=delete;

    static std::string freshSecret() {
#ifdef _WIN32
        unsigned char bytes[32]{};
        if (BCryptGenRandom(nullptr,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)
            return {};
        const char* hex="0123456789abcdef";
        std::string result;
        result.reserve(64);
        for (auto byte : bytes) { result+=hex[byte>>4]; result+=hex[byte&15]; }
        SecureZeroMemory(bytes,sizeof(bytes));
        return result;
#else
        return {}; // Windows process owner only; never substitute a weak RNG.
#endif
    }
    bool start(const std::wstring& python, const std::wstring& script, std::string json) {
        if (thread_.joinable() || json.empty() || json.size()>16384) return false;
#ifdef _WIN32
        if (!validPath(python) || !validPath(script)) return false;
        stopping_.store(false);
        state_.store(State::starting);
        started_=std::chrono::steady_clock::now();
        try {
            thread_=std::thread([this,python,script,json=std::move(json)]() mutable {
                bool success=false;
                try { success=run(python,script,json); } catch (...) {}
                if (!json.empty()) SecureZeroMemory(json.data(),json.size());
                if (!stopping_.load()) state_.store(success ? State::succeeded : State::failed);
            });
        } catch (...) { state_.store(State::failed); return false; }
        return true;
#else
        (void)python; (void)script;
        return false;
#endif
    }
    State poll() {
        // Owner calls regularly even while playback is stopped. If the child
        // refuses stdin, terminating its job releases the blocked pipe write.
        if (state_.load()==State::starting &&
            std::chrono::steady_clock::now()-started_>std::chrono::seconds(5)) {
            stop();
            state_.store(State::failed);
        }
        return state_.load();
    }
    void stop() {
        stopping_.store(true);
#ifdef _WIN32
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (job_!=nullptr) TerminateJobObject(job_,1);
        }
#endif
        if (thread_.joinable()) thread_.join();
        state_.store(State::idle);
    }
private:
#ifdef _WIN32
    struct Handle {
        HANDLE value=nullptr;
        ~Handle() { reset(); }
        void reset() {
            if (value && value!=INVALID_HANDLE_VALUE) CloseHandle(value);
            value=nullptr;
        }
        bool valid() const { return value && value!=INVALID_HANDLE_VALUE; }
    };
    struct Attributes {
        std::vector<unsigned char> bytes;
        LPPROC_THREAD_ATTRIBUTE_LIST list=nullptr;
        ~Attributes() { if (list) DeleteProcThreadAttributeList(list); }
        bool init() {
            SIZE_T size=0;
            InitializeProcThreadAttributeList(nullptr,1,0,&size);
            if (size==0) return false;
            bytes.resize(size);
            auto* candidate=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(bytes.data());
            if (!InitializeProcThreadAttributeList(candidate,1,0,&size)) return false;
            list=candidate;
            return true;
        }
    };
    static bool validPath(const std::wstring& path) {
        if (path.empty() || path.find_first_of(L"\"\r\n")!=std::wstring::npos ||
            path.find(L'\0')!=std::wstring::npos) return false;
        std::error_code error;
        return std::filesystem::path(path).is_absolute() &&
               std::filesystem::is_regular_file(std::filesystem::path(path),error) && !error;
    }
    bool run(const std::wstring& python,const std::wstring& script,std::string& json) {
        Handle inRead,inWrite,outRead,outWrite,errorSink,job,process,mainThread;
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
        if (!CreatePipe(&inRead.value,&inWrite.value,&security,32768) ||
            !CreatePipe(&outRead.value,&outWrite.value,&security,4096) ||
            !SetHandleInformation(inWrite.value,HANDLE_FLAG_INHERIT,0) ||
            !SetHandleInformation(outRead.value,HANDLE_FLAG_INHERIT,0)) return false;
        errorSink.value=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                                    &security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (!errorSink.valid()) return false;
        Attributes attributes;
        if (!attributes.init()) return false;
        HANDLE inherited[]{inRead.value,outWrite.value,errorSink.value};
        if (!UpdateProcThreadAttribute(attributes.list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                        inherited,sizeof(inherited),nullptr,nullptr)) return false;
        job.value=CreateJobObjectW(nullptr,nullptr);
        if (!job.valid()) return false;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,
                                      &limits,sizeof(limits))) return false;
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb=sizeof(startup);
        startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput=inRead.value;
        startup.StartupInfo.hStdOutput=outWrite.value;
        startup.StartupInfo.hStdError=errorSink.value;
        startup.lpAttributeList=attributes.list;
        PROCESS_INFORMATION information{};
        // Paths cannot contain quotes; application is explicit, with no shell.
        std::wstring command=L"\""+python+L"\" -E -s \""+script+L"\"";
        const auto directory=std::filesystem::path(script).parent_path().wstring();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_.load()) return false;
            if (!CreateProcessW(python.c_str(),command.data(),nullptr,nullptr,TRUE,
                    EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW|CREATE_SUSPENDED,
                    nullptr,directory.c_str(),&startup.StartupInfo,&information)) return false;
            process.value=information.hProcess;
            mainThread.value=information.hThread;
            if (!AssignProcessToJobObject(job.value,process.value)) {
                TerminateProcess(process.value,1);
                return false;
            }
            job_=job.value;
        }
        // Clear the shared job pointer before its RAII handle can close.
        struct Unpublish {
            WindowsWorkerProcess& owner;
            ~Unpublish() { std::lock_guard<std::mutex> lock(owner.mutex_);owner.job_=nullptr; }
        } unpublish{*this};
        if (ResumeThread(mainThread.value)==static_cast<DWORD>(-1)) return false;
        inRead.reset(); outWrite.reset(); errorSink.reset(); mainThread.reset();
        const auto size=static_cast<std::uint32_t>(json.size());
        char header[4]{static_cast<char>(size>>24),static_cast<char>(size>>16),
                       static_cast<char>(size>>8),static_cast<char>(size)};
        auto writeAll=[&](const char* data,DWORD count) {
            DWORD position=0;
            while (position<count && !stopping_.load()) {
                DWORD written=0;
                if (!WriteFile(inWrite.value,data+position,count-position,&written,nullptr) ||
                    written==0) return false;
                position+=written;
            }
            return position==count && !stopping_.load();
        };
        if (!writeAll(header,4) || !writeAll(json.data(),size)) return false;
        SecureZeroMemory(json.data(),json.size());
        json.clear();
        inWrite.reset(); // EOF completes the one-frame bootstrap.
        std::string response;
        while (!stopping_.load() && response.size()<6) {
            DWORD available=0;
            if (!PeekNamedPipe(outRead.value,nullptr,0,nullptr,&available,nullptr)) return false;
            if (available) {
                char byte=0; DWORD received=0;
                if (!ReadFile(outRead.value,&byte,1,&received,nullptr) || received!=1) return false;
                response+=byte;
                if (response!=std::string("READY\n").substr(0,response.size())) return false;
            } else if (WaitForSingleObject(process.value,10)==WAIT_OBJECT_0) {
                // The child may write READY and exit between our peek and wait.
                if (!PeekNamedPipe(outRead.value,nullptr,0,nullptr,&available,nullptr) ||
                    available==0) return false;
            }
        }
        if (stopping_.load() || response!="READY\n") return false;
        state_.store(State::ready);
        while (!stopping_.load()) {
            const auto result=WaitForSingleObject(process.value,50);
            if (result==WAIT_OBJECT_0) {
                DWORD code=1;
                return GetExitCodeProcess(process.value,&code) && code==0;
            }
            if (result==WAIT_FAILED) return false;
        }
        return false;
    }
    std::mutex mutex_;
    HANDLE job_=nullptr;
#endif
    std::thread thread_;
    std::atomic<bool> stopping_{true};
    std::atomic<State> state_{State::idle};
    std::chrono::steady_clock::time_point started_{};
};
}
