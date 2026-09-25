import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Companion'))
from lyric_commands import append_edit,lyric_counts

def apply(text,phrase,commands=True,new_line=True):
    remove,added=append_edit(text,phrase,commands,new_line)
    return (text[:-remove] if remove else text)+added

class CommandsTests(unittest.TestCase):
    def test_disabled_is_literal(self):
        self.assertEqual(apply('','comma',False),'comma\n')
        self.assertEqual(apply('','section hook',False),'section hook\n')
    def test_exact_phrases_only(self):
        self.assertEqual(apply('','i need a new line'),'i need a new line\n')
        self.assertEqual(apply('','hello comma world'),'hello comma world\n')
    def test_punctuation_attaches_to_last_line(self):
        self.assertEqual(apply('my song 🎵\n','question mark'),'my song 🎵?\n')
        self.assertEqual(apply('hello\n',' COMMA '),'hello,\n')
        self.assertEqual(apply('hello','period',True,False),'hello. ')
    def test_empty_and_header_are_not_punctuated(self):
        self.assertEqual(apply('','period'),'')
        self.assertEqual(apply('[Hook]\n','comma'),'[Hook]\n')
        self.assertEqual(apply('abc','   '),'abc')
    def test_paragraph_and_section_boundaries(self):
        self.assertEqual(apply('one\n','new paragraph'),'one\n\n')
        self.assertEqual(apply('one  ','new line'),'one\n')
        self.assertEqual(apply('one\n','section hook'),'one\n\n[Hook]\n')
        self.assertEqual(apply('','section verse'),'[Verse]\n')
    def test_phrase_layout_and_typed_tail(self):
        self.assertEqual(apply('typed','spoken',False),'typed\nspoken\n')
        self.assertEqual(apply('one','two',False,False),'one two')
        self.assertEqual(apply('one. ','two',False,False),'one. two')
    def test_counts_ignore_blank_lines_and_headers(self):
        self.assertEqual(lyric_counts('[Verse]\nDon’t stop 🎵\n\nCafé nights\n[Hook]\n'),(4,2,2))
        self.assertEqual(lyric_counts(''),(0,0,0))
        self.assertEqual(lyric_counts('!!!\n'),(0,1,0))
