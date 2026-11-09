// LIBRARY: building the library, stage by stage (see library.cpp). Everything the library will ever be is built
// once per game and per variant, each part stamped with the lines at which it appears (and goes): the vertex shader
// pops it up, raises it out of the floor or brings it down from above as the growth passes its stamp.
#include <algorithm>
#include <cmath>

#include "universes/library/library.hpp"
#include "universes/library/world.hpp"

using namespace lib_world;

namespace {
const vec3 UP(0.f, 1.f, 0.f);
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }
} // namespace

void LibraryUniverse::put(Mesh m, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat, float p0,
                          float p1, float p2, float seed) {
    Inst in;
    in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = (float)mat;
    in.x[0] = ax.x; in.x[1] = ax.y; in.x[2] = ax.z; in.x[3] = p0;
    in.y[0] = ay.x; in.y[1] = ay.y; in.y[2] = ay.z; in.y[3] = p1;
    in.z[0] = az.x; in.z[1] = az.y; in.z[2] = az.z; in.z[3] = p2;
    in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = seed;
    in.e[0] = st_.born; in.e[1] = st_.die; in.e[2] = st_.unit; in.e[3] = st_.style;
    (putTo_ ? putTo_[m] : stat_[m]).push_back(in);
}

void LibraryUniverse::lamp(const vec3& p, const vec3& col, float r, float k, float range, float flick) {
    Lamp l;
    l.p = p;
    l.col = col;
    l.r = r;
    l.k = k;
    l.range = range;
    l.born = st_.born;
    l.die = st_.die;
    l.flick = flick;
    lamps_.push_back(l);
}

int LibraryUniverse::unit(const vec3& c) {
    Unit u;
    u.c = c;
    u.born = st_.born;
    units_.push_back(u);
    return (int)units_.size() - 1;
}

namespace {
// A box along a hexagon's side k: its center, half sizes (along, up, depth); its +z face toward the room.
struct SideFrame { vec3 t, n, in; };
SideFrame frameOf(int k) { return {sideT(k), sideN(k), sideN(k) * -1.f}; }
} // namespace

// A real bookcase: its foot at the middle of its front, `out` toward the room. Filler: its shelves hold books drawn
// procedurally (a sort unit); else it is one of the collection's (its shelves are added to the collection).
void LibraryUniverse::buildBookcase(const vec3& foot, const vec3& along, const vec3& out, float w, float h, int shelves,
                                    const vec3& wood, bool filler, int fillerIdx, float seed) {
    (void)fillerIdx;
    const vec3 back = foot - out * CASE_D;
    const vec3 mid = foot - out * (CASE_D * 0.5f);
    const vec3 dark = wood * 0.55f;
    auto ob = [&](const vec3& c, const vec3& half, const vec3& col, int mat, float p0 = 0.f, float p1 = 0.f, float p2 = 0.f) {
        obox(c, along, UP, out, half, col, mat, p0, p1, p2, seed);
    };
    // sides, back, plinth, crown
    for (int s = -1; s <= 1; s += 2) ob(mid + along * (s * (w * 0.5f - 0.025f)) + UP * (h * 0.5f), vec3(0.025f, h * 0.5f, CASE_D * 0.5f), wood, M_WOOD);
    ob(back + out * 0.012f + UP * (h * 0.5f), vec3(w * 0.5f - 0.04f, h * 0.5f - 0.06f, 0.012f), dark, M_WOOD);
    ob(mid + UP * 0.06f + out * 0.01f, vec3(w * 0.5f - 0.01f, 0.06f, CASE_D * 0.5f - 0.01f), wood * 0.85f, M_WOOD, 1.f);
    ob(mid + UP * (h - 0.06f) + out * 0.02f, vec3(w * 0.5f + 0.02f, 0.06f, CASE_D * 0.5f + 0.02f), wood, M_WOOD);
    ob(mid + UP * (h + 0.025f) + out * 0.035f, vec3(w * 0.5f + 0.04f, 0.025f, CASE_D * 0.5f + 0.035f), wood * 1.05f, M_WOOD);
    const float y0 = 0.12f, y1 = h - 0.12f;
    const float pitch = (y1 - y0) / shelves;
    const float board = 0.025f;
    const float innerW = w - 0.05f * 2.f;
    const int u = filler ? unit(foot + UP * (h * 0.5f)) : -1;
    for (int i = 0; i < shelves; i++) {
        const float yb = y0 + i * pitch;
        if (i > 0) ob(mid + UP * (yb + board * 0.5f) + out * 0.005f, vec3(innerW * 0.5f, board * 0.5f, CASE_D * 0.5f - 0.02f), wood * 1.02f, M_WOOD);
        const float clear = pitch - board - 0.005f;
        if (filler) {
            const float sIdx = (float)(shelves - 1 - i);
            const float r0 = sIdx / shelves, r1 = (sIdx + 1.f) / shelves;
            const Stamp keep = st_;
            st_.unit = (float)u;
            obox(foot - out * 0.13f + UP * (yb + board + clear * 0.5f), along, UP, out, vec3(innerW * 0.5f, clear * 0.5f, 0.1f), wood, M_SHELF,
                 r0, r1, 0.f, seed + i * 0.37f);
            st_ = keep;
        }
    }
}

// ---------------------------------------------------------------------------------------------------- the room
// One quiet room, all there from the start: a long back wall panelled below and plastered above, a fireplace in its
// middle (behind the well), the floor and its rug, a beamed ceiling. The collection's bookcases are set against the
// back wall on either side of the fireplace, one at a time as they are needed; the camera only ever frames what holds
// books, so the room never shows as an empty hall.
void LibraryUniverse::buildStudy() {
    const Palette& P = palT_;
    const float zb = ZBACK;
    st_ = Stamp{};
    // the back wall, the floor, the ceiling and its beams
    box(vec3(0.f, ROOM_H * 0.5f, zb - 0.15f), vec3(HALL_X + 0.4f, ROOM_H * 0.5f, 0.15f), P.plaster, M_PLASTER);
    box(vec3(0.f, -0.15f, 5.5f), vec3(HALL_X + 0.4f, 0.15f, 8.6f), P.floor, M_FLOOR);
    box(vec3(0.f, ROOM_H + 0.15f, 5.5f), vec3(HALL_X + 0.4f, 0.15f, 8.6f), P.plaster * 0.85f, M_PLASTER);
    for (int i = 0; i < 6; i++) box(vec3(0.f, ROOM_H - 0.13f, -1.6f + i * 2.6f), vec3(HALL_X, 0.13f, 0.12f), P.woodDark, M_WOOD);
    // panelling up to the dado rail (raised panels), a skirting, a picture rail, the cornice
    for (int i = -10; i <= 9; i++)
        box(vec3(i + 0.5f, 0.5f, zb + 0.025f), vec3(0.5f, 0.42f, 0.025f), P.wood * 0.78f, M_WOOD, 1.f, 0.f, 0.f, 0.3f * i);
    box(vec3(0.f, 0.06f, zb + 0.05f), vec3(HALL_X, 0.06f, 0.05f), P.woodDark, M_WOOD);
    box(vec3(0.f, 0.95f, zb + 0.06f), vec3(HALL_X, 0.035f, 0.06f), P.wood, M_WOOD);
    box(vec3(0.f, 3.95f, zb + 0.03f), vec3(HALL_X, 0.02f, 0.03f), P.wood * 0.9f, M_WOOD);
    box(vec3(0.f, ROOM_H - 0.16f, zb + 0.1f), vec3(HALL_X, 0.16f, 0.1f), P.wood, M_WOOD);
    // the fireplace: a marble surround, the fire, the mantel with a clock and two candlesticks, a panelled overmantel
    {
        const float z = zb + 0.2f;
        box(vec3(-0.62f, 0.62f, z), vec3(0.16f, 0.62f, 0.2f), P.marble, M_MARBLE, 0.f, 0.f, 0.f, 1.1f);
        box(vec3(0.62f, 0.62f, z), vec3(0.16f, 0.62f, 0.2f), P.marble, M_MARBLE, 0.f, 0.f, 0.f, 2.3f);
        box(vec3(0.f, 1.12f, z), vec3(0.78f, 0.12f, 0.2f), P.marble, M_MARBLE, 0.f, 0.f, 0.f, 3.7f);
        box(vec3(0.f, 1.27f, z + 0.04f), vec3(0.92f, 0.035f, 0.26f), P.marble * 0.95f, M_MARBLE);
        box(vec3(0.f, 0.5f, zb + 0.08f), vec3(0.46f, 0.5f, 0.06f), vec3(0.02f), M_IRON);
        box(vec3(0.f, 0.34f, zb + 0.16f), vec3(0.4f, 0.3f, 0.02f), vec3(1.f), M_FIRE, 0.f, 0.f, 0.f, 0.3f);
        box(vec3(0.f, 0.03f, z + 0.1f), vec3(0.95f, 0.03f, 0.35f), P.marble * 0.8f, M_MARBLE);
        lamp(vec3(0.f, 0.45f, zb + 0.5f), P.candle * vec3(1.f, 0.6f, 0.3f), 0.5f, 0.5f, 4.5f, 0.6f);
        box(vec3(0.f, 1.45f, z + 0.02f), vec3(0.13f, 0.15f, 0.07f), P.gilt * 0.8f, M_BRASS);
        put(MESH_CYL, vec3(0.f, 1.47f, z + 0.09f), vec3(0.085f, 0, 0), vec3(0, 0, 0.004f), vec3(0, -0.085f, 0), P.paper, M_PLASTER);
        for (int s = -1; s <= 1; s += 2) {
            cyl(vec3(s * 0.62f, 1.4f, z + 0.03f), 0.025f, 0.1f, P.gilt, M_BRASS);
            cyl(vec3(s * 0.62f, 1.58f, z + 0.03f), 0.016f, 0.08f, vec3(0.92f, 0.88f, 0.8f), M_PLASTER);
            sphere(vec3(s * 0.62f, 1.685f, z + 0.03f), 0.018f, P.candle, M_EMIT, 4.f);
            lamp(vec3(s * 0.62f, 1.69f, z + 0.03f), P.candle, 0.07f, 0.8f, 0.f, 1.f);
        }
        box(vec3(0.f, 2.05f, zb + 0.04f), vec3(0.8f, 0.62f, 0.04f), P.wood * 0.95f, M_WOOD, 1.2f);
        box(vec3(0.f, 2.05f, zb + 0.09f), vec3(0.6f, 0.44f, 0.01f), P.gilt * 0.55f, M_BRASS);
        box(vec3(0.f, 2.05f, zb + 0.1f), vec3(0.56f, 0.4f, 0.006f), lerp(P.plaster, P.lampCol, 0.15f) * 0.35f, M_MARBLE);
        // sconces either side: a brass arm and a pleated fabric shade lit from inside
        for (int s = -1; s <= 1; s += 2) {
            const float x = s * 1.25f;
            box(vec3(x, 2.15f, zb + 0.03f), vec3(0.05f, 0.08f, 0.02f), P.gilt, M_BRASS);
            box(vec3(x, 2.2f, zb + 0.13f), vec3(0.012f, 0.012f, 0.1f), P.gilt, M_BRASS);
            cyl(vec3(x, 2.3f, zb + 0.23f), 0.012f, 0.06f, P.gilt, M_BRASS);
            put(MESH_CYL, vec3(x, 2.42f, zb + 0.23f), vec3(0.1f, 0, 0), vec3(0, 0.08f, 0), vec3(0, 0, 0.1f), lerp(P.paper, P.lampCol, 0.4f) * 0.8f, M_SHADE, 0.55f);
            lamp(vec3(x, 2.42f, zb + 0.3f), P.lampCol, 0.25f, 0.9f, 4.f);
        }
    }
    // a rug in front of the fire
    box(vec3(0.f, 0.006f, 0.6f), vec3(2.6f, 0.006f, 2.f), P.rug, M_RUG, 0.f, 0.f, 0.f, 3.f);
}

// The side walls (seen only once the collection reaches the room's ends): panelled, tall windows over the bookcases.
void LibraryUniverse::buildHall() {
    const Palette& P = palT_;
    st_ = Stamp{};
    for (int s = -1; s <= 1; s += 2) {
        const float x = s * (HALL_X + 0.2f);
        box(vec3(x, ROOM_H * 0.5f, 5.5f), vec3(0.2f, ROOM_H * 0.5f, 8.7f), P.plaster, M_PLASTER);
        box(vec3(x - s * 0.22f, 0.5f, 5.5f), vec3(0.025f, 0.42f, 8.7f), P.wood * 0.78f, M_WOOD);
        box(vec3(x - s * 0.25f, 0.95f, 5.5f), vec3(0.06f, 0.035f, 8.7f), P.wood, M_WOOD);
        box(vec3(x - s * 0.3f, ROOM_H - 0.16f, 5.5f), vec3(0.1f, 0.16f, 8.7f), P.wood, M_WOOD);
        const vec3 u = s < 0 ? vec3(0, 0, -1) : vec3(0, 0, 1), w = vec3((float)-s, 0, 0);
        for (int i = 0; i < 2; i++) {
            const float z = 5.5f + i * 3.5f;
            obox(vec3(x - s * 0.21f, 2.3f, z), u, UP, w, vec3(0.6f, 1.25f, 0.03f), P.skyA, M_WINDOW, 1.f);
            obox(vec3(x - s * 0.27f, 1.02f, z), u, UP, w, vec3(0.7f, 0.035f, 0.1f), P.wood, M_WOOD);
        }
    }
}

// ---------------------------------------------------------------------------------------------------- everything
void LibraryUniverse::buildWorld() {
    for (auto& v : stat_) v.clear();
    lamps_.clear();
    units_.clear();
    shelves_.clear();
    const Palette& P = palT_;
    if (bays_.empty()) newBay();
    for (int i = 0; i < (int)bays_.size(); i++) {
        Bay& b = bays_[i];
        b.firstShelf = (int)shelves_.size();
        // its brass picture light over the crown, lighting the books
        {
            const Stamp keep = st_;
            st_ = Stamp{};
            lamp(b.c + UP * (TIER_H + 0.02f) + b.out * 0.22f, P.lampCol, 0.12f, 0.75f, 3.2f);
            st_ = keep;
        }
        const float y0 = 0.12f, y1 = TIER_H - 0.12f;
        const float pitch = (y1 - y0) / BAY_SHELVES;
        const float innerW = BAY_W - 0.1f;
        for (int s = BAY_SHELVES - 1; s >= 0; s--) {
            Shelf sh;
            const float yb = y0 + s * pitch;
            sh.o = b.c - b.along * (innerW * 0.5f) + UP * (yb + 0.025f) - b.out * 0.035f;
            sh.along = b.along;
            sh.out = b.out;
            sh.len = innerW;
            sh.clear = pitch - 0.03f;
            sh.bay = i;
            shelves_.push_back(sh);
        }
    }
    buildStudy();
    buildHall();
    (void)hashf;
    worldDirty_ = false;
}

// The collection's bookcases, in the order they are needed: either side of the fireplace in turn (right first),
// outward along the back wall, then down the side walls.
vec3 LibraryUniverse::bayFoot(int i, vec3* along, vec3* out) {
    vec3 a(1, 0, 0), o(0, 0, 1), c;
    if (i < 2 * BACK_BAYS) {
        const int side = i % 2 == 0 ? 1 : -1, k = i / 2;
        c = vec3(side * (FIRST_BAY_X + k * BAY_W), 0.f, ZBACK + CASE_D);
    } else {
        const int j = i - 2 * BACK_BAYS, side = j % 2 == 0 ? 1 : -1, k = j / 2;
        c = vec3(side * (HALL_X - CASE_D), 0.f, -1.9f + k * BAY_W);
        o = vec3((float)-side, 0.f, 0.f);
        a = side > 0 ? vec3(0, 0, 1) : vec3(0, 0, -1);
    }
    if (along) *along = a;
    if (out) *out = o;
    return c;
}

void LibraryUniverse::newBay() {
    const int i = (int)bays_.size();
    Bay b;
    b.c = bayFoot(i, &b.along, &b.out);
    b.born = 0.f;
    if (i > 0) b.t0 = worldTime_;
    bays_.push_back(b);
}
