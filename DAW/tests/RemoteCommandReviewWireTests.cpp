#include "../RemoteCommandReviewWire.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif
using namespace idw::daw::remote;
int main(int argc,char** argv) {
    if(argc==2) { // Bounded data-only codec fixture for Python interoperability.
#if defined(_WIN32)
        if(_setmode(_fileno(stdin),_O_BINARY)==-1 || _setmode(_fileno(stdout),_O_BINARY)==-1) return 2;
#endif
        const std::string mode=argv[1];
        const auto bound=mode=="--match" ? 2+maxCommandReviewBytes+maxCommandDecisionBytes : maxCommandReviewBytes;
        CommandReviewBytes bytes;char c=0;
        while(std::cin.get(c)) {
            if(bytes.size()>=bound) return 2;
            bytes.push_back(static_cast<unsigned char>(c));
        }
        if(mode=="--match") {
            if(bytes.size()<2) return 2;
            const auto size=(static_cast<std::size_t>(bytes[0])<<8)|bytes[1];
            if(size>maxCommandReviewBytes || bytes.size()<2+size) return 2;
            LocalCommandProposal proposal;
            if(!decodeCommandProposal(CommandReviewBytes(bytes.begin()+2,bytes.begin()+2+size),proposal)) return 2;
            bool allow=false;
            if(!matchCommandDecision(CommandReviewBytes(bytes.begin()+2+size,bytes.end()),proposal,allow)) return 2;
            std::cout.put(allow?'1':'0');return std::cout.good()?0:2;
        }
        LocalCommandProposal proposal;if(!decodeCommandProposal(bytes,proposal)) return 2;
        CommandReviewBytes out;
        if(mode=="--proposal") {if(!encodeCommandProposal(proposal,out)) return 2;}
        else if(mode=="--allow" || mode=="--deny") {if(!encodeCommandDecision(proposal,mode=="--allow",out)) return 2;}
        else return 2;
        std::cout.write(reinterpret_cast<const char*>(out.data()),static_cast<std::streamsize>(out.size()));
        return std::cout.good()?0:2;
    }
    if(argc!=1) return 2;
    LocalCommandProposal proposal{7,std::string(32,'c'),"guest",{45,std::string(64,'a'),std::string(64,'b'),13},
        "1|45|1|review|arm|1"},decoded;
    CommandReviewBytes bytes;assert(encodeCommandProposal(proposal,bytes));
    assert(bytes.size()==195+proposal.wire.size() && bytes[4]==1 && bytes[12]==7 && bytes[45]==5);
    assert(decodeCommandProposal(bytes,decoded) && decoded.wire==proposal.wire && decoded.reviewOwner==proposal.reviewOwner);
    const auto original=bytes;
    for(std::size_t size=0;size<bytes.size();++size) {
        CommandReviewBytes shortRecord(bytes.begin(),bytes.begin()+size);
        assert(!decodeCommandProposal(shortRecord,decoded) && decoded.generation==7);
    }
    bytes.push_back(0);assert(!decodeCommandProposal(bytes,decoded));bytes=original;
    for(const auto offset:{0,4,13,45,51,193,194}) {
        auto bad=bytes;bad[static_cast<std::size_t>(offset)]=255;assert(!decodeCommandProposal(bad,decoded));
    }
    for(const bool allowed:{false,true}) {
        CommandReviewBytes decision;assert(encodeCommandDecision(proposal,allowed,decision));
        bool result=!allowed;assert(matchCommandDecision(decision,proposal,result) && result==allowed);
        for(std::size_t i=0;i<decision.size();++i) {
            if(i==5) continue; // Both explicit decisions are valid data.
            auto changed=decision;changed[i]^=1;result=!allowed;
            assert(!matchCommandDecision(changed,proposal,result) && result==!allowed);
        }
        auto bad=decision;bad[5]=2;assert(!matchCommandDecision(bad,proposal,result));
        bad=decision;bad.push_back(0);assert(!matchCommandDecision(bad,proposal,result));
    }
    for(const auto wire:{"1|045|1|review|arm|1","1|45|01|review|arm|1","1|45|1|review|record|1",
                         "1|45|1|review|arm|128","1|46|1|review|arm|1"}) {
        auto bad=proposal;bad.wire=wire;bytes={42};assert(!encodeCommandProposal(bad,bytes) && bytes==CommandReviewBytes{42});
    }
    proposal.generation=~std::uint64_t{0};proposal.offer.epoch=~std::uint64_t{0};proposal.peerId=std::string(64,'p');
    proposal.wire="1|18446744073709551615|18446744073709551615|"+std::string(64,'r')+"|select|127";
    assert(encodeCommandProposal(proposal,bytes) && bytes.size()<=maxCommandReviewBytes);
    assert(decodeCommandProposal(bytes,decoded) && decoded.generation==proposal.generation);
}
