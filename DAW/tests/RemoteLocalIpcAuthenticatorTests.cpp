#include "../RemoteLocalIpcAuthenticator.h"
#include <iostream>
#include <string>
int main() {
    using idw::daw::remote::LocalIpcAuthenticator;
    const std::string token(64,'a');
    LocalIpcAuthenticator gate(token,45);
    std::string wire;
    int failed=0;
    auto check=[&](bool ok,const char* name){if(!ok){++failed;std::cerr<<name<<" failed\n";}};
    check(gate.valid(),"valid token");
    check(!gate.accept(std::string(64,'b')+"\n1|45|1|review|arm|1",wire),"wrong token");
    check(!gate.accept(token+"\n1|46|1|review|arm|1",wire),"wrong epoch");
    check(!gate.accept(token+"\n1|45|1|review|record|1",wire),"disallowed action");
    check(!gate.accept(token+"\n1|45|1|review|arm|128",wire),"track range");
    check(!gate.accept(token+"\n1|45|1|review|arm|1|extra",wire),"extra fields");
    check(!gate.accept(token+"\n1|45|01|review|arm|1",wire),"noncanonical number");
    check(!gate.accept(token+"\n1|45|1|bad id|arm|1",wire),"bad id");
    check(!gate.accept(token+"\n1|45|1|review|arm|-1",wire),"negative track");
    check(!gate.accept(token+"\n1|45|1|review|arm|18446744073709551616",wire),"overflow");
    check(gate.accept(token+"\n1|45|1|review|arm|1",wire) &&
          wire=="1|45|1|review|arm|1","accept");
    check(!gate.accept(token+"\n1|45|1|review|arm|1",wire),"replay");
    check(gate.accept(token+"\n1|45|2|review2|select|0",wire),"next sequence");
    check(!gate.accept(token+"\n1|45|2|review3|arm|1",wire),"duplicate sequence");
    check(!LocalIpcAuthenticator(std::string(64,'z'),45).valid(),"weakly formatted secret");
    check(!LocalIpcAuthenticator(token,0).valid(),"zero epoch");
    return failed ? 1 : 0;
}
