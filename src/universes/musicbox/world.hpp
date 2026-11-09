#pragma once
// MUSIC BOX: the box's dimensions (centimeters) and its mechanism's layout (see musicbox.cpp).
#include <cstdlib>

#include "mathutil.hpp"

namespace mb_world {

// ---- The box (centimeters; the table at y = 0, the box centered on x = 0, its front toward +z).
constexpr float BX = 31.f, BZ = 15.f, BH = 13.f, WT = 1.2f; // half width, half depth, height, walls
constexpr float FLOOR = 3.f;                                 // the velvet floor
constexpr float BED = 3.8f;                                  // the bedplate's top
constexpr float CY = 8.6f, CZ = -2.5f, RC = 3.f;               // the cylinder's axis, radius
constexpr float PITCH = 3.f;                                 // a track (a tooth, a column)
constexpr float CYL_HALF = 15.4f;                            // the cylinder's half length
constexpr float TIP_Z = CZ + RC + 0.32f;                          // the teeth's tips
constexpr float COMB_BACK = CZ + 11.6f;                           // the comb's back edge
constexpr float PIN_R = 0.12f, PIN_L = 0.24f, HEAD_R = 0.23f;
constexpr float TUNE_DX = 0.62f;                             // the tunes' spacing along the axis
inline const vec3 STAGE(0.f, FLOOR, CZ - 7.6f);                         // the dancer's trap
constexpr float STAGE_R = 2.7f;
constexpr float DANCER_S = 1.5f;                             // the dancer's scale (about 10 cm tall)
inline const vec3 HINGE(0.f, BH, -BZ);                              // the lid's hinge
constexpr float LID_L = 2.f * BZ, LID_T = 1.4f;
constexpr float LID_OPEN = 1.92f;                            // radians from closed (leaning back a little)
inline const vec3 LAMP(-52.f, 78.f, 34.f);                          // the lamp (out of the frame, top left)
// The great wheel (left end), the spring barrel behind it, the governor's train (right end).
constexpr float GW_X = -16.6f, GW_R = 3.5f, BAR_R = 3.6f, BAR_GR = 3.82f;
inline const vec3 BAR_C(-22.f, CY - 0.6f, CZ - 7.3f);
constexpr float GR_R = 2.4f, PIN_GR = 1.15f, WH2_R = 2.7f;

inline float trackX(int i) { return (i - (10 - 1) * 0.5f) * PITCH; }
inline float tuneDx(int t) { return (t - (4 - 1) * 0.5f) * TUNE_DX; }
// The teeth's length (one plate: their roots on a straight line).
inline float toothLen(int i) { (void)i; return 4.6f; }

// Test hooks: ZEN_MUSICBOX_LINES=N pins N rows already (random cells: screenshots), ZEN_GROW_RATE=R clears R rows a
// second (each one like a real clear: ten cells fly from the bottom of the well to the cylinder).
inline int startLinesHook() {
    static const int n = std::getenv("ZEN_MUSICBOX_LINES") ? std::atoi(std::getenv("ZEN_MUSICBOX_LINES")) : 0;
    return n;
}
inline int rateLinesHook(double t) {
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return (int)(rate * t);
}

} // namespace mb_world
