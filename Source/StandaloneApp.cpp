#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginProcessor.h"
namespace {
constexpr const char* currentSettingsName = "IDW Voice MIDI Studio V9.2";
constexpr const char* legacySettingsName = "IDW Voice MIDI Studio V4";

juce::PropertiesFile::Options settingsOptions(const juce::String& applicationName)
{
    juce::PropertiesFile::Options options;
    options.applicationName = applicationName;
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

// The application name is part of the settings file's path, so renaming it would
// start the updated build with no audio or MIDI device setup at all. Carry the
// previous file across once, before anything reads it.
void adoptLegacySettings()
{
    const auto current = settingsOptions(currentSettingsName).getDefaultFile();
    const auto legacy = settingsOptions(legacySettingsName).getDefaultFile();
    if (current.existsAsFile() || !legacy.existsAsFile())
        return;
    // On Windows the app name is also the folder the file lives in, so that
    // folder has to exist before the copy can be written into it.
    current.getParentDirectory().createDirectory();
    legacy.copyFileTo(current);
}
}

class IDWStandaloneApp final : public juce::JUCEApplication {
public:
    IDWStandaloneApp(){adoptLegacySettings();settings.setStorageParameters(settingsOptions(currentSettingsName));}
    const juce::String getApplicationName() override{return currentSettingsName;}
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
