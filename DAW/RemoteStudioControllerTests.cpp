#include "RemoteStudioController.h"
#include "StudioNetworkClient.h"
#include <iostream>
#include <stdexcept>
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    check(StudioNetworkClient::validOrigin("https://studio.example.com"),"secure origin");
    check(StudioNetworkClient::validOrigin("http://127.0.0.1:8787"),"loopback origin");
    for(const auto& url:{"http://example.com","https://user:pass@example.com","https://example.com/path","https://example.com\r\nX: y"})
        check(!StudioNetworkClient::validOrigin(url),"reject unsafe origin");
    auto now=idw::RemoteStudioController::Clock::now();
    idw::RemoteStudioController c([&]{return now;});std::vector<juce::var> sent;juce::var visible;juce::String status;
    c.send=[&](const auto& v){sent.push_back(v);};c.tracksChanged=[&](const auto& v){visible=v;};
    c.status=[&](const auto& v){status=v;};
    c.requestControl();check(sent.empty(),"no request before join");
    c.receive(juce::JSON::parse(R"({"type":"joined","role":"host","peer_present":true,"control_session":"abcdefghijklmnopqrstuvwx"})"));
    c.requestControl();check(sent.size()==1,"request sends");
    auto grant=juce::JSON::parse(R"({"type":"control_grant","from":"guest","session":"abcdefghijklmnopqrstuvwx","grant":"1","scope":"all","tracks":[],"ttl_seconds":60})");
    grant.getDynamicObject()->setProperty("request",sent.back().getProperty("request",{}));
    c.receive(grant);
    check(!c.command("record","a"),"catalog required");
    c.receive(juce::JSON::parse(R"({"type":"control_tracks","from":"guest","session":"abcdefghijklmnopqrstuvwx","grant":"1","tracks":[{"id":"a","name":"Lead"}]})"));
    check(visible.isArray() && visible.getArray()->size()==1,"catalog visible");
    check(!c.command("record","b"),"unlisted track denied");
    check(c.command("arm","a"),"approved command");
    check(!c.command("record","a"),"one pending command");
    c.receive(juce::JSON::parse(R"({"type":"control_result","from":"guest","session":"abcdefghijklmnopqrstuvwx","grant":"1","sequence":"99","status":"succeeded"})"));
    check(!c.command("record","a"),"unmatched result ignored");
    c.receive(juce::JSON::parse(R"({"type":"control_result","from":"guest","session":"abcdefghijklmnopqrstuvwx","grant":"1","sequence":"1","status":"not_ready"})"));
    check(c.command("record","a"),"matching result releases pending");
    c.receive(juce::JSON::parse(R"({"type":"control_revoke","from":"guest","session":"abcdefghijklmnopqrstuvwx","grant":"1"})"));
    check(!c.command("stop","a") && visible.getArray()->isEmpty(),"revoke clears controls");
    c.requestControl();grant.getDynamicObject()->setProperty("request",sent.back().getProperty("request",{}));c.receive(grant);
    now+=std::chrono::seconds(60);c.tick();
    check(status.contains("expired") && !c.command("arm","a"),"expiry clears controls");
    c.requestControl();grant.getDynamicObject()->setProperty("request",sent.back().getProperty("request",{}));c.receive(grant);
    c.receive(juce::JSON::parse(R"({"type":"control_tracks","from":"guest","session":"abcdefghijklmnopqrstuvwx","grant":"1","tracks":[{"id":"a","name":"Lead"}]})"));
    check(c.command("record","a"),"command before timeout");auto before=sent.size();
    now+=std::chrono::seconds(10);c.tick();
    check(status.contains("unknown") && sent.size()==before && !c.command("record","a"),"timeout never retries");
    c.requestControl();grant.getDynamicObject()->setProperty("request",sent.back().getProperty("request",{}));c.receive(grant);
    c.receive(juce::JSON::parse(R"({"type":"control_tracks","from":"guest","session":"abcdefghijklmnopqrstuvwx","grant":"1","tracks":[{"id":"a","name":"Lead"}]})"));
    c.send=[&](const auto&){c.reset();status="Connection failed; recording state unknown.";};
    check(!c.command("record","a"),"synchronous send failure is not success");
    check(status.contains("Connection failed") && visible.getArray()->isEmpty(),"send failure status and cleared controls preserved");
    c.receive(juce::JSON::parse(R"({"type":"joined","role":"host","peer_present":true,"control_session":"abcdefghijklmnopqrstuvwx"})"));
    c.requestControl();check(status.contains("Connection failed"),"request failure status preserved");
    c.receive(juce::JSON::parse(R"({"type":"peer_left","role":"guest"})"));
    auto count=sent.size();c.requestControl();check(sent.size()==count,"departed peer cannot request");
    std::cout<<"Studio controller tests: PASS\n";
}
