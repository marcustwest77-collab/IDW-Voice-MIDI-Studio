// Real Windows child-process bootstrap test; TLS evidence remains synthetic.
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
#include "../RemoteWindowsBootstrapChannel.h"
#include "../RemoteLocalBootstrapWire.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <thread>
#include <limits>
#include <cstdlib>
using namespace idw::daw::remote;

#if defined(_WIN32)
bool submit(const LocalSessionBootstrap& bootstrap) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
    const auto socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bool accepted = false;
    if (socket != INVALID_SOCKET) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(bootstrap.port);
        std::vector<std::uint8_t> frame;
        if (connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
            encodeFrame(bootstrap.secret + "\n1|" + std::to_string(bootstrap.epoch) +
                        "|1|private_child|arm|1", frame)) {
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
            char byte{};
            accepted = offset == frame.size() && select(0, &reads, nullptr, nullptr, &timeout) == 1 &&
                recv(socket, &byte, 1, 0) == 1 && byte == '1';
        }
        closesocket(socket);
    }
    WSACleanup();
    return accepted;
}
struct Child {
    PROCESS_INFORMATION process{};
    Child(const std::wstring& pipe, const std::wstring& mode) {
        wchar_t executable[32768]{};
        const auto length = GetModuleFileNameW(nullptr, executable, 32768);
        assert(length != 0 && length < 32768);
        auto command = std::wstring(L"\"") + executable + L"\" --bootstrap-child " + pipe +
            L" " + std::to_wstring(GetCurrentProcessId()) + L" " + mode;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        // No handles are inherited, and no session credential appears in argv.
        assert(CreateProcessW(executable, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                              nullptr, nullptr, &startup, &process));
    }
    ~Child() {
        if (WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT)
            TerminateProcess(process.hProcess, 99); // Own test child only.
        WaitForSingleObject(process.hProcess, 2000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    void expectSuccess() {
        assert(WaitForSingleObject(process.hProcess, 3000) == WAIT_OBJECT_0);
        DWORD code = 1;
        assert(GetExitCodeProcess(process.hProcess, &code) && code == 0);
    }
};
int childMain(int argc, char** argv) {
    if (argc != 5) return 2;
    const std::string ascii(argv[2]);
    const std::wstring pipe(ascii.begin(), ascii.end());
    const std::string mode(argv[4]);
    if (mode == "exit") return 0;
    if (mode == "stall") { Sleep(1500); return 0; }
    if (mode == "delayed") Sleep(150);
    const auto parentId = static_cast<DWORD>(std::strtoul(argv[3], nullptr, 10));
    const auto parent = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, parentId);
    if (!parent) return 3;
    bool consumed = false;
    const bool result = receiveWindowsBootstrap(pipe,
        mode == "wrong-server" ? GetCurrentProcess() : parent,
        [&](const LocalSessionBootstrap& bootstrap) {
            consumed = true;
            if (mode == "reject") return false;
            if (mode == "throw") throw 1;
            if (mode == "no-receipt") { Sleep(1500); return false; }
            if (mode == "expect-refused") return false;
            return bootstrap.peerSha256 == std::string(64, 'a') &&
                   bootstrap.connectionId == std::string(64, 'b') && submit(bootstrap);
        }, 2000);
    CloseHandle(parent);
    if (mode == "expect-refused" || mode == "wrong-server") return !result && !consumed ? 0 : 4;
    if (mode == "reject" || mode == "throw") return !result && consumed ? 0 : 5;
    // Stay alive until the parent has received the bounded bootstrap receipt.
    if (result) Sleep(100);
    return result && consumed ? 0 : 6;
}
#endif

int main(int argc, char** argv) {
#if defined(_WIN32)
    if (argc > 1) return childMain(argc, argv);
#else
    (void)argc; (void)argv;
#endif
    LocalSessionBootstrap example{54321, std::numeric_limits<std::uint64_t>::max(),
        std::string(64, '1'), std::string(64, 'a'), std::string(64, 'b')};
    LocalBootstrapBytes bytes{};
    assert(encodeLocalBootstrap(example, bytes));
    LocalSessionBootstrap decoded;
    assert(decodeLocalBootstrap(bytes, decoded));
    assert(decoded.port == example.port && decoded.epoch == example.epoch && decoded.secret == example.secret);
    assert(decoded.peerSha256 == example.peerSha256 && decoded.connectionId == example.connectionId);
    auto bad = bytes; bad[4] = 2;
    assert(!decodeLocalBootstrap(bad, decoded) && decoded.secret == example.secret);
    bad = bytes; bad[15] = 'z';
    assert(!decodeLocalBootstrap(bad, decoded));
    bad = bytes; bad[5] = bad[6] = 0;
    assert(!decodeLocalBootstrap(bad, decoded));
    auto invalid = example; invalid.connectionId.clear();
    assert(!encodeLocalBootstrap(invalid, bytes));

#if defined(_WIN32)
    TrustedTransportEventQueue queue;
    LocalSessionOwner owner(queue);
    LocalSocketReceiver::Options options;
    options.sessionTimeoutMs = 15000;
    LocalTransportBinding transport{70, std::string(64, 'a'), std::string(64, 'b'), true};
    LocalHostConsent consent{70, transport.peerSha256, transport.connectionId, true, true};
    // A real generated secret crosses into the intended child over the private
    // pipe; its TCP command is then authenticated and appears in the native queue.
    {
        WindowsBootstrapChannel channel;
        assert(channel.create() && !channel.create());
        Child child(channel.name(), L"success");
        assert(owner.start(options, transport, consent, [&](const LocalSessionBootstrap& bootstrap) {
            assert(channel.name().find(std::wstring(bootstrap.secret.begin(), bootstrap.secret.end())) == std::wstring::npos);
            return channel.deliverTo(child.process.hProcess, bootstrap);
        }));
        child.expectSuccess();
        const auto events = queue.drain();
        assert(events.size() == 2 && events.back().wire == "1|70|1|private_child|arm|1");
        assert(!channel.deliverTo(child.process.hProcess, example)); // consumed once
        owner.stop(); queue.drain();
    }
    // A different process under the same account gets no bootstrap bytes.
    {
        WindowsBootstrapChannel channel; assert(channel.create());
        Child intruder(channel.name(), L"expect-refused");
        Child intended(channel.name(), L"delayed");
        ++transport.epoch; consent.epoch = transport.epoch;
        assert(owner.start(options, transport, consent, [&](const LocalSessionBootstrap& bootstrap) {
            return channel.deliverTo(intended.process.hProcess, bootstrap);
        }));
        intruder.expectSuccess(); intended.expectSuccess();
        owner.stop(); queue.drain();
    }
    for (const auto& mode : {L"wrong-server", L"reject", L"throw"}) {
        WindowsBootstrapChannel channel; assert(channel.create());
        Child child(channel.name(), mode);
        ++transport.epoch; consent.epoch = transport.epoch;
        assert(!owner.start(options, transport, consent, [&](const LocalSessionBootstrap& bootstrap) {
            return channel.deliverTo(child.process.hProcess, bootstrap, 1000);
        }));
        child.expectSuccess();
        const auto events = queue.drain();
        assert(!owner.running() && events.size() == 1 &&
               events.front().kind == TrustedTransportEvent::Kind::authenticationLost);
    }
    {
        WindowsBootstrapChannel channel; assert(channel.create());
        Child child(channel.name(), L"no-receipt");
        ++transport.epoch; consent.epoch = transport.epoch;
        const auto started = std::chrono::steady_clock::now();
        assert(!owner.start(options, transport, consent, [&](const LocalSessionBootstrap& bootstrap) {
            return channel.deliverTo(child.process.hProcess, bootstrap, 500);
        }));
        assert(std::chrono::steady_clock::now() - started < std::chrono::seconds(1));
        const auto events = queue.drain();
        assert(!owner.running() && events.size() == 1 &&
               events.front().kind == TrustedTransportEvent::Kind::authenticationLost);
    }
    {
        WindowsBootstrapChannel channel; assert(channel.create());
        Child child(channel.name(), L"stall");
        const auto started = std::chrono::steady_clock::now();
        assert(!channel.deliverTo(child.process.hProcess, example, 50));
        assert(std::chrono::steady_clock::now() - started < std::chrono::seconds(1));
    }
    {
        WindowsBootstrapChannel channel; assert(channel.create());
        Child child(channel.name(), L"stall");
        bool result = true;
        std::thread waiting([&] { result = channel.deliverTo(child.process.hProcess, example, 5000); });
        Sleep(50);
        channel.cancel();
        waiting.join();
        assert(!result);
    }
    assert(!receiveWindowsBootstrap(L"\\\\remote\\pipe\\unsafe", GetCurrentProcess(),
        [](const LocalSessionBootstrap&) { assert(false); return true; }));
    { WindowsBootstrapChannel channel; assert(channel.create());
      assert(!channel.deliverTo(nullptr, example)); }
#else
    WindowsBootstrapChannel channel;
    assert(!channel.create() && channel.name().empty());
    assert(!channel.deliverTo(nullptr, example));
    assert(!receiveWindowsBootstrap(L"anything", nullptr, {}));
#endif
}
