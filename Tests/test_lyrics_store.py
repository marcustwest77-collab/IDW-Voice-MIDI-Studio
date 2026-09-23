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

if __name__=='__main__':unittest.main()
