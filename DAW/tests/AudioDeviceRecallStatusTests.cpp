#include "AudioDeviceRecallStatus.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    using Kind=AudioDeviceRecallStatus::Kind;
    juce::XmlElement chosen("DEVICESETUP");chosen.setAttribute("deviceType","ASIO");
    chosen.setAttribute("audioInputDeviceName","AudioBox microphone");chosen.setAttribute("audioOutputDeviceName","AudioBox headphones");
    const auto before=chosen.toString();
    const auto normal=describeAudioDeviceRecall(&chosen,"ASIO","AudioBox microphone","AudioBox headphones",true);
    require(!normal.needsAttention() && normal.kind==Kind::normal && normal.line.isEmpty() && normal.detail.isEmpty(),"matched identity warned");
    const auto fallback=describeAudioDeviceRecall(&chosen,"Windows Audio","Realtek microphone","Realtek headphones",true);
    require(fallback.kind==Kind::fallback && fallback.needsAttention() && fallback.line.contains("AUDIO FALLBACK") &&
        fallback.line.contains("Realtek headphones") && fallback.line.contains("AUDIO DEVICES") &&
        fallback.detail.contains("AudioBox headphones") && fallback.detail.contains("ASIO") && fallback.detail.contains("Windows Audio"),"fallback diagnosis lacks wanted/active route");
    require(describeAudioDeviceRecall(&chosen,"ASIO","Different input","AudioBox headphones",true).kind==Kind::fallback,"input-only mismatch missed");
    require(describeAudioDeviceRecall(&chosen,"Other backend","AudioBox microphone","AudioBox headphones",true).kind==Kind::fallback,"backend-only mismatch missed");
    const auto absent=describeAudioDeviceRecall(&chosen,"ASIO","AudioBox microphone","AudioBox headphones",false);
    require(absent.kind==Kind::unavailable && absent.line.contains("NO AUDIO DEVICE") && absent.detail.contains("AudioBox headphones"),"closed device described as restored");
    const auto first=describeAudioDeviceRecall(nullptr,"Windows Audio","Mic","Speakers",true);
    require(first.kind==Kind::normal && first.line.isEmpty(),"first default startup called a failed recall");
    const auto noDevice=describeAudioDeviceRecall(nullptr,"","","",false);
    require(noDevice.kind==Kind::unavailable && !noDevice.detail.contains("Saved:") && !noDevice.detail.contains("saved choice"),"no-device default implied a saved choice");
    juce::XmlElement explicitNew("DEVICESETUP");explicitNew.setAttribute("deviceType","Windows Audio");
    explicitNew.setAttribute("audioInputDeviceName","Realtek microphone");explicitNew.setAttribute("audioOutputDeviceName","Realtek headphones");
    require(!describeAudioDeviceRecall(&explicitNew,"Windows Audio","Realtek microphone","Realtek headphones",true).needsAttention(),"explicit new choice did not clear warning");
    juce::XmlElement legacy("DEVICESETUP");legacy.setAttribute("deviceType","ASIO");legacy.setAttribute("audioDeviceName","Legacy interface");
    legacy.setAttribute("audioInputDeviceName","ignored newer field");legacy.setAttribute("audioOutputDeviceName","ignored newer field");
    require(!describeAudioDeviceRecall(&legacy,"ASIO","Legacy interface","Legacy interface",true).needsAttention(),"JUCE legacy name precedence differs");
    juce::XmlElement outputOnly("DEVICESETUP");outputOnly.setAttribute("deviceType","CoreAudio");outputOnly.setAttribute("audioOutputDeviceName",u8"Output \u03b2");
    require(!describeAudioDeviceRecall(&outputOnly,"CoreAudio","Default input",u8"Output \u03b2",true).needsAttention(),"unspecified input or Unicode mismatch");
    const auto safe=describeAudioDeviceRecall(&chosen,"Other","Input","Speakers\r\n\t"+juce::String::repeatedString("x",500),true);
    require(!safe.line.containsAnyOf("\r\n\t") && safe.line.length()<240,"status line contains control characters or unbounded device name");
    juce::XmlElement wrongRoot("PROJECT");
    require(!describeAudioDeviceRecall(&wrongRoot,"ASIO","Input","Output",true).needsAttention(),"unrelated XML treated as saved device state");
    require(chosen.toString()==before,"status formatting changed preferred setup");
    std::cout<<"PASS audio recall status: wanted/active identities, fallback, no device, defaults, explicit replacement, legacy/Unicode names and bounded display; no audio drivers\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
