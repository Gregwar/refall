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
    // the back wall (the hexagon's side 0) and its wainscot, the dado rail
    box(vec3(0.f, 0.95f * 0.5f, zb + 0.03f), vec3(3.45f, 0.95f * 0.5f, 0.03f), P.wood * 0.8f, M_WOOD, 1.f);
    box(vec3(0.f, 0.97f, zb + 0.05f), vec3(3.45f, 0.025f, 0.05f), P.wood, M_WOOD);
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
    // the window over the desk: arched, its frame, the curtains
    {
        const float x = -2.75f;
        box(vec3(x, 2.05f, zb + 0.02f), vec3(0.62f, 1.05f, 0.03f), P.skyA, M_WINDOW, 1.f);
        box(vec3(x, 0.98f, zb + 0.08f), vec3(0.72f, 0.03f, 0.1f), P.wood, M_WOOD);
        for (int s = -1; s <= 1; s += 2) {
            box(vec3(x + s * 0.78f, 1.95f, zb + 0.14f), vec3(0.14f, 1.25f, 0.06f), P.rug * 0.9f, M_PLASTER, 0.f, 0.f, 0.f, 2.f + s);
            box(vec3(x + s * 0.66f, 2.05f, zb + 0.05f), vec3(0.04f, 1.08f, 0.04f), P.wood, M_WOOD);
        }
        box(vec3(x, 3.25f, zb + 0.16f), vec3(1.0f, 0.03f, 0.03f), P.gilt, M_BRASS);
    }
    // the desk: its top, two pedestals of drawers, the lamp, books, papers; the chair
    {
        const float x = -2.75f, z = zb + 0.75f;
        box(vec3(x, 0.745f, z), vec3(0.78f, 0.03f, 0.4f), P.wood, M_WOOD);
        box(vec3(x, 0.77f, z), vec3(0.6f, 0.004f, 0.3f), P.leather * 0.8f, M_LEATHER);
        for (int s = -1; s <= 1; s += 2) box(vec3(x + s * 0.52f, 0.36f, z), vec3(0.24f, 0.36f, 0.37f), P.wood * 0.92f, M_WOOD);
        for (int s = -1; s <= 1; s += 2)
            for (int i = 0; i < 3; i++) {
                box(vec3(x + s * 0.52f, 0.14f + i * 0.22f, z + 0.371f), vec3(0.21f, 0.095f, 0.005f), P.wood * 1.05f, M_WOOD, 0.5f);
                box(vec3(x + s * 0.52f, 0.17f + i * 0.22f, z + 0.38f), vec3(0.03f, 0.008f, 0.008f), P.gilt, M_BRASS);
            }
        // the banker's lamp
        cyl(vec3(x - 0.45f, 0.775f, z - 0.1f), 0.08f, 0.012f, P.gilt, M_BRASS);
        cyl(vec3(x - 0.45f, 0.95f, z - 0.1f), 0.012f, 0.17f, P.gilt, M_BRASS);
        obox(vec3(x - 0.45f, 1.13f, z - 0.03f), vec3(1, 0, 0), normalize(vec3(0, 1, 0.35f)), normalize(vec3(0, -0.35f, 1)), vec3(0.17f, 0.035f, 0.07f),
             P.shade, M_SHADE, 1.3f);
        lamp(vec3(x - 0.45f, 1.06f, z), P.lampCol, 0.22f, 1.2f, 4.f);
        // books on the desk
        const vec3 bc[3] = {P.piece[0] * 0.6f, P.piece[3] * 0.6f, P.piece[5] * 0.6f};
        for (int i = 0; i < 3; i++)
            obox(vec3(x + 0.42f, 0.78f + 0.025f + i * 0.05f, z - 0.05f), normalize(vec3(std::cos(i * 0.4f), 0, std::sin(i * 0.4f))), vec3(0, -1, 0) * -1.f,
                 normalize(vec3(-std::sin(i * 0.4f), 0, std::cos(i * 0.4f))), vec3(0.13f, 0.024f, 0.09f), bc[i], M_BOOK, (float)(i % 4), 0.f, 0.f,
                 0.3f + i);
        obox(vec3(x + 0.05f, 0.776f, z + 0.12f), normalize(vec3(1, 0, 0.2f)), UP, normalize(vec3(-0.2f, 0, 1)), vec3(0.12f, 0.002f, 0.085f), P.paper,
             M_PAPER, 0.f, 0.f, 0.f, 4.1f);
        // the chair
        const float cz = z + 0.75f;
        box(vec3(x, 0.46f, cz), vec3(0.25f, 0.05f, 0.24f), P.leather, M_LEATHER);
        box(vec3(x, 0.82f, cz + 0.22f), vec3(0.25f, 0.32f, 0.04f), P.leather, M_LEATHER);
        for (int a = -1; a <= 1; a += 2)
            for (int b = -1; b <= 1; b += 2) cyl(vec3(x + a * 0.21f, 0.21f, cz + b * 0.2f), 0.02f, 0.21f, P.woodDark, M_WOOD);
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

// ---------------------------------------------------------------------------------------------------- the long room, its galleries
void LibraryUniverse::buildHall() {
    const Palette& P = palT_;
    const float zb = ZBACK;
    const vec3 along(1, 0, 0), out(0, 0, 1);
    // the filler bookcases left of the study, popping up one after the other as the room opens; the collection
    // takes them over late in a long game (bays 8..13)
    st_ = Stamp{};
    fillerFeet_.clear();
    for (int i = 0; i < 6; i++) {
        const vec3 foot(-3.95f - i * BAY_W, 0.f, zb + CASE_D);
        const int bay = 7 + i;
        st_.born = S_ROOM + 0.4f + 0.55f * i;
        st_.die = (bay < (int)bays_.size() && bays_[bay].born >= 0.f) ? bays_[bay].born : -1.f;
        st_.style = 0.f;
        fillerFeet_.push_back(foot);
        buildBookcase(foot, along, out, BAY_W, TIER_H, BAY_SHELVES, P.wood, true, i, 10.f + i);
    }
    // the plaster wall above the wainscot, from end to end; the brass rail the ladders run on
    st_ = Stamp{};
    st_.born = S_ROOM + 0.8f;
    st_.style = 0.f;
    box(vec3(0.f, 3.55f, zb + CASE_D + 0.06f), vec3(9.6f, 0.012f, 0.012f), P.gilt, M_BRASS);
    for (int i = -4; i <= 4; i++) box(vec3(i * 2.3f, 3.55f, zb + CASE_D + 0.03f), vec3(0.02f, 0.02f, 0.04f), P.gilt, M_BRASS);
    // the ladders: one by the collection, one by the fillers
    auto ladder = [&](float x, float born) {
        st_.born = born;
        const vec3 b(x, 0.f, zb + CASE_D + 0.95f), t(x, 3.55f, zb + CASE_D + 0.1f);
        const vec3 d = normalize(t - b);
        const vec3 w = normalize(cross(along, d));
        const float L = length(t - b);
        for (int s = -1; s <= 1; s += 2) obox((b + t) * 0.5f + along * (s * 0.22f), along, d, w, vec3(0.025f, L * 0.5f, 0.035f), P.wood, M_WOOD);
        for (int i = 1; i < 12; i++) {
            const vec3 p = b + (t - b) * (i / 12.f);
            put(MESH_CYL, p, d * 0.014f, along * 0.22f, cross(d * 0.014f, along * 0.22f) * (1.f / 0.22f), P.wood * 1.1f, M_WOOD);
        }
        box(t + vec3(0, 0.02f, -0.02f), vec3(0.26f, 0.03f, 0.03f), P.gilt, M_BRASS);
    };
    ladder(5.6f, S_ROOM + 2.5f);
    ladder(-6.4f, S_ROOM + 3.5f);
    // reading tables with their lamps and chairs
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
    table(vec3(-4.4f, 0.f, 0.6f), S_ROOM + 1.6f);
    table(vec3(4.6f, 0.f, 0.6f), S_ROOM + 2.8f);
    // chandeliers
    auto chandelier = [&](const vec3& c, float r, int n, float born) {
        st_.born = born;
        st_.die = S_SHAFT - 0.5f;
        st_.style = 2.f;
        put(MESH_CYL, c + UP * 0.02f, vec3(r, 0, 0), vec3(0, 0.03f, 0), vec3(0, 0, r), P.gilt, M_BRASS, 0.f, 0.f, 1.f);
        put(MESH_CYL, c + UP * 0.3f, vec3(r * 0.5f, 0, 0), vec3(0, 0.025f, 0), vec3(0, 0, r * 0.5f), P.gilt, M_BRASS, 0.f, 0.f, 1.f);
        cyl(c + UP * 1.6f, 0.02f, 1.6f, P.gilt * 0.6f, M_IRON);
        sphere(c + UP * 0.12f, 0.12f, P.gilt, M_BRASS);
        for (int i = 0; i < n; i++) {
            const float a = i * 6.2832f / n;
            const vec3 p = c + vec3(std::cos(a) * r, 0.f, std::sin(a) * r);
            cyl(p + UP * 0.1f, 0.016f, 0.08f, vec3(0.93f, 0.9f, 0.82f), M_PLASTER);
            sphere(p + UP * 0.2f, 0.022f, P.candle, M_EMIT, 4.f);
            lamp(p + UP * 0.21f, P.candle, 0.09f, 0.7f, 0.f, 1.f);
        }
        lamp(c + UP * 0.15f, P.candle, 0.6f, 0.35f, 7.f, 0.3f);
        st_.style = 0.f;
        st_.die = -1.f;
    };
    chandelier(vec3(-4.4f, 3.3f, 0.4f), 0.45f, 8, S_ROOM + 2.0f);
    chandelier(vec3(4.6f, 3.3f, 0.4f), 0.45f, 8, S_ROOM + 3.0f);

    // ---- the galleries: sides 5, 0, 1 first (the room), the others with the rotunda
    st_ = Stamp{};
    const int sideOrder[6] = {0, 5, 1, 4, 2, 3};
    for (int oi = 0; oi < 6; oi++) {
        const int k = sideOrder[oi];
        const bool room = oi < 3;
        const SideFrame F = frameOf(k);
        const vec3 C(0.f, 0.f, ZC);
        const float bornT0 = room ? S_GALLERY + 0.6f * oi : S_ROTUNDA + 0.5f + 0.7f * (oi - 3);
        for (int t = 0; t < 3; t++) {
            // tier 0 of the back wall holds the real bookcases (the study, the collection, the fillers)
            if (k == 0 && t == 0) continue;
            const float tb = (t == 0 ? (room ? S_GALLERY + 1.0f + 0.5f * oi : bornT0) : t == 1 ? bornT0 + 0.5f : bornT0 + 7.f + 0.5f * oi);
            const float bornTier = room ? tb : bornT0 + 0.8f * t;
            st_.born = bornTier;
            st_.style = 1.f;
            const float y = tierY(t);
            const float len = R - 2.f * CASE_D * 0.5774f - 0.02f;
            const vec3 c = C + F.n * (A - CASE_D * 0.5f) + UP * (y + TIER_H * 0.5f);
            const Stamp keep = st_;
            st_.unit = (float)unit(c);
            obox(c, F.t, UP, F.in, vec3(len * 0.5f, TIER_H * 0.5f, CASE_D * 0.5f), P.wood, M_BOOKWALL, BAY_W, (float)BAY_SHELVES, 0.f,
                 1.3f * k + 7.1f * t);
            st_ = keep;
            if (t > 0) {
                // the gallery's floor, its fascia, brackets under it, the balustrade
                st_.style = 0.f;
                const float a0 = GAL_IN, a1 = A - CASE_D;
                const float gl = 2.f * a1 * 0.5774f;
                const vec3 fc = C + F.n * ((a0 + a1) * 0.5f) + UP * (y - 0.1f);
                obox(fc, F.t, UP, F.in, vec3(gl * 0.5f, 0.1f, (a1 - a0) * 0.5f), P.wood * 0.9f, M_WOOD);
                obox(C + F.n * (a0 + 0.03f) + UP * (y - 0.16f), F.t, UP, F.in, vec3(a0 * 0.5774f, 0.16f, 0.04f), P.wood, M_WOOD, 0.6f);
                for (int b = -4; b <= 4; b++)
                    obox(C + F.n * (a1 - 0.35f) + F.t * (b * 2.1f) + UP * (y - 0.42f), F.t, UP, F.in, vec3(0.06f, 0.22f, 0.35f), P.wood * 0.8f, M_WOOD);
                obox(C + F.n * (a0 + 0.06f) + UP * (y + 0.5f), F.t, UP, F.in, vec3(a0 * 0.5774f, 0.5f, 0.03f), P.wood * 1.05f, M_BALUSTRADE);
                obox(C + F.n * (a0 + 0.06f) + UP * (y + 1.02f), F.t, UP, F.in, vec3(a0 * 0.5774f, 0.025f, 0.06f), P.wood * 0.8f, M_WOOD);
                // a lamp on the balustrade at each end
                for (int s = -1; s <= 1; s += 2) {
                    const vec3 lp = C + F.n * (a0 + 0.06f) + F.t * (s * a0 * 0.5774f * 0.6f) + UP * (y + 1.3f);
                    cyl(lp - UP * 0.15f, 0.015f, 0.12f, P.gilt, M_BRASS);
                    sphere(lp, 0.11f, P.lampCol, M_EMIT, 1.6f, 1.f);
                    lamp(lp, P.lampCol, 0.35f, 0.8f, 6.f);
                }
            }
        }
        // the cornice
        st_.born = room ? bornT0 + 9.f : bornT0 + 2.5f;
        st_.style = 0.f;
        obox(C + F.n * (A - 0.35f) + UP * 12.1f, F.t, UP, F.in, vec3(R * 0.5f, 0.3f, 0.35f), P.wood, M_WOOD, 0.8f);
    }
    // the ladders upstairs
    st_.born = S_GALLERY + 5.f;
    {
        const vec3 b(-2.5f, 4.1f, zb + CASE_D + 0.85f), t(-2.5f, 7.6f, zb + CASE_D + 0.1f);
        const vec3 d = normalize(t - b), w = normalize(cross(along, d));
        const float L = length(t - b);
        for (int s = -1; s <= 1; s += 2) obox((b + t) * 0.5f + along * (s * 0.22f), along, d, w, vec3(0.025f, L * 0.5f, 0.035f), P.wood, M_WOOD);
        for (int i = 1; i < 12; i++) put(MESH_CYL, b + (t - b) * (i / 12.f), d * 0.014f, along * 0.22f, cross(d, along) * 0.014f, P.wood, M_WOOD);
    }
    // the spiral stair up to the gallery, step by step
    {
        const vec3 c(-8.2f, 0.f, -0.35f);
        st_.born = S_GALLERY + 1.5f;
        cyl(c + UP * 2.1f, 0.09f, 2.1f, P.wood, M_WOOD);
        sphere(c + UP * 4.3f, 0.12f, P.gilt, M_BRASS);
        const int n = 22;
        for (int i = 0; i < n; i++) {
            st_.born = S_GALLERY + 1.5f + 0.12f * i;
            const float a = 1.2f + i * 0.33f;
            const vec3 u(std::cos(a), 0.f, std::sin(a));
            const vec3 w = cross(u, UP);
            const float y = (i + 1) * (4.1f / (n + 1));
            obox(c + u * 0.66f + UP * y, u, UP, w, vec3(0.56f, 0.03f, 0.17f + 0.0f), P.wood * 1.05f, M_WOOD);
            put(MESH_CYL, c + u * 1.16f + UP * (y + 0.45f), vec3(0.012f, 0, 0), vec3(0, 0.45f, 0), vec3(0, 0, 0.012f), P.gilt * 0.8f, M_BRASS);
        }
    }
    // a great chandelier in the middle of the room
    chandelier(vec3(0.f, 7.2f, 2.5f), 1.1f, 14, S_GALLERY + 6.f);
}

// ---------------------------------------------------------------------------------------------------- the rotunda (the reading room)
void LibraryUniverse::buildRotunda() {
    const Palette& P = palT_;
    const vec3 C(0.f, 0.f, ZC);
    // the floor: the ring (always), the middle (gone when the shaft opens), a marble inlay
    st_ = Stamp{};
    for (int k = 0; k < 6; k++) {
        const SideFrame F = frameOf(k);
        obox(C + F.n * ((HOLE + A) * 0.5f) + UP * -0.15f, F.t, UP, F.in, vec3(R * 0.5f, 0.15f, (A - HOLE) * 0.5f), P.floor, M_FLOOR);
        // the plaster walls behind everything
        obox(C + F.n * (A + 0.2f) + UP * (HL * 0.5f), F.t, UP, F.in, vec3(R * 0.5f + 0.3f, HL * 0.5f, 0.2f), P.plaster, M_PLASTER, k == 0 ? 1.f : 0.f);
    }
    st_.die = S_SHAFT;
    st_.style = 1.f;
    for (int k = 0; k < 6; k++) {
        const SideFrame F = frameOf(k);
        obox(C + F.n * (HOLE * 0.5f) + UP * -0.15f, F.t, UP, F.in, vec3(HOLE * 0.5774f, 0.15f, HOLE * 0.5f), P.floor, M_FLOOR);
    }
    st_.born = S_ROTUNDA + 0.5f;
    for (int k = 0; k < 6; k++) {
        const SideFrame F = frameOf(k);
        obox(C + F.n * (HOLE * 0.5f) + UP * 0.004f, F.t, UP, F.in, vec3(HOLE * 0.5774f, 0.006f, HOLE * 0.5f), P.marble, M_FLOOR, 1.f);
    }
    // the issue desk at the middle, a lamp post on it
    st_.born = S_ROTUNDA + 1.f;
    for (int k = 0; k < 6; k++) {
        const SideFrame F = frameOf(k);
        obox(C + F.n * 2.4f + UP * 0.55f, F.t * -1.f, UP, F.n, vec3(1.38f, 0.55f, 0.3f), P.wood, M_WOOD, 0.8f);
        obox(C + F.n * 2.4f + UP * 1.13f, F.t * -1.f, UP, F.n, vec3(1.5f, 0.03f, 0.4f), P.wood * 0.8f, M_WOOD);
    }
    cyl(C + UP * 1.8f, 0.07f, 1.8f, P.gilt, M_BRASS);
    for (int k = 0; k < 6; k++) {
        const vec3 d = cornerDir(k);
        sphere(C + d * 0.45f + UP * 3.3f, 0.16f, P.lampCol, M_EMIT, 1.5f, 1.f);
        cyl(C + d * 0.22f + UP * 3.2f, 0.012f, 0.24f, P.gilt, M_BRASS);
    }
    lamp(C + UP * 3.4f, P.lampCol, 0.9f, 1.2f, 12.f);
    // the reading desks: twelve spokes running out from the middle, green lamps along their screens
    for (int i = 0; i < 12; i++) {
        st_.born = S_ROTUNDA + 6.f + 0.45f * i;
        st_.die = S_SHAFT + 0.1f * i;
        st_.style = 1.f;
        const float a = (i + 0.5f) * 6.2832f / 12.f;
        const vec3 u(std::cos(a), 0.f, std::sin(a));
        const vec3 w = cross(u, UP);
        const float r0 = 4.0f, r1 = 8.4f;
        const vec3 m = C + u * ((r0 + r1) * 0.5f);
        const float hl = (r1 - r0) * 0.5f;
        obox(m + UP * 0.38f, u, UP, w, vec3(hl, 0.38f, 0.55f), P.wood * 0.9f, M_WOOD, 0.5f);
        obox(m + UP * 0.78f, u, UP, w, vec3(hl + 0.05f, 0.025f, 0.62f), P.leather * 0.7f, M_LEATHER);
        obox(m + UP * 1.0f, u, UP, w, vec3(hl, 0.2f, 0.03f), P.wood, M_WOOD, 0.4f);
        for (int j = 0; j < 4; j++) {
            const vec3 l = C + u * (r0 + 0.6f + j * 1.05f);
            obox(l + UP * 1.27f, u, UP, w, vec3(0.16f, 0.04f, 0.07f), P.shade, M_SHADE, 1.2f);
            if (j == 1) lamp(l + UP * 1.2f, P.lampCol, 0.25f, 0.9f, 0.f);
            for (int s = -1; s <= 1; s += 2) {
                const vec3 ch = l + w * (s * 0.95f);
                obox(ch + UP * 0.46f, u, UP, w, vec3(0.21f, 0.04f, 0.21f), P.leather, M_LEATHER);
                obox(ch + UP * 0.76f + w * (s * 0.2f), u, UP, w, vec3(0.21f, 0.26f, 0.03f), P.leather, M_LEATHER);
            }
        }
    }
    // the ceiling round the dome, the dome (brought down from above), the sky over its oculus
    st_ = Stamp{};
    st_.born = S_ROTUNDA + 3.5f;
    st_.die = S_SHAFT;
    st_.style = 2.f;
    box(C + UP * (HL + 0.25f), vec3(R + 0.6f, 0.25f, R + 0.6f), P.marble * 0.6f, M_CEIL, A - 0.25f, 0.f, ZC);
    st_.born = S_ROTUNDA + 4.0f;
    const float rd = A - 0.25f;
    put(MESH_DOME, C + UP * HL, vec3(rd, 0, 0), vec3(0, rd * 0.92f, 0), vec3(0, 0, rd), lerp(P.marble, P.paper, 0.5f) * 0.75f, M_DOME);
    put(MESH_CYL, C + UP * (HL + 0.15f), vec3(rd + 0.1f, 0, 0), vec3(0, 0.18f, 0), vec3(0, 0, rd + 0.1f), P.gilt * 0.7f, M_BRASS, 0.f, 0.f, 1.f);
    box(C + UP * (HL + rd * 0.92f + 0.6f), vec3(4.f, 0.05f, 4.f), P.skyA, M_EMIT, 1.4f);
    st_ = Stamp{};
    // the shaft's balustrade round the opening, its lamps
    st_.born = S_SHAFT + 0.8f;
    for (int k = 0; k < 6; k++) {
        const SideFrame F = frameOf(k);
        obox(C + F.n * (HOLE + 0.06f) + UP * 0.5f, F.t, UP, F.in, vec3(HOLE * 0.5774f, 0.5f, 0.03f), P.wood, M_BALUSTRADE);
        obox(C + F.n * (HOLE + 0.06f) + UP * 1.02f, F.t, UP, F.in, vec3(HOLE * 0.5774f, 0.025f, 0.07f), P.wood * 0.8f, M_WOOD);
        obox(C + F.n * (HOLE + 0.02f) + UP * -0.2f, F.t, UP, F.in, vec3(HOLE * 0.5774f, 0.2f, 0.04f), P.wood, M_WOOD);
        const vec3 lp = C + cornerDir(k) * (HOLE / 0.866f + 0.15f);
        cyl(lp + UP * 0.8f, 0.04f, 0.8f, P.gilt, M_BRASS);
        sphere(lp + UP * 1.85f, 0.26f, P.lampCol, M_EMIT, 1.6f, 1.f);
        lamp(lp + UP * 1.85f, P.lampCol, 0.55f, 1.f, 8.f);
    }
}

// ---------------------------------------------------------------------------------------------------- a level of the shaft
// A whole level (12.4 m): the floor ring round the opening and its balustrade, three tiers of bookcases on the six
// walls with two galleries, lamps at the opening's corners; `full`: also a spiral stair.
void LibraryUniverse::buildLevel(const vec3& center, int level, float born, bool full, int shaft) {
    const Palette& P = palT_;
    const vec3 C = center + UP * (level * HL);
    st_ = Stamp{};
    st_.born = born;
    st_.style = 0.f;
    for (int k = 0; k < 6; k++) {
        const SideFrame F = frameOf(k);
        obox(C + F.n * ((HOLE + A) * 0.5f) + UP * -0.15f, F.t, UP, F.in, vec3(R * 0.5f, 0.15f, (A - HOLE) * 0.5f), P.floor, M_FLOOR);
        obox(C + F.n * (A + 0.2f) + UP * (HL * 0.5f), F.t, UP, F.in, vec3(R * 0.5f + 0.3f, HL * 0.5f, 0.2f), P.plaster, M_PLASTER);
        obox(C + F.n * (HOLE + 0.06f) + UP * 0.5f, F.t, UP, F.in, vec3(HOLE * 0.5774f, 0.5f, 0.03f), P.wood, M_BALUSTRADE);
        obox(C + F.n * (HOLE + 0.06f) + UP * 1.02f, F.t, UP, F.in, vec3(HOLE * 0.5774f, 0.025f, 0.07f), P.wood * 0.8f, M_WOOD);
        obox(C + F.n * (HOLE + 0.02f) + UP * -0.2f, F.t, UP, F.in, vec3(HOLE * 0.5774f, 0.2f, 0.04f), P.wood, M_WOOD);
        for (int t = 0; t < 3; t++) {
            const float y = tierY(t);
            const float len = R - 2.f * CASE_D * 0.5774f - 0.02f;
            const vec3 c = C + F.n * (A - CASE_D * 0.5f) + UP * (y + TIER_H * 0.5f);
            const Stamp keep = st_;
            st_.unit = (float)unit(c);
            obox(c, F.t, UP, F.in, vec3(len * 0.5f, TIER_H * 0.5f, CASE_D * 0.5f), P.wood, M_BOOKWALL, BAY_W, (float)BAY_SHELVES, 0.f,
                 1.3f * k + 7.1f * t + level * 3.3f + shaft * 11.7f);
            st_ = keep;
            if (t > 0) {
                const float a0 = GAL_IN, a1 = A - CASE_D;
                obox(C + F.n * ((a0 + a1) * 0.5f) + UP * (y - 0.1f), F.t, UP, F.in, vec3(a1 * 0.5774f, 0.1f, (a1 - a0) * 0.5f), P.wood * 0.9f, M_WOOD);
                obox(C + F.n * (a0 + 0.06f) + UP * (y + 0.5f), F.t, UP, F.in, vec3(a0 * 0.5774f, 0.5f, 0.03f), P.wood * 1.05f, M_BALUSTRADE);
                obox(C + F.n * (a0 + 0.06f) + UP * (y + 1.02f), F.t, UP, F.in, vec3(a0 * 0.5774f, 0.025f, 0.06f), P.wood * 0.8f, M_WOOD);
                obox(C + F.n * (a0 + 0.03f) + UP * (y - 0.16f), F.t, UP, F.in, vec3(a0 * 0.5774f, 0.16f, 0.04f), P.wood, M_WOOD);
            }
        }
        const vec3 lp = C + cornerDir(k) * (HOLE / 0.866f + 0.15f);
        cyl(lp + UP * 0.8f, 0.04f, 0.8f, P.gilt, M_BRASS);
        sphere(lp + UP * 1.85f, 0.26f, P.lampCol, M_EMIT, 1.6f, 1.f);
        if (full || (k % 2 == 0)) lamp(lp + UP * 1.85f, P.lampCol, 0.55f, 1.f, 8.f);
    }
    if (full) {
        // a spiral stair in the opening's corner, up to the next level
        const vec3 sc = C + cornerDir(4) * (HOLE / 0.866f - 1.9f);
        cyl(sc + UP * (HL * 0.5f), 0.1f, HL * 0.5f, P.wood, M_WOOD);
        const int n = 44;
        for (int i = 0; i < n; i++) {
            const float a = i * 0.36f + level * 0.7f;
            const vec3 u(std::cos(a), 0.f, std::sin(a));
            const vec3 w = cross(u, UP);
            const float y = (i + 1) * (HL / (n + 1));
            obox(sc + u * 0.75f + UP * y, u, UP, w, vec3(0.62f, 0.03f, 0.18f), P.wood * 1.05f, M_WOOD);
            if (i % 2 == 0)
                put(MESH_CYL, sc + u * 1.33f + UP * (y + 0.45f), vec3(0.012f, 0, 0), vec3(0, 0.45f, 0), vec3(0, 0, 0.012f), P.gilt * 0.8f, M_BRASS);
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
    // the collection's bookcases: bay 0 is the study's; the others are made as they are needed (newBay)
    // (the six to the right of the study's stand empty from the room's opening, waiting for their books)
    while (bays_.size() < 7) newBay();
    for (int i = 0; i < (int)bays_.size(); i++) {
        Bay& b = bays_[i];
        const Stamp keep = st_;
        st_ = Stamp{};
        st_.born = std::max(0.f, b.born);
        b.firstShelf = (int)shelves_.size();
        // (geometry drawn each frame with the dynamic parts; the shelves recorded here)
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
        st_ = keep;
    }
    buildStudy();
    buildHall();
    buildRotunda();
    // the shaft: levels below and above the reading room
    const vec3 C(0.f, 0.f, ZC);
    for (int n = 1; n <= 16; n++) {
        buildLevel(C, -n, S_SHAFT + 1.4f + 0.95f * (n - 1), n <= 3, 0);
        buildLevel(C, n, S_SHAFT + 1.9f + 0.95f * (n - 1), n <= 2, 0);
    }
    // the honeycomb: shafts along the section's row, farther and farther
    const vec3 dir = sideN(1) * (2.f * A);
    for (int m = 1; m <= 4; m++) {
        const int nLev = m == 1 ? 12 : 16;
        const float base = m == 1 ? S_HONEY + 0.5f : m == 2 ? S_HONEY + 10.f : m == 3 ? S_ENDLESS - 5.f : S_ENDLESS + 15.f;
        for (int s = -1; s <= 1; s += 2) {
            const vec3 sc = C + dir * (float)(s * m);
            for (int n = -nLev; n <= nLev; n++) buildLevel(sc, n, base + 0.28f * std::abs(n) + (s > 0 ? 0.f : 0.6f), false, m * 2 + (s > 0));
        }
    }
    // the sort units, nearest to the collection first
    const vec3 anchor(5.5f, 1.8f, ZBACK);
    for (Unit& u : units_) {
        const vec3 d = u.c - anchor;
        // (in the room, along the walls; in the shaft, level by level up and down, across the whole honeycomb)
        const float lateral = std::sqrt(d.x * d.x + d.z * d.z);
        u.order = u.born < S_SHAFT ? lateral + std::fabs(d.y) * 2.f + u.born * 0.4f
                                   : S_SHAFT * 2.f + std::fabs(d.y) * 1.2f + lateral * 0.12f;
    }
    if ((int)sortTimes_.size() < (int)units_.size()) sortTimes_.resize(units_.size(), -1.f);
    (void)P;
    (void)hashf;
    worldDirty_ = false;
}

// A new bookcase for the collection: along the back wall to the right of the study's, then over the fillers on its left.
void LibraryUniverse::newBay() {
    const int i = (int)bays_.size();
    Bay b;
    b.along = vec3(1, 0, 0);
    b.out = vec3(0, 0, 1);
    if (i < 7) b.c = vec3(2.9f + i * BAY_W, 0.f, ZBACK + CASE_D);
    else if (i < 13) {
        b.c = vec3(-3.95f - (i - 7) * BAY_W, 0.f, ZBACK + CASE_D);
        b.filler = i - 7;
    } else {
        // beyond: up on the first gallery, over the back wall's middle (rare: some 210 lines)
        const int j = i - 13;
        b.c = vec3(-9.f + (j % 18) * BAY_W, 4.1f + 4.1f * (j / 18 % 2), ZBACK + CASE_D + 0.02f);
    }
    b.born = i == 0 ? 0.f : i < 7 ? S_ROOM + 0.6f + 0.45f * i : grow_;
    bays_.push_back(b);
}
