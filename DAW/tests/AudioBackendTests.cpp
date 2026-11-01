#include <juce_audio_devices/juce_audio_devices.h>
#include <iostream>

int main() {
#if JUCE_WINDOWS
    static_assert(JUCE_ASIO==1,"Windows workstation must include ASIO");
    juce::ScopedJuceInitialiser_GUI initialiser;
    juce::AudioDeviceManager manager;
    bool asio=false,windowsAudio=false;
    // Enumerate backend factories only. Never scan/open installed drivers,
    // initialise hardware, play sound, or request microphone permissions in CI.
    for(const auto* type:manager.getAvailableDeviceTypes()) {
        const auto name=type->getTypeName();
        asio=asio || name=="ASIO";
        windowsAudio=windowsAudio || name.startsWith("Windows Audio");
    }
    if(!asio || !windowsAudio) {
        std::cerr << "FAIL: expected both ASIO and Windows Audio backend factories\n";
        return 1;
    }
    std::cout << "PASS: Windows workstation configuration exposes ASIO and Windows Audio\n";
#else
    std::cout << "Windows backend configuration check is not applicable on this platform\n";
#endif
    return 0;
}
