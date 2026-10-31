#pragma once
// Experimental JUCE host-review UI. Host must own and refresh on message thread.
// Never connect button callbacks to untrusted network data or execute DAW audio here.
#include <juce_gui_basics/juce_gui_basics.h>
#include "RemoteHostReviewController.h"
#include <functional>
#include <memory>

namespace idw::daw::remote {
class HostReviewPanel final : public juce::Component {
public:
    // Called ONLY by an explicit local button click. Host integration must
    // refresh live grant/track state and use HostReviewController::approve.
    std::function<void(const std::string&, bool approve)> onHostDecision;
    std::function<void()> onRevokeControl;
    std::function<void()> onLoadTestSetup,onChoosePython,onStartLocalTest,onStopLocalTest;

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
        for (auto* button : {&setupButton_,&pythonButton_,&startButton_,&stopButton_})
            addAndMakeVisible(*button);
        setupButton_.onClick=[this] { if (onLoadTestSetup) onLoadTestSetup(); };
        pythonButton_.onClick=[this] { if (onChoosePython) onChoosePython(); };
        startButton_.onClick=[this] { if (onStartLocalTest) onStartLocalTest(); };
        stopButton_.onClick=[this] { if (onStopLocalTest) onStopLocalTest(); };
        addAndMakeVisible(setupSummary_);
        setupSummary_.setText("Local test only: load setup and choose Python",juce::dontSendNotification);
        setTestControls(false,false,false);
        addAndMakeVisible(emptyState_);
        emptyState_.setText("No pending remote requests", juce::dontSendNotification);
    }

    void setConnectionStatus(const juce::String& status) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        status_.setText(status, juce::dontSendNotification);
    }

    void setApprovalEnabled(bool enabled) {
        approvalEnabled_ = enabled;
        for (auto& row : rows_) row->approve.setEnabled(enabled);
    }
    void setTestControls(bool supported,bool configured,bool active) {
        setupButton_.setEnabled(supported && !active);
        pythonButton_.setEnabled(supported && !active);
        startButton_.setEnabled(supported && configured && !active);
        stopButton_.setEnabled(supported && active);
        if (!supported) setupSummary_.setText("Local worker tests require Windows",juce::dontSendNotification);
    }
    void setSetupSummary(const juce::String& text) {
        setupSummary_.setText(text,juce::dontSendNotification);
        setupSummary_.setTooltip(text);
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
            row->approve.setEnabled(approvalEnabled_);
            row->approve.onClick = [this, id] {
                if (approvalEnabled_ && onHostDecision) onHostDecision(id, true);
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
        auto controls=area.removeFromTop(32);
        const auto width=controls.getWidth()/4;
        for (auto* button : {&setupButton_,&pythonButton_,&startButton_,&stopButton_})
            button->setBounds(controls.removeFromLeft(width).reduced(2));
        setupSummary_.setBounds(area.removeFromTop(24));
        emptyState_.setBounds(area.removeFromTop(rows_.empty() ? 30 : 0));
        for (auto& row : rows_) row->setBounds(area.removeFromTop(36));
    }
private:
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
    juce::TextButton setupButton_{"LOAD TEST SETUP"},pythonButton_{"CHOOSE PYTHON"},
        startButton_{"START LOCAL TEST"},stopButton_{"STOP TEST"};
    juce::Label setupSummary_;
    juce::Label header_, status_, emptyState_;
    std::vector<std::unique_ptr<ReviewRow>> rows_;
};
}
