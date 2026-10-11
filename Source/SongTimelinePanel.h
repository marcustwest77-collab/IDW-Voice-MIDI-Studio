#pragma once
#include <JuceHeader.h>
#include "SongStudio.h"
#include "SongGrid.h"
#include "SongTimelineView.h"

class SongTimelinePanel final : public juce::Component,private juce::Timer {
    class Timeline final : public juce::Component,public juce::SettableTooltipClient {
    public:
        idw::SongTimelineView viewport;
        std::array<float,6> levels{},holds{};std::function<void()> resetPeaks;
        static juce::String peakText(float peak){return peak<=.000001f?"−∞":juce::String(20*std::log10(peak),1);}
        void meterUpdate(const std::array<float,6>& peaks){for(size_t i=0;i<levels.size();++i){levels[i]=std::max(peaks[i],levels[i]*.75f);holds[i]=std::max(holds[i],peaks[i]);}repaint();}
        struct ClipView {idw::SongLane clip;std::array<float,256> peaks{};};
        std::vector<SongStudio::Section> sections;double secondsPerBar=2;std::array<std::vector<ClipView>,4> rows;int selected=0,selectedClip=0,armed=0;double duration=10,position=0,cursor=0,gridSeconds=0;
        std::function<void(int,int,double)> select;
        void setClips(const std::array<std::vector<idw::SongLane>,4>& clips){
            std::map<const void*,std::array<float,256>> cache;
            for(int row=0;row<4;++row){rows[(size_t)row].clear();for(const auto& clip:clips[(size_t)row]){
                ClipView view;view.clip=clip;auto found=cache.find(clip.audio.get());
                if(found!=cache.end())view.peaks=found->second;
                else if(clip.audio&&clip.frames>0){for(int bin=0;bin<256;++bin){const int begin=(int)((std::int64_t)clip.frames*bin/256),end=(int)((std::int64_t)clip.frames*(bin+1)/256),step=juce::jmax(1,(end-begin)/64);for(int i=begin;i<end;i+=step)for(int ch=0;ch<2;++ch)view.peaks[(size_t)bin]=juce::jmax(view.peaks[(size_t)bin],std::abs(clip.audio->getSample(ch,i)));}cache[clip.audio.get()]=view.peaks;}
                rows[(size_t)row].push_back(std::move(view));
            }}repaint();
        }
        void mouseDown(const juce::MouseEvent& event)override{
            if(event.y<22&&event.x<168){if(resetPeaks)resetPeaks();return;}
            const int row=juce::jlimit(0,3,(event.y-22)/juce::jmax(1,(getHeight()-22)/4));
            int index=0;double at=cursor;
            if(event.x>=168){at=viewport.timeAt((event.x-168.0)/juce::jmax(1,getWidth()-168));
                const auto& clips=rows[(size_t)row];for(int i=(int)clips.size()-1;i>=0;--i){const auto& clip=clips[(size_t)i].clip;const auto end=clip.settings.trimOut<0?clip.duration():clip.settings.trimOut;if(at>=clip.settings.start&&at<clip.settings.start+end-clip.settings.trimIn){index=i;break;}}
            }
            if(select)select(row,index,at);
        }
        void paint(juce::Graphics& g)override{
            const float left=168,width=(float)getWidth()-left;const int rowHeight=(getHeight()-22)/4;
            g.setColour(findColour(juce::Label::textColourId));g.drawText(juce::String(viewport.start,2)+" s",168,0,90,20,juce::Justification::left);g.drawText(juce::String(viewport.start+viewport.span(),2)+" s",getWidth()-85,0,85,20,juce::Justification::right);
            g.setColour(findColour(juce::Label::textColourId));g.drawRect(4,1,150,19);g.drawText("Reset peaks",4,1,150,19,juce::Justification::centred);
            g.setColour(holds[5]>.94f?juce::Colours::orange:findColour(juce::Label::textColourId));g.drawText("Beat peak "+peakText(holds[4])+" dBFS  /  Mix peak "+peakText(holds[5])+" dBFS"+(holds[5]>.94f?"  LIMIT":""),270,0,juce::jmax(0,getWidth()-360),20,juce::Justification::centred);
            for(int row=0;row<4;++row){const int y=22+row*rowHeight;
                g.setColour(findColour(juce::ComboBox::backgroundColourId).brighter(row==selected?.25f:.08f));g.fillRect(0,y,getWidth(),rowHeight-3);
                g.setColour(findColour(juce::Label::textColourId));g.drawText(juce::String(row==armed?"● ":"")+juce::String(row+1)+" / "+SongStudio::laneName(row),8,y+2,155,16,juce::Justification::left);
                const auto& clips=rows[(size_t)row];g.setColour(holds[(size_t)row]>=1?juce::Colours::orangered:findColour(juce::Label::textColourId));g.drawText("Peak "+peakText(holds[(size_t)row])+" dBFS",8,y+18,150,12,juce::Justification::left);
                if(rowHeight>=39){g.setColour(juce::Colours::black.withAlpha(.35f));g.fillRect(8,y+rowHeight-7,145,3);const float amount=levels[(size_t)row]>0?juce::jlimit(0.f,1.f,(20*std::log10(levels[(size_t)row])+60)/60):0;g.setColour(levels[(size_t)row]>=1?juce::Colours::orangered:juce::Colours::aqua);g.fillRect(8,y+rowHeight-7,(int)(145*amount),3);}
                g.saveState();g.reduceClipRegion(168,y,juce::jmax(0,(int)width),rowHeight-3);
                if(gridSeconds>0&&width>0){const double step=gridSeconds*juce::jmax(1.,std::ceil(6.0*viewport.span()/(width*gridSeconds)));g.setColour(juce::Colours::white.withAlpha(.12f));for(double at=std::ceil(viewport.start/step)*step;at<viewport.start+viewport.span();at+=step)g.drawVerticalLine((int)(left+viewport.fractionAt(at)*width),(float)y,(float)(y+rowHeight-3));}
                for(int index=0;index<(int)clips.size();++index){const auto& view=clips[(size_t)index];const auto& clip=view.clip;const double end=clip.settings.trimOut<0?clip.duration():clip.settings.trimOut;
                    const float x=left+(float)(viewport.fractionAt(clip.settings.start))*width,xEnd=juce::jmin((float)getWidth(),x+(float)((end-clip.settings.trimIn)/viewport.span())*width);if(x>=getWidth()||xEnd<=left||xEnd<=x)continue;
                    const bool picked=row==selected&&index==selectedClip;
                    g.setColour(juce::Colours::gold.withAlpha((clip.settings.mute||clip.settings.clipMuted)?.1f:picked?.4f:.2f));g.fillRect(x,(float)y+3,xEnd-x,(float)rowHeight-8);
                    g.setColour(juce::Colours::gold.withAlpha((clip.settings.mute||clip.settings.clipMuted)?.3f:.9f));
                    for(int pixel=juce::jmax(168,(int)std::ceil(x));pixel<(int)xEnd;++pixel){
                        const double time=viewport.timeAt((pixel-left)/width),source=time-clip.settings.start+clip.settings.trimIn;
                        float peak=0;
                        if(viewport.zoom==1){const int bin=juce::jlimit(0,255,(int)(source/clip.duration()*256));peak=view.peaks[(size_t)bin];}
                        else if(clip.audio){const int begin=juce::jlimit(0,clip.frames-1,(int)(source*clip.rate));const int finish=juce::jlimit(begin+1,clip.frames,(int)std::ceil((source+viewport.span()/width)*clip.rate));const int stride=juce::jmax(1,(finish-begin)/64);for(int sample=begin;sample<finish;sample+=stride)for(int channel=0;channel<2;++channel)peak=juce::jmax(peak,std::abs(clip.audio->getSample(channel,sample)));}
                        const float height=juce::jmin(1.f,peak)*(rowHeight-16)*.5f*clip.settings.envelope(time-clip.settings.start,clip.settings.length(clip.duration())),middle=y+rowHeight*.5f;g.drawVerticalLine(pixel,middle-height,middle+height);
                    }
                    g.setColour(picked?juce::Colours::white:juce::Colours::gold);g.drawRect(x,(float)y+3,xEnd-x,(float)rowHeight-8,picked?2.f:1.f);g.drawText(juce::String(index+1)+(clip.settings.clipMuted?" M":""),juce::jmax(168,(int)x)+3,y+2,55,18,juce::Justification::left);
                    const float endX=left+(float)(viewport.fractionAt(clip.settings.start+end-clip.settings.trimIn))*width;
                    const auto clipBounds=juce::Rectangle<int>((int)x,y+3,juce::jmax(1,(int)(xEnd-x)),rowHeight-8);g.saveState();g.reduceClipRegion(clipBounds);g.setColour(juce::Colours::aqua.withAlpha(.8f));
                    if(clip.settings.fadeIn>0)g.drawLine(x,(float)(y+rowHeight-5),x+(float)(clip.settings.fadeIn/viewport.span())*width,(float)y+4,1.5f);
                    if(clip.settings.fadeOut>0)g.drawLine(endX-(float)(clip.settings.fadeOut/viewport.span())*width,(float)y+4,endX,(float)(y+rowHeight-5),1.5f);g.restoreState();
                }
                g.restoreState();
            }
            g.saveState();g.reduceClipRegion(168,0,juce::jmax(0,(int)width),getHeight());
            float labelEnd=left+94;for(const auto& section:sections){const float x=left+(float)(viewport.fractionAt((section.firstBar-1)*secondsPerBar))*width;if(x<left||x>=getWidth())continue;g.setColour(juce::Colours::aqua.withAlpha(.65f));g.drawVerticalLine((int)x,22.f,(float)getHeight());if(x>=labelEnd){g.drawText(section.name,(int)x+2,0,juce::jmin(100,getWidth()-(int)x-2),20,juce::Justification::left);labelEnd=x+104;}}
            g.setColour(juce::Colours::aqua);if(viewport.contains(cursor))g.drawVerticalLine((int)(left+viewport.fractionAt(cursor)*width),22.f,(float)getHeight());
            g.setColour(juce::Colours::white);if(viewport.contains(position))g.drawVerticalLine((int)(left+viewport.fractionAt(position)*width),22.f,(float)getHeight());g.restoreState();
        }
    };
public:
    explicit SongTimelinePanel(SongStudio& studio,std::function<void()> enableMic):engine(studio),monitor(std::move(enableMic)){
        for(auto* c:std::initializer_list<juce::Component*>{&takes,&clipMute,&timeline,&zoomIn,&zoomOut,&fit,&scroll,&viewLabel,&playCursor,&seekCursor,&recordCursor,&arm,&import,&remove,&undo,&redo,&apply,&clipChoice,&split,&duplicate,&move,&cursor,&cursorLabel,&grid,&fadeIn,&fadeOut,&fadeInLabel,&fadeOutLabel,&crossfade,&start,&in,&out,&gain,&pan,&startLabel,&inLabel,&outLabel,&gainLabel,&panLabel,&mute,&solo,&message})addAndMakeVisible(c);
        clipMute.onClick=[this]{result(engine.setClipMuted(selected,selectedClip,clipMute.getToggleState()),"Clip mute changed.");};
        clipMute.setTooltip("Mutes only this clip. Applied immediately while stopped; separate from Track mute.");
        takes.setTooltip("Use a whole take, or use its trimmed phrase while keeping surrounding audio. Stop transport first; Undo restores the previous choice.");
        takes.onClick=[this]{juce::PopupMenu menu;menu.addItem(1,"Use this take (mute whole overlapping clips)");menu.addItem(2,"Unmute this take and overlapping clips");menu.addItem(3,"Use this phrase (keep surrounding audio)");
            const juce::Component::SafePointer<SongTimelinePanel> safe(this);const int lane=selected,clip=selectedClip;const auto revision=engine.audioRevision();
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&takes),[safe,lane,clip,revision](int choice){if(!safe||choice==0)return;if(safe->engine.audioRevision()!=revision){safe->message.setText("Clips changed while the menu was open. Select the take again.",juce::dontSendNotification);return;}
                if(choice==3){const bool ok=safe->engine.selectPhrase(lane,clip);safe->result(ok,"Phrase selected; surrounding audio kept. Undo restores all splits.");if(!ok)safe->message.setText("Phrase rejected: stop transport, keep a positive phrase within 180 s, avoid splits inside fades or shorter than one sample, and allow room within 64 clips.",juce::dontSendNotification);return;}
                safe->result(safe->engine.selectTake(lane,clip,choice==2),choice==1?"Selected take enabled; whole overlapping clips on this track muted. Track mute/solo still applies.":"Selected take and overlapping clips unmuted; they will mix together.");});};
        timeline.resetPeaks=[this]{engine.consumeMeterPeaks();timeline.levels.fill(0);timeline.holds.fill(0);timeline.repaint();};
        timeline.setTooltip("Sample peaks after track/master gain, before the output limiter. Peak holds last until Reset peaks or closing Song Studio. Mix includes the metronome; LIMIT marks peaks above the limiter knee. Not true-peak or loudness meters.");
        scroll.setSliderStyle(juce::Slider::LinearHorizontal);scroll.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,26);scroll.setRange(0,180,0);scroll.onValueChange=[this]{timeline.viewport.setStart(scroll.getValue());updateViewControls();};
        zoomIn.onClick=[this]{timeline.viewport.setZoom(timeline.viewport.zoom*2,cursor.getValue());updateViewControls();};zoomOut.onClick=[this]{timeline.viewport.setZoom(timeline.viewport.zoom*.5,cursor.getValue());updateViewControls();};fit.onClick=[this]{timeline.viewport.fit();updateViewControls();};
        recordCursor.onClick=[this]{if(engine.isPlaying())return;monitor();const bool ok=engine.recordAtCursor(snapped(cursor.getValue()));message.setText(ok?"Recording a NEW clip on the armed track at the cursor, after any count-in. Existing clips are kept and monitored.":"Cannot record: stop transport, place the cursor before song end and check the 64-clip/audio-memory limits and audio device.",juce::dontSendNotification);};
        recordCursor.setTooltip("Adds a new recording on the ARMED track at the edit cursor. Existing overlapping clips play and remain intact. Use headphones.");
        playCursor.onClick=[this]{const bool ok=engine.start(false,snapped(cursor.getValue()));message.setText(ok?"Playing from edit cursor to song end.":"Stop transport and place the cursor before the song end.",juce::dontSendNotification);};
        seekCursor.onClick=[this]{const bool ok=engine.seekPlayback(snapped(cursor.getValue()));message.setText(ok?"Playback moved to edit cursor.":"Seek requires playback and a cursor inside its current song/section range. Recording cannot seek.",juce::dontSendNotification);};
        startLabel.setText("Start / s",juce::dontSendNotification);inLabel.setText("Trim in / s",juce::dontSendNotification);outLabel.setText("Trim out / s",juce::dontSendNotification);gainLabel.setText("Track gain",juce::dontSendNotification);panLabel.setText("Track pan",juce::dontSendNotification);cursorLabel.setText("Edit cursor / s",juce::dontSendNotification);fadeInLabel.setText("Fade in / s",juce::dontSendNotification);fadeOutLabel.setText("Fade out / s",juce::dontSendNotification);
        for(auto* slider:{&start,&in,&out,&gain,&pan,&cursor,&fadeIn,&fadeOut}){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,68,26);}
        start.setRange(0,180,0);in.setRange(0,180,0);out.setRange(0,180,0);gain.setRange(0,2,.01);pan.setRange(-1,1,.01);cursor.setRange(0,180,0);fadeIn.setRange(0,180,0);fadeOut.setRange(0,180,0);
        grid.addItem("Snap: Off",1);grid.addItem("Snap: Bar",2);grid.addItem("Snap: Beat",3);grid.addItem("Snap: 1/2 beat",4);grid.addItem("Snap: 1/4 beat",5);grid.setSelectedId(1,juce::dontSendNotification);
        grid.onChange=[this]{cursor.setValue(snapped(cursor.getValue()));timeline.gridSeconds=gridBeats()*60.0/engine.pattern().bpm;timeline.repaint();};
        cursor.onValueChange=[this]{cursor.setValue(snapped(cursor.getValue()),juce::dontSendNotification);timeline.cursor=cursor.getValue();timeline.repaint();};
        timeline.select=[this](int lane,int clip,double at){selected=lane;selectedClip=clip;cursor.setValue(at);refreshChoice();refreshFields();};
        clipChoice.onChange=[this]{selectedClip=juce::jmax(0,clipChoice.getSelectedId()-1);refreshFields();};
        arm.onClick=[this]{if(engine.armLane(selected))message.setText("Armed: "+juce::String(SongStudio::laneName(selected))+". Use Record cursor to add a take, or main Record on an empty track.",juce::dontSendNotification);};
        apply.onClick=[this]{auto settings=selectedSettings();settings.start=snapped(start.getValue());settings.fadeIn=fadeIn.getValue();settings.fadeOut=fadeOut.getValue();settings.trimIn=in.getValue();settings.trimOut=out.getValue()==shownOut?settings.trimOut:out.getValue();settings.gain=(float)gain.getValue();settings.pan=(float)pan.getValue();settings.mute=mute.getToggleState();settings.solo=solo.getToggleState();const bool ok=hasClip()?engine.setClipSettings(selected,selectedClip,settings):engine.setLaneSettings(selected,settings);result(ok,"Clip timing and track mix applied.");};
        import.onClick=[this]{chooseImport();};
        remove.onClick=[this]{if(!hasClip())return;const juce::Component::SafePointer<SongTimelinePanel> safe(this);const int lane=selected,clip=selectedClip;juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,"Remove selected clip?","Only this clip is removed. Save song first for a permanent copy.","Remove","Cancel",this,juce::ModalCallbackFunction::create([safe,lane,clip](int answer){if(safe&&answer)safe->result(safe->engine.removeClip(lane,clip),"Clip removed.");}));};
        crossfade.onClick=[this]{const bool ok=engine.crossfadeNext(selected,selectedClip);result(ok,"Linear crossfade applied to the next overlapping clip.");if(!ok)message.setText("Crossfade needs the next clip to start later, overlap this clip and reach its end. Move/trim clips first; transport must be stopped.",juce::dontSendNotification);};
        split.onClick=[this]{result(engine.splitClip(selected,selectedClip,snapped(cursor.getValue())),"Clip split at edit cursor.");};
        duplicate.onClick=[this]{const bool ok=engine.duplicateClip(selected,selectedClip,snapped(cursor.getValue()));if(ok)selectedClip=(int)engine.clips(selected).size()-1;result(ok,"Copy placed at edit cursor.");};
        move.onClick=[this]{if(!hasClip())return;auto settings=selectedSettings();settings.start=snapped(cursor.getValue());result(engine.setClipSettings(selected,selectedClip,settings),"Clip moved to edit cursor.");};
        undo.onClick=[this]{engine.undoTrackEdit();refresh();};redo.onClick=[this]{engine.redoTrackEdit();refresh();};
        message.setText("Select a clip or use the clip menu. Click timeline/set Edit cursor for Split, Copy or Move. Take options chooses one overlapping take; Clip mute affects one clip. Apply commits timing and track mix.",juce::dontSendNotification);
        refresh();startTimerHz(10);
    }
    void resized()override{
        const int w=getWidth(),h=getHeight();import.setBounds(0,0,120,30);remove.setBounds(130,0,115,30);undo.setBounds(255,0,90,30);redo.setBounds(355,0,90,30);arm.setBounds(455,0,155,30);takes.setBounds(620,0,130,30);apply.setBounds(w-170,0,170,30);
        clipChoice.setBounds(0,36,150,30);split.setBounds(160,36,130,30);duplicate.setBounds(300,36,130,30);move.setBounds(440,36,130,30);cursorLabel.setBounds(580,36,105,30);cursor.setBounds(685,36,w-685,30);
        grid.setBounds(0,72,150,30);fadeInLabel.setBounds(160,72,80,30);fadeIn.setBounds(240,72,190,30);fadeOutLabel.setBounds(440,72,90,30);fadeOut.setBounds(530,72,190,30);crossfade.setBounds(w-190,72,190,30);
        zoomOut.setBounds(0,108,70,30);zoomIn.setBounds(78,108,70,30);fit.setBounds(156,108,75,30);viewLabel.setBounds(240,108,100,30);scroll.setBounds(345,108,w-765,30);playCursor.setBounds(w-410,108,120,30);seekCursor.setBounds(w-280,108,120,30);recordCursor.setBounds(w-150,108,150,30);
        timeline.setBounds(0,146,w,h-282);const int y=h-130;
        startLabel.setBounds(0,y,74,28);start.setBounds(78,y,186,28);inLabel.setBounds(280,y,90,28);in.setBounds(374,y,190,28);outLabel.setBounds(580,y,100,28);out.setBounds(684,y,w-684,28);
        gainLabel.setBounds(0,y+38,74,28);gain.setBounds(78,y+38,186,28);panLabel.setBounds(280,y+38,90,28);pan.setBounds(374,y+38,190,28);mute.setBounds(590,y+38,110,28);solo.setBounds(710,y+38,110,28);clipMute.setBounds(825,y+38,w-825,28);message.setBounds(0,h-46,w,44);
    }
private:
    void updateViewControls(){const double maximum=timeline.viewport.duration-timeline.viewport.span();scroll.setRange(0,juce::jmax(.000001,maximum),0);scroll.setValue(timeline.viewport.start,juce::dontSendNotification);scroll.setEnabled(maximum>0);zoomIn.setEnabled(timeline.viewport.zoom<32);zoomOut.setEnabled(timeline.viewport.zoom>1);viewLabel.setText(juce::String(timeline.viewport.zoom,0)+"x / View / s",juce::dontSendNotification);timeline.repaint();}
    double gridBeats()const{const double steps[]{0,0,4,1,.5,.25};return steps[juce::jlimit(0,5,grid.getSelectedId())];}
    double snapped(double time)const{return idw::snapSongTime(time,engine.pattern().bpm,gridBeats());}
    bool hasClip()const{return selectedClip>=0&&selectedClip<(int)timeline.rows[(size_t)selected].size();}
    idw::LaneSettings selectedSettings()const{return hasClip()?timeline.rows[(size_t)selected][(size_t)selectedClip].clip.settings:engine.audioLanes()[(size_t)selected].settings;}
    void result(bool ok,const juce::String& text){message.setText(ok?text+" Save song to keep it.":"Edit rejected. Stop transport, select a clip, keep trims inside its source and split inside its unfaded region; fades must fit the trimmed length. Maximum 64 clips per track.",juce::dontSendNotification);refresh();}
    void refreshChoice(){const auto count=(int)timeline.rows[(size_t)selected].size();selectedClip=juce::jlimit(0,juce::jmax(0,count-1),selectedClip);clipChoice.clear(juce::dontSendNotification);for(int i=0;i<count;++i)clipChoice.addItem("Clip "+juce::String(i+1)+(timeline.rows[(size_t)selected][(size_t)i].clip.settings.clipMuted?" / muted":""),i+1);clipChoice.setTextWhenNothingSelected("Empty track");clipChoice.setSelectedId(count?selectedClip+1:0,juce::dontSendNotification);}
    void refreshFields(){const auto settings=selectedSettings();const double duration=hasClip()?timeline.rows[(size_t)selected][(size_t)selectedClip].clip.duration():0;in.setRange(0,juce::jmax(.001,duration),0);out.setRange(0,juce::jmax(.001,duration),0);start.setValue(settings.start,juce::dontSendNotification);in.setValue(settings.trimIn,juce::dontSendNotification);out.setValue(settings.trimOut<0?duration:settings.trimOut,juce::dontSendNotification);shownOut=out.getValue();fadeIn.setValue(settings.fadeIn,juce::dontSendNotification);fadeOut.setValue(settings.fadeOut,juce::dontSendNotification);gain.setValue(settings.gain,juce::dontSendNotification);pan.setValue(settings.pan,juce::dontSendNotification);mute.setToggleState(settings.mute,juce::dontSendNotification);solo.setToggleState(settings.solo,juce::dontSendNotification);clipMute.setToggleState(settings.clipMuted,juce::dontSendNotification);timeline.selected=selected;timeline.selectedClip=selectedClip;timeline.repaint();}
    void refresh(){if(engine.isPlaying())return;std::array<std::vector<idw::SongLane>,4> clips;for(int row=0;row<4;++row)clips[(size_t)row]=engine.clips(row);timeline.setClips(clips);const auto pattern=engine.pattern();timeline.duration=pattern.bars*240.0/pattern.bpm;timeline.viewport.setDuration(timeline.duration);updateViewControls();timeline.secondsPerBar=240.0/pattern.bpm;timeline.sections=engine.sections();std::stable_sort(timeline.sections.begin(),timeline.sections.end(),[](const auto& a,const auto& b){return a.firstBar<b.firstBar;});lastRevision=engine.audioRevision();refreshChoice();refreshFields();}
    void timerCallback()override{timeline.meterUpdate(engine.consumeMeterPeaks());const bool active=engine.isPlaying();takes.setEnabled(!active&&hasClip());clipMute.setEnabled(!active&&hasClip());playCursor.setEnabled(!active);recordCursor.setEnabled(!active);recordCursor.setButtonText("Record cursor / "+juce::String(engine.armedLane()+1));seekCursor.setEnabled(active&&!engine.isRecording());for(auto* c:std::initializer_list<juce::Component*>{&arm,&import,&apply,&fadeIn,&fadeOut,&start,&in,&out,&gain,&pan,&mute,&solo})c->setEnabled(!active);for(auto* button:{&remove,&split,&duplicate,&move,&crossfade})button->setEnabled(!active&&hasClip());undo.setEnabled(engine.canUndo());redo.setEnabled(engine.canRedo());if(!active&&lastRevision!=engine.audioRevision())refresh();if(!active)cursor.setValue(snapped(cursor.getValue()));timeline.gridSeconds=gridBeats()*60.0/engine.pattern().bpm;timeline.armed=engine.armedLane();arm.setButtonText(selected==timeline.armed?"Selected lane armed":"Arm selected lane");timeline.position=engine.seconds();timeline.repaint();}
    void chooseImport(){
        if(engine.isPlaying())return;const int lane=selected;const double at=snapped(cursor.getValue());chooser=std::make_unique<juce::FileChooser>("Add WAV clip at edit cursor",juce::File::getSpecialLocation(juce::File::userMusicDirectory),"*.wav");const juce::Component::SafePointer<SongTimelinePanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,lane,at](const juce::FileChooser& file){if(!safe||file.getResult()==juce::File{})return;const auto result=safe->engine.addWav(lane,file.getResult(),at);if(result.wasOk()){safe->selected=lane;safe->selectedClip=(int)safe->engine.clips(lane).size()-1;}safe->message.setText(result.wasOk()?"WAV added at edit cursor. Existing clips kept. Set Bars long enough for the arrangement and Save song.":result.getErrorMessage(),juce::dontSendNotification);safe->refresh();});
    }
    SongStudio& engine;std::function<void()> monitor;double shownOut=0;int selected=0,selectedClip=0;std::uint64_t lastRevision=0;Timeline timeline;
    juce::TextButton zoomOut{"Zoom −"},zoomIn{"Zoom +"},fit{"Fit song"},playCursor{"Play cursor"},seekCursor{"Seek cursor"},recordCursor{"Record cursor"};juce::Slider scroll;juce::Label viewLabel;
    juce::TextButton takes{"Take options"};juce::ToggleButton clipMute{"Clip mute"};
    juce::TextButton arm{"Arm selected lane"},import{"Add WAV clip…"},remove{"Remove clip…"},undo{"Undo edit"},redo{"Redo edit"},apply{"Apply clip / mix"},split{"Split at cursor"},duplicate{"Copy to cursor"},move{"Move to cursor"},crossfade{"Crossfade next"};
    juce::ComboBox clipChoice,grid;juce::Slider start,in,out,gain,pan,cursor,fadeIn,fadeOut;juce::Label startLabel,inLabel,outLabel,gainLabel,panLabel,cursorLabel,fadeInLabel,fadeOutLabel,message;juce::ToggleButton mute{"Track mute"},solo{"Track solo"};std::unique_ptr<juce::FileChooser> chooser;
};
