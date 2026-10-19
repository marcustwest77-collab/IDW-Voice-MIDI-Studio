#include "DAWGuides.h"
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    using namespace idw;
    if (dawGuides.size() != 8) throw std::runtime_error("Unexpected DAW guide count");
    for (std::size_t i=0;i<dawGuides.size();++i) {
        const auto& guide=dawGuides[i];
        if (static_cast<int>(guide.id) != static_cast<int>(i+1)) throw std::runtime_error("DAW guide IDs must stay contiguous");
        if (std::string(guide.name).empty() || std::string(guide.instructions).empty()) throw std::runtime_error("Empty DAW guide");
        if (&dawGuide(static_cast<int>(i+1)) != &guide) throw std::runtime_error("DAW guide lookup mismatch");
    }
    const auto proTools=std::string(dawGuide(static_cast<int>(DawGuide::proToolsWindows)).instructions);
    if (proTools.find("does not load VST3") == std::string::npos ||
        proTools.find("loopMIDI") == std::string::npos ||
        proTools.find("AudioBox USB 96") == std::string::npos ||
        proTools.find("48V") == std::string::npos)
        throw std::runtime_error("Pro Tools / AudioBox safety guidance incomplete");
    const auto standalone=std::string(dawGuide(static_cast<int>(DawGuide::standaloneMidi)).instructions);
    if (standalone.find("does not install a MIDI driver") == std::string::npos)
        throw std::runtime_error("Standalone virtual MIDI limitation missing");
    if (dawGuide(999).id != DawGuide::builtIn) throw std::runtime_error("Invalid DAW guide did not fall back safely");
    std::cout << "PASS: eight guided DAW routes including Pro Tools Intro / AudioBox USB 96\n";
}
