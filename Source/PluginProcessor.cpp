#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StateCompatibility.h"
#include <cmath>
IDWVoiceMIDIStudioAudioProcessor::IDWVoiceMIDIStudioAudioProcessor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::mono(),true)
                  .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"IDW_V9",layout()),presets(apvts) {learn.prepare(apvts);}
juce::AudioProcessorValueTreeState::ParameterLayout IDWVoiceMIDIStudioAudioProcessor::layout()
{
    using F = juce::AudioParameterFloat;
    using I = juce::AudioParameterInt;
    using B = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<F>("gate", "Gate", juce::NormalisableRange<float>(.001f, .2f, .001f), .008f));
    p.add(std::make_unique<F>("confidence", "Confidence", .5f, 1.f, .75f));
    p.add(std::make_unique<I>("bend", "Bend", 1, 24, 2));
    p.add(std::make_unique<F>("tuneCents", "Pitch Calibration", -100.f, 100.f, 0.f));
    p.add(std::make_unique<B>("scaleLock", "Scale Lock", false));
    p.add(std::make_unique<I>("root", "Root", 0, 11, 0));
    p.add(std::make_unique<I>("scaleMask", "Scale Mask", 1, 4095, 2741));

    p.add(std::make_unique<B>("beatbox", "Beatbox", false));
    p.add(std::make_unique<F>("beatThreshold", "Beat Threshold", juce::NormalisableRange<float>(.005f, .2f, .001f), .035f));
    p.add(std::make_unique<I>("kick", "Kick", 0, 127, 36));
    p.add(std::make_unique<I>("snare", "Snare", 0, 127, 38));
    p.add(std::make_unique<I>("hat", "Hat", 0, 127, 42));

    p.add(std::make_unique<B>("gestureCC", "Gesture CC", true));
    p.add(std::make_unique<I>("cc", "CC", 0, 127, 74));
    p.add(std::make_unique<F>("ccSense", "CC Sense", .25f, 3.f, 1.f));
    p.add(std::make_unique<B>("mpe", "MPE", false));
    p.add(std::make_unique<I>("mpeFirst", "MPE First", 2, 16, 2));
    p.add(std::make_unique<I>("mpeLast", "MPE Last", 2, 16, 16));
    p.add(std::make_unique<I>("latencyMs", "Latency", 0, 100, 0));
    p.add(std::make_unique<I>("scaleExpression", "Scale Expression", 0, 1, 0));
    p.add(std::make_unique<I>("inputMode", "Input Channel", 0, 2, 0));
    p.add(std::make_unique<B>("monitorMic", "Monitor Microphone", false));
    p.add(std::make_unique<B>("previewAudio", "Preview Instrument", false));
    p.add(std::make_unique<B>("melody", "Melody Tracking", true));
    p.add(std::make_unique<F>("captureBpm", "Capture BPM", juce::NormalisableRange<float>(40.0f, 240.0f, 1.0f), 120.0f));
    const int extraDrumNotes[]={39,45,46,49,51};
    for (int i=3;i<8;++i)
        p.add(std::make_unique<I>("pad"+juce::String(i+1), "Drum Pad "+juce::String(i+1), 0, 127, extraDrumNotes[i-3]));
    p.add(std::make_unique<I>("harmonyMode", "Harmony Scale", 0, 2, 0));
    p.add(std::make_unique<I>("harmonyVoicing", "Chord Voicing", 0, 1, 0));
    p.add(std::make_unique<B>("harmonyBass", "Bass Layer", false));
    p.add(std::make_unique<I>("voiceLow", "Lowest Voice Note", 0, 127, 0));
    p.add(std::make_unique<I>("voiceHigh", "Highest Voice Note", 0, 127, 127));
    return p;
}


void IDWVoiceMIDIStudioAudioProcessor::prepareToPlay(double sr,int block){
    heldHarmony={};
    inputPeak.store(0);observedRate.store(sr);observedBlock.store(block);
    sampleRateHz=sr;hopSize=juce::jmax(1,(int)std::lround(sr*0.005));hop.assign((size_t)hopSize,0);hopFill=0;
    pitch.prepare(sr,block);beats.prepare(sr);mpe.reset();learn.prepare(apvts);capture.resetAudio();
    active=candidate=-1;activeChannel=1;candidateSamples=silentSamples=ccSamples=0;lastCC=lastWheel=-1;
    lastHz=0;smoothNote=-1;negotiatedRanges.fill(-1);drumRemaining.fill(-1);testRemaining=inhibitSamples=0;
    freq.store(0);confidence.store(0);inputRms.store(0);midi.store(-1);displayedBend.store(0);drumDisplay.store(-1);
    previousMpe=value("mpe")>0.5f;previousFirst=(int)value("mpeFirst");previousLast=(int)value("mpeLast");phase=0;previewGain=0;
    setLatencySamples(0); // Live tracking is causal; no artificial delay is added.
}
void IDWVoiceMIDIStudioAudioProcessor::releaseResources(){pitch.reset();capture.resetAudio();}
bool IDWVoiceMIDIStudioAudioProcessor::isBusesLayoutSupported(const BusesLayout& b) const{
    const auto i=b.getMainInputChannelSet(),o=b.getMainOutputChannelSet();
    return (i==juce::AudioChannelSet::mono()||i==juce::AudioChannelSet::stereo())
        && (o==juce::AudioChannelSet::mono()||o==juce::AudioChannelSet::stereo());
}
void IDWVoiceMIDIStudioAudioProcessor::stopHarmony(juce::MidiBuffer& out,int at){
    for(int note:heldHarmony.chord)if(note>=0)out.addEvent(juce::MidiMessage::noteOff(2,note),at);
    if(heldHarmony.bass>=0)out.addEvent(juce::MidiMessage::noteOff(3,heldHarmony.bass),at);
    heldHarmony={};
}
void IDWVoiceMIDIStudioAudioProcessor::updateHarmony(juce::MidiBuffer& out,int at){
    // MPE occupies member channels, including 2/3. Never mix the two modes.
    const auto wanted=value("mpe")>0.5f?idw::HarmonyNotes{}:idw::harmonize(active,(int)value("root"),(int)value("harmonyMode"),value("harmonyVoicing")>0.5f,value("harmonyBass")>0.5f);
    if(wanted==heldHarmony)return;
    stopHarmony(out,at);heldHarmony=wanted;
    for(int note:heldHarmony.chord)if(note>=0)out.addEvent(juce::MidiMessage::noteOn(2,note,(juce::uint8)80),at);
    if(heldHarmony.bass>=0)out.addEvent(juce::MidiMessage::noteOn(3,heldHarmony.bass,(juce::uint8)90),at);
}
void IDWVoiceMIDIStudioAudioProcessor::endVoice(juce::MidiBuffer& out,int at){
    stopHarmony(out,at);
    if(active>=0){out.addEvent(juce::MidiMessage::noteOff(activeChannel,active),at);out.addEvent(juce::MidiMessage::pitchWheel(activeChannel,8192),at);mpe.release(active);}
    active=candidate=-1;candidateSamples=0;midi.store(-1);lastWheel=-1;lastCC=-1;displayedBend.store(0);
}
void IDWVoiceMIDIStudioAudioProcessor::allOff(juce::MidiBuffer& out,int at){
    endVoice(out,at);
    for(int n=0;n<128;++n)if(drumRemaining[(size_t)n]>=0)out.addEvent(juce::MidiMessage::noteOff(10,n),at);
    if(testRemaining>0)out.addEvent(juce::MidiMessage::noteOff(1,60),at);
    for(int ch=1;ch<=16;++ch){out.addEvent(juce::MidiMessage::allNotesOff(ch),at);out.addEvent(juce::MidiMessage::allSoundOff(ch),at);out.addEvent(juce::MidiMessage::pitchWheel(ch,8192),at);}
    drumRemaining.fill(-1);testRemaining=0;mpe.reset();smoothNote=-1;lastHz=0;lastCC=-1;
}
void IDWVoiceMIDIStudioAudioProcessor::negotiateBend(juce::MidiBuffer& out,int at,int channel,int range){
    if(negotiatedRanges[(size_t)channel]==range)return;
    for(auto pair: {std::pair<int,int>{101,0},{100,0},{6,range},{38,0},{101,127},{100,127}})
        out.addEvent(juce::MidiMessage::controllerEvent(channel,pair.first,pair.second),at);
    negotiatedRanges[(size_t)channel]=range;
}
void IDWVoiceMIDIStudioAudioProcessor::analyse(juce::MidiBuffer& out,int at){
    const float detected=pitch.process(hop.data(),hopSize),rms=pitch.rms(),quality=pitch.confidence();
    freq.store(detected);confidence.store(quality);inputRms.store(rms);
    const bool mpeOn=value("mpe")>0.5f;
    const int first=(int)value("mpeFirst"),last=(int)value("mpeLast");
    if(mpeOn!=previousMpe||first!=previousFirst||last!=previousLast){endVoice(out,at);mpe.reset();negotiatedRanges.fill(-1);previousMpe=mpeOn;previousFirst=first;previousLast=last;}
    mpe.setZone(first,last);scale.setMask((uint16_t)value("scaleMask"));
    const float voiceNote=detected>0?69.0f+12.0f*std::log2(detected/440.0f):-1000.0f;
    const bool inVoiceRange=voiceNote>=juce::jmin(value("voiceLow"),value("voiceHigh"))-0.5f && voiceNote<=juce::jmax(value("voiceLow"),value("voiceHigh"))+0.5f;
    const bool valid=inVoiceRange && detected>0 && rms>=value("gate") && quality>=value("confidence") && value("melody")>0.5f && inhibitSamples==0 && beats.trainingPad()<0;
    if(valid){
        const float measured=69.0f+12.0f*std::log2(detected/440.0f)+value("tuneCents")/100.0f;
        if(smoothNote<0||std::abs(measured-smoothNote)>7)smoothNote=measured;
        else smoothNote+=(1.0f-std::exp(-(float)hopSize/(float)(sampleRateHz*0.012)))*(measured-smoothNote);
        const bool locked=value("scaleLock")>0.5f;
        int wanted=juce::jlimit(0,127,(int)std::lround(smoothNote));
        if(locked)wanted=scale.quantize(wanted,(int)value("root"));
        if(wanted==active){candidate=-1;candidateSamples=0;}
        else {
            if(candidate!=wanted){candidate=wanted;candidateSamples=hopSize;}else candidateSamples+=hopSize;
            const int confirmation=(int)(sampleRateHz*(active>=0&&std::abs(wanted-active)>7?0.025:0.010));
            if(active<0 || candidateSamples>=confirmation){endVoice(out,at);active=wanted;activeChannel=mpeOn?mpe.allocate(active):1;
                if(activeChannel==0){active=-1;}else lastWheel=-1;
            }
        }
        if(active>=0){
            const int range=(int)value("bend");
            // Strict mode locks pitch completely. Natural mode keeps only within-note deviation.
            float offset=locked?(value("scaleExpression")>0.5f ? juce::jlimit(-0.45f,0.45f,smoothNote-std::round(smoothNote)) : 0.0f):smoothNote-active;
            offset=juce::jlimit(-(float)range,(float)range,offset);
            const int wheel=juce::jlimit(0,16383,(int)std::lround(8192+offset/range*8192));
            const bool starting=lastWheel<0;
            negotiateBend(out,at,activeChannel,range);
            if(wheel!=lastWheel){out.addEvent(juce::MidiMessage::pitchWheel(activeChannel,wheel),at);lastWheel=wheel;}
            if(starting){const float normal=juce::jlimit(0.0f,1.0f,(rms-value("gate"))/juce::jmax(0.001f,0.2f-value("gate")));
                out.addEvent(juce::MidiMessage::noteOn(activeChannel,active,(juce::uint8)juce::jlimit(1,127,(int)(35+92*normal))),at);}
            midi.store(active);displayedBend.store(offset);
            ccSamples+=hopSize;
            if(value("gestureCC")>0.5f && lastHz>0 && ccSamples>=(int)(sampleRateHz*0.020)){
                const float movement=std::abs(12.0f*std::log2(detected/lastHz));
                const int cc=juce::jlimit(0,127,(int)(movement*64*value("ccSense")));
                if(cc!=lastCC){out.addEvent(juce::MidiMessage::controllerEvent(activeChannel,(int)value("cc"),cc),at);lastCC=cc;}
                ccSamples=0;lastHz=detected;
            }else if(lastHz<=0)lastHz=detected;
        }
        silentSamples=0;
    }else{
        candidate=-1;candidateSamples=0;silentSamples=juce::jmin((int)sampleRateHz,silentSamples+hopSize);
        if(silentSamples>=(int)(sampleRateHz*0.075)||value("melody")<0.5f){endVoice(out,at);smoothNote=-1;lastHz=0;}
    }
    updateHarmony(out,at);
    // Always run onset bookkeeping so toggling beatbox cannot resurrect a stale onset.
    const auto hit=beats.process(hop.data(),hopSize,value("beatThreshold"));
    if(value("beatbox")>0.5f && inhibitSamples==0 && hit.pad>=0){
        static const char* ids[]={"kick","snare","hat","pad4","pad5","pad6","pad7","pad8"};
        const int note=(int)value(ids[hit.pad]);
        if(drumRemaining[(size_t)note]>=0)out.addEvent(juce::MidiMessage::noteOff(10,note),at);
        out.addEvent(juce::MidiMessage::noteOn(10,note,(juce::uint8)hit.velocity),at);
        drumRemaining[(size_t)note]=juce::jmax(1,(int)(sampleRateHz*0.040));drumDisplay.store(hit.pad);drumEvents.fetch_add(1);
    }
}
void IDWVoiceMIDIStudioAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& out){
    juce::ScopedNoDenormals noDenormals;
    const double started=juce::Time::getMillisecondCounterHiRes();
    learn.process(out,apvts);out.clear();
    const int count=buffer.getNumSamples(),channels=buffer.getNumChannels();
    if(count<=0 || channels<=0 || hop.empty())return;
    callbacks.fetch_add(1,std::memory_order_relaxed);
    observedRate.store(sampleRateHz);observedBlock.store(count);
    float blockPeak=0;
    capture.beginBlock(sampleRateHz);
    if(panicRequested.exchange(false)){allOff(out,0);inhibitSamples=(int)(sampleRateHz*0.300);}
    if(testRequested.exchange(false)){
        allOff(out,0);negotiateBend(out,0,1,(int)value("bend"));
        out.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);
        testRemaining=(int)(sampleRateHz*0.350);inhibitSamples=(int)(sampleRateHz*0.500);
    }
    const bool monitor=value("monitorMic")>0.5f,preview=value("previewAudio")>0.5f;
    const int inputMode=(int)value("inputMode"),inputs=juce::jmin(getTotalNumInputChannels(),channels);
    for(int i=0;i<count;++i){
        if(inhibitSamples>0)--inhibitSamples;
        if(testRemaining>0 && --testRemaining==0)out.addEvent(juce::MidiMessage::noteOff(1,60),i);
        for(int n=0;n<128;++n)if(drumRemaining[(size_t)n]>=0 && --drumRemaining[(size_t)n]<=0){out.addEvent(juce::MidiMessage::noteOff(10,n),i);drumRemaining[(size_t)n]=-1;}
        const float left=inputs>0?buffer.getSample(0,i):0,right=inputs>1?buffer.getSample(1,i):left;
        const float selected=inputMode==1?right:inputMode==2?(left+right)*0.5f:left;
        blockPeak=juce::jmax(blockPeak,std::abs(selected));
        hop[(size_t)hopFill++]=selected;
        if(hopFill==hopSize){analyse(out,i);hopFill=0;}
        const int previewNote=testRemaining>0?60:active;
        const float target=preview&&previewNote>=0?0.12f:0.0f;
        previewGain+=(target-previewGain)*(float)(1.0-std::exp(-1.0/(sampleRateHz*0.005)));
        const double noteHz=previewNote>=0?440.0*std::pow(2.0,(previewNote+(testRemaining>0?0.0f:displayedBend.load())-69.0)/12.0):440.0;
        phase+=juce::MathConstants<double>::twoPi*noteHz/sampleRateHz;if(phase>=juce::MathConstants<double>::twoPi)phase-=juce::MathConstants<double>::twoPi;
        const float tone=previewGain*(float)std::sin(phase);
        for(int ch=0;ch<channels;++ch)buffer.setSample(ch,i,(monitor?(ch<inputs?buffer.getSample(ch,i):left):0.0f)+tone);
    }
    inputPeak.store(juce::jmax(blockPeak,inputPeak.load()*(float)std::exp(-count/(sampleRateHz*0.5))));
    int notes=0,events=0;
    for(const auto event:out){++events;if(event.getMessage().isNoteOn())++notes;}
    emittedNotes.fetch_add(notes);emittedEvents.fetch_add(events);
    capture.finishBlock(out,count);
    const double load=(juce::Time::getMillisecondCounterHiRes()-started)/(1000.0*count/sampleRateHz);
    cpuLoad.store(cpuLoad.load()*0.95+load*0.05);
}
void IDWVoiceMIDIStudioAudioProcessor::saveExtraState(){
    for(const auto name: {"MidiMappings","DrumProfiles"}){auto child=apvts.state.getChildWithName(name);if(child.isValid())apvts.state.removeChild(child,nullptr);}
    apvts.state.addChild(learn.save(),-1,nullptr);apvts.state.addChild(beats.save(),-1,nullptr);
}
void IDWVoiceMIDIStudioAudioProcessor::restoreExtraState(){learn.restore(apvts.state.getChildWithName("MidiMappings"));beats.restore(apvts.state.getChildWithName("DrumProfiles"));requestPanic();}
void IDWVoiceMIDIStudioAudioProcessor::getStateInformation(juce::MemoryBlock& data){
    auto state=apvts.copyState();
    for(const auto name:{"MidiMappings","DrumProfiles"}){auto child=state.getChildWithName(name);if(child.isValid())state.removeChild(child,nullptr);}
    state.addChild(learn.save(),-1,nullptr);state.addChild(beats.save(),-1,nullptr);
    if(auto xml=state.createXml())copyXmlToBinary(*xml,data);
}
void IDWVoiceMIDIStudioAudioProcessor::setStateInformation(const void* data,int size){
    if(auto xml=getXmlFromBinary(data,size))if(xml->hasTagName(apvts.state.getType())){apvts.replaceState(withV5Defaults(juce::ValueTree::fromXml(*xml),apvts));restoreExtraState();}
}
juce::AudioProcessorEditor* IDWVoiceMIDIStudioAudioProcessor::createEditor(){return new IDWVoiceMIDIStudioAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new IDWVoiceMIDIStudioAudioProcessor();}
