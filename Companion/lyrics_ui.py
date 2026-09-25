"""Local songwriting workspace with opt-in offline microphone dictation."""
import os
import queue
import tkinter as tk
from tkinter import ttk,filedialog,messagebox,simpledialog
from lyrics_store import LyricsStore
from lyric_files import read_lyrics
from lyric_commands import append_edit,lyric_counts,HELP
from offline_speech import Dictation,input_devices

class LyricsWorkspace(ttk.Frame):
    def __init__(self,parent,write,is_busy=lambda:False,directory=None):
        super().__init__(parent,padding=10)
        self.write=write;self.is_busy=is_busy;self.store=LyricsStore(directory);self.doc=self.store.new();self.revision=None
        self.loading=False;self.dirty=False;self.listening=False;self.pending_close=None;self.dictation=Dictation();self.song_ids=[];self.device_ids=[None]
        bar=ttk.Frame(self);bar.pack(fill='x')
        self.saved=ttk.Combobox(bar,state='readonly',width=23);self.saved.pack(side='left',padx=2)
        self.new_button=ttk.Button(bar,text='New song',command=self.new);self.new_button.pack(side='left',padx=2)
        self.open_button=ttk.Button(bar,text='Open song',command=self.open);self.open_button.pack(side='left',padx=2)
        ttk.Button(bar,text='Save now',command=lambda:self.flush(True)).pack(side='left',padx=2)
        ttk.Button(bar,text='Export TXT',command=self.export).pack(side='left',padx=2)
        self.restore_button=ttk.Button(bar,text='History',command=self.restore);self.restore_button.pack(side='left',padx=2)
        search=ttk.Frame(self);search.pack(fill='x',pady=(4,0))
        ttk.Label(search,text='Find song').pack(side='left',padx=4)
        self.query=tk.StringVar();ttk.Entry(search,textvariable=self.query,width=35).pack(side='left')
        self.query.trace_add('write',lambda *args:self.refresh_songs())
        row=ttk.Frame(self);row.pack(fill='x',pady=4)
        self.import_button=ttk.Button(row,text='Import TXT',command=self.import_text);self.import_button.pack(side='left',padx=2)
        self.checkpoint_button=ttk.Button(row,text='Save checkpoint',command=self.checkpoint);self.checkpoint_button.pack(side='left',padx=2)
        self.checkpoints_button=ttk.Button(row,text='Checkpoints',command=lambda:self.restore(True));self.checkpoints_button.pack(side='left',padx=2)
        ttk.Label(row,text='Named versions stay beyond rolling history.').pack(side='left',padx=8)
        row=ttk.Frame(self);row.pack(fill='x',pady=6);ttk.Label(row,text='Song title').pack(side='left',padx=4)
        self.title=tk.StringVar(value=self.doc['title']);ttk.Entry(row,textvariable=self.title).pack(side='left',fill='x',expand=True)
        ttk.Button(row,text='Lyrics folder',command=self.folder).pack(side='left',padx=5)
        self.title.trace_add('write',lambda *args:self.changed())
        row=ttk.Frame(self);row.pack(fill='x')
        for heading in ('Intro','Verse','Hook','Bridge','Outro'):
            ttk.Button(row,text=heading,command=lambda name=heading:self.section(name)).pack(side='left',padx=2)
        ttk.Button(row,text='Undo',command=self.undo).pack(side='left',padx=5)
        ttk.Button(row,text='Redo',command=self.redo).pack(side='left',padx=2)
        row=ttk.Frame(self);row.pack(fill='x',pady=6)
        self.devices=ttk.Combobox(row,state='readonly',width=28,values=['Default microphone']);self.devices.current(0);self.devices.pack(side='left',padx=2)
        self.refresh_button=ttk.Button(row,text='Refresh mics',command=self.refresh_devices);self.refresh_button.pack(side='left',padx=2)
        self.start_button=ttk.Button(row,text='Start dictation',command=self.start);self.start_button.pack(side='left',padx=2)
        self.stop_button=ttk.Button(row,text='Stop',command=self.stop,state='disabled');self.stop_button.pack(side='left',padx=2)
        row=ttk.Frame(self);row.pack(fill='x',pady=(0,4))
        ttk.Label(row,text='Mic channel').pack(side='left')
        self.channel=ttk.Combobox(row,state='readonly',width=9,values=['Input 1','Input 2']);self.channel.current(0);self.channel.pack(side='left',padx=5)
        self.meter=ttk.Progressbar(row,length=120,maximum=60);self.meter.pack(side='left',padx=5)
        self.level_text=ttk.Label(row,text='Mic idle',width=42);self.level_text.pack(side='left')
        row=ttk.Frame(self);row.pack(fill='x',pady=(0,4))
        self.commands=tk.BooleanVar(value=False);self.phrase_lines=tk.BooleanVar(value=True)
        self.command_toggle=ttk.Checkbutton(row,text='Voice commands',variable=self.commands);self.command_toggle.pack(side='left')
        self.line_toggle=ttk.Checkbutton(row,text='New line per phrase',variable=self.phrase_lines);self.line_toggle.pack(side='left',padx=8)
        ttk.Button(row,text='Command help',command=lambda:messagebox.showinfo('Spoken commands',HELP)).pack(side='left')
        self.counts=ttk.Label(row,text='0 words / 0 lines / 0 sections');self.counts.pack(side='left',padx=8)
        self.count_job=None
        self.mic=ttk.Label(self,text='MIC OFF — English dictation runs locally. No recording or upload.',wraplength=700);self.mic.pack(anchor='w')
        self.partial=ttk.Label(self,text='Speak clearly; edit names, slang and punctuation afterward.',wraplength=700);self.partial.pack(anchor='w',pady=(0,4))
        area=ttk.Frame(self);area.pack(fill='both',expand=True)
        self.text=tk.Text(area,height=10,wrap='word',undo=True,maxundo=100,font=('Segoe UI',12),background='#101722',foreground='#e9eef5',insertbackground='#eac36c',padx=10,pady=8)
        self.text.pack(side='left',fill='both',expand=True);scroll=ttk.Scrollbar(area,command=self.text.yview);scroll.pack(side='right',fill='y');self.text.configure(yscrollcommand=scroll.set)
        self.text.bind('<<Modified>>',self.modified)
        self.status=ttk.Label(self,text='Drafts auto-save locally every 2 seconds after edits. Dictation sessions stop after 10 minutes.',wraplength=700);self.status.pack(side='bottom',anchor='w',pady=5,before=area)
        self.refresh_songs();self.after(100,self.poll);self.after(2000,self.autosave)

    def changed(self):
        if not self.loading:self.dirty=True
    def modified(self,event=None):
        if self.text.edit_modified():
            self.changed();self.text.edit_modified(False)
            if self.count_job is not None:self.after_cancel(self.count_job)
            self.count_job=self.after(250,self.update_counts)
    def update_counts(self):
        self.count_job=None
        words,lines,sections=lyric_counts(self.text.get('1.0','end-1c'))
        self.counts.configure(text=f'{words} words / {lines} lines / {sections} sections')
    def append_dictation(self,phrase):
        remove,addition=append_edit(self.text.get('1.0','end-1c'),phrase,self.commands.get(),self.phrase_lines.get())
        if not remove and not addition:return
        self.text.edit_separator();self.text.configure(autoseparators=False)
        try:
            if remove:self.text.delete(f'end-1c - {remove} chars','end-1c')
            if addition:self.text.insert('end',addition)
        finally:self.text.configure(autoseparators=True);self.text.edit_separator()
        self.text.see('end');self.dirty=True
    def content(self):return dict(self.doc,title=self.title.get(),text=self.text.get('1.0','end-1c'))
    def flush(self,show=False):
        if not self.dirty and self.title.get()==self.doc['title'] and self.text.get('1.0','end-1c')==self.doc['text']:return True
        try:
            self.doc,self.revision=self.store.save(self.content(),self.revision)
            self.loading=True;self.title.set(self.doc['title']);self.loading=False;self.dirty=False
            self.status.configure(text='Saved locally: '+self.doc['title']+' | '+self.doc['modified'][:19].replace('T',' ')+' UTC')
            self.refresh_songs();return True
        except Exception as error:
            self.status.configure(text='NOT SAVED: '+str(error))
            if show:messagebox.showerror('Lyrics not saved',str(error)+'\nYour text remains in the editor. Export TXT to keep a separate copy.')
            return False
    def autosave(self):
        self.flush();self.after(2000,self.autosave)
    def refresh_songs(self):
        try:
            songs=self.store.list();query=self.query.get().strip().casefold()
            songs=[d for d in songs if query in (d['title']+' '+d['text']).casefold()]
            self.song_ids=[d['id'] for d in songs]
            self.saved['values']=[d['title']+' / '+d.get('modified','')[:10] for d in songs]
            if self.doc['id'] in self.song_ids:self.saved.current(self.song_ids.index(self.doc['id']))
            else:self.saved.set('')
        except Exception as error:self.status.configure(text='Could not list songs: '+str(error))
    def set_document(self,data,revision):
        self.loading=True;self.doc=data;self.revision=revision;self.title.set(data['title'])
        self.text.delete('1.0','end');self.text.insert('1.0',data['text']);self.text.edit_reset();self.text.edit_modified(False)
        self.update_counts();self.loading=False;self.dirty=False;self.status.configure(text='Opened: '+data['title']);self.refresh_songs()
    def leave_current(self):
        if self.flush():return True
        return messagebox.askyesno('Lyrics not saved','The current draft could not be saved. Discard the displayed edits and continue? Choose No and Export TXT first to keep them.')
    def new(self):
        if self.listening:return
        if self.leave_current():self.set_document(self.store.new(),None);self.saved.set('')
    def open(self):
        if self.listening:return
        index=self.saved.current()
        if index<0:return
        identifier=self.song_ids[index]
        if not self.leave_current():return
        try:self.set_document(*self.store.load(identifier))
        except Exception as error:messagebox.showerror('Could not open lyrics',str(error))
    def restore(self,checkpoints=False):
        if self.listening:return
        try:
            versions=([dict(e['song'],modified=e['created'],checkpoint_label=e['label']) for e in self.store.checkpoints(self.doc['id'])] if checkpoints else self.store.history(self.doc['id']))
            if not versions:messagebox.showinfo('Lyric versions','No named checkpoints yet. Use Save checkpoint.' if checkpoints else 'No earlier saved version yet. History starts after your second save.');return
            window=tk.Toplevel(self);window.title(('Checkpoints' if checkpoints else 'Lyric history')+' — restore as a new song');window.geometry('650x450')
            window.transient(self.winfo_toplevel());window.grab_set()
            ttk.Label(window,text=('Named checkpoints.' if checkpoints else 'Last 20 saves.')+' Restoring creates a separate song and keeps the current draft.').pack(pady=8)
            selector=ttk.Combobox(window,state='readonly',values=[str(i+1)+' / '+d.get('modified','')[:19]+' / '+d.get('checkpoint_label',d['title'])[:40] for i,d in enumerate(versions)])
            selector.pack(fill='x',padx=10);selector.current(0)
            preview=tk.Text(window,wrap='word',height=12);preview.pack(fill='both',expand=True,padx=10,pady=8)
            def show(event=None):
                preview.configure(state='normal');preview.delete('1.0','end');preview.insert('1.0',versions[selector.current()]['text']);preview.configure(state='disabled')
            selector.bind('<<ComboboxSelected>>',show);show()
            def recover():
                if not self.flush(True):return
                data=self.store.new();old=versions[selector.current()]
                data.update(title=(old['title'][:188]+' (recovered)'),text=old['text'])
                try:saved,revision=self.store.save(data)
                except Exception as error:messagebox.showerror('Recovery failed',str(error),parent=window);return
                self.query.set('');self.set_document(saved,revision);window.destroy()
            ttk.Button(window,text='Restore as new song',command=recover).pack(pady=8)
        except Exception as error:messagebox.showerror('Lyric history',str(error))
    def checkpoint(self):
        if self.listening:return
        label=simpledialog.askstring('Save checkpoint','Name this version (for example: Original hook):',parent=self)
        if label is None:return
        if not self.flush(True):return
        try:
            self.store.checkpoint(self.content(),label)
            self.status.configure(text='Checkpoint saved: '+label.strip()+' — retained beyond rolling history.')
        except Exception as error:messagebox.showerror('Checkpoint not saved',str(error))
    def import_text(self):
        if self.listening:return
        path=filedialog.askopenfilename(title='Import lyrics as a NEW song',filetypes=[('Plain text','*.txt')])
        if not path:return
        try:title,text=read_lyrics(path)
        except Exception as error:messagebox.showerror('Could not import lyrics',str(error));return
        if not self.flush(True):return
        data=self.store.new();data.update(title=title,text=text)
        try:saved,revision=self.store.save(data)
        except Exception as error:messagebox.showerror('Could not save imported lyrics',str(error));return
        self.query.set('');self.set_document(saved,revision)
    def section(self,name):
        self.text.edit_separator();self.text.insert('insert','\n['+name+']\n');self.text.edit_separator();self.text.focus_set()
    def undo(self):
        try:self.text.edit_undo()
        except tk.TclError:pass
    def redo(self):
        try:self.text.edit_redo()
        except tk.TclError:pass
    def export(self):
        path=filedialog.asksaveasfilename(title='Export lyrics as a NEW text file',defaultextension='.txt',initialfile='IDW-lyrics.txt',filetypes=[('Text file','*.txt')])
        if not path:return
        try:self.write('Lyrics exported: '+str(self.store.export_text(path,self.title.get(),self.text.get('1.0','end-1c'))))
        except Exception as error:messagebox.showerror('Lyrics export',str(error))
    def folder(self):
        try:
            self.store.directory.mkdir(parents=True,exist_ok=True)
            if os.name=='nt':os.startfile(self.store.directory)
            else:
                import webbrowser
                webbrowser.open(self.store.directory.as_uri())
        except Exception as error:messagebox.showerror('Lyrics folder',str(error))
    def refresh_devices(self):
        if self.listening:return
        try:
            devices=input_devices();self.device_ids=[None]+[d[0] for d in devices]
            self.devices['values']=['Default microphone']+[str(i)+' / '+name for i,name in devices];self.devices.current(0)
        except Exception as error:messagebox.showerror('Microphone devices',str(error))
    def start(self):
        if self.listening:return
        if self.is_busy():messagebox.showinfo('Audio Lab is working','Wait for the current audio job to finish before starting dictation.');return
        self.listening=True;self.mic.configure(text='Preparing offline dictation…');self.partial.configure(text='')
        for button in (self.start_button,self.new_button,self.open_button,self.restore_button,self.refresh_button,self.command_toggle,self.line_toggle,self.import_button,self.checkpoint_button,self.checkpoints_button):button.configure(state='disabled')
        self.channel.configure(state='disabled');self.devices.configure(state='disabled');self.stop_button.configure(state='normal')
        try:self.dictation.start(self.device_ids[max(0,self.devices.current())],self.channel.current()+1)
        except Exception as error:
            self.dictation.events.put(('error',str(error)));self.dictation.events.put(('done','MIC OFF — could not start dictation.'))
    def stop(self):
        self.dictation.stop();self.stop_button.configure(state='disabled');self.mic.configure(text='Stopping dictation; finishing the last phrase…')
    def close_when_ready(self,callback):
        if self.listening:self.pending_close=callback;self.stop()
        else:callback()
    def poll(self):
        db,clipped=self.dictation.level
        self.meter['value']=max(0,db+60) if self.listening else 0
        self.level_text.configure(text=(f'{db:.0f} dBFS — '+('Clipping: lower input gain' if clipped else 'Quiet: check mic/gain' if db < -55 else 'Signal detected')) if self.listening else 'Mic idle')
        try:
            while True:
                kind,value=self.dictation.events.get_nowait()
                if kind=='text':
                    self.append_dictation(value);self.partial.configure(text='')
                elif kind=='partial':self.partial.configure(text=value[:300])
                elif kind=='error':self.write('Dictation error: '+value);self.partial.configure(text=value[:500])
                elif kind=='status':self.mic.configure(text=value)
                elif kind=='done':
                    self.listening=False;self.mic.configure(text=value)
                    for button in (self.start_button,self.new_button,self.open_button,self.restore_button,self.refresh_button,self.command_toggle,self.line_toggle,self.import_button,self.checkpoint_button,self.checkpoints_button):button.configure(state='normal')
                    self.channel.configure(state='readonly');self.devices.configure(state='readonly');self.stop_button.configure(state='disabled');self.flush()
                    if self.pending_close:
                        callback=self.pending_close;self.pending_close=None;callback()
                        try:
                            if self.winfo_exists():self.after(100,self.poll)
                        except tk.TclError:pass
                        return
        except queue.Empty:pass
        self.after(100,self.poll)

