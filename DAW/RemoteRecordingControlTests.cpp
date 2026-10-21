#include "RemoteRecordingControl.h"
#include <cstdlib>
#include <iostream>

using Gate = idw::RemoteRecordingControl;
using R = Gate::Result;
using A = Gate::Action;
void check(bool ok, const char* name) {
    if (!ok) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
int main() {
    Gate gate;
    const auto now = Gate::Time{};
    const auto ttl = std::chrono::seconds(60);
    auto call = [&](std::uint64_t id, std::uint64_t seq, A action = A::Record) {
        return gate.authorize("guest", 7, "track-a", id, seq, action, now);
    };
    check(call(1, 1) == R::NoGrant, "default deny");
    auto grant = gate.grant("guest", "track-a", 7, now, ttl);
    check(grant.has_value(), "local grant");
    const auto id = grant->id;
    check(gate.authorize("intruder", 7, "track-a", id, 999, A::Record, now)
          == R::WrongPeer, "peer binding");
    check(gate.authorize("guest", 7, "track-b", id, 999, A::Record, now)
          == R::WrongTrack, "track binding");
    check(call(id + 1, 999) == R::WrongGrant, "grant binding");
    check(call(id, 999, static_cast<A>(99)) == R::InvalidAction, "unknown command");
    check(call(id, 0) == R::Replay, "zero sequence");
    check(call(id, 1, A::Arm) == R::Allowed, "denials do not consume sequence");
    check(call(id, 1) == R::Replay, "duplicate rejected");
    check(call(id, 3) == R::Allowed, "new record allowed");
    check(call(id, 2, A::Stop) == R::Replay, "out of order rejected");
    check(call(id, 4, A::Stop) == R::Allowed, "remote stop allowed");
    check(gate.authorize("guest", 7, "track-a", id, 5, A::Record, now + ttl)
          == R::Expired, "expiry boundary");
    check(call(id, 6) == R::NoGrant, "expiry cannot revive");
    grant = gate.grant("guest", "track-a", 7, now, ttl);
    check(grant->id != id, "replacement gets new generation");
    check(call(id, 50) == R::WrongGrant, "old grant cannot replay");
    check(call(grant->id, 1) == R::Allowed, "new grant resets sequence");
    gate.revoke();
    check(call(grant->id, 2) == R::NoGrant, "immediate revoke");
    grant = gate.grant("guest", "track-a", 7, now, ttl);
    check(gate.authorize("guest", 8, "track-a", grant->id, 1, A::Record, now)
          == R::WrongProject, "project replacement");
    check(call(grant->id, 2) == R::NoGrant, "old project cannot revive grant");
    grant = gate.grant("host", "track-b", 8, now, ttl);
    check(gate.authorize("host", 8, "track-b", grant->id, 1, A::Record, now)
          == R::Allowed, "roles can reverse with local consent");
    check(!gate.grant("", "track-a", 7, now, ttl), "empty peer rejected");
    check(call(grant->id, 2) == R::NoGrant, "invalid replacement revokes");
    check(!gate.grant("guest", "", 7, now, ttl), "empty track rejected");
    check(!gate.grant("guest", "track-a", 7, now, std::chrono::seconds(0)), "zero lifetime");
    check(!gate.grant("guest", "track-a", 7, now, std::chrono::seconds(-1)), "negative lifetime");
    check(!gate.grant("guest", "track-a", 7, now, std::chrono::seconds(901)), "bounded lifetime");
    check(!gate.grant("guest", "track-a", 7, Gate::Time::max(), ttl), "time overflow");
    check(gate.grant("guest", "track-a", 7, now, std::chrono::seconds(900)).has_value(),
          "maximum lifetime accepted");
    grant = gate.grant("guest", Gate::Scope::SelectedTracks, {"track-b", "track-a"}, 7, now, ttl);
    check(grant.has_value(), "multiple tracks granted");
    check(call(grant->id, 1) == R::Allowed, "first selected track");
    check(gate.authorize("guest", 7, "track-b", grant->id, 2, A::Arm, now)
          == R::Allowed, "second selected track");
    check(gate.authorize("guest", 7, "track-c", grant->id, 3, A::Record, now)
          == R::WrongTrack, "unselected track denied");
    check(!gate.grant("guest", Gate::Scope::SelectedTracks, {"a", "a"}, 7, now, ttl), "duplicate tracks");
    check(!gate.grant("guest", Gate::Scope::SelectedTracks, {}, 7, now, ttl), "empty selection");
    check(!gate.grant("guest", Gate::Scope::AllTracks, {"a"}, 7, now, ttl), "ambiguous scope");
    grant = gate.grant("guest", Gate::Scope::AllTracks, {}, 7, now, ttl);
    check(grant.has_value(), "all tracks granted");
    check(call(grant->id, 1) == R::Allowed, "all includes original track");
    check(gate.authorize("guest", 7, "new-track", grant->id, 2, A::Record, now)
          == R::Allowed, "all includes new track");
    check(gate.authorize("guest", 7, "", grant->id, 3, A::Record, now)
          == R::WrongTrack, "all still requires target");
    check(gate.authorize("guest", 8, "new-track", grant->id, 3, A::Record, now)
          == R::WrongProject, "all remains project scoped");
    grant = gate.grant("guest", Gate::Scope::AllTracks, {}, 7, now, ttl);
    auto broadId = grant->id;
    grant = gate.grant("guest", "track-a", 7, now, ttl);
    check(call(broadId, 4) == R::WrongGrant, "narrowing invalidates broad grant");
    check(gate.authorize("guest", 7, "track-b", grant->id, 1, A::Record, now)
          == R::WrongTrack, "narrowing removes other tracks");
    grant = gate.grant("guest", Gate::Scope::SelectedTracks, {"a", "b"}, 7, now, ttl);
    check(gate.authorizeTargets("guest", 7, {"a", "c"}, grant->id, 1, A::Record, now)
          == R::WrongTrack, "one unapproved armed track denies entire capture");
    check(gate.authorizeTargets("guest", 7, {"b", "a"}, grant->id, 1, A::Record, now)
          == R::Allowed, "entire approved capture succeeds atomically");
    check(gate.authorizeTargets("guest", 7, {"a", "b"}, grant->id, 1, A::Stop, now)
          == R::Replay, "batch consumes sequence once");
    check(gate.authorizeTargets("guest", 7, {"a", "c"}, grant->id, 2, A::Stop, now)
          == R::WrongTrack, "remote stop cannot stop unapproved recording");
    check(gate.authorizeTargets("guest", 7, {}, grant->id, 2, A::Record, now)
          == R::WrongTrack, "empty capture denied");
    check(gate.authorizeTargets("guest", 7, {"a", "a"}, grant->id, 2, A::Record, now)
          == R::WrongTrack, "duplicate capture target denied");
    grant = gate.grant("guest", Gate::Scope::AllTracks, {}, 7, now, ttl);
    check(gate.authorizeTargets("guest", 7, {"a", "b", "c"}, grant->id, 1, A::Record, now)
          == R::Allowed, "all scope authorizes complete armed set");
    std::cout << "Remote recording permission tests: PASS\n";
}
