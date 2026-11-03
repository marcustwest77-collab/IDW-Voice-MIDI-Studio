#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <cmath>
#include <cstring>
#include <array>
#include "SessionModel.h"
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
struct ArmedRecordingTarget {
    size_t track=0;int physicalInput=-1,callbackInput=-1;
    juce::String name,inputName;
    bool empty=false,routeValid=false,uniqueInput=true;
};
struct ArmedRecordingTargets {
    std::vector<ArmedRecordingTarget> tracks;
    bool allEmpty=true,routesValid=true,distinctInputs=true,withinCapacity=true;
    bool ready() const {return !tracks.empty() && allEmpty && routesValid && distinctInputs && withinCapacity;}
};
// Same armed-audio-track scope as RECORD, independent of which track the user
// currently has selected. Inspection never arms, reroutes or changes a track.
inline ArmedRecordingTargets inspectArmedRecordingTargets(const Session& session,const std::array<bool,128>& armed,
        const juce::StringArray& inputNames,const juce::BigInteger& activeInputs,size_t capacity){
    ArmedRecordingTargets result;
    for(size_t i=0;i<session.tracks.size() && i<armed.size();++i){
        const auto& track=session.tracks[i];if(!armed[i] || track.type!=TrackType::audio)continue;
        ArmedRecordingTarget target;target.track=i;target.name=juce::String(track.name);
        target.physicalInput=track.inputChannel;target.empty=track.clips.empty();
        target.routeValid=target.physicalInput>=0 && target.physicalInput<inputNames.size() && activeInputs[target.physicalInput];
        if(target.routeValid){
            target.inputName=inputNames[target.physicalInput];target.callbackInput=0;
            for(int channel=0;channel<target.physicalInput;++channel)if(activeInputs[channel])++target.callbackInput;
        }
        result.allEmpty=result.allEmpty && target.empty;result.routesValid=result.routesValid && target.routeValid;
        result.tracks.push_back(std::move(target));
    }
    for(size_t i=0;i<result.tracks.size();++i)for(size_t j=0;j<i;++j)
        if(result.tracks[i].physicalInput>=0 && result.tracks[i].physicalInput==result.tracks[j].physicalInput){
            result.tracks[i].uniqueInput=false;result.tracks[j].uniqueInput=false;result.distinctInputs=false;
        }
    result.withinCapacity=result.tracks.size()<=capacity;return result;
}
inline RecordingPreflightChecks recordingPreflightForTargets(const ArmedRecordingTargets& targets,
        const RecordingDeviceReadiness& device,bool writable,juce::int64 freeBytes){
    RecordingPreflightChecks checks;
    checks.audioTrack=!targets.tracks.empty();checks.armed=!targets.tracks.empty();checks.emptyTrack=checks.armed && targets.allEmpty;
    checks.inputReady=checks.armed && targets.routesValid && targets.distinctInputs && targets.withinCapacity;
    checks.folderWritable=writable;checks.device=device;checks.freeBytes=freeBytes;return checks;
}
inline juce::StringArray describeArmedRecordingTargets(const ArmedRecordingTargets& targets,size_t maximumLines=16){
    juce::StringArray lines;
    if(targets.tracks.empty()){lines.add("FAIL - arm at least one empty audio track");return lines;}
    for(size_t i=0;i<targets.tracks.size() && i<maximumLines;++i){
        const auto& t=targets.tracks[i];
        const auto name=t.name.substring(0,80).replaceCharacters("\r\n\t","   ");
        const auto input=t.physicalInput>=0?juce::String((juce::int64)t.physicalInput+1):juce::String("invalid");
        lines.add("Track "+juce::String((juce::int64)t.track+1)+" "+name+
            " | Empty: "+(t.empty?"PASS":"FAIL")+" | Input "+input+": "+
            (t.routeValid?t.inputName.substring(0,60).replaceCharacters("\r\n\t","   ")+" (PASS)":juce::String("FAIL - enable/select input"))+
            (t.uniqueInput?juce::String{}:juce::String(" | DUPLICATE input")));
    }
    if(targets.tracks.size()>maximumLines)lines.add(juce::String((juce::int64)(targets.tracks.size()-maximumLines))+" additional armed tracks not shown");
    return lines;
}
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
