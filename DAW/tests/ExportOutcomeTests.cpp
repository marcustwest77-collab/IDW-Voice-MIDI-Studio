#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "ExportOutcome.h"
using namespace idw::daw;
namespace {
void require(bool condition,const char* message){
    if(!condition)throw std::runtime_error(message);
}
void expectPlan(ExportMediaKind media,ExportOutcome outcome,ExportAlertIcon icon,const char* title){
    const auto plan=exportAlertPlan(media,outcome);
    if(plan.icon!=icon || std::strcmp(plan.title,title)!=0)
        throw std::runtime_error(std::string("alert plan mismatch for ")+title+
            ": got \""+plan.title+"\" with icon "+std::to_string((int)plan.icon));
}
}
int main(){
    // A canceled export must stay an informational result, never a warning,
    // and its title must name the cancelation for the medium being exported.
    expectPlan(ExportMediaKind::wav,ExportOutcome::canceled,ExportAlertIcon::info,"WAV export canceled");
    expectPlan(ExportMediaKind::midi,ExportOutcome::canceled,ExportAlertIcon::info,"MIDI export canceled");
    expectPlan(ExportMediaKind::wav,ExportOutcome::completed,ExportAlertIcon::info,"WAV export complete");
    expectPlan(ExportMediaKind::midi,ExportOutcome::completed,ExportAlertIcon::info,"MIDI export complete");
    expectPlan(ExportMediaKind::wav,ExportOutcome::failed,ExportAlertIcon::warning,"WAV bounce unavailable");
    expectPlan(ExportMediaKind::midi,ExportOutcome::failed,ExportAlertIcon::warning,"MIDI export unavailable");
    // Only a completed export counts as success; a cancel is not a silent pass.
    require(static_cast<bool>(ExportResult(ExportOutcome::completed)),"completed result not true");
    require(!ExportResult(ExportOutcome::canceled),"canceled result reported success");
    require(!ExportResult(ExportOutcome::failed),"failed result reported success");
    require(ExportResult(ExportOutcome::canceled)==ExportOutcome::canceled,"canceled outcome lost");
    require(ExportResult(ExportOutcome::failed)!=ExportOutcome::canceled,"failure reported as cancel");
    std::cout<<"PASS: export outcomes map to distinct alert titles and icons\n";
}
