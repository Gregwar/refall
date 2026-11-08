#pragma once
// LIBRARY: the library's dimensions (meters) and its stages (lines). The study's back wall is one side of a
// hexagon 40 m across: the rotunda the study turns out to stand in, and later one level of an endless shaft of
// such hexagons. Side k of a hexagon has the outward normal (sin 60k, 0, -cos 60k): side 0 is the back wall (-z).
#include <cmath>

#include "mathutil.hpp"

namespace lib_world {

constexpr float R = 20.f;               // the hexagon's side (its circumradius)
constexpr float A = 17.3205f;           // its apothem
constexpr float ZBACK = -3.f;           // the back wall (side 0, the study's)
constexpr float ZC = ZBACK + A;         // the hexagon's center
constexpr float HL = 12.4f;             // a level's height
constexpr float CASE_D = 0.45f;         // a bookcase's depth
constexpr float TIER_H = 3.7f;          // a tier of bookcases
constexpr float GAL_IN = A - CASE_D - 1.8f; // the galleries' inner edge (apothem)
constexpr float HOLE = 12.f;             // the shaft's opening (apothem)
constexpr float BAY_W = 1.f;            // a bookcase
constexpr int BAY_SHELVES = 8;
inline float tierY(int t) { return t == 0 ? 0.f : t == 1 ? 4.1f : 8.2f; }

// The stages, in lines.
constexpr float S_ROOM = 5.f;       // the study's walls fall away: a long room of bookcases
constexpr float S_GALLERY = 14.f;   // a gallery, a second and a third storey, ladders, a spiral stair
constexpr float S_ROTUNDA = 26.f;   // the hexagon closes around it: the reading room under its dome
constexpr float S_SHAFT = 42.f;     // the floor opens: galleries above and below, the abyss
constexpr float S_HONEY = 65.f;     // the camera leaves the shaft: a section through the honeycomb
constexpr float S_ENDLESS = 100.f;  // farther and farther

inline vec3 sideN(int k) { const float a = k * (3.14159265f / 3.f); return {std::sin(a), 0.f, -std::cos(a)}; }
inline vec3 sideT(int k) { const float a = k * (3.14159265f / 3.f); return {std::cos(a), 0.f, std::sin(a)}; }
// The corner between side k and side k + 1 (outward direction, unit).
inline vec3 cornerDir(int k) { const float a = (k + 0.5f) * (3.14159265f / 3.f); return {std::sin(a), 0.f, -std::cos(a)}; }

} // namespace lib_world
