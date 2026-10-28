#include "../RemoteReviewMailbox.h"
#include "../RemoteValidatedCommandReviewReceiver.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
    RemoteReviewMailbox mailbox;
    SessionReviewCoordinator coordinator;
    ValidatedCommandReviewReceiver receiver(coordinator);
    Request live;
    live.trackCount=3;
    live.trackIsAudio=true;

    // Mailbox admission is not a network identity or host permission.
    assert(mailbox.enqueue("1|700|1|first|arm|1",700));
    for (const auto& pending : mailbox.drain())
        assert(!receiver.receive(pending.payload,live,pending.trustedEpoch));
    assert(coordinator.pending().empty());

    assert(coordinator.beginHostApprovedSession(700,true,true,true));
    assert(mailbox.enqueue("1|700|1|first|arm|1",700));
    assert(mailbox.enqueue("1|700|2|second|select|0",700));
    int accepted=0;
    for (const auto& pending : mailbox.drain())
        if(receiver.receive(pending.payload,live,pending.trustedEpoch)) ++accepted;
    assert(accepted==2 && coordinator.pending().size()==2);

    // Older commands cannot be replayed after the coordinator records sequence.
    assert(mailbox.enqueue("1|700|1|replay|arm|1",700));
    for (const auto& pending : mailbox.drain())
        assert(!receiver.receive(pending.payload,live,pending.trustedEpoch));

    // Host revocation removes reviewed and mailbox-pending work.
    assert(mailbox.enqueue("1|700|3|third|arm|1",700));
    assert(coordinator.revokeControl());
    mailbox.clear();
    assert(mailbox.drain().empty() && coordinator.pending().empty());

    coordinator.disconnect();
    assert(coordinator.beginHostApprovedSession(701,true,true,true));
    assert(mailbox.enqueue("1|700|4|stale|arm|1",700));
    for (const auto& pending : mailbox.drain())
        assert(!receiver.receive(pending.payload,live,pending.trustedEpoch));
    assert(mailbox.enqueue("1|701|1|new|arm|1",701));
    for (const auto& pending : mailbox.drain())
        assert(receiver.receive(pending.payload,live,pending.trustedEpoch));
    assert(coordinator.pending().size()==1);
}
