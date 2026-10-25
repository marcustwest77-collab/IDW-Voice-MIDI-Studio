#include "SessionHistory.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace idw::daw;
using Result=ClipEditHistory::Result;
int main(){
    Session s;s.addTrack(TrackType::audio);s.addClip(0,{"a.wav",0,4,0});
    s.addClip(0,{"b.wav",3,4,0});s.addTrack(TrackType::midi);
    ClipEditHistory h;auto before=s;
    assert(s.crossfadeClips(0,0,1,FadeCurve::equalPower));assert(h.record(before,s));
    // These non-clip changes must survive both directions of a clip edit.
    s.setTempo(93);s.tracks[0].name="Vocals";s.tracks[0].inputChannel=3;
    s.setTrackMix(0,0.7f,-0.4f);s.tracks[0].mute=true;s.tracks[0].solo=true;
    s.addMidiNote(1,{67,1,2,88});s.setTrackSynthPreset(1,3);
    s.addTrack(TrackType::audio);s.addClip(2,{"new-recording.wav",2,8,0});
    const auto checkUnrelated=[&]{
        assert(s.bpm==93 && s.tracks.size()==3 && s.tracks[0].name=="Vocals");
        assert(s.tracks[0].gain==0.7f && s.tracks[0].pan==-0.4f && s.tracks[0].inputChannel==3);
        assert(s.tracks[0].mute && s.tracks[0].solo);
        assert(s.tracks[1].notes.size()==1 && s.tracks[1].notes[0].pitch==67 && s.tracks[1].synthPreset==3);
        assert(s.tracks[2].clips.size()==1 && s.tracks[2].clips[0].sourcePath=="new-recording.wav");
    };
    assert(h.undo(s)==Result::applied);checkUnrelated();
    assert(s.tracks[0].clips[0].fadeOutBeats==0 && s.tracks[0].clips[1].fadeInBeats==0);
    assert(!h.record(s,s)); // No-op preserves redo.
    assert(h.redo(s)==Result::applied);checkUnrelated();
    assert(s.tracks[0].clips[0].fadeOutBeats==1 && s.tracks[0].clips[1].fadeInBeats==1);
    // An externally added take on the affected track makes the old edit unsafe.
    s.addClip(0,{"later-take.wav",10,2,0});const auto withTake=s.tracks[0].clips;
    assert(h.undo(s)==Result::conflict && sameClips(s.tracks[0].clips,withTake));checkUnrelated();
    s.tracks[0].clips.pop_back();assert(h.undo(s)==Result::applied); // Conflict did not pop history.
    s.addClip(0,{"import-after-undo.wav",12,1,0});const auto afterUndo=s.tracks[0].clips;
    assert(h.redo(s)==Result::conflict && sameClips(afterUndo,s.tracks[0].clips));
    s.tracks[0].clips.pop_back();
    before=s;s.moveClip(0,0,0.5);assert(h.record(before,s));assert(h.redo(s)==Result::empty);
    // Multi-track transaction: validate all tracks before replacing either.
    h.clear();before=s;s.setClipFades(0,0,0.5,0.5);s.setClipFades(2,0,1,1);assert(h.record(before,s));
    s.tracks[2].clips[0].sourceOffsetSeconds=0.1;const auto first=s.tracks[0].clips;
    assert(h.undo(s)==Result::conflict && sameClips(first,s.tracks[0].clips));
    s.tracks[2].clips[0].sourceOffsetSeconds=0;assert(h.undo(s)==Result::applied);
    assert(sameClips(before.tracks[0].clips,s.tracks[0].clips) && sameClips(before.tracks[2].clips,s.tracks[2].clips));
    // A missing affected track must reject the complete redo transaction.
    auto missing=s;missing.tracks.pop_back();const auto untouched=missing.tracks[0].clips;
    assert(h.redo(missing)==Result::conflict && sameClips(untouched,missing.tracks[0].clips));
    // Split remains one reversible operation and leaves unrelated tracks intact.
    h.clear();before=s;assert(s.splitClip(0,0,2));assert(h.record(before,s));
    const auto split=s.tracks[0].clips;assert(h.undo(s)==Result::applied);
    assert(sameClips(s.tracks[0].clips,before.tracks[0].clips));assert(h.redo(s)==Result::applied);
    assert(sameClips(s.tracks[0].clips,split));checkUnrelated();
    // Bounded history, empty operations, and project-load reset.
    h.clear();assert(h.undo(s)==Result::empty);
    for(int i=0;i<70;++i){before=s;s.moveClip(0,0,20+i);assert(h.record(before,s));}
    int count=0;while(h.undo(s)==Result::applied)++count;assert(count==64);
    count=0;while(h.redo(s)==Result::applied)++count;assert(count==64);
    h.clear();assert(h.undo(s)==Result::empty && h.redo(s)==Result::empty);
    std::cout<<"PASS: clip-only history preserves newer project work\n";
}

