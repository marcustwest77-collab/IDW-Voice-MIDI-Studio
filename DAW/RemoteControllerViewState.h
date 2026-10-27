#pragma once
// Presenter-only state for a future host controller UI.
// It never changes a grant, authenticates a peer or operates a DAW.
#include "RemoteHostApprovalQueue.h"
namespace idw::daw::remote {
struct ControllerViewState {
    bool connected=false;
    bool authenticated=false;
    bool hostApproved=false;
    bool controlEnabled=false;
    bool recordingEnabled=false;
    bool canApproveRequests=false;
    std::size_t pending=0;
    std::uint64_t epoch=0;
};
inline ControllerViewState controllerViewState(const Grant& grant,
                                               const HostApprovalQueue& queue) noexcept {
    ControllerViewState view;
    view.epoch=grant.sessionEpoch;
    view.connected=grant.connected && grant.revokedEpoch<grant.sessionEpoch;
    view.authenticated=view.connected && grant.authenticated;
    view.hostApproved=view.authenticated && grant.hostApproved;
    view.controlEnabled=view.hostApproved && grant.controlGranted;
    view.recordingEnabled=view.controlEnabled && grant.recordingGranted;
    view.canApproveRequests=view.controlEnabled && queue.size()>0;
    view.pending=view.hostApproved ? queue.size() : 0;
    return view;
}
} // namespace idw::daw::remote
