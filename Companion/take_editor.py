"""Non-destructive Standard MIDI take editing. Times are integer MIDI ticks."""
from dataclasses import dataclass, replace
from pathlib import Path
import io
import math

MAX_TICK = 0x0FFFFFFF

@dataclass(frozen=True)
class Note:
    id: int
    track: int
    channel: int
    pitch: int
    velocity: int
    start: int
    end: int
    on_index: int
    off_index: int


def validate(notes):
    if len(notes) > 20000: raise ValueError('The take editor supports up to 20,000 notes.')
    previous = {}
    for n in sorted(notes, key=lambda n:(n.track, n.channel, n.pitch, n.start, n.end)):
        if not all(isinstance(v, int) for v in (n.pitch,n.velocity,n.start,n.end)):
            raise ValueError('Pitch, velocity and tick positions must be integers.')
        if not (0 <= n.pitch <= 127 and 1 <= n.velocity <= 127 and 0 <= n.start < n.end <= MAX_TICK):
            raise ValueError('Use MIDI pitch 0–127, velocity 1–127 and a positive note length.')
        key = (n.track,n.channel,n.pitch)
        if previous.get(key, -1) > n.start:
            raise ValueError('This would overlap repeated notes of the same pitch/channel. Shorten or move a note first.')
        previous[key] = n.end


class Take:
    def __init__(self, notes, ticks_per_beat=480, source=None, tracks=None, midi_type=1):
        validate(notes)
        if not 1 <= ticks_per_beat <= 32767: raise ValueError('SMPTE-timed MIDI is not supported; use a beat-based MIDI file.')
        self.notes = tuple(notes); self.original = self.notes; self.saved = self.notes
        self.ticks_per_beat = ticks_per_beat; self.source = Path(source) if source else None
        self.tracks = tracks or []; self.midi_type = midi_type
        self.history = []; self.future = []

    @property
    def dirty(self): return self.notes != self.saved

    def change(self, notes):
        notes = tuple(notes); validate(notes)
        if notes == self.notes: return
        self.history.append(self.notes); self.history = self.history[-20:]
        self.notes = notes; self.future.clear()

    def undo(self):
        if self.history: self.future.append(self.notes); self.notes = self.history.pop()

    def redo(self):
        if self.future: self.history.append(self.notes); self.notes = self.future.pop()

    def reset(self): self.change(self.original)

    def edit(self, note_id, pitch, velocity, start_beats, length_beats):
        start, length = float(start_beats), float(length_beats)
        if not (math.isfinite(start) and math.isfinite(length)) or start < 0 or length <= 0:
            raise ValueError('Use finite, non-negative start beats and a positive length.')
        pitch, velocity = int(pitch), int(velocity)
        start_tick = round(start * self.ticks_per_beat)
        length_tick = round(length * self.ticks_per_beat)
        if not any(n.id == note_id for n in self.notes): raise ValueError('Select a note first.')
        self.change(replace(n,pitch=pitch,velocity=velocity,start=start_tick,end=start_tick+length_tick) if n.id==note_id else n for n in self.notes)

    def delete(self, note_id): self.change(n for n in self.notes if n.id != note_id)

    def transpose(self, semitones, track=None):
        amount = int(semitones)
        if not -48 <= amount <= 48: raise ValueError('Transpose between -48 and +48 semitones.')
        self.change(replace(n,pitch=n.pitch+amount) if n.channel!=9 and (track is None or n.track==track) else n for n in self.notes)

    def quantize(self, grid_beats, track=None):
        grid = float(grid_beats)
        if not math.isfinite(grid) or not 1/32 <= grid <= 4: raise ValueError('Choose a grid between 1/32 and 4 quarter-note beats.')
        step = grid * self.ticks_per_beat
        def snap(n):
            if track is not None and n.track!=track: return n
            start = round(math.floor(n.start/step + .5)*step)
            return replace(n,start=start,end=start+n.end-n.start)
        self.change(snap(n) for n in self.notes)

    @classmethod
    def load(cls, path):
        import mido
        path = Path(path)
        if not path.is_file() or path.stat().st_size > 4*1024*1024:
            raise ValueError('Choose a MIDI file up to 4 MiB.')
        midi = mido.MidiFile(str(path))
        if midi.type not in (0,1): raise ValueError('Asynchronous type-2 MIDI is not supported.')
        if len(midi.tracks)>128: raise ValueError('Choose a MIDI file with at most 128 tracks.')
        tracks = []; notes = []; total = 0
        for ti, track in enumerate(midi.tracks):
            tick = 0; events = []; pending = {}
            for ei, msg in enumerate(track):
                total += 1
                if total > 100000: raise ValueError('This MIDI exceeds the 100,000-event editor limit.')
                tick += msg.time
                if not 0 <= tick <= MAX_TICK: raise ValueError('MIDI timeline exceeds the editor limit.')
                events.append((tick,msg.copy()))
                if msg.type=='note_on' and msg.velocity>0:
                    key=(msg.channel,msg.note)
                    if key in pending: raise ValueError('Overlapping repeated pitches on one channel require editing in your DAW.')
                    pending[key]=(tick,ei,msg.velocity)
                elif msg.type=='note_off' or (msg.type=='note_on' and msg.velocity==0):
                    key=(msg.channel,msg.note)
                    if key in pending:
                        start,on,velocity=pending.pop(key)
                        notes.append(Note(len(notes),ti,msg.channel,msg.note,velocity,start,tick,on,ei))
            if pending: raise ValueError('This MIDI has notes without note-offs. Repair it in your DAW first.')
            tracks.append(events)
        return cls(notes,midi.ticks_per_beat,path,tracks,midi.type)

    def export_copy(self, path):
        import mido
        path = Path(path)
        if path.suffix.lower() not in ('.mid','.midi'): path=path.with_suffix('.mid')
        if self.source and path.resolve()==self.source.resolve(): raise ValueError('Choose a new filename; the original take is protected.')
        if path.exists(): raise FileExistsError('Choose a new filename. Existing files are never overwritten.')
        midi=mido.MidiFile(type=self.midi_type,ticks_per_beat=self.ticks_per_beat)
        original_events={(n.track,i) for n in self.original for i in (n.on_index,n.off_index)}
        replacements={}
        for n in self.notes:
            for ei,tick in ((n.on_index,n.start),(n.off_index,n.end)):
                msg=self.tracks[n.track][ei][1].copy(note=n.pitch)
                if ei==n.on_index: msg.velocity=n.velocity
                replacements[(n.track,ei)]=(tick,msg)
        for ti,events in enumerate(self.tracks):
            ordered=[]; end_tick=0
            for ei,(tick,msg) in enumerate(events):
                if msg.type=='end_of_track': end_tick=max(end_tick,tick); continue
                key=(ti,ei)
                if key in original_events:
                    if key not in replacements: continue
                    tick,msg=replacements[key]
                ordered.append((tick,ei,msg))
            ordered.sort(key=lambda e:(e[0],e[1]))
            # At a moved repeated-note boundary, emit its old note-off before its new note-on.
            index=0
            while index<len(ordered):
                end=index+1
                while end<len(ordered) and ordered[end][0]==ordered[index][0]: end+=1
                group=ordered[index:end]
                first_off={}
                for k,(_,_,msg) in enumerate(group):
                    if msg.type=='note_off' or (msg.type=='note_on' and msg.velocity==0):
                        first_off.setdefault((msg.channel,msg.note),k)
                deferred={};fixed=[]
                for k,event in enumerate(group):
                    msg=event[2]
                    if msg.type=='note_on' and msg.velocity>0 and first_off.get((msg.channel,msg.note),-1)>k:
                        deferred[(msg.channel,msg.note)]=event;continue
                    fixed.append(event)
                    if msg.type=='note_off' or (msg.type=='note_on' and msg.velocity==0):
                        waiting=deferred.pop((msg.channel,msg.note),None)
                        if waiting is not None:fixed.append(waiting)
                group=fixed
                ordered[index:end]=group; index=end
            track=mido.MidiTrack(); last=0
            for tick,_,msg in ordered: track.append(msg.copy(time=tick-last)); last=tick
            track.append(mido.MetaMessage('end_of_track',time=max(last,end_tick)-last));midi.tracks.append(track)
        data=io.BytesIO();midi.save(file=data)
        created=False
        try:
            with path.open('xb') as handle:
                created=True;handle.write(data.getvalue())
        except Exception:
            if created: path.unlink(missing_ok=True)
            raise
        self.saved=self.notes
        return path
