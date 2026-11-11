// ARCHITECT's building site: the villa's program, element by element, and the scenery (see site.hpp).
//
// The villa (meters, ground y = 0): a ground floor of three rooms side by side, the living room on the left (its
// front fully glazed onto the pool), the kitchen in the middle (behind the well), the guest wing on the right
// (glazed, a canopy cantilevered over its deck, a roof terrace with a pergola on it); upstairs, the master suite
// cantilevers out over the pool's deck on the left (glazed in front, clad on its side), and a long box over the
// kitchen. Two free-standing stone walls frame it, left and right. The order: the field's stakes, the levelled
// site, the slabs poured, the cranes, the rooms lowered one by one (ground floor, the cantilevered slab, upstairs,
// the roofs), the glazing, the cladding, the canopy, the roof terrace, the pool (dug, tiled, filled), the garden
// (lawn, decks, trees, loungers, lamps).
#include "universes/architect/site.hpp"

#include <algorithm>

namespace arch_site {

const float FOOTPRINTS[][4] = {
    {-21.6f, -7.6f, 18.6f, 3.4f},   // the ground floor
    {-24.5f, -5.5f, -10.5f, 6.5f},  // the cantilever's shadow on the ground
    {-25.6f, 6.4f, -3.4f, 16.4f},   // the pool's deck
    {4.6f, 1.8f, 18.4f, 6.4f},      // the front deck
};
const int FOOTPRINT_COUNT = 4;

void Element::finish() {
    lo = vec3(1e9f, 1e9f, 1e9f);
    hi = vec3(-1e9f, -1e9f, -1e9f);
    for (const Part& p : parts) {
        // (rotated parts: their bounding sphere is enough here)
        const float r = (std::fabs(p.q.x) + std::fabs(p.q.y) + std::fabs(p.q.z)) > 1e-4f ? length(p.s) * 0.5f : 0.f;
        const vec3 h = r > 0.f ? vec3(r, r, r) : p.s * 0.5f;
        lo = vec3(std::min(lo.x, p.c.x - h.x), std::min(lo.y, p.c.y - h.y), std::min(lo.z, p.c.z - h.z));
        hi = vec3(std::max(hi.x, p.c.x + h.x), std::max(hi.y, p.c.y + h.y), std::max(hi.z, p.c.z + h.z));
    }
    anchor = vec3((lo.x + hi.x) * 0.5f, hi.y, (lo.z + hi.z) * 0.5f);
}

namespace {

constexpr float SLAB = 0.45f;          // the ground slab's top
constexpr float GF_H = 3.4f;           // a storey
constexpr float GF_TOP = SLAB + GF_H;  // 3.85: the ground floor's ceiling
constexpr float DECK_T = 0.4f;         // a floor slab
constexpr float FF = GF_TOP + DECK_T;  // 4.25: upstairs' floor
constexpr float FF_TOP = FF + 3.2f;    // 7.45
constexpr float WALL = 0.3f;

float hashf(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (x & 0xFFFFFF) / 16777216.f;
}

Part box(float x0, float y0, float z0, float x1, float y1, float z1, int mat, const vec3& col, float param = 0.f) {
    Part p;
    p.c = vec3((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f);
    p.s = vec3(std::fabs(x1 - x0), std::fabs(y1 - y0), std::fabs(z1 - z0));
    p.mat = mat;
    p.col = col;
    p.param = param;
    return p;
}
Part shape(int mesh, const vec3& c, const vec3& s, int mat, const vec3& col, const Q& q = Q{}) {
    Part p;
    p.c = c;
    p.s = s;
    p.q = q;
    p.mesh = mesh;
    p.mat = mat;
    p.col = col;
    return p;
}
// A rod from a to b (a cylinder or a box), of the given thickness.
Part rod(int mesh, const vec3& a, const vec3& b, float w, int mat, const vec3& col) {
    const vec3 d = b - a;
    return shape(mesh, (a + b) * 0.5f, vec3(w, length(d), w), mat, col, qFromY(d));
}

Element el(Kind k, float due, float dur = 1.f) {
    Element e;
    e.kind = k;
    e.due = due;
    e.dur = dur;
    return e;
}

// A room module lowered by a crane: its walls (back, the sides asked for) and its roof slab, open in front.
void shell(Element& e, const Palette& P, float x0, float x1, float z0, float z1, float y0, float h, bool left, bool right,
           bool roof, float overFront = 0.4f) {
    const int m = P.wallMat;
    const vec3 wc = P.wallMat == BOARDFORM ? P.concrete : P.render;
    e.parts.push_back(box(x0, y0, z0, x1, y0 + h, z0 + WALL, m, wc));
    if (left) e.parts.push_back(box(x0, y0, z0, x0 + WALL, y0 + h, z1, m, wc));
    if (right) e.parts.push_back(box(x1 - WALL, y0, z0, x1, y0 + h, z1, m, wc));
    if (roof) e.parts.push_back(box(x0 - 0.15f, y0 + h, z0 - 0.15f, x1 + 0.15f, y0 + h + DECK_T, z1 + overFront, RENDER, P.render2));
}

// A glazed front: one pane (the room seen behind it), slim frames, mullions every ~2.6 m. Facing +z (dir 0),
// -x (dir 1) or +x (dir 2).
void glazing(Element& e, const Palette& P, float a0, float a1, float at, float y0, float y1, float depth, int light, int dir = 0) {
    const float t = 0.06f, fw = 0.08f;
    auto B = [&](float u0, float v0, float u1, float v1, float d0, float d1, int mat, const vec3& col) {
        if (dir == 0) e.parts.push_back(box(u0, v0, at + d0, u1, v1, at + d1, mat, col));
        else {
            const float s = dir == 1 ? -1.f : 1.f;
            e.parts.push_back(box(at + s * d0, v0, u0, at + s * d1, v1, u1, mat, col));
        }
    };
    B(a0, y0, a1, y1, -t, 0.f, GLASS, P.glass);
    Part& g = e.parts.back();
    g.depth = depth;
    g.light = light;
    B(a0, y0, a1, y0 + fw, -t, fw * 0.6f, FRAME, P.frame);
    B(a0, y1 - fw, a1, y1, -t, fw * 0.6f, FRAME, P.frame);
    B(a0, y0, a0 + fw, y1, -t, fw * 0.6f, FRAME, P.frame);
    B(a1 - fw, y0, a1, y1, -t, fw * 0.6f, FRAME, P.frame);
    const int n = std::max(1, (int)std::round((a1 - a0) / 2.6f));
    for (int i = 1; i < n; i++) {
        const float u = a0 + (a1 - a0) * i / n;
        B(u - fw * 0.4f, y0, u + fw * 0.4f, y1, -t, fw * 0.5f, FRAME, P.frame);
    }
}

// ---------------------------------------------------------------- trees and plants (parts around a base point)
void olive(std::vector<Part>& o, const Palette& P, const vec3& b, float s, uint32_t seed) {
    const float a = hashf(seed) * 6.28f;
    const vec3 lean(std::cos(a) * 0.5f, 1.f, std::sin(a) * 0.5f);
    const vec3 k1 = b + vec3(lean.x * 0.6f, 1.3f, lean.z * 0.6f) * s;
    o.push_back(rod(CYL, b, k1, 0.42f * s, BARK, P.bark));
    o.push_back(rod(CYL, k1, k1 + vec3(-0.5f, 1.1f, 0.3f) * s, 0.26f * s, BARK, P.bark));
    o.push_back(rod(CYL, k1, k1 + vec3(0.6f, 1.0f, -0.3f) * s, 0.24f * s, BARK, P.bark));
    for (int i = 0; i < 6; i++) {
        const float u = hashf(seed * 7 + i), v = hashf(seed * 13 + i), w = hashf(seed * 29 + i);
        const vec3 c = k1 + vec3((u - 0.5f) * 3.0f, 1.3f + v * 1.2f, (w - 0.5f) * 2.6f) * s;
        const float r = (1.5f + 0.7f * v) * s;
        Part p = shape(SPHERE, c, vec3(r * 1.25f, r * 0.8f, r * 1.2f), FOLIAGE, i % 2 ? P.leaf : P.leaf2);
        p.param = u;
        o.push_back(p);
    }
}
void cypress(std::vector<Part>& o, const Palette& P, const vec3& b, float s, uint32_t seed) {
    o.push_back(rod(CYL, b, b + vec3(0, 1.0f * s, 0), 0.3f * s, BARK, P.bark));
    const vec3 dark = P.leaf2 * 0.55f;
    Part p = shape(SPHERE, b + vec3(0, 4.2f * s, 0), vec3(1.7f, 8.0f, 1.7f) * s, FOLIAGE, dark);
    p.param = hashf(seed);
    o.push_back(p);
    o.push_back(shape(SPHERE, b + vec3(0.15f, 6.4f * s, 0.1f), vec3(1.2f, 4.0f, 1.2f) * s, FOLIAGE, dark * 1.1f));
}
void umbrellaPine(std::vector<Part>& o, const Palette& P, const vec3& b, float s, uint32_t seed) {
    // a stone pine: a tall leaning trunk forking into a few limbs, a wide flat dome of clumps on them
    const float lean = (hashf(seed) - 0.5f) * 2.f;
    const vec3 fork = b + vec3(lean, 6.5f, 0.f) * s;
    o.push_back(rod(CYL, b, fork, 0.45f * s, BARK, P.bark));
    for (int i = 0; i < 9; i++) {
        const float u = hashf(seed * 3 + i), v = hashf(seed * 5 + i);
        const float a = 6.2831f * i / 9.f + u;
        const float rr = i == 0 ? 0.f : (1.6f + 2.4f * v);
        const vec3 c = fork + vec3(std::cos(a) * rr * 1.2f, 2.3f + (1.f - rr / 4.f) * 0.9f + u * 0.4f, std::sin(a) * rr * 0.9f) * s;
        if (i % 2 == 1) o.push_back(rod(CYL, fork, c - vec3(0, 0.5f * s, 0), 0.18f * s, BARK, P.bark));
        Part p = shape(SPHERE, c, vec3(3.0f + u, 1.5f + 0.4f * v, 2.8f + v) * s, FOLIAGE, (i % 3 ? P.leaf2 : P.leaf2 * 1.25f) * 0.85f);
        p.param = u;
        o.push_back(p);
    }
}
void fir(std::vector<Part>& o, const Palette& P, const vec3& b, float s, uint32_t seed) {
    o.push_back(rod(CYL, b, b + vec3(0, 2.f * s, 0), 0.35f * s, BARK, P.bark));
    const vec3 g = P.leaf2 * (0.55f + 0.2f * hashf(seed));
    for (int i = 0; i < 4; i++) {
        const float y = (1.3f + i * 2.1f) * s, r = (3.6f - i * 0.75f) * s;
        Part p = shape(CONE, b + vec3(0, y + 1.6f * s, 0), vec3(r, 3.6f * s, r), FOLIAGE, g * (1.f + 0.06f * i));
        p.q = qYaw(hashf(seed + i) * 6.f);
        o.push_back(p);
    }
}
void birch(std::vector<Part>& o, const Palette& P, const vec3& b, float s, uint32_t seed) {
    const vec3 top = b + vec3((hashf(seed) - 0.5f) * 0.8f, 6.f, 0.f) * s;
    o.push_back(rod(CYL, b, top, 0.28f * s, BARK, vec3(0.75f, 0.72f, 0.68f)));
    for (int i = 0; i < 5; i++) {
        const float u = hashf(seed * 3 + i), v = hashf(seed * 5 + i);
        Part p = shape(SPHERE, top + vec3((u - 0.5f) * 2.4f, -1.2f + v * 2.2f, (v - 0.5f) * 2.f) * s,
                       vec3(2.2f, 2.4f, 2.1f) * s * (0.8f + 0.3f * u), FOLIAGE, i % 2 ? P.leaf : P.leaf2);
        p.param = u;
        o.push_back(p);
    }
}
void palm(std::vector<Part>& o, const Palette& P, const vec3& b, float s, uint32_t seed) {
    const float a = hashf(seed) * 6.28f, bend = 0.12f + 0.12f * hashf(seed + 9);
    vec3 p0 = b;
    const int SEG = 6;
    const float H = 8.5f * s;
    vec3 dir(0, 1, 0);
    for (int i = 0; i < SEG; i++) {
        const float u = (i + 1.f) / SEG;
        const vec3 p1 = b + vec3(std::cos(a) * bend * u * u * H, u * H, std::sin(a) * bend * u * u * H);
        o.push_back(rod(CYL, p0, p1, (0.42f - 0.12f * u) * s, BARK, P.bark * (i % 2 ? 1.f : 0.9f)));
        dir = p1 - p0;
        p0 = p1;
    }
    const int F = 11;
    for (int i = 0; i < F; i++) {
        const float az = 6.2831f * i / F + hashf(seed * 3 + i) * 0.4f;
        const float up = 0.35f - 0.5f * hashf(seed * 7 + i);
        vec3 q0 = p0;
        for (int k = 0; k < 2; k++) {
            const float droop = up - 0.55f * k;
            const vec3 d(std::cos(az) * std::cos(droop), std::sin(droop), std::sin(az) * std::cos(droop));
            const float len = (k == 0 ? 2.4f : 2.2f) * s;
            Part f;
            f.mesh = BOX;
            f.mat = FROND;
            f.col = k == 0 ? P.leaf : P.leaf * 0.92f;
            f.s = vec3(len, 0.04f * s, 1.5f * s);
            f.c = q0 + d * (len * 0.5f);
            // the frond's length along +x, turned to d, its blade flat
            f.q = qMul(qYaw(-az), qAxis(vec3(0, 0, 1), droop));
            f.param = (float)k;
            o.push_back(f);
            q0 = q0 + d * len;
        }
    }
}
void bush(std::vector<Part>& o, const Palette& P, const vec3& b, float s, uint32_t seed) {
    for (int i = 0; i < 3; i++) {
        const float u = hashf(seed * 3 + i), v = hashf(seed * 11 + i);
        Part p = shape(SPHERE, b + vec3((u - 0.5f) * 1.6f, 0.6f + v * 0.3f, (v - 0.5f) * 1.2f) * s, vec3(1.7f, 1.3f, 1.6f) * s,
                       FOLIAGE, i % 2 ? P.leaf : P.leaf2);
        p.param = u;
        o.push_back(p);
    }
}
void cactus(std::vector<Part>& o, const Palette& P, const vec3& b, float s, uint32_t seed) {
    const float H = (4.f + 2.f * hashf(seed)) * s, r = 0.55f * s;
    const vec3 g = P.leaf;
    o.push_back(shape(CYL, b + vec3(0, H * 0.5f, 0), vec3(r, H, r), CACTUS, g));
    o.push_back(shape(SPHERE, b + vec3(0, H, 0), vec3(r, r, r), CACTUS, g));
    for (int k = 0; k < 2; k++) {
        if (hashf(seed + k * 31) < 0.3f) continue;
        const float sg = k ? 1.f : -1.f, y = H * (0.35f + 0.2f * hashf(seed + k));
        const float ah = H * (0.25f + 0.2f * hashf(seed + 7 * k));
        const vec3 e0 = b + vec3(sg * 1.1f * s, y, 0);
        o.push_back(rod(CYL, b + vec3(0, y, 0), e0, r * 0.75f, CACTUS, g));
        o.push_back(shape(CYL, e0 + vec3(0, ah * 0.5f, 0), vec3(r * 0.75f, ah, r * 0.75f), CACTUS, g));
        o.push_back(shape(SPHERE, e0 + vec3(0, ah, 0), vec3(r * 0.75f), CACTUS, g));
    }
}
void agave(std::vector<Part>& o, const Palette& P, const vec3& b, float s, uint32_t seed) {
    const vec3 g = mixc(P.leaf, vec3(0.45f, 0.55f, 0.55f), 0.5f);
    for (int i = 0; i < 9; i++) {
        const float az = 6.2831f * i / 9.f + hashf(seed + i);
        const float tilt = 0.5f + 0.5f * hashf(seed * 3 + i);
        const vec3 d(std::cos(az) * std::sin(tilt), std::cos(tilt), std::sin(az) * std::sin(tilt));
        o.push_back(shape(CONE, b + d * (0.75f * s), vec3(0.35f, 1.6f, 0.12f) * s, CACTUS, g, qMul(qFromY(d), qYaw(az))));
    }
}

void tree(std::vector<Part>& o, const Palette& P, int kind, const vec3& b, float s, uint32_t seed) {
    switch (kind) {
    case 0: olive(o, P, b, s, seed); break;
    case 1: cypress(o, P, b, s, seed); break;
    case 2: umbrellaPine(o, P, b, s, seed); break;
    case 3: fir(o, P, b, s, seed); break;
    case 4: birch(o, P, b, s, seed); break;
    case 5: palm(o, P, b, s, seed); break;
    case 6: bush(o, P, b, s, seed); break;
    case 7: cactus(o, P, b, s, seed); break;
    default: agave(o, P, b, s, seed); break;
    }
}

} // namespace

// ---------------------------------------------------------------- the program
std::vector<Element> program(const Palette& P, uint64_t seed) {
    std::vector<Element> E;
    const uint32_t sd = (uint32_t)(seed % 100000u);
    const vec3 white = P.render, roofC = P.render2;

    // ---- the field: the surveyor's stakes at the footprint's corners, their heads painted (the lines between
    // them are sprayed on the grass, see the ground's shader)
    {
        Element e = el(GROW, -1.f, 0.1f);
        e.gone = L_SLAB + 0.6f;
        const vec3 stake(0.62f, 0.45f, 0.28f), paint(0.95f, 0.36f, 0.08f);
        const float pts[10][2] = {{-21.f, -7.f}, {-21.f, 3.f}, {-8.f, 3.f}, {-8.f, -7.f}, {5.f, -7.f}, {5.f, 2.f},
                                  {18.f, -6.f}, {18.f, 2.f}, {-24.f, -5.f}, {-24.f, 6.f}};
        for (auto& p : pts) {
            const float x = p[0], z = p[1];
            e.parts.push_back(box(x - 0.05f, 0.f, z - 0.05f, x + 0.05f, 1.0f, z + 0.05f, WOOD, stake));
            e.parts.push_back(box(x - 0.055f, 0.85f, z - 0.055f, x + 0.055f, 1.02f, z + 0.055f, FABRIC, paint));
        }
        // the surveyor's level on its tripod, by the future pool
        const vec3 head(-15.f, 1.45f, 9.f);
        for (int k = 0; k < 3; k++) {
            const float a = 6.2831f * k / 3.f + 0.4f;
            e.parts.push_back(rod(CYL, head, vec3(head.x + std::cos(a) * 0.55f, 0.f, head.z + std::sin(a) * 0.55f), 0.05f, WOOD, stake));
        }
        e.parts.push_back(box(head.x - 0.14f, head.y, head.z - 0.2f, head.x + 0.14f, head.y + 0.22f, head.z + 0.2f, STEEL, vec3(0.9f, 0.62f, 0.08f)));
        e.parts.push_back(shape(CYL, head + vec3(0, 0.3f, 0), vec3(0.08f, 0.12f, 0.08f), FRAME, P.frame));
        E.push_back(e);
    }
    // ---- the slabs poured (ground floor, kitchen, guest wing)
    {
        const float sl[3][4] = {{-21.3f, -6.3f, -8.f, 3.3f}, {-8.f, -7.3f, 5.f, 0.3f}, {5.f, -6.3f, 18.3f, 2.3f}};
        for (int i = 0; i < 3; i++) {
            Element e = el(GROW, L_SLAB + 0.8f * i, 1.1f);
            e.parts.push_back(box(sl[i][0], 0.f, sl[i][1], sl[i][2], SLAB, sl[i][3], CONCRETE, P.concrete));
            E.push_back(e);
        }
    }
    // ---- the cement mixer, parked at the front right while the walls go up
    {
        Element e = el(DRIVE, 1.4f, 1.6f);
        e.gone = 26.f;
        e.goneDur = 1.6f;
        const vec3 base(25.f, 0.f, 5.f);
        const vec3 cab(0.92f, 0.92f, 0.9f), drum = P.crane, dark(0.08f, 0.08f, 0.09f);
        e.parts.push_back(box(base.x - 3.6f, 0.55f, base.z - 1.1f, base.x + 3.6f, 1.0f, base.z + 1.1f, STEEL, dark));
        e.parts.push_back(box(base.x - 3.6f, 1.0f, base.z - 1.15f, base.x - 1.6f, 3.1f, base.z + 1.15f, STEEL, cab));
        e.parts.push_back(box(base.x - 3.62f, 2.0f, base.z - 1.0f, base.x - 3.5f, 2.9f, base.z + 1.0f, FRAME, vec3(0.05f, 0.07f, 0.09f)));
        for (float x : {-2.7f, 0.4f, 2.0f})
            for (float z : {-1.f, 1.f}) {
                Part w = shape(CYL, base + vec3(x, 0.55f, z * 1.05f), vec3(1.1f, 0.38f, 1.1f), STEEL, dark, qAxis(vec3(1, 0, 0), 1.5708f));
                e.parts.push_back(w);
            }
        // the drum: tilted, turning (the renderer turns the parts marked param 9)
        Part d = shape(CYL, base + vec3(1.0f, 2.35f, 0.f), vec3(2.0f, 4.6f, 2.0f), STEEL, drum, qAxis(vec3(0, 0, 1), 1.30f));
        d.param = 9.f;
        e.parts.push_back(d);
        Part d2 = shape(CONE, base + vec3(3.55f, 2.95f, 0.f), vec3(1.9f, 1.2f, 1.9f), STEEL, drum, qAxis(vec3(0, 0, 1), -1.84f));
        d2.param = 9.f;
        e.parts.push_back(d2);
        E.push_back(e);
    }
    // ---- pallets of materials by the cranes' feet
    {
        const float pal[5][3] = {{-23.f, 0.f, -12.f}, {-31.f, 0.f, -4.f}, {19.f, 0.f, -10.f}, {27.f, 0.f, -5.f}, {22.f, 0.f, -15.f}};
        for (int i = 0; i < 5; i++) {
            Element e = el(POP, 3.f + 0.5f * i, 0.8f);
            e.gone = L_LEAVE + 0.4f * i;
            const vec3 b(pal[i][0], 0.f, pal[i][1]);
            e.parts.push_back(box(b.x - 0.6f, 0.f, b.z - 0.5f, b.x + 0.6f, 0.15f, b.z + 0.5f, WOOD, vec3(0.55f, 0.42f, 0.28f)));
            const int rows = 2 + (int)(hashf(sd + i) * 3.f);
            const vec3 bc = i % 2 ? P.concrete * 1.1f : P.stone;
            e.parts.push_back(box(b.x - 0.55f, 0.15f, b.z - 0.45f, b.x + 0.55f, 0.15f + 0.25f * rows, b.z + 0.45f, i % 2 ? CONCRETE : STONE, bc));
            E.push_back(e);
        }
    }

    // ---- the rooms, lowered by the cranes one by one
    auto crane = [&](float due, int c) {
        Element e = el(CRANE, due);
        e.crane = c;
        return e;
    };
    float d = L_WALLS;
    const float step = 0.75f;
    {   // living room, left module (its side wall, no roof: the cantilevered slab will cover it)
        Element e = crane(d, 0);
        shell(e, P, -21.f, -14.5f, -6.f, 3.f, SLAB, GF_H, true, false, false);
        e.parts.push_back(box(-21.15f, GF_TOP, -6.15f, -14.5f, FF, -5.f, RENDER, roofC));
        E.push_back(e);
        d += step;
    }
    {   // guest wing, right module
        Element e = crane(d, 1);
        shell(e, P, 11.5f, 18.f, -6.f, 2.f, SLAB, GF_H, false, true, true, 0.3f);
        E.push_back(e);
        d += step;
    }
    {   // living room, right module (its roof only where the suite will not stand)
        Element e = crane(d, 0);
        shell(e, P, -14.5f, -8.f, -6.f, 3.f, SLAB, GF_H, false, true, false);
        e.parts.push_back(box(-11.f, GF_TOP, -6.15f, -7.85f, FF, 3.15f, RENDER, roofC));
        e.parts.push_back(box(-14.5f, GF_TOP, -6.15f, -11.f, FF, -5.f, RENDER, roofC));
        E.push_back(e);
        d += step;
    }
    {   // guest wing, left module (the roof's slab cantilevers into the canopy later)
        Element e = crane(d, 1);
        shell(e, P, 5.f, 11.5f, -6.f, 2.f, SLAB, GF_H, true, false, true, 0.3f);
        E.push_back(e);
        d += step;
    }
    {   // kitchen, left
        Element e = crane(d, 0);
        shell(e, P, -8.f, -1.5f, -7.f, 0.f, SLAB, GF_H, false, false, true, 0.3f);
        E.push_back(e);
        d += step;
    }
    {   // kitchen, right
        Element e = crane(d, 1);
        shell(e, P, -1.5f, 5.f, -7.f, 0.f, SLAB, GF_H, false, false, true, 0.3f);
        E.push_back(e);
        d += step;
    }
    // ---- the stone walls, laid course by course (masons, not cranes), framing the villa left and right
    {
        Element e = el(GROW, d, 2.2f);
        e.parts.push_back(box(-22.3f, 0.f, -8.f, -21.6f, 4.0f, 7.5f, STONE, P.stone, 0.4f));
        E.push_back(e);
        Element f = el(GROW, d + 0.9f, 2.2f);
        f.parts.push_back(box(18.6f, 0.f, -7.5f, 19.3f, 3.6f, 6.5f, STONE, P.stone, 0.4f));
        f.parts.push_back(box(19.3f, 0.f, -0.9f, 27.f, 2.2f, -0.3f, STONE, P.stone, 0.4f));
        E.push_back(f);
    }
    {   // the suite's cantilevered slab (the part not over the living room)
        Element e = crane(d, 0);
        e.parts.push_back(box(-24.f, GF_TOP, -5.f, -21.15f, FF, 6.f, CONCRETE, P.concrete));
        e.parts.push_back(box(-21.15f, GF_TOP, 3.f, -11.f, FF, 6.f, CONCRETE, P.concrete));
        e.parts.push_back(box(-21.15f, GF_TOP, -5.f, -14.5f, FF, 3.f, CONCRETE, P.concrete));
        E.push_back(e);
        d += step;
    }
    {   // the suite, left module
        Element e = crane(d, 0);
        shell(e, P, -24.f, -17.5f, -5.f, 6.f, FF, 3.2f, true, false, false);
        E.push_back(e);
        d += step;
    }
    {   // the upper box over the kitchen, right
        Element e = crane(d, 1);
        shell(e, P, -3.5f, 4.f, -7.f, -1.f, FF, 3.2f, false, true, false);
        E.push_back(e);
        d += step;
    }
    {   // the suite, right module
        Element e = crane(d, 0);
        shell(e, P, -17.5f, -11.f, -5.f, 6.f, FF, 3.2f, false, true, false);
        E.push_back(e);
        d += step;
    }
    {   // the upper box, left
        Element e = crane(d, 0);
        shell(e, P, -11.f, -3.5f, -7.f, -1.f, FF, 3.2f, false, false, false);
        E.push_back(e);
        d += step;
    }
    {   // the suite's roof: a thin white slab overhanging in front
        Element e = crane(d, 0);
        e.parts.push_back(box(-24.4f, FF_TOP, -5.3f, -10.8f, FF_TOP + 0.35f, 7.2f, RENDER, roofC));
        E.push_back(e);
        d += step;
    }
    {   // the upper box's roof
        Element e = crane(d, 1);
        e.parts.push_back(box(-10.8f, FF_TOP, -7.3f, 4.3f, FF_TOP + 0.35f, -0.6f, RENDER, roofC));
        E.push_back(e);
        d += step;
    }

    // ---- the glazing: big panes in slim frames, the rooms behind them (they light up at dusk)
    d = L_GLASS;
    {
        Element e = crane(d, 0);
        glazing(e, P, -20.7f, -8.3f, 3.f, SLAB, GF_TOP, 8.6f, 0);
        E.push_back(e);
        d += 1.f;
    }
    {
        Element e = crane(d, 1);
        glazing(e, P, 5.3f, 17.7f, 2.f, SLAB, GF_TOP, 7.6f, 1);
        E.push_back(e);
        d += 1.f;
    }
    {
        Element e = crane(d, 0);
        glazing(e, P, -23.7f, -11.3f, 6.f, FF, FF_TOP, 10.6f, 2);
        E.push_back(e);
        d += 1.f;
    }
    {   // the upper box: a ribbon window in its front wall (the wall's solid parts come with it)
        Element e = crane(d, 1);
        const int m = P.wallMat;
        const vec3 wc = P.wallMat == BOARDFORM ? P.concrete : P.render;
        e.parts.push_back(box(-11.f, FF, -1.3f, 4.f, FF + 0.9f, -1.f, m, wc));
        e.parts.push_back(box(-11.f, FF_TOP - 0.6f, -1.3f, 4.f, FF_TOP, -1.f, m, wc));
        glazing(e, P, -10.8f, 3.8f, -1.f, FF + 0.9f, FF_TOP - 0.6f, 5.6f, 4);
        E.push_back(e);
        d += 1.f;
    }
    {   // the kitchen's glazing (behind the well)
        Element e = crane(d, 1);
        glazing(e, P, -7.7f, 4.7f, 0.f, SLAB, GF_TOP, 6.6f, 5);
        E.push_back(e);
        d += 1.f;
    }
    {   // the suite's side: clad (wood, or stone), a tall slot window
        Element e = crane(d, 0);
        const int m = P.upperMat;
        const vec3 c = m == WOOD ? P.wood : m == STONE ? P.stone : P.render;
        e.parts.push_back(box(-11.f, FF - 0.4f, -1.f, -10.8f, FF_TOP + 0.35f, 2.4f, m, c));
        e.parts.push_back(box(-11.f, FF - 0.4f, 3.6f, -10.8f, FF_TOP + 0.35f, 6.f, m, c));
        e.parts.push_back(box(-11.f, FF_TOP - 0.2f, 2.4f, -10.8f, FF_TOP + 0.35f, 3.6f, m, c));
        e.parts.push_back(box(-11.f, FF - 0.4f, 2.4f, -10.8f, FF + 0.2f, 3.6f, m, c));
        glazing(e, P, 2.4f, 3.6f, -10.8f, FF + 0.2f, FF_TOP - 0.2f, 6.f, 3, 2);
        // the cladding wraps the front band of the suite's slab and roof
        e.parts.push_back(box(-24.2f, GF_TOP - 0.05f, 6.f, -10.8f, FF + 0.02f, 6.2f, m, c));
        E.push_back(e);
        d += 1.f;
    }
    {   // the canopy: the guest wing's roof cantilevered over its deck, on two slim steel columns
        Element e = crane(d, 1);
        e.parts.push_back(box(4.7f, GF_TOP, 2.3f, 18.3f, GF_TOP + 0.3f, 6.2f, RENDER, roofC));
        for (float x : {6.f, 17.f}) e.parts.push_back(box(x - 0.07f, SLAB, 5.9f, x + 0.07f, GF_TOP, 6.04f, FRAME, P.frame));
        E.push_back(e);
        d += 1.f;
    }
    {   // the roof terrace: a slim balustrade round the guest wing's roof
        Element e = crane(d, 1);
        const float y0 = FF, y1 = FF + 1.05f;
        const float x0 = 5.1f, x1 = 18.2f, z0 = -6.1f, z1 = 2.2f;
        e.parts.push_back(box(x0, y1 - 0.05f, z1 - 0.05f, x1, y1, z1 + 0.05f, FRAME, P.frame));
        e.parts.push_back(box(x0 - 0.05f, y1 - 0.05f, z0, x0 + 0.05f, y1, z1, FRAME, P.frame));
        e.parts.push_back(box(x1 - 0.05f, y1 - 0.05f, z0, x1 + 0.05f, y1, z1, FRAME, P.frame));
        e.parts.push_back(box(x0, y0 + 0.5f, z1 - 0.03f, x1, y0 + 0.53f, z1 + 0.03f, FRAME, P.frame));
        for (int i = 0; i <= 13; i++) {
            const float x = x0 + (x1 - x0) * i / 13.f;
            e.parts.push_back(box(x - 0.03f, y0, z1 - 0.03f, x + 0.03f, y1, z1 + 0.03f, FRAME, P.frame));
        }
        for (int i = 0; i <= 8; i++) {
            const float z = z0 + (z1 - z0) * i / 8.f;
            e.parts.push_back(box(x0 - 0.03f, y0, z - 0.03f, x0 + 0.03f, y1, z + 0.03f, FRAME, P.frame));
        }
        // the terrace's floor: deck boards
        e.parts.push_back(box(5.2f, FF, -5.9f, 18.f, FF + 0.06f, 2.f, DECK, P.deck));
        E.push_back(e);
        d += 1.f;
    }
    {   // the pergola on the terrace
        Element e = crane(d, 1);
        const float x0 = 11.f, x1 = 17.6f, z0 = -5.6f, z1 = -0.6f, y0 = FF + 0.06f, y1 = FF + 2.7f;
        for (float x : {x0, x1})
            for (float z : {z0, z1}) e.parts.push_back(box(x - 0.1f, y0, z - 0.1f, x + 0.1f, y1, z + 0.1f, RENDER, white));
        for (float z : {z0, z1}) e.parts.push_back(box(x0 - 0.2f, y1, z - 0.1f, x1 + 0.2f, y1 + 0.25f, z + 0.1f, RENDER, white));
        for (int i = 0; i < 14; i++) {
            const float x = x0 + 0.1f + (x1 - x0 - 0.2f) * i / 13.f;
            e.parts.push_back(box(x - 0.05f, y1 + 0.25f, z0 - 0.3f, x + 0.05f, y1 + 0.42f, z1 + 0.3f, WOOD, P.wood, 1.f));
        }
        E.push_back(e);
        d += 1.f;
    }
    {   // planters on the terrace
        Element e = el(POP, d, 1.f);
        for (int i = 0; i < 3; i++) {
            const float x = 6.f + i * 1.6f;
            e.parts.push_back(box(x - 0.5f, FF + 0.06f, -5.6f, x + 0.5f, FF + 0.7f, -4.6f, STONE, P.stone, 0.f));
            Part b = shape(SPHERE, vec3(x, FF + 1.15f, -5.1f), vec3(1.1f, 1.0f, 1.0f), FOLIAGE, P.leaf);
            b.param = 0.3f * i;
            e.parts.push_back(b);
        }
        E.push_back(e);
    }

    // ---- the pool: dug, tiled, filled
    {
        const float x0 = POOL[0], z0 = POOL[1], x1 = POOL[2], z1 = POOL[3], D = POOL_DEPTH;
        Element e = el(DIG, L_POOL, 2.5f);
        auto tileBox = [&](float a0, float b0, float c0, float a1, float b1, float c1) {
            Part p = box(a0, b0, c0, a1, b1, c1, TILES, P.tile);
            p.tiles = true;
            e.parts.push_back(p);
        };
        tileBox(x0, -D - 0.2f, z0, x1, -D, z1);
        tileBox(x0 - 0.3f, -D - 0.2f, z0 - 0.3f, x1 + 0.3f, 0.f, z0);
        tileBox(x0 - 0.3f, -D - 0.2f, z1, x1 + 0.3f, 0.f, z1 + 0.3f);
        tileBox(x0 - 0.3f, -D - 0.2f, z0, x0, 0.f, z1);
        tileBox(x1, -D - 0.2f, z0, x1 + 0.3f, 0.f, z1);
        E.push_back(e);
        // the coping: pale stone round the edge
        Element c = el(GROW, L_POOL + 3.5f, 1.f);
        c.parts.push_back(box(x0 - 0.5f, 0.f, z0 - 0.5f, x1 + 0.5f, 0.1f, z0, STONE, P.stone * 1.08f, 1.f));
        c.parts.push_back(box(x0 - 0.5f, 0.f, z1, x1 + 0.5f, 0.1f, z1 + 0.5f, STONE, P.stone * 1.08f, 1.f));
        c.parts.push_back(box(x0 - 0.5f, 0.f, z0, x0, 0.1f, z1, STONE, P.stone * 1.08f, 1.f));
        c.parts.push_back(box(x1, 0.f, z0, x1 + 0.5f, 0.1f, z1, STONE, P.stone * 1.08f, 1.f));
        E.push_back(c);
        // the water rising
        Element w = el(GROW, L_POOL + 4.5f, 2.f);
        w.parts.push_back(box(x0, -D, z0, x1, -0.12f, z1, WATER, P.water));
        E.push_back(w);
    }

    // ---- the garden
    {   // the deck round the pool, and the guest wing's deck
        Element e = el(GROW, L_GARDEN + 0.5f, 1.5f);
        const float x0 = POOL[0], z0 = POOL[1], x1 = POOL[2], z1 = POOL[3];
        e.parts.push_back(box(-25.5f, 0.f, 6.5f, -3.5f, 0.12f, z0 - 0.5f, DECK, P.deck));
        e.parts.push_back(box(-25.5f, 0.f, z1 + 0.5f, -3.5f, 0.12f, 16.4f, DECK, P.deck));
        e.parts.push_back(box(-25.5f, 0.f, z0 - 0.5f, x0 - 0.5f, 0.12f, z1 + 0.5f, DECK, P.deck));
        e.parts.push_back(box(x1 + 0.5f, 0.f, z0 - 0.5f, -3.5f, 0.12f, z1 + 0.5f, DECK, P.deck));
        E.push_back(e);
        Element f = el(GROW, L_GARDEN + 1.2f, 1.2f);
        f.parts.push_back(box(4.7f, 0.f, 2.f, 18.3f, 0.3f, 6.4f, DECK, P.deck));
        E.push_back(f);
    }
    // the trees, planted one by one
    {
        struct T { float x, z, s; int k; };
        std::vector<T> ts;
        switch (P.flora) {
        case 0: ts = {{-28.f, 11.f, 1.0f, 0}, {9.f, 11.5f, 0.9f, 0}, {-27.5f, -9.f, 1.0f, 1}, {21.5f, 3.f, 1.0f, 1},
                      {24.f, 9.f, 1.1f, 0}, {-2.f, 13.f, 0.75f, 0}, {14.f, -10.f, 0.9f, 1}}; break;
        case 1: ts = {{-28.f, 10.f, 1.0f, 4}, {9.f, 12.f, 0.9f, 4}, {-28.f, -8.f, 0.8f, 3}, {23.f, 4.f, 0.9f, 4},
                      {24.f, 10.f, 0.8f, 3}, {-2.f, 13.f, 0.8f, 4}, {14.f, -11.f, 0.8f, 3}}; break;
        case 2: ts = {{-28.f, 11.f, 1.05f, 5}, {9.f, 11.5f, 0.95f, 5}, {-27.f, -8.f, 1.1f, 5}, {22.f, 3.f, 0.9f, 5},
                      {24.f, 10.f, 1.15f, 5}, {-1.5f, 13.f, 0.8f, 6}, {12.f, 13.5f, 0.8f, 6}, {-24.f, 15.5f, 0.7f, 6}}; break;
        default: ts = {{-28.f, 11.f, 0.9f, 7}, {9.f, 12.f, 0.8f, 8}, {-27.f, -8.f, 1.0f, 0}, {22.f, 3.f, 0.9f, 7},
                       {24.f, 10.f, 0.9f, 8}, {-2.f, 13.f, 0.9f, 8}, {13.f, 13.f, 0.75f, 7}, {-25.f, 15.5f, 0.8f, 8}}; break;
        }
        for (size_t i = 0; i < ts.size(); i++) {
            Element e = el(TREE, L_GARDEN + 1.5f + 0.75f * i, 1.6f);
            tree(e.parts, P, ts[i].k, vec3(ts[i].x, 0.f, ts[i].z), ts[i].s, sd * 31 + (uint32_t)i * 97);
            E.push_back(e);
        }
    }
    {   // the loungers by the pool, a parasol
        Element e = el(POP, L_GARDEN + 2.5f, 1.f);
        for (int i = 0; i < 3; i++) {
            const float x = -21.f + i * 2.4f, z = 14.8f;
            e.parts.push_back(box(x - 0.36f, 0.12f, z - 1.f, x + 0.36f, 0.42f, z + 1.f, WOOD, P.deck * 1.05f, 1.f));
            e.parts.push_back(box(x - 0.34f, 0.42f, z - 0.95f, x + 0.34f, 0.52f, z + 0.5f, FABRIC, P.fabric));
            Part back = box(x - 0.34f, 0.42f, -0.4f, x + 0.34f, 0.52f, 0.4f, FABRIC, P.fabric);
            back.c = vec3(x, 0.72f, z + 0.78f);
            back.q = qAxis(vec3(1, 0, 0), 0.75f);
            e.parts.push_back(back);
        }
        E.push_back(e);
        Element u = el(POP, L_GARDEN + 3.2f, 1.f);
        u.parts.push_back(shape(CYL, vec3(-13.6f, 1.25f, 15.2f), vec3(0.08f, 2.5f, 0.08f), FRAME, P.frame));
        u.parts.push_back(shape(CONE, vec3(-13.6f, 2.55f, 15.2f), vec3(3.2f, 0.7f, 3.2f), FABRIC, P.fabric));
        u.parts.push_back(shape(CYL, vec3(-13.6f, 0.2f, 15.2f), vec3(0.6f, 0.15f, 0.6f), STONE, P.stone));
        E.push_back(u);
    }
    {   // the hedges along the boundary, grown
        Element e = el(GROW, L_GARDEN + 3.5f, 2.f);
        const vec3 hc = P.flora == 3 ? P.leaf * 0.9f : P.leaf2 * 0.85f;
        if (P.flora != 3) {
            e.parts.push_back(box(-31.f, 0.f, -12.f, -30.f, 1.4f, 14.f, FOLIAGE, hc));
            e.parts.push_back(box(27.f, 0.f, -12.f, 28.f, 1.4f, 12.f, FOLIAGE, hc));
        } else {
            e.parts.push_back(box(-31.f, 0.f, -12.f, -30.f, 1.4f, 14.f, STONE, P.stone * 0.9f, 0.6f));
            e.parts.push_back(box(27.f, 0.f, -12.f, 28.f, 1.4f, 12.f, STONE, P.stone * 0.9f, 0.6f));
        }
        E.push_back(e);
    }
    {   // an outdoor table under the canopy
        Element e = el(POP, L_GARDEN + 4.2f, 1.f);
        e.parts.push_back(box(9.f, 0.3f + 0.72f, 3.4f, 13.f, 0.3f + 0.78f, 4.6f, WOOD, P.deck * 0.8f, 1.f));
        for (float x : {9.3f, 12.7f}) e.parts.push_back(box(x - 0.05f, 0.3f, 3.5f, x + 0.05f, 1.02f, 4.5f, FRAME, P.frame));
        for (int i = 0; i < 3; i++)
            for (float z : {3.0f, 5.0f}) {
                const float x = 9.7f + i * 1.3f;
                e.parts.push_back(box(x - 0.24f, 0.3f, z - 0.22f, x + 0.24f, 0.75f, z + 0.22f, FABRIC, P.fabric * 0.9f));
            }
        E.push_back(e);
    }
    {   // the garden's lamps: low bollards by the paths and the pool
        Element e = el(POP, L_GARDEN + 5.5f, 1.f);
        const float lp[9][2] = {{-25.8f, 7.f}, {-25.8f, 12.f}, {-25.8f, 16.6f}, {-3.2f, 7.f}, {-3.2f, 16.6f},
                                {4.4f, 6.8f}, {18.6f, 6.8f}, {2.f, 10.f}, {20.f, 12.f}};
        for (auto& l : lp) {
            e.parts.push_back(shape(CYL, vec3(l[0], 0.35f, l[1]), vec3(0.16f, 0.7f, 0.16f), FRAME, P.frame));
            Part lamp = shape(CYL, vec3(l[0], 0.66f, l[1]), vec3(0.17f, 0.1f, 0.17f), LAMP, P.lampCol);
            lamp.light = GARDEN_LAMP;
            e.parts.push_back(lamp);
        }
        E.push_back(e);
    }
    for (Element& e : E) e.finish();
    return E;
}

// ---------------------------------------------------------------- the scenery: what stands in the field from the start
std::vector<Part> scenery(const Palette& P, uint64_t seed) {
    std::vector<Part> o;
    const uint32_t sd = (uint32_t)(seed % 100000u);
    // a few big trees near the site
    struct T { float x, z, s; int k; };
    std::vector<T> near;
    switch (P.flora) {
    case 0: near = {{-40.f, -18.f, 1.3f, 2}, {36.f, -26.f, 1.2f, 2}, {-46.f, 4.f, 1.1f, 0}, {40.f, 2.f, 1.0f, 0}, {-34.f, -30.f, 1.f, 1}}; break;
    case 1: near = {{-40.f, -18.f, 1.2f, 3}, {36.f, -24.f, 1.3f, 3}, {-44.f, 4.f, 1.1f, 4}, {40.f, 2.f, 1.0f, 4}, {-33.f, -30.f, 1.2f, 3}}; break;
    case 2: near = {{-40.f, -16.f, 1.3f, 5}, {36.f, -24.f, 1.2f, 5}, {-44.f, 4.f, 1.1f, 6}, {40.f, 2.f, 1.2f, 5}, {-33.f, -28.f, 1.1f, 5}}; break;
    default: near = {{-40.f, -18.f, 1.1f, 7}, {36.f, -24.f, 1.0f, 7}, {-44.f, 4.f, 1.0f, 8}, {40.f, 2.f, 1.0f, 8}, {-33.f, -30.f, 1.0f, 0}}; break;
    }
    for (size_t i = 0; i < near.size(); i++) tree(o, P, near[i].k, vec3(near[i].x, 0.f, near[i].z), near[i].s, sd + (uint32_t)i * 7);
    // the far tree line (a band behind the site, thinner where the sea opens)
    const int far = P.flora == 3 ? 14 : 40;
    for (int i = 0; i < far; i++) {
        const float u = hashf(sd * 3 + i * 11), v = hashf(sd * 7 + i * 13), w = hashf(sd * 5 + i * 17);
        const float x = -120.f + 240.f * (i + u * 0.8f) / far;
        const float z = -55.f - v * 60.f;
        int k;
        switch (P.flora) {
        case 0: k = w < 0.45f ? 1 : (w < 0.75f ? 2 : 0); break;
        case 1: k = w < 0.75f ? 3 : 4; break;
        case 2: k = w < 0.6f ? 5 : 6; break;
        default: k = w < 0.5f ? 7 : 8; break;
        }
        if ((P.flora == 0 || P.flora == 2) && std::fabs(x) < 70.f && w < 0.75f) continue;
        tree(o, P, k, vec3(x, 0.f, z), 0.9f + 0.6f * u, sd + 1000u + (uint32_t)i);
    }
    // bushes on the field's edges
    for (int i = 0; i < 10; i++) {
        const float u = hashf(sd * 19 + i), v = hashf(sd * 23 + i);
        const float side = i % 2 ? 1.f : -1.f;
        const vec3 b(side * (34.f + u * 20.f), 0.f, -20.f + v * 34.f);
        if (P.flora == 3) agave(o, P, b, 0.8f + u * 0.4f, sd + 300u + i);
        else bush(o, P, b, 0.8f + u * 0.5f, sd + 300u + i);
    }
    return o;
}

} // namespace arch_site
