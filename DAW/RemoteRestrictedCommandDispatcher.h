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
        // Default-deny: only reviewed track selection and arming are dispatchable.
        // The permission gate already revalidated the action against live state.
        if (approved.action != Action::select_track &&
            approved.action != Action::arm_track) return false;
        return adapter.apply(approved); // no retry; approval has been consumed
    }
private:
    HostApprovalDispatchGate& gate_;
};
}
