#pragma once
// Data-only bridge for a future verified TLS handoff. This cannot establish TLS
// identity, cross process boundaries or call JUCE off the message thread.
#include "RemoteControlCommandParser.h"
#include "RemoteSessionReviewCoordinator.h"
namespace idw::daw::remote {
class ValidatedCommandReviewReceiver {
public:
 explicit ValidatedCommandReviewReceiver(SessionReviewCoordinator& coordinator)
     : coordinator_(coordinator) {}
 // Called by trusted DAW-side transport adapter with fresh local state.
 bool receive(const std::string& serialized, const Request& liveDawState,
              std::uint64_t transportSessionEpoch) {
     // This epoch must come from trusted local transport state, not the payload.
     if (transportSessionEpoch == 0 || transportSessionEpoch != coordinator_.grant().sessionEpoch)
         return false;
     CommandMessage command;
     if (!parseControlCommand(serialized, command) || command.sessionEpoch != transportSessionEpoch)
         return false;
     if (command.action != Action::select_track && command.action != Action::arm_track)
         return false;
     return coordinator_.stageInspected(command, liveDawState);
 }
private:
 SessionReviewCoordinator& coordinator_;
};
}
