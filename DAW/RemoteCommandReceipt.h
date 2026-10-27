#pragma once
// Data-only acknowledgements for the remote controller. No transport/DAW execution.
#include "RemoteCommandValidation.h"
#include <cstdint>
#include <string>
namespace idw::daw::remote {
enum class ReceiptStatus { rejected, pendingHostApproval, approvedForDispatch, executed, failed };
struct CommandReceipt {
    std::uint64_t epoch=0;
    std::uint64_t sequence=0;
    std::string requestId;
    ReceiptStatus status=ReceiptStatus::rejected;
};
inline bool makeReceipt(const CommandMessage& command, ReceiptStatus status,
                        CommandReceipt& result) {
    if(command.protocolVersion!=1 || command.sessionEpoch==0 ||
       command.sequence==0 || !validId(command.requestId)) return false;
    CommandReceipt candidate;
    candidate.epoch=command.sessionEpoch;
    candidate.sequence=command.sequence;
    candidate.requestId=command.requestId;
    candidate.status=status;
    result=candidate;
    return true;
}
// A host may display status; only the actual dispatcher may report "executed".
// This component does not claim commands succeeded.
inline bool isTerminalReceipt(ReceiptStatus status) noexcept {
    return status==ReceiptStatus::rejected || status==ReceiptStatus::executed ||
           status==ReceiptStatus::failed;
}
}
