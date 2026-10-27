#include "../RemoteCommandReceipt.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main(){
 CommandMessage command; command.sessionEpoch=24; command.sequence=8;
 command.requestId="command_8"; command.action=Action::arm_track;
 CommandReceipt receipt;
 assert(makeReceipt(command,ReceiptStatus::pendingHostApproval,receipt));
 assert(receipt.epoch==24 && receipt.sequence==8 && receipt.requestId=="command_8");
 assert(!isTerminalReceipt(receipt.status));
 assert(!isTerminalReceipt(ReceiptStatus::approvedForDispatch));
 assert(isTerminalReceipt(ReceiptStatus::rejected));
 assert(isTerminalReceipt(ReceiptStatus::failed));
 assert(isTerminalReceipt(ReceiptStatus::executed));
 command.sequence=0;
 assert(!makeReceipt(command,ReceiptStatus::executed,receipt));
 assert(receipt.status==ReceiptStatus::pendingHostApproval);
 command.sequence=9;command.requestId="../bad";
 assert(!makeReceipt(command,ReceiptStatus::executed,receipt));
}
