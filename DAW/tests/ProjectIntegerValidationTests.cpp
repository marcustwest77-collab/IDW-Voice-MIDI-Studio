#include "SessionRecovery.h"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace idw::daw;
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static juce::DynamicObject* location(juce::var& data,const juce::String& field) {
    if(field=="version" || field=="numerator" || field=="denominator")return data.getDynamicObject();
    auto* tracks=data.getDynamicObject()->getProperty("tracks").getArray();
    if(field=="inputChannel")return tracks->getReference(0).getDynamicObject();
    if(field=="synthPreset")return tracks->getReference(1).getDynamicObject();
    if(field=="pitch" || field=="velocity")
        return tracks->getReference(1).getDynamicObject()->getProperty("notes").getArray()->getReference(0).getDynamicObject();
    return tracks->getReference(0).getDynamicObject()->getProperty("clips").getArray()->getReference(0).getDynamicObject();
}
static juce::String json(const Session& session) {return juce::JSON::toString(sessionToJson(session));}
int main() {
    try {
        const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("idw-project-integers","",false);
        require(root.createDirectory().wasOk(),"fixture directory");
        Session original;original.setTempo(93.5);original.addTrack(TrackType::audio);
        require(original.addClip(0,{root.getChildFile("missing.wav").getFullPathName().toStdString(),1,4,0.25}),"clip");
        original.tracks[0].inputChannel=3;
        original.addTrack(TrackType::midi);require(original.addMidiNote(1,{64,2,1,95}),"note");
        const auto before=json(original);
        const auto file=root.getChildFile("corrupt.idwproj"),origin=root.getChildFile("Original.idwproj");
        const auto reject=[&](const juce::String& field,const juce::var& value) {
            auto data=sessionToJson(original);location(data,field)->setProperty(field,value);
            auto live=original;
            if(sessionFromJson(data,live) || json(live)!=before)
                throw std::runtime_error("coerced/out-of-range integer accepted or changed live session: "+field.toStdString());
            require(file.replaceWithText(juce::JSON::toString(data,true)),"corrupt fixture write");
            require(!loadSession(file,live) && json(live)==before,"disk load changed live session");
            data.getDynamicObject()->setProperty("recoveryVersion",1);
            data.getDynamicObject()->setProperty("recoveryProject",origin.getFullPathName());
            require(file.replaceWithText(juce::JSON::toString(data,true)),"corrupt recovery write");
            auto unchangedOrigin=root.getChildFile("Keep.idwproj");const auto keep=unchangedOrigin;
            require(!SessionRecovery::read(file,live,unchangedOrigin) && json(live)==before && unchangedOrigin==keep,
                    "corrupt integer recovery changed session/origin");
        };
        struct Field {const char* name;int low,high;};
        const Field fields[]{{"version",1,1},{"numerator",1,32},{"denominator",1,32},
            {"inputChannel",0,7},{"synthPreset",0,4},{"pitch",0,127},{"velocity",1,127},
            {"fadeInCurve",0,1},{"fadeOutCurve",0,1}};
        for(const auto& field:fields) {
            for(const auto& value:{juce::var(true),juce::var(false),juce::var("1"),
                                  juce::var(1.0),juce::var(1.5),juce::var(field.low-1),juce::var(field.high+1),
                                  juce::var(std::numeric_limits<juce::int64>::max()),
                                  juce::var(std::numeric_limits<juce::int64>::min()),
                                  juce::var((juce::int64{1}<<32)+field.low),
                                  juce::var(-(juce::int64{1}<<32)+field.low)})reject(field.name,value);
            // Optional legacy properties may be omitted, but explicit coercions must fail.
            if(juce::String(field.name)=="version" || juce::String(field.name)=="numerator" ||
               juce::String(field.name)=="denominator" || juce::String(field.name)=="pitch" ||
               juce::String(field.name)=="velocity")reject(field.name,juce::var{});
            for(int boundary:{field.low,field.high})for(bool wide:{false,true}) {
                auto valid=sessionToJson(original);
                location(valid,field.name)->setProperty(field.name,wide?juce::var(static_cast<juce::int64>(boundary)):juce::var(boundary));
                Session loaded;
                require(sessionFromJson(valid,loaded),"valid in-memory integer boundary rejected");
                require(sessionFromJson(juce::JSON::parse(juce::JSON::toString(valid)),loaded),"valid integer boundary rejected");
                auto saved=sessionToJson(loaded);
                require(static_cast<juce::int64>(location(saved,field.name)->getProperty(field.name))==boundary,
                        "valid integer boundary changed");
            }
        }
        for(const auto* optional:{"inputChannel","synthPreset","fadeInCurve","fadeOutCurve"}) {
            auto legacy=sessionToJson(original);location(legacy,optional)->removeProperty(optional);
            Session loaded;require(sessionFromJson(legacy,loaded),"legacy omitted property rejected");
        }
        require(root.deleteRecursively(),"fixture cleanup");
        std::cout << "PASS: strict bounded project integers; disk/recovery rejection preserves live state; legacy omissions supported\n";
        return 0;
    } catch(const std::exception& error) {std::cerr << "FAIL: " << error.what() << '\n';return 1;}
}
