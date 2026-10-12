#include "MidiNoteLedger.h"
#include <cassert>
#include <utility>
#include <vector>
#include <iostream>
int main() {
    idw::MidiNoteLedger ledger;
    assert(!ledger.isActive(1, 60));
    assert(ledger.noteOn(1, 60));
    assert(!ledger.noteOn(1, 60)); // Retrigger doesn't duplicate ledger entry.
    assert(ledger.noteOn(2, 60)); // Same note on separate channel.
    assert(ledger.noteOn(16, 127));
    assert(!ledger.noteOn(0, 60));
    assert(!ledger.noteOn(17, 60));
    assert(!ledger.noteOn(1, 128));
    assert(!ledger.noteOff(1, -1));
    assert(ledger.noteOff(1, 60));
    assert(!ledger.noteOff(1, 60));
    std::vector<std::pair<int, int>> sent;
    assert(ledger.flush([&](int ch, int note) { sent.emplace_back(ch, note); }) == 2);
    assert((sent == std::vector<std::pair<int, int>>{{2, 60}, {16, 127}}));
    assert(ledger.flush([&](int, int) { assert(false); }) == 0);
    assert(ledger.noteOn(1, 0));
    assert(ledger.noteOff(1, 0));
    std::cout << "PASS: V13 MIDI ledger safety checks\n";
}
