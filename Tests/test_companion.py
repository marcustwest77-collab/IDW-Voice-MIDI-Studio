import importlib.util
import json
import struct
import sys
import tempfile
import types
import unittest
import urllib.error
import wave
from pathlib import Path
from unittest.mock import patch
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Companion'))
import audio_lab

class Response:
    def __init__(self, value): self.value = value
    def __enter__(self): return self
    def __exit__(self, *args): pass
    def read(self, n): return json.dumps(self.value).encode()
class Opener:
    def __init__(self): self.requests = []
    def open(self, request, timeout): self.requests.append(request);return Response({'id': 42, 'status': 'running'})

class AudioTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name);self.wav = self.root / 'test.wav'
        self.write_wav([1000, -1000] * 4000)
    def write_wav(self, samples):
        with wave.open(str(self.wav), 'wb') as w:
            w.setparams((1, 2, 8000, 0, 'NONE', 'not compressed'))
            w.writeframes(struct.pack('<' + 'h' * len(samples), *samples))
    def test_inspection(self):
        result = audio_lab.inspect_wav(self.wav)
        self.assertEqual(result['seconds'], 1);self.assertFalse(result['silent']);self.assertEqual(result['clipped_samples'], 0)
    def test_silence(self):
        self.write_wav([0] * 8000)
        self.assertTrue(audio_lab.inspect_wav(self.wav)['silent'])
        with self.assertRaises(ValueError): audio_lab.transcribe(self.wav, self.root)
    def test_clipping(self):
        self.write_wav([32767, -32768] * 20)
        self.assertEqual(audio_lab.inspect_wav(self.wav)['clipped_samples'], 40)
    def test_invalid_file(self):
        self.wav.write_bytes(b'broken')
        with self.assertRaises((wave.Error, EOFError)): audio_lab.inspect_wav(self.wav)
    def test_explicit_upload(self):
        opener = Opener();client = audio_lab.KitsClient('test-key', opener)
        with self.assertRaises(PermissionError): client.convert(self.wav, 3)
        self.assertEqual(opener.requests, [])
        self.assertEqual(client.convert(self.wav, 3, True)['id'], 42)
        req = opener.requests[0]
        self.assertEqual(req.full_url, audio_lab.API + 'voice-conversions')
        self.assertIn(b'name="voiceModelId"', req.data);self.assertIn(b'RIFF', req.data)
        self.assertNotIn(b'test-key', req.data)
    def test_job_path(self):
        opener = Opener();client = audio_lab.KitsClient('test-key', opener)
        with self.assertRaises(ValueError): client.job('../voice-models')
        self.assertFalse(opener.requests)
        client.job(42);self.assertTrue(opener.requests[0].full_url.endswith('/42'))
    def test_no_redirect(self):
        with self.assertRaises(RuntimeError): audio_lab.NoRedirect().redirect_request(None,None,302,'',{},'https://other.example')
    def test_http_error(self):
        opener = Opener()
        with patch.object(opener, 'open', side_effect=urllib.error.HTTPError('url',401,'secret-response',{},None)):
            with self.assertRaisesRegex(RuntimeError, 'HTTP 401') as caught:
                audio_lab.KitsClient('test-key', opener).models()
            self.assertNotIn('secret-response', str(caught.exception));self.assertNotIn('test-key', str(caught.exception))
    def test_transcription_export_contract(self):
        # This tests the adapter and output files; it is NOT a model accuracy test.
        class Midi:
            def write(self, path): Path(path).write_bytes(b'MThd-test-fixture')
        package = types.ModuleType('basic_pitch');module = types.ModuleType('basic_pitch.inference')
        module.predict = lambda *a, **kw: ({}, Midi(), [(0., .5, 60, .8, None), (0., .5, 64, .7, None)])
        with patch.dict(sys.modules, {'basic_pitch': package, 'basic_pitch.inference': module}):
            output = audio_lab.transcribe(self.wav, self.root)
            second = audio_lab.transcribe(self.wav, self.root)
        self.assertNotEqual(output, second)
        self.assertEqual(json.loads((output/'report.json').read_text())['notes'], 2)
        self.assertIn('60', (output/'notes.csv').read_text())

class RouterTests(unittest.TestCase):
    def setUp(self):
        self.names = ['IDW Lead', 'IDW Chords', 'IDW Bass', 'IDW Drums', 'Other']
        self.sent = []
        channels = types.SimpleNamespace(channelCount=lambda x: len(self.names), getChannelName=lambda i, glob: self.names[i], midiNoteOn=lambda *a: self.sent.append(a))
        spec = importlib.util.spec_from_file_location('router', ROOT / 'Integrations/FL-Studio/IDW-Performance/device_IDW_Performance.py')
        self.router = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, {'channels': channels}): spec.loader.exec_module(self.router)
        self.router.OnInit()
    def event(self, ch, kind, note=60, vel=100):
        e = types.SimpleNamespace(midiChan=ch, status=kind|ch, data1=note, data2=vel, handled=False)
        self.router.OnMidiMsg(e);return e
    def test_layers(self):
        for ch, target in [(0,0),(1,1),(2,2),(9,3)]:
            self.assertTrue(self.event(ch,0x90).handled);self.event(ch,0x80)
            self.assertEqual(self.sent[-2:], [(target,60,100),(target,60,0)])
    def test_refresh_preserves_notes_when_mapping_unchanged(self):
        self.event(0,0x90);self.router.OnRefresh(0)
        self.assertEqual(self.sent, [(0,60,100)])
    def test_duplicate_suppresses_route(self):
        self.names.append('IDW Chords');self.router.OnRefresh(0);self.event(1,0x90)
        self.assertEqual(self.sent, [])
    def test_panic_and_other_channel(self):
        self.event(2,0x90);self.event(2,0xb0,123,0)
        self.assertEqual(self.sent[-1], (2,60,0));self.assertFalse(self.event(5,0x90).handled)

if __name__ == '__main__': unittest.main()
