"""Real model gate for the proposed Windows workflow; not run in local contract tests."""
import array
import math
import sys
import tempfile
import wave
from pathlib import Path
import pretty_midi

def run():
    from audio_lab import transcribe
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp); path = root / 'triad.wav'; sr = 22050
        values = array.array('h')
        for i in range(sr * 3):
            t = i / sr
            envelope = min(1, t / .04, max(0, (2.5-t) / .2))
            signal = sum(math.sin(2*math.pi*f*t) + .25*math.sin(4*math.pi*f*t) for f in (261.6256,329.6276,391.9954))
            values.append(int(signal * envelope * 4500))
        if sys.byteorder != 'little': values.byteswap()
        with wave.open(str(path), 'wb') as w:
            w.setparams((1,2,sr,0,'NONE','not compressed'));w.writeframes(values.tobytes())
        result = transcribe(path, root)
        midi = pretty_midi.PrettyMIDI(str(result/'transcription.mid'))
        notes = [n for instrument in midi.instruments for n in instrument.notes]
        assert notes, 'Model emitted no notes'
        assert any(a.pitch != b.pitch and min(a.end,b.end)-max(a.start,b.start)>.1 for a in notes for b in notes), 'Model did not recover overlapping pitches'
        return ('PASS real Basic Pitch inference: readable MIDI with overlapping pitches on synthetic triad; not a music-quality benchmark')
