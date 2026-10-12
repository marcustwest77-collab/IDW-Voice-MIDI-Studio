#pragma once
#include <JuceHeader.h>
#include "AIProposal.h"
#include <functional>

// Local approval only. No network endpoint, file-watching or automatic execution.
class IDWAIApprovalPanel final : public juce::Component {
public:
    std::function<bool(const idw::daw::AIProposal&)> onApproved;
    IDWAIApprovalPanel(){
        heading.setText("IDW AI STUDIO  |  REVIEW PROPOSED ACTION",juce::dontSendNotification);
        heading.setColour(juce::Label::textColourId,juce::Colours::gold);
        addAndMakeVisible(heading);
        editor.setMultiLine(true);
        editor.setReturnKeyStartsNewLine(true);
        editor.setText(R"({"format":"idw-ai-proposal-v1","action":{"type":"set_tempo","bpm":140}})");
        addAndMakeVisible(editor);
        paste.setButtonText("PASTE JSON");
        addAndMakeVisible(paste);
        paste.onClick=[this]{
            editor.setText(juce::SystemClipboard::getTextFromClipboard());
            apply.setEnabled(false);
        };
        review.setButtonText("REVIEW PROPOSAL");
        apply.setButtonText("APPROVE & APPLY");
        apply.setEnabled(false);
        for(auto* button:{&review,&apply})addAndMakeVisible(*button);
        feedback.setColour(juce::Label::textColourId,juce::Colours::lightgrey);
        feedback.setText("Paste a supported JSON proposal. No changes occur until approved.",
                         juce::dontSendNotification);
        addAndMakeVisible(feedback);
        editor.onTextChange=[this]{apply.setEnabled(false);feedback.setText(
            "Review the updated proposal before approval.",juce::dontSendNotification);};
        review.onClick=[this]{
            idw::daw::AIProposal parsed{idw::daw::AIProposal::Kind::tempo};
            if(!idw::daw::parseAIProposal(juce::JSON::parse(editor.getText()),parsed)){
                apply.setEnabled(false);
                feedback.setText("Invalid or unsupported AI proposal.",juce::dontSendNotification);
                return;
            }
            pending=parsed;
            const auto description=parsed.kind==idw::daw::AIProposal::Kind::tempo
                ? "Set tempo to "+juce::String(parsed.bpm,1)+" BPM"
                : parsed.kind==idw::daw::AIProposal::Kind::addAudioTrack
                    ? "Add ONE audio track" : "Add ONE MIDI track";
            feedback.setText("REVIEW: "+description,juce::dontSendNotification);
            apply.setEnabled(true);
        };
        apply.onClick=[this]{
            apply.setEnabled(false); // Single use; cannot be executed twice.
            const bool applied=onApproved && onApproved(pending);
            feedback.setText(applied?"Applied to this session.":"Action rejected: stop playback/recording or check track capacity.",
                             juce::dontSendNotification);
        };
    }
    void resized() override{
        auto a=getLocalBounds().reduced(12);
        heading.setBounds(a.removeFromTop(28));
        editor.setBounds(a.removeFromTop(94));
        a.removeFromTop(8);
        auto row=a.removeFromTop(34);
        paste.setBounds(row.removeFromLeft(120).reduced(2));
        review.setBounds(row.removeFromLeft(185).reduced(2));
        apply.setBounds(row.removeFromLeft(170).reduced(2));
        a.removeFromTop(6);
        feedback.setBounds(a.removeFromTop(35));
    }
    void paint(juce::Graphics& g) override{
        g.fillAll(juce::Colour(0xff171e27));g.setColour(juce::Colours::gold);g.drawRect(getLocalBounds(),1);
    }
private:
    idw::daw::AIProposal pending{idw::daw::AIProposal::Kind::tempo};
    juce::Label heading,feedback;
    juce::TextEditor editor;
    juce::TextButton paste,review,apply;
};
