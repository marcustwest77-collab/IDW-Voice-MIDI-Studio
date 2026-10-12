#include "MidiNoteLedger.h"
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>
// Unlike assert(), these checks remain active in CMake Release configurations.
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
int main() {
    using Pair = std::pair<int, int>;
    idw::MidiNoteLedger ledger;
    require(!ledger.isActive(1, 60), "initial state");
    require(ledger.noteOn(1, 60), "first note on");
    require(!ledger.noteOn(1, 60), "duplicate detection");
    require(ledger.noteOn(2, 60), "channel independence");
    require(ledger.noteOn(16, 127), "last valid channel/note");
    require(ledger.noteOn(1, 0), "first valid note");
    require(!ledger.noteOn(0, 60), "channel zero rejected");
    require(!ledger.noteOn(17, 60), "channel seventeen rejected");
    require(!ledger.noteOn(1, 128), "invalid note rejected");
    require(!ledger.noteOff(1, -1), "negative note rejected");
    require(ledger.noteOff(1, 60), "note off");
    require(!ledger.noteOff(1, 60), "duplicate note off");
    std::vector<Pair> sent;
    require(ledger.flush([&](int ch, int note) { sent.emplace_back(ch, note); }) == 3, "flush count");
    require(sent == std::vector<Pair>({{1, 0}, {2, 60}, {16, 127}}), "flush order");
    require(ledger.flush([&](int, int) { throw std::runtime_error("unexpected flush callback"); }) == 0, "idempotent flush");
    require(ledger.noteOn(10, 36), "drum note on");
    require(ledger.isActive(10, 36), "drum state");
    require(ledger.noteOff(10, 36), "drum note off");
    std::cout << "PASS: V13 MIDI ledger regression checks in Debug and Release\n";
}
