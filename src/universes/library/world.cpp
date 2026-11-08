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

// ---------------------------------------------------------------------------------------------------- the study
void LibraryUniverse::buildStudy() {
    const Palette& P = palT_;
    const float zb = ZBACK;
    st_ = Stamp{};
    // the back wall, the floor, the wainscot and its dado rail
    box(vec3(0.f, HALL_H * 0.5f, zb - 0.15f), vec3(HALL_X + 0.4f, HALL_H * 0.5f, 0.15f), P.plaster, M_PLASTER, 1.f);
    box(vec3(0.f, -0.15f, 5.5f), vec3(HALL_X + 0.4f, 0.15f, 8.6f), P.floor, M_FLOOR);
    box(vec3(0.f, 0.95f * 0.5f, zb + 0.03f), vec3(HALL_X, 0.95f * 0.5f, 0.03f), P.wood * 0.8f, M_WOOD, 1.f);
    box(vec3(0.f, 0.97f, zb + 0.05f), vec3(HALL_X, 0.025f, 0.05f), P.wood, M_WOOD);
    // the room's own walls and ceiling: they fall away when the room opens
    st_.die = S_ROOM;
    st_.style = 1.f;
    for (int s = -1; s <= 1; s += 2) {
        const float x = s * 3.55f;
        box(vec3(x, 2.0f, -0.25f), vec3(0.1f, 2.0f, 2.95f), P.plaster, M_PLASTER, 1.f);
        box(vec3(x - s * 0.13f, 0.475f, -0.25f), vec3(0.03f, 0.475f, 2.95f), P.wood * 0.8f, M_WOOD, 1.f);
        box(vec3(x - s * 0.15f, 3.86f, -0.25f), vec3(0.06f, 0.06f, 2.95f), P.wood, M_WOOD);
    }
    box(vec3(0.f, 3.86f, zb + 0.07f), vec3(3.5f, 0.06f, 0.07f), P.wood, M_WOOD);
    st_.style = 2.f;
    box(vec3(0.f, 4.05f, -0.25f), vec3(3.7f, 0.12f, 3.0f), P.plaster * 0.9f, M_PLASTER);
    for (int i = -1; i <= 1; i++) box(vec3(0.f, 3.85f, -0.6f + i * 1.7f), vec3(3.5f, 0.09f, 0.12f), P.woodDark, M_WOOD);
    st_ = Stamp{};

    // the fireplace: a marble surround, the fire, the mantel with its clock and candlesticks, a mirror
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
        // the clock, the candlesticks
        box(vec3(0.f, 1.45f, z + 0.02f), vec3(0.13f, 0.15f, 0.07f), P.gilt * 0.8f, M_BRASS);
        cyl(vec3(0.f, 1.47f, z + 0.09f), 0.085f, 0.004f, P.paper, M_PLASTER);
        for (int s = -1; s <= 1; s += 2) {
            cyl(vec3(s * 0.62f, 1.4f, z + 0.03f), 0.025f, 0.1f, P.gilt, M_BRASS);
            cyl(vec3(s * 0.62f, 1.58f, z + 0.03f), 0.016f, 0.08f, vec3(0.92f, 0.88f, 0.8f), M_PLASTER);
            sphere(vec3(s * 0.62f, 1.685f, z + 0.03f), 0.018f, P.candle, M_EMIT, 4.f);
            lamp(vec3(s * 0.62f, 1.69f, z + 0.03f), P.candle, 0.07f, 0.8f, 0.f, 1.f);
        }
        // the mirror
        box(vec3(0.f, 2.35f, zb + 0.04f), vec3(0.62f, 0.72f, 0.03f), P.gilt * 0.7f, M_BRASS);
        box(vec3(0.f, 2.35f, zb + 0.075f), vec3(0.54f, 0.64f, 0.005f), P.skyB * 0.25f + vec3(0.02f), M_IRON);
        // the sconces on either side
        for (int s = -1; s <= 1; s += 2) {
            box(vec3(s * 1.15f, 2.2f, zb + 0.05f), vec3(0.05f, 0.09f, 0.03f), P.gilt, M_BRASS);
            box(vec3(s * 1.15f, 2.25f, zb + 0.13f), vec3(0.015f, 0.015f, 0.08f), P.gilt, M_BRASS);
            box(vec3(s * 1.15f, 2.38f, zb + 0.2f), vec3(0.07f, 0.08f, 0.07f), P.shade, M_SHADE, 1.0f);
            lamp(vec3(s * 1.15f, 2.38f, zb + 0.22f), P.lampCol, 0.18f, 0.9f, 3.5f);
        }
    }
    // the window in the left wall, over the desk: arched, its frame, the curtains (they go with the wall)
    {
        const float x = -3.44f, z = -1.45f;
        const vec3 u(0, 0, -1), w(1, 0, 0);
        st_.die = S_ROOM;
        st_.style = 1.f;
        obox(vec3(x, 2.05f, z), u, UP, w, vec3(0.54f, 1.0f, 0.03f), P.skyA, M_WINDOW, 1.f);
        obox(vec3(x + 0.08f, 1.03f, z), u, UP, w, vec3(0.64f, 0.03f, 0.1f), P.wood, M_WOOD);
        for (int s = -1; s <= 1; s += 2) {
            obox(vec3(x + 0.14f, 1.95f, z - s * 0.68f), u, UP, w, vec3(0.12f, 1.25f, 0.06f), P.rug * 0.9f, M_PLASTER, 0.f, 0.f, 0.f, 2.f + s);
            obox(vec3(x + 0.05f, 2.05f, z - s * 0.57f), u, UP, w, vec3(0.04f, 1.03f, 0.04f), P.wood, M_WOOD);
        }
        obox(vec3(x + 0.16f, 3.25f, z), u, UP, w, vec3(0.85f, 0.03f, 0.03f), P.gilt, M_BRASS);
        st_ = Stamp{};
    }
    // the desk under it: its top, two pedestals of drawers, the banker's lamp, papers, an inkwell; the chair
    {
        const float x = -3.0f, z = -1.45f;
        box(vec3(x, 0.745f, z), vec3(0.4f, 0.03f, 0.72f), P.wood, M_WOOD);
        box(vec3(x, 0.77f, z), vec3(0.3f, 0.004f, 0.55f), P.leather * 0.8f, M_LEATHER);
        for (int s = -1; s <= 1; s += 2) box(vec3(x, 0.36f, z + s * 0.48f), vec3(0.37f, 0.36f, 0.22f), P.wood * 0.92f, M_WOOD);
        const vec3 du(0, 0, -1), dw(1, 0, 0);
        for (int s = -1; s <= 1; s += 2)
            for (int i = 0; i < 3; i++) {
                obox(vec3(x + 0.371f, 0.14f + i * 0.22f, z + s * 0.48f), du, UP, dw, vec3(0.19f, 0.095f, 0.005f), P.wood * 1.05f, M_WOOD, 0.5f);
                box(vec3(x + 0.38f, 0.17f + i * 0.22f, z + s * 0.48f), vec3(0.008f, 0.008f, 0.03f), P.gilt, M_BRASS);
            }
        cyl(vec3(x - 0.1f, 0.775f, z - 0.45f), 0.08f, 0.012f, P.gilt, M_BRASS);
        cyl(vec3(x - 0.1f, 0.95f, z - 0.45f), 0.012f, 0.17f, P.gilt, M_BRASS);
        obox(vec3(x - 0.03f, 1.13f, z - 0.45f), du, normalize(vec3(0.35f, 1, 0)), normalize(vec3(1, -0.35f, 0)), vec3(0.17f, 0.035f, 0.07f), P.shade, M_SHADE,
             1.3f);
        lamp(vec3(x, 1.06f, z - 0.45f), P.lampCol, 0.22f, 1.2f, 4.f);
        obox(vec3(x + 0.1f, 0.776f, z + 0.1f), normalize(vec3(0.2f, 0, -1)), UP, normalize(vec3(1, 0, 0.2f)), vec3(0.12f, 0.002f, 0.085f), P.paper,
             M_PAPER, 0.f, 0.f, 0.f, 4.1f);
        cyl(vec3(x - 0.15f, 0.8f, z + 0.42f), 0.03f, 0.03f, vec3(0.03f), M_IRON);
        const float cx = x + 0.78f;
        box(vec3(cx, 0.46f, z), vec3(0.24f, 0.05f, 0.25f), P.leather, M_LEATHER);
        box(vec3(cx + 0.22f, 0.82f, z), vec3(0.04f, 0.32f, 0.25f), P.leather, M_LEATHER);
        for (int a = -1; a <= 1; a += 2)
            for (int b = -1; b <= 1; b += 2) cyl(vec3(cx + a * 0.2f, 0.21f, z + b * 0.21f), 0.02f, 0.21f, P.woodDark, M_WOOD);
    }
    // a longcase clock against the back wall
    {
        const float x = -2.45f, z = ZBACK + 0.2f;
        box(vec3(x, 0.95f, z), vec3(0.22f, 0.95f, 0.17f), P.wood, M_WOOD, 0.6f);
        box(vec3(x, 2.15f, z + 0.01f), vec3(0.28f, 0.27f, 0.19f), P.wood * 1.05f, M_WOOD);
        box(vec3(x, 2.48f, z + 0.02f), vec3(0.3f, 0.05f, 0.2f), P.wood * 0.9f, M_WOOD);
        put(MESH_CYL, vec3(x, 2.15f, z + 0.2f), vec3(0.17f, 0, 0), vec3(0, 0, 0.008f), vec3(0, -0.17f, 0), P.paper, M_PLASTER);
        put(MESH_CYL, vec3(x, 2.15f, z + 0.205f), vec3(0.19f, 0, 0), vec3(0, 0, 0.006f), vec3(0, -0.19f, 0), P.gilt, M_BRASS, 0.f, 0.f, 1.f);
        box(vec3(x, 2.15f, z + 0.212f), vec3(0.006f, 0.12f, 0.003f), vec3(0.02f), M_IRON);
        box(vec3(x + 0.04f, 2.17f, z + 0.214f), vec3(0.08f, 0.005f, 0.003f), vec3(0.02f), M_IRON);
        box(vec3(x, 1.1f, z + 0.172f), vec3(0.11f, 0.42f, 0.004f), vec3(0.03f), M_IRON);
        cyl(vec3(x, 0.85f, z + 0.18f), 0.07f, 0.008f, P.gilt, M_BRASS);
    }
    // the rug, an armchair by the fire, a globe on its stand
    box(vec3(0.f, 0.006f, -0.9f), vec3(2.1f, 0.006f, 1.35f), P.rug, M_RUG, 0.f, 0.f, 0.f, 3.f);
    {
        const vec3 c(-1.55f, 0.f, -1.55f);
        const vec3 f = normalize(vec3(0.75f, 0.f, -0.6f)), r = normalize(cross(f, UP)) * -1.f;
        const vec3 w = cross(r, UP);
        obox(c + UP * 0.25f, r, UP, w, vec3(0.42f, 0.2f, 0.4f), P.leather, M_LEATHER);
        obox(c + UP * 0.62f - w * 0.34f, r, UP, w, vec3(0.42f, 0.42f, 0.08f), P.leather, M_LEATHER);
        for (int s = -1; s <= 1; s += 2) obox(c + UP * 0.5f + r * (s * 0.4f), r, UP, w, vec3(0.07f, 0.12f, 0.38f), P.leather * 0.95f, M_LEATHER);
    }
    {
        const vec3 g(1.65f, 0.f, zb + 0.8f);
        for (int i = 0; i < 3; i++) {
            const float a = i * 2.094f;
            const vec3 foot = g + vec3(std::cos(a) * 0.2f, 0.f, std::sin(a) * 0.2f);
            const vec3 top = g + UP * 0.7f;
            const vec3 Y = (top - foot) * 0.5f;
            const vec3 X = normalize(cross(Y, vec3(0, 0, 1))) * 0.018f;
            const vec3 Z = normalize(cross(X, Y)) * 0.018f;
            put(MESH_CYL, (foot + top) * 0.5f, X, Y, Z, P.woodDark, M_WOOD);
        }
        sphere(g + UP * 0.95f, 0.24f, vec3(0.5f), M_GLOBE);
        put(MESH_CYL, g + UP * 0.95f, vec3(0.27f, 0, 0), vec3(0, 0, 0.006f), vec3(0, -0.27f, 0), P.gilt, M_BRASS);
    }
}

// ---------------------------------------------------------------------------------------------------- the long room
// When the study's walls have gone: a tall room, its back wall waiting for bookcases from end to end, high windows
// over them and down its side walls, reading tables, chandeliers, a brass rail with its ladders.
void LibraryUniverse::buildHall() {
    const Palette& P = palT_;
    const float zb = ZBACK;
    const vec3 along(1, 0, 0);
    st_ = Stamp{};
    st_.born = S_ROOM;
    st_.style = 1.f;
    // the side walls, their wainscot, the cornice; tall arched windows high on every wall
    for (int s = -1; s <= 1; s += 2) {
        const float x = s * (HALL_X + 0.2f);
        box(vec3(x, HALL_H * 0.5f, 5.5f), vec3(0.2f, HALL_H * 0.5f, 8.7f), P.plaster, M_PLASTER, 1.f);
        box(vec3(x - s * 0.23f, 0.475f, 5.5f), vec3(0.03f, 0.475f, 8.7f), P.wood * 0.8f, M_WOOD, 1.f);
        box(vec3(x - s * 0.3f, HALL_H - 0.3f, 5.5f), vec3(0.12f, 0.16f, 8.7f), P.wood, M_WOOD);
        const vec3 u = s < 0 ? vec3(0, 0, -1) : vec3(0, 0, 1), w = vec3((float)-s, 0, 0);
        for (int i = 0; i < 3; i++) {
            const float z = 0.f + i * 3.6f;
            obox(vec3(x - s * 0.21f, 5.45f, z), u, UP, w, vec3(0.62f, 1.0f, 0.03f), P.skyA, M_WINDOW, 1.f);
            obox(vec3(x - s * 0.27f, 4.35f, z), u, UP, w, vec3(0.85f, 0.04f, 0.1f), P.wood, M_WOOD);
            // a sconce between the windows, lighting the bookcases below
            const vec3 lp(x - s * 0.45f, 3.95f, z + 1.8f);
            box(lp - UP * 0.1f + vec3(s * 0.17f, 0, 0), vec3(0.03f, 0.1f, 0.06f), P.gilt, M_BRASS);
            box(lp, vec3(0.09f, 0.1f, 0.09f), P.shade, M_SHADE, 1.f);
            lamp(lp, P.lampCol, 0.22f, 0.9f, 4.5f);
        }
    }
    box(vec3(0.f, HALL_H - 0.3f, zb + 0.12f), vec3(HALL_X, 0.16f, 0.12f), P.wood, M_WOOD);
    for (int i = -1; i <= 1; i++) {
        const float x = i * 6.6f;
        box(vec3(x, 5.45f, zb + 0.02f), vec3(0.7f, 1.0f, 0.03f), P.skyA, M_WINDOW, 1.f);
        box(vec3(x, 4.35f, zb + 0.08f), vec3(0.95f, 0.04f, 0.1f), P.wood, M_WOOD);
    }
    // sconces over the back wall's bookcases
    for (int i = 0; i < 4; i++) {
        const vec3 lp(-9.9f + i * 6.6f + 3.3f, 4.05f, zb + 0.32f);
        box(lp - vec3(0, 0.1f, 0.2f), vec3(0.06f, 0.1f, 0.03f), P.gilt, M_BRASS);
        box(lp, vec3(0.09f, 0.1f, 0.09f), P.shade, M_SHADE, 1.f);
        lamp(lp, P.lampCol, 0.22f, 0.9f, 5.f);
    }
    // the ceiling: plaster between dark beams, coming down from above
    st_.style = 2.f;
    box(vec3(0.f, HALL_H + 0.15f, 5.5f), vec3(HALL_X + 0.4f, 0.15f, 8.7f), P.plaster * 0.9f, M_PLASTER);
    for (int i = 0; i < 7; i++) box(vec3(0.f, HALL_H - 0.12f, -2.f + i * 2.4f), vec3(HALL_X, 0.14f, 0.14f), P.woodDark, M_WOOD);
    st_ = Stamp{};
    st_.born = S_ROOM + 0.8f;
    // the brass rail the ladders run on, along the back wall's bookcases
    box(vec3(0.f, 3.85f, zb + CASE_D + 0.06f), vec3(HALL_X - 0.3f, 0.012f, 0.012f), P.gilt, M_BRASS);
    for (int i = -4; i <= 4; i++) box(vec3(i * 2.3f, 3.85f, zb + CASE_D + 0.03f), vec3(0.02f, 0.02f, 0.04f), P.gilt, M_BRASS);
    auto ladder = [&](float x, float born) {
        st_.born = born;
        const vec3 b(x, 0.f, zb + CASE_D + 0.95f), t(x, 3.85f, zb + CASE_D + 0.1f);
        const vec3 d = normalize(t - b);
        const vec3 w = normalize(cross(along, d));
        const float L = length(t - b);
        for (int s = -1; s <= 1; s += 2) obox((b + t) * 0.5f + along * (s * 0.22f), along, d, w, vec3(0.025f, L * 0.5f, 0.035f), P.wood, M_WOOD);
        for (int i = 1; i < 13; i++) put(MESH_CYL, b + (t - b) * (i / 13.f), d * 0.014f, along * 0.22f, cross(d, along) * 0.014f, P.wood * 1.1f, M_WOOD);
        box(t + vec3(0, 0.02f, -0.02f), vec3(0.26f, 0.03f, 0.03f), P.gilt, M_BRASS);
    };
    ladder(-1.55f, S_ROOM + 1.5f);
    // reading tables with their lamps and chairs, a long rug
    auto table = [&](const vec3& c, float born) {
        st_.born = born;
        box(c + UP * 0.76f, vec3(1.25f, 0.03f, 0.48f), P.wood, M_WOOD);
        box(c + UP * 0.79f, vec3(1.0f, 0.004f, 0.32f), P.leather * 0.75f, M_LEATHER);
        for (int a = -1; a <= 1; a += 2)
            for (int b = -1; b <= 1; b += 2) box(c + vec3(a * 1.1f, 0.37f, b * 0.38f), vec3(0.04f, 0.37f, 0.04f), P.woodDark, M_WOOD);
        for (int s = -1; s <= 1; s += 2) {
            const vec3 l = c + vec3(s * 0.55f, 0.f, 0.f);
            cyl(l + UP * 0.805f, 0.07f, 0.01f, P.gilt, M_BRASS);
            cyl(l + UP * 0.97f, 0.011f, 0.16f, P.gilt, M_BRASS);
            box(l + UP * 1.14f, vec3(0.17f, 0.035f, 0.07f), P.shade, M_SHADE, 1.2f);
            lamp(l + UP * 1.08f, P.lampCol, 0.22f, 1.1f, 4.5f);
            for (int b = -1; b <= 1; b += 2) {
                const vec3 ch = c + vec3(s * 0.6f, 0.f, b * 0.8f);
                box(ch + UP * 0.46f, vec3(0.22f, 0.04f, 0.21f), P.leather, M_LEATHER);
                box(ch + UP * 0.78f + vec3(0, 0, b * 0.2f), vec3(0.22f, 0.28f, 0.03f), P.woodDark, M_WOOD, 0.4f);
            }
        }
    };
    // (down the middle of the room, so they never stand in front of the collection)
    table(vec3(0.f, 0.f, 1.4f), S_ROOM + 1.0f);
    st_.born = S_ROOM + 0.5f;
    box(vec3(0.f, 0.005f, 2.4f), vec3(2.2f, 0.005f, 3.6f), P.rug, M_RUG, 0.f, 0.f, 0.f, 5.f);
    // chandeliers, let down from the ceiling
    auto chandelier = [&](const vec3& c, float r, int n, float born) {
        st_.born = born;
        st_.style = 2.f;
        put(MESH_CYL, c + UP * 0.02f, vec3(r, 0, 0), vec3(0, 0.03f, 0), vec3(0, 0, r), P.gilt, M_BRASS, 0.f, 0.f, 1.f);
        put(MESH_CYL, c + UP * 0.3f, vec3(r * 0.5f, 0, 0), vec3(0, 0.025f, 0), vec3(0, 0, r * 0.5f), P.gilt, M_BRASS, 0.f, 0.f, 1.f);
        const float chain = (HALL_H - c.y) * 0.5f;
        cyl(c + UP * chain, 0.02f, chain, P.gilt * 0.6f, M_IRON);
        sphere(c + UP * 0.12f, 0.12f, P.gilt, M_BRASS);
        for (int i = 0; i < n; i++) {
            const float a = i * 6.2832f / n;
            const vec3 p = c + vec3(std::cos(a) * r, 0.f, std::sin(a) * r);
            cyl(p + UP * 0.1f, 0.016f, 0.08f, vec3(0.93f, 0.9f, 0.82f), M_PLASTER);
            sphere(p + UP * 0.2f, 0.022f, P.candle, M_EMIT, 4.f);
            lamp(p + UP * 0.21f, P.candle, 0.09f, 0.7f, 0.f, 1.f);
        }
        lamp(c + UP * 0.15f, P.candle, 0.6f, 0.4f, 8.f, 0.3f);
        st_.style = 0.f;
    };
    chandelier(vec3(-5.4f, 7.0f, 0.9f), 0.6f, 10, S_ROOM + 2.2f);
    chandelier(vec3(5.6f, 7.0f, 0.9f), 0.6f, 10, S_ROOM + 2.6f);
    st_ = Stamp{};
}

// ---------------------------------------------------------------------------------------------------- everything
void LibraryUniverse::buildWorld() {
    for (auto& v : stat_) v.clear();
    lamps_.clear();
    units_.clear();
    shelves_.clear();
    // the collection's bookcases: the study's, the six standing ready right of it once the room opens; the others
    // are carried in as they are needed (newBay)
    while (bays_.size() < 7) newBay();
    for (int i = 0; i < (int)bays_.size(); i++) {
        Bay& b = bays_[i];
        b.firstShelf = (int)shelves_.size();
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

// The collection's bookcases, in the order they fill: the study's, then along the back wall to its right, then left
// of the study, then down the side walls (one each side in turn).
void LibraryUniverse::newBay() {
    const int i = (int)bays_.size();
    Bay b;
    b.along = vec3(1, 0, 0);
    b.out = vec3(0, 0, 1);
    if (i < 7) b.c = vec3(2.9f + i * BAY_W, 0.f, ZBACK + CASE_D);
    else if (i < 13) b.c = vec3(-3.95f - (i - 7) * BAY_W, 0.f, ZBACK + CASE_D);
    else {
        const int j = i - 13, side = j % 2 == 0 ? 1 : -1, k = j / 2;
        b.c = vec3(side * (HALL_X - CASE_D), 0.f, -1.9f + k * BAY_W);
        b.out = vec3((float)-side, 0.f, 0.f);
        b.along = side > 0 ? vec3(0, 0, 1) : vec3(0, 0, -1);
    }
    b.born = i == 0 ? 0.f : i < 7 ? S_ROOM + 0.6f + 0.45f * i : grow_;
    bays_.push_back(b);
}
