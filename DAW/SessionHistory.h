#pragma once
#include "SessionModel.h"
namespace idw::daw {
inline bool sameClips(const std::vector<Clip>& a,const std::vector<Clip>& b){
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i){
        const auto& x=a[i];const auto& y=b[i];
        if(x.sourcePath!=y.sourcePath || x.startBeat!=y.startBeat ||
           x.lengthBeats!=y.lengthBeats || x.sourceOffsetSeconds!=y.sourceOffsetSeconds ||
           x.fadeInBeats!=y.fadeInBeats || x.fadeOutBeats!=y.fadeOutBeats ||
           x.gainDb!=y.gainDb || x.fadeInCurve!=y.fadeInCurve || x.fadeOutCurve!=y.fadeOutCurve)return false;
    }
    return true;
}
// Clip edits must not restore unrelated session state (tracks, takes, mixer,
// tempo, routing, MIDI notes). Validate expected clip contents before replacing
// anything, so an import/recording on an affected track cannot be overwritten.
class ClipEditHistory {
public:
    enum class Result { applied, empty, conflict };
    bool record(const Session& before,const Session& after){
        if(before.tracks.size()!=after.tracks.size())return false;
        Entry entry;
        for(size_t i=0;i<before.tracks.size();++i){
            if(before.tracks[i].type!=after.tracks[i].type)return false;
            if(!sameClips(before.tracks[i].clips,after.tracks[i].clips)){
                if(after.tracks[i].type!=TrackType::audio)return false;
                entry.push_back({i,before.tracks[i].clips,after.tracks[i].clips});
            }
        }
        if(entry.empty())return false; // No-op gestures must preserve redo.
        undoEntries.push_back(std::move(entry));
        if(undoEntries.size()>64)undoEntries.erase(undoEntries.begin());
        redoEntries.clear();return true;
    }
    Result undo(Session& session){return restore(session,undoEntries,redoEntries,true);}
    Result redo(Session& session){return restore(session,redoEntries,undoEntries,false);}
    void clear(){undoEntries.clear();redoEntries.clear();}
private:
    struct Change {size_t track;std::vector<Clip> before,after;};
    using Entry=std::vector<Change>;
    std::vector<Entry> undoEntries,redoEntries;
    static Result restore(Session& session,std::vector<Entry>& from,std::vector<Entry>& to,bool undo){
        if(from.empty())return Result::empty;
        const auto& entry=from.back();
        for(const auto& change:entry){
            if(change.track>=session.tracks.size() || session.tracks[change.track].type!=TrackType::audio ||
               !sameClips(session.tracks[change.track].clips,undo?change.after:change.before))return Result::conflict;
        }
        // Prepare every replacement first; apply the whole edit with no-throw swaps.
        std::vector<std::vector<Clip>> replacements;
        for(const auto& change:entry)replacements.push_back(undo?change.before:change.after);
        to.push_back(entry);
        if(to.size()>64)to.erase(to.begin());
        for(size_t i=0;i<entry.size();++i)session.tracks[entry[i].track].clips.swap(replacements[i]);
        from.pop_back();return Result::applied;
    }
};
}

