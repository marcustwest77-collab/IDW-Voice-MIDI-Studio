#pragma once
#include <JuceHeader.h>
#include "AIProposal.h"
#include <functional>

// Local approval only. No network endpoint, file-watching or automatic execution.
class IDWAIApprovalPanel final : public juce::Component {
public:
    std::function<bool(const idw::daw::AIProposal&)> onApproved;
    std::function<juce::String()> getSanitizedStatus;
    IDWAIApprovalPanel(){
        heading.setText("IDW AI STUDIO  |  REVIEW PROPOSED ACTION",juce::dontSendNotification);
        heading.setColour(juce::Label::textColourId,juce::Colours::gold);
        addAndMakeVisible(heading);
        editor.setMultiLine(true);
        editor.setReturnKeyStartsNewLine(true);
        editor.setText(R"({"format":"idw-ai-proposal-v1","action":{"type":"set_tempo","bpm":140}})");
        addAndMakeVisible(editor);
        copyStatus.setButtonText("COPY STATUS");
        addAndMakeVisible(copyStatus);
        copyStatus.onClick=[this]{
            if(!getSanitizedStatus)return;
            const auto status=getSanitizedStatus();
            if(status.isEmpty())return;
            // Require deliberate confirmation before putting project metadata
            // on the clipboard, where other applications may read it.
            juce::AlertWindow::showOkCancelBox(
                juce::MessageBoxIconType::QuestionIcon,
                "Share IDW session metadata?",
                "Copy track names, tempo and mixer settings to your clipboard? "
                "No audio content or file paths are included. Other apps may read clipboard data.",
                "COPY STATUS","CANCEL",this,
                juce::ModalCallbackFunction::create([this,status](int result){
                    if(result!=1)return;
                    juce::SystemClipboard::copyTextToClipboard(status);
                    feedback.setText("Sanitized session metadata copied.",juce::dontSendNotification);
                }));
        };
        paste.setButtonText("PASTE JSON");
        addAndMakeVisible(paste);
        paste.onClick=[this]{
            const auto incoming=juce::SystemClipboard::getTextFromClipboard();
            if(incoming.length()>4096){
                apply.setEnabled(false);
                feedback.setText("Clipboard proposal exceeds 4 KB limit.",juce::dontSendNotification);
                return;
            }
            editor.setText(incoming);
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
            if(editor.getText().length()>4096){
                apply.setEnabled(false);
                feedback.setText("Proposal exceeds 4 KB limit.",juce::dontSendNotification);
                return;
            }
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
                    ? "Add ONE audio track"
                    : parsed.kind==idw::daw::AIProposal::Kind::addMidiTrack
                        ? "Add ONE MIDI track"
                        : "Set track "+juce::String((int)parsed.trackIndex+1)+" volume to "+juce::String(parsed.gain,2)+" and pan to "+juce::String(parsed.pan,2);
            feedback.setText("REVIEW: "+description,juce::dontSendNotification);
            apply.setEnabled(true);
        };
        apply.onClick=[this]{
            apply.setEnabled(false); // Single use; cannot be executed twice.
            // Reparse the exact text at approval time: never execute a stale
            // proposal if the editor was changed after review.
            idw::daw::AIProposal current{idw::daw::AIProposal::Kind::tempo};
            if(!idw::daw::parseAIProposal(juce::JSON::parse(editor.getText()),current)
               || current.kind!=pending.kind || current.bpm!=pending.bpm
               || current.trackIndex!=pending.trackIndex
               || current.gain!=pending.gain || current.pan!=pending.pan){
                feedback.setText("Proposal changed: review again.",juce::dontSendNotification);
                return;
            }
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
        copyStatus.setBounds(row.removeFromLeft(100).reduced(2));
        paste.setBounds(row.removeFromLeft(108).reduced(2));
        review.setBounds(row.removeFromLeft(165).reduced(2));
        apply.setBounds(row.removeFromLeft(150).reduced(2));
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
    juce::TextButton copyStatus,paste,review,apply;
};
