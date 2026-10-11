#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "SongPattern.h"
#include "SongMetronome.h"
#include "SongLane.h"
#include "StudioSynth.h"
#include "ReleaseSafety.h"

// Four audio lanes with nondestructive clips, armed-lane recording and a sample-clocked drum pattern.
// Edits/import/export run while stopped. Recovery writes detached immutable snapshots.
class SongStudio {
public:
    struct Section {
        juce::String name;int firstBar=1,lastBar=1;
        bool valid()const{return name.isNotEmpty()&&name.trim().isNotEmpty()&&name.length()<=64&&name.getNumBytesAsUTF8()<=256&&!name.containsChar('\n')&&!name.containsChar('\r')&&firstBar>=1&&lastBar>=firstBar&&lastBar<=64;}
    };
    std::vector<Section> sections()const{const juce::ScopedLock guard(lock);return songSections;}
    bool setSection(int index,Section section){const juce::ScopedLock guard(lock);if(playing||!section.valid()||index<0||index>(int)songSections.size()||(index==(int)songSections.size()&&songSections.size()>=32))return false;rememberEdit();if(index==(int)songSections.size())songSections.push_back(std::move(section));else songSections[(size_t)index]=std::move(section);trimHistory();++revision;return true;}
    bool removeSection(int index){const juce::ScopedLock guard(lock);if(playing||index<0||index>=(int)songSections.size())return false;rememberEdit();songSections.erase(songSections.begin()+index);trimHistory();++revision;return true;}
    bool playSection(int index,bool repeat){const juce::ScopedLock guard(lock);if(index<0||index>=(int)songSections.size())return false;const auto& section=songSections[(size_t)index];if(section.lastBar>song.bars)return false;return start(false,(section.firstBar-1)*240.0/song.bpm,section.lastBar*240.0/song.bpm,repeat);}
    bool isLooping()const{return playing.load()&&!recording.load()&&loopEnabled.load();}
    struct Lyrics {juce::String title,text;};
    struct RecoveryWriting {Lyrics lyrics;juce::String document;};
    RecoveryWriting recoveryWriting()const{const juce::ScopedLock guard(lyricsLock);return {words,documentId};}
    std::uint64_t audioRevision()const{return revision.load();}
    void forkRecoveryDocument(){const juce::ScopedLock guard(lyricsLock);documentId=juce::Uuid().toString();++revision;}
    std::unique_ptr<SongStudio> recoverySnapshot(std::uint64_t& stamp)const{
        const juce::ScopedTryLock guard(lock);if(!guard.isLocked()||playing)return {};
        auto copy=std::make_unique<SongStudio>();stamp=revision.load();
        copy->song=song;copy->takeRate=takeRate;copy->recorded=recorded;
        copy->track=track;copy->lanes=lanes;copy->extraClips=extraClips;copy->songSections=songSections;
        const auto writing=recoveryWriting();copy->words=writing.lyrics;copy->documentId=writing.document;
        return copy;
    }
    static constexpr int maxTitleBytes=1024,maxLyricsBytes=131072;
    Lyrics lyrics()const{const juce::ScopedLock guard(lyricsLock);return words;}
    bool setLyrics(Lyrics next){
        if(next.title.length()>256||next.text.length()>32768||next.title.getNumBytesAsUTF8()>maxTitleBytes||next.text.getNumBytesAsUTF8()>maxLyricsBytes)return false;
        const juce::ScopedLock guard(lyricsLock);words=std::move(next);return true;
    }
    void prepare(double nextRate){const juce::ScopedLock guard(lock);playing=false;recording=false;rate=nextRate;displayRate=nextRate;drums.prepare(rate);configureDrums(drums);countRemaining=0;click.reset();}
    bool isPlaying()const{return playing.load();}
    bool isRecording()const{return recording.load();}
    bool isCountingIn()const{return playing.load()&&countRemaining.load()>0;}
    int countInBeat()const{return countBeat.load();}
    int countInTotalBeats()const{return totalCountBeats.load();}
    int songBar()const{return displayBar.load();}
    int songBeat()const{return displayBeat.load();}
    int countInBars()const{return requestedCountBars.load();}
    bool setCountInBars(int bars){const juce::ScopedLock guard(lock);if(playing||bars<0||bars>2)return false;requestedCountBars=bars;return true;}
    bool metronomeEnabled()const{return clickEnabled.load();}
    void setMetronomeEnabled(bool enabled){clickEnabled=enabled;}
    float metronomeGain()const{return clickGain.load();}
    void setMetronomeGain(float gain){if(std::isfinite(gain))clickGain=juce::jlimit(0.f,.5f,gain);}
    double seconds()const{return position.load()/displayRate.load();}
    static const char* laneName(int lane){const char* names[]{"Lead vocal","Beat / backing","Double","Ad-lib"};return lane>=0&&lane<4?names[lane]:"Unknown";}
    int armedLane()const{return recordLane.load();}
    bool armLane(int lane){const juce::ScopedLock guard(lock);if(playing||lane<0||lane>=4)return false;recordLane=lane;return true;}
    bool armedLaneHasAudio()const{const juce::ScopedLock guard(lock);return laneHasAudio(recordLane.load());}
    bool hasTake()const{const juce::ScopedLock guard(lock);return recorded>0;}
    idw::SongPattern pattern()const{const juce::ScopedLock guard(lock);return song;}
    bool setPattern(idw::SongPattern next){const juce::ScopedLock guard(lock);if(playing||!next.valid())return false;song=next;++revision;return true;}
    bool recordAtCursor(double seconds){return start(true,seconds,0,false,true);}
    bool start(bool record,double from=0,double until=0,bool repeat=false,bool append=false){
        const juce::ScopedLock guard(lock);
        if(playing||!std::isfinite(rate)||rate<8000||rate>192000||!song.valid()||(record&&!append&&laneHasAudio(recordLane.load())))return false;
        if(!std::isfinite(from)||!std::isfinite(until)||from<0||until<0||from>180||until>180||(!record&&append)||(record&&((!append&&from!=0)||until!=0||repeat)))return false;
        if(from>=song.bars*240.0/song.bpm||until>song.bars*240.0/song.bpm+1e-9)return false;
        const auto begin=(std::int64_t)std::ceil(from*rate-1e-9),finish=until==0?song.samples(rate):std::min(song.samples(rate),(std::int64_t)std::ceil(until*rate-1e-9));
        if(from>180||until>180||begin>=finish||finish>song.samples(rate))return false;
        if(record){
            const int target=recordLane.load();const bool occupied=laneHasAudio(target);
            if(occupied&&extraClips[(size_t)target].size()>=maxClipsPerLane-1)return false;
            const auto capacity=finish-begin;
            if(currentBytes()+(std::uint64_t)capacity*8>maxAudioBytes)return false;
            std::shared_ptr<juce::AudioBuffer<float>> fresh;
            try{fresh=std::make_shared<juce::AudioBuffer<float>>(2,(int)capacity);if(occupied)extraClips[(size_t)target].reserve(extraClips[(size_t)target].size()+1);}catch(const std::bad_alloc&){return false;}
            fresh->clear();rememberEdit();pendingCapture=std::move(fresh);captureBuffer=pendingCapture.get();captureOffset=begin;captureIndex=occupied?(int)extraClips[(size_t)target].size()+1:0;capturePublished=false;trimHistory();
        }else{pendingCapture.reset();captureBuffer=nullptr;}
        drums.reset(true);click.reset();lastStep=-1;position=begin;loopStart=(repeat||until>0)?begin:0;playEnd=finish;loopEnabled=repeat;displayBar=(int)(song.stepAt(begin,rate)/16)+1;displayBeat=(int)((song.stepAt(begin,rate)/4)%4)+1;
        preRollPosition=0;preRollLength=record?idw::countInSamples(requestedCountBars.load(),rate,song.bpm):0;
        countRemaining=preRollLength;totalCountBeats=record?requestedCountBars.load()*4:0;countBeat=1;
        recording=record;playing=true;return true;
    }
    bool seekPlayback(double seconds){
        const juce::ScopedLock guard(lock);
        if(!playing||recording||!std::isfinite(seconds)||seconds<0||seconds>180)return false;
        const auto destination=(std::int64_t)std::ceil(seconds*rate-1e-9);
        // Section playback remains inside its original range, even in one-shot mode.
        if(destination<loopStart||destination>=playEnd)return false;
        position=destination;drums.reset(true);click.reset();lastStep=-1;
        displayBar=(int)(song.stepAt(destination,rate)/16)+1;displayBeat=(int)((song.stepAt(destination,rate)/4)%4)+1;
        return true;
    }
    void stop(){playing=false;recording=false;countRemaining=0;}
    // Interval sample peaks: four tracks, drum pattern, then pre-limiter output.
    // One UI consumer exchanges these values; no locks or allocation on the audio thread.
    std::array<float,6> consumeMeterPeaks(){std::array<float,6> result{};for(size_t i=0;i<result.size();++i)result[i]=meterPeaks[i].exchange(0);return result;}
    void clearTake(){removeLane(0);}
    void process(juce::AudioBuffer<float>& output){
        // UI actions never block the audio callback. UI edits are disabled in transport.
        const juce::ScopedTryLock guard(lock);if(!guard.isLocked()||!playing)return;
        std::array<float,6> blockPeaks{};const auto meter=[&](int index,float sample){if(std::isfinite(sample))blockPeaks[(size_t)index]=std::max(blockPeaks[(size_t)index],std::abs(sample));};
        auto at=position.load();const bool rec=recording.load();const int target=recordLane.load();auto* capture=captureBuffer;const auto end=rec?song.samples(rate):playEnd;
        const bool hearClick=clickEnabled.load();const float cueGain=clickGain.load();
        for(int i=0;i<output.getNumSamples();++i){
            if(preRollPosition<preRollLength){
                countBeat=(int)idw::songBeatAt(preRollPosition,rate,song.bpm)+1;
                const float cue=click.sample(preRollPosition,rate,song.bpm,cueGain);
                for(int ch=0;ch<output.getNumChannels();++ch){
                    const auto& settings=lanes[(size_t)target].settings;
                    const float liveGain=output.getNumChannels()==1?(settings.channelGain(0)+settings.channelGain(1))*.5f:settings.channelGain(ch);
                    const float live=output.getSample(ch,i)*song.trackGain*liveGain*(laneAudible(target)?1.f:0.f);meter(target,live);meter(5,live+cue);
                    output.setSample(ch,i,idw::guardedOutput(live+cue,true));
                }
                ++preRollPosition;
                if(preRollPosition==preRollLength)click.reset();
                continue;
            }
            if(at>=end&&!rec&&loopEnabled){at=loopStart;drums.reset(true);click.reset();lastStep=-1;}
            if(at>=end){for(int ch=0;ch<output.getNumChannels();++ch)output.clear(ch,i,output.getNumSamples()-i);playing=false;recording=false;break;}
            if(rec&&!capturePublished){
                idw::SongLane clip;clip.audio=pendingCapture;clip.rate=rate;clip.settings.start=captureOffset/rate;
                if(captureIndex==0){copyMix(clip.settings,lanes[(size_t)target].settings);assignPrimary(target,clip);}
                else extraClips[(size_t)target].push_back(std::move(clip)); // capacity reserved before transport starts
                pendingCapture.reset();capturePublished=true;
            }
            const auto step=song.stepAt(at,rate);if(step!=lastStep){trigger(drums,song,step);lastStep=step;displayBar=(int)(step/16)+1;displayBeat=(int)((step/4)%4)+1;}
            const float beat=drums.sample()*song.beatGain*(anySolo()?0.f:1.f);
            const float cue=click.sample(at,rate,song.bpm,cueGain)*(hearClick?1.f:0.f);
            for(int ch=0;ch<output.getNumChannels();++ch){
                float vocal=0;
                if(rec){vocal=output.getSample(ch,i);if(!std::isfinite(vocal))vocal=0;capture->setSample(ch,(int)(at-captureOffset),vocal);}
                else vocal=0;
                const bool mono=output.getNumChannels()==1;
                std::array<float,4> laneLevels{},rightLevels{};
                float backing=readMix(mono?0:ch,at/rate,rec?target:-1,rec?captureIndex:-1,-1,laneLevels.data());
                if(mono){backing=(backing+readMix(1,at/rate,rec?target:-1,rec?captureIndex:-1,-1,rightLevels.data()))*.5f;for(int lane=0;lane<4;++lane)laneLevels[(size_t)lane]=(laneLevels[(size_t)lane]+rightLevels[(size_t)lane])*.5f;}
                const float liveGain=mono?(lanes[(size_t)target].settings.channelGain(0)+lanes[(size_t)target].settings.channelGain(1))*.5f:lanes[(size_t)target].settings.channelGain(ch);
                const float mixed=backing+(rec&&laneAudible(target)?vocal*liveGain:0);
                if(rec&&laneAudible(target))laneLevels[(size_t)target]+=vocal*liveGain;
                for(int lane=0;lane<4;++lane)meter(lane,laneLevels[(size_t)lane]*song.trackGain);
                meter(4,beat);meter(5,mixed*song.trackGain+beat+cue);
                output.setSample(ch,i,idw::guardedOutput(mixed*song.trackGain+beat+cue,true));
            }
            if(rec&&output.getNumChannels()==1)capture->setSample(1,(int)(at-captureOffset),capture->getSample(0,(int)(at-captureOffset)));
            ++at;if(rec){const int captured=(int)(at-captureOffset);if(captureIndex>0)extraClips[(size_t)target][(size_t)captureIndex-1].frames=captured;else if(target==0)recorded=captured;else lanes[(size_t)target].frames=captured;}
        }
        for(size_t i=0;i<blockPeaks.size();++i){auto previous=meterPeaks[i].load();while(previous<blockPeaks[i]&&!meterPeaks[i].compare_exchange_weak(previous,blockPeaks[i])){}}
        position=at;countRemaining=playing?preRollLength-preRollPosition:0;if(rec&&at>captureOffset)++revision;
    }
    static constexpr std::uint64_t maxAudioBytes=512ull*1024*1024;
    std::array<idw::SongLane,4> audioLanes()const{const juce::ScopedLock guard(lock);auto result=lanes;result[0]=laneView(0);return result;}
    bool setLaneSettings(int lane,idw::LaneSettings settings){
        const juce::ScopedLock guard(lock);if(playing||lane<0||lane>=4||!settings.valid(laneView(lane).duration()))return false;
        if(settings==lanes[(size_t)lane].settings)return true;
        rememberEdit();lanes[(size_t)lane].settings=settings;trimHistory();++revision;return true;
    }
    bool removeLane(int lane){
        const juce::ScopedLock guard(lock);if(playing||lane<0||lane>=4)return false;
        rememberEdit();if(lane==0){track.reset();recorded=0;}lanes[(size_t)lane]={};extraClips[(size_t)lane].clear();trimHistory();++revision;return true;
    }
    static constexpr int maxClipsPerLane=64;
    std::vector<idw::SongLane> clips(int lane)const{
        const juce::ScopedLock guard(lock);if(lane<0||lane>=4)return {};
        std::vector<idw::SongLane> result;if(laneView(lane).frames>0)result.push_back(laneView(lane));
        for(auto clip:extraClips[(size_t)lane]){copyMix(clip.settings,lanes[(size_t)lane].settings);result.push_back(std::move(clip));}return result;
    }
    bool setClipSettings(int lane,int clip,idw::LaneSettings settings){
        const juce::ScopedLock guard(lock);if(playing||!validClip(lane,clip)||!settings.valid(clipView(lane,clip).duration()))return false;
        rememberEdit();auto& destination=clip==0?lanes[(size_t)lane].settings:extraClips[(size_t)lane][(size_t)clip-1].settings;
        destination.start=settings.start;destination.trimIn=settings.trimIn;destination.trimOut=settings.trimOut;destination.fadeIn=settings.fadeIn;destination.fadeOut=settings.fadeOut;
        copyMix(lanes[(size_t)lane].settings,settings);trimHistory();++revision;return true;
    }
    bool setClipMuted(int lane,int clip,bool muted){
        const juce::ScopedLock guard(lock);if(playing||!validClip(lane,clip))return false;
        auto& settings=clipSettings(lane,clip);if(settings.clipMuted==muted)return true;
        rememberEdit();settings.clipMuted=muted;trimHistory();++revision;return true;
    }
    // Direct overlap only. Muting always affects the entire clip, never source audio.
    bool selectTake(int lane,int clip,bool unmuteOverlaps=false){
        const juce::ScopedLock guard(lock);if(playing||!validClip(lane,clip))return false;
        const auto chosen=clipView(lane,clip);const double begin=chosen.settings.start,end=begin+chosen.settings.length(chosen.duration());
        std::vector<std::pair<int,bool>> changes;
        for(int i=0;i<=(int)extraClips[(size_t)lane].size();++i){const auto other=clipView(lane,i);const double length=other.settings.length(other.duration());
            const bool overlap=end>begin&&length>0&&std::max(begin,other.settings.start)<std::min(end,other.settings.start+length);
            if(i!=clip&&!overlap)continue;const bool muted=i!=clip&&!unmuteOverlaps;
            if(other.settings.clipMuted!=muted)changes.emplace_back(i,muted);
        }
        if(changes.empty())return true;rememberEdit();for(const auto& change:changes)clipSettings(lane,change.first).clipMuted=change.second;
        trimHistory();++revision;return true;
    }
    // Use the selected trimmed clip as a phrase; preserve other clips outside it.
    // Build the entire edit first so limits/fade failures cannot partially alter a song.
    bool selectPhrase(int lane,int clip){
        const juce::ScopedLock guard(lock);if(playing||!validClip(lane,clip))return false;
        const auto chosen=clipView(lane,clip);const double begin=chosen.settings.start,end=begin+chosen.settings.length(chosen.duration());
        if(end<=begin||end>180)return false;
        std::vector<idw::SongLane> planned;for(int i=0;i<=(int)extraClips[(size_t)lane].size();++i)planned.push_back(clipView(lane,i));
        const int originalCount=(int)planned.size();planned[(size_t)clip].settings.clipMuted=false;
        for(int i=0;i<originalCount;++i){if(i==clip)continue;const auto original=planned[(size_t)i];const auto& settings=original.settings;
            const double start=settings.start,finish=start+settings.length(original.duration());
            if(settings.clipMuted||std::max(start,begin)>=std::min(finish,end))continue;
            std::vector<double> cuts{start};if(begin>start&&begin<finish)cuts.push_back(begin);if(end>start&&end<finish)cuts.push_back(end);cuts.push_back(finish);
            for(size_t j=1;j+1<cuts.size();++j)if(cuts[j]-start<settings.fadeIn||finish-cuts[j]<settings.fadeOut)return false;
            for(size_t j=0;j+1<cuts.size();++j){
                if(cuts[j+1]-cuts[j]<1.0/original.rate)return false;
                auto part=original;part.settings.start=cuts[j];part.settings.trimIn=settings.trimIn+cuts[j]-start;
                part.settings.trimOut=j+2==cuts.size()?settings.trimOut:settings.trimIn+cuts[j+1]-start;
                part.settings.fadeIn=j==0?settings.fadeIn:0;part.settings.fadeOut=j+2==cuts.size()?settings.fadeOut:0;
                part.settings.clipMuted=cuts[j]<end&&cuts[j+1]>begin;
                if(!part.settings.valid(part.duration()))return false;
                if(j==0)planned[(size_t)i]=std::move(part);else planned.push_back(std::move(part));
            }
            if(planned.size()>maxClipsPerLane)return false;
        }
        const auto mixer=lanes[(size_t)lane].settings;
        std::vector<idw::SongLane> extras;extras.reserve(planned.size()-1);
        for(size_t i=1;i<planned.size();++i){clearMix(planned[i].settings);extras.push_back(std::move(planned[i]));}
        rememberEdit();copyMix(planned[0].settings,mixer);assignPrimary(lane,planned[0]);extraClips[(size_t)lane]=std::move(extras);trimHistory();++revision;return true;
    }
    bool splitClip(int lane,int clip,double timelineSeconds){
        const juce::ScopedLock guard(lock);if(playing||!validClip(lane,clip)||extraClips[(size_t)lane].size()>=maxClipsPerLane-1||(!std::isfinite(timelineSeconds)||timelineSeconds<0||timelineSeconds>180))return false;
        auto right=clipView(lane,clip);const double end=right.settings.trimOut<0?right.duration():right.settings.trimOut;
        const double split=right.settings.trimIn+timelineSeconds-right.settings.start;
        // Keep at least one source sample on either side.
        if(split-right.settings.trimIn<1/right.rate||end-split<1/right.rate||timelineSeconds-right.settings.start<right.settings.fadeIn||end-split<right.settings.fadeOut)return false;
        rememberEdit();auto& left=clip==0?lanes[(size_t)lane].settings:extraClips[(size_t)lane][(size_t)clip-1].settings;
        left.trimOut=split;left.fadeOut=0;right.settings.fadeIn=0;right.settings.start=timelineSeconds;right.settings.trimIn=split;clearMix(right.settings);
        extraClips[(size_t)lane].push_back(std::move(right));trimHistory();++revision;return true;
    }
    bool crossfadeNext(int lane,int clip){
        const juce::ScopedLock guard(lock);if(playing||!validClip(lane,clip))return false;
        const auto left=clipView(lane,clip);int next=-1;double nextStart=181;
        for(int i=0;i<=(int)extraClips[(size_t)lane].size();++i){const auto candidate=clipView(lane,i);if(candidate.settings.start>left.settings.start&&candidate.settings.start<nextStart){next=i;nextStart=candidate.settings.start;}}
        if(next<0)return false;const auto right=clipView(lane,next);
        const double leftLength=left.settings.length(left.duration()),rightLength=right.settings.length(right.duration());
        const double end=left.settings.start+leftLength,overlap=end-nextStart;
        if(overlap<=0||overlap>leftLength||overlap>rightLength)return false;
        rememberEdit();auto& a=clip==0?lanes[(size_t)lane].settings:extraClips[(size_t)lane][(size_t)clip-1].settings;
        auto& b=next==0?lanes[(size_t)lane].settings:extraClips[(size_t)lane][(size_t)next-1].settings;
        a.fadeOut=overlap;a.fadeIn=std::min(a.fadeIn,leftLength-overlap);b.fadeIn=overlap;b.fadeOut=std::min(b.fadeOut,rightLength-overlap);
        trimHistory();++revision;return true;
    }
    bool duplicateClip(int lane,int clip,double timelineSeconds){
        const juce::ScopedLock guard(lock);if(playing||!validClip(lane,clip)||extraClips[(size_t)lane].size()>=maxClipsPerLane-1)return false;
        auto next=clipView(lane,clip);next.settings.start=timelineSeconds;clearMix(next.settings);if(!next.settings.valid(next.duration()))return false;
        rememberEdit();extraClips[(size_t)lane].push_back(std::move(next));trimHistory();++revision;return true;
    }
    bool removeClip(int lane,int clip){
        const juce::ScopedLock guard(lock);if(playing||!validClip(lane,clip))return false;
        rememberEdit();auto& extras=extraClips[(size_t)lane];
        if(clip>0)extras.erase(extras.begin()+clip-1);
        else if(!extras.empty()){auto first=extras.front();extras.erase(extras.begin());copyMix(first.settings,lanes[(size_t)lane].settings);assignPrimary(lane,first);}
        else{const auto mix=lanes[(size_t)lane].settings;if(lane==0){track.reset();recorded=0;}lanes[(size_t)lane]={};copyMix(lanes[(size_t)lane].settings,mix);}
        trimHistory();++revision;return true;
    }
    juce::Result addWav(int lane,const juce::File& file,double timelineSeconds){
        const juce::ScopedLock guard(lock);if(playing||lane<0||lane>=4||!std::isfinite(timelineSeconds)||timelineSeconds<0||timelineSeconds>180)return juce::Result::fail("Stop transport and choose a position from 0 to 180 seconds.");
        if(extraClips[(size_t)lane].size()>=maxClipsPerLane-1)return juce::Result::fail("This track has reached the 64-clip limit.");
        SongStudio imported;const auto result=imported.importWav(0,file);if(result.failed())return result;
        auto next=imported.laneView(0);next.settings.start=timelineSeconds;
        if(currentBytes()+bufferBytes(next.audio)>maxAudioBytes)return juce::Result::fail("Project audio exceeds the 512 MB limit.");
        rememberEdit();if(!laneHasAudio(lane)){copyMix(next.settings,lanes[(size_t)lane].settings);assignPrimary(lane,next);}else extraClips[(size_t)lane].push_back(std::move(next));
        trimHistory();++revision;return juce::Result::ok();
    }
    bool canUndo()const{const juce::ScopedLock guard(lock);return !playing&&!undoEdits.empty();}
    bool canRedo()const{const juce::ScopedLock guard(lock);return !playing&&!redoEdits.empty();}
    bool undoTrackEdit(){const juce::ScopedLock guard(lock);if(playing||undoEdits.empty())return false;redoEdits.push_back(editState());const auto edit=undoEdits.back();undoEdits.pop_back();applyEdit(edit);trimHistory();return true;}
    bool redoTrackEdit(){const juce::ScopedLock guard(lock);if(playing||redoEdits.empty())return false;undoEdits.push_back(editState());const auto edit=redoEdits.back();redoEdits.pop_back();applyEdit(edit);trimHistory();return true;}
    juce::Result importWav(int lane,const juce::File& file){
        const juce::ScopedLock guard(lock);if(playing||lane<0||lane>=4)return juce::Result::fail("Stop transport and select an audio lane.");
        auto input=file.createInputStream();if(!input)return juce::Result::fail("Cannot open WAV. Existing clip kept.");
        juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(input.release(),true));
        if(!reader||reader->numChannels<1||reader->numChannels>2||reader->sampleRate<8000||reader->sampleRate>192000||reader->lengthInSamples<=0||reader->lengthInSamples>reader->sampleRate*180)return juce::Result::fail("Use a mono/stereo WAV, 8–192 kHz and no longer than 180 seconds.");
        if(currentBytes(lane)+(std::uint64_t)reader->lengthInSamples*8>maxAudioBytes)return juce::Result::fail("Project audio exceeds the 512 MB memory limit.");
        std::shared_ptr<juce::AudioBuffer<float>> audio;try{audio=std::make_shared<juce::AudioBuffer<float>>(2,(int)reader->lengthInSamples);}catch(const std::bad_alloc&){return juce::Result::fail("Not enough memory to import audio.");}
        if(!reader->read(audio.get(),0,audio->getNumSamples(),0,true,true))return juce::Result::fail("Could not read WAV. Existing clip kept.");
        if(reader->numChannels==1)audio->copyFrom(1,0,*audio,0,0,audio->getNumSamples());
        for(int ch=0;ch<2;++ch)for(int i=0;i<audio->getNumSamples();++i)if(!std::isfinite(audio->getSample(ch,i)))return juce::Result::fail("WAV contains invalid audio. Existing clip kept.");
        rememberEdit();lanes[(size_t)lane]={};extraClips[(size_t)lane].clear();
        if(lane==0){track=audio;recorded=audio->getNumSamples();takeRate=reader->sampleRate;}
        else{lanes[(size_t)lane].audio=audio;lanes[(size_t)lane].frames=audio->getNumSamples();lanes[(size_t)lane].rate=reader->sampleRate;}
        trimHistory();++revision;return juce::Result::ok();
    }
    juce::Result save(const juce::File& destination){
        const juce::ScopedLock guard(lock);if(playing)return juce::Result::fail("Stop playback or recording first.");
        juce::TemporaryFile temp(destination);auto out=temp.getFile().createOutputStream();if(!out)return juce::Result::fail("Cannot create project file.");
        const bool muted=hasMutedClips(),marked=!songSections.empty()||muted,faded=hasFades()||marked,multiple=hasMultipleClips()||faded,arranged=hasArrangement()||multiple;out->writeInt(0x49445731);out->writeInt(muted?7:marked?6:faded?5:multiple?4:arranged?3:2);out->writeInt(song.bpm);out->writeInt(song.bars);
        for(auto row:song.rows)out->writeInt(row);
        out->writeFloat(song.beatGain);out->writeFloat(song.trackGain);out->writeInt((int)takeRate);out->writeInt(recorded);
        const auto document=lyrics();
        for(const auto& text:{document.title,document.text}){const int bytes=(int)text.getNumBytesAsUTF8();out->writeInt(bytes);out->write(text.toRawUTF8(),(size_t)bytes);}
        for(int i=0;i<recorded;++i)for(int ch=0;ch<2;++ch)out->writeFloat(track->getSample(ch,i));
        if(arranged)for(int lane=0;lane<4;++lane){
            const auto& item=lanes[(size_t)lane];const auto& settings=item.settings;
            out->writeDouble(settings.start);out->writeDouble(settings.trimIn);out->writeDouble(settings.trimOut);out->writeFloat(settings.gain);out->writeFloat(settings.pan);out->writeInt((settings.mute?1:0)|(settings.solo?2:0));
            out->writeInt((int)item.rate);out->writeInt(lane==0?0:item.frames);
            if(lane>0)for(int i=0;i<item.frames;++i)for(int ch=0;ch<2;++ch)out->writeFloat(item.audio->getSample(ch,i));
        }
        if(multiple){
            std::vector<idw::SongLane> sources;for(int lane=0;lane<4;++lane)sources.push_back(laneView(lane));
            for(const auto& extras:extraClips){out->writeInt((int)extras.size());for(const auto& clip:extras){
                out->writeDouble(clip.settings.start);out->writeDouble(clip.settings.trimIn);out->writeDouble(clip.settings.trimOut);
                int reference=-1;for(int i=0;i<(int)sources.size();++i)if(sources[(size_t)i].audio==clip.audio){reference=i;break;}
                out->writeInt(reference);
                if(reference<0){out->writeInt((int)clip.rate);out->writeInt(clip.frames);for(int i=0;i<clip.frames;++i)for(int ch=0;ch<2;++ch)out->writeFloat(clip.audio->getSample(ch,i));sources.push_back(clip);}
            }}
        }
        if(faded)for(int lane=0;lane<4;++lane){const auto primary=laneView(lane);out->writeDouble(primary.settings.fadeIn);out->writeDouble(primary.settings.fadeOut);for(const auto& clip:extraClips[(size_t)lane]){out->writeDouble(clip.settings.fadeIn);out->writeDouble(clip.settings.fadeOut);}}
        if(marked){out->writeInt((int)songSections.size());for(const auto& section:songSections){const int bytes=(int)section.name.getNumBytesAsUTF8();out->writeInt(bytes);out->write(section.name.toRawUTF8(),(size_t)bytes);out->writeInt(section.firstBar);out->writeInt(section.lastBar);}}
        if(muted)for(int lane=0;lane<4;++lane){out->writeInt(lanes[(size_t)lane].settings.clipMuted?1:0);for(const auto& clip:extraClips[(size_t)lane])out->writeInt(clip.settings.clipMuted?1:0);}
        out->flush();const bool ok=out->getStatus().wasOk();out.reset();
        return ok&&temp.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Project save failed; previous file preserved.");
    }
    juce::Result load(const juce::File& file){
        const juce::ScopedLock guard(lock);if(playing)return juce::Result::fail("Stop transport first.");
        auto in=file.createInputStream();if(!in||in->getTotalLength()<44||in->readInt()!=0x49445731)return juce::Result::fail("Not a supported IDW song project.");
        const int version=in->readInt();if(version!=1&&version!=2&&version!=3&&version!=4&&version!=5&&version!=6&&version!=7)return juce::Result::fail("Unsupported song version. Current song kept.");
        idw::SongPattern next;next.bpm=in->readInt();next.bars=in->readInt();
        for(auto& row:next.rows){const int value=in->readInt();if(value<0||value>65535)return juce::Result::fail("Invalid drum pattern.");row=(std::uint16_t)value;}
        next.beatGain=in->readFloat();next.trackGain=in->readFloat();const int sr=in->readInt(),frames=in->readInt();
        if(!next.valid()||sr<8000||sr>192000||frames<0||frames>sr*180)return juce::Result::fail("Invalid or truncated song project.");
        Lyrics loadedWords;
        if(version>=2){
            if(!readText(*in,loadedWords.title,maxTitleBytes)||!readText(*in,loadedWords.text,maxLyricsBytes))return juce::Result::fail("Invalid or truncated lyrics. Current song kept.");
        }
        if(in->getTotalLength()>static_cast<juce::int64>(maxAudioBytes)+200000||in->getTotalLength()<in->getPosition()+(juce::int64)frames*8||(version<3&&in->getTotalLength()!=in->getPosition()+(juce::int64)frames*8))return juce::Result::fail("Invalid or truncated song audio.");
        juce::AudioBuffer<float> loaded(2,frames);
        for(int i=0;i<frames;++i)for(int ch=0;ch<2;++ch){const float value=in->readFloat();if(!std::isfinite(value))return juce::Result::fail("Invalid recorded audio.");loaded.setSample(ch,i,value);}
        std::array<idw::SongLane,4> loadedLanes{};std::uint64_t totalBytes=(std::uint64_t)frames*8;
        if(version>=3)for(int lane=0;lane<4;++lane){
            if(in->getTotalLength()-in->getPosition()<44)return juce::Result::fail("Truncated track settings. Current song kept.");
            auto& item=loadedLanes[(size_t)lane];auto& settings=item.settings;
            settings.start=in->readDouble();settings.trimIn=in->readDouble();settings.trimOut=in->readDouble();settings.gain=in->readFloat();settings.pan=in->readFloat();const int flags=in->readInt();
            const int clipRate=in->readInt(),clipFrames=in->readInt();settings.mute=(flags&1)!=0;settings.solo=(flags&2)!=0;
            if(flags<0||flags>3||clipRate<8000||clipRate>192000||clipFrames<0||clipFrames>clipRate*180||(lane==0&&clipFrames!=0)||!settings.valid(lane==0?frames/(double)sr:clipFrames/(double)clipRate))return juce::Result::fail("Invalid track settings. Current song kept.");
            totalBytes+=(std::uint64_t)clipFrames*8;
            if(totalBytes>maxAudioBytes||in->getTotalLength()-in->getPosition()<(juce::int64)clipFrames*8)return juce::Result::fail("Invalid/truncated track audio. Current song kept.");
            item.rate=clipRate;item.frames=clipFrames;
            if(clipFrames>0){item.audio=std::make_shared<juce::AudioBuffer<float>>(2,clipFrames);for(int i=0;i<clipFrames;++i)for(int ch=0;ch<2;++ch){const float value=in->readFloat();if(!std::isfinite(value))return juce::Result::fail("Invalid track samples. Current song kept.");item.audio->setSample(ch,i,value);}}
        }
        auto loadedTrack=std::make_shared<juce::AudioBuffer<float>>(std::move(loaded));
        std::array<std::vector<idw::SongLane>,4> loadedExtras;
        if(version>=4){
            std::vector<idw::SongLane> sources(loadedLanes.begin(),loadedLanes.end());sources[0].audio=loadedTrack;sources[0].frames=frames;sources[0].rate=sr;std::vector<int> owners{0,1,2,3};
            for(int lane=0;lane<4;++lane){
                if(in->getTotalLength()-in->getPosition()<4)return juce::Result::fail("Truncated clip count. Current song kept.");
                const int count=in->readInt();if(count<0||count>=maxClipsPerLane||(count>0&&sources[(size_t)lane].frames==0))return juce::Result::fail("Invalid clip count. Current song kept.");
                for(int i=0;i<count;++i){
                    if(in->getTotalLength()-in->getPosition()<28)return juce::Result::fail("Truncated clip. Current song kept.");
                    idw::LaneSettings settings;settings.start=in->readDouble();settings.trimIn=in->readDouble();settings.trimOut=in->readDouble();const int reference=in->readInt();idw::SongLane clip;
                    if(reference==-1){
                        if(in->getTotalLength()-in->getPosition()<8)return juce::Result::fail("Truncated clip source.");
                        const int clipRate=in->readInt(),clipFrames=in->readInt();
                        if(clipRate<8000||clipRate>192000||clipFrames<=0||clipFrames>clipRate*180)return juce::Result::fail("Invalid clip source.");
                        totalBytes+=(std::uint64_t)clipFrames*8;
                        if(totalBytes>maxAudioBytes||in->getTotalLength()-in->getPosition()<(juce::int64)clipFrames*8)return juce::Result::fail("Invalid or oversized clip audio.");
                        clip.rate=clipRate;clip.frames=clipFrames;clip.audio=std::make_shared<juce::AudioBuffer<float>>(2,clipFrames);
                        for(int frame=0;frame<clipFrames;++frame)for(int ch=0;ch<2;++ch){const float value=in->readFloat();if(!std::isfinite(value))return juce::Result::fail("Invalid clip samples.");clip.audio->setSample(ch,frame,value);}sources.push_back(clip);owners.push_back(lane);
                    }else{if(reference<0||reference>=(int)sources.size()||owners[(size_t)reference]!=lane||sources[(size_t)reference].frames<=0)return juce::Result::fail("Invalid clip source reference.");clip=sources[(size_t)reference];}
                    if(!settings.valid(clip.duration()))return juce::Result::fail("Invalid clip timing. Current song kept.");
                    clip.settings=settings;loadedExtras[(size_t)lane].push_back(std::move(clip));
                }
            }
        }
        if(version>=5)for(int lane=0;lane<4;++lane){for(int clip=0;clip<=(int)loadedExtras[(size_t)lane].size();++clip){
            if(in->getTotalLength()-in->getPosition()<16)return juce::Result::fail("Truncated clip fades. Current song kept.");
            auto& settings=clip==0?loadedLanes[(size_t)lane].settings:loadedExtras[(size_t)lane][(size_t)clip-1].settings;
            settings.fadeIn=in->readDouble();settings.fadeOut=in->readDouble();
            const double duration=clip==0?(lane==0?frames/(double)sr:loadedLanes[(size_t)lane].duration()):loadedExtras[(size_t)lane][(size_t)clip-1].duration();
            if(!settings.valid(duration))return juce::Result::fail("Invalid clip fades. Current song kept.");
        }}
        std::vector<Section> loadedSections;
        if(version>=6){if(in->getTotalLength()-in->getPosition()<4)return juce::Result::fail("Missing sections. Current song kept.");const int count=in->readInt();if(count<0||count>32)return juce::Result::fail("Invalid section count.");for(int i=0;i<count;++i){Section section;if(!readText(*in,section.name,256)||in->getTotalLength()-in->getPosition()<8)return juce::Result::fail("Truncated section.");section.firstBar=in->readInt();section.lastBar=in->readInt();if(!section.valid())return juce::Result::fail("Invalid section.");loadedSections.push_back(std::move(section));}}
        if(version>=7)for(int lane=0;lane<4;++lane)for(int clip=0;clip<=(int)loadedExtras[(size_t)lane].size();++clip){
            if(in->getTotalLength()-in->getPosition()<4)return juce::Result::fail("Truncated clip mute data. Current song kept.");
            const int flag=in->readInt();if(flag!=0&&flag!=1)return juce::Result::fail("Invalid clip mute flag. Current song kept.");
            auto& settings=clip==0?loadedLanes[(size_t)lane].settings:loadedExtras[(size_t)lane][(size_t)clip-1].settings;settings.clipMuted=flag!=0;
        }
        if(in->getPosition()!=in->getTotalLength())return juce::Result::fail("Unexpected project data. Current song kept.");
        {const juce::ScopedLock writingGuard(lyricsLock);words=std::move(loadedWords);documentId=juce::Uuid().toString();}
        track=std::move(loadedTrack);lanes=std::move(loadedLanes);extraClips=std::move(loadedExtras);songSections=std::move(loadedSections);loopEnabled=false;undoEdits.clear();redoEdits.clear();song=next;takeRate=sr;recorded=frames;recordLane=0;position=0;++revision;return juce::Result::ok();
    }
    struct MixCheck {
        std::array<float,6> peaks{};std::int64_t limitedFrames=0,totalFrames=0;double firstLimitSeconds=-1;
        juce::String summary()const{
            const auto db=[](float peak){return peak>0?juce::String(20*std::log10(peak),1)+" dBFS":juce::String("silent");};
            juce::String text="48 kHz sample peaks before limiting (not LUFS/true peak):\n";
            const char* names[]{"Lead","Backing","Double","Ad-lib","Drum pattern","Full mix"};for(size_t i=0;i<peaks.size();++i)text+=juce::String(names[i])+": "+db(peaks[i])+"\n";
            if(peaks[5]==0)text+="The current mix is silent. Check clips, mute/solo, gain and song length.\n";
            else if(limitedFrames>0)text+="Limiter threshold crossed in "+juce::String(100.0*limitedFrames/totalFrames,2)+"% of frames; first at "+juce::String(firstLimitSeconds,3)+" s. Lower levels and scan again.\n";
            else text+="No sample exceeded the output limiter threshold in this scan.\n";
            return text+"Current mute/solo and printed effects apply. Metronome/live input excluded. Later edits require another scan.";
        }
    };
    juce::Result checkMix(MixCheck& report,std::atomic<bool>* cancelled=nullptr,std::atomic<float>* progress=nullptr){
        const juce::ScopedLock guard(lock);if(playing)return juce::Result::fail("Stop transport first.");
        MixCheck next;next.totalFrames=song.samples(48000);idw::StudioSynth synth;synth.prepare(48000);configureDrums(synth);std::int64_t previous=-1;
        for(std::int64_t base=0;base<next.totalFrames;base+=1024){
            if(cancelled&&cancelled->load())return juce::Result::fail("Level check cancelled.");
            if(progress)progress->store((float)base/next.totalFrames);
            const int count=(int)std::min<std::int64_t>(1024,next.totalFrames-base);
            for(int i=0;i<count;++i){const auto at=base+i,step=song.stepAt(at,48000);if(step!=previous){trigger(synth,song,step);previous=step;}
                const float beat=synth.sample()*song.beatGain*(anySolo()?0.f:1.f);next.peaks[4]=std::max(next.peaks[4],std::abs(beat));bool limited=false;
                for(int ch=0;ch<2;++ch){std::array<float,4> lanes{};const float mix=readMix(ch,at/48000.0,-1,-1,-1,lanes.data())*song.trackGain+beat;
                    for(size_t lane=0;lane<lanes.size();++lane)next.peaks[lane]=std::max(next.peaks[lane],std::abs(lanes[lane]*song.trackGain));
                    next.peaks[5]=std::max(next.peaks[5],std::abs(mix));limited=limited||std::abs(mix)>.94f;
                }
                if(limited){++next.limitedFrames;if(next.firstLimitSeconds<0)next.firstLimitSeconds=at/48000.0;}
            }
        }
        if(cancelled&&cancelled->load())return juce::Result::fail("Level check cancelled.");
        report=next;if(progress)progress->store(1.f);return juce::Result::ok();
    }
    juce::Result exportStems(const juce::File& destination,std::atomic<bool>* cancelled=nullptr,std::atomic<float>* progress=nullptr){
        const juce::ScopedLock guard(lock);if(playing)return juce::Result::fail("Stop transport first.");
        if(destination.exists()||!destination.getParentDirectory().isDirectory())return juce::Result::fail("Choose a new export folder inside an existing directory.");
        const auto staging=destination.getParentDirectory().getChildFile("IDW-export-"+juce::Uuid().toString());
        if(staging.createDirectory().failed())return juce::Result::fail("Cannot create export folder.");
        const auto fail=[&](const juce::String& message){const bool removed=staging.deleteRecursively();return juce::Result::fail(message+(removed?juce::String{}:" Partial files remain in "+staging.getFullPathName()));};
        const char* names[]{"01-Lead.wav","02-Backing.wav","03-Double.wav","04-Ad-lib.wav","05-Drum-pattern.wav"};
        for(int stem=0;stem<5;++stem){const auto result=renderWav(staging.getChildFile(names[stem]),stem,cancelled,progress);if(result.failed())return fail(result.getErrorMessage());}
        const juce::String notes="IDW Voice MIDI Studio stem export\nTempo: "+juce::String(song.bpm)+" BPM\nBars: "+juce::String(song.bars)+" (4/4)\n48 kHz, 24-bit stereo; every file starts at song time zero and has the full song length.\nImport all five WAVs at the same position on separate stereo tracks. Disable automatic stretching.\nCurrent clip mute, track mute/solo, gain, pan, fades and track/beat master levels are printed. Empty or muted stems may be silent.\nNo count-in, metronome, live microphone or MIDI is included. Recorded effects are already in the source audio.\nStems are individually safety-limited. At high levels their sum may differ from the safety-limited full mix. Start DAW faders at unity and reduce levels if the sum clips.\n";
        if(!staging.getChildFile("IMPORT-INSTRUCTIONS.txt").replaceWithText(notes))return fail("Cannot write import instructions.");
        if(cancelled&&cancelled->load())return fail("Export cancelled.");
        if(destination.exists()||!staging.moveFileTo(destination))return fail("Could not finish export folder; existing files kept.");
        if(progress)progress->store(1.f);return juce::Result::ok();
    }
    juce::Result exportWav(const juce::File& file,std::atomic<bool>* cancelled=nullptr,std::atomic<float>* progress=nullptr){return renderWav(file,-1,cancelled,progress);}
private:
    juce::Result renderWav(const juce::File& file,int stem,std::atomic<bool>* cancelled,std::atomic<float>* progress){
        const juce::ScopedLock guard(lock);if(playing)return juce::Result::fail("Stop transport first.");
        juce::TemporaryFile temp(file);std::unique_ptr<juce::OutputStream> stream=temp.getFile().createOutputStream();
        if(!stream)return juce::Result::fail("Cannot create WAV file.");
        juce::WavAudioFormat format;
        auto writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        if(!writer)return juce::Result::fail("Cannot initialize WAV writer.");
        idw::StudioSynth synth;synth.prepare(48000);configureDrums(synth);juce::AudioBuffer<float> block(2,1024);
        const auto end=song.samples(48000);std::int64_t previous=-1;
        for(std::int64_t base=0;base<end;base+=1024){if(cancelled&&cancelled->load())return juce::Result::fail("Export cancelled.");if(progress)progress->store(.99f*(stem<0?(float)base/end:(stem+(float)base/end)/5.f));const int count=(int)std::min<std::int64_t>(1024,end-base);
            for(int i=0;i<count;++i){const auto step=song.stepAt(base+i,48000);if(step!=previous){trigger(synth,song,step);previous=step;}
                const float beat=synth.sample()*song.beatGain*(anySolo()|| (stem>=0&&stem<4)?0.f:1.f);
                for(int ch=0;ch<2;++ch)block.setSample(ch,i,idw::guardedOutput((stem==4?0.f:readMix(ch,(base+i)/48000.0,-1,-1,stem))*song.trackGain+beat,true));
            }
            if(!writer->writeFromAudioSampleBuffer(block,0,count))return juce::Result::fail("WAV write failed; check free disk space.");
        }
        writer.reset();if(cancelled&&cancelled->load())return juce::Result::fail("Export cancelled.");
        if(!temp.overwriteTargetFileWithTemporary())return juce::Result::fail("Could not finish WAV export.");
        if(progress&&stem<0)progress->store(1.f);return juce::Result::ok();
    }
private:
    static bool readText(juce::InputStream& input,juce::String& result,int limit){
        if(input.getTotalLength()-input.getPosition()<4)return false;
        const int bytes=input.readInt();if(bytes<0||bytes>limit||input.getTotalLength()-input.getPosition()<bytes)return false;
        juce::MemoryBlock buffer((size_t)bytes+1,true);
        if(input.read(buffer.getData(),bytes)!=bytes)return false;
        const auto* data=static_cast<const char*>(buffer.getData());
        if(std::memchr(data,0,(size_t)bytes)!=nullptr||!juce::CharPointer_UTF8::isValidString(data,bytes))return false;
        result=juce::String::fromUTF8(data,bytes);return result.length()<=limit/4;
    }
    mutable juce::CriticalSection lyricsLock;Lyrics words;juce::String documentId=juce::Uuid().toString();
    std::atomic<std::uint64_t> revision{0};
    static_assert(std::atomic<float>::is_always_lock_free,"Audio meters require lock-free float atomics");
    std::array<std::atomic<float>,6> meterPeaks{0.f,0.f,0.f,0.f,0.f,0.f};
    static void configureDrums(idw::StudioSynth& synth){idw::StudioSynth::Settings settings;settings.gain=1;settings.delay=0;synth.configure(settings);}
    static void trigger(idw::StudioSynth& synth,const idw::SongPattern& pattern,std::int64_t step){const int notes[]{36,38,42};for(int row=0;row<3;++row)if(pattern.rows[(size_t)row]&(1u<<(step%16)))synth.message(0x99,notes[row],100);}
    bool anySolo()const{for(const auto& lane:lanes)if(lane.settings.solo)return true;return false;}
    bool laneAudible(int lane)const{return !lanes[(size_t)lane].settings.mute&&(!anySolo()||lanes[(size_t)lane].settings.solo);}
    idw::SongLane laneView(int lane)const{auto result=lanes[(size_t)lane];if(lane==0){result.audio=track;result.frames=recorded;result.rate=takeRate;}return result;}
    float readMix(int channel,double time,int excluded=-1,int excludedClip=-1,int onlyLane=-1,float* contributions=nullptr)const{
        float value=0;for(int lane=0;lane<4;++lane)if((onlyLane<0||lane==onlyLane)&&laneAudible(lane)){
            // Extra clip samples have neutral gain; all clips use their track's mixer.
            for(int index=0;index<(int)extraClips[(size_t)lane].size();++index)if(lane!=excluded||index+1!=excludedClip){const float sample=extraClips[(size_t)lane][(size_t)index].sample(channel,time)*lanes[(size_t)lane].settings.channelGain(channel);value+=sample;if(contributions)contributions[lane]+=sample;}
            if(lane==excluded&&excludedClip==0)continue;
            // Avoid shared_ptr reference-count operations in the sample loop.
            const auto& settings=lanes[(size_t)lane].settings;
            const auto* audio=lane==0?track.get():lanes[(size_t)lane].audio.get();
            const int frames=lane==0?recorded:lanes[(size_t)lane].frames;const double sr=lane==0?takeRate:lanes[(size_t)lane].rate;
            const double source=time-settings.start+settings.trimIn,end=settings.trimOut<0?frames/sr:settings.trimOut;
            if(settings.clipMuted||!audio||time<settings.start||source<settings.trimIn||source>=end)continue;
            const double location=source*sr;const int index=(int)location;if(index<0||index>=frames)continue;
            const float a=audio->getSample(channel,index),b=index+1<frames?audio->getSample(channel,index+1):0;
            const float sample=(a+(b-a)*(float)(location-index))*settings.channelGain(channel)*settings.envelope(time-settings.start,settings.length(frames/sr));value+=sample;if(contributions)contributions[lane]+=sample;
        }return value;
    }
    static void copyMix(idw::LaneSettings& to,const idw::LaneSettings& from){to.gain=from.gain;to.pan=from.pan;to.mute=from.mute;to.solo=from.solo;}
    static void clearMix(idw::LaneSettings& settings){settings.gain=1;settings.pan=0;settings.mute=false;settings.solo=false;}
    bool laneHasAudio(int lane)const{return laneView(lane).frames>0||!extraClips[(size_t)lane].empty();}
    bool validClip(int lane,int clip)const{return lane>=0&&lane<4&&clip>=0&&clip<=(int)extraClips[(size_t)lane].size()&&laneView(lane).frames>0;}
    idw::SongLane clipView(int lane,int clip)const{return clip==0?laneView(lane):extraClips[(size_t)lane][(size_t)clip-1];}
    void assignPrimary(int lane,const idw::SongLane& clip){lanes[(size_t)lane]=clip;if(lane==0){track=clip.audio;recorded=clip.frames;takeRate=clip.rate;lanes[0].audio.reset();lanes[0].frames=0;}}
    idw::LaneSettings& clipSettings(int lane,int clip){return clip==0?lanes[(size_t)lane].settings:extraClips[(size_t)lane][(size_t)clip-1].settings;}
    bool hasMutedClips()const{for(int lane=0;lane<4;++lane){if(lanes[(size_t)lane].settings.clipMuted)return true;for(const auto& clip:extraClips[(size_t)lane])if(clip.settings.clipMuted)return true;}return false;}
    bool hasFades()const{for(int lane=0;lane<4;++lane){const auto& settings=lanes[(size_t)lane].settings;if(settings.fadeIn>0||settings.fadeOut>0)return true;for(const auto& clip:extraClips[(size_t)lane])if(clip.settings.fadeIn>0||clip.settings.fadeOut>0)return true;}return false;}
    bool hasMultipleClips()const{for(const auto& clips:extraClips)if(!clips.empty())return true;return false;}
    bool hasArrangement()const{for(int i=0;i<4;++i)if(!lanes[(size_t)i].settings.isDefault()||(i>0&&lanes[(size_t)i].frames>0))return true;return false;}
    static std::uint64_t bufferBytes(const std::shared_ptr<juce::AudioBuffer<float>>& audio){return audio?(std::uint64_t)audio->getNumSamples()*audio->getNumChannels()*sizeof(float):0;}
    std::uint64_t currentBytes(int excluded=-1)const{
        std::vector<const void*> seen;std::uint64_t bytes=0;const auto add=[&](const auto& audio){if(audio&&std::find(seen.begin(),seen.end(),audio.get())==seen.end()){seen.push_back(audio.get());bytes+=bufferBytes(audio);}};
        if(excluded!=0)add(track);for(int lane=0;lane<4;++lane)if(lane!=excluded){add(lanes[(size_t)lane].audio);for(const auto& clip:extraClips[(size_t)lane])add(clip.audio);}return bytes;
    }
    std::uint64_t retainedBytes()const{
        std::vector<const void*> seen;std::uint64_t bytes=0;const auto add=[&](const auto& audio){if(audio&&std::find(seen.begin(),seen.end(),audio.get())==seen.end()){seen.push_back(audio.get());bytes+=bufferBytes(audio);}};
        add(pendingCapture);add(track);for(const auto& lane:lanes)add(lane.audio);for(const auto& clips:extraClips)for(const auto& clip:clips)add(clip.audio);
        for(const auto* history:{&undoEdits,&redoEdits})for(const auto& edit:*history){add(edit.take);for(const auto& lane:edit.lanes)add(lane.audio);for(const auto& clips:edit.extras)for(const auto& clip:clips)add(clip.audio);}return bytes;
    }
    void trimHistory(){while(retainedBytes()>maxAudioBytes){if(!undoEdits.empty())undoEdits.erase(undoEdits.begin());else if(!redoEdits.empty())redoEdits.erase(redoEdits.begin());else break;}}
    struct Edit {std::shared_ptr<juce::AudioBuffer<float>> take;int frames;double sr;std::array<idw::SongLane,4> lanes;std::array<std::vector<idw::SongLane>,4> extras;std::vector<Section> sections;};
    Edit editState()const{return {track,recorded,takeRate,lanes,extraClips,songSections};}
    void applyEdit(const Edit& edit){track=edit.take;recorded=edit.frames;takeRate=edit.sr;lanes=edit.lanes;extraClips=edit.extras;songSections=edit.sections;++revision;}
    void rememberEdit(){if(!playing&&pendingCapture){pendingCapture.reset();captureBuffer=nullptr;}undoEdits.push_back(editState());redoEdits.clear();if(undoEdits.size()>8)undoEdits.erase(undoEdits.begin());}
    std::array<idw::SongLane,4> lanes{};std::array<std::vector<idw::SongLane>,4> extraClips;std::vector<Edit> undoEdits,redoEdits;
    std::shared_ptr<juce::AudioBuffer<float>> pendingCapture;juce::AudioBuffer<float>* captureBuffer=nullptr;std::int64_t captureOffset=0;int captureIndex=0;bool capturePublished=false;
    std::vector<Section> songSections;std::atomic<bool> loopEnabled{false};std::int64_t loopStart=0,playEnd=0;
    idw::SongMetronome click;
    std::atomic<int> recordLane{0};
    std::atomic<int> requestedCountBars{0},countBeat{1},totalCountBeats{0},displayBar{1},displayBeat{1};
    std::atomic<bool> clickEnabled{false};std::atomic<float> clickGain{.2f};
    std::atomic<std::int64_t> countRemaining{0};std::int64_t preRollPosition=0,preRollLength=0;
    mutable juce::CriticalSection lock;idw::SongPattern song;idw::StudioSynth drums;
    std::shared_ptr<juce::AudioBuffer<float>> track;int recorded=0;double rate=48000,takeRate=48000;std::int64_t lastStep=-1;
    std::atomic<double> displayRate{48000};
    std::atomic<bool> playing{false},recording{false};std::atomic<std::int64_t> position{0};
};
