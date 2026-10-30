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
#include <bcrypt.h>
#endif
#include "RemoteWindowsBridgeSession.h"
#include "RemoteWindowsBootstrapChannel.h"
#include "RemoteLocalBootstrapWire.h"
#include <atomic>
#include <array>
#include <thread>
#include <vector>
#include <chrono>

namespace idw::daw::remote {
#if defined(_WIN32)
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { reset(); }
    void reset(HANDLE next = INVALID_HANDLE_VALUE) {
        if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
        value = next;
    }
};
bool alive(HANDLE process) {
    return process && process != INVALID_HANDLE_VALUE && WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
}
bool absoluteDiskPath(const std::wstring& path) {
    if (path.size() < 4 || path.size() > 32000 || path.find(L'\0') != std::wstring::npos ||
        path.find_first_of(L"\"\r\n\t") != std::wstring::npos) return false;
    const wchar_t drive = path[0];
    return ((drive >= L'A' && drive <= L'Z') || (drive >= L'a' && drive <= L'z')) &&
        path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
}
bool openPinnedFile(const std::wstring& path, Handle& file, std::wstring& canonical) {
    if (!absoluteDiskPath(path)) return false;
    const wchar_t root[] = {path[0], L':', L'\\', L'\0'};
    if (GetDriveTypeW(root) != DRIVE_FIXED) return false;
    // Deny writes/deletion while hashing and creating the suspended process.
    file.reset(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    if (file.value == INVALID_HANDLE_VALUE || GetFileType(file.value) != FILE_TYPE_DISK ||
        !GetFileInformationByHandle(file.value, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) return false;
    std::vector<wchar_t> name(32768);
    const auto count = GetFinalPathNameByHandleW(file.value, name.data(), static_cast<DWORD>(name.size()),
                                                FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (count == 0 || count >= name.size()) return false;
    canonical.assign(name.data(), count);
    if (canonical.compare(0, 4, L"\\\\?\\") != 0) return false;
    canonical.erase(0, 4);
    return absoluteDiskPath(canonical);
}
bool hashFile(HANDLE file, std::string& output) {
    struct HashState {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        std::vector<unsigned char> storage;
        ~HashState() {
            if (hash) BCryptDestroyHash(hash);
            if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    } state;
    if (BCryptOpenAlgorithmProvider(&state.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    DWORD objectSize = 0, returned = 0;
    if (BCryptGetProperty(state.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                          sizeof(objectSize), &returned, 0) < 0 || objectSize == 0) return false;
    // Keep object storage alive until after BCryptDestroyHash.
    state.storage.resize(objectSize);
    if (BCryptCreateHash(state.algorithm, &state.hash, state.storage.data(), objectSize, nullptr, 0, 0) < 0) return false;
    bool success = true;
    std::array<unsigned char, 16384> buffer{};
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) { success = false; break; }
        if (count == 0) break;
        if (BCryptHashData(state.hash, buffer.data(), count, 0) < 0) { success = false; break; }
    }
    std::array<unsigned char, 32> digest{};
    success = success && BCryptFinishHash(state.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    BCryptDestroyHash(state.hash);
    state.hash = nullptr;
    if (!success) return false;
    static constexpr char hex[] = "0123456789abcdef";
    std::string encoded(64, '0');
    for (std::size_t i = 0; i < digest.size(); ++i) {
        encoded[i * 2] = hex[digest[i] >> 4];
        encoded[i * 2 + 1] = hex[digest[i] & 15];
    }
    output.swap(encoded);
    return true;
}
}
#endif
bool inspectWindowsBridgeSha256(const std::wstring& path, std::string& output) {
#if defined(_WIN32)
    Handle file;
    std::wstring canonical;
    return openPinnedFile(path, file, canonical) && hashFile(file.value, output);
#else
    (void)path; (void)output;
    return false;
#endif
}

struct WindowsBridgeSession::Impl {
    struct Operation {
        explicit Operation(const LocalTransportBinding& value)
            : binding(std::make_shared<const LocalTransportBinding>(value)) {}
        std::shared_ptr<const LocalTransportBinding> binding;
        bool matches(const LocalTransportBinding& value) const {
            const auto current=std::atomic_load(&binding);
            return current && current->epoch!=0 && value.epoch==current->epoch &&
                value.peerSha256==current->peerSha256 && value.connectionId==current->connectionId;
        }
        WindowsBootstrapChannel channel;
        std::atomic<bool> canceled{false};
    };
    explicit Impl(TrustedTransportEventQueue& queue) : owner(queue) {}
    mutable std::mutex mutex;
    LocalSessionOwner owner;
    std::shared_ptr<Operation> operation; // Accessed through C++17 atomic shared_ptr functions.
    std::atomic<bool> stopping{false};
    std::atomic<std::uint64_t> stopSequence{0};
    std::thread monitor;
    LocalTransportBinding binding;
    std::uint64_t highestOfferEpoch=0;
#if defined(_WIN32)
    Handle process, job;
    std::uint32_t pid = 0;
    bool launch(const std::wstring& canonical,const std::shared_ptr<Operation>& op,
            std::uint64_t expectedStop,const wchar_t* mode,std::uint64_t offerEpoch) {
        const auto directory=canonical.substr(0,canonical.find_last_of(L'\\'));
        if (stopping.load() || op->canceled.load() || stopSequence.load() != expectedStop) return false;
        job.reset(CreateJobObjectW(nullptr, nullptr));
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation,
                                                    &limits, sizeof(limits))) return false;
        auto command = std::wstring(L"\"") + canonical + L"\" " + std::wstring(mode) + L" " +
            op->channel.name() + L" " + std::to_wstring(GetCurrentProcessId());
        if(offerEpoch!=0) command+=L" "+std::to_wstring(offerEpoch);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION child{};
        // Malformed/missing images must return an error, never open an OS
        // error dialog that can stall this background helper launch.
        DWORD previousMode = 0;
        if (!SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX,
                                &previousMode)) return false;
        const auto launched = CreateProcessW(canonical.c_str(), &command[0], nullptr, nullptr, FALSE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &child);
        SetThreadErrorMode(previousMode, nullptr);
        if (!launched) return false;
        process.reset(child.hProcess);
        Handle thread{child.hThread};
        pid = child.dwProcessId;
        // Never execute child code until its entire process tree is owned.
        if (!AssignProcessToJobObject(job.value, process.value) || stopping.load() || op->canceled.load() ||
            stopSequence.load() != expectedStop ||
            ResumeThread(thread.value) == static_cast<DWORD>(-1)) return false;
        return true;
    }
    void watch(const LocalTransportBinding& current,const std::shared_ptr<Operation>& op) {
        monitor=std::thread([this,current,op] {
            while(!stopping.load() && !op->canceled.load()) {
                const auto status=WaitForSingleObject(process.value,10);
                if(status!=WAIT_TIMEOUT || !owner.running()) {
                    owner.onTransportLost(current);op->canceled.store(true);
                    TerminateJobObject(job.value,0);break;
                }
            }
        });
    }
#endif
    void cleanup() {
        stopping.store(true);
        const auto op = std::atomic_load(&operation);
        if (op) { op->canceled.store(true); op->channel.cancel(); }
        owner.stop();
        if (monitor.joinable()) monitor.join();
#if defined(_WIN32)
        job.reset(); // Kill-on-close also stops descendants of the owned bridge.
        if (alive(process.value)) TerminateProcess(process.value, 0); // Covers failed job assignment.
        if (process.value != INVALID_HANDLE_VALUE) WaitForSingleObject(process.value, 2000);
        process.reset();
        pid = 0;
#endif
        if (op) op->channel.close();
        std::atomic_store(&operation, std::shared_ptr<Operation>{});
        binding = {};
    }
};
WindowsBridgeSession::WindowsBridgeSession(TrustedTransportEventQueue& queue)
    : impl_(std::make_unique<Impl>(queue)) {}
WindowsBridgeSession::~WindowsBridgeSession() { stop(); }
bool WindowsBridgeSession::start(const std::wstring& executable, const std::string& approvedSha256,
        const LocalSocketReceiver::Options& options, const LocalTransportBinding& transport,
        const LocalHostConsent& consent, unsigned timeoutMs) {
    auto& p = *impl_;
    const auto stopSequence = p.stopSequence.load();
    std::lock_guard<std::mutex> lock(p.mutex);
#if defined(_WIN32)
    if (p.monitor.joinable() || p.process.value != INVALID_HANDLE_VALUE || !validBootstrapHex(approvedSha256) ||
        timeoutMs < 20 || timeoutMs > 5000) return false;
    p.binding = transport;
    const auto op = std::make_shared<Impl::Operation>(transport);
    p.stopping.store(false);
    std::atomic_store(&p.operation, op); // Loss can cancel even while the image is hashed.
    Handle pinnedFile;
    std::wstring canonical;
    std::string observed;
    if (!openPinnedFile(executable, pinnedFile, canonical) || !hashFile(pinnedFile.value, observed) ||
        observed != approvedSha256 || canonical.size() < 4 ||
        _wcsicmp(canonical.c_str() + canonical.size() - 4, L".exe") != 0) { p.cleanup(); return false; }
    if (op->canceled.load() || p.stopSequence.load() != stopSequence || !op->channel.create()) {
        p.cleanup(); return false;
    }
    bool admitted = false;
    try {
        admitted = p.owner.start(options, transport, consent, [&](const LocalSessionBootstrap& bootstrap) {
            if(!p.launch(canonical,op,stopSequence,L"--idw-native-bootstrap",0)) return false;
            return op->channel.deliverTo(p.process.value, bootstrap, timeoutMs);
        });
        if (!admitted || p.stopping.load() || op->canceled.load() || p.stopSequence.load() != stopSequence ||
            !alive(p.process.value)) { p.cleanup(); return false; }
        p.watch(p.binding,op);
    } catch (...) { p.cleanup(); return false; }
    return true;
#else
    (void)stopSequence;
    (void)executable; (void)approvedSha256; (void)options; (void)transport; (void)consent; (void)timeoutMs;
    return false;
#endif
}
bool WindowsBridgeSession::startWithOffer(const std::wstring& executable,const std::string& approvedSha256,
        const LocalSocketReceiver::Options& options,std::uint64_t expectedEpoch,
        const std::string& approvedPeerSha256,const OfferConsent& consentFor,unsigned timeoutMs) {
    auto& p=*impl_;const auto stopSequence=p.stopSequence.load();
    std::lock_guard<std::mutex> lock(p.mutex);
#if defined(_WIN32)
    if(p.monitor.joinable() || p.process.value!=INVALID_HANDLE_VALUE || !consentFor ||
        expectedEpoch==0 || expectedEpoch<=p.highestOfferEpoch || !validBootstrapHex(approvedSha256) ||
        !validBootstrapHex(approvedPeerSha256) || timeoutMs<20 || timeoutMs>5000) return false;
    p.highestOfferEpoch=expectedEpoch; // Failed offers cannot reuse an epoch.
    try {
        const auto op=std::make_shared<Impl::Operation>(LocalTransportBinding{});
        p.stopping.store(false);std::atomic_store(&p.operation,op);
        Handle pinnedFile;std::wstring canonical;std::string observed;
        if(!openPinnedFile(executable,pinnedFile,canonical) || !hashFile(pinnedFile.value,observed) ||
            observed!=approvedSha256 || canonical.size()<4 ||
            _wcsicmp(canonical.c_str()+canonical.size()-4,L".exe")!=0 ||
            op->canceled.load() || p.stopSequence.load()!=stopSequence || !op->channel.create() ||
            !p.launch(canonical,op,stopSequence,L"--idw-native-offer",expectedEpoch)) {
            p.cleanup();return false;
        }
        LocalTLSOffer offer;
        const auto offerDeadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeoutMs);
        if(!op->channel.receiveOfferFrom(p.process.value,offer,timeoutMs) ||
            offer.epoch!=expectedEpoch || offer.peerSha256!=approvedPeerSha256) {
            p.cleanup();return false;
        }
        // Only the pinned, process-verified bridge reports this TLS evidence.
        // Consent must come from the trusted local host, never from the offer.
        p.binding={offer.epoch,offer.peerSha256,offer.connectionId,true};
        std::atomic_store(&op->binding,std::make_shared<const LocalTransportBinding>(p.binding));
        LocalHostConsent consent;
        if(op->canceled.load() || p.stopSequence.load()!=stopSequence ||
            !consentFor(offer,consent,op->canceled) || op->canceled.load() ||
            p.stopSequence.load()!=stopSequence || std::chrono::steady_clock::now()>=offerDeadline ||
            !alive(p.process.value) ||
            !p.owner.start(options,p.binding,consent,[&](const LocalSessionBootstrap& bootstrap) {
                if(op->canceled.load() || p.stopSequence.load()!=stopSequence) return false;
                return op->channel.deliverTo(p.process.value,bootstrap,timeoutMs);
            }) || op->canceled.load() || p.stopSequence.load()!=stopSequence || !alive(p.process.value)) {
            p.cleanup();return false;
        }
        p.watch(p.binding,op);
        return true;
    } catch(...) {p.cleanup();return false;}
#else
    (void)stopSequence;(void)executable;(void)approvedSha256;(void)options;(void)expectedEpoch;
    (void)approvedPeerSha256;(void)consentFor;(void)timeoutMs;return false;
#endif
}
void WindowsBridgeSession::stop() {
    auto& p = *impl_;
    p.stopSequence.fetch_add(1);
    p.stopping.store(true);
    const auto op = std::atomic_load(&p.operation);
    if (op) { op->canceled.store(true); op->channel.cancel(); }
    std::lock_guard<std::mutex> lock(p.mutex);
    p.cleanup();
}
bool WindowsBridgeSession::onTransportLost(const LocalTransportBinding& transport) {
    auto& p = *impl_;
    auto op = std::atomic_load(&p.operation);
    if (!op) {
        // Do not lose a callback racing the initial publication of a startup.
        std::lock_guard<std::mutex> lock(p.mutex);
        op = std::atomic_load(&p.operation);
        if (!op || !op->matches(transport)) return false;
        p.cleanup();
        return true;
    }
    if (!op || !op->matches(transport)) return false;
    // Revoke without waiting for the process/pipe startup mutex. The snapshot's
    // channel is generation-specific, so a late callback cannot cancel a new one.
    op->canceled.store(true);
    op->channel.cancel();
    p.owner.onTransportLost(transport);
    std::lock_guard<std::mutex> lock(p.mutex);
    if (std::atomic_load(&p.operation) == op) p.cleanup();
    return true;
}
bool WindowsBridgeSession::running() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
#if defined(_WIN32)
    const auto op = std::atomic_load(&impl_->operation);
    return op && !op->canceled.load() && !impl_->stopping.load() && alive(impl_->process.value) && impl_->owner.running();
#else
    return false;
#endif
}
std::uint32_t WindowsBridgeSession::processId() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
#if defined(_WIN32)
    return impl_->pid;
#else
    return 0;
#endif
}
}
