#pragma once
#include "ExportOutcome.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace idw::daw {
// The one place an export alert's tone becomes a JUCE message-box icon. Keeping
// it out of the window code lets the presentation be tested on its own.
constexpr juce::MessageBoxIconType messageBoxIconFor(ExportAlertIcon icon){
    return icon==ExportAlertIcon::info?juce::MessageBoxIconType::InfoIcon
                                      :juce::MessageBoxIconType::WarningIcon;
}
static_assert(messageBoxIconFor(ExportAlertIcon::info)==juce::MessageBoxIconType::InfoIcon,
              "informational export alerts must use the information icon");
static_assert(messageBoxIconFor(ExportAlertIcon::warning)==juce::MessageBoxIconType::WarningIcon,
              "failed export alerts must use the warning icon");
}
