// LIBRARY: building the atrium (see library.cpp). It is all there from the start, built once per variant: the
// shelving bays on every floor (empty until the books come), the galleries round three sides of the void, the glazed
// slot behind the well, the skylight. The camera only frames the bays in use; each floor's lights come on as it
// starts to fill.
#include <algorithm>
#include <cmath>

#include "universes/library/library.hpp"
#include "universes/library/world.hpp"

using namespace lib_world;

namespace {
const vec3 UP(0.f, 1.f, 0.f);
} // namespace

void LibraryUniverse::put(Mesh m, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat, float p0,
                          float p1, float p2, float seed) {
    Inst in;
    in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = (float)mat;
    in.x[0] = ax.x; in.x[1] = ax.y; in.x[2] = ax.z; in.x[3] = p0;
    in.y[0] = ay.x; in.y[1] = ay.y; in.y[2] = ay.z; in.y[3] = p1;
    in.z[0] = az.x; in.z[1] = az.y; in.z[2] = az.z; in.z[3] = p2;
    in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = seed;
    in.e[0] = -100.f; in.e[1] = -1.f; in.e[2] = stLevel_; in.e[3] = 0.f;
    if (mat == M_GLASS && !putTo_) glass_.push_back(in);
    else (putTo_ ? putTo_[m] : stat_[m]).push_back(in);
}

void LibraryUniverse::lamp(const vec3& p, const vec3& col, float r, float k, float range, int level) {
    Lamp l;
    l.p = p;
    l.col = col;
    l.r = r;
    l.k = k;
    l.range = range;
    l.level = level;
    lamps_.push_back(l);
}

void LibraryUniverse::buildWorld() {
    for (auto& v : stat_) v.clear();
    glass_.clear();
    lamps_.clear();
    const Palette& P = pal_;
    const float top = LEVELS * LEVEL_H;
    const float roomD = ROOM_FRONT - ZBACK;
    const float zMid = ZBACK + roomD * 0.5f;
    const float zg = ZBACK + CASE_D + GALLERY_D; // the back gallery's edge
    stLevel_ = 0.f;
    // an oriented box: center, axes (unit), half sizes
    auto obox = [&](const vec3& c, const vec3& u, const vec3& v, const vec3& w, const vec3& h, const vec3& col, int mat, float p0 = 0.f,
                    float p1 = 0.f, float p2 = 0.f, float seed = 0.f) { put(MESH_BOX, c, u * h.x, v * h.y, w * h.z, col, mat, p0, p1, p2, seed); };

    // ---- the shell: the floor, the back wall, the side walls, the skylight over the void
    box(vec3(0.f, -0.15f, zMid), vec3(HALL_HALF + 0.3f, 0.15f, roomD * 0.5f), P.floor, M_FLOOR, P.floorKind);
    box(vec3(0.f, top * 0.5f, ZBACK - 0.15f), vec3(HALL_HALF + 0.3f, top * 0.5f + 0.4f, 0.15f), P.wall, M_PLASTER);
    for (int s = -1; s <= 1; s += 2)
        box(vec3(s * (HALL_HALF + 0.15f), top * 0.5f, zMid), vec3(0.15f, top * 0.5f + 0.4f, roomD * 0.5f), P.wall, M_PLASTER);
    box(vec3(0.f, top + 0.25f, zMid), vec3(HALL_HALF + 0.3f, 0.25f, roomD * 0.5f), P.slab, M_SKY);
    // its beams (their shadows fall in a grid down the void)
    for (float x = -HALL_HALF + 1.2f; x < HALL_HALF - 0.5f; x += 2.4f)
        box(vec3(x, top + 0.1f, zMid), vec3(0.09f, 0.18f, roomD * 0.5f), P.slab, M_CONCRETE);
    for (float z = ZBACK + 1.2f; z < ROOM_FRONT; z += 2.4f) box(vec3(0.f, top + 0.1f, z), vec3(HALL_HALF, 0.18f, 0.09f), P.slab, M_CONCRETE);

    // ---- the glazed slot behind the well, between two deep white reveals
    put(MESH_BOX, vec3(0.f, top * 0.5f, ZBACK + 0.02f), vec3(SLOT_HALF, 0, 0), vec3(0, top * 0.5f, 0), vec3(0, 0, 0.02f), P.steel, M_WINDOW,
        0.f);
    for (int s = -1; s <= 1; s += 2)
        box(vec3(s * (SLOT_HALF - 0.04f), top * 0.5f, ZBACK + CASE_D * 0.5f + 0.01f), vec3(0.06f, top * 0.5f, CASE_D * 0.5f + 0.01f), P.slab,
            M_CONCRETE);

    // ---- the reading tables down the void: chunky painted tops on trestles, benches, mushroom lamps, a little bush
    for (int s = -1; s <= 1; s += 2) {
        const float x = s * 2.15f, z0 = 1.2f, z1 = 4.6f, zc = (z0 + z1) * 0.5f, hz = (z1 - z0) * 0.5f;
        box(vec3(x, 0.72f, zc), vec3(0.47f, 0.045f, hz), P.oak, M_WOOD, 0.f, 0.f, 0.f, 3.f + s);
        for (int e = -1; e <= 1; e += 2) box(vec3(x, 0.36f, zc + e * (hz - 0.35f)), vec3(0.4f, 0.36f, 0.025f), P.slab, M_CONCRETE);
        for (int b = -1; b <= 1; b += 2) {
            const float xb = x + b * 0.72f;
            box(vec3(xb, 0.43f, zc), vec3(0.17f, 0.04f, hz - 0.15f), P.oak, M_WOOD, 0.f, 0.f, 0.f, 5.f + b);
            for (int e = -1; e <= 1; e += 2) box(vec3(xb, 0.21f, zc + e * (hz - 0.5f)), vec3(0.15f, 0.21f, 0.02f), P.slab, M_CONCRETE);
        }
        for (int l = 0; l < 2; l++) {
            // a chunky mushroom lamp: a stem, a round shade, its bulb glowing under it
            const vec3 lp(x, 0.765f, z0 + 1.2f + l * 1.6f);
            cyl(lp + UP * 0.03f, 0.07f, 0.03f, P.steel, M_STEEL);
            cyl(lp + UP * 0.2f, 0.018f, 0.16f, P.steel, M_STEEL);
            sphere(lp + UP * 0.34f, 0.06f, P.led, 2.2f);
            put(MESH_DOME, lp + UP * 0.34f, vec3(0.17f, 0, 0), vec3(0, 0.13f, 0), vec3(0, 0, 0.17f), P.pot, M_TOY);
            lamp(lp + UP * 0.3f, P.led, 0.08f, 0.6f, 1.6f, 0);
        }
    }
    // ---- potted plants: round toon bushes in terracotta pots, at the tables' ends and on the galleries
    auto plant = [&](const vec3& base, float k, float seed) {
        cyl(base + UP * (0.17f * k), 0.2f * k, 0.17f * k, P.pot, M_TOY);
        cyl(base + UP * (0.35f * k), 0.23f * k, 0.035f * k, P.pot * 1.1f, M_TOY);
        const float a = seed * 2.4f;
        const vec3 offs[3] = {vec3(0.f, 0.68f, 0.f), vec3(std::cos(a) * 0.17f, 0.56f, std::sin(a) * 0.17f),
                              vec3(-std::cos(a + 0.8f) * 0.16f, 0.54f, -std::sin(a + 0.8f) * 0.16f)};
        const float rad[3] = {0.27f, 0.21f, 0.19f};
        for (int j = 0; j < 3; j++) sphere(base + offs[j] * k, rad[j] * k, P.leaf * (j == 0 ? 1.f : 0.9f), 0.f);
    };
    for (int s = -1; s <= 1; s += 2) {
        plant(vec3(s * 2.15f, 0.765f, 3.2f), 0.42f, 1.f + s); // a little one on the table, between its lamps
    }
    // ---- every floor: its bays, its galleries (above the ground floor), its lights
    for (int k = 0; k < LEVELS; k++) {
        stLevel_ = (float)k;
        const vec3 lift(0.f, k * LEVEL_H, 0.f);
        for (int i = 0; i < BAYS_LEVEL; i++) {
            const BayFrame b = bayFrame(i);
            const vec3 foot = b.foot + lift;
            const vec3 mid = foot - b.out * (CASE_D * 0.5f);
            auto ob = [&](const vec3& c, const vec3& h, const vec3& col, int mat, float p0 = 0.f, float p1 = 0.f, float p2 = 0.f, float sd = 0.f) {
                obox(c, b.along, UP, b.out, h, col, mat, p0, p1, p2, sd);
            };
            const float sd = 0.37f * i + 1.3f * k;
            // the back panel (washed by the strips under each shelf), the uprights, the plinth, the boards, the top
            ob(foot - b.out * (CASE_D - 0.012f) + UP * (CASE_H * 0.5f), vec3(BAY_W * 0.5f, CASE_H * 0.5f, 0.012f), P.bay, M_CASE, SHELF_PITCH,
               SHELF_Y0, 0.f, sd);
            for (int s = -1; s <= 1; s += 2)
                ob(mid + b.along * (s * (BAY_W * 0.5f - UPRIGHT)) + UP * (CASE_H * 0.5f), vec3(UPRIGHT, CASE_H * 0.5f, CASE_D * 0.5f), P.oak, M_WOOD,
                   0.f, 0.f, 0.f, sd + s);
            const float innerH = BAY_W * 0.5f - 2.f * UPRIGHT;
            ob(mid + UP * (SHELF_Y0 * 0.5f) - b.out * 0.01f, vec3(innerH, SHELF_Y0 * 0.5f, CASE_D * 0.5f - 0.01f), P.oak * 0.8f, M_WOOD);
            for (int j = 1; j <= SHELVES; j++) {
                const float yt = SHELF_Y0 + j * SHELF_PITCH;
                ob(mid + UP * (yt - BOARD * 0.5f), vec3(innerH, BOARD * 0.5f, CASE_D * 0.5f - 0.004f), P.oak, M_WOOD, 0.f, 0.f, 0.f, sd + 0.31f * j);
                if (j < SHELVES) // the light strip under the board's front edge
                    ob(foot - b.out * 0.035f + UP * (yt - BOARD - 0.004f), vec3(innerH - 0.01f, 0.0035f, 0.008f), P.led, M_EMIT, 1.6f, 1.f);
            }
            ob(mid + UP * (CASE_H - 0.03f), vec3(BAY_W * 0.5f, 0.03f, CASE_D * 0.5f), P.oak, M_WOOD);
        }
        if (k > 0) {
            // the galleries: white slabs round three sides (bridging the slot), a steel edge, glass with an oak rail
            const float y0 = k * LEVEL_H;
            box(vec3(0.f, y0 - SLAB * 0.5f, (ZBACK + zg) * 0.5f), vec3(HALL_HALF, SLAB * 0.5f, (zg - ZBACK) * 0.5f), P.slab, M_CONCRETE, 1.f);
            for (int s = -1; s <= 1; s += 2) {
                const float xc = s * (INNER + HALL_HALF) * 0.5f;
                box(vec3(xc, y0 - SLAB * 0.5f, (zg + ROOM_FRONT) * 0.5f), vec3((HALL_HALF - INNER) * 0.5f, SLAB * 0.5f, (ROOM_FRONT - zg) * 0.5f),
                    P.slab, M_CONCRETE, 1.f);
                // the side galleries' balustrade, along z
                const float xe = s * (INNER + 0.03f);
                box(vec3(xe, y0 + 0.55f, (zg + ROOM_FRONT) * 0.5f), vec3(0.008f, 0.53f, (ROOM_FRONT - zg) * 0.5f), P.skyB, M_GLASS);
                box(vec3(xe, y0 + 1.1f, (zg + ROOM_FRONT) * 0.5f), vec3(0.03f, 0.022f, (ROOM_FRONT - zg) * 0.5f), P.oak, M_WOOD);
                box(vec3(s * (INNER + 0.025f), y0 + 0.012f, (zg + ROOM_FRONT) * 0.5f), vec3(0.025f, 0.012f, (ROOM_FRONT - zg) * 0.5f), P.steel, M_STEEL);
                box(vec3(xc, y0 + 0.003f, (zg + ROOM_FRONT) * 0.5f), vec3((HALL_HALF - INNER) * 0.5f - 0.05f, 0.003f, (ROOM_FRONT - zg) * 0.5f),
                    P.floor, M_FLOOR, P.floorKind);
            }
            // the back gallery's balustrade, along x
            box(vec3(0.f, y0 + 0.012f, zg - 0.025f), vec3(INNER + 0.05f, 0.012f, 0.025f), P.steel, M_STEEL);
            box(vec3(0.f, y0 + 0.55f, zg - 0.03f), vec3(INNER + 0.03f, 0.53f, 0.008f), P.skyB, M_GLASS);
            box(vec3(0.f, y0 + 1.1f, zg - 0.03f), vec3(INNER + 0.06f, 0.022f, 0.03f), P.oak, M_WOOD);
            box(vec3(0.f, y0 + 0.003f, (ZBACK + zg) * 0.5f + 0.2f), vec3(HALL_HALF - 0.05f, 0.003f, (zg - ZBACK) * 0.5f - 0.2f), P.floor, M_FLOOR,
                P.floorKind);
            for (int s = -1; s <= 1; s += 2) plant(vec3(s * (INNER + 0.5f), y0, zg + 0.45f + 0.6f * (k % 2)), 0.9f, (float)(k * 3 + s));
        }
        // the floor's lights: round globes hanging under the slab above, in front of each bay
        const float yl = (k + 1) * LEVEL_H - SLAB - 0.01f;
        for (int i = 0; i < BAYS_LEVEL; i++) {
            const BayFrame b = bayFrame(i);
            const vec3 p = vec3(b.foot.x, yl, b.foot.z) + b.out * 0.7f;
            if (k == LEVELS - 1) continue; // (the top floor: under the skylight)
            // a round globe on a short cord, glowing warm
            cyl(p - UP * 0.08f, 0.006f, 0.08f, P.steel, M_STEEL);
            sphere(p - UP * 0.24f, 0.1f, P.led, 1.5f);
            lamp(p - UP * 0.24f, P.led, 0.1f, 0.5f, 4.2f, k);
        }
    }
    stLevel_ = 0.f;
    worldDirty_ = false;
}
