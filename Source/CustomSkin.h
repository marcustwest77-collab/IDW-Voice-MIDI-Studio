#pragma once
#include "SkinTheme.h"
#include <string>

namespace idw {
inline constexpr std::array<const char*,5> skinColourIds{{"skinBackground","skinCard","skinAccent","skinText","skinSignal"}};
inline constexpr std::array<const char*,5> skinColourNames{{"Background","Panels","Accent","Text","Signal"}};
inline constexpr std::array<std::uint32_t,5> skinDefaults{{0x0b0e14,0x141a24,0xeac36c,0x9aabc2,0x68dfbd}};
inline bool parseSkinHex(std::string text,std::uint32_t& result){
    if(!text.empty() && text.front()=='#')text.erase(0,1);
    if(text.size()!=6)return false;
    std::uint32_t value=0;
    for(char c:text){
        int digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
        if(digit<0)return false;
        value=(value<<4)|static_cast<std::uint32_t>(digit);
    }
    result=value;return true;
}
inline SkinPalette customSkin(SkinPalette base,const std::array<std::uint32_t,5>& colours){
    base.name="Custom";
    base.background=0xff000000u|colours[0];base.card=0xff000000u|colours[1];
    base.accent=0xff000000u|colours[2];base.muted=0xff000000u|colours[3];base.highlight=0xff000000u|colours[4];
    return base;
}
}
