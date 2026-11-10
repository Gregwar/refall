#pragma once
// LIBRARY: the atrium's dimensions (meters). A hall open toward us: shelving bays on its back wall either side of a
// tall glazed slot (behind the well) and down both side walls, on LEVELS floors; the upper floors are galleries
// running round three sides with glass balustrades over the void; a skylight over it all.
#include <cmath>

#include "mathutil.hpp"

namespace lib_world {

constexpr float ZBACK = -3.f;           // the back wall
constexpr float LEVEL_H = 3.6f;         // a floor's height (slab to slab)
constexpr int LEVELS = 4;
constexpr float SLAB = 0.32f;           // a gallery slab's thickness
constexpr float GALLERY_D = 1.5f;       // how far a gallery runs out from the shelves
constexpr float BAY_W = 1.2f;           // a shelving bay
constexpr int BACK_BAYS = 3;            // bays on each side of the slot, on the back wall
constexpr int SIDE_BAYS = 5;            // bays down each side wall (with BACK_BAYS: some 100 lines fill all four floors)
constexpr float SLOT_HALF = 1.35f;      // the glazed slot behind the well (half width)
constexpr float HALL_HALF = SLOT_HALF + BACK_BAYS * BAY_W; // the side walls
constexpr float ROOM_FRONT = 24.f;
constexpr float CASE_D = 0.4f;          // a bay's depth
constexpr float UPRIGHT = 0.03f;        // an upright's half thickness (each bay has its own two)
constexpr int SHELVES = 8;
constexpr float SHELF_Y0 = 0.12f;       // the first shelf's top (the plinth)
constexpr float SHELF_PITCH = 0.32f;
constexpr float BOARD = 0.024f;         // a shelf board's thickness
constexpr float CASE_H = SHELF_Y0 + SHELVES * SHELF_PITCH + 0.06f;
constexpr int SLOTS = 4;                // stacks per shelf
constexpr float SLOT_W = (BAY_W - 4.f * UPRIGHT) / SLOTS;
constexpr float BOOK_K = 0.255f;        // a cell's stack -> meters on the shelves
constexpr int BAYS_LEVEL = 2 * (BACK_BAYS + SIDE_BAYS);
constexpr int SLOTS_BAY = SHELVES * SLOTS;
constexpr int SLOTS_LEVEL = BAYS_LEVEL * SLOTS_BAY;
constexpr int SLOTS_ALL = SLOTS_LEVEL * LEVELS;
constexpr float SIDE_Z0 = ZBACK + CASE_D;  // where the side walls' shelving starts
constexpr float INNER = HALL_HALF - CASE_D - GALLERY_D; // the void's half width (the galleries' edge)

// A bay of a floor, in the order they fill: the back wall (right of the slot, left, outward), then down the side
// walls (right, left, from the back toward us). Its foot at the middle of its front edge, `along` left to right as
// seen from the room, `out` toward the room.
struct BayFrame { vec3 foot, along, out; bool side; };
inline BayFrame bayFrame(int i) {
    BayFrame b;
    if (i < 2 * BACK_BAYS) {
        const int side = i % 2 == 0 ? 1 : -1, k = i / 2;
        b.foot = vec3(side * (SLOT_HALF + (k + 0.5f) * BAY_W), 0.f, ZBACK + CASE_D);
        b.along = vec3(1.f, 0.f, 0.f);
        b.out = vec3(0.f, 0.f, 1.f);
        b.side = false;
    } else {
        const int j = i - 2 * BACK_BAYS, side = j % 2 == 0 ? 1 : -1, k = j / 2;
        b.foot = vec3(side * (HALL_HALF - CASE_D), 0.f, SIDE_Z0 + (k + 0.5f) * BAY_W);
        b.along = vec3(0.f, 0.f, (float)side);
        b.out = vec3((float)-side, 0.f, 0.f);
        b.side = true;
    }
    return b;
}

} // namespace lib_world
