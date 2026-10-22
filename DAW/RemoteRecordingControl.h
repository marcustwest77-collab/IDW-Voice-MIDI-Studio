#pragma once

#include <algorithm>
#include <vector>
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
    enum class Scope { SelectedTracks, AllTracks };
    struct Grant {
        std::string peer;
        Scope scope;
        std::vector<std::string> tracks;
        std::uint64_t project;
        std::uint64_t id;
        Time expires;
    };

    // Call only in response to explicit LOCAL consent. Replacing a grant
    // revokes its predecessor, including when the new request is invalid.
    std::optional<Grant> grant(const std::string& peer, const std::string& track,
                               std::uint64_t project, Time now,
                               std::chrono::seconds lifetime) {
        return grant(peer, Scope::SelectedTracks, {track}, project, now, lifetime);
    }

    // AllTracks includes tracks added later within this project. The native
    // adapter must validate every target still exists and can record audio.
    std::optional<Grant> grant(const std::string& peer, Scope scope,
                               const std::vector<std::string>& tracks,
                               std::uint64_t project, Time now,
                               std::chrono::seconds lifetime) {
        revoke();
        if (scope != Scope::SelectedTracks && scope != Scope::AllTracks)
            return std::nullopt;
        if ((scope == Scope::AllTracks && !tracks.empty())
            || (scope == Scope::SelectedTracks && (tracks.empty() || tracks.size() > 128)))
            return std::nullopt;
        auto sorted = tracks;
        std::sort(sorted.begin(), sorted.end());
        if (std::any_of(sorted.begin(), sorted.end(), [](const auto& t) { return t.empty(); })
            || std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
            return std::nullopt;
        if (peer.empty() || lifetime <= std::chrono::seconds::zero()
            || lifetime > std::chrono::minutes(15)
            || now > Time::max() - lifetime
            || nextId == std::numeric_limits<std::uint64_t>::max())
            return std::nullopt;
        current = Grant{peer, scope, sorted, project, ++nextId, now + lifetime};
        return current;
    }

    // Invoke on local revoke, peer disconnect, project replacement or deletion
    // of the granted track. Local Stop must always bypass this remote gate.
    void revoke() { current.reset(); lastSequence = 0; }

    Result authorize(const std::string& authenticatedPeer,
                     std::uint64_t currentProject, const std::string& targetTrack,
                     std::uint64_t grantId, std::uint64_t sequence,
                     Action action, Time now) {
        return authorizeTargets(authenticatedPeer, currentProject, {targetTrack},
                                grantId, sequence, action, now);
    }

    // One command may affect every armed/recording track. Authorize the entire
    // engine-derived set atomically before calling the engine, on the same
    // message thread. Never substitute only the track named by the sender.
    Result authorizeTargets(const std::string& authenticatedPeer,
                            std::uint64_t currentProject,
                            const std::vector<std::string>& affectedTracks,
                            std::uint64_t grantId, std::uint64_t sequence,
                            Action action, Time now) {
        if (!current) return Result::NoGrant;
        if (now >= current->expires) { revoke(); return Result::Expired; }
        if (currentProject != current->project) {
            revoke(); return Result::WrongProject;
        }
        if (authenticatedPeer != current->peer) return Result::WrongPeer;
        if (affectedTracks.empty() || affectedTracks.size() > 128)
            return Result::WrongTrack;
        auto targets = affectedTracks;
        std::sort(targets.begin(), targets.end());
        if (std::adjacent_find(targets.begin(), targets.end()) != targets.end())
            return Result::WrongTrack;
        for (const auto& target : targets)
            if (target.empty() || (current->scope == Scope::SelectedTracks
                && !std::binary_search(current->tracks.begin(), current->tracks.end(), target)))
                return Result::WrongTrack;
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
