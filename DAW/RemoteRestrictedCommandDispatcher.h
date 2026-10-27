#pragma once
// Restricted, synchronous dispatch bridge. No DAW operations are implemented here.
// Only a trusted local host supplies the adapter after explicit approval.
#include "RemoteHostApprovalDispatchGate.h"
namespace idw::daw::remote {
class RemoteDawCommandAdapter {
public:
    virtual ~RemoteDawCommandAdapter() = default;
    virtual bool apply(const CommandMessage& approved) = 0;
};
class RestrictedCommandDispatcher {
public:
    explicit RestrictedCommandDispatcher(HostApprovalDispatchGate& gate) : gate_(gate) {}
    bool dispatchAfterHostClick(const std::string& id, const Grant& grant,
                                const Request& liveState, RemoteDawCommandAdapter& adapter) {
        CommandMessage approved;
        if (!gate_.approveForDispatch(id, grant, liveState, approved)) return false;
        // Never dispatch dangerous actions, even with a staged approval.
        if (approved.action == Action::delete_track ||
            approved.action == Action::export_audio ||
            approved.action == Action::view) return false;
        return adapter.apply(approved); // no retry; approval has been consumed
    }
private:
    HostApprovalDispatchGate& gate_;
};
}
