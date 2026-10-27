#pragma once
// Minimal local DAW adapter boundary. No network access or audio callbacks.
// A trusted host UI must supply fresh callbacks running on its message thread.
#include "RemoteRestrictedCommandDispatcher.h"
#include <functional>
namespace idw::daw::remote {
class TrackControlAdapter final : public RemoteDawCommandAdapter {
public:
    using TrackCallback = std::function<bool(std::size_t)>;
    TrackControlAdapter(std::size_t tracks, TrackCallback select, TrackCallback arm)
        : trackCount_(tracks), select_(std::move(select)), arm_(std::move(arm)) {}
    bool apply(const CommandMessage& command) override {
        if (command.trackIndex >= trackCount_) return false;
        switch (command.action) {
            case Action::select_track: return select_ && select_(command.trackIndex);
            case Action::arm_track: return arm_ && arm_(command.trackIndex);
            default: return false;
        }
    }
private:
    std::size_t trackCount_;
    TrackCallback select_, arm_;
};
}
