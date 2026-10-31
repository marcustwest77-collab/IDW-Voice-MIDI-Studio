// Test-only host for a packaged TLS child through the actual pinned manager.
// Test policy is explicit; no JUCE actions, discovery policy or production UI.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <io.h>
#include <fcntl.h>
#endif
#include "../RemoteWindowsBridgeSession.h"
#include "../RemoteTransportSessionLifecycle.h"
#include "../RemoteValidatedCommandReviewReceiver.h"
#include <iostream>
using namespace idw::daw::remote;

int main() {
#if defined(_WIN32)
    _set_error_mode(_OUT_TO_STDERR);
    if(GetFileType(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdin))))!=FILE_TYPE_PIPE ||
       GetFileType(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))))!=FILE_TYPE_PIPE ||
       _setmode(_fileno(stdin),_O_BINARY)==-1 || _setmode(_fileno(stdout),_O_BINARY)==-1) return 2;
    int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
    struct FreeArgs {wchar_t** value;~FreeArgs(){if(value) LocalFree(value);}} freeArgs{args};
    if(!args || count!=2) return 2;
    auto readLine=[](std::string& line) {
        line.clear();char c=0;
        while(std::cin.get(c)) {
            if(c=='\n') return true;
            if(line.size()>=256) return false;
            line.push_back(c);
        }
        return false;
    };
    std::string setup;
    if(!readLine(setup) || setup.size()!=129 || setup[64]!='|' ||
       !validBootstrapHex(setup.substr(0,64)) || !validBootstrapHex(setup.substr(65))) return 3;
    TrustedTransportEventQueue events;
    WindowsBridgeSession session(events);
    LocalSocketReceiver::Options options;options.sessionTimeoutMs=60000;
    // These are intentionally trusted TEST approvals, not a production consent UI.
    const auto approve=[](const LocalTLSOffer& offer,LocalHostConsent& consent,const std::atomic<bool>& cancel) {
        if(cancel.load()) return false;
        consent={offer.epoch,offer.peerSha256,offer.connectionId,true,true};return true;
    };
    if(!session.startWithOffer(args[1],setup.substr(0,64),options,45,setup.substr(65),approve,5000)) return 4;
    std::cout << "ACTIVE\n" << std::flush;
    SessionReviewCoordinator coordinator;
    RemoteReviewMailbox mailbox;
    TransportSessionLifecycle lifecycle(coordinator,mailbox);
    ValidatedCommandReviewReceiver review(coordinator);
    Request live;live.trackCount=4;live.trackIsAudio=true;
    std::string command;
    while(readLine(command)) {
        if(command=="QUIT") break;
        if(command=="STOP") session.stop();
        else if(command!="DRAIN") return 5;
        for(const auto& event:events.drain()) {
            if(event.kind==TrustedTransportEvent::Kind::connected)
                lifecycle.onVerifiedPeerConnected(event.epoch,event.tlsVerified,event.hostConsented,event.trackControlGranted);
            else if(event.kind==TrustedTransportEvent::Kind::command)
                lifecycle.onAuthenticatedTrackCommand(event.wire,event.epoch);
            else lifecycle.onAuthenticationLost();
        }
        for(const auto& item:mailbox.drain()) review.receive(item.payload,live,item.trustedEpoch);
        std::cout << "PENDING " << coordinator.pending().size() << '\n' << std::flush;
    }
    session.stop();return 0;
#else
    return 2;
#endif
}
