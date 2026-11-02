#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
namespace idw::daw {
// Machine-local JUCE device state, separate from projects and recovery copies.
// Call on the message thread, never from the audio callback.
class AudioDevicePreferences {
public:
    enum class Result {unchanged,saved,failed};
    static constexpr juce::int64 maximumBytes=64*1024;
    explicit AudioDevicePreferences(juce::File location):file(std::move(location)){}
    std::unique_ptr<juce::XmlElement> load() const {return read(file);}
    Result save(const juce::XmlElement* state) const {
        return saveWithWriter(state,[](const juce::File& target,const juce::String& text){
            auto output=target.createOutputStream();if(!output)return false;
            const bool wrote=output->write(text.toRawUTF8(),(size_t)text.getNumBytesAsUTF8());
            output->flush();return wrote && output->getStatus().wasOk();
        });
    }
    // Internal write seam for deterministic short-write/failure regressions.
    template<class Writer> Result saveWithWriter(const juce::XmlElement* state,Writer&& writer) const {try{
        // JUCE returns null when the user has not explicitly chosen a setup.
        // Do not erase a previous preference or save an incidental default.
        if(!state)return Result::unchanged;
        if(file==juce::File{} || file.isDirectory() || !valid(*state))return Result::failed;
        const auto text=state->toString();const auto count=(size_t)text.getNumBytesAsUTF8();
        if(count==0 || count>(size_t)maximumBytes)return Result::failed;
        if(file.existsAsFile() && file.getSize()==(juce::int64)count && file.loadFileAsString()==text)return Result::unchanged;
        if(!file.getParentDirectory().createDirectory().wasOk())return Result::failed;
        juce::TemporaryFile staged(file);
        if(!writer(staged.getFile(),text) || staged.getFile().getSize()!=(juce::int64)count)return Result::failed;
        juce::MemoryBlock actual;
        if(!staged.getFile().loadFileAsData(actual) || actual.getSize()!=count ||
           std::memcmp(actual.getData(),text.toRawUTF8(),count)!=0)return Result::failed;
        const auto checked=read(staged.getFile());
        if(!checked || !state->isEquivalentTo(checked.get(),true) || !staged.overwriteTargetFileWithTemporary())return Result::failed;
        return Result::saved;
    }catch(...){return Result::failed;}}
    const juce::File& location() const {return file;}
private:
    juce::File file;
    static bool numeric(const juce::String& text,double minimum,double maximum,bool whole){
        const auto raw=text.trim().toStdString();size_t used=0;
        if(raw.empty() || raw.find_first_not_of("0123456789.+-eE")!=std::string::npos)return false;
        // JUCE reads buffer size as an integer, not scientific/fractional text.
        if(whole && raw.find_first_not_of("0123456789")!=std::string::npos)return false;
        try{const double value=std::stod(raw,&used);
            return used==raw.size() && std::isfinite(value) && value>=minimum && value<=maximum && (!whole || std::floor(value)==value);
        }catch(...){return false;}
    }
    static bool valid(const juce::XmlElement& state){
        if(!state.hasTagName("DEVICESETUP") || state.getStringAttribute("deviceType").trim().isEmpty() || state.getNumAttributes()>32)return false;
        for(int i=0;i<state.getNumAttributes();++i)if(state.getAttributeValue(i).length()>4096)return false;
        if(state.hasAttribute("audioDeviceRate") && !numeric(state.getStringAttribute("audioDeviceRate"),1,1000000,false))return false;
        if(state.hasAttribute("audioDeviceBufferSize") && !numeric(state.getStringAttribute("audioDeviceBufferSize"),1,1048576,true))return false;
        for(const char* name:{"audioDeviceInChans","audioDeviceOutChans"})if(state.hasAttribute(name)){
            const auto mask=state.getStringAttribute(name);
            if(mask.length()>256 || mask.removeCharacters("01").isNotEmpty())return false;
        }
        int children=0;
        for(const auto* child=state.getFirstChildElement();child;child=child->getNextElement()){
            if(++children>128 || !child->hasTagName("MIDIINPUT") || child->getFirstChildElement()!=nullptr || child->getNumAttributes()>8)return false;
            for(int i=0;i<child->getNumAttributes();++i)if(child->getAttributeValue(i).length()>4096)return false;
        }
        return true;
    }
    static std::unique_ptr<juce::XmlElement> read(const juce::File& source){
        if(!source.existsAsFile() || source.getSize()<=0 || source.getSize()>maximumBytes)return {};
        const auto text=source.loadFileAsString();
        // JUCE's XML parser supports DTDs. These are never part of device state.
        // Reject them before parsing to prevent entity/file resolution.
        if(text.containsIgnoreCase("<!DOCTYPE") || text.containsIgnoreCase("<!ENTITY"))return {};
        const auto raw=text.toStdString();if(std::count(raw.begin(),raw.end(),'<')>256)return {};
        auto result=juce::XmlDocument::parse(text);
        if(!result || !valid(*result))return {};
        return result;
    }
};
}
