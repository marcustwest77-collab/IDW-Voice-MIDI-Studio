# name=IDW Performance Router
"""Opt-in controller script. Notes only; never changes ports, inserts or project files.
Rename four existing Channel Rack instruments exactly as TARGET_NAMES below.
Attach this script to an existing MIDI input carrying IDW's standalone output.
"""
import channels

TARGET_NAMES = {0: 'IDW Lead', 1: 'IDW Chords', 2: 'IDW Bass', 9: 'IDW Drums'}
targets = {}
held = {}

def panic():
    for (channel, note), target in list(held.items()):
        try:
            if channels.getChannelName(target, True) == TARGET_NAMES[channel]:
                channels.midiNoteOn(target, note, 0)
        except Exception:
            pass
    held.clear()

def mapping():
    found_targets = {}
    for midi_channel, name in TARGET_NAMES.items():
        found = [i for i in range(channels.channelCount(1))
                 if channels.getChannelName(i, True) == name]
        if len(found) == 1:
            found_targets[midi_channel] = found[0]
    return found_targets

def scan():
    targets.clear()
    targets.update(mapping())
    print('IDW routes: ' + ', '.join(TARGET_NAMES[c] for c in targets))
    for c in TARGET_NAMES:
        if c not in targets:
            print('IDW: exactly one Channel Rack instrument must be named ' + TARGET_NAMES[c])

def OnInit():
    scan()

def OnDeInit():
    panic()

def OnRefresh(flags):
    # Release before rebuilding indexes; matching names prevents sending into an unrelated channel.
    new_targets = mapping()
    if new_targets != targets:
        panic()
        targets.clear()
        targets.update(new_targets)

def OnMidiMsg(event):
    c = event.midiChan
    if c not in TARGET_NAMES:
        return
    kind = event.status & 0xF0
    if kind == 0xB0 and event.data1 in (120, 123):
        panic()
        event.handled = True
        return
    if kind not in (0x80, 0x90):
        # Do not forward bends/CC into FL's currently selected instrument by accident.
        event.handled = True
        return
    key = (c, event.data1)
    velocity = event.data2 if kind == 0x90 else 0
    target = targets.get(c) if velocity else held.pop(key, None)
    if target is not None and channels.getChannelName(target, True) == TARGET_NAMES[c]:
        channels.midiNoteOn(target, event.data1, velocity)
        if velocity:
            held[key] = target
    event.handled = True
