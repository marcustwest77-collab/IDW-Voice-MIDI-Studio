#include <iostream>
#include <stdexcept>
#include <string>
#include "ExportAlertMessageBox.h"
using namespace idw::daw;
namespace {
void require(bool condition,const char* message){
    if(!condition)throw std::runtime_error(message);
}
void expectIcon(ExportMediaKind media,ExportOutcome outcome,juce::MessageBoxIconType icon,const char* label){
    const auto iconForOutcome=messageBoxIconFor(exportAlertPlan(media,outcome).icon);
    if(iconForOutcome!=icon)
        throw std::runtime_error(std::string("message-box icon mismatch for ")+label);
}
}
int main(){
    // A canceled export is information for the user, not a warning, so a
    // regression that flips the icon fails here for both media.
    expectIcon(ExportMediaKind::wav,ExportOutcome::canceled,juce::MessageBoxIconType::InfoIcon,"WAV canceled");
    expectIcon(ExportMediaKind::midi,ExportOutcome::canceled,juce::MessageBoxIconType::InfoIcon,"MIDI canceled");
    expectIcon(ExportMediaKind::wav,ExportOutcome::completed,juce::MessageBoxIconType::InfoIcon,"WAV completed");
    expectIcon(ExportMediaKind::midi,ExportOutcome::completed,juce::MessageBoxIconType::InfoIcon,"MIDI completed");
    expectIcon(ExportMediaKind::wav,ExportOutcome::failed,juce::MessageBoxIconType::WarningIcon,"WAV failed");
    expectIcon(ExportMediaKind::midi,ExportOutcome::failed,juce::MessageBoxIconType::WarningIcon,"MIDI failed");
    // The mapping itself is the only source of the icon; nothing may fall back
    // to the warning icon for a canceled result.
    require(messageBoxIconFor(exportAlertPlan(ExportMediaKind::wav,ExportOutcome::canceled).icon)
                !=juce::MessageBoxIconType::WarningIcon,"canceled WAV alerted as a warning");
    require(messageBoxIconFor(exportAlertPlan(ExportMediaKind::midi,ExportOutcome::canceled).icon)
                !=juce::MessageBoxIconType::WarningIcon,"canceled MIDI alerted as a warning");
    std::cout<<"PASS: export alert plans map to the expected message-box icons\n";
}
