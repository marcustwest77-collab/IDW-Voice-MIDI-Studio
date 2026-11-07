#pragma once
#include <cmath>
#include <optional>
namespace idw::daw {
inline bool validPianoRollSnap(int divisions){return divisions==1 || divisions==2 || divisions==3 || divisions==4 || divisions==6 || divisions==8;}
// Signed deltas snap symmetrically; exact half-grid ties round away from zero.
inline std::optional<double> snapPianoRollBeats(double beats,int divisions){
    if(!validPianoRollSnap(divisions) || !std::isfinite(beats))return {};
    const double scaled=beats*divisions;
    if(!std::isfinite(scaled) || std::abs(scaled)>4503599627370495.0)return {};
    return std::round(scaled)/divisions;
}
}
