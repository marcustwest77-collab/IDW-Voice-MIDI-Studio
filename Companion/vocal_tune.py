"""Non-destructive, local vocal pitch correction for IDW Audio Lab.

The implementation uses librosa only for pitch analysis. Audio resynthesis is an
original dual-read-head granular shifter shared conceptually with the live V10
engine. The source WAV is never changed.
"""
from pathlib import Path
import math
import wave

NOTE_NAMES = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')
SCALE_MASKS = {
    'Chromatic': 0xFFF,
    'Major': sum(1 << n for n in (0, 2, 4, 5, 7, 9, 11)),
    'Natural minor': sum(1 << n for n in (0, 2, 3, 5, 7, 8, 10)),
    'Minor pentatonic': sum(1 << n for n in (0, 3, 5, 7, 10)),
}


def tuning_settings(root='C', scale='Chromatic', amount=85, humanize=40,
                    speed_ms=45, mix=100, output_db=0):
    if root not in NOTE_NAMES:
        raise ValueError('Root must be C through B, including sharp notes.')
    if scale not in SCALE_MASKS:
        raise ValueError('Choose Chromatic, Major, Natural minor or Minor pentatonic.')
    values = {'amount': amount, 'humanize': humanize, 'speed_ms': speed_ms,
              'mix': mix, 'output_db': output_db}
    limits = {'amount': (0, 100), 'humanize': (0, 100), 'speed_ms': (5, 200),
              'mix': (0, 100), 'output_db': (-12, 6)}
    for name, value in values.items():
        try:
            value = float(value)
        except (TypeError, ValueError):
            raise ValueError(name + ' must be a number.') from None
        lo, hi = limits[name]
        if not math.isfinite(value) or not lo <= value <= hi:
            raise ValueError('%s must be between %s and %s.' % (name, lo, hi))
        values[name] = value
    values.update(root=root, scale=scale)
    return values


def nearest_note(measured_midi, root='C', scale='Chromatic'):
    if not math.isfinite(measured_midi):
        return None
    settings = tuning_settings(root=root, scale=scale)
    root_number = NOTE_NAMES.index(settings['root'])
    mask = SCALE_MASKS[settings['scale']]
    allowed = [note for note in range(128) if mask & (1 << ((note - root_number) % 12))]
    return min(allowed, key=lambda note: (abs(note - measured_midi), note))


def correction_for_midi(measured_midi, root='C', scale='Chromatic', amount=85, humanize=40):
    settings = tuning_settings(root=root, scale=scale, amount=amount, humanize=humanize)
    target = nearest_note(measured_midi, root, scale)
    if target is None:
        return 0.0
    difference = target - measured_midi
    deadband = settings['humanize'] / 100.0 * .30
    if abs(difference) <= deadband:
        difference = 0.0
    else:
        difference -= math.copysign(deadband, difference)
    return max(-12.0, min(12.0, difference * settings['amount'] / 100.0))


def _read_pcm16(path):
    import numpy as np
    with wave.open(str(path), 'rb') as source:
        channels, width, rate = source.getnchannels(), source.getsampwidth(), source.getframerate()
        frames = source.getnframes()
        if width != 2 or channels not in (1, 2) or not 8000 <= rate <= 96000 or frames <= 0:
            raise ValueError('Tune Studio requires a 16-bit PCM mono/stereo WAV at 8-96 kHz.')
        raw = source.readframes(frames)
    audio = np.frombuffer(raw, dtype='<i2').astype(np.float32).reshape(-1, channels) / 32768.0
    return audio, rate


def _write_pcm16(path, audio, rate):
    import numpy as np
    output = np.clip(audio, -1.0, 1.0)
    pcm = np.rint(output * 32767.0).astype('<i2')
    with wave.open(str(path), 'wb') as target:
        target.setnchannels(pcm.shape[1]); target.setsampwidth(2); target.setframerate(rate)
        target.writeframes(pcm.tobytes())


def _shift_channel(audio, ratio, mix, rate):
    import numpy as np
    count = len(audio)
    minimum = max(2, round(rate * .003)); span = max(32, round(rate * .022)); fixed = minimum + span // 2
    difference = ratio - 1.0
    phase = np.mod(np.cumsum(np.abs(difference) / span), 1.0)
    other = np.mod(phase + .5, 1.0)
    delay_a = minimum + np.where(difference > 0, 1.0 - phase, phase) * span
    delay_b = minimum + np.where(difference > 0, 1.0 - other, other) * span
    timeline = np.arange(count, dtype=np.float64) + fixed
    source_x = np.arange(count, dtype=np.float64)
    read_a = np.interp(timeline - delay_a, source_x, audio, left=0.0, right=0.0)
    read_b = np.interp(timeline - delay_b, source_x, audio, left=0.0, right=0.0)
    wa = np.sin(np.pi * phase) ** 2; wb = np.sin(np.pi * other) ** 2
    wet = (read_a * wa + read_b * wb) / np.maximum(1.0e-6, wa + wb)
    dry = audio.astype(np.float64)
    return (dry + (wet - dry) * mix).astype(np.float32)


def tune_wav(source, destination, root='C', scale='Chromatic', amount=85,
             humanize=40, speed_ms=45, mix=100, output_db=0):
    """Correct a WAV locally and write a new PCM WAV; return a processing report."""
    import numpy as np
    try:
        import librosa
        from scipy.signal import lfilter
    except ImportError as error:
        raise RuntimeError('Tune Studio components are missing. Reinstall the complete IDW Audio Lab package.') from error
    settings = tuning_settings(root, scale, amount, humanize, speed_ms, mix, output_db)
    source, destination = Path(source), Path(destination)
    if source.resolve() == destination.resolve():
        raise ValueError('Choose a new output file; Tune Studio never overwrites the source.')
    audio, rate = _read_pcm16(source)
    mono = audio.mean(axis=1)
    hop = 256
    f0 = librosa.yin(mono, fmin=65, fmax=1000, sr=rate, frame_length=2048, hop_length=hop)
    rms = librosa.feature.rms(y=mono, frame_length=2048, hop_length=hop)[0]
    gate = max(10 ** (-55 / 20), float(np.percentile(rms, 10)) * 2.5)
    corrections = np.zeros_like(f0, dtype=np.float64)
    valid = np.isfinite(f0) & (rms[:len(f0)] >= gate)
    for index in np.flatnonzero(valid):
        midi = 69.0 + 12.0 * math.log2(float(f0[index]) / 440.0)
        corrections[index] = correction_for_midi(midi, root, scale, amount, humanize)
    frame_x = np.minimum(np.arange(len(f0)) * hop, len(mono) - 1)
    target = 2.0 ** (np.interp(np.arange(len(mono)), frame_x, corrections) / 12.0)
    alpha = 1.0 - math.exp(-1.0 / (rate * settings['speed_ms'] / 1000.0))
    ratio = lfilter([alpha], [1.0, -(1.0 - alpha)], target, zi=[1.0 - alpha])[0]
    wet_mix = settings['mix'] / 100.0
    processed = np.column_stack([_shift_channel(audio[:, channel], ratio, wet_mix, rate)
                                 for channel in range(audio.shape[1])])
    processed *= 10.0 ** (settings['output_db'] / 20.0)
    destination.parent.mkdir(parents=True, exist_ok=True)
    _write_pcm16(destination, processed, rate)
    return {'output': str(destination), 'seconds': len(audio) / rate, 'sample_rate': rate,
            'channels': audio.shape[1], 'tracked_frames': int(valid.sum()), 'settings': settings}
