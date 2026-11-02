#include "AudioDevicePreferences.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace idw::daw;
using Result=AudioDevicePreferences::Result;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MemoryBlock bytes(const juce::File& file){juce::MemoryBlock b;require(file.loadFileAsData(b),"read bytes");return b;}
static bool write(const juce::File& file,const juce::String& text){
    auto output=file.createOutputStream();if(!output)return false;
    const bool ok=output->write(text.toRawUTF8(),(size_t)text.getNumBytesAsUTF8());output->flush();return ok && output->getStatus().wasOk();
}
int main(){try{
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-audio-preferences","",false);
    require(root.createDirectory().wasOk(),"fixture directory");const auto file=root.getChildFile("settings/AudioDeviceSettings.xml");
    AudioDevicePreferences prefs(file);
    require(!prefs.load() && prefs.save(nullptr)==Result::unchanged && !file.exists(),"default startup created settings");
    // Exact JUCE createStateXml format; no drivers are enumerated or opened.
    juce::XmlElement chosen("DEVICESETUP");chosen.setAttribute("deviceType","ASIO");
    chosen.setAttribute("audioInputDeviceName",u8"AudioBox USB 96 & microphone \u03b2");
    chosen.setAttribute("audioOutputDeviceName","AudioBox USB 96");
    chosen.setAttribute("audioDeviceRate",48000.0);chosen.setAttribute("audioDeviceBufferSize",256);
    chosen.setAttribute("audioDeviceInChans","11");chosen.setAttribute("audioDeviceOutChans","11");
    auto* midi=chosen.createNewChildElement("MIDIINPUT");midi->setAttribute("name","Keyboard");midi->setAttribute("identifier","local-test-id");
    const auto originalXml=chosen.toString();
    require(prefs.save(&chosen)==Result::saved,"save chosen setup");
    auto loaded=prefs.load();require(loaded && loaded->isEquivalentTo(&chosen,true),"setup recall differs");
    require(loaded->getStringAttribute("deviceType")=="ASIO" && loaded->getIntAttribute("audioDeviceBufferSize")==256 &&
            loaded->getDoubleAttribute("audioDeviceRate")==48000 && loaded->getStringAttribute("audioDeviceInChans")=="11", "driver/rate/buffer/channel recall");
    require(chosen.toString()==originalXml,"save mutated supplied state");
    bool invoked=false;
    require(prefs.saveWithWriter(&chosen,[&](const juce::File&,const juce::String&){invoked=true;return false;})==Result::unchanged && !invoked,"unchanged setup rewritten");
    const auto saved=bytes(file);
    require(prefs.save(nullptr)==Result::unchanged && bytes(file)==saved,"incidental default erased chosen setup");
    juce::XmlElement changed(chosen);changed.setAttribute("audioDeviceRate",44100.0);changed.setAttribute("audioDeviceBufferSize",128);
    changed.setAttribute("audioDeviceInChans","10");
    // Deterministically reproduce failed/partial/wrong writes against a prior file.
    require(prefs.saveWithWriter(&changed,[](const juce::File&,const juce::String&){return false;})==Result::failed && bytes(file)==saved,"failed write damaged preferences");
    require(prefs.saveWithWriter(&changed,[](const juce::File& f,const juce::String& text){return write(f,text.substring(0,10));})==Result::failed && bytes(file)==saved,"partial write replaced preferences");
    require(prefs.saveWithWriter(&changed,[](const juce::File& f,const juce::String& text){
        auto wrong=text.toStdString();wrong.back()='!';return write(f,juce::String::fromUTF8(wrong.data(),(int)wrong.size()));
    })==Result::failed && bytes(file)==saved,"same-size wrong content replaced preferences");
    require(prefs.saveWithWriter(&changed,[](const juce::File&,const juce::String&)->bool{throw std::runtime_error("simulated write exception");})==Result::failed && bytes(file)==saved,"exception damaged preferences");
    require(file.getParentDirectory().findChildFiles(juce::File::findFiles,false).size()==1,"staged file leak");
    require(prefs.save(&changed)==Result::saved,"updated setup save");
    AudioDevicePreferences restarted(file);loaded=restarted.load();
    require(loaded && loaded->isEquivalentTo(&changed,true) && loaded->getStringAttribute("audioDeviceInChans")=="10","restart lost updated setup");
    const auto updated=bytes(file);
    const auto rejectState=[&](const juce::XmlElement& bad){
        require(prefs.save(&bad)==Result::failed && bytes(file)==updated,"invalid state overwrote chosen setup");};
    juce::XmlElement wrongRoot("PROJECT");rejectState(wrongRoot);
    juce::XmlElement missing(changed);missing.removeAttribute("deviceType");rejectState(missing);
    for(const char* rate:{"0","-1","1000001","nan","inf","48000Hz","0x1p8","1e309"}){
        juce::XmlElement bad(changed);bad.setAttribute("audioDeviceRate",rate);rejectState(bad);
    }
    for(const char* buffer:{"0","-1","128.5","128.0","1e3","+256","1048577","256samples"}){
        juce::XmlElement bad(changed);bad.setAttribute("audioDeviceBufferSize",buffer);rejectState(bad);
    }
    for(const auto& mask:std::vector<juce::String>{"102","11x","-1",juce::String::repeatedString("1",257)}){
        juce::XmlElement bad(changed);bad.setAttribute("audioDeviceInChans",mask);rejectState(bad);
    }
    juce::XmlElement nested(changed);nested.getFirstChildElement()->createNewChildElement("NESTED");rejectState(nested);
    juce::XmlElement oversized(changed);oversized.setAttribute("audioOutputDeviceName",juce::String::repeatedString("x",70000));rejectState(oversized);
    // Empty masks are a legitimate explicit choice of no active inputs.
    juce::XmlElement noInput(changed);noInput.setAttribute("audioDeviceInChans","");
    require(prefs.save(&noInput)==Result::saved && prefs.load()->getStringAttribute("audioDeviceInChans").isEmpty(),"disabled input mask rejected");
    const auto malformed=root.getChildFile("malformed.xml");AudioDevicePreferences invalid(malformed);
    for(const auto& text:std::vector<juce::String>{"","not XML","<DEVICESETUP>","<PROJECT />",
        "<!DOCTYPE DEVICESETUP SYSTEM 'file:///must-not-be-read'><DEVICESETUP deviceType='ASIO'/>",
        "<!DOCTYPE DEVICESETUP [<!ENTITY x 'expanded'>]><DEVICESETUP deviceType='&x;'/>",
        "<DEVICESETUP deviceType='ASIO' audioDeviceBufferSize='0'/>",
        juce::String::repeatedString("<N>",300)+juce::String::repeatedString("</N>",300),
        juce::String::repeatedString("x",(int)AudioDevicePreferences::maximumBytes+1)}){
        require(malformed.replaceWithText(text) && !invalid.load(),"malformed preferences accepted");
    }
    // Startup ignoring a bad file does not delete or rewrite it.
    const auto malformedBytes=bytes(malformed);require(!invalid.load() && bytes(malformed)==malformedBytes,"bad file mutated by load");
    const auto directory=root.getChildFile("directory");require(directory.createDirectory().wasOk(),"directory fixture");
    require(AudioDevicePreferences(directory).save(&chosen)==Result::failed,"directory destination accepted");
    const auto blocker=root.getChildFile("blocker");require(blocker.replaceWithText("keep"),"blocked parent fixture");
    require(AudioDevicePreferences(blocker.getChildFile("prefs.xml")).save(&chosen)==Result::failed && blocker.loadFileAsString()=="keep","blocked parent altered");
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS audio-device preference recall, null/default preservation, exact ASIO state, updates, partial/wrong/failed writes, invalid XML/numbers/masks, entity limits and failure protection; no drivers opened\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
