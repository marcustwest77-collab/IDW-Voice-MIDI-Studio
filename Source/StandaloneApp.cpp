#include <JuceHeader.h>
#include <BinaryData.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginProcessor.h"
class IDWStandaloneApp final : public juce::JUCEApplication {
public:
    IDWStandaloneApp(){juce::PropertiesFile::Options options;options.applicationName="IDW Voice MIDI Studio V4";options.filenameSuffix=".settings";options.osxLibrarySubFolder="Application Support";settings.setStorageParameters(options);}
    const juce::String getApplicationName() override{return "IDW Voice MIDI Studio V11 Preview";}
    const juce::String getApplicationVersion() override{return "11.0.0";}
    bool moreThanOneInstanceAllowed() override{return false;}
    void anotherInstanceStarted(const juce::String&) override{if(window)window->toFront(true);}
    void initialise(const juce::String&) override{
        const auto logo=juce::ImageFileFormat::loadFrom(BinaryData::IDWLogoFull_png,BinaryData::IDWLogoFull_pngSize);
        if(logo.isValid()){
            splash=std::make_unique<juce::SplashScreen>("IDW Voice MIDI Studio",logo,true);
            juce::Timer::callAfterDelay(1400,[this]{splash.reset();});
        }
        auto holder=std::make_unique<juce::StandalonePluginHolder>(settings.getUserSettings(),false);
        // JUCE's generic effect host mutes input to prevent feedback. Our processor has
        // an independent monitor switch, so input must remain available for analysis.
        if(auto* processor=dynamic_cast<IDWVoiceMIDIStudioAudioProcessor*>(holder->processor.get())){
            if(auto* monitor=processor->apvts.getParameter("monitorMic"))monitor->setValueNotifyingHost(0.0f);
        }
        holder->getMuteInputValue().setValue(false);
        window=std::make_unique<juce::StandaloneFilterWindow>(getApplicationName(),juce::Colour(0xff0b0e14),std::move(holder));
        window->setVisible(true);
        if(splash)splash->toFront(false);
    }
    void shutdown() override{if(window)window->getPluginHolder()->savePluginState();window.reset();settings.saveIfNeeded();}
    void systemRequestedQuit() override{if(juce::ModalComponentManager::getInstance()->cancelAllModalComponents())juce::Timer::callAfterDelay(100,[]{if(auto* app=juce::JUCEApplicationBase::getInstance())app->systemRequestedQuit();});else quit();}
private:
    juce::ApplicationProperties settings;
    std::unique_ptr<juce::SplashScreen> splash;
    std::unique_ptr<juce::StandaloneFilterWindow> window;
};
JUCE_CREATE_APPLICATION_DEFINE(IDWStandaloneApp)
