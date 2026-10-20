#pragma once
#include "../Source/SkinTheme.h"
#include <algorithm>
namespace idw::daw {
struct FactorySkin {
    const char* name;
    std::uint32_t background,panel,accent,button,header,rowOdd,rowEven;
};
// Keep the first four IDs stable for existing preferences and exported skins.
inline constexpr std::array<FactorySkin,4> neoSkins{{
    {"Neo Cyan",0xff080f1c,0xff101f32,0xff15d4ff,0xff152638,0xff122338,0xff16283d,0xff102033},
    {"Royal Gold",0xff17100b,0xff2b2118,0xffffc857,0xff292014,0xff352a1e,0xff33291e,0xff282016},
    {"Emerald Circuit",0xff091c18,0xff112d26,0xff73f9ae,0xff102b25,0xff173e32,0xff173b2f,0xff112c24},
    {"Purple Night",0xff160d22,0xff261d39,0xffd4a6ff,0xff20182f,0xff302340,0xff302342,0xff261d36}
}};
inline constexpr int factorySkinCount(){return (int)neoSkins.size()+idw::skinCount();}
inline constexpr int validFactorySkinIndex(int index){return std::clamp(index,0,factorySkinCount()-1);}
inline constexpr FactorySkin factorySkin(int index){
    index=validFactorySkinIndex(index);
    if(index<(int)neoSkins.size())return neoSkins[(std::size_t)index];
    const auto& old=idw::skinByIndex(index-(int)neoSkins.size());
    return {old.name,old.background,old.card,old.accent,old.button,old.grid,old.card,old.background};
}
}
