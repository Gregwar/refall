// LIBRARY: building the room (see library.cpp), a cosy diorama. It is built once per variant: the slab, the walls cut
// in section, the two big bookcases, the round windows, the reading nook, the big plant; and, hidden until the
// collection needs them (each grows in from its floor), the ladder, the second bookcases, the mezzanine with its
// bookcases. The runs are the places the books go, in order.
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
    in.e[0] = stFeat_; in.e[1] = stPivot_; in.e[2] = stLevel_; in.e[3] = 0.f;
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

namespace {
// A bookcase on an angled wall: from t0 to t1 along it, standing at height y0.
struct CaseSpec { int side; float t0, t1, y0; int shelves; };
constexpr CaseSpec MAIN_CASE[2] = {{1, CASE_T0, CASE_T1, 0.f, MAIN_SHELVES}, {-1, CASE_T0, CASE_T1, 0.f, MAIN_SHELVES}};
constexpr CaseSpec LOW_CASE[2] = {{1, 0.12f, 0.44f, 0.f, MAIN_SHELVES}, {-1, 0.12f, 0.44f, 0.f, MAIN_SHELVES}};
constexpr CaseSpec MEZZ_CASE[2] = {{1, 0.5f, 0.93f, MEZZ_Y, MEZZ_SHELVES}, {-1, 0.5f, 0.93f, MEZZ_Y, MEZZ_SHELVES}};
constexpr float SIDE_T = 0.1f; // a bookcase's sides
} // namespace

// The places the books go, in order: the two big bookcases shelf by shelf from the bottom (right, left, alternating);
// piles on the floor in front of them; the mezzanine's bookcases and the second bookcases at the back, in turn.
void LibraryUniverse::buildRuns() {
    runs_.clear();
    auto shelf = [&](const CaseSpec& cs, int j, int level, int feat) {
        const WallFrame w = wallAt(cs.side, cs.t1);
        Run r;
        r.along = w.along * -1.f;
        r.out = w.out;
        r.start = w.p - w.along * SIDE_T + w.out * CASE_D + UP * (cs.y0 + SHELF_Y0 + j * SHELF_PITCH);
        r.len = (cs.t1 - cs.t0) * w.len - 2.f * SIDE_T;
        r.kind = 0;
        r.level = level;
        r.feat = feat;
        r.maxH = SHELF_PITCH - BOARD - 0.04f;
        runs_.push_back(r);
    };
    for (int j = 0; j < MAIN_SHELVES; j++)
        for (const CaseSpec& cs : MAIN_CASE) shelf(cs, j, 0, j == MAIN_SHELVES - 1 ? F_LADDER : -1);
    for (int s = 1; s >= -1; s -= 2) {
        const WallFrame w = wallAt(s, 0.93f);
        Run r;
        r.along = w.along * -1.f;
        r.out = w.out;
        r.start = w.p + w.out * (CASE_D + 1.3f);
        r.len = 2.f;
        r.kind = 1;
        r.level = -1;
        r.feat = F_PILES;
        runs_.push_back(r);
    }
    // then the mezzanine's shelves and the back bookcases' in turn, so both keep growing to the end
    for (const CaseSpec& cs : MEZZ_CASE) shelf(cs, 0, 1, F_MEZZ);
    for (const CaseSpec& cs : LOW_CASE) shelf(cs, 0, 0, F_LOW);
    for (const CaseSpec& cs : MEZZ_CASE) shelf(cs, 1, 1, F_MEZZ);
    for (int j = 1; j < MAIN_SHELVES; j++)
        for (const CaseSpec& cs : LOW_CASE) shelf(cs, j, 0, F_LOW);
}

void LibraryUniverse::buildWorld() {
    for (auto& v : stat_) v.clear();
    glass_.clear();
    lamps_.clear();
    const Palette& P = pal_;
    stLevel_ = 0.f;
    stFeat_ = -1.f;
    stPivot_ = 0.f;
    // an oriented box: center, axes (unit), half sizes
    auto obox = [&](const vec3& c, const vec3& u, const vec3& v, const vec3& w, const vec3& h, const vec3& col, int mat, float p0 = 0.f,
                    float p1 = 0.f, float p2 = 0.f, float seed = 0.f) { put(MESH_BOX, c, u * h.x, v * h.y, w * h.z, col, mat, p0, p1, p2, seed); };
    auto feature = [&](int f, float pivot) {
        stFeat_ = (float)f;
        stPivot_ = pivot;
    };
    auto noFeature = [&]() { stFeat_ = -1.f; stPivot_ = 0.f; };

    // ---- the backdrop (the variant's sky), the diorama's slab: a plank floor on a thick base cut in section
    box(vec3(0.f, 8.f, -19.f), vec3(80.f, 40.f, 0.1f), vec3(0.f), M_BACKDROP);
    const float zb = BACK_Z - 0.7f, zc = (zb + BASE_FRONT) * 0.5f, hz = (BASE_FRONT - zb) * 0.5f;
    box(vec3(0.f, -0.02f, zc), vec3(10.4f, 0.02f, hz), P.floor, M_FLOOR, P.floorKind);
    box(vec3(0.f, -0.04f - BASE_T * 0.5f, zc), vec3(10.4f, BASE_T * 0.5f, hz), P.cut, M_PLASTER);

    // ---- the walls: thick, cut in section at the top and the front ends; a wainscot and a rail
    box(vec3(0.f, WALL_H * 0.5f, BACK_Z - WALL_T * 0.5f), vec3(BACK_HALF + 0.7f, WALL_H * 0.5f, WALL_T * 0.5f), P.wall, M_PLASTER);
    box(vec3(0.f, WALL_H + 0.03f, BACK_Z - WALL_T * 0.5f), vec3(BACK_HALF + 0.7f, 0.03f, WALL_T * 0.5f + 0.02f), P.cut, M_PLASTER);
    box(vec3(0.f, 0.55f, BACK_Z + 0.02f), vec3(BACK_HALF, 0.55f, 0.02f), P.wall2, M_PLASTER);
    box(vec3(0.f, 1.12f, BACK_Z + 0.04f), vec3(BACK_HALF, 0.04f, 0.04f), P.oak, M_WOOD);
    for (int s = -1; s <= 1; s += 2) {
        const WallFrame w0 = wallAt(s, 0.f), w1 = wallAt(s, 1.f);
        const vec3 a = w0.p - w0.along * 0.7f, b = w1.p + w0.along * 0.3f;
        const vec3 mid = (a + b) * 0.5f - w0.out * (WALL_T * 0.5f);
        const float hl = length(b - a) * 0.5f;
        obox(mid + UP * (WALL_H * 0.5f), w0.along, UP, w0.out, vec3(hl, WALL_H * 0.5f, WALL_T * 0.5f), P.wall, M_PLASTER);
        obox(mid + UP * (WALL_H + 0.03f), w0.along, UP, w0.out, vec3(hl, 0.03f, WALL_T * 0.5f + 0.02f), P.cut, M_PLASTER);
        obox(b - w0.out * (WALL_T * 0.5f) + UP * (WALL_H * 0.5f) + w0.along * 0.012f, w0.along, UP, w0.out,
             vec3(0.012f, WALL_H * 0.5f + 0.03f, WALL_T * 0.5f + 0.02f), P.cut, M_PLASTER);
        const vec3 inner = (w0.p + w1.p) * 0.5f;
        obox(inner + w0.out * 0.02f + UP * 0.55f, w0.along, UP, w0.out, vec3(w0.len * 0.5f, 0.55f, 0.02f), P.wall2, M_PLASTER);
        obox(inner + w0.out * 0.04f + UP * 1.12f, w0.along, UP, w0.out, vec3(w0.len * 0.5f, 0.04f, 0.04f), P.oak, M_WOOD);
    }

    // ---- bookcases: chunky painted wood, a calm back, thick boards and a crown
    auto bookcase = [&](const CaseSpec& cs, float sd) {
        const WallFrame w = wallAt(cs.side, (cs.t0 + cs.t1) * 0.5f);
        const float W = (cs.t1 - cs.t0) * w.len;
        const float H = SHELF_Y0 + cs.shelves * SHELF_PITCH + 0.1f;
        const vec3 base = w.p + UP * cs.y0;
        const vec3 mid = base + w.out * (CASE_D * 0.5f);
        auto ob = [&](const vec3& c, const vec3& h, const vec3& col, int mat, float p0 = 0.f, float p1 = 0.f) {
            obox(c, w.along, UP, w.out, h, col, mat, p0, p1, 0.f, sd);
        };
        ob(base + w.out * 0.03f + UP * (H * 0.5f), vec3(W * 0.5f, H * 0.5f, 0.03f), P.bay, M_CASE, SHELF_PITCH, SHELF_Y0);
        for (int e = -1; e <= 1; e += 2)
            ob(mid + w.along * (e * (W * 0.5f - SIDE_T * 0.5f)) + UP * (H * 0.5f), vec3(SIDE_T * 0.5f, H * 0.5f, CASE_D * 0.5f), P.oak, M_WOOD);
        const float innerH = W * 0.5f - SIDE_T;
        ob(mid + UP * (SHELF_Y0 * 0.5f), vec3(innerH, SHELF_Y0 * 0.5f, CASE_D * 0.5f), P.oak * 0.85f, M_WOOD);
        for (int j = 1; j < cs.shelves; j++)
            ob(mid + UP * (SHELF_Y0 + j * SHELF_PITCH - BOARD * 0.5f), vec3(innerH, BOARD * 0.5f, CASE_D * 0.5f - 0.01f), P.oak, M_WOOD);
        ob(mid + UP * (H - 0.04f) + w.out * 0.03f, vec3(W * 0.5f + 0.07f, 0.07f, CASE_D * 0.5f + 0.05f), P.oak, M_WOOD);
    };
    for (const CaseSpec& cs : MAIN_CASE) bookcase(cs, 1.f + cs.side);

    // ---- round windows high on the angled walls, near the back: the sky through a cross of bars, a thick frame
    for (int s = -1; s <= 1; s += 2) {
        const WallFrame w = wallAt(s, 0.18f);
        const vec3 c = w.p + UP * 4.75f;
        const float r = 0.78f;
        put(MESH_CYL, c + w.out * 0.05f, w.along * (r + 0.16f), w.out * 0.05f, UP * (r + 0.16f), P.oak, M_TOY);
        put(MESH_CYL, c + w.out * 0.08f, w.along * r, w.out * 0.05f, UP * r, P.oak * 0.9f, M_ROUNDWIN, 0.f, 0.f, 0.f, 0.3f * s);
        // a sconce between the window and the bookcases: a round globe
        const WallFrame w2 = wallAt(s, 0.47f);
        cyl(w2.p + w2.out * 0.12f + UP * 3.45f, 0.03f, 0.12f, P.steel, M_STEEL);
        sphere(w2.p + w2.out * 0.2f + UP * 3.62f, 0.16f, P.led, 1.4f);
        lamp(w2.p + w2.out * 0.3f + UP * 3.62f, P.led, 0.16f, 0.5f, 3.6f, 0);
    }

    // ---- the reading nook at the front left: an oval rug, a chunky armchair, a floor lamp, a round side table
    {
        const vec3 rc(-6.4f, 0.f, 2.3f);
        put(MESH_CYL, rc + UP * 0.012f, vec3(1.75f, 0, 0), vec3(0, 0.012f, 0), vec3(0, 0, 1.15f), P.rug, M_TOY);
        put(MESH_CYL, rc + UP * 0.016f, vec3(1.3f, 0, 0), vec3(0, 0.014f, 0), vec3(0, 0, 0.85f), P.rug2, M_TOY);
        put(MESH_CYL, rc + UP * 0.02f, vec3(0.85f, 0, 0), vec3(0, 0.016f, 0), vec3(0, 0, 0.55f), P.rug, M_TOY);
        const vec3 f = normalize(vec3(0.5f, 0.f, 0.86f)), sd = normalize(cross(UP, f));
        const vec3 ac(-6.8f, 0.f, 1.9f);
        auto ab = [&](const vec3& c, const vec3& h, const vec3& col) { obox(c, sd, UP, f, h, col, M_TOY); };
        ab(ac + UP * 0.32f, vec3(0.62f, 0.2f, 0.52f), P.chair);
        ab(ac + UP * 0.58f + f * 0.04f, vec3(0.44f, 0.08f, 0.44f), lerp(P.chair, vec3(1.f), 0.25f));
        ab(ac - f * 0.44f + UP * 0.86f, vec3(0.62f, 0.52f, 0.14f), P.chair);
        for (int e = -1; e <= 1; e += 2) ab(ac + sd * (e * 0.56f) + UP * 0.62f, vec3(0.12f, 0.2f, 0.52f), P.chair * 0.92f);
        for (int e = -1; e <= 1; e += 2)
            for (int g = -1; g <= 1; g += 2) cyl(ac + sd * (e * 0.5f) + f * (g * 0.4f) + UP * 0.06f, 0.05f, 0.06f, P.oak, M_WOOD);
        // the floor lamp
        const vec3 lp(-8.1f, 0.f, 1.5f);
        cyl(lp + UP * 0.03f, 0.26f, 0.03f, P.steel, M_STEEL);
        cyl(lp + UP * 1.0f, 0.035f, 0.97f, P.steel, M_STEEL);
        sphere(lp + UP * 1.98f, 0.15f, P.led, 2.f);
        put(MESH_DOME, lp + UP * 2.02f, vec3(0.42f, 0, 0), vec3(0, 0.32f, 0), vec3(0, 0, 0.42f), P.rug2, M_TOY);
        lamp(lp + UP * 1.9f, P.led, 0.2f, 0.7f, 4.5f, 0);
        // the side table, a book on it
        const vec3 tp(-5.0f, 0.f, 2.9f);
        cyl(tp + UP * 0.28f, 0.06f, 0.28f, P.oak, M_WOOD);
        cyl(tp + UP * 0.58f, 0.38f, 0.035f, P.oak, M_TOY);
        box(tp + UP * 0.67f, vec3(0.2f, 0.05f, 0.14f), P.rug, M_TOY);
    }
    // ---- the big plant at the front right, a round pouf
    auto plant = [&](const vec3& b, float k, float seed) {
        cyl(b + UP * (0.3f * k), 0.42f * k, 0.3f * k, P.pot, M_TOY);
        cyl(b + UP * (0.62f * k), 0.47f * k, 0.05f * k, P.pot * 1.1f, M_TOY);
        const float a = seed * 2.4f;
        const vec3 offs[5] = {vec3(0.f, 1.3f, 0.f), vec3(std::cos(a) * 0.45f, 1.05f, std::sin(a) * 0.3f),
                              vec3(-std::cos(a + 0.9f) * 0.42f, 1.0f, -std::sin(a + 0.9f) * 0.3f), vec3(0.12f, 1.8f, -0.1f),
                              vec3(-0.28f, 1.6f, 0.25f)};
        const float rad[5] = {0.68f, 0.5f, 0.5f, 0.48f, 0.42f};
        for (int j = 0; j < 5; j++) sphere(b + offs[j] * k, rad[j] * k, P.leaf * (j == 0 ? 1.f : 0.88f + 0.06f * j), 0.f);
    };
    plant(vec3(7.9f, 0.f, 2.3f), 1.f, 1.f);
    put(MESH_CYL, vec3(6.3f, 0.22f, 2.9f), vec3(0.42f, 0, 0), vec3(0, 0.22f, 0), vec3(0, 0, 0.42f), P.rug2, M_TOY);

    // ---- the ladder (once the top shelves are in use), leaning on the wall just in front of the bookcases
    feature(F_LADDER, 0.f);
    {
        const WallFrame w = wallAt(1, 0.985f);
        const vec3 foot = w.p + w.out * 1.0f, top = w.p + w.out * 0.18f + UP * 4.5f;
        const vec3 dir = normalize(top - foot), nrm = normalize(cross(w.along, dir));
        const float L = length(top - foot);
        for (int e = -1; e <= 1; e += 2) obox((foot + top) * 0.5f + w.along * (e * 0.3f), w.along, dir, nrm, vec3(0.045f, L * 0.5f, 0.045f), P.oak, M_WOOD);
        for (float u = 0.3f; u < L - 0.1f; u += 0.38f) obox(foot + dir * u, w.along, dir, nrm, vec3(0.28f, 0.03f, 0.03f), P.oak * 0.9f, M_WOOD);
    }
    // ---- the mezzanine along both angled walls, on round columns, a low rail, its bookcases, a lamp and a plant
    feature(F_MEZZ, MEZZ_Y);
    stLevel_ = 1.f;
    for (int s = -1; s <= 1; s += 2) {
        const WallFrame w0 = wallAt(s, MEZZ_T0), w1 = wallAt(s, 1.f);
        const vec3 a = w0.p, b = w1.p, mid = (a + b) * 0.5f;
        const float hl = length(b - a) * 0.5f;
        obox(mid + w0.out * (MEZZ_D * 0.5f) + UP * (MEZZ_Y - 0.16f), w0.along, UP, w0.out, vec3(hl, 0.16f, MEZZ_D * 0.5f), P.oak, M_WOOD);
        obox(mid + w0.out * (MEZZ_D * 0.5f) + UP * (MEZZ_Y - 0.33f), w0.along, UP, w0.out, vec3(hl - 0.05f, 0.03f, MEZZ_D * 0.5f - 0.05f),
             P.cut, M_PLASTER);
        // the rail: chunky posts and a top rail at its edge
        const vec3 edge = w0.out * (MEZZ_D - 0.08f);
        for (float u = 0.f; u <= hl * 2.f + 0.01f; u += hl * 2.f / 7.f)
            obox(a + w0.along * u + edge + UP * (MEZZ_Y + 0.3f), w0.along, UP, w0.out, vec3(0.05f, 0.3f, 0.05f), P.oak, M_WOOD);
        obox(mid + edge + UP * (MEZZ_Y + 0.62f), w0.along, UP, w0.out, vec3(hl, 0.05f, 0.07f), P.oak, M_WOOD);
        obox(mid + edge + UP * (MEZZ_Y + 0.3f), w0.along, UP, w0.out, vec3(hl, 0.025f, 0.03f), P.oak * 0.9f, M_WOOD);
        stPivot_ = 0.f;
        for (float t : {MEZZ_T0 + 0.03f, 0.97f}) {
            const WallFrame wc = wallAt(s, t);
            cyl(wc.p + wc.out * (MEZZ_D - 0.2f) + UP * ((MEZZ_Y - 0.3f) * 0.5f), 0.12f, (MEZZ_Y - 0.3f) * 0.5f, P.oak, M_WOOD);
        }
        stPivot_ = MEZZ_Y;
        // a globe on a post at its front end, a little plant at its back end
        const WallFrame wl = wallAt(s, 0.975f);
        cyl(wl.p + wl.out * 0.9f + UP * (MEZZ_Y + 0.55f), 0.035f, 0.55f, P.steel, M_STEEL);
        sphere(wl.p + wl.out * 0.9f + UP * (MEZZ_Y + 1.22f), 0.17f, P.led, 1.6f);
        lamp(wl.p + wl.out * 0.9f + UP * (MEZZ_Y + 1.1f), P.led, 0.18f, 0.6f, 4.f, 1);
        plant(wallAt(s, 0.37f).p + wallAt(s, 0.37f).out * 0.7f + UP * MEZZ_Y, 0.45f, 2.f + s);
    }
    for (const CaseSpec& cs : MEZZ_CASE) bookcase(cs, 3.f + cs.side);
    stLevel_ = 0.f;
    // ---- the second bookcases at the back of the angled walls
    feature(F_LOW, 0.f);
    for (const CaseSpec& cs : LOW_CASE) bookcase(cs, 5.f + cs.side);
    noFeature();
    worldDirty_ = false;
}
