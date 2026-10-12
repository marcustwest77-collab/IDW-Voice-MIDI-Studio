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
        saveStatus.setButtonText("SAVE STATUS");
        addAndMakeVisible(saveStatus);
        saveStatus.onClick=[this]{
            if(!getSanitizedStatus)return;
            const auto snapshot=getSanitizedStatus();
            juce::AlertWindow::showOkCancelBox(
                juce::MessageBoxIconType::QuestionIcon,
                "Export IDW status snapshot?",
                "Save track names, tempo and mixer settings as JSON? "
                "No audio file paths are included. Only share this file with trusted AI clients.",
                "SAVE","CANCEL",this,
                juce::ModalCallbackFunction::create([this,snapshot](int result){
                    if(result!=1)return;
                    statusChooser=std::make_unique<juce::FileChooser>(
                        "Save sanitized IDW status",juce::File{}.getChildFile("IDW-Status.json"),"*.json");
                    statusChooser->launchAsync(
                        juce::FileBrowserComponent::saveMode |
                        juce::FileBrowserComponent::canSelectFiles,
                        [this,snapshot](const juce::FileChooser& selected){
                            const auto file=selected.getResult();
                            if(file.getFullPathName().isEmpty())return;
                            const bool ok=file.replaceWithText(snapshot);
                            feedback.setText(ok?"Sanitized status saved.":"Status export failed.",
                                             juce::dontSendNotification);
                        });
                }));
        };
        inboxPath.setText("AI inbox path",juce::dontSendNotification);
        inboxPath.setTooltip("Choose your configured local MCP proposal inbox once.");
        addAndMakeVisible(inboxPath);
        chooseInbox.setButtonText("SET INBOX");
        addAndMakeVisible(chooseInbox);
        chooseInbox.onClick=[this]{
            inboxChooser=std::make_unique<juce::FileChooser>(
                "Choose the local MCP proposal inbox",juce::File{},"*.json");
            inboxChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                       juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& picked){
                    const auto file=picked.getResult();
                    if(file.existsAsFile() && file.hasFileExtension("json")){
                        inboxFile=file;
                        inboxPath.setText(file.getFileName(),juce::dontSendNotification);
                    }
                });
        };
        checkInbox.setButtonText("CHECK INBOX");
        addAndMakeVisible(checkInbox);
        checkInbox.onClick=[this]{
            if(!inboxFile.existsAsFile()){
                feedback.setText("Choose an existing inbox JSON file first.",juce::dontSendNotification);
                return;
            }
            if(inboxFile.getSize()>4096){
                feedback.setText("Inbox proposal exceeds 4 KB limit.",juce::dontSendNotification);
                return;
            }
            const auto contents=inboxFile.loadFileAsString();
            idw::daw::AIProposal candidate{idw::daw::AIProposal::Kind::tempo};
            if(!idw::daw::parseAIProposal(juce::JSON::parse(contents),candidate)){
                feedback.setText("Inbox contains an invalid proposal.",juce::dontSendNotification);
                return;
            }
            editor.setText(contents);
            apply.setEnabled(false);
            feedback.setText("Pending inbox proposal loaded. Review before applying.",
                             juce::dontSendNotification);
        };
        // Import a proposal from a local JSON file chosen by the user. No polling
        // or background filesystem access: the user explicitly selects the file.
        importFile.setButtonText("IMPORT JSON");
        addAndMakeVisible(importFile);
        importFile.onClick=[this]{
            chooser=std::make_unique<juce::FileChooser>(
                "Choose an IDW AI proposal",juce::File{},"*.json");
            chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& selected){
                    const auto file=selected.getResult();
                    if(!file.existsAsFile())return;
                    if(file.getSize()>4096){
                        apply.setEnabled(false);
                        feedback.setText("Proposal file exceeds 4 KB limit.",juce::dontSendNotification);
                        return;
                    }
                    editor.setText(file.loadFileAsString());
                    apply.setEnabled(false);
                });
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
            if(editor.getText().length()>4096){
                feedback.setText("Proposal exceeds 4 KB limit.",juce::dontSendNotification);
                return;
            }
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
        auto inboxRow=a.removeFromTop(28);
        inboxPath.setBounds(inboxRow.removeFromLeft(160).reduced(2));
        chooseInbox.setBounds(inboxRow.removeFromLeft(110).reduced(2));
        checkInbox.setBounds(inboxRow.removeFromLeft(125).reduced(2));
        auto row=a.removeFromTop(34);
        saveStatus.setBounds(row.removeFromLeft(100).reduced(2));
        importFile.setBounds(row.removeFromLeft(96).reduced(2));
        copyStatus.setBounds(row.removeFromLeft(90).reduced(2));
        paste.setBounds(row.removeFromLeft(88).reduced(2));
        review.setBounds(row.removeFromLeft(113).reduced(2));
        apply.setBounds(row.removeFromLeft(72).reduced(2));
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
    juce::TextButton saveStatus,importFile,copyStatus,paste,review,apply,chooseInbox,checkInbox;
    juce::Label inboxPath;
    juce::File inboxFile;
    std::unique_ptr<juce::FileChooser> chooser,statusChooser,inboxChooser;
};
