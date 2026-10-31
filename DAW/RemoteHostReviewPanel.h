#pragma once
// Experimental JUCE host-review UI. Host must own and refresh on message thread.
// Never connect button callbacks to untrusted network data or execute DAW audio here.
#include <juce_gui_basics/juce_gui_basics.h>
#include "RemoteHostReviewController.h"
#include "RemoteLocalOfferConsent.h"
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
        for(auto& row:rows_) row->approve.setEnabled(approvalEnabled_ && !consentRow_);
        resized();repaint();
    }

    void setConnectionStatus(const juce::String& status) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        status_.setText(status, juce::dontSendNotification);
    }

    void setApprovalEnabled(bool enabled) {
        approvalEnabled_ = enabled;
        for (auto& row : rows_) row->approve.setEnabled(enabled && !consentRow_);
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
            row->approve.setEnabled(approvalEnabled_ && !consentRow_);
            row->approve.onClick = [this, id] {
                if (approvalEnabled_ && !consentRow_ && onHostDecision) onHostDecision(id, true);
            };
            row->deny.onClick = [this, id] { if (onHostDecision) onHostDecision(id, false); };
            addAndMakeVisible(*row);
            rows_.push_back(std::move(row));
        }
        emptyState_.setVisible(rows_.empty());
        resized();
        repaint();
    }

    void resized() override {
        auto area = getLocalBounds().reduced(8);
        auto headerArea=area.removeFromTop(30);
        revokeButton_.setBounds(headerArea.removeFromRight(150).reduced(2));
        header_.setBounds(headerArea);
        status_.setBounds(area.removeFromTop(24));
        if(consentRow_) consentRow_->setBounds(area.removeFromTop(97));
        emptyState_.setBounds(area.removeFromTop(rows_.empty() ? 30 : 0));
        for (auto& row : rows_) row->setBounds(area.removeFromTop(36));
    }
private:
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
    juce::Label header_, status_, emptyState_;
    std::vector<std::unique_ptr<ReviewRow>> rows_;
};
}
