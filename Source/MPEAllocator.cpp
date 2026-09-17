#include "MPEAllocator.h"

int MPEAllocator::allocate(int note)
{
    if (map.count(note))
        return map[note];

    for (int channel = first; channel <= last; ++channel)
    {
        // MIDI channel 10 is reserved for beatbox drum output.
        if (channel == 10)
            continue;

        if (!used.count(channel))
        {
            map[note] = channel;
            used.insert(channel);
            return channel;
        }
    }

    // Fall back to channel 1 instead of colliding with the drum channel.
    return 1;
}

int MPEAllocator::channelFor(int note) const
{
    auto i = map.find(note);
    return i == map.end() ? 1 : i->second;
}

void MPEAllocator::release(int note)
{
    auto i = map.find(note);
    if (i != map.end())
    {
        used.erase(i->second);
        map.erase(i);
    }
}
