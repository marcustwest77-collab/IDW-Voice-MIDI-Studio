#include "SessionModel.h"
#include "SessionHistory.h"
#include <limits>
// Keep regression assertions active in Release, including setup calls.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
    using namespace idw::daw;
    Session s;
    assert(s.addTrack(TrackType::audio));
    assert(s.addClip(0,{"original.wav",4.0,8.0,1.0}));
    const Session before=s;
    assert(!s.splitClip(0,0,4.0));
    assert(!s.splitClip(0,0,12.0));
    assert(s.splitClip(0,0,7.0));
    assert(s.tracks[0].clips.size()==2);
    const auto& left=s.tracks[0].clips[0];
    const auto& right=s.tracks[0].clips[1];
    assert(left.sourcePath=="original.wav" && right.sourcePath=="original.wav");
    assert(std::abs(left.lengthBeats-3.0)<1e-9);
    assert(std::abs(right.startBeat-7.0)<1e-9);
    assert(std::abs(right.lengthBeats-5.0)<1e-9);
    assert(std::abs(right.sourceOffsetSeconds-2.5)<1e-9);
    assert(s.trimClipLeft(0,1,8.0));
    assert(std::abs(s.tracks[0].clips[1].sourceOffsetSeconds-3.0)<1e-9);
    assert(std::abs(s.tracks[0].clips[1].lengthBeats-4.0)<1e-9);
    assert(s.trimClipRight(0,1,10.0));
    assert(std::abs(s.tracks[0].clips[1].lengthBeats-2.0)<1e-9);
    assert(!s.trimClipRight(0,1,11.0));
    assert(!s.trimClipLeft(0,1,11.0));
    assert(s.tracks[0].clips[0].sourcePath==before.tracks[0].clips[0].sourcePath);
    // Fine keyboard edits (Alt+arrow / Alt+bracket) use 1/16-beat metadata.
    Session fine;fine.bpm=120;assert(fine.addTrack(TrackType::audio));
    assert(fine.addClip(0,{"fine.wav",1.0,2.0,0.0}));
    assert(fine.moveClip(0,0,1.0625));
    assert(std::abs(fine.tracks[0].clips[0].startBeat-1.0625)<1e-9);
    assert(fine.resizeClip(0,0,2.0625));
    assert(std::abs(fine.tracks[0].clips[0].lengthBeats-2.0625)<1e-9);
    assert(fine.trimClipLeft(0,0,1.125));
    assert(std::abs(fine.tracks[0].clips[0].sourceOffsetSeconds-0.03125)<1e-9);
    assert(fine.trimClipRight(0,0,3.0625));
    assert(std::abs(fine.tracks[0].clips[0].lengthBeats-1.9375)<1e-9);
    Session duplicate;duplicate.addTrack(TrackType::audio);duplicate.addTrack(TrackType::midi);
    duplicate.addClip(0,{"trimmed.wav",1.125,1.9375,0.03125,0.25,0.5,
                         FadeCurve::equalPower,FadeCurve::linear,-7.5});
    duplicate.addClip(0,{"neighbor.wav",3.0,1.0,0});
    const auto original=duplicate;
    ClipEditHistory history;
    assert(duplicate.duplicateClip(0,0) && history.record(original,duplicate));
    auto expected=original.tracks[0].clips[0];expected.startBeat=3.0625;
    assert(sameClips({duplicate.tracks[0].clips[1]},{expected}));
    assert(sameClips({duplicate.tracks[0].clips[0]},{original.tracks[0].clips[0]}));
    assert(sameClips({duplicate.tracks[0].clips[2]},{original.tracks[0].clips[1]}));
    // Existing neighboring clips are neither moved nor replaced.
    assert(history.undo(duplicate)==ClipEditHistory::Result::applied);
    assert(sameClips(duplicate.tracks[0].clips,original.tracks[0].clips));
    assert(history.redo(duplicate)==ClipEditHistory::Result::applied);
    assert(duplicate.tracks[0].clips.size()==3);
    assert(duplicate.duplicateClip(0,1) && duplicate.tracks[0].clips[2].startBeat==5.0);
    const auto safe=duplicate.tracks[0].clips;
    assert(!duplicate.duplicateClip(99,0) && !duplicate.duplicateClip(1,0) &&
           !duplicate.duplicateClip(0,99));
    assert(sameClips(safe,duplicate.tracks[0].clips));
    Session overflow;overflow.addTrack(TrackType::audio);
    overflow.addClip(0,{"huge.wav",std::numeric_limits<double>::max()/2,
                                  std::numeric_limits<double>::max()/2,0});
    const auto huge=overflow.tracks[0].clips;
    assert(!overflow.duplicateClip(0,0) && sameClips(huge,overflow.tracks[0].clips));
    std::cout<<"Clip editing regression tests passed\n";
}
