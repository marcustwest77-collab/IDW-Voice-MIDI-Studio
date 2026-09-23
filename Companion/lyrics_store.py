"""Local lyric drafts, atomic saves and a previous-save recovery copy."""
from pathlib import Path
import datetime
import hashlib
import json
import os
import re
import tempfile
import uuid

class LyricsStore:
    def __init__(self, directory=None):
        self.directory=Path(directory) if directory else Path.home()/'IDW Audio Lab'/'Lyrics'
    def path(self, identifier):
        if not re.fullmatch(r'[a-f0-9]{32}',identifier): raise ValueError('Invalid song identifier.')
        return self.directory/(identifier+'.json')
    @staticmethod
    def new(): return {'id':uuid.uuid4().hex,'title':'Untitled song','text':'','modified':''}
    @staticmethod
    def validate(data):
        if not isinstance(data,dict) or not isinstance(data.get('title'),str) or not isinstance(data.get('text'),str): raise ValueError('Invalid lyric draft.')
        if len(data['title'])>200 or len(data['text'])>1000000: raise ValueError('Use a title up to 200 characters and lyrics up to 1 million characters.')
    @staticmethod
    def revision(raw):return hashlib.sha256(raw).hexdigest()
    def load(self,identifier):
        path=self.path(identifier)
        if path.stat().st_size>4*1024*1024:raise ValueError('Lyric draft exceeds the size limit.')
        raw=path.read_bytes();data=json.loads(raw);self.validate(data)
        if data.get('id')!=identifier:raise ValueError('Song identity mismatch.')
        return data,self.revision(raw)
    @staticmethod
    def atomic_write(path,raw):
        fd,name=tempfile.mkstemp(prefix='.idw-',suffix='.tmp',dir=path.parent)
        try:
            with os.fdopen(fd,'wb') as handle:handle.write(raw);handle.flush();os.fsync(handle.fileno())
            os.replace(name,path)
        finally:
            Path(name).unlink(missing_ok=True)
    def save(self,data,expected_revision=None):
        self.validate(data);path=self.path(data['id']);self.directory.mkdir(parents=True,exist_ok=True)
        # Exclusive per-draft lock prevents two Audio Lab instances from losing each other's edits.
        lock=path.with_suffix('.lock')
        try:fd=os.open(lock,os.O_CREAT|os.O_EXCL|os.O_WRONLY)
        except FileExistsError:raise RuntimeError('This draft is locked by another save. Export TXT to preserve your text; retry when the other app finishes.') from None
        os.close(fd)
        try:
            old=path.read_bytes() if path.exists() else None
            if (self.revision(old) if old is not None else None)!=expected_revision:
                raise RuntimeError('This song changed in another window. Export TXT before reopening it; your current text has been kept.')
            saved=dict(data);saved['title']=saved['title'].strip() or 'Untitled song';saved['modified']=datetime.datetime.now(datetime.timezone.utc).isoformat()
            raw=json.dumps(saved,ensure_ascii=False,indent=2).encode('utf-8')
            if old is not None:
                self.atomic_write(path.with_suffix('.previous.json'),old)
                history=self.directory/'history'/data['id'];history.mkdir(parents=True,exist_ok=True)
                # Retain distinct pre-save versions; current draft is never pruned.
                snapshot=history/(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%f')+'-'+uuid.uuid4().hex+'.json')
                self.atomic_write(snapshot,old)
            self.atomic_write(path,raw)
            history=self.directory/'history'/data['id']
            if history.exists():
                for extra in sorted(history.glob('*.json'),reverse=True)[20:]:
                    try:extra.unlink()
                    except OSError:pass  # Do not report a successful draft save as failed.
            return saved,self.revision(raw)
        finally:lock.unlink(missing_ok=True)
    def list(self):
        entries=[]
        if not self.directory.exists():return entries
        for path in self.directory.glob('*.json'):
            if re.fullmatch(r'[a-f0-9]{32}',path.stem):
                try:data,_=self.load(path.stem);entries.append(data)
                except (OSError,ValueError):continue
        return sorted(entries,key=lambda d:d.get('modified',''),reverse=True)
    def previous(self,identifier):
        path=self.path(identifier).with_suffix('.previous.json')
        if not path.is_file():raise ValueError('There is no previous saved version yet.')
        if path.stat().st_size>4*1024*1024:raise ValueError('Previous draft exceeds the size limit.')
        data=json.loads(path.read_bytes());self.validate(data)
        if data.get('id')!=identifier:raise ValueError('Song identity mismatch.')
        return data
    def history(self,identifier):
        self.path(identifier)  # Validate before building any history path.
        entries=[]
        for path in sorted((self.directory/'history'/identifier).glob('*.json'),reverse=True):
            try:
                if path.stat().st_size>4*1024*1024:continue
                data=json.loads(path.read_bytes());self.validate(data)
                if data.get('id')==identifier:entries.append(data)
            except (OSError,ValueError):continue
        if not entries:
            try:entries.append(self.previous(identifier))  # V6.4 migration.
            except (OSError,ValueError):pass
        return entries[:20]
    @staticmethod
    def export_text(path,title,text):
        path=Path(path)
        if path.suffix.lower()!='.txt':path=path.with_suffix('.txt')
        created=False
        try:
            with path.open('x',encoding='utf-8') as handle:
                created=True;handle.write(title.strip()+'\n\n'+text)
        except Exception:
            if created:path.unlink(missing_ok=True)
            raise
        return path

