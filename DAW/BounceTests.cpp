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
 // Verify master gain and output ceiling are applied to the exported samples.
 const auto controlledOutput=root.getChildFile("master-controlled.wav");
 if(!renderSessionWav(stereoSession,controlledOutput,error,44100,0.5f,-6.0f,0.0f,0.0f))
    throw std::runtime_error("master bounce: "+error.toStdString());
 auto controlledReader=std::unique_ptr<juce::AudioFormatReader>(
    wav.createReaderFor(controlledOutput.createInputStream().release(),true));
 if(!controlledReader)throw std::runtime_error("master reader");
 juce::AudioBuffer<float> controlledAudio(2,256);
 if(!controlledReader->read(&controlledAudio,0,256,0,true,true))
    throw std::runtime_error("master read");
 if(std::abs(controlledAudio.getSample(0,100)-0.1f)>0.01f ||
    std::abs(controlledAudio.getSample(1,100)+0.1f)>0.01f)
    throw std::runtime_error("master gain was not applied");
 controlledReader.reset();
 // Verify the output ceiling clamps a louder stereo source.
 const auto limitedOutput=root.getChildFile("ceiling-controlled.wav");
 if(!renderSessionWav(stereoSession,limitedOutput,error,44100,5.0f,-12.0f,0.0f,0.0f))
    throw std::runtime_error("ceiling bounce: "+error.toStdString());
 auto limitedReader=std::unique_ptr<juce::AudioFormatReader>(
    wav.createReaderFor(limitedOutput.createInputStream().release(),true));
 if(!limitedReader)throw std::runtime_error("ceiling reader");
 juce::AudioBuffer<float> limitedAudio(2,256);
 if(!limitedReader->read(&limitedAudio,0,256,0,true,true))
    throw std::runtime_error("ceiling read");
 const float ceiling=std::pow(10.0f,-12.0f/20.0f);
 if(std::abs(limitedAudio.getSample(0,100)-ceiling)>0.01f ||
    std::abs(limitedAudio.getSample(1,100)+ceiling)>0.01f)
    throw std::runtime_error("master ceiling was not applied");
 limitedReader.reset();
 // Regress a 48 kHz source resampled to 44.1 kHz across several
 // processing blocks, checking the output amplitude and DC stability.
 const auto longInput=root.getChildFile("long-48k.wav");
 auto longStream=std::unique_ptr<juce::FileOutputStream>(longInput.createOutputStream());
 if(!longStream)throw std::runtime_error("long input stream");
 auto longWriter=std::unique_ptr<juce::AudioFormatWriter>(
    wav.createWriterFor(longStream.get(),48000.0,1,16,{},0));
 if(!longWriter)throw std::runtime_error("long input writer");
 longStream.release();
 juce::AudioBuffer<float> longSignal(1,48000);
 for(int i=0;i<48000;++i)longSignal.setSample(0,i,0.25f);
 const float* longChannels[]{longSignal.getReadPointer(0)};
 if(!longWriter->writeFromFloatArrays(longChannels,1,48000))
    throw std::runtime_error("long input write");
 longWriter.reset();
 Session longSession;longSession.setTempo(120);longSession.addTrack(TrackType::audio);
 if(!longSession.addClip(0,{longInput.getFullPathName().toStdString(),0.0,2.0,0.0}))
    throw std::runtime_error("long clip");
 const auto longOutput=root.getChildFile("long-resampled.wav");
 if(!renderSessionWav(longSession,longOutput,error))
    throw std::runtime_error("long bounce: "+error.toStdString());
 auto longReader=std::unique_ptr<juce::AudioFormatReader>(
    wav.createReaderFor(longOutput.createInputStream().release(),true));
 if(!longReader || longReader->lengthInSamples!=44100)
    throw std::runtime_error("resampling duration");
 juce::AudioBuffer<float> longResult(2,44100);
 if(!longReader->read(&longResult,0,44100,0,true,true))
    throw std::runtime_error("long read");
 for(int i=512;i<43500;i+=997)
    if(std::abs(longResult.getSample(0,i)-0.25f)>0.01f ||
       std::abs(longResult.getSample(1,i)-0.25f)>0.01f)
        throw std::runtime_error("resampling continuity across blocks");
 longReader.reset();
 // Test downsampling a high-frequency alternating waveform. A basic
 // linear resampler may alias; this test records the output RMS so future
 // anti-alias improvements have an explicit quality target.
 const auto hfInput=root.getChildFile("high-frequency.wav");
 auto hfStream=std::unique_ptr<juce::FileOutputStream>(hfInput.createOutputStream());
 if(!hfStream)throw std::runtime_error("HF input stream");
 auto hfWriter=std::unique_ptr<juce::AudioFormatWriter>(
    wav.createWriterFor(hfStream.get(),48000.0,1,16,{},0));
 if(!hfWriter)throw std::runtime_error("HF writer");
 hfStream.release();
 juce::AudioBuffer<float> hfSignal(1,4800);
 for(int i=0;i<4800;++i)hfSignal.setSample(0,i,(i%2==0)?0.4f:-0.4f);
 const float* hfChannels[]{hfSignal.getReadPointer(0)};
 if(!hfWriter->writeFromFloatArrays(hfChannels,1,4800))
    throw std::runtime_error("HF write");
 hfWriter.reset();
 Session hfSession;hfSession.setTempo(120);hfSession.addTrack(TrackType::audio);
 if(!hfSession.addClip(0,{hfInput.getFullPathName().toStdString(),0.0,0.2,0.0}))
    throw std::runtime_error("HF clip");
 const auto hfOutput=root.getChildFile("high-frequency-out.wav");
 if(!renderSessionWav(hfSession,hfOutput,error))
    throw std::runtime_error("HF bounce: "+error.toStdString());
 auto hfReader=std::unique_ptr<juce::AudioFormatReader>(
    wav.createReaderFor(hfOutput.createInputStream().release(),true));
 if(!hfReader || hfReader->lengthInSamples!=4410)
    throw std::runtime_error("HF output duration");
 juce::AudioBuffer<float> hfResult(2,4410);
 if(!hfReader->read(&hfResult,0,4410,0,true,true))
    throw std::runtime_error("HF output read");
 const float hfRms=hfResult.getRMSLevel(0,128,4096);
 // 24 kHz is above the 22.05 kHz output Nyquist frequency: the
 // downsampler should attenuate this alternating input substantially.
 if(!std::isfinite(hfRms) || hfRms>0.08f)
    throw std::runtime_error("HF alias rejection inadequate");
 hfReader.reset();
 // The two-band master tone must modify low-frequency content predictably.
 const auto bassOutput=root.getChildFile("bass-boost.wav");
 if(!renderSessionWav(stereoSession,bassOutput,error,44100,1.0f,-0.1f,6.0f,0.0f))
    throw std::runtime_error("bass EQ bounce: "+error.toStdString());
 auto bassReader=std::unique_ptr<juce::AudioFormatReader>(
    wav.createReaderFor(bassOutput.createInputStream().release(),true));
 if(!bassReader)throw std::runtime_error("bass reader");
 juce::AudioBuffer<float> bassAudio(2,1024);
 if(!bassReader->read(&bassAudio,0,1024,0,true,true))
    throw std::runtime_error("bass read");
 if(bassAudio.getSample(0,800)<0.30f || bassAudio.getSample(1,800)>-0.30f)
    throw std::runtime_error("master bass boost was not applied");
 bassReader.reset();
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
