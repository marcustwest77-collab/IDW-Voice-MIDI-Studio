"""Bounded plain-text lyric import; never modifies the source file."""
from pathlib import Path

def read_lyrics(path):
    path=Path(path)
    if path.suffix.lower()!='.txt':raise ValueError('Choose a plain-text .txt file.')
    with path.open('rb') as handle:raw=handle.read(4*1024*1024+1)
    if len(raw)>4*1024*1024:raise ValueError('Text file exceeds 4 MiB.')
    try:text=raw.decode('utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig')
    except UnicodeError:raise ValueError('Save this file as UTF-8 or UTF-16 with a byte-order mark, then retry.') from None
    text=text.replace('\r\n','\n').replace('\r','\n')
    if '\0' in text:raise ValueError('This does not appear to be a plain-text lyrics file.')
    if len(text)>1000000:raise ValueError('Lyrics exceed 1 million characters.')
    return path.stem[:200] or 'Imported lyrics',text
