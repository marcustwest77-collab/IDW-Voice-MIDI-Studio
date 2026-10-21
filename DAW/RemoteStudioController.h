#pragma once
#include "RemoteRecordingProtocol.h"
#include <juce_events/juce_events.h>
#include <set>
namespace idw {
class RemoteStudioController : private juce::Timer {
public:
    std::function<void(const juce::var&)> send;
    std::function<void(const juce::String&)> status;
    std::function<void(const juce::var&)> tracksChanged;
    using Clock=std::chrono::steady_clock;
    explicit RemoteStudioController(std::function<Clock::time_point()> clock=[] {return Clock::now();})
        : now(std::move(clock)){startTimer(250);}
    ~RemoteStudioController() override {stopTimer();}
    void reset(){session.clear();peer.clear();request.clear();clearGrant();}
    void receive(const juce::var& value){
        auto* o=value.getDynamicObject();if(!o)return;
        auto type=o->getProperty("type").toString();
        if(type=="joined"){
            reset();role=o->getProperty("role").toString();
            if(role!="host" && role!="guest")return;
            session=o->getProperty("control_session").toString();
            if((bool)o->getProperty("peer_present"))peer=role=="host"?"guest":"host";
            say(peer.isEmpty()?"Waiting for the other participant.":"Connected. Request control, then ask the other person to allow it.");return;
        }
        if(type=="peer_joined"){
            clearGrant();request.clear();session=o->getProperty("control_session").toString();
            peer=role=="host"?"guest":"host";
            say("Peer connected. Fresh recording permission is required.");return;
        }
        if(type=="peer_left" || type=="peer_unavailable"){
            reset();say("Peer unavailable. Recording control revoked.");return;
        }
        if(peer.isEmpty() || o->getProperty("from").toString()!=peer || o->getProperty("session").toString()!=session)return;
        if(type=="control_grant"){
            uint64_t id=0;auto ttl=o->getProperty("ttl_seconds");auto scope=o->getProperty("scope").toString();
            auto ids=o->getProperty("tracks");
            if(request.isEmpty() || o->getProperty("request").toString()!=request ||
               !RemoteRecordingProtocol::number(o->getProperty("grant"),id) || !ttl.isInt() ||
               int(ttl)<1 || int(ttl)>900 || (scope!="all" && scope!="selected") || !ids.isArray())return;
            std::set<std::string> approved;
            for(const auto& track:*ids.getArray()){
                if(!RemoteRecordingProtocol::token(track) || !approved.insert(track.toString().toStdString()).second)return;
            }
            if((scope=="all" && !approved.empty()) || (scope=="selected" && (approved.empty() || approved.size()>128)))return;
            clearGrant();grant=id;all=scope=="all";allowed=std::move(approved);
            expires=now()+std::chrono::seconds(int(ttl));request.clear();
            say("Permission received. Waiting for approved track list.");return;
        }
        uint64_t id=0;
        if(!RemoteRecordingProtocol::number(o->getProperty("grant"),id) || id!=grant || !grant)return;
        if(type=="control_revoke"){clearGrant();say("The other person revoked recording control.");return;}
        if(type=="control_tracks"){
            auto list=o->getProperty("tracks");auto* array=list.getArray();if(!array || array->size()>128)return;
            std::set<std::string> seen;
            for(const auto& t:*array){
                auto track=t.getProperty("id",{});auto name=t.getProperty("name",{});
                if(!RemoteRecordingProtocol::token(track) || !name.isString() || name.toString().length()>256 ||
                   !seen.insert(track.toString().toStdString()).second || (!all && !allowed.count(track.toString().toStdString())))return;
            }
            known=std::move(seen);if(tracksChanged)tracksChanged(list);
            say("Recording control ready. Record affects all approved armed tracks.");return;
        }
        if(type=="control_result"){
            uint64_t seq=0;if(!RemoteRecordingProtocol::number(o->getProperty("sequence"),seq) || !pending || seq!=pending)return;
            const auto result=o->getProperty("status").toString();
            if(result!="succeeded" && result!="denied" && result!="not_ready" && result!="failed")return;
            pending=0;say("Command result: "+result+". This does not confirm recorded audio quality.");
        }
    }
    void requestControl(){
        if(!send || peer.isEmpty()){say("Connect both participants first.");return;}
        clearGrant();request=juce::Uuid().toString();auto v=message("control_request");
        v.getDynamicObject()->setProperty("request",request);send(v);say("Request sent. The other person must open My Permissions and allow control.");
    }
    bool command(const std::string& action,const std::string& track){
        if(!send || !grant || now()>=expires || pending || !known.count(track) ||
           (action!="arm" && action!="record" && action!="stop") || sequence==UINT64_MAX){
            say("Command unavailable: check permission, track and pending result.");return false;
        }
        pending=++sequence;pendingUntil=now()+std::chrono::seconds(10);
        auto v=message("control_command");auto* o=v.getDynamicObject();
        o->setProperty("grant",juce::String(std::to_string(grant)));o->setProperty("sequence",juce::String(std::to_string(pending)));
        o->setProperty("action",juce::String(action));o->setProperty("track",juce::String(track));send(v);
        say("Command sent; waiting for recorder result…");return true;
    }
    void tick(){
        if(grant && now()>=expires){clearGrant();say("Recording permission expired. Request control again.");}
        else if(pending && now()>=pendingUntil){
            clearGrant();say("No command result. Recording state is unknown; ask the performer to check. No retry was sent.");
        }
    }
private:
    std::function<Clock::time_point()> now;
    juce::String role,peer,session,request;
    uint64_t grant=0,sequence=0,pending=0;bool all=false;
    std::set<std::string> allowed,known;Clock::time_point expires{},pendingUntil{};
    void say(const juce::String& text){if(status)status(text);}
    void clearGrant(){grant=sequence=pending=0;known.clear();allowed.clear();if(tracksChanged)tracksChanged(juce::var(juce::Array<juce::var>{}));}
    juce::var message(const char* type){auto* o=new juce::DynamicObject();o->setProperty("type",type);o->setProperty("session",session);return juce::var(o);}
    void timerCallback() override {tick();}
};
}
