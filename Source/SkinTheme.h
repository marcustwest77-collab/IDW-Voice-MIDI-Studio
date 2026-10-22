#pragma once
#include <array>
#include <cstdint>

namespace idw {
struct SkinPalette {
    const char* name;
    std::uint32_t background, card, accent, muted, highlight;
    std::uint32_t button, buttonOn, danger, grid, safety;
};

inline constexpr std::array<SkinPalette, 6> skins{{
    {"IDW Gold",       0xff0b0e14,0xff141a24,0xffeac36c,0xff9aabc2,0xff68dfbd,0xff263144,0xff76602d,0xff863f45,0xff263144,0xffffd77c},
    {"Midnight Studio",0xff070a12,0xff101726,0xff8fa8d8,0xff94a3b8,0xff67e8f9,0xff1d2940,0xff334f78,0xffa83f52,0xff25324a,0xfff8cf72},
    {"Electric Blue",  0xff050b18,0xff0b1730,0xff2f8cff,0xff91b8e8,0xff39f5d2,0xff142b52,0xff175faf,0xffb93b55,0xff163764,0xffffd166},
    {"Vocal Heat",     0xff140908,0xff251210,0xffff8a3d,0xffd6aaa0,0xffffcf5a,0xff472018,0xff8f3d20,0xffb8323f,0xff5d271d,0xffffdd75},
    {"Platinum",       0xff101214,0xff1c2024,0xffd5d9de,0xffaab1b9,0xffb8f1ff,0xff30363d,0xff58616b,0xffa4434d,0xff414951,0xffffd978},
    {"High Contrast",  0xff000000,0xff111111,0xffffffff,0xffe6e6e6,0xff00ffbd,0xff242424,0xff005f4b,0xffff4d4d,0xff666666,0xffffff00}
}};

inline constexpr int skinCount() { return static_cast<int>(skins.size()); }
inline constexpr int validSkinIndex(int index) { return index < 0 ? 0 : index >= skinCount() ? skinCount() - 1 : index; }
inline constexpr const SkinPalette& skinByIndex(int index) { return skins[static_cast<std::size_t>(validSkinIndex(index))]; }
}
