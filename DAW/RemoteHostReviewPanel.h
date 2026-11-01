#pragma once
// Experimental JUCE host-review UI. Host must own and refresh on message thread.
// Never connect button callbacks to untrusted network data or execute DAW audio here.
#include <juce_gui_basics/juce_gui_basics.h>
#include "RemoteHostReviewController.h"
#include "RemoteLocalOfferConsent.h"
#include "RemoteBridgeReadyWire.h"
#include "RemoteLocalCommandConsent.h"
#include <functional>
#include <memory>

namespace idw::daw::remote {
class HostReviewPanel final : public juce::Component {
public:
    // Called ONLY by an explicit local button click. Host integration must
    // refresh live grant/track state and use HostReviewController::approve.
    std::function<void(const std::string&, bool approve)> onHostDecision;
    std::function<void()> onRevokeControl;
    std::function<void(const LocalOfferPrompt&,bool)> onLocalConsentDecision;
    std::function<void(const LocalCommandPrompt&,bool)> onCommandConsentDecision;

    static const char* actionName(Action action) noexcept {
        switch (action) {
            case Action::select_track: return "Select track";
            case Action::arm_track: return "Arm track";
            case Action::start_recording: return "Start recording";
            case Action::stop_recording: return "Stop recording";
            case Action::set_gain: return "Set gain";
            case Action::set_pan: return "Set pan";
            case Action::delete_track: return "Delete track";
            case Action::export_audio: return "Export audio";
            case Action::view: return "View";
        }
        return "Unknown command";
    }
    HostReviewPanel() {
        addAndMakeVisible(header_);
        header_.setText("Remote Studio - Host Approval", juce::dontSendNotification);
        addAndMakeVisible(status_);
        status_.setText("Offline: remote approvals unavailable", juce::dontSendNotification);
        addChildComponent(discovery_);
        addAndMakeVisible(revokeButton_);
        revokeButton_.setButtonText("REVOKE CONTROL");
        revokeButton_.onClick=[this] { if (onRevokeControl) onRevokeControl(); };
        addAndMakeVisible(emptyState_);
        emptyState_.setText("No pending remote requests", juce::dontSendNotification);
    }

    void showLocalConsent(std::optional<LocalOfferPrompt> prompt) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        if(prompt && consentRow_ && samePrompt(consentRow_->prompt,*prompt)) return;
        if(consentRow_) removeChildComponent(consentRow_.get());
        consentRow_.reset();
        if(prompt) {
            auto row=std::make_unique<LocalConsentRow>(*prompt);
            const auto reviewed=*prompt; // Immutable displayed snapshot, not a later poll.
            row->allow.onClick=[this,reviewed]{decideLocalConsent(reviewed,true);};
            row->deny.onClick=[this,reviewed]{decideLocalConsent(reviewed,false);};
            addAndMakeVisible(*row);consentRow_=std::move(row);
        }
        refreshConsentState();
        resized();repaint();
    }

    // Owner supplies only a live process-verified mailbox snapshot and polls
    // again after expiry/loss. UI display itself does not establish authority.
    void showCommandConsent(std::optional<LocalCommandPrompt> prompt) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        CommandReviewBytes checked;
        if(prompt && (!prompt->owner || !encodeCommandProposal(prompt->proposal,checked))) return;
        if(!prompt && !commandConsentRow_) return;
        if(prompt && commandConsentRow_ && sameCommandPrompt(commandConsentRow_->prompt,*prompt)) return;
        if(commandConsentRow_) removeChildComponent(commandConsentRow_.get());
        commandConsentRow_.reset();
        if(prompt) {
            auto row=std::make_unique<CommandConsentRow>(*prompt);
            const auto reviewed=*prompt;
            juce::Component::SafePointer<HostReviewPanel> safe(this);
            row->allow.onClick=[safe,reviewed]{if(safe) safe->decideCommandConsent(reviewed,true);};
            row->deny.onClick=[safe,reviewed]{if(safe) safe->decideCommandConsent(reviewed,false);};
            addAndMakeVisible(*row);commandConsentRow_=std::move(row);
        }
        refreshConsentState();resized();repaint();
    }

    // Trusted owner polls live controller state; this label grants no consent.
    void showLocalReadiness(std::optional<LocalBridgeReady> ready) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        const bool visible=ready && ready->epoch!=0 && ready->tlsPort!=0;
        const auto text=visible ? "Local bridge: 127.0.0.1:"+juce::String(ready->tlsPort)+
            "\nDiscovery grants no control; TLS and host consent required." : juce::String{};
        if(discovery_.isVisible()==visible && discovery_.getText()==text) return;
        discovery_.setText(text,juce::dontSendNotification);
        discovery_.setVisible(visible);resized();repaint();
    }

    void setConnectionStatus(const juce::String& status) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        status_.setText(status, juce::dontSendNotification);
    }

    void setApprovalEnabled(bool enabled) {
        approvalEnabled_ = enabled;
        refreshConsentState();
    }

    void showPending(const std::vector<PendingReview>& pending) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        // Remove old child components before destroying their owning rows.
        for (auto& row : rows_) removeChildComponent(row.get());
        rows_.clear();
        for (const auto& item : pending) {
            auto row = std::make_unique<ReviewRow>();
            const std::string id = item.requestId;
            row->caption.setText(
                juce::String(id) + " | " + juce::String(actionName(item.action))
                + " | track " + juce::String(static_cast<int>(item.trackIndex + 1)),
                juce::dontSendNotification);
            row->approve.setEnabled(approvalEnabled_ && !consentRow_ && !commandConsentRow_);
            row->approve.onClick = [this, id] {
                if (approvalEnabled_ && !consentRow_ && !commandConsentRow_ && onHostDecision) onHostDecision(id, true);
            };
            row->deny.onClick = [this, id] { if (onHostDecision) onHostDecision(id, false); };
            addAndMakeVisible(*row);
            rows_.push_back(std::move(row));
        }
        refreshConsentState();
        resized();
        repaint();
    }

    void resized() override {
        auto area = getLocalBounds().reduced(8);
        auto headerArea=area.removeFromTop(30);
        revokeButton_.setBounds(headerArea.removeFromRight(150).reduced(2));
        header_.setBounds(headerArea);
        status_.setBounds(area.removeFromTop(24));
        discovery_.setBounds(area.removeFromTop(discovery_.isVisible() ? 42 : 0));
        if(consentRow_) consentRow_->setBounds(area.removeFromTop(97));
        if(commandConsentRow_) commandConsentRow_->setBounds(area.removeFromTop(consentRow_ ? 0 : 117));
        emptyState_.setBounds(area.removeFromTop(emptyState_.isVisible() ? 30 : 0));
        for (auto& row : rows_) row->setBounds(area.removeFromTop(36));
    }
private:
    void refreshConsentState() {
        for(auto& row:rows_) row->approve.setEnabled(approvalEnabled_ && !consentRow_ && !commandConsentRow_);
        if(commandConsentRow_) {
            commandConsentRow_->setVisible(!consentRow_ && !commandConsentRow_->decided);
            commandConsentRow_->allow.setEnabled(approvalEnabled_ && !consentRow_ && !commandConsentRow_->decided);
        }
        emptyState_.setVisible(rows_.empty() && !consentRow_ && !commandConsentRow_);
    }
    static bool sameCommandPrompt(const LocalCommandPrompt& a,const LocalCommandPrompt& b) {
        CommandReviewBytes first,second;
        return a.owner==b.owner && encodeCommandProposal(a.proposal,first) &&
            encodeCommandProposal(b.proposal,second) && first==second;
    }
    void decideCommandConsent(const LocalCommandPrompt& reviewed,bool allow) {
        if(!commandConsentRow_ || commandConsentRow_->decided || (allow && (!approvalEnabled_ || consentRow_)) ||
           !sameCommandPrompt(commandConsentRow_->prompt,reviewed) || !onCommandConsentDecision) return;
        const auto callback=onCommandConsentDecision;
        commandConsentRow_->decided=true;
        commandConsentRow_->allow.setEnabled(false);commandConsentRow_->deny.setEnabled(false);
        commandConsentRow_->setVisible(false);
        callback(reviewed,allow); // Callback resolves the exact live mailbox, not a later poll.
    }
    struct CommandConsentRow final : juce::Component {
        const LocalCommandPrompt prompt;
        bool decided=false;
        juce::Label info;
        juce::TextButton allow{"Allow review request"},deny{"Deny review request"};
        explicit CommandConsentRow(const LocalCommandPrompt& value):prompt(value) {
            addAndMakeVisible(info);addAndMakeVisible(allow);addAndMakeVisible(deny);
            CommandMessage command;parseControlCommand(prompt.proposal.wire,command);
            info.setFont(juce::Font(juce::FontOptions(12.0f)));
            info.setText("Forward "+juce::String(actionName(command.action))+" for track "+juce::String(static_cast<int>(command.trackIndex+1))+
                " to host review?\nRequest: "+juce::String(command.requestId)+"\nPeer: "+juce::String(prompt.proposal.peerId)+
                "\nClient SHA-256: "+juce::String(prompt.proposal.offer.peerSha256)+
                "\nForwarding does not execute the command.",juce::dontSendNotification);
        }
        void resized() override {
            auto bounds=getLocalBounds();info.setBounds(bounds.removeFromTop(85));
            allow.setBounds(bounds.removeFromLeft(180).reduced(2));
            deny.setBounds(bounds.removeFromLeft(180).reduced(2));
        }
    };
    static bool samePrompt(const LocalOfferPrompt& a,const LocalOfferPrompt& b) {
        return a.generation==b.generation && a.offer.epoch==b.offer.epoch &&
            a.offer.peerSha256==b.offer.peerSha256 && a.offer.connectionId==b.offer.connectionId &&
            a.offer.tlsVersion==b.offer.tlsVersion;
    }
    void decideLocalConsent(const LocalOfferPrompt& reviewed,bool allow) {
        if(!consentRow_ || consentRow_->decided || !samePrompt(consentRow_->prompt,reviewed) ||
           !onLocalConsentDecision) return;
        const auto snapshot=reviewed;
        const auto callback=onLocalConsentDecision;
        consentRow_->decided=true;
        consentRow_->allow.setEnabled(false);consentRow_->deny.setEnabled(false);
        consentRow_->setVisible(false); // Dispose on the next poll, after this click returns.
        callback(snapshot,allow);
    }
    struct LocalConsentRow final : juce::Component {
        const LocalOfferPrompt prompt;
        bool decided=false;
        juce::Label info;
        juce::TextButton allow{"Allow track control"},deny{"Deny connection"};
        explicit LocalConsentRow(const LocalOfferPrompt& value):prompt(value) {
            addAndMakeVisible(info);addAndMakeVisible(allow);addAndMakeVisible(deny);
            info.setFont(juce::Font(juce::FontOptions(12.0f)));
            info.setText("Verified client certificate SHA-256:\n"+juce::String(prompt.offer.peerSha256)+
                "\nAllow track selection and arming? Each command still needs your approval.",juce::dontSendNotification);
        }
        void resized() override {
            auto bounds=getLocalBounds();info.setBounds(bounds.removeFromTop(65));
            allow.setBounds(bounds.removeFromLeft(180).reduced(2));
            deny.setBounds(bounds.removeFromLeft(150).reduced(2));
        }
    };
    struct ReviewRow final : juce::Component {
        juce::Label caption;
        juce::TextButton approve{"Approve"}, deny{"Deny"};
        ReviewRow() {
            addAndMakeVisible(caption);
            addAndMakeVisible(approve);
            addAndMakeVisible(deny);
        }
        void resized() override {
            auto bounds = getLocalBounds();
            deny.setBounds(bounds.removeFromRight(75).reduced(2));
            approve.setBounds(bounds.removeFromRight(85).reduced(2));
            caption.setBounds(bounds);
        }
    };
    bool approvalEnabled_ = false; // deny by default until live authenticated DAW integration
    juce::TextButton revokeButton_;
    std::unique_ptr<LocalConsentRow> consentRow_;
    std::unique_ptr<CommandConsentRow> commandConsentRow_;
    juce::Label header_, status_, emptyState_, discovery_;
    std::vector<std::unique_ptr<ReviewRow>> rows_;
};
}
