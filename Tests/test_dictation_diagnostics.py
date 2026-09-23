import array
import math
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Companion'))
from offline_speech import select_pcm_channel

class MicrophoneTests(unittest.TestCase):
    def test_input_two_ignores_input_one(self):
        raw=array.array('h',[0,16384,0,-16384]).tobytes()
        mono,db,clipped=select_pcm_channel(raw,2,2)
        self.assertEqual(list(array.array('h',mono)),[16384,-16384])
        self.assertAlmostEqual(db,20*math.log10(.5));self.assertFalse(clipped)
        _,left,_=select_pcm_channel(raw,2,1);self.assertEqual(left,-100)
    def test_silence_and_clipping(self):
        self.assertEqual(select_pcm_channel(bytes(8),1,1)[1:],(-100,False))
        self.assertTrue(select_pcm_channel(array.array('h',[-32768,32767]).tobytes(),1,1)[2])
    def test_invalid_frame_and_channel(self):
        for channels,channel in [(1,2),(2,0),(0,1)]:
            with self.assertRaises(ValueError):select_pcm_channel(bytes(8),channels,channel)
        with self.assertRaises(ValueError):select_pcm_channel(bytes(6),2,1)
