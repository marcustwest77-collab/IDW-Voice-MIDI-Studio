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


def transcribe(path, destination, onset=.5, frame=.3, minimum_ms=127.7, tempo=120):
    info = inspect_wav(path)
    if info['silent']:
        raise ValueError('The recording is digital silence. Check the input before transcribing.')
    if not (0 < onset <= 1 and 0 < frame <= 1 and 20 <= minimum_ms <= 2000 and 40 <= tempo <= 240):
        raise ValueError('Invalid transcription settings.')
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
    (job / 'report.json').write_text(json.dumps({'engine': 'Spotify Basic Pitch', 'mode': 'offline file transcription', 'notes': len(notes), 'input': info}, indent=2), encoding='utf-8')
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


def main():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    import queue
    import threading
    import webbrowser
    root = tk.Tk()
    root.title('IDW Audio Lab — V6 candidate')
    root.geometry('920x700')
    root.minsize(800, 650)
    panel = ttk.Frame(root, padding=18)
    panel.pack(fill='both', expand=True)
    ttk.Label(panel, text='IDW / AUDIO LAB', font=('Segoe UI', 20, 'bold')).pack(anchor='w')
    ttk.Label(panel, text='Offline transcription • recording inspection • optional cloud voice conversion').pack(anchor='w', pady=(0, 12))
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
    ttk.Label(local, text='Transcribe chords or a single instrument into MIDI. Full mixes may need isolated stems first.\nThe model runs locally after installation; this is file processing, not live polyphonic tracking.').pack(anchor='w')
    result_queue = queue.Queue(); busy = False; actions = []
    output = tk.Text(panel, height=12, wrap='word'); output.pack(fill='both', expand=True)
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
            write(('ERROR: ' if status == 'error' else '') + str(value))
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
        destination = filedialog.askdirectory(title='Folder for the new MIDI take')
        if destination: run(lambda: 'Saved MIDI, note table and report in: ' + str(transcribe(path, destination)))
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
        if busy and not messagebox.askyesno('Work is running', 'Close Audio Lab? Local processing will stop. An accepted cloud job may continue in your Kits account.'):
            return
        key.set(''); root.destroy()
    root.protocol('WM_DELETE_WINDOW', close)
    write('Local tools do not upload audio. Model inference and cloud account tests are pending for this candidate.')
    poll(); root.mainloop()

if __name__ == '__main__':
    main()
