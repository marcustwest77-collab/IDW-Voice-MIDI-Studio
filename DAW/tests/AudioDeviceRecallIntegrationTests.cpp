#include "AudioDevicePreferences.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static constexpr const char* preferredName="IDW simulated AudioBox";
static constexpr const char* fallbackName="IDW simulated speakers";
static constexpr const char* fallbackType="IDW simulated output";
struct Hardware {bool preferred=true,fallback=true,failPreferred=false;int preferredOpens=0,fallbackOpens=0;};
// These devices never call an OS driver or produce an audio stream. JUCE owns
// their real initialise/setup/selection/callback lifecycle during the test.
class SimulatedDevice final : public juce::AudioIODevice {
public:
    SimulatedDevice(Hardware& h,bool p):AudioIODevice(p?preferredName:fallbackName,p?"ASIO":fallbackType),hardware(h),preferred(p){}
    ~SimulatedDevice() override {close();}
    juce::StringArray getOutputChannelNames() override {return {"Left","Right"};}
    juce::StringArray getInputChannelNames() override {return preferred?juce::StringArray{"Mic 1","Mic 2"}:juce::StringArray{};}
    juce::Array<double> getAvailableSampleRates() override {return preferred?juce::Array<double>{44100,48000}:juce::Array<double>{44100};}
    juce::Array<int> getAvailableBufferSizes() override {return preferred?juce::Array<int>{128,256}:juce::Array<int>{512};}
    int getDefaultBufferSize() override {return preferred?256:512;}
    juce::String open(const juce::BigInteger& in,const juce::BigInteger& out,double sampleRate,int block) override {
        if(preferred)++hardware.preferredOpens;else ++hardware.fallbackOpens;
        if(preferred && hardware.failPreferred)return "Simulated interface open failure";
        if(!getAvailableSampleRates().contains(sampleRate) || !getAvailableBufferSizes().contains(block))return "Unsupported simulated setup";
        inputs=in;outputs=out;rate=sampleRate;buffer=block;opened=true;return {};
    }
    void close() override {stop();opened=false;}
    bool isOpen() override {return opened;}
    void start(juce::AudioIODeviceCallback* next) override {require(opened,"started closed simulated device");callback=next;if(callback)callback->audioDeviceAboutToStart(this);}
    void stop() override {if(callback){auto* previous=callback;callback=nullptr;previous->audioDeviceStopped();}}
    bool isPlaying() override {return callback!=nullptr;}
    juce::String getLastError() override {return {};}
    int getCurrentBufferSizeSamples() override {return buffer;}
    double getCurrentSampleRate() override {return rate;}
    int getCurrentBitDepth() override {return 24;}
    juce::BigInteger getActiveOutputChannels() const override {return outputs;}
    juce::BigInteger getActiveInputChannels() const override {return inputs;}
    int getOutputLatencyInSamples() override {return 0;}
    int getInputLatencyInSamples() override {return 0;}
private:
    Hardware& hardware;bool preferred=false,opened=false;double rate=44100;int buffer=256;
    juce::BigInteger inputs,outputs;juce::AudioIODeviceCallback* callback=nullptr;
};
class SimulatedType final : public juce::AudioIODeviceType {
public:
    SimulatedType(Hardware& h,bool p):AudioIODeviceType(p?"ASIO":fallbackType),hardware(h),preferred(p){}
    void scanForDevices() override {}
    juce::StringArray getDeviceNames(bool input=false) const override {
        if(preferred)return hardware.preferred?juce::StringArray{preferredName}:juce::StringArray{};
        return hardware.fallback && !input?juce::StringArray{fallbackName}:juce::StringArray{};
    }
    int getDefaultDeviceIndex(bool input) const override {return getDeviceNames(input).isEmpty()?-1:0;}
    int getIndexOfDevice(juce::AudioIODevice* device,bool input) const override {return device?getDeviceNames(input).indexOf(device->getName()):-1;}
    bool hasSeparateInputsAndOutputs() const override {return !preferred;}
    juce::AudioIODevice* createDevice(const juce::String& output,const juce::String& input) override {
        if((output.isNotEmpty() && !getDeviceNames(false).contains(output)) ||
           (input.isNotEmpty() && !getDeviceNames(true).contains(input)))return nullptr;
        return new SimulatedDevice(hardware,preferred);
    }
private:
    Hardware& hardware;bool preferred=false;
};
class SimulatedManager final : public juce::AudioDeviceManager {
public:
    explicit SimulatedManager(Hardware& h):hardware(h){}
    void createAudioDeviceTypes(juce::OwnedArray<juce::AudioIODeviceType>& types) override {
        // Deliberately never call the native-device factory implementation.
        types.add(new SimulatedType(hardware,true));types.add(new SimulatedType(hardware,false));
    }
private:
    Hardware& hardware;
};
class Probe final : public juce::AudioIODeviceCallback {
public:
    int starts=0,stops=0;double rate=0;int block=0;juce::BigInteger inputs;
    void audioDeviceAboutToStart(juce::AudioIODevice* d) override {++starts;rate=d->getCurrentSampleRate();block=d->getCurrentBufferSizeSamples();inputs=d->getActiveInputChannels();}
    void audioDeviceStopped() override {++stops;}
    void audioDeviceIOCallbackWithContext(const float* const*,int,float* const*,int,int,const juce::AudioIODeviceCallbackContext&) override {}
};
static juce::MemoryBlock bytes(const juce::File& f){juce::MemoryBlock b;require(f.loadFileAsData(b),"read preference bytes");return b;}
static void preferredSetup(juce::AudioDeviceManager& manager){
    auto* device=manager.getCurrentAudioDevice();require(device && device->getName()==preferredName && manager.getCurrentAudioDeviceType()=="ASIO","preferred interface not restored");
    require(device->getCurrentSampleRate()==48000 && device->getCurrentBufferSizeSamples()==128 &&
        device->getActiveInputChannels().toString(2)=="10" && device->getActiveOutputChannels().toString(2)=="11","restored rate/buffer/masks differ");
}
int main(){try{
    juce::ScopedJuceInitialiser_GUI init;
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-device-recall-integration","",false);
    require(root.createDirectory().wasOk(),"fixture directory");const auto file=root.getChildFile("AudioDeviceSettings.xml");
    AudioDevicePreferences prefs(file);std::unique_ptr<juce::XmlElement> chosen;
    {
        Hardware hardware;SimulatedManager manager(hardware);
        require(manager.initialise(8,2,nullptr,true).isEmpty(),"default startup");
        require(!manager.createStateXml() && prefs.save(nullptr)==AudioDevicePreferences::Result::unchanged && !file.exists(),"default startup persisted an unchosen interface");
        manager.setCurrentAudioDeviceType("ASIO",false);auto setup=manager.getAudioDeviceSetup();
        setup.inputDeviceName=preferredName;setup.outputDeviceName=preferredName;
        setup.sampleRate=48000;setup.bufferSize=128;setup.useDefaultInputChannels=false;setup.useDefaultOutputChannels=false;
        setup.inputChannels.clear();setup.inputChannels.setBit(1);setup.outputChannels.clear();setup.outputChannels.setRange(0,2,true);
        require(manager.setAudioDeviceSetup(setup,true).isEmpty(),"explicit chosen setup");preferredSetup(manager);
        chosen=manager.createStateXml();require(chosen && prefs.save(chosen.get())==AudioDevicePreferences::Result::saved,"chosen setup not persisted");
    }
    const auto original=bytes(file);
    {
        Hardware hardware;SimulatedManager manager(hardware);const auto saved=prefs.load();
        require(manager.initialise(8,2,saved.get(),true).isEmpty(),"saved startup");preferredSetup(manager);
        Probe probe;manager.addAudioCallback(&probe);
        require(probe.starts==1 && probe.rate==48000 && probe.block==128 && probe.inputs.toString(2)=="10","saved setup did not reach callback preparation");
        manager.removeAudioCallback(&probe);require(probe.stops==1,"callback lifecycle");
        auto state=manager.createStateXml();require(state && state->isEquivalentTo(chosen.get(),true),"JUCE changed chosen state on restart");
        require(prefs.save(state.get())==AudioDevicePreferences::Result::unchanged && bytes(file)==original,"restart rewrote preferences");
        manager.closeAudioDevice();state=manager.createStateXml();require(state && prefs.save(state.get())==AudioDevicePreferences::Result::unchanged,"shutdown lost chosen setup");
    }
    {
        Hardware hardware;hardware.preferred=false;SimulatedManager manager(hardware);const auto saved=prefs.load();
        require(manager.initialise(8,2,saved.get(),true).isEmpty(),"missing preferred interface fallback failed");
        const auto* device=manager.getCurrentAudioDevice();require(device && device->getName()==fallbackName && manager.getCurrentAudioDeviceType()==fallbackType,"did not open simulated fallback");
        auto state=manager.createStateXml();require(state && state->isEquivalentTo(chosen.get(),true),"fallback replaced JUCE chosen state");
        require(prefs.save(state.get())==AudioDevicePreferences::Result::unchanged && bytes(file)==original,"fallback overwrote remembered interface");
        require(hardware.preferredOpens==0 && hardware.fallbackOpens>0,"unavailable interface unexpectedly opened");
    }
    {
        Hardware hardware;SimulatedManager manager(hardware);const auto saved=prefs.load();
        require(manager.initialise(8,2,saved.get(),true).isEmpty(),"reconnected startup");preferredSetup(manager);
        // An explicit new user choice must replace the remembered preference.
        manager.setCurrentAudioDeviceType(fallbackType,true);auto state=manager.createStateXml();
        require(state && state->getStringAttribute("deviceType")==fallbackType && prefs.save(state.get())==AudioDevicePreferences::Result::saved,"explicit new choice not saved");
    }
    {
        Hardware hardware;SimulatedManager manager(hardware);const auto saved=prefs.load();
        require(manager.initialise(8,2,saved.get(),true).isEmpty() && manager.getCurrentAudioDevice()->getName()==fallbackName,"new user choice not recalled");
    }
    require(prefs.save(chosen.get())==AudioDevicePreferences::Result::saved,"restore test preference");
    {
        Hardware hardware;hardware.preferred=false;hardware.fallback=false;SimulatedManager manager(hardware);const auto saved=prefs.load();
        manager.initialise(8,2,saved.get(),true);require(manager.getCurrentAudioDevice()==nullptr,"device exists when all simulated hardware absent");
        const auto state=manager.createStateXml();require(state && state->isEquivalentTo(chosen.get(),true) && prefs.save(state.get())==AudioDevicePreferences::Result::unchanged,"no-device startup erased preference");
        require(bytes(file)==original,"no-device startup changed saved bytes");
    }
    {
        Hardware hardware;hardware.failPreferred=true;SimulatedManager manager(hardware);const auto saved=prefs.load();
        manager.initialise(8,2,saved.get(),true);
        require(hardware.preferredOpens>0,"open failure not exercised");
        const auto state=manager.createStateXml();require(state && state->isEquivalentTo(chosen.get(),true) && prefs.save(state.get())==AudioDevicePreferences::Result::unchanged && bytes(file)==original,"open failure erased preference");
    }
    {
        const auto bad=root.getChildFile("corrupt.xml");require(bad.replaceWithText("not device XML"),"corrupt fixture");
        AudioDevicePreferences corrupt(bad);Hardware hardware;SimulatedManager manager(hardware);const auto saved=corrupt.load();
        require(!saved && manager.initialise(8,2,saved.get(),true).isEmpty(),"corrupt-file default startup");
        const auto state=manager.createStateXml();require(!state && corrupt.save(state.get())==AudioDevicePreferences::Result::unchanged && bad.loadFileAsString()=="not device XML","default startup rewrote corrupt preference");
    }
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS real JUCE manager simulated-device recall, callback preparation, missing-interface fallback, reconnect, explicit replacement, no devices, open failure and corrupt-file defaults; no real audio drivers\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
