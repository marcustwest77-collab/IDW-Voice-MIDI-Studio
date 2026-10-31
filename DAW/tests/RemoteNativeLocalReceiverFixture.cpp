#include "../RemoteNativeLocalReceiver.h"
#include <iostream>
int main() {
    using namespace idw::daw::remote;
    std::string token;
    if (!std::getline(std::cin,token)) return 1; // Test secret through stdin, not argv/logs.
    TrustedTransportEventQueue queue;
    NativeLocalReceiver receiver(queue);
    if (!receiver.start(token,"peer",45)) return 2;
    std::cout<<receiver.port()<<std::endl;
    std::string operation;
    while (std::getline(std::cin,operation)) {
        if (operation=="drain") {
            auto events=queue.drain();
            std::size_t commands=0;
            for (const auto& event : events)
                if (event.kind==TrustedTransportEvent::Kind::command) ++commands;
            std::cout<<commands<<std::endl;
        } else if (operation=="admitted") {
            std::cout<<(queue.isAdmitted(45) ? "yes" : "no")<<std::endl;
        } else if (operation=="revoke") {
            queue.push({TrustedTransportEvent::Kind::authenticationLost,45,false,false,false,{}});
            std::cout<<"revoked"<<std::endl;
        } else if (operation=="stop") {
            receiver.stop();
            std::cout<<"stopped"<<std::endl;
            break;
        }
    }
}
