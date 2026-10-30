// Process/session lifecycle integration using synthetic trusted TLS evidence.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif
#include "../RemoteWindowsBridgeSession.h"
#include "../RemoteWindowsBootstrapChannel.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <thread>
#include <cstdlib>
using namespace idw::daw::remote;
#if defined(_WIN32)
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value) CloseHandle(value); }
};
std::wstring selfPath() {
    wchar_t path[32768]{};
    const auto count = GetModuleFileNameW(nullptr, path, 32768);
    assert(count > 0 && count < 32768);
    return std::wstring(path, count);
}
bool sendCommand(const LocalSessionBootstrap& bootstrap, const std::string& request) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
    const auto socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bool result = false;
    if (socket != INVALID_SOCKET) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(bootstrap.port);
        std::vector<std::uint8_t> frame;
        if (connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
            encodeFrame(bootstrap.secret + "\n1|" + std::to_string(bootstrap.epoch) + "|1|" + request + "|arm|1", frame)) {
            std::size_t offset = 0;
            while (offset < frame.size()) {
                const auto n = send(socket, reinterpret_cast<const char*>(frame.data() + offset),
                                    static_cast<int>(frame.size() - offset), 0);
                if (n <= 0) break;
                offset += static_cast<std::size_t>(n);
            }
            shutdown(socket, SD_SEND);
            fd_set reads;
            FD_ZERO(&reads); FD_SET(socket, &reads);
            timeval timeout{}; timeout.tv_sec = 2;
            char receipt{};
            result = offset == frame.size() && select(0, &reads, nullptr, nullptr, &timeout) == 1 &&
                     recv(socket, &receipt, 1, 0) == 1 && receipt == '1';
        }
        closesocket(socket);
    }
    WSACleanup();
    return result;
}
std::wstring startedEvent(DWORD parent) { return L"Local\\IDW-BridgeTestStarted-" + std::to_wstring(parent); }
int childMain(int argc, char** argv) {
    if (argc == 2) { Sleep(15000); return 0; } // Owned descendant fixture.
    if (argc != 4 || std::string(argv[1]) != "--idw-native-bootstrap") return 2;
    const auto parentId = static_cast<DWORD>(std::strtoul(argv[3], nullptr, 10));
    const auto path = selfPath();
    if (path.find(L"-stall.exe") != std::wstring::npos) {
        Handle event{OpenEventW(EVENT_MODIFY_STATE, FALSE, startedEvent(parentId).c_str())};
        if (event.value) SetEvent(event.value);
        Sleep(15000); return 0;
    }
    Handle parent{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, parentId)};
    if (!parent.value) return 3;
    const std::string ascii(argv[2]);
    const bool result = receiveWindowsBootstrap(std::wstring(ascii.begin(), ascii.end()), parent.value,
        [&](const LocalSessionBootstrap& bootstrap) {
            std::string request = "managed_bridge";
            if (path.find(L"-tree.exe") != std::wstring::npos) {
                auto command = L"\"" + path + L"\" --descendant";
                STARTUPINFOW startup{}; startup.cb = sizeof(startup);
                PROCESS_INFORMATION child{};
                if (!CreateProcessW(path.c_str(), &command[0], nullptr, nullptr, FALSE,
                                    CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child)) return false;
                CloseHandle(child.hThread); CloseHandle(child.hProcess);
                request = "descendant_" + std::to_string(child.dwProcessId);
            }
            return bootstrap.peerSha256 == std::string(64, 'a') &&
                   bootstrap.connectionId == std::string(64, 'b') && sendCommand(bootstrap, request);
        }, 3000);
    if (!result) return 4;
    Sleep(path.find(L"-exit.exe") != std::wstring::npos ? 100 : 15000);
    return 0;
}
struct Copy {
    std::wstring path;
    explicit Copy(const std::wstring& suffix) : path(selfPath() + suffix + L".exe") {
        assert(CopyFileW(selfPath().c_str(), path.c_str(), FALSE));
    }
    ~Copy() { DeleteFileW(path.c_str()); }
};
void awaitStopped(WindowsBridgeSession& session) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (session.running() && std::chrono::steady_clock::now() < until) Sleep(10);
    assert(!session.running());
}
#endif
int main(int argc, char** argv) {
#if defined(_WIN32)
    _set_error_mode(_OUT_TO_STDERR);
    if (argc > 1) return childMain(argc, argv);
#else
    (void)argc; (void)argv;
#endif
    TrustedTransportEventQueue queue;
    WindowsBridgeSession session(queue);
    LocalSocketReceiver::Options options;
    options.sessionTimeoutMs = 15000;
    LocalTransportBinding transport{90, std::string(64, 'a'), std::string(64, 'b'), true};
    LocalHostConsent consent{90, transport.peerSha256, transport.connectionId, true, true};
#if defined(_WIN32)
    const auto executable = selfPath();
    std::string approved;
    assert(inspectWindowsBridgeSha256(executable, approved) && approved.size() == 64);
    // Known SHA-256 vector exercises the manifest diagnostic independently of launch.
    const auto directory = executable.substr(0, executable.find_last_of(L'\\'));
    wchar_t temporary[MAX_PATH]{};
    assert(GetTempFileNameW(directory.c_str(), L"idw", 0, temporary));
    { Handle file{CreateFileW(temporary, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr)};
      DWORD count = 0;
      assert(file.value != INVALID_HANDLE_VALUE && WriteFile(file.value, "abc", 3, &count, nullptr) && count == 3); }
    std::string digest;
    assert(inspectWindowsBridgeSha256(temporary, digest));
    assert(digest == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(DeleteFileW(temporary));
    assert(!session.start(L"relative.exe", approved, options, transport, consent));
    assert(!session.start(executable, std::string(64, '0'), options, transport, consent));
    auto denied = consent; denied.hostApproved = false;
    assert(!session.start(executable, approved, options, transport, denied));
    assert(queue.drain().empty() && !session.running() && session.processId() == 0);
    assert(session.start(executable, approved, options, transport, consent));
    Handle child{OpenProcess(SYNCHRONIZE, FALSE, session.processId())};
    assert(child.value && session.running());
    assert(!session.start(executable, approved, options, transport, consent));
    auto events = queue.drain();
    assert(events.size() == 2 && events.back().wire == "1|90|1|managed_bridge|arm|1");
    auto stale = transport; stale.connectionId = std::string(64, 'c');
    assert(!session.onTransportLost(stale) && session.running());
    assert(session.onTransportLost(transport));
    assert(!session.running() && session.processId() == 0 && WaitForSingleObject(child.value, 2000) == WAIT_OBJECT_0);
    events = queue.drain();
    assert(events.size() == 1 && events.front().kind == TrustedTransportEvent::Kind::authenticationLost);
    const auto previous = transport;
    ++transport.epoch; consent.epoch = transport.epoch;
    assert(session.start(executable, approved, options, transport, consent));
    assert(!session.onTransportLost(previous) && session.running());
    session.stop(); queue.drain();
    // A correctly pinned but invalid executable must roll back admission and
    // close the newly created job without leaving a child/listener behind.
    Copy invalidImage(L"-bad-image");
    { Handle file{CreateFileW(invalidImage.path.c_str(), GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING, 0, nullptr)};
      DWORD count = 0;
      assert(file.value != INVALID_HANDLE_VALUE && WriteFile(file.value, "abc", 3, &count, nullptr) && count == 3); }
    ++transport.epoch; consent.epoch = transport.epoch;
    assert(!session.start(invalidImage.path, digest, options, transport, consent));
    events = queue.drain();
    assert(events.size() == 1 && !session.running() && session.processId() == 0);
    Copy exiting(L"-exit"), tree(L"-tree"), stalled(L"-stall");
    ++transport.epoch; consent.epoch = transport.epoch;
    assert(session.start(exiting.path, approved, options, transport, consent));
    awaitStopped(session);
    session.stop();
    events = queue.drain();
    assert(events.size() == 1 && events.front().epoch == transport.epoch);
    // Killing the owner job also kills descendants, not only the direct bridge.
    ++transport.epoch; consent.epoch = transport.epoch;
    assert(session.start(tree.path, approved, options, transport, consent));
    events = queue.drain();
    assert(events.size() == 2);
    const auto begin = events.back().wire.find("descendant_");
    assert(begin != std::string::npos);
    const auto descendantId = static_cast<DWORD>(std::strtoul(events.back().wire.c_str() + begin + 11, nullptr, 10));
    Handle descendant{OpenProcess(SYNCHRONIZE, FALSE, descendantId)};
    assert(descendant.value && WaitForSingleObject(descendant.value, 0) == WAIT_TIMEOUT);
    session.stop();
    assert(WaitForSingleObject(descendant.value, 2000) == WAIT_OBJECT_0);
    queue.drain();
    ++transport.epoch; consent.epoch = transport.epoch;
    assert(!session.start(stalled.path, approved, options, transport, consent, 50));
    assert(session.processId() == 0 && !session.running());
    queue.drain();
    // Loss/stop during bootstrap cancels promptly, without waiting five seconds.
    Handle entered{CreateEventW(nullptr, TRUE, FALSE, startedEvent(GetCurrentProcessId()).c_str())};
    assert(entered.value);
    for (bool loss : {false, true}) {
        ResetEvent(entered.value);
        ++transport.epoch; consent.epoch = transport.epoch;
        bool result = true;
        std::thread starting([&] { result = session.start(stalled.path, approved, options, transport, consent, 5000); });
        assert(WaitForSingleObject(entered.value, 3000) == WAIT_OBJECT_0);
        const auto now = std::chrono::steady_clock::now();
        if (loss) assert(session.onTransportLost(transport)); else session.stop();
        starting.join();
        assert(std::chrono::steady_clock::now() - now < std::chrono::seconds(1));
        assert(!result && !session.running() && session.processId() == 0);
        queue.drain();
    }
    auto expiring = options;
    expiring.frameTimeoutMs = 200; expiring.sessionTimeoutMs = 1500;
    ++transport.epoch; consent.epoch = transport.epoch;
    assert(session.start(executable, approved, expiring, transport, consent));
    Handle expiringChild{OpenProcess(SYNCHRONIZE, FALSE, session.processId())};
    awaitStopped(session);
    session.stop();
    assert(WaitForSingleObject(expiringChild.value, 2000) == WAIT_OBJECT_0);
    queue.drain();
    ++transport.epoch; consent.epoch = transport.epoch;
    Handle scopedChild;
    { WindowsBridgeSession scoped(queue);
      assert(scoped.start(executable, approved, options, transport, consent));
      scopedChild.value = OpenProcess(SYNCHRONIZE, FALSE, scoped.processId()); }
    assert(scopedChild.value && WaitForSingleObject(scopedChild.value, 2000) == WAIT_OBJECT_0);
    session.stop();
#else
    std::string output = "unchanged";
    assert(!inspectWindowsBridgeSha256(L"anything", output) && output == "unchanged");
    assert(!session.start(L"anything", std::string(64, 'a'), options, transport, consent));
    assert(!session.running() && session.processId() == 0 && queue.drain().empty());
    session.stop();
#endif
}
