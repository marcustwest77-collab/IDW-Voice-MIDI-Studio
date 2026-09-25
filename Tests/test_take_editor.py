import sys
import unittest
from dataclasses import replace
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Companion'))
from take_editor import Note,Take

class TakeTests(unittest.TestCase):
    def setUp(self):
        self.notes=[Note(0,0,0,60,90,55,255,0,1),Note(1,0,0,64,80,520,700,2,3),Note(2,1,9,36,110,60,180,0,1)]
        self.take=Take(self.notes)
    def test_transpose_skips_drums_and_scopes_tracks(self):
        self.take.transpose(12,0)
        self.assertEqual([n.pitch for n in self.take.notes],[72,76,36])
        self.assertEqual(self.take.original,tuple(self.notes))
    def test_out_of_range_is_atomic(self):
        self.take.change([replace(self.notes[0],pitch=127)]+self.notes[1:])
        old=self.take.notes
        with self.assertRaises(ValueError):self.take.transpose(1)
        self.assertEqual(self.take.notes,old)
    def test_snap_preserves_lengths(self):
        self.take.quantize(.25,0)
        self.assertEqual([n.start for n in self.take.notes],[0,480,60])
        self.assertEqual([n.end-n.start for n in self.take.notes],[200,180,120])
    def test_edit_and_undo_redo(self):
        self.take.edit(0,'62','75','0.5','0.25')
        self.assertEqual((self.take.notes[0].pitch,self.take.notes[0].start,self.take.notes[0].end),(62,240,360))
        self.take.undo();self.assertEqual(self.take.notes,tuple(self.notes));self.assertFalse(self.take.dirty)
        self.take.redo();self.assertTrue(self.take.dirty)
        self.take.reset();self.assertEqual(self.take.notes,tuple(self.notes))
    def test_invalid_and_overlapping_edits_preserve_take(self):
        for start,length,pitch in [('nan',1,60),(0,0,60),(-1,1,60),(0,2,64)]:
            with self.subTest(start=start,length=length,pitch=pitch),self.assertRaises(ValueError):self.take.edit(0,pitch,90,start,length)
            self.assertEqual(self.take.notes,tuple(self.notes))
    def test_delete_and_history_limit(self):
        self.take.delete(0);self.assertEqual(len(self.take.notes),2);self.take.undo();self.assertEqual(len(self.take.notes),3)
        for i in range(25):self.take.edit(0,60,50+i,0,1)
        self.assertEqual(len(self.take.history),20)
    def test_invalid_grid(self):
        for grid in ['nan',0,5]:
            with self.assertRaises(ValueError):self.take.quantize(grid)

if __name__=='__main__':unittest.main()
