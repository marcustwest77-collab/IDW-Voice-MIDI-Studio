"""IDW Audio Lab: offline file transcription + explicitly requested Kits jobs.
No network calls occur on import, startup, inspection or transcription.
"""
from pathlib import Path
import array
import json
import math
import sys
import uuid
import wave
import urllib.request
import urllib.error

API = 'https://arpeggi.io/api/kits/v1/'
MAX_AUDIO_BYTES = 25 * 1024 * 1024


def inspect_wav(path):
    path = Path(path)
    if path.suffix.lower() != '.wav' or not path.is_file():
        raise ValueError('Choose a WAV file exported as 16-bit PCM.')
    if path.stat().st_size > MAX_AUDIO_BYTES:
        raise ValueError('This Audio Lab candidate accepts files up to 25 MiB.')
    with wave.open(str(path), 'rb') as wav:
        channels, width, rate, frames = wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()
        if width != 2 or channels not in (1, 2) or not 8000 <= rate <= 96000:
            raise ValueError('Export 16-bit PCM WAV, mono or stereo, 8–96 kHz.')
        duration = frames / rate
        if not 0 < duration <= 600:
            raise ValueError('Choose between 0 and 600 seconds of audio.')
        peak = total = clipped = count = 0
        while True:
            raw = wav.readframes(8192)
            if not raw:
                break
            samples = array.array('h', raw)
            if sys.byteorder != 'little':
                samples.byteswap()
            for value in samples:
                amplitude = abs(value)
                peak = max(peak, amplitude)
                total += value * value
                clipped += amplitude >= 32760
                count += 1
        if count != frames * channels:
            raise ValueError('The WAV is truncated or has an invalid frame count.')
        return {'seconds': duration, 'sample_rate': rate, 'channels': channels,
                'peak_dbfs': round(20 * math.log10(max(peak / 32768, 1e-6)), 2),
                'rms_dbfs': round(20 * math.log10(max(math.sqrt(total / count) / 32768, 1e-6)), 2),
                'clipped_samples': clipped, 'silent': peak == 0}


def transcription_settings(onset=.5, frame=.3, minimum_ms=127.7, tempo=120):
    values = dict(onset=onset, frame=frame, minimum_ms=minimum_ms, tempo=tempo)
    ranges = dict(onset=(.001, 1), frame=(.001, 1), minimum_ms=(20, 2000), tempo=(40, 240))
    for name, value in values.items():
        try: value = float(value)
        except (ValueError, TypeError): raise ValueError(name + ' must be a number.') from None
        lo, hi = ranges[name]
        if not math.isfinite(value) or not lo <= value <= hi:
            raise ValueError('%s must be between %s and %s.' % (name, lo, hi))
        values[name] = value
    return values


def transcribe(path, destination, onset=.5, frame=.3, minimum_ms=127.7, tempo=120):
    info = inspect_wav(path)
    if info['silent']:
        raise ValueError('The recording is digital silence. Check the input before transcribing.')
    settings = transcription_settings(onset, frame, minimum_ms, tempo)
    onset, frame, minimum_ms, tempo = (settings[k] for k in ('onset', 'frame', 'minimum_ms', 'tempo'))
    try:
        from basic_pitch.inference import predict
    except ImportError as error:
        raise RuntimeError('Local transcription model is not installed. Run Setup-Transcription.cmd once; no audio upload is needed.') from error
    # Basic Pitch is a file model; this never runs in the audio callback.
    _, midi, notes = predict(str(path), onset_threshold=onset, frame_threshold=frame,
                             minimum_note_length=minimum_ms, midi_tempo=tempo)
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    job = destination / ('IDW-transcription-' + uuid.uuid4().hex[:12])
    job.mkdir()
    midi.write(str(job / 'transcription.mid'))
    # Plain note table keeps the unquantized performance available for inspection.
    import csv
    with (job / 'notes.csv').open('w', newline='', encoding='utf-8') as handle:
        writer = csv.writer(handle)
        writer.writerow(['start_seconds', 'end_seconds', 'midi_note', 'amplitude'])
        for start, end, pitch, amplitude, *_ in notes:
            writer.writerow([float(start), float(end), int(pitch), float(amplitude)])
    (job / 'report.json').write_text(json.dumps({'engine': 'Spotify Basic Pitch', 'mode': 'offline file transcription', 'notes': len(notes), 'input': info, 'settings': settings}, indent=2), encoding='utf-8')
    return job


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise RuntimeError('Provider redirected the API request; stopped without forwarding credentials.')


class KitsClient:
    def __init__(self, key, opener=None):
        self.key = key.strip()
        if not self.key or '\r' in self.key or '\n' in self.key:
            raise ValueError('Enter your Kits API key in Audio Lab; it is kept only in memory.')
        self.opener = opener or urllib.request.build_opener(NoRedirect())

    def _request(self, endpoint, data=None, content_type=None):
        headers = {'Authorization': 'Bearer ' + self.key, 'Accept': 'application/json'}
        if content_type:
            headers['Content-Type'] = content_type
        request = urllib.request.Request(API + endpoint, data=data, headers=headers)
        try:
            with self.opener.open(request, timeout=120 if data else 30) as response:
                raw = response.read(2 * 1024 * 1024 + 1)
                if len(raw) > 2 * 1024 * 1024:
                    raise RuntimeError('Provider response exceeded the size limit.')
                return json.loads(raw)
        except urllib.error.HTTPError as error:
            raise RuntimeError('Kits returned HTTP %s. Check account access, credits and API key. Uploads are not retried automatically.' % error.code) from None
        except urllib.error.URLError:
            raise RuntimeError('Could not reach Kits. If submitting, check your Kits job history before retrying to avoid duplicate charges.') from None

    def models(self):
        return self._request('voice-models')

    def job(self, job_id):
        return self._request('voice-conversions/' + str(self._id(job_id)))

    @staticmethod
    def _id(value):
        text = str(value)
        if not text.isascii() or not text.isdecimal() or int(text) <= 0:
            raise ValueError('Enter a positive numeric model or job ID from Kits.')
        return int(text)

    def convert(self, path, model_id, upload_approved=False):
        if not upload_approved:
            raise PermissionError('Approve this file upload before sending audio to Kits.')
        model = self._id(model_id)
        info = inspect_wav(path)
        if info['silent']:
            raise ValueError('The source WAV is silent.')
        boundary = 'IDW' + uuid.uuid4().hex
        body = ('--' + boundary + '\r\nContent-Disposition: form-data; name="voiceModelId"\r\n\r\n' + str(model) + '\r\n--' + boundary + '\r\nContent-Disposition: form-data; name="soundFile"; filename="idw-input.wav"\r\nContent-Type: audio/wav\r\n\r\n').encode()
        body += Path(path).read_bytes() + ('\r\n--' + boundary + '--\r\n').encode()
        return self._request('voice-conversions', body, 'multipart/form-data; boundary=' + boundary)


def main(gui_smoke=False, preview_path=None):
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    import queue
    import threading
    import webbrowser
    root = tk.Tk()
    root.title('IDW Audio Lab — V6.4')
    root.geometry('1020x900')
    root.minsize(800, 760)
    panel = ttk.Frame(root, padding=18)
    panel.pack(fill='both', expand=True)
    ttk.Label(panel, text='IDW / AUDIO LAB', font=('Segoe UI', 20, 'bold')).pack(anchor='w')
    ttk.Label(panel, text='Offline transcription • recording inspection • MIDI take editing • optional cloud voices').pack(anchor='w', pady=(0, 12))
    chosen = tk.StringVar()
    file_row = ttk.Frame(panel); file_row.pack(fill='x')
    ttk.Entry(file_row, textvariable=chosen).pack(side='left', fill='x', expand=True)
    def choose():
        path = filedialog.askopenfilename(filetypes=[('PCM WAV', '*.wav')])
        if path:
            chosen.set(path)
    ttk.Button(file_row, text='Choose WAV', command=choose).pack(side='left', padx=8)
    ttk.Label(panel, text='16-bit PCM WAV • mono/stereo • up to 10 minutes and 25 MiB').pack(anchor='w')
    notebook = ttk.Notebook(panel); notebook.pack(fill='x', pady=16)
    local = ttk.Frame(notebook, padding=14); cloud = ttk.Frame(notebook, padding=14)
    notebook.add(local, text='Local recording tools'); notebook.add(cloud, text='Cloud / trained voices')
    ttk.Label(local, text='Transcribe chords or a single instrument into MIDI. Full mixes may need isolated stems first.\nThe Windows portable edition includes the local model; this is file processing, not live polyphonic tracking.').pack(anchor='w')
    settings_row = ttk.Frame(local); settings_row.pack(fill='x', pady=10)
    settings_vars = {}
    for column, (name, label, default) in enumerate([
            ('tempo', 'MIDI tempo (BPM)', '120'), ('onset', 'Note onset threshold', '0.5'),
            ('frame', 'Sustain threshold', '0.3'), ('minimum_ms', 'Minimum note (ms)', '127.7')]):
        variable = tk.StringVar(value=default); settings_vars[name] = variable
        ttk.Label(settings_row, text=label).grid(row=0, column=column, padx=6, sticky='w')
        ttk.Entry(settings_row, textvariable=variable, width=17).grid(row=1, column=column, padx=6, sticky='w')
    ttk.Label(local, text='Lower thresholds detect quieter notes but may add false notes. Longer minimums remove short notes.\nTempo sets the MIDI tempo map; it does not quantize or change the audio speed.').pack(anchor='w')
    latest_midi = [None]
    result_queue = queue.Queue(); busy = False; actions = []
    output = tk.Text(panel, height=6, wrap='word'); output.pack(fill='both', expand=True)
    def write(text):
        output.insert('end', str(text) + '\n'); output.see('end')
    def run(fn):
        nonlocal busy
        if busy: return
        busy = True
        for button in actions: button.configure(state='disabled')
        write('Working…')
        def work():
            try: result_queue.put(('ok', fn()))
            except Exception as error: result_queue.put(('error', str(error)))
        threading.Thread(target=work, daemon=True).start()
    def poll():
        nonlocal busy
        try:
            status, value = result_queue.get_nowait()
            if status == 'ok' and isinstance(value, tuple) and value[0] == 'transcription':
                latest_midi[0] = Path(value[1]) / 'transcription.mid'
                write('Saved MIDI, note table and report in: ' + value[1] + '\nClick Review latest MIDI to correct notes.')
            else: write(('ERROR: ' if status == 'error' else '') + str(value))
            busy = False
            for button in actions: button.configure(state='normal')
        except queue.Empty: pass
        root.after(150, poll)
    def add_button(parent, label, fn):
        button = ttk.Button(parent, text=label, command=fn); button.pack(anchor='w', pady=5); actions.append(button)
    def inspect():
        path = chosen.get(); run(lambda: json.dumps(inspect_wav(path), indent=2))
    def local_transcribe():
        path = chosen.get()
        try: settings = transcription_settings(**{k: v.get() for k, v in settings_vars.items()})
        except ValueError as error: messagebox.showerror('Check transcription settings', str(error)); return
        destination = filedialog.askdirectory(title='Folder for the new MIDI take')
        if destination: run(lambda: ('transcription', str(transcribe(path, destination, **settings))))
    from take_editor_ui import TakeEditor
    review = TakeEditor(notebook, write); notebook.add(review, text='MIDI take editor')
    def review_latest():
        if latest_midi[0] is None: write('Transcribe a WAV first, or open an existing .mid in MIDI take editor.'); return
        if review.confirm_discard():
            review.attempt(lambda: review.load_path(latest_midi[0])); notebook.select(review)
    add_button(local, 'Review latest MIDI', review_latest)
    add_button(local, 'Inspect recording levels', inspect)
    add_button(local, 'Transcribe WAV to MIDI locally', local_transcribe)
    ttk.Label(cloud, text='Uses an existing trained or licensed Kits voice model. Training is done in Kits.\nAn account/key and provider credits may be required. Nothing uploads until you confirm.').pack(anchor='w')
    key = tk.StringVar(); model = tk.StringVar(); job = tk.StringVar()
    for label, variable, masked in [('API key (memory only)', key, True), ('Voice model ID', model, False), ('Job ID to check', job, False)]:
        row = ttk.Frame(cloud); row.pack(fill='x', pady=2)
        ttk.Label(row, text=label, width=25).pack(side='left')
        ttk.Entry(row, textvariable=variable, show='*' if masked else '').pack(side='left', fill='x', expand=True)
    def list_models():
        token = key.get(); run(lambda: json.dumps(KitsClient(token).models(), indent=2))
    def upload():
        path, token, voice = chosen.get(), key.get(), model.get()
        try: inspect_wav(path); KitsClient._id(voice)
        except Exception as e: messagebox.showerror('Check input', str(e)); return
        if messagebox.askyesno('Approve this cloud job', 'Send the selected WAV to Kits / arpeggi.io using voice model ' + voice + '?\n\n' + path + '\n\nProceed only with audio and a voice you are authorized to use. Provider charges and retention terms apply. This is not local processing.'):
            def submit():
                response = KitsClient(token).convert(path, voice, upload_approved=True)
                # No key, source audio or signed result URL is written to disk.
                receipt = Path.home() / 'IDW Audio Lab' / 'Jobs'; receipt.mkdir(parents=True, exist_ok=True)
                data = {k: response.get(k) for k in ('id', 'status', 'voiceModelId')}
                (receipt / ('job-' + uuid.uuid4().hex + '.json')).write_text(json.dumps(data), encoding='utf-8')
                return 'Job submitted. Copy its ID to check progress. Receipt: ' + str(receipt) + '\n' + json.dumps(data, indent=2)
            run(submit)
    def check_job():
        token, number = key.get(), job.get()
        run(lambda: json.dumps(KitsClient(token).job(number), indent=2))
    add_button(cloud, 'List available voice models (first page)', list_models)
    add_button(cloud, 'Upload WAV and convert voice…', upload)
    add_button(cloud, 'Check job / show download URL', check_job)
    ttk.Button(cloud, text='Open Kits to create your own voice', command=lambda:webbrowser.open('https://app.kits.ai/')).pack(anchor='w', pady=5)
    def close():
        if not review.confirm_discard(): return
        if busy and not messagebox.askyesno('Work is running', 'Close Audio Lab? Local processing will stop. An accepted cloud job may continue in your Kits account.'):
            return
        key.set(''); root.destroy()
    root.protocol('WM_DELETE_WINDOW', close)
    write('Local tools do not upload audio. Choose WAV, inspect the levels, then transcribe. Cloud conversion requires your Kits account.')
    poll()
    if gui_smoke:
        import tempfile
        from take_editor_smoke import create_fixture
        fixture_folder = tempfile.TemporaryDirectory()
        fixture = Path(fixture_folder.name) / 'IDW-demo-take.mid'
        create_fixture(fixture);review.load_path(fixture);notebook.select(review)
        root.geometry('800x800+0+0');root.update()
        review.draw();root.update()
        def check_bounds(widget):
            for child in widget.winfo_children():
                if child.winfo_ismapped():
                    assert child.winfo_x() >= -2 and child.winfo_y() >= -2
                    assert child.winfo_x()+child.winfo_width() <= widget.winfo_width()+2, 'Editor control exceeds width: '+str(child)
                    assert child.winfo_y()+child.winfo_height() <= widget.winfo_height()+2, 'Editor control exceeds height: '+str(child)
                    check_bounds(child)
        check_bounds(review)
        assert review.canvas.find_withtag('note'), 'Piano roll rendered no notes'
        if preview_path:
            from PIL import ImageGrab
            root.lift();root.update()
            x,y=review.winfo_rootx(),review.winfo_rooty()
            ImageGrab.grab(bbox=(x,y,x+review.winfo_width(),y+review.winfo_height())).save(preview_path)
        root.after(1000, root.destroy)
    root.mainloop()

def cli():
    import argparse
    parser = argparse.ArgumentParser(description='IDW Audio Lab 6.4')
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--gui-smoke', action='store_true')
    parser.add_argument('--test-report', type=Path)
    parser.add_argument('--preview-path', type=Path)
    args = parser.parse_args()
    # Windowed executables have no stdout. Libraries still expect a writable stream.
    import os
    if sys.stdout is None: sys.stdout = open(os.devnull, 'w')
    if sys.stderr is None: sys.stderr = open(os.devnull, 'w')
    try:
        if args.self_test:
            from model_selftest import run
            result = run()
            from take_editor_smoke import run as editor_test
            result += '\n' + editor_test()
            main(gui_smoke=True, preview_path=args.preview_path)
            result += '\nPASS packaged Tk GUI startup / piano-roll drawing / minimum-width editor bounds'
            if args.test_report: args.test_report.write_text(result, encoding='utf-8')
            print(result)
        else: main(gui_smoke=args.gui_smoke)
    except Exception:
        import traceback
        error = traceback.format_exc()
        if args.test_report: args.test_report.write_text(error, encoding='utf-8')
        else:
            from tkinter import messagebox
            messagebox.showerror('Audio Lab could not start', error)
        return 1
    return 0

if __name__ == '__main__':
    sys.exit(cli())
