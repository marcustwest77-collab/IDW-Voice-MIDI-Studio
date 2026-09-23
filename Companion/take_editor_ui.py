"""Tk piano-roll UI. File mutations occur only through Export copy."""
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from pathlib import Path
from take_editor import Take

class TakeEditor(ttk.Frame):
    ROW = 14
    def __init__(self,parent,write):
        super().__init__(parent,padding=10)
        self.write=write;self.take=None;self.selected=None;self.scale=48;self.track=None;self.track_ids=[None]
        bar=ttk.Frame(self);bar.pack(fill='x')
        for label,fn in [('Open MIDI',self.open),('Export copy',self.export),('Undo',self.undo),('Redo',self.redo),('Reset to original',self.reset)]:
            ttk.Button(bar,text=label,command=fn).pack(side='left',padx=2)
        self.summary=ttk.Label(self,wraplength=700,text='Open a transcription or recorded .mid take. Edits stay in memory until Export copy.');self.summary.pack(anchor='w',pady=5)
        tools=ttk.Frame(self);tools.pack(fill='x',pady=3)
        ttk.Label(tools,text='Track:').pack(side='left')
        self.tracks=ttk.Combobox(tools,state='readonly',width=21,values=['All tracks']);self.tracks.current(0);self.tracks.pack(side='left',padx=4)
        self.tracks.bind('<<ComboboxSelected>>',self.filter)
        ttk.Button(tools,text='Zoom +',command=lambda:self.zoom(2)).pack(side='right',padx=2)
        ttk.Button(tools,text='Zoom -',command=lambda:self.zoom(.5)).pack(side='right',padx=2)
        tools=ttk.Frame(self);tools.pack(fill='x',pady=3)
        self.shift=tk.StringVar(value='0');ttk.Entry(tools,textvariable=self.shift,width=4).pack(side='left')
        ttk.Button(tools,text='Transpose semitones',command=self.transpose).pack(side='left',padx=4)
        self.grid=ttk.Combobox(tools,state='readonly',width=5,values=['1/4','1/8','1/16','1/32']);self.grid.set('1/16');self.grid.pack(side='left')
        ttk.Button(tools,text='Snap starts',command=self.quantize).pack(side='left',padx=4)
        area=ttk.Frame(self);area.pack(fill='x',pady=6)
        self.canvas=tk.Canvas(area,height=210,background='#101722',highlightthickness=0)
        self.canvas.grid(row=0,column=0,sticky='nsew');area.columnconfigure(0,weight=1)
        self.xscroll=ttk.Scrollbar(area,orient='horizontal',command=self.scroll_x);self.xscroll.grid(row=1,column=0,sticky='ew')
        self.yscroll=ttk.Scrollbar(area,orient='vertical',command=self.scroll_y);self.yscroll.grid(row=0,column=1,sticky='ns')
        self.canvas.configure(xscrollcommand=self.xscroll.set,yscrollcommand=self.yscroll.set)
        self.canvas.bind('<Configure>',lambda event:self.draw());self.canvas.bind('<Button-1>',self.click)
        edit=ttk.Frame(self);edit.pack(fill='x')
        self.fields={}
        for name,label,value in [('pitch','MIDI pitch','60'),('velocity','Velocity','100'),('start','Start beats','0'),('length','Length beats','1')]:
            ttk.Label(edit,text=label).pack(side='left',padx=(5,2));var=tk.StringVar(value=value);self.fields[name]=var
            ttk.Entry(edit,textvariable=var,width=7).pack(side='left')
        ttk.Button(edit,text='Apply note',command=self.apply).pack(side='left',padx=4)
        ttk.Button(edit,text='Delete note',command=self.delete).pack(side='left')
        ttk.Label(self,wraplength=700,text='Click a note to edit. Beats are quarter notes starting at 0. Transpose skips channel-10 drums.\nSnap moves note starts only; lengths and controller/pitch-bend times stay unchanged. Audition the exported MIDI in your DAW.').pack(anchor='w',pady=5)
        self.draw()

    def confirm_discard(self):
        return not (self.take and self.take.dirty) or messagebox.askyesno('Unsaved MIDI edits','Discard the current unsaved edits? Export copy first if you want to keep them.')

    def open(self):
        path=filedialog.askopenfilename(filetypes=[('MIDI files','*.mid *.midi')])
        if path and self.confirm_discard():
            self.attempt(lambda:self.load_path(path))

    def load_path(self,path):
        take=Take.load(path) # Keep the current take intact if loading fails.
        self.take=take;self.selected=None;self.track=None
        self.track_ids=[None]+sorted({n.track for n in take.notes})
        self.tracks['values']=['All tracks']+['Track '+str(t+1) for t in self.track_ids[1:]];self.tracks.current(0)
        self.canvas.xview_moveto(0)
        top=max((n.pitch for n in take.notes),default=72)
        self.canvas.configure(scrollregion=(0,0,1000,128*self.ROW+24))
        self.canvas.yview_moveto(max(0,(127-top-2)*self.ROW)/(128*self.ROW+24))
        self.refresh();self.write('Loaded MIDI for editing: '+str(path))

    def attempt(self,fn):
        try: fn();self.refresh()
        except Exception as error: messagebox.showerror('Take editor',str(error))

    def refresh(self):
        if self.take:
            name=self.take.source.name if self.take.source else 'Untitled'
            self.summary.configure(text=name+' | '+str(len(self.take.notes))+' notes | '+('unsaved edits' if self.take.dirty else 'no unsaved edits'))
            current=next((n for n in self.take.notes if n.id==self.selected),None)
            if current is None: self.selected=None
            else:
                for name,value in [('pitch',current.pitch),('velocity',current.velocity),('start',current.start/self.take.ticks_per_beat),('length',(current.end-current.start)/self.take.ticks_per_beat)]:
                    self.fields[name].set(str(value))
        self.draw()

    def filter(self,event=None):
        self.track=self.track_ids[self.tracks.current()];self.selected=None;self.draw()
    def undo(self):
        if self.take:self.take.undo();self.selected=None;self.refresh()
    def redo(self):
        if self.take:self.take.redo();self.selected=None;self.refresh()
    def reset(self):
        if self.take:self.take.reset();self.selected=None;self.refresh()
    def transpose(self):
        if self.take:self.attempt(lambda:self.take.transpose(self.shift.get(),self.track))
    def quantize(self):
        if self.take:self.attempt(lambda:self.take.quantize(4/int(self.grid.get().split('/')[1]),self.track))
    def apply(self):
        if self.take and self.selected is not None:
            self.attempt(lambda:self.take.edit(self.selected,self.fields['pitch'].get(),self.fields['velocity'].get(),self.fields['start'].get(),self.fields['length'].get()))
    def delete(self):
        if self.take and self.selected is not None:self.attempt(lambda:self.take.delete(self.selected))
    def export(self):
        if not self.take:return
        path=filedialog.asksaveasfilename(title='Export a NEW MIDI file',defaultextension='.mid',initialfile=self.take.source.stem+'-edited.mid',filetypes=[('MIDI file','*.mid')])
        if path:self.attempt(lambda:self.write('Saved edited MIDI copy: '+str(self.take.export_copy(path))))
    def zoom(self,factor):
        self.scale=max(12,min(192,self.scale*factor));self.draw()
    def scroll_x(self,*args):self.canvas.xview(*args);self.draw()
    def scroll_y(self,*args):self.canvas.yview(*args);self.draw()

    def draw(self):
        c=self.canvas;c.delete('all')
        if not self.take:
            c.create_text(25,35,anchor='nw',fill='#68dfbd',text='Open MIDI to review and correct your take.');return
        notes=[n for n in self.take.notes if self.track is None or n.track==self.track]
        tpb=self.take.ticks_per_beat
        last=max((n.end/tpb for n in self.take.notes),default=16)
        width=max(c.winfo_width(),50+(last+4)*self.scale)
        c.configure(scrollregion=(0,0,width,128*self.ROW+24))
        left=c.canvasx(0);right=left+c.winfo_width();top=c.canvasy(0);bottom=top+c.winfo_height()
        for pitch in range(128):
            y=24+(127-pitch)*self.ROW
            if y+self.ROW<top or y>bottom:continue
            color='#1a2330' if pitch%12 in (1,3,6,8,10) else '#253141'
            c.create_rectangle(left,y,right,y+self.ROW,fill=color,outline='#334153')
        first=max(0,int((left-50)/self.scale));end=int((right-50)/self.scale)+2
        for beat in range(first,end):
            x=50+beat*self.scale;c.create_line(x,top,x,bottom,fill='#546071' if beat%4==0 else '#344153')
        for n in notes:
            x1=50+n.start/tpb*self.scale;x2=50+n.end/tpb*self.scale;y=24+(127-n.pitch)*self.ROW
            if x2<left+40 or x1>right or y+self.ROW<top or y>bottom:continue
            color='#edc467' if n.id==self.selected else ('#b797df' if n.channel==9 else '#66d5b4')
            c.create_rectangle(x1,y+1,max(x1+3,x2),y+self.ROW-1,fill=color,outline='#0b1118',tags=('note',str(n.id)))
        c.create_rectangle(left,top,left+42,bottom,fill='#101722',outline='')
        for pitch in range(128):
            y=24+(127-pitch)*self.ROW
            if top<=y<=bottom:
                label=('C','C#','D','D#','E','F','F#','G','G#','A','A#','B')[pitch%12]+str(pitch//12-1)
                c.create_text(left+4,y+7,anchor='w',fill='#dbe4ef',font=('Segoe UI',8),text=label)
        c.create_rectangle(left,top,right,top+19,fill='#101722',outline='')
        for beat in range(first,end):
            if 50+beat*self.scale>=left+42:c.create_text(50+beat*self.scale+2,top+3,anchor='nw',fill='#b5c2d3',text=str(beat))

    def click(self,event):
        if not self.take:return
        hits=self.canvas.find_overlapping(self.canvas.canvasx(event.x),self.canvas.canvasy(event.y),self.canvas.canvasx(event.x),self.canvas.canvasy(event.y))
        for item in reversed(hits):
            tags=self.canvas.gettags(item)
            if tags and tags[0]=='note':
                self.selected=int(tags[1]);n=next(n for n in self.take.notes if n.id==self.selected)
                for name,value in [('pitch',n.pitch),('velocity',n.velocity),('start',n.start/self.take.ticks_per_beat),('length',(n.end-n.start)/self.take.ticks_per_beat)]:
                    self.fields[name].set(str(value))
                self.summary.configure(text='Selected note '+str(n.pitch)+' | track '+str(n.track+1)+' | channel '+str(n.channel+1))
                self.draw();return
