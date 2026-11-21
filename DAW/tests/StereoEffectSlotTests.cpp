#include "StereoEffectSlot.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Counts {int prepared=0,released=0,processed=0,destroyed=0;};
class Effect final:public juce::AudioProcessor {
public:
    Effect(Counts& c,bool bad=false):AudioProcessor(BusesProperties().withInput("In",juce::AudioChannelSet::stereo()).withOutput("Out",juce::AudioChannelSet::stereo())),counts(c),invalid(bad){}
    ~Effect() override {++counts.destroyed;}
    const juce::String getName() const override{return "Synthetic gain";}
    void prepareToPlay(double,int) override{++counts.prepared;setLatencySamples(16);}
    void releaseResources() override{++counts.released;}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override{++counts.processed;b.applyGain(volume);if(invalid)b.setSample(0,0,std::numeric_limits<float>::quiet_NaN());}
    bool isBusesLayoutSupported(const BusesLayout& b) const override{return b.getMainInputChannelSet()==juce::AudioChannelSet::stereo() && b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    double getTailLengthSeconds() const override{return 0;}
    bool acceptsMidi() const override{return false;}
    bool producesMidi() const override{return false;}
    bool hasEditor() const override{return false;}
    juce::AudioProcessorEditor* createEditor() override{return nullptr;}
    int getNumPrograms() override{return 1;}
    int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{}
    const juce::String getProgramName(int) override{return {};}
    void changeProgramName(int,const juce::String&) override{}
    void getStateInformation(juce::MemoryBlock& state) override{state.replaceWith(&volume,sizeof(volume));}
    void setStateInformation(const void* data,int bytes) override{if(bytes!=(int)sizeof(volume))throw std::runtime_error("bad synthetic state");std::memcpy(&volume,data,sizeof(volume));}
private:Counts& counts;bool invalid;float volume=0.25f;
};
static void fill(juce::AudioBuffer<float>& b){for(int ch=0;ch<b.getNumChannels();++ch)for(int i=0;i<b.getNumSamples();++i)b.setSample(ch,i,0.8f);}
int main(){try{
    Counts counts,badCounts;juce::String error;StereoEffectSlot slot;
    juce::AudioBuffer<float> audio(2,64);fill(audio);
    require(slot.process(audio) && audio.getSample(0,0)==0.8f,"empty slot not dry");
    require(!slot.install(std::make_unique<Effect>(badCounts),0,64,error) && !slot.hasEffect(),"invalid rate accepted");
    require(slot.install(std::make_unique<Effect>(counts),44100,64,error) && counts.prepared==1 && slot.latencySamples()==16,"prepare/latency");
    require(slot.process(audio) && std::abs(audio.getSample(1,63)-0.2f)<1e-6 && counts.processed==1,"stereo effect output");
    slot.setBypassed(true);fill(audio);require(slot.process(audio) && audio.getSample(0,0)==0.8f && counts.processed==1,"bypass");
    require(!slot.install(nullptr,44100,64,error) && slot.hasEffect() && counts.released==0,"failed replacement dropped effect");
    slot.setBypassed(false);juce::AudioBuffer<float> tooLarge(2,65);fill(tooLarge);
    require(!slot.process(tooLarge) && tooLarge.getMagnitude(0,65)==0 && counts.processed==1,"oversized block");
    require(slot.install(std::make_unique<Effect>(badCounts,true),48000,128,error) && counts.released==1 && counts.destroyed==1,"replace lifecycle");
    fill(audio);require(!slot.process(audio) && slot.hasFault() && audio.getMagnitude(0,64)==0,"nonfinite output not contained");
    fill(audio);require(!slot.process(audio) && badCounts.processed==1,"faulted processor called again");
    slot.clear();require(!slot.hasEffect() && !slot.hasFault() && badCounts.released==1,"clear lifecycle");
    Counts recalledCounts,rejectedCounts;
    require(slot.install(std::make_unique<Effect>(recalledCounts),44100,64,error),"state fixture");
    require(!slot.captureState("test-identity",false,error),"capture while busy");
    auto saved=slot.captureState("test-identity",true,error);
    require(saved && saved->bytes.getSize()==sizeof(float) && !saved->bypassed,"capture state");
    slot.setBypassed(true);saved=slot.captureState("test-identity",true,error);
    require(saved && saved->bypassed,"capture bypass");
    const int releases=recalledCounts.released;
    require(!slot.restoreCandidate(std::make_unique<Effect>(rejectedCounts),"wrong-identity",*saved,44100,64,true,error) &&
        recalledCounts.released==releases && slot.hasEffect(),"identity mismatch replaced live effect");
    require(!slot.restoreCandidate(std::make_unique<Effect>(rejectedCounts),"test-identity",*saved,44100,64,false,error) &&
        recalledCounts.released==releases,"busy restore replaced live effect");
    auto malformed=*saved;malformed.bytes.reset();
    require(!slot.restoreCandidate(std::make_unique<Effect>(rejectedCounts),"test-identity",malformed,44100,64,true,error) &&
        recalledCounts.released==releases && slot.hasEffect(),"throwing restore replaced live effect");
    const float restoredVolume=0.5f;saved->bytes.replaceWith(&restoredVolume,sizeof(restoredVolume));saved->bypassed=false;
    require(slot.restoreCandidate(std::make_unique<Effect>(rejectedCounts),"test-identity",*saved,44100,64,true,error),"restore candidate");
    fill(audio);require(slot.process(audio) && std::abs(audio.getSample(0,0)-0.4f)<1e-6,"restored settings not processing");
    saved->bypassed=true;
    require(slot.restoreCandidate(std::make_unique<Effect>(rejectedCounts),"test-identity",*saved,44100,64,true,error),"restore bypass");
    fill(audio);require(slot.process(audio) && audio.getSample(0,0)==0.8f,"restored bypass not dry");
    slot.clear();
    std::cout<<"Stereo effect slot tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
