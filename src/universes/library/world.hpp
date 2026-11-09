#pragma once
// LIBRARY: the library's dimensions (meters): the study, then the long room it opens into, its back wall at ZBACK.
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

// The room: its half width, its height; the collection's bookcases: the first one's x (either side of the
// fireplace), how many along the back wall on each side.
constexpr float HALL_X = 10.4f;
constexpr float ROOM_H = 4.6f;
constexpr float FIRST_BAY_X = 2.9f;
constexpr int BACK_BAYS = 7;

inline vec3 sideN(int k) { const float a = k * (3.14159265f / 3.f); return {std::sin(a), 0.f, -std::cos(a)}; }
inline vec3 sideT(int k) { const float a = k * (3.14159265f / 3.f); return {std::cos(a), 0.f, std::sin(a)}; }
// The corner between side k and side k + 1 (outward direction, unit).
inline vec3 cornerDir(int k) { const float a = (k + 0.5f) * (3.14159265f / 3.f); return {std::sin(a), 0.f, -std::cos(a)}; }

} // namespace lib_world
