#include "SessionPersistence.h"
#include "AIProposal.h"
#include "AIStatusSnapshot.h"
#include "MidiScheduler.h"
#include "MidiSineSynth.h"
#include "ProjectPackaging.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <stdexcept>
#include <iostream>
int main(){
 using namespace idw::daw;
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
 if(!midiFixture.setTrackSynthPreset(0,2) || midiFixture.setTrackSynthPreset(0,3))
    throw std::runtime_error("MIDI preset validation");
 if(!midiFixture.setTrackSynthEnvelope(0,25.0f,90.0f) ||
    midiFixture.setTrackSynthEnvelope(0,-1.0f,50.0f))
    throw std::runtime_error("MIDI envelope validation");
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
 const auto sourceAudio=root.getChildFile("source.wav");
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
 root.deleteRecursively();
 std::cout<<"PASS: session project persistence\n";
}
