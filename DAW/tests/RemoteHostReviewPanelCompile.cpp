#include "../RemoteHostReviewPanel.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
// Headless component behavior: initializes JUCE without displaying a window or audio.
int main() {
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
    idw::daw::remote::LocalOfferPrompt first{7,{45,std::string(64,'a'),std::string(64,'b'),13}};
    panel.showLocalConsent(first);
    auto* allow=findButton(panel,"Allow track control");assert(allow && decisions==0 && allow->isEnabled());
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
    return 0;
}
