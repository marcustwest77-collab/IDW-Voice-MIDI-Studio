#include "SkinTheme.h"
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace { void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); } }

int main() {
    try {
        check(idw::skinCount() == 6, "Expected six built-in skins");
        check(&idw::skinByIndex(-1) == &idw::skins.front(), "Negative index was not clamped");
        check(&idw::skinByIndex(99) == &idw::skins.back(), "Large index was not clamped");
        std::set<std::string> names;
        for (const auto& skin : idw::skins) {
            check(names.insert(skin.name).second, "Duplicate skin name");
            for (auto colour : {skin.background,skin.card,skin.accent,skin.muted,skin.highlight,skin.button,skin.buttonOn,skin.danger,skin.grid,skin.safety})
                check((colour >> 24) == 0xff, "Skin colour must be opaque ARGB");
            check(skin.background != skin.card && skin.accent != skin.background, "Skin has no visible hierarchy");
        }
        check(idw::skins.back().accent == 0xffffffff && idw::skins.back().background == 0xff000000, "High Contrast palette changed");
        std::cout << "PASS V10.6 six-skin palette and bounds\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
}
