#include "OfflineBounce.h"
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <atomic>
int main(){
 using namespace idw::daw;
 auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-bounce-test","",false);
 if(!root.createDirectory())throw std::runtime_error("temp directory");
 const auto input=root.getChildFile("source.wav"),output=root.getChildFile("result.wav");
 juce::WavAudioFormat wav;
 auto stream=std::unique_ptr<juce::FileOutputStream>(input.createOutputStream());
 if(!stream)throw std::runtime_error("input stream");
 auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(),48000.0,1,16,{},0));
 if(!writer)throw std::runtime_error("input writer");
 stream.release();
 juce::AudioBuffer<float> audio(1,4800);audio.clear();
 for(int i=0;i<4800;++i)audio.setSample(0,i,0.25f);
 const float* ptrs[]{audio.getReadPointer(0)};
 if(!writer->writeFromFloatArrays(ptrs,1,4800))throw std::runtime_error("write input");
 writer.reset();
 // Mix a second source recorded at 44.1 kHz with the 48 kHz take.
 const auto second=root.getChildFile("second.wav");
 auto secondStream=std::unique_ptr<juce::FileOutputStream>(second.createOutputStream());
 if(!secondStream)throw std::runtime_error("second stream");
 auto secondWriter=std::unique_ptr<juce::AudioFormatWriter>(
    wav.createWriterFor(secondStream.get(),44100.0,1,16,{},0));
 if(!secondWriter)throw std::runtime_error("second writer");
 secondStream.release();
 juce::AudioBuffer<float> secondAudio(1,4410);
 for(int i=0;i<4410;++i)secondAudio.setSample(0,i,0.125f);
 const float* secondPtrs[]{secondAudio.getReadPointer(0)};
 if(!secondWriter->writeFromFloatArrays(secondPtrs,1,4410))throw std::runtime_error("second write");
 secondWriter.reset();
 Session session;session.setTempo(120);session.addTrack(TrackType::audio);
 if(!session.addClip(0,{input.getFullPathName().toStdString(),1.0,0.2,0.0}))
  throw std::runtime_error("clip");
 session.addTrack(TrackType::audio);
 if(!session.addClip(1,{second.getFullPathName().toStdString(),1.0,0.2,0.0}))
  throw std::runtime_error("second clip");
 juce::String error;
 std::atomic<float> completedProgress{0.0f};
 std::atomic<bool> notCanceled{false};
 if(!renderSessionWav(session,output,error,44100,1.0f,-1.0f,0.0f,0.0f,
                      &completedProgress,&notCanceled))
    throw std::runtime_error(error.toStdString());
 if(std::abs(completedProgress.load()-1.0f)>0.00001f)
    throw std::runtime_error("export did not report 100 percent progress");
 auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(output.createInputStream().release(),true));
 if(!reader || reader->numChannels!=2 || reader->sampleRate!=44100 || reader->lengthInSamples!=26460)
  throw std::runtime_error("output metadata");
 juce::AudioBuffer<float> rendered(2,26460);
 if(!reader->read(&rendered,0,26460,0,true,true))throw std::runtime_error("read output");
 if(std::abs(rendered.getSample(0,100))>0.00001f || std::abs(rendered.getSample(0,23000)-0.375f)>0.01f)
  throw std::runtime_error("timeline alignment or signal level");
 // Verify a stereo source survives export without channel collapse.
 const auto stereo=root.getChildFile("stereo.wav"),stereoOutput=root.getChildFile("stereo-out.wav");
 auto stereoStream=std::unique_ptr<juce::FileOutputStream>(stereo.createOutputStream());
 if(!stereoStream)throw std::runtime_error("stereo stream");
 auto stereoWriter=std::unique_ptr<juce::AudioFormatWriter>(
    wav.createWriterFor(stereoStream.get(),44100.0,2,16,{},0));
 if(!stereoWriter)throw std::runtime_error("stereo writer");
 stereoStream.release();
 juce::AudioBuffer<float> stereoAudio(2,4410);
 for(int i=0;i<4410;++i){stereoAudio.setSample(0,i,0.2f);stereoAudio.setSample(1,i,-0.2f);}
 const float* stereoPtrs[]{stereoAudio.getReadPointer(0),stereoAudio.getReadPointer(1)};
 if(!stereoWriter->writeFromFloatArrays(stereoPtrs,2,4410))throw std::runtime_error("stereo write");
 stereoWriter.reset();
 Session stereoSession;stereoSession.setTempo(120);stereoSession.addTrack(TrackType::audio);
 if(!stereoSession.addClip(0,{stereo.getFullPathName().toStdString(),0.0,0.2,0.0}))
    throw std::runtime_error("stereo clip");
 if(!renderSessionWav(stereoSession,stereoOutput,error))
    throw std::runtime_error("stereo bounce: "+error.toStdString());
 auto stereoReader=std::unique_ptr<juce::AudioFormatReader>(
    wav.createReaderFor(stereoOutput.createInputStream().release(),true));
 if(!stereoReader)throw std::runtime_error("stereo output reader");
 juce::AudioBuffer<float> stereoResult(2,4410);
 if(!stereoReader->read(&stereoResult,0,4410,0,true,true))
    throw std::runtime_error("stereo output read");
 if(std::abs(stereoResult.getSample(0,100)-0.2f)>0.01f ||
    std::abs(stereoResult.getSample(1,100)+0.2f)>0.01f)
    throw std::runtime_error("stereo channel mismatch");
 stereoReader.reset();
 // Check the last sample before a clip and the first sample of the clip,
 // including a transition that lands inside an export processing block.
 const auto boundaryOutput=root.getChildFile("boundary.wav");
 Session boundary;boundary.setTempo(120);boundary.addTrack(TrackType::audio);
 const double startBeat=1025.0/22050.0; // sample 1025 at 44.1kHz, 120 BPM
 if(!boundary.addClip(0,{stereo.getFullPathName().toStdString(),startBeat,0.2,0.0}))
    throw std::runtime_error("boundary clip");
 if(!renderSessionWav(boundary,boundaryOutput,error))
    throw std::runtime_error("boundary bounce: "+error.toStdString());
 auto boundaryReader=std::unique_ptr<juce::AudioFormatReader>(
    wav.createReaderFor(boundaryOutput.createInputStream().release(),true));
 if(!boundaryReader)throw std::runtime_error("boundary reader");
 juce::AudioBuffer<float> boundaryAudio(2,1030);
 if(!boundaryReader->read(&boundaryAudio,0,1030,0,true,true))
    throw std::runtime_error("boundary read");
 if(std::abs(boundaryAudio.getSample(0,1024))>0.00001f ||
    std::abs(boundaryAudio.getSample(0,1025)-0.2f)>0.01f ||
    std::abs(boundaryAudio.getSample(1,1025)+0.2f)>0.01f)
    throw std::runtime_error("clip entry not sample aligned");
 boundaryReader.reset();
 // A source offset near end-of-file must contribute valid audio first,
 // then silence after the available source frames are exhausted.
 const auto tailOutput=root.getChildFile("tail.wav");
 Session tail;tail.setTempo(120);tail.addTrack(TrackType::audio);
 if(!tail.addClip(0,{stereo.getFullPathName().toStdString(),0.0,0.2,0.09}))
    throw std::runtime_error("tail clip");
 if(!renderSessionWav(tail,tailOutput,error))
    throw std::runtime_error("tail bounce: "+error.toStdString());
 auto tailReader=std::unique_ptr<juce::AudioFormatReader>(
    wav.createReaderFor(tailOutput.createInputStream().release(),true));
 if(!tailReader)throw std::runtime_error("tail reader");
 juce::AudioBuffer<float> tailAudio(2,4410);
 if(!tailReader->read(&tailAudio,0,4410,0,true,true))
    throw std::runtime_error("tail read");
 if(std::abs(tailAudio.getSample(0,100)-0.2f)>0.01f ||
    std::abs(tailAudio.getSample(0,2000))>0.0001f)
    throw std::runtime_error("end-of-source padding incorrect");
 tailReader.reset();
 // Cancellation must return failure without leaving an incomplete WAV behind.
 const auto canceledOutput=root.getChildFile("canceled.wav");
 std::atomic<float> progress{0.0f};
 std::atomic<bool> cancel{true};
 error.clear();
 if(renderSessionWav(session,canceledOutput,error,44100,1.0f,-1.0f,0.0f,0.0f,&progress,&cancel))
    throw std::runtime_error("accepted canceled bounce");
 if(canceledOutput.existsAsFile() || error!="Export canceled.")
    throw std::runtime_error("canceled bounce left an output");
 reader.reset();root.deleteRecursively();
 std::cout<<"PASS: stereo WAV bounce timeline and signal test\n";
}
