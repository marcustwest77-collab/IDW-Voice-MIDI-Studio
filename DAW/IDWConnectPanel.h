#pragma once
#include <JuceHeader.h>

// IDW Connect V14 prototype: invitation UI for a two-person video session.
// Calls run in the user's browser via a third-party video service.
// This is NOT synchronized DAW collaboration and does not enforce attendance limits.
class IDWConnectPanel final : public juce::Component {
public:
    IDWConnectPanel() {
        heading.setText("IDW CONNECT  |  2-PERSON STUDIO CALL",juce::dontSendNotification);
        heading.setColour(juce::Label::textColourId,juce::Colours::gold);
        addAndMakeVisible(heading);
        room.setTextToShowWhenEmpty("Paste an IDW room code or meeting URL",juce::Colours::grey);
        addAndMakeVisible(room);
        create.setButtonText("CREATE PRIVATE CODE");
        join.setButtonText("OPEN VIDEO ROOM");
        invite.setButtonText("COPY INVITE");
        for(auto* button:{&create,&join,&invite})addAndMakeVisible(*button);
        notice.setText("Video opens in your browser using Jitsi Meet (third party). "
                       "Room codes are invitations, not passwords. Two people intended; "
                       "participant limit is not technically enforced. "
                       "DAW audio and projects are NOT shared.",
                       juce::dontSendNotification);
        notice.setJustificationType(juce::Justification::topLeft);
        notice.setColour(juce::Label::textColourId,juce::Colours::lightgrey);
        addAndMakeVisible(notice);
        create.onClick=[this] {
            // 128 bits of system-generated randomness make guessing rooms impractical.
            auto& random=juce::Random::getSystemRandom();
            juce::String suffix;
            for(int n=0;n<4;++n)
                suffix+=juce::String::toHexString((int)random.nextInt()).paddedLeft('0',8);
            room.setText("IDW-"+suffix,false);
            invite.setEnabled(true);
        };
        join.onClick=[this] {
            auto code=room.getText().trim();
            if(code.startsWithIgnoreCase("https://meet.jit.si/"))
                code=code.fromFirstOccurrenceOf("https://meet.jit.si/",false,false);
            if(!isValidCode(code)) {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Invalid IDW room",
                    "Enter a generated IDW room code (IDW- followed by 32 hexadecimal digits).");
                return;
            }
            juce::URL("https://meet.jit.si/"+code).launchInDefaultBrowser();
        };
        invite.onClick=[this] {
            const auto code=room.getText().trim();
            if(!isValidCode(code))return;
            juce::SystemClipboard::copyTextToClipboard(
                "Join my IDW Connect two-person video session: https://meet.jit.si/"+code+
                "\nThis uses Jitsi Meet; do not share the link publicly.");
        };
        invite.setEnabled(false);
        room.onTextChange=[this]{invite.setEnabled(isValidCode(room.getText().trim()));};
    }

    void resized() override {
        auto bounds=getLocalBounds().reduced(14);
        heading.setBounds(bounds.removeFromTop(30));
        room.setBounds(bounds.removeFromTop(30));
        bounds.removeFromTop(8);
        auto actions=bounds.removeFromTop(34);
        create.setBounds(actions.removeFromLeft(180).reduced(2));
        join.setBounds(actions.removeFromLeft(165).reduced(2));
        invite.setBounds(actions.removeFromLeft(130).reduced(2));
        bounds.removeFromTop(10);
        notice.setBounds(bounds.removeFromTop(80));
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff181e28));
        g.setColour(juce::Colours::gold);
        g.drawRect(getLocalBounds(),1);
    }
private:
    static bool isValidCode(const juce::String& code) {
        if(code.length()!=36 || !code.startsWith("IDW-"))return false;
        for(int i=4;i<code.length();++i)
            if(!juce::CharacterFunctions::isHexDigit(code[i]))return false;
        return true;
    }
    juce::Label heading,notice;
    juce::TextEditor room;
    juce::TextButton create,join,invite;
};
