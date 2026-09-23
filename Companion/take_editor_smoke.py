"""Real MIDI roundtrip tests, run inside the packaged executable."""
from pathlib import Path
import tempfile
import mido
from take_editor import Take

def create_fixture(path):
    midi=mido.MidiFile(type=1,ticks_per_beat=480)
    conductor=mido.MidiTrack([mido.MetaMessage('track_name',name='IDW take demo'),mido.MetaMessage('set_tempo',tempo=500000),mido.MetaMessage('set_tempo',tempo=600000,time=1920)])
    midi.tracks.append(conductor)
    events=[(0,mido.Message('program_change',channel=0,program=4)),(90,mido.Message('control_change',channel=0,control=1,value=42)),(160,mido.Message('pitchwheel',channel=0,pitch=123))]
    for beat,pitches in [(0,(60,64,67)),(4,(62,65,69)),(8,(59,62,67)),(12,(60,64,67))]:
        for pitch in pitches:
            events += [(beat*480+30,mido.Message('note_on',channel=0,note=pitch,velocity=80+(pitch%3)*10)),(beat*480+700,mido.Message('note_off',channel=0,note=pitch))]
    notes=mido.MidiTrack();last=0
    for tick,msg in sorted(events,key=lambda item:item[0]):notes.append(msg.copy(time=tick-last));last=tick
    midi.tracks.append(notes)
    midi.tracks.append(mido.MidiTrack([mido.Message('note_on',channel=9,note=36,velocity=100),mido.Message('note_off',channel=9,note=36,time=120)]))
    midi.save(str(path))

def other_events(path):
    result=[]
    for ti,track in enumerate(mido.MidiFile(str(path)).tracks):
        tick=0
        for msg in track:
            tick+=msg.time
            if msg.type not in ('note_on','note_off','end_of_track'):result.append((ti,tick,msg.copy(time=0)))
    return result

def run():
    with tempfile.TemporaryDirectory() as temp:
        root=Path(temp);source=root/'fixture.mid';create_fixture(source);original=source.read_bytes()
        take=Take.load(source);assert len(take.notes)==13
        take.transpose(12,1);take.quantize(.25,1)
        output=take.export_copy(root/'edited.mid');restored=Take.load(output)
        assert source.read_bytes()==original, 'Source MIDI was changed'
        assert other_events(source)==other_events(output), 'Controller/tempo/program data changed'
        assert restored.midi_type==1 and restored.ticks_per_beat==480
        for old,new in zip(take.original,restored.notes):
            assert new.channel==old.channel and new.track==old.track
            assert new.pitch==old.pitch+(12 if old.track==1 else 0)
            assert new.end-new.start==old.end-old.start
        for target in (source,output):
            try:take.export_copy(target)
            except (ValueError,FileExistsError):pass
            else:raise AssertionError('Editor overwrote an existing file')
        take.delete(take.notes[0].id);take.export_copy(root/'deleted.mid');assert len(Take.load(root/'deleted.mid').notes)==12
        take.undo();assert len(take.notes)==13
        # Moving a later original event to an earlier repeated-note boundary must end the old note first.
        midi=mido.MidiFile(type=0);track=mido.MidiTrack();midi.tracks.append(track)
        track.extend([mido.Message('note_on',note=60,velocity=80),mido.Message('note_off',note=60,time=100),mido.Message('note_on',note=60,velocity=90,time=100),mido.Message('note_off',note=60,time=100)])
        boundary=root/'boundary.mid';midi.save(str(boundary));edited=Take.load(boundary)
        edited.edit(0,60,80,300/480,100/480);copy=edited.export_copy(root/'boundary-edited.mid')
        assert len(Take.load(copy).notes)==2
    return 'PASS packaged MIDI editing / channels / tempo+CC+bend preservation / no overwrite / repeated-note boundaries'
