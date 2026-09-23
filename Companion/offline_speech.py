"""Opt-in local dictation. No network calls or microphone-audio files."""
from pathlib import Path
import json
import queue
import sys
import threading
import time
import array
import math

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

def select_pcm_channel(data,channels,channel):
    """PortAudio native-endian int16 interleaving -> mono PCM bytes."""
    if not 1<=channel<=channels:raise ValueError('Input channel is outside the available range.')
    samples=array.array('h');samples.frombytes(data)
    if len(samples)%channels:raise ValueError('Incomplete microphone audio frame.')
    mono=samples[channel-1::channels]
    peak=max((abs(v) for v in mono),default=0)/32768.0
    rms=math.sqrt(sum(v*v for v in mono)/max(1,len(mono)))/32768.0
    return mono.tobytes(), max(-100.0,20*math.log10(max(rms,1e-5))), peak>=0.98

class Dictation:
    def __init__(self):
        self.events=queue.Queue();self.stop_event=threading.Event();self.thread=None;self.level=(-100.0,False)
    @property
    def running(self):return self.thread is not None and self.thread.is_alive()
    def start(self,device=None,channel=1):
        if self.running:raise RuntimeError('Dictation is already running.')
        self.level=(-100.0,False);self.stop_event.clear();self.thread=threading.Thread(target=self._work,args=(device,channel),daemon=True);self.thread.start()
    def stop(self):self.stop_event.set()
    def _work(self,device,channel):
        try:
            import sounddevice as sd
            from vosk import KaldiRecognizer
            self.events.put(('status','Loading offline English speech model…'))
            model=load_model()
            if self.stop_event.is_set():return
            info=sd.query_devices(device,'input')
            if channel not in (1,2) or channel>info['max_input_channels']:
                raise ValueError('Selected input channel is unavailable. Choose Input 1 or another microphone device.')
            rate=int(info['default_samplerate'])
            if not 16000<=rate<=96000:rate=48000
            sd.check_input_settings(device=device,channels=channel,dtype='int16',samplerate=rate)
            recognizer=KaldiRecognizer(model,rate)
            audio=queue.Queue(maxsize=16);overflow=threading.Event()
            def capture(data,frames,timing,status):
                if status:overflow.set()
                try:audio.put_nowait(bytes(data))
                except queue.Full:overflow.set()
            with sd.RawInputStream(samplerate=rate,blocksize=4000,device=device,dtype='int16',channels=channel,callback=capture):
                self.events.put(('status',f'MIC ON — {info["name"]} / Input {channel} / {rate} Hz'))
                started=time.monotonic()
                while not self.stop_event.is_set() and time.monotonic()-started<600:
                    if overflow.is_set():raise RuntimeError('Microphone audio could not keep up. Dictation stopped; close heavy audio jobs and try again.')
                    try:data=audio.get(timeout=.2)
                    except queue.Empty:continue
                    data,db,clipped=select_pcm_channel(data,channel,channel)
                    self.level=(db,clipped)
                    if recognizer.AcceptWaveform(data):
                        text=json.loads(recognizer.Result()).get('text','').strip()
                        if text:self.events.put(('text',text))
                    else:self.events.put(('partial',json.loads(recognizer.PartialResult()).get('partial','')))
            # Drain captured audio after closing the device, then finalize the last phrase.
            while not audio.empty():
                data,_,_=select_pcm_channel(audio.get_nowait(),channel,channel)
                if recognizer.AcceptWaveform(data):
                    text=json.loads(recognizer.Result()).get('text','').strip()
                    if text:self.events.put(('text',text))
            text=json.loads(recognizer.FinalResult()).get('text','').strip()
            if text:self.events.put(('text',text))
        except Exception as error:self.events.put(('error',str(error)+' Check the selected input/channel, Windows microphone permissions, and close Pro Tools if it holds the interface.'))
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

