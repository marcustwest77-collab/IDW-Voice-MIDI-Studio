import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Companion'))
from lyrics_store import LyricsStore

class LyricsTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.store=LyricsStore(self.temp.name)
    def test_save_reopen_unicode(self):
        song=self.store.new();song.update(title='Memphis nights',text='[Hook]\nHere’s my line 🎵\n')
        saved,revision=self.store.save(song)
        self.assertEqual(self.store.load(song['id']),(saved,revision));self.assertEqual(len(self.store.list()),1)
    def test_previous_save(self):
        song=self.store.new();song['text']='first';saved,rev=self.store.save(song)
        saved['text']='second';self.store.save(saved,rev)
        self.assertEqual(self.store.previous(song['id'])['text'],'first')
    def test_conflict_keeps_both_current_and_file(self):
        song=self.store.new();saved,rev=self.store.save(song);saved['text']='new';self.store.save(saved,rev)
        stale=dict(saved,text='stale')
        with self.assertRaises(RuntimeError):self.store.save(stale,rev)
        self.assertEqual(self.store.load(song['id'])[0]['text'],'new');self.assertEqual(stale['text'],'stale')
    def test_failed_atomic_write_keeps_draft(self):
        song=self.store.new();saved,rev=self.store.save(song);original=self.store.path(song['id']).read_bytes()
        saved['text']='replacement'
        with patch('lyrics_store.os.replace',side_effect=OSError('disk failure')),self.assertRaises(OSError):self.store.save(saved,rev)
        self.assertEqual(self.store.path(song['id']).read_bytes(),original)
    def test_bad_identifier(self):
        with self.assertRaises(ValueError):self.store.path('../elsewhere')
    def test_export_never_overwrites(self):
        path=Path(self.temp.name)/'song.txt';self.store.export_text(path,'Title','Lyrics')
        with self.assertRaises(FileExistsError):self.store.export_text(path,'Other','Replacement')
        self.assertEqual(path.read_text(),'Title\n\nLyrics')
    def test_lock_and_invalid_input(self):
        song=self.store.new();self.store.path(song['id']).with_suffix('.lock').touch()
        with self.assertRaises(RuntimeError):self.store.save(song)
        with self.assertRaises(ValueError):self.store.save(dict(song,text=123))

    def test_history_retains_twenty_and_current(self):
        song=self.store.new();song['text']='0';song,rev=self.store.save(song)
        for i in range(1,25):
            song['text']=str(i);song,rev=self.store.save(song,rev)
        versions=self.store.history(song['id'])
        self.assertEqual([d['text'] for d in versions],[str(i) for i in range(23,3,-1)])
        self.assertEqual(self.store.load(song['id'])[0]['text'],'24')
        recovered=self.store.new();recovered.update(title='Recovered',text=versions[-1]['text'])
        self.store.save(recovered)
        self.assertEqual(self.store.load(song['id'])[0]['text'],'24')
        self.assertEqual(self.store.load(recovered['id'])[0]['text'],'4')
    def test_history_failure_preserves_current(self):
        song,rev=self.store.save(self.store.new());original=self.store.path(song['id']).read_bytes()
        song['text']='new'
        with patch('lyrics_store.Path.mkdir',side_effect=OSError('disk full')),self.assertRaises(OSError):self.store.save(song,rev)
        self.assertEqual(self.store.path(song['id']).read_bytes(),original)
    def test_v64_previous_fallback_and_corrupt_history(self):
        song,rev=self.store.save(self.store.new());song['text']='second';self.store.save(song,rev)
        history=self.store.directory/'history'/song['id']
        for p in history.glob('*.json'):p.write_text('bad json')
        self.assertEqual(self.store.history(song['id'])[0]['text'],'')
        with self.assertRaises(ValueError):self.store.history('../bad')

if __name__=='__main__':unittest.main()

