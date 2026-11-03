#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <cmath>
#include <cstring>
namespace idw::daw {
struct RecordingDeviceReadiness {
    bool open=false,rateMatches=false,outputActive=false;
    double sampleRate=0;
    bool captureReady() const {return open && rateMatches;}
};
inline RecordingDeviceReadiness assessRecordingDevice(bool present,bool open,double actualRate,double engineRate,bool outputs){
    const bool live=present && open;
    return {live,live && std::isfinite(actualRate) && actualRate>0 && std::isfinite(engineRate) && engineRate>0 &&
        std::abs(actualRate-engineRate)<0.000001,live && outputs,live?actualRate:0};
}
inline RecordingDeviceReadiness inspectRecordingDevice(juce::AudioIODevice* device,double engineRate){
    if(!device || !device->isOpen())return {};
    return assessRecordingDevice(true,true,device->getCurrentSampleRate(),engineRate,
                                 device->getActiveOutputChannels().countNumberOfSetBits()>0);
}
struct RecordingPreflightChecks {
    bool audioTrack=false,armed=false,emptyTrack=false,inputReady=false,folderWritable=false;
    RecordingDeviceReadiness device;
    juce::int64 freeBytes=-1;
    bool passes() const {
        return audioTrack && armed && emptyTrack && inputReady && device.captureReady() && device.outputActive &&
            folderWritable && freeBytes>=100LL*1024LL*1024LL;
    }
};
struct RecordingFolderProbe {bool writable=false;juce::String detail;};
namespace preflight_detail {
// Fresh temporary marker only; never overwrite a take or publish a final file.
template<class Writer> RecordingFolderProbe probeFolder(const juce::File& folder,Writer&& writer){try{
    if(folder==juce::File{} || !folder.createDirectory().wasOk() || !folder.isDirectory())return {false,"cannot create recording folder"};
    juce::TemporaryFile probe(folder.getNonexistentChildFile(".idw-preflight",".tmp",false));
    const juce::String marker("IDW recording preflight\n");
    const auto count=(size_t)marker.getNumBytesAsUTF8();
    if(!writer(probe.getFile(),marker) || probe.getFile().getSize()!=(juce::int64)count)return {false,"temporary write/flush failed"};
    juce::MemoryBlock bytes;
    if(!probe.getFile().loadFileAsData(bytes) || bytes.getSize()!=count || std::memcmp(bytes.getData(),marker.toRawUTF8(),count)!=0)
        return {false,"temporary write readback failed"};
    if(!probe.getFile().deleteFile())return {false,"temporary write cleanup failed"};
    return {true,"temporary write, flush, readback and cleanup passed"};
}catch(...){return {false,"temporary write test failed"};}}
}
// Only called from the message thread during an explicit PREFLIGHT check.
inline RecordingFolderProbe probeRecordingFolder(const juce::File& folder){
    return preflight_detail::probeFolder(folder,[](const juce::File& file,const juce::String& text){
        auto output=file.createOutputStream();if(!output)return false;
        const bool wrote=output->write(text.toRawUTF8(),(size_t)text.getNumBytesAsUTF8());
        output->flush();return wrote && output->getStatus().wasOk();
    });
}
}
