// Test-only process: private parent stdio -> native socket -> queue/host review.
// Not the production bridge, JUCE GUI, or a TLS identity provider. Never deploy.
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
#include <io.h>
#include <fcntl.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "../RemoteLocalBootstrapWire.h"
#include "../RemoteWindowsBootstrapChannel.h"
#include "../RemoteTransportSessionLifecycle.h"
#include "../RemoteValidatedCommandReviewReceiver.h"
#include <iostream>
#include <cstdlib>
#include "RemotePythonTLSChildFixture.h"
using namespace idw::daw::remote;

bool privateStdio() {
#if defined(_WIN32)
    return GetFileType(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdin)))) == FILE_TYPE_PIPE &&
           GetFileType(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout)))) == FILE_TYPE_PIPE &&
           _setmode(_fileno(stdin), _O_BINARY) != -1 && _setmode(_fileno(stdout), _O_BINARY) != -1;
#else
    struct stat input{}, output{};
    return fstat(STDIN_FILENO, &input) == 0 && fstat(STDOUT_FILENO, &output) == 0 &&
           S_ISFIFO(input.st_mode) && S_ISFIFO(output.st_mode);
#endif
}
bool readControl(std::string& line) {
    line.clear();
    char c = 0;
    while (std::cin.get(c)) {
        if (c == '\n') return true;
        if (line.size() == 256) return false;
        line.push_back(c);
    }
    return false;
}
int main(int argc, char** argv) {
    if (!privateStdio()) return 2; // Never print a credential to a terminal/file.
    const bool runtimeChild=argc==5 && std::string(argv[1])=="--runtime-child-host";
    const bool privateOffer=runtimeChild || (argc==3 && std::string(argv[1])=="--private-offer");
    const bool privateBootstrap=privateOffer || (argc==3 && std::string(argv[1])=="--private-bootstrap");
    if (argc!=1 && !privateBootstrap) return 2;
    WindowsBootstrapChannel channel;
#if defined(_WIN32)
    struct PeerHandle { HANDLE value=nullptr; ~PeerHandle() { if(value) CloseHandle(value); } } peer;
    FixtureHandle runtimeJob;
    if (privateBootstrap) {
        if(!runtimeChild) {
            char* end=nullptr;
            const auto pid=std::strtoul(argv[2],&end,10);
            if (!end || *end || pid==0 || pid==GetCurrentProcessId()) return 2;
            peer.value=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
            if(!peer.value) return 2;
        }
        if (!channel.create()) return 2;
    }
#else
    if (privateBootstrap) return 2;
#endif
    std::string setup;
    if (!readControl(setup) || setup.size() != 129 || setup[64] != '|' ||
        !validBootstrapHex(setup.substr(0, 64)) || !validBootstrapHex(setup.substr(65))) return 3;
    LocalTransportBinding transport{45, setup.substr(0, 64), setup.substr(65), true};
#if defined(_WIN32)
    auto emitPipe=[&] {
        const auto name=channel.name();std::string ascii;
        for(const auto c:name) {if(c>127) return false;ascii.push_back(static_cast<char>(c));}
        std::cout << "PIPE " << ascii << '\n' << std::flush;return bool(std::cout);
    };
    if(privateOffer) {
        LocalTLSOffer offer;
        if(runtimeChild) {
            std::string ready;
            if(!launchPythonTLSFixture(channel.name(),peer.value,runtimeJob,ready)) return 6;
            std::cout << ready << '\n' << std::flush;
        } else if(!emitPipe()) return 6;
        if(!channel.receiveOfferFrom(peer.value,offer,runtimeChild?5000:2000) ||
            offer.epoch!=45 || offer.peerSha256!=setup.substr(0,64)) return 6;
        // Trusted test bridge owns the actual TLS socket. This fixture policy
        // checks its approved pin/epoch; production still needs real host consent.
        transport={offer.epoch,offer.peerSha256,offer.connectionId,true};
    }
#endif
    LocalHostConsent consent{45, transport.peerSha256, transport.connectionId, true, true};
    TrustedTransportEventQueue events;
    LocalSessionOwner owner(events);
    LocalSocketReceiver::Options options;
    options.sessionTimeoutMs = 60000;
    if (!owner.start(options, transport, consent, [&](const LocalSessionBootstrap& bootstrap) {
#if defined(_WIN32)
        if (privateBootstrap) {
            if(!privateOffer && !emitPipe()) return false;
            return channel.deliverTo(peer.value,bootstrap);
        }
#endif
        LocalBootstrapBytes bytes{};
        if (!encodeLocalBootstrap(bootstrap, bytes)) return false;
        std::cout.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::cout.flush();
        return bool(std::cout);
    })) return 4;
    SessionReviewCoordinator coordinator;
    RemoteReviewMailbox mailbox;
    TransportSessionLifecycle lifecycle(coordinator, mailbox);
    ValidatedCommandReviewReceiver review(coordinator);
    Request live;
    live.trackCount = 4;
    live.trackIsAudio = true;
    auto drain = [&] {
        for (const auto& event : events.drain()) {
            if (event.kind == TrustedTransportEvent::Kind::connected)
                lifecycle.onVerifiedPeerConnected(event.epoch, event.tlsVerified,
                    event.hostConsented, event.trackControlGranted);
            else if (event.kind == TrustedTransportEvent::Kind::command)
                lifecycle.onAuthenticatedTrackCommand(event.wire, event.epoch);
            else lifecycle.onAuthenticationLost();
        }
        for (const auto& item : mailbox.drain()) review.receive(item.payload, live, item.trustedEpoch);
        std::cout << "PENDING " << coordinator.pending().size() << '\n' << std::flush;
    };
    std::string control;
    while (readControl(control)) {
        if (control == "QUIT") break;
        if (control == "REVOKE") owner.onTransportLost(transport);
        else if (control == "BUSY") live.transportBusy = true;
        else if (control == "IDLE") live.transportBusy = false;
        else if (control != "DRAIN") return 5;
        drain();
    }
    owner.stop();
    return 0;
}
