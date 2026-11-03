#pragma once
#include <juce_core/juce_core.h>
namespace idw::daw {
struct AudioDeviceRecallStatus {
    enum class Kind {normal,fallback,unavailable};
    Kind kind=Kind::normal;
    juce::String line,detail;
    bool needsAttention() const {return kind!=Kind::normal;}
};
// Report identity mismatches without reopening devices, changing preferences,
// arming tracks or making any assertion about physical audibility.
inline AudioDeviceRecallStatus describeAudioDeviceRecall(const juce::XmlElement* chosen,
        const juce::String& actualType,const juce::String& actualInput,const juce::String& actualOutput,bool deviceOpen){
    const bool saved=chosen && chosen->hasTagName("DEVICESETUP");
    const auto wantedType=saved?chosen->getStringAttribute("deviceType"):juce::String{};
    const auto legacy=saved?chosen->getStringAttribute("audioDeviceName"):juce::String{};
    const auto wantedInput=legacy.isNotEmpty()?legacy:saved?chosen->getStringAttribute("audioInputDeviceName"):juce::String{};
    const auto wantedOutput=legacy.isNotEmpty()?legacy:saved?chosen->getStringAttribute("audioOutputDeviceName"):juce::String{};
    const auto display=[](juce::String value){
        value=value.replaceCharacters("\r\n\t","   ").trim();
        return value.isEmpty()?juce::String("none"):value.substring(0,160);
    };
    const auto wanted="Saved: "+display(wantedType)+" | input: "+display(wantedInput)+" | output: "+display(wantedOutput);
    const auto actual="Active: "+display(actualType)+" | input: "+display(actualInput)+" | output: "+display(actualOutput);
    if(!deviceOpen)return {AudioDeviceRecallStatus::Kind::unavailable,
        "NO AUDIO DEVICE | Open AUDIO DEVICES",
        (saved?wanted+"\n":juce::String{})+"No audio device is open. Connect your interface and choose AUDIO DEVICES."+
            (saved?" The saved choice has not been cleared.":"")};
    if(saved && ((wantedType.isNotEmpty() && wantedType!=actualType) ||
                 (wantedInput.isNotEmpty() && wantedInput!=actualInput) ||
                 (wantedOutput.isNotEmpty() && wantedOutput!=actualOutput))){
        return {AudioDeviceRecallStatus::Kind::fallback,
            "AUDIO FALLBACK | "+display(actualOutput.isNotEmpty()?actualOutput:actualInput)+" | Open AUDIO DEVICES",
            wanted+"\n"+actual+"\nThe saved interface is not active. Choose AUDIO DEVICES to select an interface. This status does not change your saved choice."};
    }
    return {};
}
}
