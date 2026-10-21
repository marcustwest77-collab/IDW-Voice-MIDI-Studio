#include "RemoteRecordingProtocol.h"
#include <stdexcept>
#include <iostream>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
    using Protocol=idw::RemoteRecordingProtocol;
    uint64_t value=0;
    check(Protocol::number(juce::var("18446744073709551615"),value),"uint64 max");
    for(const auto& text:{"18446744073709551616","0","01","-1","1.0",""})
        check(!Protocol::number(juce::var(text),value),"invalid number");
    check(!Protocol::number(juce::var(1),value),"numeric JSON loses precision");
    Protocol p;int calls=0,changes=0;std::vector<juce::var> sent;
    p.send=[&](const auto& v){sent.push_back(v);};
    p.peerChanged=[&](const auto&,const auto&){++changes;};
    p.execute=[&](const auto& peer,const auto& session,const auto& track,uint64_t grant,uint64_t seq,auto action){
        check(peer=="guest" && session=="abcdefghijklmnopqrstuvwx" && track=="track-a" &&
              grant==1 && seq==2 && action==idw::RemoteRecordingControl::Action::Record,"dispatch fields");
        ++calls;return std::string("not_ready");
    };
    const auto command=juce::JSON::parse(R"({"type":"control_command","session":"abcdefghijklmnopqrstuvwx","from":"guest","grant":"1","sequence":"2","action":"record","track":"track-a"})");
    check(!p.receive(command) && calls==0,"unjoined command");
    check(p.receive(juce::JSON::parse(R"({"type":"joined","role":"host","peer_present":true,"control_session":"abcdefghijklmnopqrstuvwx"})")),"join");
    check(p.receive(command) && calls==1 && sent.size()==1,"dispatch once");
    check(sent.back().getProperty("status",{}).toString()=="not_ready","actual result");
    auto wrong=juce::JSON::parse(juce::JSON::toString(command));
    wrong.getDynamicObject()->setProperty("from","host");
    check(!p.receive(wrong) && calls==1,"sender spoof");
    wrong.getDynamicObject()->setProperty("from","guest");
    wrong.getDynamicObject()->setProperty("session","old");
    check(!p.receive(wrong),"old connection");
    check(p.receive(juce::JSON::parse(R"({"type":"control_request","session":"abcdefghijklmnopqrstuvwx","from":"guest","request":"req1"})")),"request");
    idw::RemoteRecordingControl gate;
    auto grant=gate.grant("guest",idw::RemoteRecordingControl::Scope::AllTracks,{},7,
        idw::RemoteRecordingControl::Clock::now(),std::chrono::seconds(900));
    check(p.publishGrant(*grant),"publish local grant");
    check(sent.back().getProperty("scope",{}).toString()=="all","all scope");
    p.publishRevoke();check(sent.back().getProperty("type",{}).toString()=="control_revoke","revoke");
    check(p.receive(juce::JSON::parse(R"({"type":"peer_left","role":"guest"})")),"peer leave");
    check(!p.receive(command) && calls==1,"departed peer denied");
    check(!p.publishGrant(*grant),"departed grant denied");
    p.disconnect();check(changes>=3,"connection invalidates consent");
    check(p.receive(juce::JSON::parse(R"({"type":"joined","role":"host","peer_present":true,"control_session":"abcdefghijklmnopqrstuvwx"})")),"rejoin for send failure");
    check(p.receive(juce::JSON::parse(R"({"type":"control_request","session":"abcdefghijklmnopqrstuvwx","from":"guest","request":"req2"})")),"fresh request");
    int failedSends=0;
    p.send=[&, marker=std::string("callback survives reset")](const auto&){
        ++failedSends;p.disconnect();p.send={};
        check(marker=="callback survives reset","callback retained through reset");
    };
    check(!p.publishGrant(*grant),"failed grant is not reported as published");
    p.publishTracks(*grant,juce::var(juce::Array<juce::var>{}));
    p.publishRevoke();
    check(failedSends==1,"disconnect suppresses follow-up messages");
    std::cout<<"Remote recording protocol tests: PASS\n";
}
