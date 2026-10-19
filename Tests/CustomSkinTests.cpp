#include "CustomSkin.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    std::uint32_t colour=123;
    check(idw::parseSkinHex("#eAc36C",colour)&&colour==0xeac36c,"Mixed-case hex parse failed");
    check(idw::parseSkinHex("000000",colour)&&colour==0,"Black parse failed");
    check(idw::parseSkinHex("FFFFFF",colour)&&colour==0xffffff,"White parse failed");
    for(const auto* invalid:{"","#","fff","#1234567","12345Z","11223344"," 123456","-12345"}){
        colour=123;check(!idw::parseSkinHex(invalid,colour),"Invalid colour accepted");check(colour==123,"Invalid colour changed the output");
    }
    const auto base=idw::skins[2];const auto custom=idw::customSkin(base,{{0,0x123456,0xffffff,0xabcdef,0x123abc}});
    check(custom.background==0xff000000&&custom.card==0xff123456&&custom.accent==0xffffffff&&custom.muted==0xffabcdef&&custom.highlight==0xff123abc,"Custom colour mapping failed");
    check(custom.danger==base.danger&&custom.safety==base.safety,"Safety colours changed");
    check(idw::skins[2].background==base.background,"Built-in skin mutated");
    std::cout<<"PASS custom skin hex validation / mapping / built-in preservation\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
