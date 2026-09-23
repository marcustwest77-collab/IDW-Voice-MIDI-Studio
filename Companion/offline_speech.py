"""Opt-in local dictation. No network calls or microphone-audio files."""
from pathlib import Path
import json
import queue
import sys
import threading
import time

MODEL_NAME='vosk-model-small-en-us-0.15'

def model_path():
    base=Path(sys._MEIPASS) if getattr(sys,'frozen',False) else Path(__file__).resolve().parents[1]
    return base/'speech-model'/MODEL_NAME

def load_model():
    from vosk import Model,SetLogLevel
    path=model_path()
    if not (path/'am/final.mdl').is_file():raise RuntimeError('Speech model missing. Extract the entire Windows package; source users run scripts/fetch-speech-model.py.')
    SetLogLevel(-1)
    return Model(str(path))

def input_devices():
    import sounddevice as sd
    return [(index,d['name']) for index,d in enumerate(sd.query_devices()) if d['max_input_channels']>0]

class Dictation:
    def __init__(self):
        self.events=queue.Queue();self.stop_event=threading.Event();self.thread=None
    @property
    def running(self):return self.thread is not None and self.thread.is_alive()
    def start(self,device=None):
        if self.running:raise RuntimeError('Dictation is already running.')
        self.stop_event.clear();self.thread=threading.Thread(target=self._work,args=(device,),daemon=True);self.thread.start()
    def stop(self):self.stop_event.set()
    def _work(self,device):
        try:
            import sounddevice as sd
            from vosk import KaldiRecognizer
            self.events.put(('status','Loading offline English speech model…'))
            model=load_model()
            if self.stop_event.is_set():return
            rate=int(sd.query_devices(device,'input')['default_samplerate'])
            if not 16000<=rate<=96000:rate=48000
            recognizer=KaldiRecognizer(model,rate)
            audio=queue.Queue(maxsize=16);overflow=threading.Event()
            def capture(data,frames,timing,status):
                if status:overflow.set()
                try:audio.put_nowait(bytes(data))
                except queue.Full:overflow.set()
            with sd.RawInputStream(samplerate=rate,blocksize=4000,device=device,dtype='int16',channels=1,callback=capture):
                self.events.put(('status','MIC ON — speak clearly; each finished phrase appends a lyric line.'))
                started=time.monotonic()
                while not self.stop_event.is_set() and time.monotonic()-started<600:
                    if overflow.is_set():raise RuntimeError('Microphone audio could not keep up. Dictation stopped; close heavy audio jobs and try again.')
                    try:data=audio.get(timeout=.2)
                    except queue.Empty:continue
                    if recognizer.AcceptWaveform(data):
                        text=json.loads(recognizer.Result()).get('text','').strip()
                        if text:self.events.put(('text',text))
                    else:self.events.put(('partial',json.loads(recognizer.PartialResult()).get('partial','')))
            # Drain captured audio after closing the device, then finalize the last phrase.
            while not audio.empty():
                if recognizer.AcceptWaveform(audio.get_nowait()):
                    text=json.loads(recognizer.Result()).get('text','').strip()
                    if text:self.events.put(('text',text))
            text=json.loads(recognizer.FinalResult()).get('text','').strip()
            if text:self.events.put(('text',text))
        except Exception as error:self.events.put(('error',str(error)))
        finally:self.events.put(('done','MIC OFF — dictation stopped. No microphone recording was saved.'))

def speech_smoke(path):
    import wave
    import sounddevice as sd
    from vosk import KaldiRecognizer
    # Loads PortAudio as well as the speech DLL/model; never opens a microphone.
    assert sd.get_portaudio_version()[0]>0
    model=load_model();words=[]
    with wave.open(str(path),'rb') as wav:
        assert wav.getnchannels()==1 and wav.getsampwidth()==2
        recognizer=KaldiRecognizer(model,wav.getframerate())
        while True:
            data=wav.readframes(4000)
            if not data:break
            if recognizer.AcceptWaveform(data):words.extend(json.loads(recognizer.Result()).get('text','').split())
        words.extend(json.loads(recognizer.FinalResult()).get('text','').split())
    assert len(words)>=4 and 'zero' in words, 'Speech fixture was not recognized: '+str(words)
    return 'PASS bundled Vosk speech recognition on official spoken-number fixture / PortAudio load; physical microphone not tested'
