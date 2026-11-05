#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cstring>
#include <vector>

namespace idw::daw {
// Machine-local filename metadata only. Reading this list never opens a project.
class RecentProjects {
public:
    static constexpr size_t capacity=8;
    static constexpr juce::int64 maximumBytes=256*1024;
    explicit RecentProjects(juce::File location):file(std::move(location)){}
    const std::vector<juce::File>& entries() const {return projects;}
    bool load(){
        std::vector<juce::File> next;
        if(!read(file,next))return false;
        projects=std::move(next);return true;
    }
    // Call only after a successful manual save or ordinary saved-project open.
    // Preference failure must never undo the already-successful project operation.
    bool remember(const juce::File& project){
        if(!validPath(project.getFullPathName()))return false;
        auto next=projects;
        next.erase(std::remove(next.begin(),next.end(),project),next.end());
        next.insert(next.begin(),project);if(next.size()>capacity)next.resize(capacity);
        projects=std::move(next);return save();
    }
    template<class Writer> bool saveWithWriter(Writer&& writer) const {try{
        if(file==juce::File{} || file.isDirectory())return false;
        const auto text=encode(projects);
        const auto bytes=static_cast<size_t>(text.getNumBytesAsUTF8());
        if(bytes>static_cast<size_t>(maximumBytes) || !file.getParentDirectory().createDirectory().wasOk())return false;
        juce::TemporaryFile staged(file);
        if(!writer(staged.getFile(),text) || staged.getFile().getSize()!=static_cast<juce::int64>(bytes))return false;
        juce::MemoryBlock actual;
        if(!staged.getFile().loadFileAsData(actual) || actual.getSize()!=bytes || std::memcmp(actual.getData(),text.toRawUTF8(),bytes)!=0)return false;
        std::vector<juce::File> checked;
        return read(staged.getFile(),checked) && checked==projects && staged.overwriteTargetFileWithTemporary();
    }catch(...){return false;}}
private:
    juce::File file;
    std::vector<juce::File> projects;
    static bool validPath(const juce::String& path){
        if(path.isEmpty() || path.length()>4096 || !juce::File::isAbsolutePath(path))return false;
        for(auto c:path)if(c<32 || (c>=127 && c<=159) || c==0x2028 || c==0x2029)return false;
        return true;
    }
    static juce::String encode(const std::vector<juce::File>& entries){
        auto data=std::make_unique<juce::DynamicObject>();data->setProperty("version",1);
        juce::Array<juce::var> paths;for(const auto& entry:entries)paths.add(entry.getFullPathName());
        data->setProperty("paths",paths);return juce::JSON::toString(juce::var(data.release()),true);
    }
    static bool read(const juce::File& source,std::vector<juce::File>& output){
        if(!source.existsAsFile() || source.getSize()<=0 || source.getSize()>maximumBytes)return false;
        juce::FileInputStream stream(source);if(!stream.openedOk())return false;
        juce::MemoryBlock bytes;
        stream.readIntoMemoryBlock(bytes,static_cast<int>(maximumBytes+1));
        if(bytes.getSize()>static_cast<size_t>(maximumBytes))return false;
        const auto text=juce::String::fromUTF8(static_cast<const char*>(bytes.getData()),static_cast<int>(bytes.getSize()));
        int depth=0;bool quoted=false,escaped=false;
        for(auto c:text){
            if(quoted){
                if(escaped){escaped=false;continue;}
                if(c=='\\'){escaped=true;continue;}
                if(c=='"')quoted=false;
                continue;
            }
            if(c=='"'){quoted=true;continue;}
            if((c=='{' || c=='[') && ++depth>2)return false;
            if((c=='}' || c==']') && --depth<0)return false;
        }
        if(quoted || depth!=0)return false;
        const auto value=juce::JSON::parse(text);
        auto* root=value.getDynamicObject();if(!root)return false;
        const auto version=root->getProperty("version");const auto paths=root->getProperty("paths");
        if(!version.isInt() || (int)version!=1 || !paths.isArray() || paths.getArray()->size()>static_cast<int>(capacity))return false;
        std::vector<juce::File> next;
        for(const auto& path:*paths.getArray()){
            if(!path.isString() || !validPath(path.toString()))return false;
            juce::File entry(path.toString());
            if(std::find(next.begin(),next.end(),entry)!=next.end())return false;
            next.push_back(entry);
        }
        output=std::move(next);return true;
    }
    bool save() const {
        return saveWithWriter([](const juce::File& target,const juce::String& text){
            auto output=target.createOutputStream();if(!output)return false;
            const bool ok=output->write(text.toRawUTF8(),static_cast<size_t>(text.getNumBytesAsUTF8()));
            output->flush();return ok && output->getStatus().wasOk();
        });
    }
};
}
