#include "../RemoteHostReviewPanel.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <thread>
// Headless component behavior: initializes JUCE without displaying a window or audio.
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    idw::daw::remote::HostReviewPanel panel;panel.setSize(630,260);
    std::function<juce::TextButton*(juce::Component&,const juce::String&)> findButton;
    findButton=[&](juce::Component& component,const juce::String& caption)->juce::TextButton* {
        if(auto* button=dynamic_cast<juce::TextButton*>(&component))
            if(button->getButtonText()==caption) return button;
        for(int i=0;i<component.getNumChildComponents();++i)
            if(auto* found=findButton(*component.getChildComponent(i),caption)) return found;
        return nullptr;
    };
    unsigned decisions=0;bool approved=false;idw::daw::remote::LocalOfferPrompt last;
    panel.onLocalConsentDecision=[&](const auto& prompt,bool value){++decisions;last=prompt;approved=value;};
    assert(!findButton(panel,"Allow track control"));
    std::function<juce::Label*(juce::Component&)> findDiscovery;
    findDiscovery=[&](juce::Component& component)->juce::Label* {
        if(auto* label=dynamic_cast<juce::Label*>(&component))
            if(label->getText().startsWith("Local bridge:")) return label;
        for(int i=0;i<component.getNumChildComponents();++i)
            if(auto* found=findDiscovery(*component.getChildComponent(i))) return found;
        return nullptr;
    };
    panel.showLocalReadiness(idw::daw::remote::LocalBridgeReady{45,12345});
    auto* discovery=findDiscovery(panel);
    assert(discovery && discovery->isVisible() && discovery->getText().contains("127.0.0.1:12345"));
    assert(discovery->getText().contains("no control") && decisions==0 && !findButton(panel,"Allow track control"));
    panel.showLocalReadiness(idw::daw::remote::LocalBridgeReady{46,23456});
    assert(discovery->getText().contains("127.0.0.1:23456") && !discovery->getText().contains("12345"));
    panel.showLocalReadiness(std::nullopt);
    assert(!discovery->isVisible() && discovery->getText().isEmpty() && decisions==0);
    panel.showLocalReadiness(idw::daw::remote::LocalBridgeReady{0,12345});
    assert(!discovery->isVisible() && !findButton(panel,"Allow track control"));
    idw::daw::remote::LocalOfferPrompt first{7,{45,std::string(64,'a'),std::string(64,'b'),13}};
    panel.showLocalConsent(first);
    auto* allow=findButton(panel,"Allow track control");assert(allow && decisions==0 && allow->isEnabled());
    panel.showLocalReadiness(idw::daw::remote::LocalBridgeReady{45,12345});
    panel.showLocalReadiness(std::nullopt);
    assert(allow->isEnabled() && decisions==0); // Discovery refresh cannot decide consent.
    const auto staleClick=allow->onClick;
    allow->onClick();allow->onClick();
    assert(decisions==1 && approved && last.generation==7 && last.offer.connectionId==first.offer.connectionId);
    assert(!allow->isEnabled() && !allow->getParentComponent()->isVisible());
    auto replacement=first;replacement.generation=8;replacement.offer.connectionId=std::string(64,'c');
    panel.showLocalConsent(replacement);
    staleClick(); // A queued old callback cannot act on replacement controls.
    assert(decisions==1 && findButton(panel,"Allow track control")->isEnabled());
    findButton(panel,"Deny connection")->onClick();
    assert(decisions==2 && !approved && last.generation==8 && last.offer.connectionId==replacement.offer.connectionId);
    panel.showLocalConsent(first);panel.showLocalConsent(std::nullopt);staleClick();
    assert(decisions==2 && !findButton(panel,"Allow track control"));
    using namespace idw::daw::remote;
    using namespace std::chrono_literals;
    const LocalCommandProposal proposal{7,std::string(32,'c'),"guest",{45,std::string(64,'a'),std::string(64,'b'),13},
        "1|45|1|review_request|arm|1"};
    auto pending=[](LocalCommandConsent& consent) {
        const auto until=LocalCommandConsent::Clock::now()+1s;
        while(LocalCommandConsent::Clock::now()<until) {
            if(const auto prompt=consent.pending()) return *prompt;
            std::this_thread::yield();
        }
        assert(false);return LocalCommandPrompt{};
    };
    LocalCommandConsent commandConsent;LocalCommandConsent* liveConsent=&commandConsent;
    std::atomic<bool> cancel{false};bool decided=false,forward=false,resolved=false;
    unsigned commandDecisions=0,finalApprovals=0;
    panel.onCommandConsentDecision=[&](const auto& prompt,bool value){++commandDecisions;resolved=liveConsent->resolve(prompt,value);};
    panel.onHostDecision=[&](const auto&,bool approve){if(approve) ++finalApprovals;};
    panel.showPending({PendingReview{"already_queued",Action::arm_track,0,45}});panel.setApprovalEnabled(true);
    auto* finalButton=findButton(panel,"Approve");assert(finalButton && finalButton->isEnabled());
    const auto queuedFinalClick=finalButton->onClick;
    std::thread commandWorker([&]{decided=commandConsent.request(proposal,forward,cancel,LocalCommandConsent::Clock::now()+2s);});
    const auto commandPrompt=pending(commandConsent);panel.showCommandConsent(commandPrompt);
    auto* forwardButton=findButton(panel,"Allow review request");assert(forwardButton && forwardButton->isEnabled());
    assert(!finalButton->isEnabled() && finalApprovals==0 && commandDecisions==0);
    panel.setApprovalEnabled(false);assert(!forwardButton->isEnabled());
    forwardButton->onClick();assert(commandDecisions==0); // Display cannot bypass disabled live approval state.
    panel.setApprovalEnabled(true);assert(forwardButton->isEnabled());
    queuedFinalClick();assert(finalApprovals==0);
    const auto oldForwardClick=forwardButton->onClick;
    forwardButton->onClick();oldForwardClick();commandWorker.join();
    assert(decided && forward && resolved && commandDecisions==1 && finalApprovals==0);
    panel.showCommandConsent(commandConsent.pending());
    assert(!findButton(panel,"Allow review request") && finalButton->isEnabled());
    LocalCommandConsent replacementCommand;liveConsent=&replacementCommand;decided=false;forward=true;
    std::thread nextCommand([&]{decided=replacementCommand.request(proposal,forward,cancel,LocalCommandConsent::Clock::now()+2s);});
    panel.showCommandConsent(pending(replacementCommand));oldForwardClick();assert(commandDecisions==1);
    panel.showLocalConsent(first);forwardButton=findButton(panel,"Allow review request");
    assert(forwardButton && !forwardButton->isEnabled() && !forwardButton->getParentComponent()->isVisible());
    forwardButton->onClick();assert(commandDecisions==1); // Connection consent takes precedence.
    panel.showLocalConsent(std::nullopt);assert(forwardButton->isEnabled());
    findButton(panel,"Deny review request")->onClick();nextCommand.join();
    assert(decided && !forward && resolved && commandDecisions==2 && finalApprovals==0);
    panel.showCommandConsent(std::nullopt);queuedFinalClick();assert(finalApprovals==1);
    LocalCommandConsent expiredCommand;liveConsent=&expiredCommand;decided=true;forward=true;
    std::thread expiredWorker([&]{decided=expiredCommand.request(proposal,forward,cancel,LocalCommandConsent::Clock::now()+100ms);});
    panel.showCommandConsent(pending(expiredCommand));expiredWorker.join();assert(!decided && forward);
    const auto expiredClick=findButton(panel,"Allow review request")->onClick;
    expiredClick();assert(!resolved && commandDecisions==3 && finalApprovals==1);
    panel.showCommandConsent(expiredCommand.pending());expiredClick();assert(commandDecisions==3);
    // Queued callbacks cannot dereference a destroyed JUCE panel.
    std::function<void()> disposedClick;
    {
        auto temporary=std::make_unique<HostReviewPanel>();temporary->setSize(630,260);
        temporary->setApprovalEnabled(true);
        temporary->onCommandConsentDecision=[&](const auto&,bool){++commandDecisions;};
        temporary->showCommandConsent(commandPrompt);
        disposedClick=findButton(*temporary,"Allow review request")->onClick;
    }
    disposedClick();assert(commandDecisions==3);
    if(argc==3 && std::string(argv[1])=="--snapshot") {
        panel.showPending({});panel.onCommandConsentDecision={};panel.showCommandConsent(commandPrompt);
        panel.setConnectionStatus("Authenticated connection: review request pending");
        auto* snapshotButton=findButton(panel,"Allow review request");
        assert(snapshotButton && snapshotButton->getParentComponent()->isVisible() && snapshotButton->getHeight()>0);
        assert(snapshotButton->getWidth()>0 && panel.getLocalBounds().contains(
            panel.getLocalArea(snapshotButton,snapshotButton->getLocalBounds())));
        juce::FileOutputStream output{juce::File{juce::String{argv[2]}}};
        assert(output.openedOk() && output.setPosition(0) && output.truncate().wasOk());juce::PNGImageFormat format;
        // Deterministic headless rendering; no desktop/GPU surface is required.
        assert(format.writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds(),true,1.0f,juce::SoftwareImageType{}),output));
    } else if(argc!=1) return 2;
    return 0;
}
