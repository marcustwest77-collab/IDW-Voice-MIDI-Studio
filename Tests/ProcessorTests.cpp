#include "PluginProcessor.h"
#include "InstrumentPanel.h"
#include "SongScenes.h"
#include "MidiExport.h"
#include "VoiceProfiles.h"
#include <iostream>
#include <random>
#include <chrono>
#include <stdexcept>
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void parameter(IDWVoiceMIDIStudioAudioProcessor& p,const char* id,float value){auto* q=p.apvts.getParameter(id);check(q!=nullptr,"Missing parameter");q->setValueNotifyingHost(q->convertTo0to1(value));}
struct Event{int sample;juce::MidiMessage message;};
std::vector<Event> run(IDWVoiceMIDIStudioAudioProcessor& p,double sr,int block,double seconds,float hz,float amplitude=0.1f,int base=0){
    std::vector<Event> events;int total=(int)std::lround(seconds*sr);
    for(int offset=0;offset<total;offset+=block){const int n=juce::jmin(block,total-offset);juce::AudioBuffer<float> buffer(2,n);buffer.clear();juce::MidiBuffer midi;
        for(int i=0;i<n;++i)buffer.setSample(0,i,amplitude*(float)std::sin(juce::MathConstants<double>::twoPi*hz*(base+offset+i)/sr));
        p.processBlock(buffer,midi);for(const auto e:midi)events.push_back({base+offset+e.samplePosition,e.getMessage()});
        for(int ch=0;ch<2;++ch)for(int i=0;i<n;++i)check(std::isfinite(buffer.getSample(ch,i)),"Nonfinite audio output");
    }return events;
}
auto processor(double sr,int block){auto p=std::make_unique<IDWVoiceMIDIStudioAudioProcessor>();p->setRateAndBufferSizeDetails(sr,block);p->prepareToPlay(sr,block);return p;}
void testPitchAndBuffers(){
    for(double sr:{44100.0,48000.0,96000.0}){
        int referenceOn=-1,referenceOff=-1;
        for(int block:{64,128,256,512}){
            auto p=processor(sr,block);const auto events=run(*p,sr,block,0.3,220);
            int first=-1,notes=0;for(const auto& e:events)if(e.message.isNoteOn()){check(e.message.getNoteNumber()==57,"A3 detected as wrong MIDI note");if(first<0)first=e.sample;++notes;}
            check(notes==1,"Steady pitch retriggered");check(std::abs(p->hz()-220)<0.8,"Pitch estimate error");
            const auto silence=run(*p,sr,block,0.15,0,0,(int)(0.3*sr));int off=-1;
            for(const auto& e:silence)if(e.message.isNoteOff()&&e.message.getNoteNumber()==57)off=e.sample;
            check(off>=0,"Missing note off after silence");
            if(referenceOn<0){referenceOn=first;referenceOff=off;}else{check(first==referenceOn,"Onset depends on host buffer size");check(off==referenceOff,"Release depends on host buffer size");}
        }
        std::cout<<"PASS fixed-hop pitch/onset/release at "<<sr<<" Hz across 64/128/256/512 samples\n";
    }
    for(float hz:{70.0f,110.0f,440.0f,880.0f,990.0f}){auto p=processor(48000,128);run(*p,48000,128,0.15,hz);check(std::abs(1200*std::log2(p->hz()/hz))<15,"Pitch range accuracy outside 15 cents");}
    std::cout<<"PASS pitch range and tuning\n";
}
void testScale(){
    auto p=processor(48000,128);parameter(*p,"scaleLock",1);parameter(*p,"scaleMask",1);parameter(*p,"root",0);
    auto events=run(*p,48000,128,0.2,277.1826f);bool note=false;
    for(const auto& e:events){if(e.message.isNoteOn()){check(e.message.getNoteNumber()==60,"Strict scale note not C4");note=true;}if(e.message.isPitchWheel())check(e.message.getPitchWheelValue()==8192,"Strict scale bends off scale");}
    check(note,"Strict scale produced no note");
    parameter(*p,"scaleExpression",1);events=run(*p,48000,128,0.1,280.0f,0.1f,9600);bool bend=false;
    for(const auto& e:events)if(e.message.isPitchWheel()&&e.message.getPitchWheelValue()!=8192){bend=true;check(std::abs(e.message.getPitchWheelValue()-8192)<=1844,"Natural vibrato exceeds 45 cents");}
    check(bend,"Natural mode lost expression");
    ScaleManager scale;scale.setMask(1);check(scale.quantize(127,7)==127,"Scale quantization edge regression");
    std::cout<<"PASS strict scale / natural expression\n";
}
void testMpePanic(){
    auto p=processor(48000,128);parameter(*p,"mpe",1);auto start=run(*p,48000,128,0.15,220);int ch=0;
    for(const auto& e:start)if(e.message.isNoteOn())ch=e.message.getChannel();check(ch>=2&&ch!=10,"MPE channel invalid");
    parameter(*p,"mpe",0);auto next=run(*p,48000,128,0.02,220,0.1f,7200);bool oldOff=false,newOn=false;
    for(const auto& e:next){if(e.message.isNoteOff()&&e.message.getChannel()==ch)oldOff=true;if(e.message.isNoteOn()&&e.message.getChannel()==1)newOn=true;}
    check(oldOff&&newOn,"MPE mode switch leaves old note active");p->requestPanic();auto stop=run(*p,48000,128,0.005,220);bool off=false;
    for(const auto& e:stop)if(e.message.isNoteOff()&&e.message.getChannel()==1)off=true;check(off,"Panic missing explicit note off");
    p->requestTestNote();auto test=run(*p,48000,128,0.5,0,0);int on=-1,end=-1;
    for(const auto& e:test){if(e.message.isNoteOn()&&e.message.getNoteNumber()==60)on=e.sample;if(e.message.isNoteOff()&&e.message.getNoteNumber()==60)end=e.sample;}
    check(on==0 && end>=16799&&end<=16800,"Test note duration incorrect");
    std::cout<<"PASS MPE channel changes / Panic / test-note pairing\n";
}
void testMidiLearnState(){
    auto p=processor(48000,128);p->learn.arm("gate");juce::MidiBuffer m;m.addEvent(juce::MidiMessage::controllerEvent(1,20,0),0);p->learn.process(m,p->apvts);
    p->learn.arm("confidence");m.clear();m.addEvent(juce::MidiMessage::controllerEvent(1,21,0),0);p->learn.process(m,p->apvts);
    m.clear();m.addEvent(juce::MidiMessage::controllerEvent(1,20,127),0);m.addEvent(juce::MidiMessage::controllerEvent(1,21,127),1);p->learn.process(m,p->apvts);
    check(p->apvts.getRawParameterValue("gate")->load()>0.199f,"First CC not handled");check(p->apvts.getRawParameterValue("confidence")->load()>0.999f,"Second CC not handled");
    juce::MemoryBlock data;p->getStateInformation(data);auto restored=processor(48000,128);restored->setStateInformation(data.getData(),(int)data.getSize());
    m.clear();m.addEvent(juce::MidiMessage::controllerEvent(1,20,0),0);m.addEvent(juce::MidiMessage::controllerEvent(1,21,0),1);restored->learn.process(m,restored->apvts);
    check(restored->apvts.getRawParameterValue("gate")->load()<0.002f,"CC mapping not restored");check(restored->apvts.getRawParameterValue("confidence")->load()<0.501f,"Second mapping not restored");
    m.clear();m.addEvent(juce::MidiMessage::controllerEvent(2,20,127),0);restored->learn.process(m,restored->apvts);check(restored->apvts.getRawParameterValue("gate")->load()<0.002f,"CC mapping ignores MIDI channel");
    std::cout<<"PASS multiple CC messages / channel isolation / session persistence\n";
}
void testCapture(){
    auto p=processor(48000,128);p->capture.start();run(*p,48000,128,0.1,220);p->capture.stop();run(*p,48000,128,0.01,220,0.1f,4800);
    check(!p->capture.isRecording(),"Capture failed to stop");PerformanceCapture::Event e;int ons=0,closure=0;double previous=-1;
    while(p->capture.pop(e)){check(e.seconds>=previous,"Capture timestamp order invalid");previous=e.seconds;juce::MidiMessage m(e.data,e.size);if(m.isNoteOn())++ons;if(m.isAllNotesOff())++closure;}
    check(ons==1&&closure==16,"Capture missing note or closing events");check(p->note()==57,"Stopping capture interrupts live voice");
    std::cout<<"PASS capture timestamps / closing events / live continuation\n";
}
void testMidiExport(){
    const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("Regression-Take.mid");
    std::vector<PerformanceCapture::Event> take;
    auto event=[&](const juce::MidiMessage& m,double seconds){PerformanceCapture::Event e;e.seconds=seconds;e.size=m.getRawDataSize();std::copy_n(m.getRawData(),e.size,e.data);take.push_back(e);};
    event(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);event(juce::MidiMessage::pitchWheel(1,9000),0.2);event(juce::MidiMessage::allNotesOff(1),0.5);
    check(writePerformanceMidi(file,take,120),"MIDI export failed");juce::FileInputStream stream(file);juce::MidiFile midi;check(midi.readFrom(stream),"Exported MIDI cannot be read");
    check(midi.getTimeFormat()==960&&midi.getNumTracks()==1,"MIDI header incorrect");const auto* track=midi.getTrack(0);bool off=false,bend=false,tempo=false;
    for(int i=0;i<track->getNumEvents();++i){const auto& m=track->getEventPointer(i)->message;if(m.isNoteOff()&&m.getNoteNumber()==60){off=true;check(std::abs(m.getTimeStamp()-960)<1,"MIDI duration/tempo incorrect");}if(m.isPitchWheel())bend=true;if(m.isTempoMetaEvent())tempo=true;}
    check(off&&bend&&tempo,"MIDI export dropped notes, expression or tempo");std::cout<<"PASS readable MIDI file / tempo / explicit held-note closure\n";
}
void testDrums(){
    BeatboxClassifier beats;beats.prepare(48000);beats.train(0);std::array<float,240> frame{};
    for(int hit=0;hit<5;++hit){for(int hop=0;hop<50;++hop){for(int i=0;i<240;++i){const int t=hop*240+i;frame[(size_t)i]=hop<8?0.6f*std::exp(-t/1200.0f)*(float)std::sin(juce::MathConstants<double>::twoPi*110*t/48000):0;}beats.process(frame.data(),240,0.02f);}}
    check(beats.examples(0)==5&&beats.trainingPad()<0,"Drum training did not complete");
    BeatboxClassifier restored;restored.prepare(48000);restored.restore(beats.save());check(restored.examples(0)==5,"Drum profile not restored");
    bool detected=false;int velocity=0;
    for(int hop=0;hop<50;++hop){for(int i=0;i<240;++i){const int t=hop*240+i;frame[(size_t)i]=hop<8?0.6f*std::exp(-t/1200.0f)*(float)std::sin(juce::MathConstants<double>::twoPi*110*t/48000):0;}const auto hit=restored.process(frame.data(),240,0.02f);if(hit.pad==0){detected=true;velocity=hit.velocity;}}
    check(detected&&velocity>1&&velocity<=127,"Trained drum not recognized");
    std::cout<<"PASS five-hit training / persistence / trained hit classification\n";
}
void testPreviewAndStereo(){
    auto p=processor(48000,128);parameter(*p,"previewAudio",1);p->requestTestNote();juce::AudioBuffer<float> buffer(2,512);buffer.clear();juce::MidiBuffer midi;p->processBlock(buffer,midi);
    check(buffer.getRMSLevel(0,0,512)>0.025f,"Preview instrument is silent");
    p=processor(48000,128);auto layout=p->getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::stereo());layout.outputBuses.set(0,juce::AudioChannelSet::stereo());check(p->setBusesLayout(layout),"Stereo input not supported");p->prepareToPlay(48000,128);parameter(*p,"inputMode",1);
    bool played=false;
    for(int base=0;base<9600;base+=128){juce::AudioBuffer<float> audio(2,128);audio.clear();juce::MidiBuffer events;for(int i=0;i<128;++i)audio.setSample(1,i,0.1f*(float)std::sin(juce::MathConstants<double>::twoPi*220*(base+i)/48000));p->processBlock(audio,events);check(audio.getRMSLevel(0,0,128)==0&&audio.getRMSLevel(1,0,128)==0,"Microphone monitoring not muted by default");for(auto e:events)if(e.getMessage().isNoteOn())played=true;}
    check(played,"Right-channel microphone not tracked");std::cout<<"PASS audible preview / right-channel input / default monitor mute\n";
}
void testArrangement(){
    auto p=processor(48000,128);parameter(*p,"harmonyMode",1);parameter(*p,"harmonyBass",1);
    auto e=run(*p,48000,128,0.2,220);int lead=0,chords=0,bass=0;
    for(const auto& x:e)if(x.message.isNoteOn()){
        const int ch=x.message.getChannel(),n=x.message.getNoteNumber();
        if(ch==1){check(n==57,"Wrong lead note");++lead;}
        if(ch==2){check(n==57||n==60||n==64,"Wrong diatonic chord");++chords;}
        if(ch==3){check(n==33,"Wrong bass octave");++bass;}
    }
    check(lead==1&&chords==3&&bass==1,"Arrangement layers missing or retriggered");
    parameter(*p,"harmonyMode",0);e=run(*p,48000,128,0.02,220,0.1f,9600);int off=0;
    for(const auto& x:e)if(x.message.isNoteOff()&&(x.message.getChannel()==2||x.message.getChannel()==3))++off;
    check(off==4,"Disabling harmony left held notes");check(p->note()==57,"Harmony disable interrupted lead");
    parameter(*p,"harmonyMode",1);parameter(*p,"mpe",1);e=run(*p,48000,128,0.1,220,0.1f,10560);int ons=0;
    for(const auto& x:e)if(x.message.isNoteOn())++ons;check(ons==1,"MPE should produce only one lead, not chord/bass layers");
    auto ranged=processor(48000,128);parameter(*ranged,"voiceLow",60);parameter(*ranged,"voiceHigh",72);
    e=run(*ranged,48000,128,0.2,220);for(const auto& x:e)check(!x.message.isNoteOn(),"Voice range did not reject low note");
    std::cout<<"PASS V5 arrangement / channel separation / mode release / MPE exclusion / voice range\n";
}
void testLegacyState(){
    auto p=processor(48000,128);auto legacy=p->apvts.copyState();
    for(const char* id:{"harmonyMode","harmonyVoicing","harmonyBass","voiceLow","voiceHigh"})legacy.removeChild(legacy.getChildWithProperty("id",id),nullptr);
    parameter(*p,"harmonyMode",2);parameter(*p,"harmonyBass",1);parameter(*p,"voiceLow",70);
    auto xml=legacy.createXml();juce::MemoryBlock data;juce::AudioProcessor::copyXmlToBinary(*xml,data);p->setStateInformation(data.getData(),(int)data.getSize());
    check(p->apvts.getRawParameterValue("harmonyMode")->load()==0,"Legacy state left harmony enabled");
    check(p->apvts.getRawParameterValue("voiceLow")->load()==0&&p->apvts.getRawParameterValue("voiceHigh")->load()==127,"Legacy state retained restrictive voice profile");
    std::cout<<"PASS V4 state migration defaults V5-only controls\n";
}
void testVoiceProfiles(){
    auto p=processor(48000,128);const auto name="Regression-"+juce::Uuid().toString();
    parameter(*p,"voiceLow",48);parameter(*p,"voiceHigh",76);parameter(*p,"gate",0.019f);parameter(*p,"harmonyMode",2);
    check(VoiceProfiles::save(p->apvts,name),"Voice profile save failed");
    parameter(*p,"voiceLow",0);parameter(*p,"gate",0.002f);parameter(*p,"harmonyMode",1);
    check(VoiceProfiles::load(p->apvts,name),"Voice profile load failed");
    check(p->apvts.getRawParameterValue("voiceLow")->load()==48,"Voice range did not restore");
    check(std::abs(p->apvts.getRawParameterValue("gate")->load()-0.019f)<0.0001f,"Voice calibration did not restore");
    check(p->apvts.getRawParameterValue("harmonyMode")->load()==1,"Voice profile overwrote arrangement");
    VoiceProfiles::directory().getChildFile(name+".xml").deleteFile();
    std::cout<<"PASS V5 named voice profile roundtrip / musical-state isolation\n";
}
void testTakeRecovery(){
    auto p=processor(48000,128);check(p->takes.start(123),"Cannot start processor-owned take");run(*p,48000,128,0.15,220);
    p->takes.drain();const auto size=p->takes.size();check(size>0,"Take not retained by processor");
    {std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());}
    check(p->capture.isRecording(),"Closing editor stopped recording");check(p->takes.size()>=size,"Closing editor discarded take");
    run(*p,48000,128,0.1,220,0.1f,7200);p->capture.stop();run(*p,48000,128,0.01,220,0.1f,12000);p->takes.drain();
    const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("V5-Recovery-Test.mid");check(p->takes.exportTo(file),"Export retained take failed");
    auto restored=processor(48000,128);check(restored->takes.recover(file),"Recovery MIDI load failed");
    check(restored->takes.size()>0&&std::abs(restored->takes.tempo()-123)<0.01,"Recovered take or tempo invalid");
    const auto before=restored->takes.size();check(!restored->takes.recover(file.getSiblingFile("nonexistent.mid")),"Invalid recovery file accepted");check(restored->takes.size()==before,"Failed recovery destroyed take");
    std::cout<<"PASS V5 recording survives editor close / MIDI recovery / tempo / failed-load preservation\n";
}
void testStudioInstrument(){
    auto p=processor(48000,256);parameter(*p,"synthEnabled",1);parameter(*p,"melody",0);parameter(*p,"synthDelay",0);
    juce::AudioBuffer<float> buffer(2,256);buffer.clear();juce::MidiBuffer notes;
    notes.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),128);
    notes.addEvent(juce::MidiMessage::noteOn(2,64,(juce::uint8)100),128);
    notes.addEvent(juce::MidiMessage::noteOn(3,36,(juce::uint8)100),128);
    p->processBlock(buffer,notes);
    check(buffer.getMagnitude(0,0,128)==0,"Synth plays before MIDI event offset");
    check(buffer.getRMSLevel(0,128,128)>0.001f,"Incoming MIDI does not play studio instrument");
    check(notes.isEmpty(),"Incoming synth notes unexpectedly passed to outgoing MIDI");
    p->requestPanic();buffer.clear();notes.clear();p->processBlock(buffer,notes);
    check(buffer.getMagnitude(0,0,256)==0,"Panic left synth audio sounding");
    juce::MemoryBlock saved;p->getStateInformation(saved);auto restored=processor(48000,256);restored->setStateInformation(saved.getData(),(int)saved.getSize());
    check(restored->apvts.getRawParameterValue("synthEnabled")->load()==1,"Instrument state failed to restore");
    std::cout<<"PASS V6 instrument MIDI offsets / incoming polyphony / Panic / state persistence\n";
}
void testRetrospectiveCapture(){
    auto p=processor(48000,256);run(*p,48000,256,0.4,220);check(!p->capture.isRecording(),"Retrospective capture started Record MIDI");
    auto take=p->retrospective.snapshot();check(!take.empty(),"Unrecorded voice performance missing");
    bool note=false;for(auto& e:take)if((e.data[0]&0xf0)==0x90&&e.data[2]>0)note=true;check(note,"Recent take has no notes");
    check(p->takes.size()==0,"Retrospective capture overwrote recorded take");
    parameter(*p,"retroEnabled",0);run(*p,48000,256,.02,0);check(p->retrospective.snapshot().empty(),"Disabled retrospective retained notes");
    parameter(*p,"retroEnabled",1);run(*p,48000,256,.15,330);check(!p->retrospective.snapshot().empty(),"Re-enabled retrospective failed");
    const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("V61-Retrospective.mid");check(writePerformanceMidi(file,p->retrospective.snapshot(),120),"Cannot export retrospective MIDI");
    juce::FileInputStream input(file);juce::MidiFile midi;check(midi.readFrom(input),"Retrospective MIDI unreadable");
    auto overflow=std::make_unique<RetrospectiveCapture>();juce::MidiBuffer burst;
    for(int i=0;i<40000;++i)burst.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)90),0);
    overflow->finishBlock(burst,512,48000,true);check(overflow->snapshot().empty()&&overflow->incomplete(),"Overflow did not discard incomplete history");
    burst.clear();burst.addEvent(juce::MidiMessage::noteOn(1,64,(juce::uint8)90),0);overflow->finishBlock(burst,512,48000,true);check(!overflow->snapshot().empty(),"Buffer did not recover after overflow");
    std::cout<<"PASS V6.1 retrospective capture without Record / isolated take / disable-clear / resume / MIDI export / queue overflow recovery\n";
}
void testSongScenes(){
    auto p=processor(48000,128);const auto name="QA-scene-"+juce::Uuid().toString();
    parameter(*p,"synthEnabled",1);parameter(*p,"synthLead",4);parameter(*p,"harmonyMode",2);parameter(*p,"harmonyBass",1);
    check(SongScenes::save(p->apvts,name),"Scene save failed");
    parameter(*p,"synthLead",0);parameter(*p,"harmonyMode",0);parameter(*p,"gate",.025f);parameter(*p,"voiceLow",50);parameter(*p,"captureBpm",93);parameter(*p,"retroEnabled",0);
    check(SongScenes::load(p->apvts,name),"Scene load failed");
    check(p->apvts.getRawParameterValue("synthLead")->load()==4&&p->apvts.getRawParameterValue("harmonyMode")->load()==2,"Musical scene did not recall");
    check(std::abs(p->apvts.getRawParameterValue("gate")->load()-.025f)<.0001f&&p->apvts.getRawParameterValue("voiceLow")->load()==50,"Scene changed microphone/voice profile");
    check(p->apvts.getRawParameterValue("captureBpm")->load()==93&&p->apvts.getRawParameterValue("retroEnabled")->load()==0,"Scene changed tempo or capture preference");
    const auto file=SongScenes::directory().getChildFile(name+".xml");auto xml=juce::XmlDocument::parse(file);check(xml!=nullptr,"Scene XML unavailable");xml->setAttribute("synthLead",0);xml->setAttribute("harmonyMode","not-a-number");check(xml->writeTo(file),"Cannot write invalid fixture");
    check(!SongScenes::load(p->apvts,name),"Malformed scene accepted");check(p->apvts.getRawParameterValue("synthLead")->load()==4,"Failed scene partially changed settings");file.deleteFile();
    std::cout<<"PASS V6.1 scene recall / microphone-profile isolation / tempo isolation / invalid-file atomic rejection\n";
}
void renderEditor(){
    auto p=processor(48000,128);std::vector<float> before;for(auto* parameter:p->getParameters())before.push_back(parameter->getValue());std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());for(int i=0;i<p->getParameters().size();++i)check(std::abs(before[(size_t)i]-p->getParameters()[i]->getValue())<1.0e-6f,"Opening editor modifies processor parameters");editor->setVisible(true);
    for(auto size:{std::pair<int,int>{1120,900},{1040,890}}){
        editor->setSize(size.first,size.second);editor->resized();
        for(auto* child:editor->getChildren())if(child->isVisible())check(editor->getLocalBounds().contains(child->getBounds()),"Visible control outside editor bounds");
        juce::Image image(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true,juce::SoftwareImageType());{juce::Graphics graphics(image);editor->paintEntireComponent(graphics,true);}
        check(image.getPixelAt(1,1).getAlpha()>0,"Editor render is empty");juce::MemoryOutputStream output;juce::PNGImageFormat png;check(png.writeImageToStream(image,output),"Cannot render UI preview");
        const auto file=juce::File::getCurrentWorkingDirectory().getChildFile(size.first==1120?"IDW-V6-Preview.png":"IDW-V6-Minimum.png");check(file.replaceWithData(output.getData(),output.getDataSize()),"Cannot save UI preview");
    }
    for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="Studio controls"){button->onClick();break;}
    for(auto* child:editor->getChildren())if(child->isVisible())check(editor->getLocalBounds().contains(child->getBounds()),"Studio control outside editor bounds");
    juce::Image studio(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true,juce::SoftwareImageType());{juce::Graphics graphics(studio);editor->paintEntireComponent(graphics,true);}
    juce::MemoryOutputStream stream;juce::PNGImageFormat png;check(png.writeImageToStream(studio,stream),"Cannot render Studio view");
    check(juce::File::getCurrentWorkingDirectory().getChildFile("IDW-V6-Studio.png").replaceWithData(stream.getData(),stream.getDataSize()),"Cannot save Studio preview");
    for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="Studio instrument"){button->onClick();break;}
    for(auto* child:editor->getChildren())if(auto* panel=dynamic_cast<InstrumentPanel*>(child)){
        check(panel->isVisible(),"Instrument panel not visible");
        for(auto* control:panel->getChildren())check(panel->getLocalBounds().contains(control->getBounds()),"Instrument control outside panel");
    }
    juce::Image instrument(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true,juce::SoftwareImageType());{juce::Graphics graphics(instrument);editor->paintEntireComponent(graphics,true);}
    juce::MemoryOutputStream instrumentStream;check(png.writeImageToStream(instrument,instrumentStream),"Cannot render instrument view");
    check(juce::File::getCurrentWorkingDirectory().getChildFile("IDW-V6-Instrument.png").replaceWithData(instrumentStream.getData(),instrumentStream.getDataSize()),"Cannot save instrument preview");
    std::cout<<"PASS V6 performance/default/minimum, Studio and Instrument editor render / bounds\n";
}
}
int main(){juce::ScopedJuceInitialiser_GUI init;try{testPitchAndBuffers();testScale();testMpePanic();testMidiLearnState();testCapture();testMidiExport();testDrums();testPreviewAndStereo();testArrangement();testLegacyState();testVoiceProfiles();testTakeRecovery();testStudioInstrument();testRetrospectiveCapture();testSongScenes();renderEditor();std::cout<<"ALL REGRESSIONS PASSED\n";return 0;}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<"\n";return 1;}}

