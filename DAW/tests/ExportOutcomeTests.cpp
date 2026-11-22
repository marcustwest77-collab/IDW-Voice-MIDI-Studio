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
void expectTitle(ExportMediaKind media,ExportOutcome outcome,const char* title){
    const auto plan=exportAlertPlan(media,outcome);
    if(std::strcmp(plan.title,title)!=0)
        throw std::runtime_error(std::string("alert title mismatch: expected \"")+title+
            "\", got \""+plan.title+"\"");
}
}
int main(){
    // Each outcome names itself for the medium being exported; the alert's
    // icon tone is covered by IDWDAWExportAlertMessageBox.
    expectTitle(ExportMediaKind::wav,ExportOutcome::canceled,"WAV export canceled");
    expectTitle(ExportMediaKind::midi,ExportOutcome::canceled,"MIDI export canceled");
    expectTitle(ExportMediaKind::wav,ExportOutcome::completed,"WAV export complete");
    expectTitle(ExportMediaKind::midi,ExportOutcome::completed,"MIDI export complete");
    expectTitle(ExportMediaKind::wav,ExportOutcome::failed,"WAV bounce unavailable");
    expectTitle(ExportMediaKind::midi,ExportOutcome::failed,"MIDI export unavailable");
    // Only a completed export counts as success; a cancel is not a silent pass.
    require(static_cast<bool>(ExportResult(ExportOutcome::completed)),"completed result not true");
    require(!ExportResult(ExportOutcome::canceled),"canceled result reported success");
    require(!ExportResult(ExportOutcome::failed),"failed result reported success");
    require(ExportResult(ExportOutcome::canceled).outcome==ExportOutcome::canceled,"canceled outcome lost");
    std::cout<<"PASS: export outcomes map to distinct alert titles\n";
}
