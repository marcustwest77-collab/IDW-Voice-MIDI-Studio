#pragma once
// V13 MIDI safety: dependency-free ledger for all sounding channel/note pairs.
// MIDI channels are 1..16; notes are 0..127. Designed to support emergency
// note-off on stop, bypass, device switch, or interrupted input streams.
#include <array>
#include <cstddef>
namespace idw {
class MidiNoteLedger {
public:
    bool noteOn(int channel, int note) noexcept {
        if (!valid(channel, note)) return false;
        const auto i = index(channel, note);
        const bool wasOff = !active_[i];
        active_[i] = true;
        return wasOff;
    }
    bool noteOff(int channel, int note) noexcept {
        if (!valid(channel, note)) return false;
        const auto i = index(channel, note);
        const bool wasOn = active_[i];
        active_[i] = false;
        return wasOn;
    }
    bool isActive(int channel, int note) const noexcept {
        return valid(channel, note) && active_[index(channel, note)];
    }
    template <typename Sink>
    std::size_t flush(Sink&& sendNoteOff) {
        std::size_t count = 0;
        for (int ch = 1; ch <= 16; ++ch)
            for (int note = 0; note < 128; ++note)
                if (noteOff(ch, note)) {
                    sendNoteOff(ch, note);
                    ++count;
                }
        return count;
    }
private:
    static bool valid(int channel, int note) noexcept {
        return channel >= 1 && channel <= 16 && note >= 0 && note < 128;
    }
    static std::size_t index(int channel, int note) noexcept {
        return static_cast<std::size_t>((channel - 1) * 128 + note);
    }
    std::array<bool, 16 * 128> active_{};
};
}
