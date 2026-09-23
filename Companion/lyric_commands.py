"""Optional exact-phrase dictation commands; never executes system actions."""
import re

PUNCTUATION={'comma':',','period':'.','full stop':'.','question mark':'?','exclamation mark':'!','colon':':','semicolon':';'}
SECTIONS={'section '+name.lower():'['+name+']' for name in ('Intro','Verse','Hook','Bridge','Outro')}
HELP='Pause between commands: comma, period, question mark, exclamation mark, colon, semicolon, new line, new paragraph, section verse (or intro/hook/bridge/outro). Commands must be a whole recognized phrase. Turn Voice commands off to dictate these words literally.'

def append_edit(current,phrase,commands=False,new_line=True):
    """Return (ASCII tail characters to remove, text to append).

    Restrict commands to complete recognized phrases; ordinary lyrics are literal.
    The caller groups the delete/insert into one undoable edit.
    """
    phrase=phrase.strip()
    if not phrase:return 0,''
    key=phrase.casefold()
    if commands:
        if key in PUNCTUATION:
            stripped=current.rstrip(' \t\r\n')
            if not stripped:return 0,''
            # Do not punctuate a section header.
            if re.fullmatch(r'\[[^\]\n]+\]',stripped.split('\n')[-1].strip()):return 0,''
            return len(current)-len(stripped),PUNCTUATION[key]+ ('\n' if new_line else ' ')
        if key in ('new line','new paragraph'):
            stripped=current.rstrip(' \t\r\n')
            return len(current)-len(stripped),'\n'*(1 if key=='new line' else 2)
        if key in SECTIONS:
            return 0,('' if not current or current.endswith('\n\n') else '\n' if current.endswith('\n') else '\n\n')+SECTIONS[key]+'\n'
    prefix=''
    if current and not current.endswith((' ','\t','\r','\n')):prefix='\n' if new_line else ' '
    return 0,prefix+phrase+('\n' if new_line else '')

def lyric_counts(text):
    """Count Unicode words and nonempty lyric lines; exclude bracketed headings."""
    lines=[];sections=0
    for line in text.splitlines():
        line=line.strip()
        if re.fullmatch(r'\[[^\]\n]+\]',line):sections+=1
        elif line:lines.append(line)
    words=re.findall(r"[^\W_]+(?:['’][^\W_]+)*",'\n'.join(lines),re.UNICODE)
    return len(words),len(lines),sections
