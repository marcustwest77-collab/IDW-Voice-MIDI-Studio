#pragma once
#include <juce_core/juce_core.h>
#include "RemoteRecordingControl.h"
#include <functional>
#include <limits>

namespace idw {
// Message-thread adapter for messages from the authenticated room service only.
// This class does not open a socket or authenticate an arbitrary JSON sender.
class RemoteRecordingProtocol {
public:
    using Gate=RemoteRecordingControl;
    std::function<void(const std::string&,const std::string&)> peerChanged;
    std::function<std::string(const std::string&,const std::string&,const std::string&,
        uint64_t,uint64_t,Gate::Action)> execute;
    std::function<void(const juce::var&)> send;

    void disconnect(){
        role.clear();peer.clear();session.clear();request.clear();lastGrant=0;
        if(peerChanged)peerChanged({},{});
    }
    static bool number(const juce::var& value,uint64_t& out){
        if(!value.isString())return false;
        const auto s=value.toString().toStdString();
        if(s.empty() || s.size()>20 || s.front()=='0')return false;
        uint64_t n=0;
        for(char c:s){
            if(c<'0'||c>'9' || n>(std::numeric_limits<uint64_t>::max()-uint64_t(c-'0'))/10)return false;
            n=n*10+uint64_t(c-'0');
        }
        out=n;return true;
    }
    static bool token(const juce::var& value,size_t max=128){
        if(!value.isString())return false;
        const auto s=value.toString().toStdString();
        return !s.empty() && s.size()<=max && s.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos;
    }
    bool receive(const juce::var& data){
        auto* obj=data.getDynamicObject();if(!obj)return false;
        const auto type=obj->getProperty("type").toString();
        if(type=="joined"){
            disconnect();
            const auto r=obj->getProperty("role").toString();
            const auto s=obj->getProperty("control_session");
            const auto present=obj->getProperty("peer_present");
            if((r!="host" && r!="guest") || !token(s,24) || s.toString().length()!=24 || !present.isBool())return false;
            role=r.toStdString();session=s.toString().toStdString();
            if((bool)present)activate();
            return true;
        }
        if(type=="peer_joined"){
            const auto r=obj->getProperty("role").toString().toStdString();
            const auto s=obj->getProperty("control_session");
            if(role.empty() || r!=(role=="host"?"guest":"host") || !token(s,24) || s.toString().length()!=24)return false;
            session=s.toString().toStdString();request.clear();activate();return true;
        }
        if(type=="peer_left" || type=="peer_unavailable"){
            peer.clear();session.clear();request.clear();
            if(peerChanged)peerChanged({},{});
            return true;
        }
        if(type!="control_request" && type!="control_command")return false;
        if(peer.empty() || !send || obj->getProperty("from").toString().toStdString()!=peer ||
           obj->getProperty("session").toString().toStdString()!=session)return false;
        if(type=="control_request"){
            if(obj->getProperties().size()!=4 || !token(obj->getProperty("request")))return false;
            request=obj->getProperty("request").toString().toStdString();return true;
        }
        uint64_t grant=0,sequence=0;
        if(obj->getProperties().size()!=7 || !number(obj->getProperty("grant"),grant) ||
           !number(obj->getProperty("sequence"),sequence) || !token(obj->getProperty("track")))return false;
        const auto action=obj->getProperty("action").toString();
        if(action!="arm" && action!="record" && action!="stop")return false;
        const auto result=execute ? execute(peer,session,obj->getProperty("track").toString().toStdString(),
            grant,sequence,action=="arm"?Gate::Action::Arm:action=="record"?Gate::Action::Record:Gate::Action::Stop) : "denied";
        auto reply=message("control_result");
        reply.getDynamicObject()->setProperty("grant",obj->getProperty("grant"));
        reply.getDynamicObject()->setProperty("sequence",obj->getProperty("sequence"));
        reply.getDynamicObject()->setProperty("status",juce::String(result));
        send(reply);return true;
    }
    bool publishGrant(const Gate::Grant& grant){
        if(!send || peer.empty() || request.empty() || grant.peer!=peer)return false;
        auto reply=message("control_grant");auto* obj=reply.getDynamicObject();
        obj->setProperty("request",juce::String(request));
        obj->setProperty("grant",juce::String(std::to_string(grant.id)));
        obj->setProperty("scope",grant.scope==Gate::Scope::AllTracks?"all":"selected");
        juce::Array<juce::var> tracks;
        for(const auto& id:grant.tracks)tracks.add(juce::String(id));
        obj->setProperty("tracks",juce::var(tracks));
        const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(grant.expires-Gate::Clock::now()).count();
        if(seconds<=0 || seconds>900)return false;
        obj->setProperty("ttl_seconds",int(seconds));
        lastGrant=grant.id;send(reply);return true;
    }
    void publishRevoke(){
        if(send && !peer.empty() && lastGrant){
            auto reply=message("control_revoke");
            reply.getDynamicObject()->setProperty("grant",juce::String(std::to_string(lastGrant)));
            send(reply);
        }
        lastGrant=0;
    }
private:
    std::string role,peer,session,request;
    uint64_t lastGrant=0;
    void activate(){peer=role=="host"?"guest":"host";lastGrant=0;if(peerChanged)peerChanged(peer,session);}
    juce::var message(const char* type) const {
        auto* obj=new juce::DynamicObject();obj->setProperty("type",type);
        obj->setProperty("session",juce::String(session));return juce::var(obj);
    }
};
}
