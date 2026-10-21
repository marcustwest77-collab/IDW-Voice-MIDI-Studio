#pragma once
#include <JuceHeader.h>
class StudioControllerPanel final : public juce::Component {
public:
    std::function<void(const juce::String&,const juce::String&,const juce::String&)> onJoin;
    std::function<void()> onLeave,onRequest,onPermissions;
    std::function<void(const std::string&,const std::string&)> onCommand;
    StudioControllerPanel(){
        server.setTextToShowWhenEmpty("Server origin: https://studio.example.com",juce::Colours::grey);
        room.setTextToShowWhenEmpty("Room ID",juce::Colours::grey);
        ticket.setTextToShowWhenEmpty("Private room ticket",juce::Colours::grey);ticket.setPasswordCharacter('*');
        for(auto* editor:{&server,&room,&ticket})addAndMakeVisible(*editor);
        for(auto* button:{&join,&leave,&request,&permissions,&arm,&record,&stop})addAndMakeVisible(*button);
        addAndMakeVisible(tracks);addAndMakeVisible(status);
        join.onClick=[this]{if(onJoin)onJoin(server.getText().trim(),room.getText().trim(),ticket.getText().trim());ticket.clear();};
        leave.onClick=[this]{if(onLeave)onLeave();};
        request.onClick=[this]{if(onRequest)onRequest();};
        permissions.onClick=[this]{if(onPermissions)onPermissions();};
        arm.onClick=[this]{command("arm");};record.onClick=[this]{command("record");};stop.onClick=[this]{command("stop");};
        status.setJustificationType(juce::Justification::topLeft);
        setStatus("Not connected. This panel controls recording only; audio and video are not transmitted.");
        enableCommands(false);setSize(640,360);
    }
    void setStatus(const juce::String& text){status.setText(text,juce::dontSendNotification);}
    void enableCommands(bool enabled){for(auto* b:{&arm,&record,&stop})b->setEnabled(enabled);}
    void clearTracks(){ids.clear();tracks.clear();enableCommands(false);}
    void setTracks(const juce::var& values){
        clearTracks();auto* array=values.getArray();if(!array)return;
        for(const auto& value:*array){
            auto id=value.getProperty("id",{}).toString().toStdString();
            ids.push_back(id);tracks.addItem(value.getProperty("name",{}).toString(),static_cast<int>(ids.size()));
        }
        if(!ids.empty())tracks.setSelectedId(1);enableCommands(!ids.empty());
    }
    void resized() override {
        auto b=getLocalBounds().reduced(16);
        for(auto* editor:{&server,&room,&ticket}){editor->setBounds(b.removeFromTop(32));b.removeFromTop(6);}
        auto row=b.removeFromTop(34);join.setBounds(row.removeFromLeft(110));leave.setBounds(row.removeFromLeft(110));
        request.setBounds(row.removeFromLeft(170));permissions.setBounds(row);
        b.removeFromTop(10);tracks.setBounds(b.removeFromTop(32));b.removeFromTop(8);
        row=b.removeFromTop(34);arm.setBounds(row.removeFromLeft(120));record.setBounds(row.removeFromLeft(120));stop.setBounds(row.removeFromLeft(120));
        b.removeFromTop(12);status.setBounds(b);
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff181e28));}
private:
    juce::TextEditor server,room,ticket;juce::ComboBox tracks;juce::Label status;
    juce::TextButton join{"JOIN"},leave{"LEAVE"},request{"REQUEST CONTROL"},permissions{"MY PERMISSIONS"},
        arm{"ARM"},record{"RECORD"},stop{"STOP"};
    std::vector<std::string> ids;
    void command(const std::string& action){const int i=tracks.getSelectedId()-1;
        if(i>=0 && i<static_cast<int>(ids.size()) && onCommand)onCommand(action,ids[static_cast<size_t>(i)]);}
};
