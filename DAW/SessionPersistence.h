#pragma once
#include <juce_core/juce_core.h>
#include "SessionModel.h"
namespace idw::daw {
inline juce::var sessionToJson(const Session& s) {
    auto root=std::make_unique<juce::DynamicObject>();
    root->setProperty("format","idw-daw-session");
    root->setProperty("version",1);
    root->setProperty("bpm",s.bpm);
    root->setProperty("numerator",s.numerator);
    root->setProperty("denominator",s.denominator);
    juce::Array<juce::var> tracks;
    for(const auto& t:s.tracks){
        auto track=std::make_unique<juce::DynamicObject>();
        track->setProperty("name",juce::String(t.name));
        track->setProperty("type",t.type==TrackType::audio?"audio":"midi");
        track->setProperty("mute",t.mute);
        track->setProperty("solo",t.solo);
        track->setProperty("gain",t.gain);
        track->setProperty("pan",t.pan);
        juce::Array<juce::var> clips;
        for(const auto& c:t.clips){
            auto clip=std::make_unique<juce::DynamicObject>();
            clip->setProperty("sourcePath",juce::String(c.sourcePath));
            clip->setProperty("startBeat",c.startBeat);
            clip->setProperty("lengthBeats",c.lengthBeats);
            clip->setProperty("sourceOffsetSeconds",c.sourceOffsetSeconds);
            clips.add(juce::var(clip.release()));
        }
        track->setProperty("clips",juce::var(clips));
        tracks.add(juce::var(track.release()));
    }
    root->setProperty("tracks",juce::var(tracks));
    return juce::var(root.release());
}
inline bool sessionFromJson(const juce::var& data,Session& output){
    auto* root=data.getDynamicObject();
    if(!root || root->getProperty("format").toString()!="idw-daw-session" ||
       (int)root->getProperty("version")!=1)return false;
    Session next;
    next.setTempo((double)root->getProperty("bpm"));
    next.numerator=(int)root->getProperty("numerator");
    next.denominator=(int)root->getProperty("denominator");
    if(next.numerator<1 || next.numerator>32 || next.denominator<1 || next.denominator>32)return false;
    const auto tracks=root->getProperty("tracks");
    if(!tracks.isArray())return false;
    for(const auto& t:*tracks.getArray()){
        auto* obj=t.getDynamicObject();
        if(!obj)return false;
        const auto type=obj->getProperty("type").toString();
        if(type!="audio" && type!="midi")return false;
        if(!next.addTrack(type=="audio"?TrackType::audio:TrackType::midi))return false;
        auto& target=next.tracks.back();
        target.name=obj->getProperty("name").toString().toStdString();
        target.mute=(bool)obj->getProperty("mute");
        target.solo=(bool)obj->getProperty("solo");
        if(!next.setTrackMix(next.tracks.size()-1,(float)(double)obj->getProperty("gain"),(float)(double)obj->getProperty("pan")))return false;
        const auto clips=obj->getProperty("clips");
        if(!clips.isArray())return false;
        for(const auto& c:*clips.getArray()){
            auto* co=c.getDynamicObject();
            if(!co)return false;
            if(!next.addClip(next.tracks.size()-1,{
                co->getProperty("sourcePath").toString().toStdString(),
                (double)co->getProperty("startBeat"),(double)co->getProperty("lengthBeats"),
                (double)co->getProperty("sourceOffsetSeconds")}))return false;
        }
    }
    output=std::move(next);
    return true;
}
inline bool saveSession(const Session& s,const juce::File& file){
    return file.replaceWithText(juce::JSON::toString(sessionToJson(s),true));
}
inline bool loadSession(const juce::File& file,Session& result){
    if(!file.existsAsFile())return false;
    auto data=juce::JSON::parse(file);
    return sessionFromJson(data,result);
}
}
