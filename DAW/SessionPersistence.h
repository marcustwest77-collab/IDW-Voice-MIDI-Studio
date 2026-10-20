#pragma once
#include <juce_core/juce_core.h>
#include "SessionModel.h"
namespace idw::daw {
inline juce::var sessionToJson(const Session& s) {
    auto root=std::make_unique<juce::DynamicObject>();
    root->setProperty("format","idw-daw-session");
    root->setProperty("version",1);
    root->setProperty("bpm",s.bpm);
    root->setProperty("lyrics",juce::String::fromUTF8(s.lyrics.c_str()));
    root->setProperty("numerator",s.numerator);
    root->setProperty("denominator",s.denominator);
    juce::Array<juce::var> tracks;
    for(const auto& t:s.tracks){
        auto track=std::make_unique<juce::DynamicObject>();
        track->setProperty("name",juce::String(t.name));
        track->setProperty("type",t.type==TrackType::audio?"audio":"midi");
        track->setProperty("mute",t.mute);
        track->setProperty("solo",t.solo);
        if(t.type==TrackType::audio)track->setProperty("inputChannel",t.inputChannel);
        track->setProperty("gain",t.gain);
        track->setProperty("pan",t.pan);
        if(t.type==TrackType::audio){
            track->setProperty("vocalFxPreset",t.vocalFxPreset);
            track->setProperty("vocalFxMix",t.vocalFxMix);
        }
        if(t.type==TrackType::midi){track->setProperty("synthPreset",t.synthPreset);track->setProperty("synthAttackMs",t.synthAttackMs);track->setProperty("synthReleaseMs",t.synthReleaseMs);}
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
        juce::Array<juce::var> notes;
        for(const auto& n:t.notes){
            auto note=std::make_unique<juce::DynamicObject>();
            note->setProperty("pitch",n.pitch);
            note->setProperty("startBeat",n.startBeat);
            note->setProperty("lengthBeats",n.lengthBeats);
            note->setProperty("velocity",n.velocity);
            notes.add(juce::var(note.release()));
        }
        track->setProperty("notes",juce::var(notes));
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
    const auto lyrics=root->getProperty("lyrics");
    if(!lyrics.isVoid()){
        if(!lyrics.isString() || lyrics.toString().length()>32768)return false;
        next.lyrics=lyrics.toString().toStdString();
    }
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
        const auto input=obj->getProperty("inputChannel");
        if(!input.isVoid()){
            if(type!="audio" || (!input.isInt() && !input.isInt64()) ||
               (int)input<0 || (int)input>7)return false;
            target.inputChannel=(int)input;
        }
        if(!next.setTrackMix(next.tracks.size()-1,(float)(double)obj->getProperty("gain"),(float)(double)obj->getProperty("pan")))return false;
        const auto vocalMix=obj->getProperty("vocalFxMix");
        if(!vocalMix.isVoid() && ((!vocalMix.isDouble() && !vocalMix.isInt() && !vocalMix.isInt64()) ||
           !next.setTrackVocalFxMix(next.tracks.size()-1,(float)(double)vocalMix)))return false;
        const auto vocalFx=obj->getProperty("vocalFxPreset");
        if(!vocalFx.isVoid() && (!vocalFx.isInt() ||
           !next.setTrackVocalFxPreset(next.tracks.size()-1,(int)vocalFx)))return false;
        const auto preset=obj->getProperty("synthPreset");
        if(!preset.isVoid() && (!preset.isInt() || !next.setTrackSynthPreset(next.tracks.size()-1,(int)preset)))return false;
        const auto attack=obj->getProperty("synthAttackMs");
        const auto release=obj->getProperty("synthReleaseMs");
        if(!attack.isVoid() || !release.isVoid()){
            if(attack.isVoid() || release.isVoid() ||
               !next.setTrackSynthEnvelope(next.tracks.size()-1,
                   (float)(double)attack,(float)(double)release))return false;
        }
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
        // Older v1 sessions may omit the new MIDI notes property.
        const auto notes=obj->getProperty("notes");
        if(!notes.isVoid()){
            if(!notes.isArray())return false;
            for(const auto& n:*notes.getArray()){
                auto* no=n.getDynamicObject();
                if(!no)return false;
                const auto pitch=no->getProperty("pitch");
                const auto velocity=no->getProperty("velocity");
                if((!pitch.isInt()&&!pitch.isInt64()) ||
                   (!velocity.isInt()&&!velocity.isInt64()))return false;
                if(!next.addMidiNote(next.tracks.size()-1,{
                    (int)pitch,
                    (double)no->getProperty("startBeat"),
                    (double)no->getProperty("lengthBeats"),
                    (int)velocity}))return false;
            }
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
