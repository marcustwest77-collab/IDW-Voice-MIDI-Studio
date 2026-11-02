#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>
namespace idw::daw {
struct MidiNoteNudge {double beats=0;int semitones=0;};
// Decode only editing arrows. The component separately requires its own focus,
// a valid selection and an idle editor before applying these commands.
inline std::optional<MidiNoteNudge> pianoRollNudge(const juce::KeyPress& key){
    const auto mods=key.getModifiers();
    if(mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())return {};
    const bool coarse=mods.isShiftDown();
    const int code=key.getKeyCode();
    if(code==juce::KeyPress::leftKey)return MidiNoteNudge{coarse?-1.0:-0.25,0};
    if(code==juce::KeyPress::rightKey)return MidiNoteNudge{coarse?1.0:0.25,0};
    if(code==juce::KeyPress::upKey)return MidiNoteNudge{0,coarse?12:1};
    if(code==juce::KeyPress::downKey)return MidiNoteNudge{0,coarse?-12:-1};
    return {};
}
}
