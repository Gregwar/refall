#pragma once
// LIBRARY: the room's dimensions (meters). A small, cosy library room seen as a diorama: a thick floor slab, a short
// back wall (behind the well) and two long walls angled toward us, all cut in section at the top and the front; two
// big bookcases on the angled walls, a round window on each, a reading nook at the front left, a big plant at the
// front right. As the books come: a ladder, piles on the floor, then a mezzanine
// along both angled walls with its own bookcases, and two more bookcases at the back.
#include <cmath>

#include "mathutil.hpp"

namespace lib_world {

constexpr float BACK_Z = -4.5f, BACK_HALF = 3.4f;  // the back wall's inner face (half its width)
constexpr float FRONT_X = 9.4f, FRONT_Z = 0.8f;    // where the angled walls end, cut, toward us
constexpr float WALL_T = 0.5f, WALL_H = 6.6f;
constexpr float BASE_FRONT = 3.4f, BASE_T = 0.7f;  // the diorama's floor slab (its front edge, its thickness)
constexpr float CASE_D = 0.62f;                     // a bookcase's depth
constexpr float CASE_T0 = 0.5f, CASE_T1 = 0.95f;  // the main bookcases along their walls (0 back corner .. 1 front)
constexpr float SHELF_Y0 = 0.18f;                   // the bottom shelf's top
constexpr float SHELF_PITCH = 0.95f;
constexpr float BOARD = 0.08f;                      // a shelf board's thickness (chunky)
constexpr int MAIN_SHELVES = 3;
constexpr float MAIN_H = SHELF_Y0 + MAIN_SHELVES * SHELF_PITCH + 0.1f;
constexpr float MEZZ_Y = 4.2f, MEZZ_D = 1.5f, MEZZ_T0 = 0.3f;
constexpr int MEZZ_SHELVES = 2;
constexpr float MEZZ_CASE_H = SHELF_Y0 + MEZZ_SHELVES * SHELF_PITCH + 0.1f;

// Things that appear as the collection grows (each grows in from the floor when it is first needed).
enum Feature { F_LADDER = 0, F_PILES = 1, F_LOW = 2, F_MEZZ = 3, F_COUNT = 4 };

// A point on an angled wall's inner face at height 0: side +1 right, -1 left; t 0 at the back corner .. 1 at its
// front end. `along` runs from the back corner toward us, `out` points into the room.
struct WallFrame { vec3 p, along, out; float len; };
inline WallFrame wallAt(int side, float t) {
    const vec3 a(side * BACK_HALF, 0.f, BACK_Z), b(side * FRONT_X, 0.f, FRONT_Z);
    const vec3 d = b - a;
    const float L = std::sqrt(d.x * d.x + d.z * d.z);
    WallFrame w;
    w.along = d * (1.f / L);
    w.out = vec3(-w.along.z, 0.f, w.along.x) * (float)side;
    w.p = a + d * t;
    w.len = L;
    return w;
}

} // namespace lib_world
