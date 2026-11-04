#pragma once
#include "SessionModel.h"
namespace idw::daw {
inline bool sameMidiNotes(const std::vector<MidiNote>& a,const std::vector<MidiNote>& b){
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i)
        if(a[i].pitch!=b[i].pitch || a[i].startBeat!=b[i].startBeat ||
           a[i].lengthBeats!=b[i].lengthBeats || a[i].velocity!=b[i].velocity)return false;
    return true;
}
// Restore only the edited note list. Newer audio, mixer, instrument and tempo
// work remains intact. Unexpected note/track changes fail without mutation.
class MidiEditHistory {
public:
    enum class Result {applied,empty,conflict};
    bool record(const Session& session,size_t track,const std::vector<MidiNote>& before){
        if(track>=session.tracks.size() || session.tracks[track].type!=TrackType::midi ||
           sameMidiNotes(before,session.tracks[track].notes))return false;
        undoEntries.push_back({track,session.tracks[track].name,before,session.tracks[track].notes});
        if(undoEntries.size()>64)undoEntries.erase(undoEntries.begin());
        redoEntries.clear();return true;
    }
    Result undo(Session& s){return restore(s,undoEntries,redoEntries,true);}
    Result redo(Session& s){return restore(s,redoEntries,undoEntries,false);}
    bool canUndo() const {return !undoEntries.empty();}
    bool canRedo() const {return !redoEntries.empty();}
    void clear(){undoEntries.clear();redoEntries.clear();}
    // An explicit metadata rename preserves note undo/redo identity. Do not
    // remove the name guard from restore: unrelated track replacement must fail.
    void trackRenamed(size_t track,const std::string& previous,const std::string& current){
        for(auto* entries:{&undoEntries,&redoEntries})for(auto& entry:*entries)
            if(entry.track==track && entry.name==previous)entry.name=current;
    }
private:
    struct Entry {size_t track;std::string name;std::vector<MidiNote> before,after;};
    std::vector<Entry> undoEntries,redoEntries;
    static Result restore(Session& s,std::vector<Entry>& from,std::vector<Entry>& to,bool undo){
        if(from.empty())return Result::empty;
        const auto& e=from.back();
        if(e.track>=s.tracks.size() || s.tracks[e.track].type!=TrackType::midi ||
           s.tracks[e.track].name!=e.name ||
           !sameMidiNotes(s.tracks[e.track].notes,undo?e.after:e.before))return Result::conflict;
        auto replacement=undo?e.before:e.after;
        to.push_back(e);
        if(to.size()>64)to.erase(to.begin());
        s.tracks[e.track].notes.swap(replacement);from.pop_back();return Result::applied;
    }
};
}
