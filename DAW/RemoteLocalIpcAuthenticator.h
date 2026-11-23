#pragma once
// Token verification for the opt-in localhost IPC listener. No network I/O here.
// Frame payload: 64 lowercase hex secret, LF, canonical versioned track command.
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace idw::daw::remote {
class LocalIpcAuthenticator {
public:
    LocalIpcAuthenticator(std::string secret, std::uint64_t epoch)
        : secret_(std::move(secret)), epoch_(epoch) {}
    ~LocalIpcAuthenticator() {
        volatile char* p = secret_.empty() ? nullptr : &secret_[0];
        for (std::size_t i=0; p && i<secret_.size(); ++i) p[i]=0;
    }
    LocalIpcAuthenticator(const LocalIpcAuthenticator&)=delete;
    LocalIpcAuthenticator& operator=(const LocalIpcAuthenticator&)=delete;
    static bool validSecret(const std::string& s) {
        if (s.size()!=64) return false;
        for (char c:s)
            if (!((c>='0' && c<='9') || (c>='a' && c<='f'))) return false;
        return true;
    }
    bool valid() const {return epoch_!=0 && validSecret(secret_);}
    bool accept(const std::string& payload, std::string& wire) {
        if (!valid() || payload.size()<67 || payload.size()>4096 ||
            payload[64]!='\n') return false;
        // Compare the entire fixed-length token, without short-circuiting.
        unsigned char diff=0;
        for (std::size_t i=0;i<64;++i)
            diff|=static_cast<unsigned char>(payload[i]^secret_[i]);
        if (diff) return false;
        const std::string candidate=payload.substr(65);
        std::array<std::string,6> parts;
        std::size_t begin=0;
        for (std::size_t i=0;i<6;++i) {
            const auto end=candidate.find('|',begin);
            if ((i<5 && end==std::string::npos) ||
                (i==5 && end!=std::string::npos)) return false;
            parts[i]=candidate.substr(begin,end==std::string::npos ? end : end-begin);
            begin=end==std::string::npos ? candidate.size() : end+1;
        }
        if (parts[0]!="1" || parts[3].empty() || parts[3].size()>128 ||
            (parts[4]!="select" && parts[4]!="arm")) return false;
        for (char c:parts[3])
            if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                  (c>='0'&&c<='9')||c=='_'||c=='-')) return false;
        std::uint64_t receivedEpoch=0, sequence=0, track=0;
        if (!decimal(parts[1],receivedEpoch) ||
            !decimal(parts[2],sequence) ||
            !decimal(parts[5],track) ||
            receivedEpoch!=epoch_ || sequence<=lastSequence_ ||
            track>=128) return false;
        lastSequence_=sequence;
        wire=candidate;
        return true;
    }
private:
    static bool decimal(const std::string& s,std::uint64_t& value) {
        if (s.empty() || (s.size()>1 && s[0]=='0')) return false;
        std::uint64_t n=0;
        for(char c:s) {
            if(c<'0'||c>'9') return false;
            const auto d=static_cast<unsigned>(c-'0');
            if(n>(std::numeric_limits<std::uint64_t>::max()-d)/10)
                return false;
            n=n*10+d;
        }
        value=n;
        return true;
    }
    std::string secret_;
    std::uint64_t epoch_;
    std::uint64_t lastSequence_=0;
};
} // namespace idw::daw::remote
