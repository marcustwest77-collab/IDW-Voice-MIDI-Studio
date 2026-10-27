#pragma once
// Experimental policy layer only. No sockets, streaming, recording or remote control.
#include <cstddef>
#include <cstdint>
namespace idw::daw::remote {
enum class Role { host, guest };
enum class Action { view, select_track, arm_track, start_recording, stop_recording,
                    set_gain, set_pan, delete_track, export_audio };
struct Grant {
    bool connected = false;
    bool authenticated = false;
    bool hostApproved = false;
    bool controlGranted = false;
    bool recordingGranted = false;
    std::uint64_t sessionEpoch = 0;
    std::uint64_t revokedEpoch = 0;
};
struct Request {
    Role actor = Role::guest;
    Action action = Action::view;
    std::size_t trackIndex = 0;
    std::size_t trackCount = 0;
    std::uint64_t sessionEpoch = 0;
    bool trackIsAudio = false;
    bool transportBusy = false;
    bool recordingActive = false;
};
// DENY by default for collaborators. A stale grant never becomes valid again.
// This is an authorization decision, NOT a command executor.
inline bool permitted(const Grant& grant, const Request& req) noexcept {
    if (req.actor == Role::host) return false; // local host permissions are handled outside this remote gate
    if (!grant.connected || !grant.authenticated || !grant.hostApproved ||
        req.sessionEpoch == 0 || req.sessionEpoch != grant.sessionEpoch ||
        req.sessionEpoch <= grant.revokedEpoch) return false;
    if (req.action == Action::view) return true;
    if (!grant.controlGranted) return false;
    switch (req.action) {
        case Action::select_track:
            return req.trackIndex < req.trackCount;
        case Action::arm_track:
            return req.trackIndex < req.trackCount && req.trackIsAudio &&
                   !req.transportBusy && !req.recordingActive;
        case Action::start_recording:
            return grant.recordingGranted && req.trackIndex < req.trackCount &&
                   req.trackIsAudio && !req.transportBusy && !req.recordingActive;
        case Action::stop_recording:
            return grant.recordingGranted && req.recordingActive;
        case Action::set_gain:
        case Action::set_pan:
            return req.trackIndex < req.trackCount && !req.recordingActive;
        case Action::delete_track:
        case Action::export_audio:
            return false; // not available remotely
        case Action::view: return true;
    }
    return false;
}
} // namespace idw::daw::remote
