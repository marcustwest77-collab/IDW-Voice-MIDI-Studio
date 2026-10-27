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

    HostReviewPanel() { addAndMakeVisible(header_); header_.setText(
        "Remote Studio - Host Approval", juce::dontSendNotification); }

    void showPending(const std::vector<PendingReview>& pending) {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        rows_.clear();
        for (const auto& item : pending) {
            auto row = std::make_unique<ReviewRow>();
            const std::string id = item.requestId;
            row->caption.setText(
                juce::String(id) + " | action " + juce::String(static_cast<int>(item.action))
                + " | track " + juce::String(static_cast<int>(item.trackIndex + 1)),
                juce::dontSendNotification);
            row->approve.onClick = [this, id] { if (onHostDecision) onHostDecision(id, true); };
            row->deny.onClick = [this, id] { if (onHostDecision) onHostDecision(id, false); };
            addAndMakeVisible(*row);
            rows_.push_back(std::move(row));
        }
        resized();
        repaint();
    }

    void resized() override {
        auto area = getLocalBounds().reduced(8);
        header_.setBounds(area.removeFromTop(30));
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
    juce::Label header_;
    std::vector<std::unique_ptr<ReviewRow>> rows_;
};
}
