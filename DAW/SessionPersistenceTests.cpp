#include "SessionPersistence.h"
#include "AIProposal.h"
#include "AIStatusSnapshot.h"
#include "MidiScheduler.h"
#include "MidiSineSynth.h"
#include "PlaybackTiming.h"
#include "LatencyAlignment.h"
#include "LoopbackWavAnalysis.h"
#include "LoopbackReference.h"
#include "LoopbackCapture.h"
#include "TakeRecorder.h"
#include "ProjectPackaging.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <stdexcept>
#include <limits>
#include <iostream>
int main(){
 using namespace idw::daw;
 {
    Session source;source.addTrack(TrackType::audio);source.addTrack(TrackType::midi);
    source.renameTrack(0,"Lead Vocal");source.renameTrack(1,"Keys / Chorus");
    Session restored;
    if(!sessionFromJson(sessionToJson(source),restored) ||
       restored.tracks[0].name!="Lead Vocal" || restored.tracks[1].name!="Keys / Chorus")
        throw std::runtime_error("renamed track round trip");
 }
 for(int fx=0;fx<=3;++fx){
    Session source;source.addTrack(TrackType::audio);source.setTrackVocalFxPreset(0,fx);
    source.setTrackVocalFxMix(0,0.4f);
    Session restored;
    auto json=sessionToJson(source);
    if(!sessionFromJson(json,restored) || restored.tracks[0].vocalFxPreset!=fx || restored.tracks[0].vocalFxMix!=0.4f)
        throw std::runtime_error("vocal FX round trip");
    auto* track=json.getDynamicObject()->getProperty("tracks").getArray()->getReference(0).getDynamicObject();
    for(const juce::var invalid:{juce::var(-0.1),juce::var(1.1),juce::var("0.5"),juce::var(true)}){
        track->setProperty("vocalFxMix",invalid);
        if(sessionFromJson(json,restored))throw std::runtime_error("invalid vocal mix accepted");
    }
    track->removeProperty("vocalFxMix");
    track->removeProperty("vocalFxPreset");
    if(!sessionFromJson(json,restored) || restored.tracks[0].vocalFxPreset!=0 || restored.tracks[0].vocalFxMix!=1.0f)
        throw std::runtime_error("old project must default FX off");
    for(const juce::var invalid:{juce::var(-1),juce::var(4),juce::var("1"),juce::var(1.5)}){
        track->setProperty("vocalFxPreset",invalid);
        if(sessionFromJson(json,restored))throw std::runtime_error("invalid vocal FX accepted");
    }
 }
 AIProposal action{AIProposal::Kind::tempo};
 if(!parseAIProposal(juce::JSON::parse(R"({"format":"idw-ai-proposal-v1","action":{"type":"set_tempo","bpm":145}})"),action)
    || action.kind!=AIProposal::Kind::tempo || action.bpm!=145)
    throw std::runtime_error("AI tempo proposal");
 Session approved;
 if(!applyApprovedAIProposal(approved,action) || approved.bpm!=145)
    throw std::runtime_error("approved tempo");
 if(parseAIProposal(juce::JSON::parse(R"({"format":"idw-ai-proposal-v1","action":{"type":"set_tempo","bpm":999}})"),action))
    throw std::runtime_error("accepted unsafe AI tempo");
 if(parseAIProposal(juce::JSON::parse(R"({"format":"idw-ai-proposal-v1","action":{"type":"set_tempo","bpm":145,"extra":"unexpected"}})"),action))
    throw std::runtime_error("accepted unexpected AI parameter");
 if(parseAIProposal(juce::JSON::parse(R"({"format":"idw-ai-proposal-v1","action":{"type":"set_track_mix","track_index":0,"gain":0.5,"pan":0,"file":"secret.wav"}})"),action))
    throw std::runtime_error("accepted extra file field");
 if(parseAIProposal(juce::JSON::parse(R"({"format":"idw-ai-proposal-v1","action":{"type":"delete_recordings","path":"all"}})"),action))
    throw std::runtime_error("accepted unsafe AI command");
 if(!parseAIProposal(juce::JSON::parse(R"({"format":"idw-ai-proposal-v1","action":{"type":"add_track","track_type":"midi"}})"),action)
     || !applyApprovedAIProposal(approved,action) || approved.tracks.size()!=1)
     throw std::runtime_error("approved MIDI track");
 Session mixSession;mixSession.addTrack(TrackType::audio);
 if(!parseAIProposal(juce::JSON::parse(R"({"format":"idw-ai-proposal-v1","action":{"type":"set_track_mix","track_index":0,"gain":0.75,"pan":-0.25}})"),action)
    || !applyApprovedAIProposal(mixSession,action) ||
    mixSession.tracks[0].gain!=0.75f || mixSession.tracks[0].pan!=-0.25f)
    throw std::runtime_error("AI approved mix");
 if(parseAIProposal(juce::JSON::parse(R"({"format":"idw-ai-proposal-v1","action":{"type":"set_track_mix","track_index":0,"gain":5,"pan":0}})"),action))
    throw std::runtime_error("accepted unsafe AI mix");
 if(parseAIProposal(juce::JSON::parse(R"({"format":"idw-ai-proposal-v1","action":{"type":"set_track_mix","track_index":-1,"gain":1,"pan":0}})"),action))
    throw std::runtime_error("accepted negative track index");
 Session midiFixture;
 if(!midiFixture.addTrack(TrackType::midi) ||
    !midiFixture.addMidiNote(0,{64,2.0,1.5,93}) ||
    !midiFixture.moveMidiNote(0,0,3.0,67) ||
    !midiFixture.resizeMidiNote(0,0,0.75))
    throw std::runtime_error("MIDI note creation/editing");
 if(midiFixture.addMidiNote(0,{128,0,1,100}) ||
    midiFixture.moveMidiNote(0,0,-1,64) ||
    midiFixture.resizeMidiNote(0,0,0) ||
    midiFixture.tracks[0].notes.size()!=1)
    throw std::runtime_error("Invalid MIDI edit accepted");
 if(!midiFixture.setTrackSynthPreset(0,2) || midiFixture.setTrackSynthPreset(0,5))
    throw std::runtime_error("MIDI preset validation");
 if(!midiFixture.setTrackSynthEnvelope(0,25.0f,90.0f) ||
    midiFixture.setTrackSynthEnvelope(0,-1.0f,50.0f))
    throw std::runtime_error("MIDI envelope validation");
 // Every supported preset must survive session save/load.
 for(int preset=0;preset<=4;++preset){
    Session presetSession;presetSession.addTrack(TrackType::midi);
    if(!presetSession.setTrackSynthPreset(0,preset))
        throw std::runtime_error("Valid MIDI preset rejected");
    Session roundTrip;
    if(!sessionFromJson(sessionToJson(presetSession),roundTrip) ||
       roundTrip.tracks.size()!=1 || roundTrip.tracks[0].synthPreset!=preset)
        throw std::runtime_error("MIDI preset round-trip failure");
 }
 Session midiRestored;
 if(!sessionFromJson(sessionToJson(midiFixture),midiRestored) ||
    midiRestored.tracks.size()!=1 || midiRestored.tracks[0].notes.size()!=1 ||
    midiRestored.tracks[0].synthPreset!=2 ||
    midiRestored.tracks[0].synthAttackMs!=25.0f ||
    midiRestored.tracks[0].synthReleaseMs!=90.0f ||
    midiRestored.tracks[0].notes[0].pitch!=67 ||
    midiRestored.tracks[0].notes[0].startBeat!=3 ||
    midiRestored.tracks[0].notes[0].lengthBeats!=0.75)
    throw std::runtime_error("MIDI notes persistence");
 if(!midiRestored.removeMidiNote(0,0) ||
    !midiRestored.tracks[0].notes.empty() ||
    midiRestored.removeMidiNote(0,0))
    throw std::runtime_error("MIDI note deletion");
 // A playback copy must not change when its editable project is modified.
 Session editableSnapshot;editableSnapshot.addTrack(TrackType::midi);
 editableSnapshot.addMidiNote(0,{60,0.0,1.0,100});
 const Session playbackCopy=editableSnapshot;
 editableSnapshot.moveMidiNote(0,0,2.0,72);
 editableSnapshot.setTrackSynthPreset(0,4);
 if(playbackCopy.tracks[0].notes[0].pitch!=60 ||
    playbackCopy.tracks[0].notes[0].startBeat!=0.0 ||
    playbackCopy.tracks[0].synthPreset!=0)
    throw std::runtime_error("Playback snapshot changed after project edit");
 if(std::abs(playbackBeat(48000,48000.0,120.0)-2.0)>1e-10 ||
    playbackBeat(-1,48000.0,120.0)!=0.0 ||
    playbackBeat(48000,0.0,120.0)!=0.0 ||
    playbackBeat(48000,48000.0,0.0)!=0.0)
    throw std::runtime_error("Playback clock conversion");
 Session arrangement;arrangement.addTrack(TrackType::midi);
 arrangement.addMidiNote(0,{60,3.0,2.5,100});
 if(std::abs(arrangementEndBeat(arrangement)-5.5)>1e-10)
    throw std::runtime_error("Arrangement end from MIDI note");
 Session mixedArrangement;
 mixedArrangement.addTrack(TrackType::audio);
 mixedArrangement.addTrack(TrackType::midi);
 mixedArrangement.addClip(0,{"mix-test.wav",1.0,8.0,0.0});
 mixedArrangement.addMidiNote(1,{72,10.0,2.0,96});
 if(std::abs(arrangementEndBeat(mixedArrangement)-12.0)>1e-10)
    throw std::runtime_error("Mixed arrangement end ignored MIDI");
 mixedArrangement.tracks[1].notes.clear();
 if(std::abs(arrangementEndBeat(mixedArrangement)-9.0)>1e-10)
    throw std::runtime_error("Audio clip arrangement end incorrect");
 Session emptyArrangement;
 if(arrangementEndBeat(emptyArrangement)!=0.0)
    throw std::runtime_error("Empty arrangement end incorrect");
 // Long multi-track arrangements must use the latest item, regardless of track order.
 Session largeArrangement;
 for(int track=0;track<32;++track){
    largeArrangement.addTrack(track%2==0?TrackType::audio:TrackType::midi);
    if(track%2==0)
        largeArrangement.addClip((std::size_t)track,{"long-test.wav",0.0,64.0+track,0.0});
    else
        largeArrangement.addMidiNote((std::size_t)track,{60+track%12,128.0+track,2.0,100});
 }
 if(std::abs(arrangementEndBeat(largeArrangement)-161.0)>1e-10)
    throw std::runtime_error("Multi-track arrangement endpoint incorrect");
 if(std::abs(playbackBeat(48000LL*60LL*30LL,48000.0,120.0)-3600.0)>1e-8)
    throw std::runtime_error("Thirty-minute playback clock incorrect");
 Session scheduleFixture;
 scheduleFixture.addTrack(TrackType::midi);
 scheduleFixture.addMidiNote(0,{60,1.0,1.0,100});
 scheduleFixture.addMidiNote(0,{62,2.0,0.5,80});
 const auto events=buildMidiSchedule(scheduleFixture);
 if(events.size()!=4 || events[0].beat!=1.0 || !events[0].noteOn ||
    events[1].beat!=2.0 || events[1].noteOn ||
    events[2].beat!=2.0 || !events[2].noteOn ||
    events[3].beat!=2.5 || events[3].noteOn)
    throw std::runtime_error("MIDI playback event ordering");
 scheduleFixture.tracks[0].mute=true;
 if(!buildMidiSchedule(scheduleFixture).empty())
    throw std::runtime_error("muted MIDI notes scheduled");
 Session sineSession;sineSession.addTrack(TrackType::midi);
 sineSession.addMidiNote(0,{69,0,1,127});
 std::vector<float> left(256,0.0f),right(256,0.0f);
 renderMidiSine(sineSession,left.data(),right.data(),256,0,48000.0);
 if(std::all_of(left.begin(),left.end(),[](float x){return x==0.0f;}) ||
    left!=right)throw std::runtime_error("MIDI sine render silent or wrong stereo");
 std::vector<float> bass(256,0.0f),lead(256,0.0f);
 sineSession.setTrackSynthPreset(0,1);
 renderMidiSine(sineSession,bass.data(),nullptr,256,0,48000.0);
 sineSession.setTrackSynthPreset(0,2);
 renderMidiSine(sineSession,lead.data(),nullptr,256,0,48000.0);
 if(std::all_of(bass.begin(),bass.end(),[](float x){return x==0.0f;}) ||
    bass==left || lead==left || bass==lead)
    throw std::runtime_error("synth presets should yield distinct audio");
 Session chordSession;chordSession.addTrack(TrackType::midi);
 chordSession.addMidiNote(0,{60,0,1,127});
 chordSession.addMidiNote(0,{64,0,1,127});
 chordSession.addMidiNote(0,{67,0,1,127});
 std::vector<float> chordLeft(2048,0.0f);
 renderMidiSine(chordSession,chordLeft.data(),nullptr,2048,0,48000.0);
 if(std::all_of(chordLeft.begin(),chordLeft.end(),[](float v){return v==0.0f;}) ||
    std::any_of(chordLeft.begin(),chordLeft.end(),[](float v){return !std::isfinite(v) || std::abs(v)>1.0f;}))
    throw std::runtime_error("polyphonic MIDI output exceeds safe range");
 Session distantSession;distantSession.addTrack(TrackType::midi);
 distantSession.addMidiNote(0,{72,1000000.0,1.0,127});
 std::vector<float> distantBuffer(256,0.0f);
 renderMidiSine(distantSession,distantBuffer.data(),nullptr,256,0,48000.0);
 if(std::any_of(distantBuffer.begin(),distantBuffer.end(),[](float sample){return sample!=0.0f;}))
    throw std::runtime_error("Distant MIDI notes produced audio outside active block");
 Session nyquistSession;nyquistSession.addTrack(TrackType::midi);
 nyquistSession.addMidiNote(0,{127,0.0,1.0,127});
 std::vector<float> lowRateSamples(256,0.0f);
 renderMidiSine(nyquistSession,lowRateSamples.data(),nullptr,256,0,8000.0);
 if(std::any_of(lowRateSamples.begin(),lowRateSamples.end(),[](float sample){return sample!=0.0f;}))
    throw std::runtime_error("Nyquist-unsafe note rendered at low sample rate");
 // At 8 kHz, MIDI A7 (3520 Hz) permits only its fundamental.
 const double phase=0.47;
 const float expected=(float)std::sin(phase);
 if(std::abs(synthWave(SynthPreset::bass,phase,3520.0,8000.0)-expected)>1e-6f ||
    std::abs(synthWave(SynthPreset::lead,phase,3520.0,8000.0)-expected)>1e-6f)
    throw std::runtime_error("Synth produced above-Nyquist harmonics");
 // At 48 kHz, bass and lead retain their intended harmonic character.
 if(synthWave(SynthPreset::bass,phase,440.0,48000.0)==expected ||
    synthWave(SynthPreset::lead,phase,440.0,48000.0)==expected)
    throw std::runtime_error("Synth lost safe harmonics");
 // Harmonic boundary: second harmonic exactly at Nyquist must be omitted.
 const double edgePhase=0.83;
 const float edgeFundamental=(float)std::sin(edgePhase);
 if(std::abs(synthWave(SynthPreset::bass,edgePhase,2000.0,8000.0)-edgeFundamental)>1e-6f)
    throw std::runtime_error("Nyquist boundary harmonic not filtered");
 // Rendering a block in two segments must match one contiguous render.
 std::vector<float> fullBlock(512,0.0f),splitBlock(512,0.0f);
 renderMidiSine(sineSession,fullBlock.data(),nullptr,512,0,48000.0);
 renderMidiSine(sineSession,splitBlock.data(),nullptr,256,0,48000.0);
 renderMidiSine(sineSession,splitBlock.data()+256,nullptr,256,256,48000.0);
 if(fullBlock!=splitBlock)
    throw std::runtime_error("MIDI audio differs across block boundaries");
 // Malformed tempo or negative playback position must be safely silent.
 Session invalidTempo;invalidTempo.addTrack(TrackType::midi);
 invalidTempo.addMidiNote(0,{69,0,1,127});
 std::vector<float> invalidOutput(256,0.0f);
 invalidTempo.bpm=0.0;
 renderMidiSine(invalidTempo,invalidOutput.data(),nullptr,256,0,48000.0);
 invalidTempo.bpm=120.0;
 renderMidiSine(invalidTempo,invalidOutput.data(),nullptr,256,-1,48000.0);
 if(std::any_of(invalidOutput.begin(),invalidOutput.end(),[](float sample){return sample!=0.0f;}))
    throw std::runtime_error("Malformed timing produced synth audio");
 std::vector<float> organ(256,0.0f);
 if(!sineSession.setTrackSynthPreset(0,3))throw std::runtime_error("Organ preset rejected");
 renderMidiSine(sineSession,organ.data(),nullptr,256,0,48000.0);
 if(organ==left || organ==bass || organ==lead ||
    std::all_of(organ.begin(),organ.end(),[](float x){return x==0.0f;}))
    throw std::runtime_error("Organ preset not audible or distinct");
 std::vector<float> pad(256,0.0f);
 if(!sineSession.setTrackSynthPreset(0,4))throw std::runtime_error("Pad preset rejected");
 renderMidiSine(sineSession,pad.data(),nullptr,256,0,48000.0);
 if(pad==left || pad==bass || pad==lead || pad==organ ||
    std::all_of(pad.begin(),pad.end(),[](float x){return x==0.0f;}))
    throw std::runtime_error("Pad preset not audible or distinct");
 // Invalid MIDI notes must not attenuate playable voices in the mix.
 Session validVoices;
 validVoices.addTrack(TrackType::midi);
 validVoices.addMidiNote(0,{60,0.0,2.0,100});
 std::vector<float> cleanOutput(256,0.0f),invalidVoiceOutput(256,0.0f);
 renderMidiSine(validVoices,cleanOutput.data(),nullptr,256,0,48000.0);
 Session invalidVoices=validVoices;
 invalidVoices.tracks[0].notes.push_back({999,0.0,2.0,100});
 invalidVoices.tracks[0].notes.push_back({60,0.0,2.0,0});
 renderMidiSine(invalidVoices,invalidVoiceOutput.data(),nullptr,256,0,48000.0);
 if(cleanOutput!=invalidVoiceOutput)
    throw std::runtime_error("Invalid MIDI notes changed voice normalization");
 // Dense multi-track MIDI render smoke test: no clipping, NaNs or inconsistent
 // output when the same audio is processed in smaller callback blocks.
 Session denseSession;
 for(int track=0;track<16;++track){
    denseSession.addTrack(TrackType::midi);
    denseSession.setTrackSynthPreset((std::size_t)track,track%5);
    for(int note=0;note<8;++note)
        denseSession.addMidiNote((std::size_t)track,{48+(track+note)%36,0.0,2.0,100});
 }
 std::vector<float> denseFull(1024,0.0f),denseSplit(1024,0.0f);
 renderMidiSine(denseSession,denseFull.data(),nullptr,1024,0,48000.0);
 for(int offset=0;offset<1024;offset+=256)
    renderMidiSine(denseSession,denseSplit.data()+offset,nullptr,256,offset,48000.0);
 if(denseFull!=denseSplit ||
    std::any_of(denseFull.begin(),denseFull.end(),[](float x){return !std::isfinite(x) || std::abs(x)>1.0f;}))
    throw std::runtime_error("Dense MIDI rendering unstable across blocks");
 sineSession.tracks[0].mute=true;
 std::fill(left.begin(),left.end(),0.0f);
 renderMidiSine(sineSession,left.data(),nullptr,256,0,48000.0);
 if(std::any_of(left.begin(),left.end(),[](float x){return x!=0.0f;}))
    throw std::runtime_error("muted synth generated sound");
 Session statusFixture;statusFixture.addTrack(TrackType::audio);
 statusFixture.addClip(0,{"PRIVATE_RECORDING_PATH_DO_NOT_SHARE.wav",0,4,0});
 const auto statusJson=juce::JSON::toString(aiSessionStatus(statusFixture,true,false));
 if(statusJson.contains("PRIVATE_RECORDING_PATH_DO_NOT_SHARE") ||
    !statusJson.contains("clipCount") || !statusJson.contains("playing"))
    throw std::runtime_error("AI snapshot leaked source paths or missed state");
 // Audio hardware routing must survive a save/load round-trip while old
 // session files without inputChannel continue defaulting to input one.
 Session routing;routing.addTrack(TrackType::audio);
 routing.tracks[0].inputChannel=5;
 Session routed;
 if(!sessionFromJson(sessionToJson(routing),routed) ||
    routed.tracks[0].inputChannel!=5)
    throw std::runtime_error("audio input routing round-trip");
 auto legacyRoute=sessionToJson(routing);
 legacyRoute.getDynamicObject()->getProperty("tracks").getArray()->getReference(0)
    .getDynamicObject()->removeProperty("inputChannel");
 Session legacyLoaded;
 if(!sessionFromJson(legacyRoute,legacyLoaded) || legacyLoaded.tracks[0].inputChannel!=0)
    throw std::runtime_error("legacy input routing fallback");
 auto badRoute=sessionToJson(routing);
 badRoute.getDynamicObject()->getProperty("tracks").getArray()->getReference(0)
    .getDynamicObject()->setProperty("inputChannel",99);
 if(sessionFromJson(badRoute,routed))
    throw std::runtime_error("invalid input channel accepted");
 // Synthetic loopback fixtures verify positive/negative offsets and silence.
 std::vector<float> pulse(2048,0.0f);
 pulse[1120]=0.85f;
 const auto late=locateLoopbackPulse(pulse.data(),pulse.size(),1024,200);
 if(!late.valid || late.lagSamples!=96 ||
    std::abs(alignmentMilliseconds(late.lagSamples,48000.0)-2.0)>1e-9)
    throw std::runtime_error("late loopback pulse detection");
 pulse[1120]=0.0f;
 pulse[928]=-0.7f;
 const auto early=locateLoopbackPulse(pulse.data(),pulse.size(),1024,200);
 if(!early.valid || early.lagSamples!=-96)
    throw std::runtime_error("early loopback pulse detection");
 pulse[928]=0.0f;
 if(locateLoopbackPulse(pulse.data(),pulse.size(),1024,200).valid ||
    locateLoopbackPulse(nullptr,0,0,1).valid)
    throw std::runtime_error("invalid/silent loopback accepted");
 // Repeated loopback pulses tolerate one noisy transient but reject
 // inconsistent measurements instead of applying an unsafe correction.
 std::vector<float> repeated(8192,0.0f);
 const std::vector<std::size_t> pulsePositions{1024,2048,3072,4096,5120};
 for(const auto pos:pulsePositions)repeated[pos+72]=0.9f;
 repeated[4096+72]=0.0f;
 repeated[4096-80]=1.0f; // one deliberate outlier
 auto calibration=estimateLoopbackLatency(repeated.data(),repeated.size(),pulsePositions,200);
 if(!calibration.valid || calibration.lagSamples!=72 || calibration.accepted!=4 ||
    calibration.jitterSamples!=0)
    throw std::runtime_error("repeated loopback estimation");
 // Three matches out of five are insufficient: at least four are required.
 std::vector<float> insufficient(8192,0.0f);
 for(std::size_t i=0;i<3;++i)insufficient[pulsePositions[i]+72]=0.9f;
 if(estimateLoopbackLatency(insufficient.data(),insufficient.size(),pulsePositions,200).valid)
    throw std::runtime_error("accepted calibration below 75 percent pulse threshold");
 repeated.assign(8192,0.0f);
 if(estimateLoopbackLatency(repeated.data(),repeated.size(),pulsePositions,200).valid)
    throw std::runtime_error("silent loopback was calibrated");
 // Reject calibration schedules whose search windows overlap: a single
 // transient must not count as evidence for several expected pulses.
 std::vector<float> overlapSignal(2048,0.0f);
 overlapSignal[520]=0.9f;
 if(estimateLoopbackLatency(overlapSignal.data(),overlapSignal.size(),
      {500,520,540},40).valid)
    throw std::runtime_error("overlapping pulse windows accepted");
 if(estimateLoopbackLatency(overlapSignal.data(),overlapSignal.size(),
      {500,500,900},40).valid)
    throw std::runtime_error("duplicate pulse schedule accepted");
 // Truncated recordings must not be reported as reliable calibration.
 std::vector<float> truncatedSignal(2048,0.0f);
 truncatedSignal[1024]=0.9f;
 truncatedSignal[1536]=0.9f;
 truncatedSignal[2040]=0.9f;
 if(estimateLoopbackLatency(truncatedSignal.data(),truncatedSignal.size(),
      {1024,1536,2040},32).valid)
    throw std::runtime_error("truncated loopback accepted");
 if(estimateLoopbackLatency(truncatedSignal.data(),truncatedSignal.size(),
      {8,512,1024},32).valid)
    throw std::runtime_error("partial initial pulse search accepted");
 RepeatedPulseAlignment stableQuality;
 stableQuality.valid=true;stableQuality.accepted=5;stableQuality.total=5;
 stableQuality.jitterSamples=4;
 if(assessCalibration(stableQuality,48000.0)!=CalibrationQuality::consistent)
    throw std::runtime_error("stable loopback quality");
 stableQuality.accepted=2;
 if(assessCalibration(stableQuality,48000.0)!=CalibrationQuality::invalid)
    throw std::runtime_error("insufficient pulse consensus metadata");
 stableQuality.accepted=5;
 stableQuality.jitterSamples=-1;
 if(assessCalibration(stableQuality,48000.0)!=CalibrationQuality::invalid)
    throw std::runtime_error("negative timing deviation metadata");
 stableQuality.jitterSamples=20;
 if(assessCalibration(stableQuality,48000.0)!=CalibrationQuality::repeat ||
    assessCalibration(stableQuality,0.0)!=CalibrationQuality::invalid)
    throw std::runtime_error("unstable loopback quality");
 // Consensus estimate should remain centered on the accepted lag cluster.
 std::vector<float> clustered(8192,0.0f);
 const std::vector<std::size_t> clusterTimes{1024,2048,3072,4096,5120};
 const int clusterLags[]{70,71,72,73,150};
 for(std::size_t i=0;i<clusterTimes.size();++i)
    clustered[clusterTimes[i]+static_cast<std::size_t>(clusterLags[i])]=0.9f;
 const auto consensus=estimateLoopbackLatency(clustered.data(),clustered.size(),clusterTimes,200);
 if(!consensus.valid || consensus.accepted!=4 || consensus.lagSamples!=72 ||
    consensus.jitterSamples!=2)
    throw std::runtime_error("loopback consensus median and jitter");
 // A valid measurement with moderate jitter should prompt repetition rather
 // than silently authorizing an uncertain overdub correction.
 std::vector<float> variable(8192,0.0f);
 const std::vector<std::size_t> variableTimes{1024,2048,3072,4096,5120};
 const int variableLags[]{60,66,70,72,68};
 for(std::size_t i=0;i<variableTimes.size();++i)
    variable[variableTimes[i]+static_cast<std::size_t>(variableLags[i])]=0.9f;
 const auto unstable=estimateLoopbackLatency(variable.data(),variable.size(),variableTimes,200,12);
 if(!unstable.valid || unstable.jitterSamples<8 ||
    assessCalibration(unstable,48000.0,0.1)!=CalibrationQuality::repeat)
    throw std::runtime_error("unstable valid calibration was not flagged");
 // Exercise the real-time engine using a synthetic device callback.
 IDWLoopbackCapture live;
 if(!live.prepare(48000.0,0,0) || !live.start())
    throw std::runtime_error("loopback capture setup");
 float inputBlock[256]{};
 float outputBlock[256]{};
 const float* captureInputs[]{inputBlock};
 float* captureOutputs[]{outputBlock};
 int pulseCount=0;
 for(int offset=0;offset<38400;offset+=256){
    std::fill(std::begin(outputBlock),std::end(outputBlock),0.0f);
    live.audioDeviceIOCallbackWithContext(captureInputs,1,captureOutputs,1,256,{});
    for(const auto value:outputBlock)if(value>0.2f)++pulseCount;
 }
 if(!live.isComplete() || live.isActive() || pulseCount!=5 ||
    live.recordedSamples()!=38400)
    throw std::runtime_error("live loopback callback pulse schedule");
 // Synthetic one-buffer hardware return: generated pulses should be
 // captured 256 samples late and recovered by the same latency estimator.
 IDWLoopbackCapture liveDelayed;
 if(!liveDelayed.prepare(44100.0,0,0) || !liveDelayed.start())
    throw std::runtime_error("delayed live loopback setup");
 float returnBlock[256]{};
 float sendBlock[256]{};
 const float* returnInputs[]{returnBlock};
 float* sendOutputs[]{sendBlock};
 for(int offset=0;offset<35280;offset+=256){
    std::fill(std::begin(sendBlock),std::end(sendBlock),0.0f);
    liveDelayed.audioDeviceIOCallbackWithContext(returnInputs,1,sendOutputs,1,256,{});
    std::copy(std::begin(sendBlock),std::end(sendBlock),std::begin(returnBlock));
 }
 const std::vector<std::size_t> livePulsePositions{4410,8820,13230,17640,22050};
 const auto liveAlignment=estimateLoopbackLatency(
    liveDelayed.getCapturedBuffer().getReadPointer(0),
    static_cast<std::size_t>(liveDelayed.recordedSamples()),livePulsePositions,512);
 if(!liveDelayed.isComplete() || !liveAlignment.valid ||
    liveAlignment.lagSamples!=256 || liveAlignment.accepted!=5)
    throw std::runtime_error("live loopback simulated return latency");
 // Output pulses must remain within the legal digital signal range.
 IDWLoopbackCapture boundedOutput;
 if(!boundedOutput.prepare(48000.0,0,0) || !boundedOutput.start())
    throw std::runtime_error("bounded calibration setup");
 float boundedInput[256]{};
 float boundedSamples[256];
 const float* boundedInputs[]{boundedInput};
 float* boundedOutputs[]{boundedSamples};
 for(int block=0;block<20;++block){
    std::fill(std::begin(boundedSamples),std::end(boundedSamples),0.95f);
    boundedOutput.audioDeviceIOCallbackWithContext(boundedInputs,1,boundedOutputs,1,256,{});
    for(const float value:boundedSamples)
        if(value>1.0f || value< -1.0f)
            throw std::runtime_error("live loopback digital output exceeded bounds");
 }
 boundedOutput.cancel();
 // Live device interruption must invalidate the capture exactly once.
 IDWLoopbackCapture interrupted;
 if(!interrupted.prepare(48000.0,0,0) || !interrupted.start())
    throw std::runtime_error("interrupted capture setup");
 float interruptedInput[128]{};
 const float* interruptedInputs[]{interruptedInput};
 interrupted.audioDeviceIOCallbackWithContext(interruptedInputs,1,nullptr,0,128,{});
 if(interrupted.isActive() || !interrupted.consumeFailed() || interrupted.consumeFailed() ||
    interrupted.consumeComplete())
    throw std::runtime_error("device interruption did not abort capture once");
 // A device shutdown during live calibration must emit exactly one failure.
 IDWLoopbackCapture stoppedCapture;
 if(!stoppedCapture.prepare(44100.0,0,0) || !stoppedCapture.start())
    throw std::runtime_error("stopped capture setup");
 stoppedCapture.audioDeviceStopped();
 if(stoppedCapture.isActive() || !stoppedCapture.consumeFailed() ||
    stoppedCapture.consumeFailed() || stoppedCapture.consumeComplete())
    throw std::runtime_error("device stop did not invalidate calibration exactly once");
 // Captures with unread status must not be restarted until consumed.
 IDWLoopbackCapture pendingStatus;
 if(!pendingStatus.prepare(48000.0,0,0) || !pendingStatus.start())
    throw std::runtime_error("pending-status capture setup");
 pendingStatus.audioDeviceStopped();
 if(pendingStatus.prepare(48000.0,0,0))
    throw std::runtime_error("prepare erased an unread failed capture");
 if(pendingStatus.start() || !pendingStatus.consumeFailed() || !pendingStatus.start())
    throw std::runtime_error("pending failure should gate restart");
 pendingStatus.cancel();
 // A device-restart notification without a valid device is an aborted test.
 juce::BigInteger enabledInputs;
 enabledInputs.setBit(0);
 enabledInputs.setBit(2);
 enabledInputs.setBit(5);
 if(IDWLoopbackCapture::callbackIndexForChannel(enabledInputs,0)!=0 ||
    IDWLoopbackCapture::callbackIndexForChannel(enabledInputs,2)!=1 ||
    IDWLoopbackCapture::callbackIndexForChannel(enabledInputs,5)!=2 ||
    IDWLoopbackCapture::callbackIndexForChannel(enabledInputs,3)!=-1 ||
    IDWLoopbackCapture::callbackIndexForChannel(enabledInputs,-1)!=-1)
    throw std::runtime_error("noncontiguous physical input channel mapping");
 IDWLoopbackCapture restarted;
 if(!restarted.prepare(48000.0,0,0) || !restarted.start())
    throw std::runtime_error("restarted loopback setup");
 restarted.audioDeviceAboutToStart(nullptr);
 if(restarted.isActive() || !restarted.consumeFailed() || restarted.consumeComplete())
    throw std::runtime_error("device restart failed to invalidate calibration");
 // A restart at the SAME nominal sample rate also breaks continuity.
 IDWLoopbackCapture sameRateRestart;
 if(!sameRateRestart.prepare(48000.0,0,0) || !sameRateRestart.start())
    throw std::runtime_error("same-rate restart setup");
 sameRateRestart.audioDeviceAboutToStart(nullptr);
 if(sameRateRestart.isActive() || !sameRateRestart.consumeFailed())
    throw std::runtime_error("same-rate restart must abort live calibration");
 // Completion is a one-shot notification and should not be reprocessed.
 if(!live.consumeComplete() || live.consumeComplete())
    throw std::runtime_error("completed loopback should only be consumed once");
 // Cancelling a finished test discards any unconsumed completion event.
 IDWLoopbackCapture discarded;
 if(!discarded.prepare(48000.0,0,0) || !discarded.start())
    throw std::runtime_error("cancelled calibration setup");
 discarded.audioDeviceStopped();
 discarded.cancel();
 if(discarded.consumeFailed() || discarded.consumeComplete() || discarded.isActive())
    throw std::runtime_error("cancelled calibration retained stale event");
 Session original;original.setTempo(137);original.addTrack(TrackType::audio);
 original.tracks[0].name="Lead Vocal";
 original.tracks[0].solo=true;
 if(!original.setTrackMix(0,0.75f,-0.25f))throw std::runtime_error("mix");
 if(!original.addClip(0,{"take.wav",8.0,16.0,0.5}))throw std::runtime_error("clip");
 if(!original.moveClip(0,0,12.0) || !original.resizeClip(0,0,8.0))
    throw std::runtime_error("timeline move and resize");
 if(original.moveClip(0,0,-1.0) || original.resizeClip(0,0,0.0) ||
    original.moveClip(0,1,3.0) ||
    original.tracks[0].clips[0].startBeat!=12.0 ||
    original.tracks[0].clips[0].lengthBeats!=8.0)
    throw std::runtime_error("unsafe timeline edit mutated session");
 Session restored;
 if(!sessionFromJson(sessionToJson(original),restored))throw std::runtime_error("roundtrip");
 if(restored.bpm!=137 || restored.tracks.size()!=1 ||
    restored.tracks[0].name!="Lead Vocal" || !restored.tracks[0].solo ||
    restored.tracks[0].clips.size()!=1 || restored.tracks[0].clips[0].startBeat!=12.0 ||
    restored.tracks[0].clips[0].lengthBeats!=8.0 ||
    restored.tracks[0].gain!=0.75f)throw std::runtime_error("data loss");
 auto corrupt=juce::JSON::parse("{\"format\":\"wrong\",\"version\":1,\"tracks\":[]}");
 if(sessionFromJson(corrupt,restored))throw std::runtime_error("accepted invalid format");
 const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
    .getNonexistentChildFile("idw-portable-test","",false);
 if(!root.createDirectory())throw std::runtime_error("test directory");
 // A missing input buffer must be counted, not silently treated as a clean take.
 IDWTakeRecorder captureDiagnostics;
 // Invalid recording setup must not create or overwrite a take file.
 const auto invalidRateTake=root.getChildFile("invalid-rate.wav");
 if(captureDiagnostics.start(invalidRateTake,0.0,1,0) || invalidRateTake.existsAsFile())
    throw std::runtime_error("invalid sample rate created a take");
 for(const double badRate:{-1.0,7999.0,192001.0,std::numeric_limits<double>::infinity()}){
    const auto path=root.getChildFile("out-of-range-rate.wav");
    if(captureDiagnostics.start(path,badRate,1,0) || path.existsAsFile())
       throw std::runtime_error("invalid recording sample rate accepted");
 }
 if(captureDiagnostics.start(root.getChildFile("invalid-channel.wav"),48000.0,1,-1))
    throw std::runtime_error("negative recording input accepted");
 if(!captureDiagnostics.start(root.getChildFile("callback-dropout.wav"),48000.0,1,0))
    throw std::runtime_error("take recorder diagnostics setup");
 captureDiagnostics.audioDeviceIOCallbackWithContext(nullptr,0,nullptr,0,256,{});
 const float sample[256]{};
 const float* missingChannelInputs[]{sample};
 // A selected callback slot outside the provided input span must also count.
 captureDiagnostics.audioDeviceIOCallbackWithContext(missingChannelInputs,0,nullptr,0,256,{});
 if(captureDiagnostics.getDroppedBlocks()!=2 || captureDiagnostics.getRejectedWrites()!=0 ||
    captureDiagnostics.getCapturedFrames()!=0 || captureDiagnostics.getQueuedFrames()!=0)
    throw std::runtime_error("missing microphone blocks incorrectly counted as captured");
 // A failed restart must not erase evidence from the previous recording.
 if(captureDiagnostics.start(root.getChildFile("failed-restart.wav"),0.0,1,0) ||
    captureDiagnostics.getDroppedBlocks()!=2 || captureDiagnostics.getRejectedWrites()!=0)
    throw std::runtime_error("failed restart erased recording diagnostics");
 captureDiagnostics.stop();
 // Starting a new take must clear the previous take's diagnostics.
 if(!captureDiagnostics.start(root.getChildFile("callback-reset.wav"),48000.0,1,0))
    throw std::runtime_error("recorder diagnostic reset setup");
 if(captureDiagnostics.getDroppedBlocks()!=0 || captureDiagnostics.getRejectedWrites()!=0 ||
    captureDiagnostics.getCapturedFrames()!=0 || captureDiagnostics.getQueuedFrames()!=0)
    throw std::runtime_error("recorder counters leaked into next take");
 // Valid input must be accepted without incrementing either error counter.
 const float* validCaptureInputs[]{sample};
 captureDiagnostics.audioDeviceIOCallbackWithContext(validCaptureInputs,1,nullptr,0,256,{});
 captureDiagnostics.audioDeviceIOCallbackWithContext(validCaptureInputs,1,nullptr,0,128,{});
 if(captureDiagnostics.getDroppedBlocks()!=0 || captureDiagnostics.getRejectedWrites()!=0 ||
    captureDiagnostics.getCapturedFrames()!=384 || captureDiagnostics.getQueuedFrames()!=384)
    throw std::runtime_error("valid microphone callback frame accounting");
 captureDiagnostics.stop();
 const auto sourceAudio=root.getChildFile("source.wav");
 // Silence is received audio, unlike a missing channel or a take with no callbacks.
 IDWTakeRecorder inputDiagnostics;
 const auto emptyTake=root.getChildFile("no-callbacks.wav");
 if(!inputDiagnostics.start(emptyTake,8000.0))throw std::runtime_error("empty take setup");
 inputDiagnostics.stop();
 juce::WavAudioFormat diagnosticWav;
 auto emptyReader=std::unique_ptr<juce::AudioFormatReader>(
     diagnosticWav.createReaderFor(emptyTake.createInputStream().release(),true));
 if(!emptyReader || emptyReader->lengthInSamples!=0 || inputDiagnostics.getCapturedFrames()!=0 ||
    inputDiagnostics.isInputMissing() || inputDiagnostics.wasDeviceInterrupted() || inputDiagnostics.hasSustainedSilence())
    throw std::runtime_error("no callbacks misclassified as silence or disconnect");
 emptyReader.reset();
 if(!inputDiagnostics.start(root.getChildFile("silent-input.wav"),8000.0))
    throw std::runtime_error("silent take setup");
 std::vector<float> silence(24000,0.0f);
 const float* silentChannels[]{silence.data()};
 inputDiagnostics.audioDeviceIOCallbackWithContext(silentChannels,1,nullptr,0,23999,{});
 if(inputDiagnostics.hasSustainedSilence())throw std::runtime_error("brief pause flagged as sustained silence");
 inputDiagnostics.audioDeviceIOCallbackWithContext(silentChannels,1,nullptr,0,1,{});
 if(!inputDiagnostics.hasSustainedSilence() || inputDiagnostics.isInputMissing() ||
    inputDiagnostics.getCapturedFrames()!=24000 || inputDiagnostics.getTakePeak()!=0.0f)
    throw std::runtime_error("three seconds of valid silent input not recognized");
 const float tone[]{0.25f};
 const float* toneChannels[]{tone};
 inputDiagnostics.audioDeviceIOCallbackWithContext(toneChannels,1,nullptr,0,1,{});
 if(inputDiagnostics.hasSustainedSilence() || inputDiagnostics.getTakePeak()!=0.25f)
    throw std::runtime_error("signal did not clear silence");
 inputDiagnostics.consumeInputPeak();
 if(inputDiagnostics.getTakePeak()!=0.25f)throw std::runtime_error("meter erased whole-take signal evidence");
 const float* nullChannels[]{nullptr};
 inputDiagnostics.audioDeviceIOCallbackWithContext(nullChannels,1,nullptr,0,1,{});
 if(!inputDiagnostics.isInputMissing() || inputDiagnostics.getMissingInputBlocks()!=1 ||
    inputDiagnostics.getCapturedFrames()!=24001 || inputDiagnostics.hasSustainedSilence())
    throw std::runtime_error("missing input classified as captured silence");
 inputDiagnostics.audioDeviceIOCallbackWithContext(toneChannels,1,nullptr,0,1,{});
 if(inputDiagnostics.isInputMissing() || inputDiagnostics.getMissingInputBlocks()!=1)
    throw std::runtime_error("input recovery lost historical diagnostic evidence");
 inputDiagnostics.audioDeviceStopped();
 inputDiagnostics.audioDeviceAboutToStart(nullptr);
 inputDiagnostics.audioDeviceIOCallbackWithContext(toneChannels,1,nullptr,0,1,{});
 if(!inputDiagnostics.isRecording() || !inputDiagnostics.wasDeviceInterrupted() ||
    inputDiagnostics.getCapturedFrames()!=24002)
    throw std::runtime_error("device interruption lost pending take or appended restarted input");
 inputDiagnostics.stop();
 auto partialReader=std::unique_ptr<juce::AudioFormatReader>(
     diagnosticWav.createReaderFor(inputDiagnostics.file().createInputStream().release(),true));
 if(!partialReader || partialReader->lengthInSamples!=static_cast<juce::int64>(inputDiagnostics.getQueuedFrames()))
    throw std::runtime_error("interrupted take was not flushed");
 partialReader.reset();
 if(inputDiagnostics.start(root.getChildFile("bad-restart.wav"),0.0) || !inputDiagnostics.wasDeviceInterrupted())
    throw std::runtime_error("failed start erased interruption");
 if(!inputDiagnostics.start(root.getChildFile("new-diagnostics.wav"),48000.0) ||
    inputDiagnostics.wasDeviceInterrupted() || inputDiagnostics.isInputMissing() ||
    inputDiagnostics.getMissingInputBlocks()!=0 || inputDiagnostics.hasSustainedSilence() ||
    inputDiagnostics.getTakePeak()!=0.0f)
    throw std::runtime_error("new take inherited stale input diagnostics");
 inputDiagnostics.audioDeviceAboutToStart(nullptr);
 if(!inputDiagnostics.wasDeviceInterrupted())throw std::runtime_error("restart without stop not detected");
 inputDiagnostics.stop();
 // Construct a genuine 16-bit PCM WAV fixture rather than copying arbitrary bytes.
 juce::WavAudioFormat wav;
 auto stream=std::unique_ptr<juce::FileOutputStream>(sourceAudio.createOutputStream());
 if(!stream)throw std::runtime_error("fixture stream");
 auto writer=std::unique_ptr<juce::AudioFormatWriter>(
    wav.createWriterFor(stream.get(),44100.0,1,16,{},0));
 if(!writer)throw std::runtime_error("fixture writer");
 stream.release();
 juce::AudioBuffer<float> audioFixture(1,4410);
 audioFixture.clear();
 audioFixture.setSample(0,100,0.5f);
 const float* channels[]{audioFixture.getReadPointer(0)};
 if(!writer->writeFromFloatArrays(channels,1,4410))throw std::runtime_error("fixture write");
 writer.reset();
 Session portableSource;portableSource.addTrack(TrackType::audio);
 if(!portableSource.addClip(0,{sourceAudio.getFullPathName().toStdString(),4.0,8.0,0.0}))
    throw std::runtime_error("portable clip");
 juce::String error;
 const auto package=root.getChildFile("Package");
 if(!exportPortableProject(portableSource,package,error))
    throw std::runtime_error("export failed: "+error.toStdString());
 const auto moved=root.getChildFile("Moved");
 if(!package.moveFileTo(moved))throw std::runtime_error("move");
 Session reopened;
 if(!loadSession(moved.getChildFile("Session.idwproj"),reopened))
    throw std::runtime_error("read moved project");
 const auto relative=juce::String(reopened.tracks[0].clips[0].sourcePath);
 if(juce::File::isAbsolutePath(relative) || !moved.getChildFile(relative).existsAsFile())
    throw std::runtime_error("broken portable audio reference");
 auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(
    moved.getChildFile(relative).createInputStream().release(),true));
 if(!reader || reader->lengthInSamples!=4410 || reader->sampleRate!=44100.0)
    throw std::runtime_error("packaged WAV unreadable");
 // Exercise the WAV-file analysis path, not just raw memory buffers.
 const auto loopbackFile=root.getChildFile("loopback-test.wav");
 auto loopStream=std::unique_ptr<juce::FileOutputStream>(loopbackFile.createOutputStream());
 if(!loopStream)throw std::runtime_error("loopback fixture stream");
 auto loopWriter=std::unique_ptr<juce::AudioFormatWriter>(
    wav.createWriterFor(loopStream.get(),48000.0,1,16,{},0));
 if(!loopWriter)throw std::runtime_error("loopback fixture writer");
 loopStream.release();
 juce::AudioBuffer<float> recorded(1,8192);
 recorded.clear();
 const std::vector<std::size_t> wavPulsePositions{1024,2048,3072,4096,5120};
 for(auto pos:wavPulsePositions)recorded.setSample(0,(int)pos+72,0.85f);
 const float* recordedChannels[]{recorded.getReadPointer(0)};
 if(!loopWriter->writeFromFloatArrays(recordedChannels,1,8192))
    throw std::runtime_error("loopback fixture write");
 loopWriter.reset();
 const auto measured=analyzeLoopbackWav(loopbackFile,0,wavPulsePositions,150);
 if(!measured.alignment.valid || measured.alignment.lagSamples!=72 ||
    std::abs(measured.lagMilliseconds-1.5)>0.01)
    throw std::runtime_error("WAV loopback analysis failed");
 if(analyzeLoopbackWav(loopbackFile,3,wavPulsePositions,150).alignment.valid)
    throw std::runtime_error("invalid WAV channel accepted");
 const auto referenceFile=root.getChildFile("reference-pulses.wav");
 if(!writeLoopbackReference(referenceFile,48000.0,{100.0,200.0,300.0,400.0,500.0}))
    throw std::runtime_error("loopback reference generator");
 auto referenceReader=std::unique_ptr<juce::AudioFormatReader>(
    wav.createReaderFor(referenceFile.createInputStream().release(),true));
 if(!referenceReader || referenceReader->sampleRate!=48000.0 ||
    referenceReader->lengthInSamples<24000)
    throw std::runtime_error("invalid generated loopback WAV");
 juce::AudioBuffer<float> referenceSamples(1,static_cast<int>(referenceReader->lengthInSamples));
 referenceSamples.clear();
 if(!referenceReader->read(&referenceSamples,0,referenceSamples.getNumSamples(),0,true,true))
    throw std::runtime_error("could not read reference pulses");
 for(int ms: {100,200,300,400,500}){
    const int frame=ms*48;
    if(std::abs(referenceSamples.getSample(0,frame)-0.25f)>0.001f)
       throw std::runtime_error("reference pulse missing or misplaced");
    if(std::abs(referenceSamples.getSample(0,frame-1))>0.001f)
       throw std::runtime_error("reference pulse unexpectedly early");
 }
 if(std::abs(referenceSamples.getSample(0,1000))>0.001f)
    throw std::runtime_error("reference silence not preserved");

 // End-to-end: generated WAV is zero-lag against its own pulse schedule.
 const std::vector<std::size_t> referencePositions{4800,9600,14400,19200,24000};
 referenceReader.reset();
 const auto noLag=analyzeLoopbackWav(referenceFile,0,referencePositions,128);
 if(!noLag.alignment.valid || noLag.alignment.lagSamples!=0 ||
    noLag.alignment.accepted!=5 || std::abs(noLag.lagMilliseconds)>1.0e-10)
    throw std::runtime_error("generated reference WAV must have zero pulse lag");
 // Simulate a physical return with 120 samples (2.5 ms) of extra latency.
 const auto delayedFile=root.getChildFile("delayed-reference.wav");
 auto delayedStream=std::unique_ptr<juce::FileOutputStream>(delayedFile.createOutputStream());
 if(!delayedStream)throw std::runtime_error("delayed fixture stream");
 auto delayedWriter=std::unique_ptr<juce::AudioFormatWriter>(
    wav.createWriterFor(delayedStream.get(),48000.0,1,16,{},0));
 if(!delayedWriter)throw std::runtime_error("delayed fixture writer");
 delayedStream.release();
 juce::AudioBuffer<float> delayedSamples(1,30000);
 delayedSamples.clear();
 for(const auto position:referencePositions)
    delayedSamples.setSample(0,static_cast<int>(position+120),0.25f);
 const float* delayedChannels[]{delayedSamples.getReadPointer(0)};
 if(!delayedWriter->writeFromFloatArrays(delayedChannels,1,30000))
    throw std::runtime_error("delayed fixture write");
 delayedWriter.reset();
 const auto delayedMeasurement=analyzeLoopbackWav(delayedFile,0,referencePositions,200);
 if(!delayedMeasurement.alignment.valid || delayedMeasurement.alignment.lagSamples!=120 ||
    std::abs(delayedMeasurement.lagMilliseconds-2.5)>0.01)
    throw std::runtime_error("generated pulse delayed capture analysis");
 // Verify the same offline calibration path at 44.1 kHz. Timing must
 // use the recording's actual sample rate rather than assuming 48 kHz.
 const auto lowerRateFile=root.getChildFile("loopback-44100.wav");
 auto lowerRateStream=std::unique_ptr<juce::FileOutputStream>(lowerRateFile.createOutputStream());
 if(!lowerRateStream)throw std::runtime_error("44.1 kHz fixture stream");
 auto lowerRateWriter=std::unique_ptr<juce::AudioFormatWriter>(
    wav.createWriterFor(lowerRateStream.get(),44100.0,1,16,{},0));
 if(!lowerRateWriter)throw std::runtime_error("44.1 kHz fixture writer");
 lowerRateStream.release();
 juce::AudioBuffer<float> lowerRateSamples(1,30000);
 lowerRateSamples.clear();
 const std::vector<std::size_t> lowerRatePositions{4410,8820,13230,17640,22050};
 for(const auto position:lowerRatePositions)
    lowerRateSamples.setSample(0,static_cast<int>(position+88),0.5f);
 const float* lowerRateChannels[]{lowerRateSamples.getReadPointer(0)};
 if(!lowerRateWriter->writeFromFloatArrays(lowerRateChannels,1,30000))
    throw std::runtime_error("44.1 kHz fixture write");
 lowerRateWriter.reset();
 const auto lowerRateMeasured=analyzeLoopbackWav(lowerRateFile,0,lowerRatePositions,150);
 if(!lowerRateMeasured.alignment.valid || lowerRateMeasured.alignment.lagSamples!=88 ||
    std::abs(lowerRateMeasured.lagMilliseconds-1000.0*88.0/44100.0)>0.01)
    throw std::runtime_error("44.1 kHz delay analysis");
 // A missing WAV must fail cleanly rather than dereferencing a stream.
 const auto missingLoopback=analyzeLoopbackWav(root.getChildFile("not-recorded.wav"),
     0,{4800,9600,14400},128);
 if(missingLoopback.alignment.valid || missingLoopback.sampleRate!=0.0)
    throw std::runtime_error("missing calibration WAV accepted");
 // Malformed pulse schedules should fail before audio decoding.
 const auto invalidPlan=analyzeLoopbackWav(referenceFile,0,{4800,4800,14400},128);
 if(invalidPlan.alignment.valid || invalidPlan.sampleRate!=0.0)
    throw std::runtime_error("duplicate pulse schedule not rejected before WAV decode");
 const auto tooClose=analyzeLoopbackWav(referenceFile,0,{4800,4900,14400},128);
 if(tooClose.alignment.valid || tooClose.sampleRate!=0.0)
    throw std::runtime_error("overlapping pulse window not rejected before WAV decode");
 root.deleteRecursively();
 std::cout<<"PASS: session project persistence\n";
}
