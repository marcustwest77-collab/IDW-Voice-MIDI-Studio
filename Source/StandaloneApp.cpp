#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginProcessor.h"
#include "SettingsMigration.h"

class IDWStandaloneApp final : public juce::JUCEApplication {
public:
    IDWStandaloneApp(){idw::adoptLegacySettings();settings.setStorageParameters(idw::settingsOptions(idw::currentSettingsName));}
    const juce::String getApplicationName() override{return idw::currentSettingsName;}
    const juce::String getApplicationVersion() override{return "9.2.0";}
    bool moreThanOneInstanceAllowed() override{return false;}
    void anotherInstanceStarted(const juce::String&) override{if(window)window->toFront(true);}
    void initialise(const juce::String&) override{
        auto holder=std::make_unique<juce::StandalonePluginHolder>(settings.getUserSettings(),false);
        // JUCE's generic effect host mutes input to prevent feedback. Our processor has
        // an independent monitor switch, so input must remain available for analysis.
        if(auto* processor=dynamic_cast<IDWVoiceMIDIStudioAudioProcessor*>(holder->processor.get())){
            if(auto* monitor=processor->apvts.getParameter("monitorMic"))monitor->setValueNotifyingHost(0.0f);
        }
        holder->getMuteInputValue().setValue(false);
        window=std::make_unique<juce::StandaloneFilterWindow>(getApplicationName(),juce::Colour(0xff0b0e14),std::move(holder));
        window->setVisible(true);
    }
    void shutdown() override{if(window)window->getPluginHolder()->savePluginState();window.reset();settings.saveIfNeeded();}
    void systemRequestedQuit() override{if(juce::ModalComponentManager::getInstance()->cancelAllModalComponents())juce::Timer::callAfterDelay(100,[]{if(auto* app=juce::JUCEApplicationBase::getInstance())app->systemRequestedQuit();});else quit();}
private:
    juce::ApplicationProperties settings;
    std::unique_ptr<juce::StandaloneFilterWindow> window;
};
JUCE_CREATE_APPLICATION_DEFINE(IDWStandaloneApp)
