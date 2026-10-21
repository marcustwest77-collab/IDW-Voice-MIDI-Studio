#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace idw {
// Message-thread only. This gate is NOT authentication or a recording engine.
// The adapter must supply authenticated peer identity, stable track identity,
// current project generation and steady_clock time; never trust wire values.
class RemoteRecordingControl {
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    enum class Action { Arm, Record, Stop };
    enum class Result { Allowed, NoGrant, Expired, WrongPeer, WrongProject,
                        WrongTrack, WrongGrant, Replay, InvalidAction };
    struct Grant {
        std::string peer;
        std::string track;
        std::uint64_t project;
        std::uint64_t id;
        Time expires;
    };

    // Call only in response to explicit LOCAL consent. Replacing a grant
    // revokes its predecessor, including when the new request is invalid.
    std::optional<Grant> grant(const std::string& peer, const std::string& track,
                               std::uint64_t project, Time now,
                               std::chrono::seconds lifetime) {
        revoke();
        if (peer.empty() || track.empty() || lifetime <= std::chrono::seconds::zero()
            || lifetime > std::chrono::minutes(15)
            || now > Time::max() - lifetime
            || nextId == std::numeric_limits<std::uint64_t>::max())
            return std::nullopt;
        current = Grant{peer, track, project, ++nextId, now + lifetime};
        return current;
    }

    // Invoke on local revoke, peer disconnect, project replacement or deletion
    // of the granted track. Local Stop must always bypass this remote gate.
    void revoke() { current.reset(); lastSequence = 0; }

    Result authorize(const std::string& authenticatedPeer,
                     std::uint64_t currentProject, const std::string& targetTrack,
                     std::uint64_t grantId, std::uint64_t sequence,
                     Action action, Time now) {
        if (!current) return Result::NoGrant;
        if (now >= current->expires) { revoke(); return Result::Expired; }
        if (currentProject != current->project) {
            revoke(); return Result::WrongProject;
        }
        if (authenticatedPeer != current->peer) return Result::WrongPeer;
        if (targetTrack != current->track) return Result::WrongTrack;
        if (grantId != current->id) return Result::WrongGrant;
        if (action != Action::Arm && action != Action::Record && action != Action::Stop)
            return Result::InvalidAction;
        if (sequence == 0 || sequence <= lastSequence) return Result::Replay;
        // Consume before execution: a failed recording action must not be
        // retried by replaying the same command. Request a fresh sequence.
        lastSequence = sequence;
        return Result::Allowed;
    }
private:
    std::optional<Grant> current;
    std::uint64_t nextId = 0;
    std::uint64_t lastSequence = 0;
};
} // namespace idw
