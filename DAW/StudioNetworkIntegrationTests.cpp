#include "StudioNetworkClient.h"
#include "RemoteStudioController.h"
#include <iostream>
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    auto env=[](const char* name){return juce::SystemStats::getEnvironmentVariable(name,{});};
    const auto origin=env("IDW_TEST_ORIGIN"),room=env("IDW_TEST_ROOM");
    StudioNetworkClient host,guest;idw::RemoteStudioController controller;idw::RemoteRecordingProtocol receiver;
    idw::RemoteRecordingControl gate;int executed=0;bool requested=false,issued=false,success=false,left=false;
    juce::var catalog;
    controller.send=[&](const auto& message){host.send(message);};
    controller.tracksChanged=[&](const auto& tracks){catalog=tracks;};
    controller.status=[&](const auto& text){if(text.contains("Command result: succeeded"))success=true;};
    receiver.send=[&](const auto& message){guest.send(message);};
    receiver.peerChanged=[&](const auto&,const auto&){gate.revoke();};
    receiver.execute=[&](const auto& peer,const auto&,const auto& track,uint64_t id,uint64_t sequence,auto action){
        if(gate.authorize(peer,1,track,id,sequence,action,idw::RemoteRecordingControl::Clock::now())
           !=idw::RemoteRecordingControl::Result::Allowed)return std::string("denied");
        ++executed;return std::string("succeeded"); // Fake recorder; never captures audio.
    };
    host.onMessage=[&](const auto& message){
        controller.receive(message);
        auto type=message.getProperty("type",{}).toString();
        if(type=="peer_left")left=true;
        if(!requested && (type=="peer_joined" || (type=="joined" && (bool)message.getProperty("peer_present",false)))){
            requested=true;controller.requestControl();
        }
        if(type=="control_tracks" && !issued){issued=true;controller.command("record","track-a");}
    };
    guest.onMessage=[&](const auto& message){
        if(!receiver.receive(message))return;
        if(message.getProperty("type",{}).toString()=="control_request"){
            auto grant=gate.grant("host","track-a",1,idw::RemoteRecordingControl::Clock::now(),std::chrono::seconds(60));
            receiver.publishGrant(*grant);
            receiver.publishTracks(*grant,juce::JSON::parse(R"([{"id":"track-a","name":"Test audio"}])"));
        }
    };
    if(!host.connect(origin,room,env("IDW_TEST_HOST")) || !guest.connect(origin,room,env("IDW_TEST_GUEST")))return 1;
    auto pump=[&](const std::function<bool()>& done){
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
        while(!done() && std::chrono::steady_clock::now()<end)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        return done();
    };
    if(!pump([&]{return success;}) || executed!=1){std::cerr<<"Network command round trip failed\n";return 2;}
    guest.disconnect();
    if(!pump([&]{return left;}) || controller.command("record","track-a"))return 3;
    host.disconnect();std::cout<<"Windows studio network round trip: PASS\n";
}
