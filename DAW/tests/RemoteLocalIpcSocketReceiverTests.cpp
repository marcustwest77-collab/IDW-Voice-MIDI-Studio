#include "../RemoteLocalIpcSocketReceiver.h"
#include <chrono>
#include <iostream>
#include <thread>
int main() {
    using namespace idw::daw::remote;
    int fails=0;
    auto check=[&](bool p,const char* s){if(!p){++fails;std::cerr<<s<<" failed\n";}};
    TrustedTransportEventQueue queue;
    const std::string token(64,'a');
    constexpr int port=46219;
    LocalIpcSocketReceiver receiver(queue,port,token,45);
    check(receiver.start(),"start");
    for(int i=0;i<100 && !receiver.listening();++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    check(receiver.listening(),"bind loopback");
    auto send=[&](const std::string& payload) {
        juce::StreamingSocket client;
        if (!client.connect("127.0.0.1",port,1500)) return '?';
        const auto n=static_cast<std::uint32_t>(payload.size());
        const char prefix[4]={char(n>>24),char(n>>16),char(n>>8),char(n)};
        if(client.write(prefix,4)!=4 ||
           client.write(payload.data(),static_cast<int>(payload.size()))!=static_cast<int>(payload.size()))
            return '?';
        if(client.waitUntilReady(true,2500)<=0) return '?';
        char response='?';
        return client.read(&response,1,false)==1 ? response : '?';
    };
    check(send(std::string(64,'b')+"\n1|45|1|review|arm|1")=='0',"unauthorized denied");
    check(queue.drain().empty(),"unauthorized produced no event");
    check(send(token+"\n1|45|1|review|arm|1")=='1',"authorized admitted");
    auto events=queue.drain();
    check(events.size()==2,"connected and command");
    if(events.size()==2) {
        check(events[0].kind==TrustedTransportEvent::Kind::connected &&
              events[0].localTokenVerified && !events[0].tlsVerified &&
              events[0].hostConsented,"authenticated admission flags");
        check(events[1].kind==TrustedTransportEvent::Kind::command &&
              events[1].wire=="1|45|1|review|arm|1","queue handoff");
    }
    check(send(token+"\n1|45|1|review|arm|1")=='0',"replay rejected");
    check(send(token+"\n1|45|2|review2|select|0")=='1',"new command");
    events=queue.drain();
    check(events.size()==1 && events[0].kind==TrustedTransportEvent::Kind::command,
          "only command on subsequent connection");
    receiver.stop();
    events=queue.drain();
    check(events.size()==1 &&
          events[0].kind==TrustedTransportEvent::Kind::disconnected,"shutdown revokes");
    return fails?1:0;
}
