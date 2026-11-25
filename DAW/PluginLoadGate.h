#pragma once
namespace idw::daw {
// Tracks one asynchronous plugin load so its deadline, a cancel and a late
// callback can never resolve the same export twice. Every call happens on the
// message thread; a token identifies the load a callback belongs to, so a
// callback from a superseded export is rejected instead of being rendered.
class PluginLoadGate {
public:
    enum class Timeout { ignored, failed, canceled };
    struct Completion {
        bool accepted=false; // the instance belongs to the current export
        bool discard=false;  // destroy the instance, the export already moved on
    };
    // Starts a load and returns the token to pass back to complete()/expire().
    int begin(){waiting=true; return ++generation;}
    bool isWaiting() const {return waiting;}
    // A callback for an export that already timed out or was cancelled is
    // discarded rather than accepted.
    Completion complete(int token){
        if(!waiting || token!=generation)return {false,true};
        waiting=false;
        return {true,false};
    }
    // Returns what the caller must report once the deadline passes; ignored when
    // the load already completed, was cancelled, or belongs to an older export.
    Timeout expire(int token,bool cancelRequested){
        if(!waiting || token!=generation)return Timeout::ignored;
        waiting=false;
        return cancelRequested?Timeout::canceled:Timeout::failed;
    }
    // True when a pending export must be resolved as canceled now. The token is
    // advanced so the load this cancel abandons cannot resolve anything later.
    bool cancel(){
        if(!waiting)return false;
        waiting=false;
        ++generation;
        return true;
    }
private:
    bool waiting=false;
    int generation=0;
};
}
