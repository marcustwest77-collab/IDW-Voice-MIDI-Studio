#pragma once
#include <array>
#include <string_view>

namespace idw {
enum class DawGuide : int {
    builtIn = 1,
    proToolsWindows,
    flStudio,
    abletonLive,
    reaper,
    logicPro,
    otherPluginHost,
    standaloneMidi
};

struct DawGuideInfo {
    DawGuide id;
    const char* name;
    const char* instructions;
};

inline constexpr std::array<DawGuideInfo, 8> dawGuides{{
    {DawGuide::builtIn, "IDW built-in sound",
     "1. In standalone Options > Audio/MIDI Settings, select the microphone and headphones.\n"
     "2. Click Enable IDW instrument, then Send test note.\n"
     "3. If silent, check output selection, volume, and track monitoring.\n"
     "4. Once the tone is audible, sing steadily and watch INPUT and MIDI."},
    {DawGuide::proToolsWindows, "Pro Tools Intro / Windows",
     "Pro Tools does not load VST3. Run the IDW standalone beside Pro Tools; an AAX plugin is not included.\n"
     "1. Create and start a virtual MIDI port in loopMIDI (for example: IDW Voice MIDI).\n"
     "2. IDW Options > Audio/MIDI Settings: AudioBox USB 96 input 1, Main Out 1/2, and the virtual MIDI output.\n"
     "3. Pro Tools Setup > MIDI > Input Devices: enable that virtual port.\n"
     "4. Create a stereo Instrument track; choose the virtual port / channel 1 as MIDI input, load an instrument, and record-arm it.\n"
     "5. Click Mute IDW sounds / use DAW, then Send test note. Set both apps to the same sample rate.\n"
     "AudioBox: start near minimum gain, raise while singing, avoid the Clip LED; enable 48V only if your condenser mic requires it."},
    {DawGuide::flStudio, "FL Studio VST3",
     "1. Load IDW on the Mixer insert receiving the microphone.\n"
     "2. Set the IDW wrapper MIDI Output port to an unused number such as 10.\n"
     "3. Set the destination instrument wrapper MIDI Input port to the same number.\n"
     "4. Click Mute IDW sounds / use DAW, then Send test note on channel 1.\n"
     "Layers use lead 1, chords 2, bass 3, and drums 10. Start with MPE off."},
    {DawGuide::abletonLive, "Ableton Live VST3 / AU",
     "1. Put IDW on an audio track receiving the microphone and set Monitor to In.\n"
     "2. Create a MIDI track with an instrument. In MIDI From, choose the IDW audio track and its plugin output.\n"
     "3. Set the MIDI track Monitor to In. Click Mute IDW sounds / use DAW, then Send test note.\n"
     "4. If Live does not expose plugin MIDI output on your platform, use the standalone plus a virtual MIDI port."},
    {DawGuide::reaper, "REAPER VST3",
     "1. Put IDW before your instrument on one armed track, or route IDW MIDI to a separate instrument track.\n"
     "2. Choose the microphone input, enable input monitoring, and keep MIDI output enabled in the send.\n"
     "3. Click Mute IDW sounds / use DAW, then Send test note.\n"
     "4. For separate tracks, send MIDI All channels and disable audio in that send if needed."},
    {DawGuide::logicPro, "Logic Pro Audio Unit",
     "1. Load the IDW Audio Unit on a microphone audio channel and enable input monitoring.\n"
     "2. Route generated MIDI to a Software Instrument using Logic's supported MIDI-FX/environment routing.\n"
     "3. Click Mute IDW sounds / use DAW, then Send test note.\n"
     "4. If the project cannot route AU MIDI output, run the standalone with an IAC virtual MIDI bus."},
    {DawGuide::otherPluginHost, "Other VST3 / AU host",
     "1. Feed microphone audio into IDW and create a destination instrument track.\n"
     "2. Choose IDW generated MIDI as that track's input; enable monitoring or record arm.\n"
     "3. Click Mute IDW sounds / use DAW, then Send test note.\n"
     "4. Direct loading requires a Windows x64 VST3 host or a macOS VST3/AU host."},
    {DawGuide::standaloneMidi, "Standalone MIDI output",
     "1. Create a real or virtual MIDI port before opening IDW; IDW does not install a MIDI driver.\n"
     "2. In IDW Options > Audio/MIDI Settings, select that MIDI output.\n"
     "3. Enable the same port in the DAW and choose it on an armed/monitored instrument track.\n"
     "4. Click Mute IDW sounds / use DAW, then Send test note and confirm both MIDI activity and sound."}
}};

inline const DawGuideInfo& dawGuide(int id) {
    for (const auto& guide : dawGuides)
        if (static_cast<int>(guide.id) == id) return guide;
    return dawGuides.front();
}
}
