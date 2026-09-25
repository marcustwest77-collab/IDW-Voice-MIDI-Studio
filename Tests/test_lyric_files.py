import sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Companion'))
from lyric_files import read_lyrics
from lyrics_store import LyricsStore

class LyricFilesTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name);self.store=LyricsStore(self.root/'songs')
    def test_import_encodings_and_source_unchanged(self):
        for encoding in ('utf-8-sig','utf-16'):
            p=self.root/'My song.txt';raw='[Hook]\r\nCafé 🎵\r\n'.encode(encoding);p.write_bytes(raw)
            self.assertEqual(read_lyrics(p),('My song','[Hook]\nCafé 🎵\n'))
            self.assertEqual(p.read_bytes(),raw)
    def test_import_rejects_binary_encoding_and_size(self):
        p=self.root/'bad.txt'
        for raw in (b'a\0b',b'\x80',b'x'*1000001,b'x'*(4*1024*1024+1)):
            p.write_bytes(raw)
            with self.assertRaises(ValueError):read_lyrics(p)
        with self.assertRaises(ValueError):read_lyrics(self.root/'song.docx')
    def test_checkpoint_survives_rolling_history(self):
        song=self.store.new();song['text']='original';song,rev=self.store.save(song)
        mark=self.store.checkpoint(song,'Original hook')
        for i in range(25):song['text']=str(i);song,rev=self.store.save(song,rev)
        entries=self.store.checkpoints(song['id'])
        self.assertEqual(len(entries),1);self.assertEqual(entries[0],mark)
        self.assertEqual(entries[0]['song']['text'],'original');self.assertEqual(self.store.load(song['id'])[0]['text'],'24')
    def test_checkpoint_labels_do_not_become_paths(self):
        song,rev=self.store.save(self.store.new())
        a=self.store.checkpoint(song,'../CON');b=self.store.checkpoint(song,'../CON')
        self.assertNotEqual(a['id'],b['id']);self.assertEqual(len(self.store.checkpoints(song['id'])),2)
        for label in ('',' '*4,'x'*81):
            with self.assertRaises(ValueError):self.store.checkpoint(song,label)
        with self.assertRaises(ValueError):self.store.checkpoints('../bad')
    def test_checkpoint_failure_keeps_song(self):
        song,rev=self.store.save(self.store.new());before=self.store.path(song['id']).read_bytes()
        with patch('lyrics_store.os.replace',side_effect=OSError('disk full')),self.assertRaises(OSError):self.store.checkpoint(song,'name')
        self.assertEqual(self.store.path(song['id']).read_bytes(),before);self.assertEqual(self.store.checkpoints(song['id']),[])
    def test_checkpoint_limit_no_pruning_and_corrupt_skip(self):
        song,rev=self.store.save(self.store.new())
        for i in range(100):self.store.checkpoint(song,str(i))
        with self.assertRaises(ValueError):self.store.checkpoint(song,'overflow')
        self.assertEqual(len(self.store.checkpoints(song['id'])),100)
        directory=self.store.directory/'checkpoints'/song['id'];next(directory.glob('*.json')).write_text('invalid')
        self.assertEqual(len(self.store.checkpoints(song['id'])),99)
