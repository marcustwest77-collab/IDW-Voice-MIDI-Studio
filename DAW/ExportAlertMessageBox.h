#pragma once
#include "ExportOutcome.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace idw::daw {
// The one place an export alert's tone becomes a JUCE message-box icon. Keeping
// it out of the window code lets IDWDAWExportAlertMessageBox check it alone.
inline juce::MessageBoxIconType messageBoxIconFor(ExportAlertIcon icon){
    return icon==ExportAlertIcon::info?juce::MessageBoxIconType::InfoIcon
                                      :juce::MessageBoxIconType::WarningIcon;
}
}
