// TILES: the mosaic, laid like a mosaicist lays one.
//
// The design is painted into a label raster over the view (the well's frame, and on each side a medallion with its
// ring, its ground and its figure, a vase below it and a vine springing from it around the medallion; NOCTURNE adds
// gold stars over the ground). Then the tesserae are laid in rows following the outlines (andamento): the distance
// to the nearest outline (an exact Euclidean distance transform of the raster) gives each point its row, and each
// row is walked along its iso-line, a tessera placed after another with a grout gap, its two ends cut square to the
// row (so on a curve they become wedges, as cut ones are), each checked against what is already laid (an occupancy
// raster). Every region's first row (its outline) is laid first, then the next row in, and so on, so the ground
// echoes the figures in rows rippling out from them, the medial seams where rows meet as real mosaics show them.
// Holes left where rows pinch out are filled with smaller pieces. The trencadis variant breaks the rows into
// irregular shards. Then each tessera takes its color, material (stone, glass, gold, silver) and tilt.
#include "universes/tiles/mosaic.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace tiles {

// ---------------------------------------------------------------------------------------------------------
// Variants
// ---------------------------------------------------------------------------------------------------------

const VariantDef VARIANTS[] = {
    // BYZANTINE: a ground of gold smalti, lapis and crimson, a gold sun and a rosette
    {"BYZANTINE",
     {0x2f5fd0, 0xd23446, 0x22a45e, 0xf0b43a, 0x8a4cc8, 0x26b2c4, 0xeee0c4},
     {0xb08a3e, 0xd4ad5a, 0x7a5a26}, MAT_GOLD,
     {0x1a2550, 0xa8243a}, MAT_GLASS,
     {0x141c3a, 0x9e2236, 0xc89a44}, MAT_GOLD,
     {0x1e3a8c, 0x14245a}, MAT_GLASS,
     {0x4a1e12, 0xd8a644, 0xf6dc8a, 0xd0602a}, MAT_GOLD, MAT_GLASS,
     {0x12361e, 0x2e7a3e, 0x6cae5a},
     {0x222a40, 0xd8dce4, 0x2a4cb0}, MAT_SILVER,
     0x5a2a6a,
     0x3a3630, 0x8a8170, 0x9a3a22,
     0x16151a,
     0xffe2b0, 0xf2e6cc, 0xe8b850,
     FIG_SUN, FIG_ROSETTE, 0.24f, 0.034f, false, false},
    // ROMAN: a floor of black basalt, white marble, terracotta and ochre stone (matte)
    {"ROMAN",
     {0xc8643a, 0xe8dcc8, 0xd8a24a, 0x7a8c5a, 0xa84a3a, 0x8a9aa8, 0xb88a5a},
     {0x1c1b1a, 0x2a2826, 0x121110}, MAT_STONE,
     {0xe6dccb, 0xb4542e}, MAT_STONE,
     {0xe8dfcf, 0xb4542e, 0xd09a48}, MAT_STONE,
     {0xddd2bf, 0xc8baa4}, MAT_STONE,
     {0x1c1a18, 0xc0602e, 0xe2b064, 0x8a3a24}, MAT_STONE, MAT_STONE,
     {0x1c1a18, 0x6a7a4a, 0x9aa870},
     {0x1c1a18, 0xb0582e, 0xe2d6c2}, MAT_STONE,
     0x6a3a4a,
     0x5a554c, 0x9c9282, 0x8a3a22,
     0x141312,
     0xfff0dc, 0xeee4d4, 0xd8a24a,
     FIG_STAR, FIG_SUN, 0.25f, 0.036f, false, false},
    // NOCTURNE: a vault of deep blue glass strewn with gold stars
    {"NOCTURNE",
     {0x4a7ce6, 0xe0465a, 0x36b874, 0xf2c04a, 0xa060e0, 0x3ac0c8, 0xe8e2f0},
     {0x13245e, 0x1c3478, 0x0c163e}, MAT_GLASS,
     {0x0a1030, 0xc8a04a}, MAT_GOLD,
     {0x0a1030, 0xe8e4dc, 0xc89a44}, MAT_GOLD,
     {0x0e1a48, 0x08102e}, MAT_GLASS,
     {0x0a1030, 0xd8aa48, 0xfae2a0, 0xe8e4dc}, MAT_GOLD, MAT_SILVER,
     {0x0a2018, 0x2a7050, 0x5aa880},
     {0x0a1030, 0xd8dce8, 0xc89a44}, MAT_SILVER,
     0x6a3a8a,
     0x2a2c34, 0x7a7c80, 0x9a3a2a,
     0x101018,
     0xd8e0ff, 0xe6e8f2, 0xe8c060,
     FIG_STAR, FIG_ROSETTE, 0.24f, 0.034f, false, true},
    // TRENCADIS: broken glazed tiles, bright, set in pale grout
    {"TRENCADIS",
     {0x2a6ee0, 0xe83c3c, 0x2ab45a, 0xf6c42a, 0x9a4ad8, 0x22b8d0, 0xf47a2a},
     {0x1a4a7a, 0x23608e, 0x123656}, MAT_GLASS,
     {0xf2efe6, 0xe84a2a}, MAT_GLASS,
     {0xf2efe6, 0x2a8a4a, 0xf6c42a}, MAT_GLASS,
     {0x0e7a8a, 0x0a5a6a}, MAT_GLASS,
     {0x7a2a12, 0xf6b42a, 0xfff0a0, 0xe8502a}, MAT_GLASS, MAT_GLASS,
     {0x14502a, 0x3aa04a, 0x9ad65a},
     {0x1a2a4a, 0xf2efe6, 0x2a6ee0}, MAT_GLASS,
     0x8a3aa8,
     0x8c8a84, 0xb0aca2, 0x9a3a22,
     0x1a1b1e,
     0xfff4e0, 0xf4f0e8, 0xf6c42a,
     FIG_SUN, FIG_ROSETTE, 0.36f, 0.05f, true, false},
    // LAGOON: Venetian glass, sea greens and turquoise with silver smalti
    {"LAGOON",
     {0x2a8ad0, 0xe0566a, 0x3ac08a, 0xe8c25a, 0x7a6ad8, 0x2ad0c8, 0xe8f0ec},
     {0x0e4a4a, 0x166060, 0x08302e}, MAT_GLASS,
     {0x062424, 0xb8c4cc}, MAT_SILVER,
     {0x062424, 0x2ab0b0, 0xc0ccd2}, MAT_SILVER,
     {0x0a2e40, 0x061c2a}, MAT_GLASS,
     {0x06262a, 0xc8d4da, 0xf2f6f6, 0x3ad0b8}, MAT_SILVER, MAT_GLASS,
     {0x08281e, 0x2a8a5a, 0x7ac89a},
     {0x062424, 0xc0ccd2, 0x2a8ad0}, MAT_SILVER,
     0x4a3a7a,
     0x2e3634, 0x7a8480, 0x8a3a2a,
     0x0e1414,
     0xe0fff8, 0xe6f2ee, 0x7ae0d0,
     FIG_ROSETTE, FIG_STAR, 0.24f, 0.034f, false, false},
};
const int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

namespace {

// Labels: the regions of the design.
enum {
    L_NONE = -1, // the well (no tesserae)
    L_FIELD = 0,
    L_FRAME = 1,
    L_STAR = 2,
    L_SIDE0 = 3,
};
enum { S_RING = 0, S_GROUND, S_FIGA, S_FIGB, S_FIGC, S_VASE, S_STEM, S_LEAF, S_GRAPE, S_COUNT };
constexpr int L_COUNT = L_SIDE0 + 2 * S_COUNT;
int sideLabel(int side, int part) { return L_SIDE0 + side * S_COUNT + part; }
int labelSide(int l) { return l >= L_SIDE0 ? (l - L_SIDE0) / S_COUNT : -1; }
int labelPart(int l) { return l >= L_SIDE0 ? (l - L_SIDE0) % S_COUNT : -1; }
int labelGroup(int l) {
    switch (labelPart(l)) {
    case S_FIGA: case S_FIGB: case S_FIGC: return 1;
    case S_GROUND: return 2;
    case S_VASE: return 3;
    case S_STEM: return 4;
    case S_LEAF: return 5;
    case S_GRAPE: return 6;
    default: return 0;
    }
}

vec2 v2(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 sub(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 mul(vec2 a, float s) { return {a.x * s, a.y * s}; }
float dot2(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
float len2(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
vec2 norm2(vec2 a) { const float l = len2(a); return l > 1e-8f ? mul(a, 1.f / l) : v2(1.f, 0.f); }
float cross2(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }
vec2 rot2(vec2 a, float ang) { const float c = std::cos(ang), s = std::sin(ang); return {c * a.x - s * a.y, s * a.x + c * a.y}; }

float hashf(float x, float y) {
    uint32_t h = (uint32_t)(int32_t)std::floor(x) * 374761393u + (uint32_t)(int32_t)std::floor(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xFFFFFF) / 16777216.f;
}
float vnoise(float x, float y) {
    const float ix = std::floor(x), iy = std::floor(y), fx = x - ix, fy = y - iy;
    const float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    const float a = hashf(ix, iy), b = hashf(ix + 1, iy), c = hashf(ix, iy + 1), d = hashf(ix + 1, iy + 1);
    return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy);
}
float fbm(float x, float y) { return vnoise(x, y) * 0.6f + vnoise(x * 2.1f + 3.7f, y * 2.1f + 1.3f) * 0.4f; }

// Felzenszwalb's 1D squared distance transform.
void edt1(const float* f, float* d, int n, int* v, float* z) {
    int k = 0;
    v[0] = 0;
    z[0] = -1e20f;
    z[1] = 1e20f;
    for (int q = 1; q < n; q++) {
        float s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.f * q - 2.f * v[k]);
        while (s <= z[k]) {
            k--;
            s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.f * q - 2.f * v[k]);
        }
        k++;
        v[k] = q;
        z[k] = s;
        z[k + 1] = 1e20f;
    }
    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[k + 1] < q) k++;
        d[q] = (q - v[k]) * (q - v[k]) + f[v[k]];
    }
}

struct Grid {
    int W = 0, H = 0;
    float r = 0.04f;
    vec2 lo;
    std::vector<int8_t> lab;
    std::vector<float> dist;
    std::vector<uint8_t> occ;
    int idx(int x, int y) const { return y * W + x; }
    vec2 pos(int x, int y) const { return {lo.x + (x + 0.5f) * r, lo.y + (y + 0.5f) * r}; }
    int labelAt(vec2 p) const {
        const int x = (int)std::floor((p.x - lo.x) / r), y = (int)std::floor((p.y - lo.y) / r);
        if (x < 0 || y < 0 || x >= W || y >= H) return -2;
        return lab[idx(x, y)];
    }
    bool occAt(vec2 p) const {
        const int x = (int)std::floor((p.x - lo.x) / r), y = (int)std::floor((p.y - lo.y) / r);
        if (x < 0 || y < 0 || x >= W || y >= H) return true;
        return occ[idx(x, y)] != 0;
    }
    float D(vec2 p) const {
        float fx = (p.x - lo.x) / r - 0.5f, fy = (p.y - lo.y) / r - 0.5f;
        fx = std::clamp(fx, 0.f, (float)W - 1.001f);
        fy = std::clamp(fy, 0.f, (float)H - 1.001f);
        const int x = (int)fx, y = (int)fy;
        const float ax = fx - x, ay = fy - y;
        const float a = dist[idx(x, y)], b = dist[idx(x + 1, y)], c = dist[idx(x, y + 1)], d = dist[idx(x + 1, y + 1)];
        return lerpf(lerpf(a, b, ax), lerpf(c, d, ax), ay);
    }
    vec2 grad(vec2 p) const {
        const float e = r;
        return {(D(v2(p.x + e, p.y)) - D(v2(p.x - e, p.y))) / (2 * e), (D(v2(p.x, p.y + e)) - D(v2(p.x, p.y - e))) / (2 * e)};
    }
};

// Paints a label over a box where `inside` holds.
template <class F> void paint(Grid& g, float x0, float y0, float x1, float y1, int label, F inside) {
    const int ix0 = std::max(0, (int)std::floor((x0 - g.lo.x) / g.r)), ix1 = std::min(g.W - 1, (int)std::ceil((x1 - g.lo.x) / g.r));
    const int iy0 = std::max(0, (int)std::floor((y0 - g.lo.y) / g.r)), iy1 = std::min(g.H - 1, (int)std::ceil((y1 - g.lo.y) / g.r));
    for (int y = iy0; y <= iy1; y++)
        for (int x = ix0; x <= ix1; x++)
            if (inside(g.pos(x, y))) g.lab[g.idx(x, y)] = (int8_t)label;
}

// A thick polyline (a vine's stem).
void paintPolyline(Grid& g, const std::vector<vec2>& pts, float hw, int label) {
    for (size_t i = 0; i + 1 < pts.size(); i++) {
        const vec2 a = pts[i], b = pts[i + 1];
        const vec2 ab = sub(b, a);
        const float l2 = std::max(1e-8f, dot2(ab, ab));
        paint(g, std::min(a.x, b.x) - hw, std::min(a.y, b.y) - hw, std::max(a.x, b.x) + hw, std::max(a.y, b.y) + hw, label, [&](vec2 p) {
            const float t = std::clamp(dot2(sub(p, a), ab) / l2, 0.f, 1.f);
            return len2(sub(p, add(a, mul(ab, t)))) < hw;
        });
    }
}

// A leaf: a pointed lens from `base` along `dir`, length l, half width w.
void paintLeaf(Grid& g, vec2 base, vec2 dir, float l, float w, int label) {
    dir = norm2(dir);
    const vec2 nrm{-dir.y, dir.x};
    const vec2 m = add(base, mul(dir, l * 0.5f));
    const float ext = l * 0.5f + w;
    paint(g, m.x - ext, m.y - ext, m.x + ext, m.y + ext, label, [&](vec2 p) {
        const vec2 q = sub(p, m);
        const float u = dot2(q, dir) / (l * 0.5f), v = dot2(q, nrm);
        if (std::fabs(u) >= 1.f) return false;
        // fuller toward the base, pointed at the tip
        const float sh = (1.f - u * u) * (1.f - 0.25f * u);
        return std::fabs(v) < w * sh;
    });
}

// The figure inside a medallion (q relative to its center, Ri its inner radius): which part (or -1: the ground).
int figurePart(int kind, vec2 q, float Ri) {
    const float r = len2(q), a = std::atan2(q.y, q.x);
    if (kind == FIG_SUN) {
        if (r < 0.33f * Ri) return S_FIGA;
        const float step = TAU / 16.f;
        const float sector = std::round(a / step);
        const float d = a - sector * step;
        const float yy = r * std::cos(d), xx = r * std::sin(d);
        const bool longRay = ((int)sector % 2 + 2) % 2 == 0;
        const float r0 = 0.39f * Ri;
        if (longRay) {
            const float r1 = 0.95f * Ri, w0 = 0.21f * Ri;
            const float t = (yy - r0) / (r1 - r0);
            if (t >= 0.f && t < 1.f && std::fabs(xx) < w0 * std::pow(1.f - t, 0.85f)) return S_FIGB;
        } else {
            const float r1 = 0.74f * Ri, w0 = 0.16f * Ri;
            const float t = (yy - r0) / (r1 - r0);
            const float xc = 0.09f * Ri * std::sin(t * TAU * 0.9f) * t;
            if (t >= 0.f && t < 1.f && std::fabs(xx - xc) < w0 * (1.f - t)) return S_FIGC;
        }
        return -1;
    }
    if (kind == FIG_ROSETTE) {
        if (r < 0.2f * Ri) return S_FIGC;
        auto petal = [&](float off, float rin, float rout, float b) {
            const float step = TAU / 8.f;
            const float sector = std::round((a - off) / step);
            const float d = a - off - sector * step;
            const float yy = r * std::cos(d), xx = r * std::sin(d);
            const float m = (rin + rout) * 0.5f, al = (rout - rin) * 0.5f;
            const float u = (yy - m) / al;
            if (std::fabs(u) >= 1.f) return false;
            return std::fabs(xx) < b * (1.f - u * u);
        };
        if (petal(TAU / 16.f, 0.25f * Ri, 0.66f * Ri, 0.15f * Ri)) return S_FIGB;
        if (petal(0.f, 0.24f * Ri, 0.97f * Ri, 0.21f * Ri)) return S_FIGA;
        return -1;
    }
    // FIG_STAR: an eight-pointed star (two squares), a disc in its heart, a core
    if (r < 0.19f * Ri) return S_FIGC;
    if (r < 0.42f * Ri) return S_FIGB;
    const float s = 0.66f * Ri;
    const vec2 q2 = rot2(q, PI / 4.f);
    if ((std::fabs(q.x) < s && std::fabs(q.y) < s) || (std::fabs(q2.x) < s && std::fabs(q2.y) < s)) return S_FIGA;
    return -1;
}

vec3 mix3(vec3 a, vec3 b, float t) { return a + (b - a) * t; }

} // namespace

static_assert(L_FRAME == FRAME_LABEL, "the frame's label");

Design build(int variant, vec2 viewC, vec2 viewH, uint64_t seed, bool well) {
    const VariantDef& V = VARIANTS[std::clamp(variant, 0, VARIANT_COUNT - 1)];
    Design out;
    Rng rng(seed * 2654435761ull + 77);
    const float h = V.pitch, gw = V.grout_w, W = h - gw;

    Grid g;
    g.r = std::min(0.04f, h / 6.f);
    out.lo = g.lo = v2(viewC.x - viewH.x - 0.3f, viewC.y - viewH.y - 0.3f);
    out.hi = v2(viewC.x + viewH.x + 0.3f, viewC.y + viewH.y + 0.3f);
    g.W = (int)std::ceil((out.hi.x - out.lo.x) / g.r);
    g.H = (int)std::ceil((out.hi.y - out.lo.y) / g.r);
    const int N = g.W * g.H;
    g.lab.assign(N, (int8_t)L_FIELD);

    // ---- The design.
    const float wellE = 0.06f;
    const float frameE = wellE + 3.f * h;
    if (well) {
        paint(g, -5.f - frameE, -10.f - frameE, 5.f + frameE, 10.f + frameE, L_FRAME, [&](vec2) { return true; });
        paint(g, -5.f - wellE, -10.f - wellE, 5.f + wellE, 10.f + wellE, L_NONE, [&](vec2) { return true; });
    }

    Layout& lay = out.lay;
    const float x0 = 11.55f, x1 = viewC.x + viewH.x;
    const float space = x1 - x0;
    lay.R = std::min(4.15f, space * 0.5f - 0.75f);
    lay.sides = lay.R > 1.6f;
    if (lay.sides) {
        const float cx = x0 + space * 0.5f + 0.1f;
        const float cy = 2.1f;
        lay.ringW = 5.f * h;
        const float R = lay.R, Ri = R - lay.ringW;
        const float vs = std::clamp(R / 3.9f * 1.3f, 0.7f, 1.4f);
        const bool vine = R > 2.6f;
        for (int side = 0; side < 2; side++) {
            const float sg = side == 0 ? -1.f : 1.f;
            const vec2 C{sg * cx, cy};
            lay.med[side] = C;
            const vec2 Vc{sg * cx, std::max(viewC.y - viewH.y + 1.9f * vs, cy - R - 3.3f * vs)};
            lay.vase[side] = Vc;
            const int kind = side == 0 ? V.figL : V.figR;
            if (vine) {
                // the vine: two stems from the vase's mouth around the medallion, a curl at their ends, leaves
                // on the outer side, a bunch of grapes on each
                const float Rv = R + 0.62f;
                for (int half = 0; half < 2; half++) {
                    const float hs = half == 0 ? -1.f : 1.f;
                    std::vector<vec2> pts;
                    auto P = [&](float al) { return add(C, v2(hs * Rv * std::sin(al), -Rv * std::cos(al))); };
                    auto T = [&](float al) { return v2(hs * std::cos(al), std::sin(al)); };
                    const float a0 = 0.55f, a1 = 2.62f;
                    const vec2 S = add(Vc, v2(hs * 0.5f * vs, 1.15f * vs));
                    const vec2 E0 = P(a0), t0 = T(a0);
                    const vec2 c1 = add(S, v2(hs * 0.25f, 0.9f)), c2 = sub(E0, mul(t0, 1.1f));
                    for (int i = 0; i <= 24; i++) {
                        const float u = i / 24.f, iu = 1.f - u;
                        pts.push_back(add(add(mul(S, iu * iu * iu), mul(c1, 3 * iu * iu * u)), add(mul(c2, 3 * iu * u * u), mul(E0, u * u * u))));
                    }
                    for (int i = 1; i <= 60; i++) pts.push_back(P(a0 + (a1 - a0) * i / 60.f));
                    // the curl, turning outward
                    {
                        const vec2 E = P(a1), te = T(a1);
                        const vec2 nout{hs * std::sin(a1), -std::cos(a1)};
                        const float rho0 = 0.62f;
                        const vec2 Q = add(E, mul(nout, rho0));
                        const vec2 e = sub(E, Q);
                        float phi0 = std::atan2(e.y, e.x);
                        const vec2 tccw{-e.y, e.x};
                        const float dir = dot2(tccw, te) > 0.f ? 1.f : -1.f;
                        for (int i = 1; i <= 40; i++) {
                            const float u = i / 40.f;
                            const float rho = rho0 * (1.f - 0.62f * u);
                            const float ph = phi0 + dir * u * TAU * 1.05f;
                            pts.push_back(add(Q, v2(rho * std::cos(ph), rho * std::sin(ph))));
                        }
                    }
                    paintPolyline(g, pts, h * 0.56f, sideLabel(side, S_STEM));
                    // leaves on the outer side of the arc, leaning forward and back in turn
                    int li = 0;
                    for (float al = a0 + 0.28f; al < a1 - 0.1f; al += 0.36f, li++) {
                        const vec2 p = P(al);
                        const vec2 nout{hs * std::sin(al), -std::cos(al)};
                        const vec2 tt = T(al);
                        const float lean = (li % 2 == 0 ? 0.75f : -0.55f);
                        const vec2 dir = norm2(add(nout, mul(tt, lean)));
                        // keep the leaves off the HUD's column
                        const vec2 tip = add(p, mul(dir, 1.1f));
                        if (std::fabs(tip.x) < 10.9f) continue;
                        paintLeaf(g, add(p, mul(nout, h * 0.4f)), dir, 1.0f, 0.3f, sideLabel(side, S_LEAF));
                    }
                    // two leaves on the rising stem
                    for (int j = 0; j < 2; j++) {
                        const vec2 p = pts[8 + j * 8];
                        const vec2 tt = norm2(sub(pts[9 + j * 8], pts[7 + j * 8]));
                        const float s2 = (j == 0 ? -1.f : 1.f) * hs;
                        const vec2 nrm{-tt.y * s2, tt.x * s2};
                        const vec2 dir = norm2(add(nrm, mul(tt, 0.6f)));
                        paintLeaf(g, p, dir, 0.85f, 0.26f, sideLabel(side, S_LEAF));
                    }
                    // grapes hanging from the outer arc
                    {
                        const float al = 1.25f;
                        const vec2 nout{hs * std::sin(al), -std::cos(al)};
                        const vec2 top = add(P(al), add(mul(nout, 0.55f), v2(0.f, -0.1f)));
                        const int rows[4] = {3, 3, 2, 1};
                        const float gr = 0.205f, sp = 0.43f;
                        for (int rr = 0; rr < 4; rr++)
                            for (int c = 0; c < rows[rr]; c++) {
                                const vec2 cc = add(top, v2((c - (rows[rr] - 1) * 0.5f) * sp + hs * 0.06f * rr, -rr * sp * 0.86f));
                                if (std::fabs(cc.x) < 11.5f) continue;
                                paint(g, cc.x - gr, cc.y - gr, cc.x + gr, cc.y + gr, sideLabel(side, S_GRAPE),
                                      [&](vec2 p) { return len2(sub(p, cc)) < gr; });
                            }
                    }
                }
            }
            // the vase (a kantharos): lip, neck, body, stem, foot, two looped handles
            {
                const float s = vs;
                paint(g, Vc.x - 1.8f * s, Vc.y - 1.3f * s, Vc.x + 1.8f * s, Vc.y + 1.3f * s, sideLabel(side, S_VASE), [&](vec2 p) {
                    const float x = (p.x - Vc.x) / s, y = (p.y - Vc.y) / s;
                    const float ax = std::fabs(x);
                    if (y > 0.95f && y < 1.22f) return ax < 1.18f;
                    if (y > 0.55f && y <= 0.95f) return ax < 0.7f + 0.42f * (y - 0.55f) / 0.4f;
                    if (y > -0.58f && y <= 0.62f) {
                        const float ex = x / 1.12f, ey = (y - 0.02f) / 0.6f;
                        if (ex * ex + ey * ey < 1.f) return true;
                    }
                    if (y > -0.98f && y <= -0.4f) return ax < 0.21f;
                    if (y > -1.24f && y <= -0.98f) return ax < 0.78f - 0.3f * (y + 1.24f) / 0.26f;
                    // handles
                    const float hx = ax - 1.08f, hy = y - 0.42f;
                    const float rr = std::sqrt(hx * hx + hy * hy);
                    if (ax > 0.98f && rr > 0.27f && rr < 0.47f) return true;
                    return false;
                });
            }
            // the medallion: its ring, its ground, its figure
            paint(g, C.x - R, C.y - R, C.x + R, C.y + R, sideLabel(side, S_RING), [&](vec2 p) {
                const float r = len2(sub(p, C));
                return r < R;
            });
            paint(g, C.x - Ri, C.y - Ri, C.x + Ri, C.y + Ri, sideLabel(side, S_GROUND), [&](vec2 p) { return len2(sub(p, C)) < Ri; });
            const float Rf = Ri - 0.12f;
            for (int part : {S_FIGA, S_FIGB, S_FIGC}) {
                paint(g, C.x - Rf, C.y - Rf, C.x + Rf, C.y + Rf, sideLabel(side, part), [&](vec2 p) {
                    vec2 q = sub(p, C);
                    if (side == 1) q.x = -q.x;
                    return figurePart(kind, q, Rf) == part;
                });
            }
        }
    }
    // NOCTURNE: gold stars over the ground (away from the figures, the well and the HUD's columns)
    if (V.stars) {
        std::vector<vec2> placed;
        for (int tries = 0; tries < 1400 && placed.size() < 70; tries++) {
            const vec2 c{rng.range(out.lo.x + 0.6f, out.hi.x - 0.6f), rng.range(out.lo.y + 0.6f, out.hi.y - 0.6f)};
            const float rad = rng.range(0.3f, 0.44f);
            if (std::fabs(c.x) < 6.4f) continue;
            if (std::fabs(c.x) < 11.6f && c.y > -7.2f) continue;
            // nor under the corners' lettering
            if (c.x < -9.f && c.y > viewC.y + viewH.y - 3.4f) continue;
            if (std::fabs(c.x) > 9.f && c.y < viewC.y - viewH.y + 3.8f) continue;
            bool ok = true;
            for (const vec2& o : placed)
                if (len2(sub(o, c)) < 1.9f) { ok = false; break; }
            if (!ok) continue;
            for (int k = 0; k < 16 && ok; k++) {
                const float a = k * TAU / 16.f;
                const int l = g.labelAt(add(c, v2(std::cos(a) * (rad + 0.6f), std::sin(a) * (rad + 0.6f))));
                if (l != L_FIELD) ok = false;
            }
            if (g.labelAt(c) != L_FIELD) ok = false;
            if (!ok) continue;
            placed.push_back(c);
            const float rot = rng.range(0.f, 0.8f);
            paint(g, c.x - rad, c.y - rad, c.x + rad, c.y + rad, L_STAR, [&](vec2 p) {
                const vec2 q = rot2(sub(p, c), rot);
                const vec2 q2 = rot2(q, PI / 4.f);
                const float s = rad * 0.7f;
                return (std::fabs(q.x) < s && std::fabs(q.y) < s) || (std::fabs(q2.x) < s && std::fabs(q2.y) < s);
            });
        }
    }

    // ---- The distance to the nearest outline.
    {
        std::vector<float> f(N, 1e20f);
        for (int y = 0; y < g.H; y++)
            for (int x = 0; x < g.W; x++) {
                const int l = g.lab[g.idx(x, y)];
                bool b = false;
                if (x > 0 && g.lab[g.idx(x - 1, y)] != l) b = true;
                if (x + 1 < g.W && g.lab[g.idx(x + 1, y)] != l) b = true;
                if (y > 0 && g.lab[g.idx(x, y - 1)] != l) b = true;
                if (y + 1 < g.H && g.lab[g.idx(x, y + 1)] != l) b = true;
                if (b) f[g.idx(x, y)] = 0.f;
            }
        const int M = std::max(g.W, g.H);
        std::vector<float> fin(M), dout(M), z(M + 1);
        std::vector<int> v(M);
        for (int x = 0; x < g.W; x++) {
            for (int y = 0; y < g.H; y++) fin[y] = f[g.idx(x, y)];
            edt1(fin.data(), dout.data(), g.H, v.data(), z.data());
            for (int y = 0; y < g.H; y++) f[g.idx(x, y)] = dout[y];
        }
        for (int y = 0; y < g.H; y++) {
            for (int x = 0; x < g.W; x++) fin[x] = f[g.idx(x, y)];
            edt1(fin.data(), dout.data(), g.W, v.data(), z.data());
            for (int x = 0; x < g.W; x++) f[g.idx(x, y)] = dout[x];
        }
        g.dist.resize(N);
        for (int i = 0; i < N; i++) g.dist[i] = std::sqrt(f[i]) * g.r + 0.5f * g.r;
    }

    // ---- The painter's sketch: the outlines of the figures still to lay.
    {
        out.sw = g.W;
        out.sh = g.H;
        std::vector<float> s(N, 0.f);
        for (int y = 1; y < g.H - 1; y++)
            for (int x = 1; x < g.W - 1; x++) {
                const int l = g.lab[g.idx(x, y)];
                const int n[4] = {g.lab[g.idx(x - 1, y)], g.lab[g.idx(x + 1, y)], g.lab[g.idx(x, y - 1)], g.lab[g.idx(x, y + 1)]};
                for (int k = 0; k < 4; k++)
                    if (n[k] != l && (labelGroup(l) > 0 || labelGroup(n[k]) > 0)) s[g.idx(x, y)] = 1.f;
            }
        // R: the outlines; G: the mosaicist's guide lines for the rows, inside the figures
        out.sketch.assign((size_t)N * 2, 0);
        for (int y = 1; y < g.H - 1; y++)
            for (int x = 1; x < g.W - 1; x++) {
                const int i = g.idx(x, y);
                float a = s[i] * 0.5f;
                a += (s[g.idx(x - 1, y)] + s[g.idx(x + 1, y)] + s[g.idx(x, y - 1)] + s[g.idx(x, y + 1)]) * 0.125f;
                out.sketch[(size_t)i * 2] = (uint8_t)std::min(255.f, a * 255.f);
                if (labelGroup(g.lab[i]) > 0) {
                    const float d = g.dist[i];
                    if (d > h * 0.6f) {
                        const float e = std::fabs(d - std::round(d / h) * h);
                        out.sketch[(size_t)i * 2 + 1] = (uint8_t)(255.f * (1.f - smoothstepf(0.f, g.r * 1.1f, e)));
                    }
                }
            }
    }

    // ---- Laying the rows.
    g.occ.assign(N, 0);
    std::vector<Tessera>& T = out.tess;
    T.reserve(30000);
    int strokeId = 0;

    auto freeAt = [&](vec2 p) { return !g.occAt(p); };
    auto mark = [&](const vec2* c) {
        float x0b = c[0].x, x1b = c[0].x, y0b = c[0].y, y1b = c[0].y;
        for (int i = 1; i < 4; i++) {
            x0b = std::min(x0b, c[i].x); x1b = std::max(x1b, c[i].x);
            y0b = std::min(y0b, c[i].y); y1b = std::max(y1b, c[i].y);
        }
        const int ix0 = std::max(0, (int)std::floor((x0b - g.lo.x) / g.r)), ix1 = std::min(g.W - 1, (int)std::ceil((x1b - g.lo.x) / g.r));
        const int iy0 = std::max(0, (int)std::floor((y0b - g.lo.y) / g.r)), iy1 = std::min(g.H - 1, (int)std::ceil((y1b - g.lo.y) / g.r));
        // inside the quad (either winding), shrunk by a third of a texel
        float sgn = 0.f;
        for (int i = 0; i < 4; i++) sgn += cross2(c[i], c[(i + 1) % 4]);
        sgn = sgn >= 0.f ? 1.f : -1.f;
        for (int y = iy0; y <= iy1; y++)
            for (int x = ix0; x <= ix1; x++) {
                const vec2 p = g.pos(x, y);
                bool in = true;
                for (int i = 0; i < 4 && in; i++) {
                    const vec2 e = sub(c[(i + 1) % 4], c[i]);
                    const float el = len2(e);
                    if (el < 1e-6f) continue;
                    if (sgn * cross2(e, sub(p, c[i])) / el < g.r * 0.33f) in = false;
                }
                if (in) g.occ[g.idx(x, y)] = 1;
            }
    };
    auto quadOk = [&](const vec2* c, int label, float L, float Wd) {
        float area = 0.f;
        for (int i = 0; i < 4; i++) area += cross2(c[i], c[(i + 1) % 4]);
        area = std::fabs(area) * 0.5f;
        if (area < 0.4f * L * Wd || area > 1.9f * L * Wd) return false;
        // convex
        float s0 = 0.f;
        for (int i = 0; i < 4; i++) {
            const float cr = cross2(sub(c[(i + 1) % 4], c[i]), sub(c[(i + 2) % 4], c[(i + 1) % 4]));
            if (i == 0) s0 = cr;
            else if (cr * s0 <= 0.f) return false;
        }
        vec2 m{0.f, 0.f};
        for (int i = 0; i < 4; i++) m = add(m, mul(c[i], 0.25f));
        if (g.labelAt(m) != label) return false;
        if (!freeAt(m)) return false;
        for (int i = 0; i < 4; i++) {
            const vec2 pc = add(m, mul(sub(c[i], m), 0.7f));
            if (!freeAt(pc) || g.labelAt(pc) != label) return false;
            const vec2 pe = add(m, mul(sub(mul(add(c[i], c[(i + 1) % 4]), 0.5f), m), 0.75f));
            if (!freeAt(pe) || g.labelAt(pe) != label) return false;
        }
        return true;
    };
    auto project = [&](vec2& p, float target) {
        for (int it = 0; it < 4; it++) {
            const vec2 gr = g.grad(p);
            const float gl2 = dot2(gr, gr);
            if (gl2 < 0.25f) return false;
            const float e = g.D(p) - target;
            p = sub(p, mul(gr, e / gl2));
            if (std::fabs(e) < g.r * 0.05f) break;
        }
        return std::fabs(g.D(p) - target) < g.r * 0.6f;
    };
    // walks `dist` along the iso-line from p (dir +1 / -1); false if it breaks off
    auto march = [&](vec2& p, float dir, float dist, float target, int label) {
        float done = 0.f;
        const float step = g.r * 0.9f;
        while (done < dist - 1e-5f) {
            const float st = std::min(step, dist - done);
            vec2 gr = g.grad(p);
            const float gl = len2(gr);
            if (gl < 0.5f) return false;
            const vec2 t{-gr.y / gl * dir, gr.x / gl * dir};
            const vec2 prev = p;
            p = add(p, mul(t, st));
            const vec2 g2 = g.grad(p);
            const float g2l = dot2(g2, g2);
            if (g2l < 0.25f) return false;
            p = sub(p, mul(g2, (g.D(p) - target) / g2l));
            const float moved = len2(sub(p, prev));
            if (moved < st * 0.3f) return false;
            done += moved;
            if (g.labelAt(p) != label) return false;
        }
        return true;
    };
    auto nrmAt = [&](vec2 p) { return norm2(g.grad(p)); };
    auto push = [&](const vec2* c, int label, int k, int stroke, int idx, std::vector<Tessera>* run) {
        Tessera t;
        for (int i = 0; i < 4; i++) t.c[i] = c[i];
        t.center = mul(add(add(c[0], c[1]), add(c[2], c[3])), 0.25f);
        t.label = label;
        t.k = k;
        t.stroke = stroke;
        t.idx = idx;
        mark(c);
        run->push_back(t);
    };

    const int KMAX = 120;
    std::vector<std::vector<int>> buckets(KMAX);
    for (int i = 0; i < N; i++) {
        const int l = g.lab[i];
        if (l < 0) continue;
        const float d = g.dist[i];
        const int k = (int)std::floor(d / h);
        if (k >= KMAX) continue;
        if (std::fabs(d - (k + 0.5f) * h) < g.r * 0.5f) buckets[k].push_back(i);
    }
    for (int k = 0; k < KMAX; k++) {
        std::vector<int>& b = buckets[k];
        for (size_t i = b.size(); i > 1; i--) std::swap(b[i - 1], b[rng.irange(0, (int)i - 1)]);
        const float target = (k + 0.5f) * h;
        for (int ti : b) {
            if (g.occ[ti]) continue;
            const int label = g.lab[ti];
            vec2 p = g.pos(ti % g.W, ti / g.W);
            if (!project(p, target) || g.labelAt(p) != label || !freeAt(p)) continue;
            std::vector<Tessera> fwd, bwd;
            const int stroke = strokeId++;
            for (int pass = 0; pass < 2; pass++) {
                const float dir = pass == 0 ? 1.f : -1.f;
                vec2 a = p;
                if (pass == 1) {
                    if (!march(a, dir, gw, target, label)) continue;
                }
                int fails = 0;
                for (int n = 0; n < 4000; n++) {
                    const float Wd = W * (V.shards ? rng.range(0.85f, 1.05f) : rng.range(0.95f, 1.03f));
                    const float L = Wd * (V.shards ? rng.range(0.55f, 1.8f) : rng.range(0.72f, 1.22f));
                    vec2 b = a;
                    if (!march(b, dir, L, target, label)) break;
                    const vec2 na = nrmAt(a), nb = nrmAt(b);
                    vec2 c[4] = {sub(a, mul(na, Wd * 0.5f)), sub(b, mul(nb, Wd * 0.5f)), add(b, mul(nb, Wd * 0.5f)), add(a, mul(na, Wd * 0.5f))};
                    // a little irregularity: each corner nudged, the shards cut at an angle
                    const float jit = V.shards ? 0.16f : 0.045f;
                    for (int i = 0; i < 4; i++) c[i] = add(c[i], v2(rng.range(-jit, jit) * Wd, rng.range(-jit, jit) * Wd));
                    if (V.shards && rng.chance(0.5f)) {
                        const int e = rng.irange(0, 3);
                        c[e] = add(c[e], mul(sub(c[(e + 2) % 4], c[e]), rng.range(0.1f, 0.3f)));
                    }
                    if (quadOk(c, label, L, Wd)) {
                        push(c, label, k, stroke, 0, pass == 0 ? &fwd : &bwd);
                        fails = 0;
                        if (!march(b, dir, gw * (V.shards ? rng.range(0.8f, 1.6f) : rng.range(0.9f, 1.15f)), target, label)) break;
                        a = b;
                    } else {
                        if (++fails > 3) break;
                        if (!march(a, dir, Wd * 0.4f, target, label)) break;
                    }
                }
            }
            // one run along the line: the backward part reversed, then the forward part
            int idx = 0;
            for (int i = (int)bwd.size() - 1; i >= 0; i--) { bwd[i].idx = idx++; T.push_back(bwd[i]); }
            for (Tessera& t : fwd) { t.idx = idx++; T.push_back(t); }
        }
    }
    // ---- The holes: smaller pieces where the rows pinch out.
    {
        std::vector<int> cand;
        for (int y = 0; y < g.H; y += 2)
            for (int x = 0; x < g.W; x += 2) {
                const int i = g.idx(x, y);
                if (g.lab[i] >= 0 && !g.occ[i]) cand.push_back(i);
            }
        for (size_t i = cand.size(); i > 1; i--) std::swap(cand[i - 1], cand[rng.irange(0, (int)i - 1)]);
        const float sizes[3] = {0.8f, 0.62f, 0.46f};
        for (int ti : cand) {
            if (g.occ[ti]) continue;
            const int label = g.lab[ti];
            const vec2 p = g.pos(ti % g.W, ti / g.W);
            vec2 gr = g.grad(p);
            vec2 t = len2(gr) > 0.3f ? norm2(v2(-gr.y, gr.x)) : rot2(v2(1.f, 0.f), rng.range(0.f, PI));
            if (V.shards) t = rot2(t, rng.range(-0.6f, 0.6f));
            const vec2 n{-t.y, t.x};
            for (float sz : sizes) {
                const float s = W * sz * 0.5f;
                const float sl = s * (V.shards ? rng.range(0.8f, 1.4f) : rng.range(0.9f, 1.15f));
                vec2 c[4] = {sub(sub(p, mul(t, sl)), mul(n, s)), sub(add(p, mul(t, sl)), mul(n, s)), add(add(p, mul(t, sl)), mul(n, s)), add(sub(p, mul(t, sl)), mul(n, s))};
                const float jit = V.shards ? 0.14f : 0.05f;
                for (int i = 0; i < 4; i++) c[i] = add(c[i], v2(rng.range(-jit, jit) * s * 2, rng.range(-jit, jit) * s * 2));
                if (quadOk(c, label, sl * 2.f, s * 2.f)) {
                    const int k = (int)std::floor(g.D(p) / h);
                    std::vector<Tessera> one;
                    push(c, label, k + 200, strokeId++, 0, &one);
                    T.push_back(one[0]);
                    break;
                }
            }
        }
    }

    // ---- Colors, materials, tilts.
    const vec3 field0 = linc(V.field[0]), field1 = linc(V.field[1]), field2 = linc(V.field[2]);
    for (Tessera& t : T) {
        t.rnd = rng.uniform();
        const float r2 = rng.uniform(), r3 = rng.uniform();
        const int l = t.label;
        const int part = labelPart(l);
        const int kk = t.k >= 200 ? t.k - 200 : t.k;
        const bool fill = t.k >= 200;
        t.side = labelSide(l);
        t.group = labelGroup(l);
        vec3 c;
        int mat = MAT_GLASS;
        if (l == L_FIELD) {
            const float n = fbm(t.center.x * 0.22f + 11.f, t.center.y * 0.22f + 5.f);
            c = mix3(field2, field1, smoothstepf(0.15f, 0.9f, n));
            c = mix3(c, field0, 0.55f);
            if (r2 < 0.05f) c = field1;
            else if (r2 < 0.09f) c = field2;
            mat = V.fieldMat;
            if (mat == MAT_GOLD && r3 < 0.018f) mat = MAT_SILVER, c = vec3(0.4f, 0.41f, 0.43f);
        } else if (l == L_FRAME) {
            c = kk == 0 ? linc(V.frame[0]) : linc(V.frame[1]);
            mat = kk == 0 ? (V.frameMat == MAT_STONE ? MAT_STONE : MAT_GLASS) : V.frameMat;
        } else if (l == L_STAR) {
            c = linc(V.ring[2]);
            mat = MAT_GOLD;
        } else if (part == S_RING) {
            c = kk == 0 ? linc(V.ring[0]) : kk == 1 ? linc(V.ring[1]) : linc(V.ring[2]);
            mat = kk == 0 ? (V.ringMat == MAT_STONE ? MAT_STONE : MAT_GLASS) : kk == 1 ? (V.ringMat == MAT_STONE ? MAT_STONE : MAT_GLASS) : V.ringMat;
        } else if (part == S_GROUND) {
            const float tt = smoothstepf(0.f, 5.f, (float)kk);
            c = mix3(linc(V.ground[1]), linc(V.ground[0]), tt);
            mat = V.groundMat;
        } else if (part == S_FIGA || part == S_FIGB || part == S_FIGC) {
            if (kk == 0 && !fill && part == S_FIGA) {
                c = linc(V.fig[0]);
                mat = V.figMat == MAT_STONE ? MAT_STONE : MAT_GLASS;
            } else if (kk == 0 && !fill) {
                c = mix3(linc(V.fig[0]), linc(part == S_FIGB ? V.fig[3] : V.fig[2]), 0.45f);
                mat = V.figMat == MAT_STONE ? MAT_STONE : MAT_GLASS;
            } else if (part == S_FIGA) {
                c = mix3(linc(V.fig[1]), linc(V.fig[2]), smoothstepf(1.f, 4.5f, (float)kk));
                mat = V.figMat;
            } else if (part == S_FIGB) {
                c = mix3(linc(V.fig[3]), linc(V.fig[2]), 0.25f * smoothstepf(1.f, 3.f, (float)kk));
                mat = (int)V.fig2Mat;
            } else {
                c = mix3(linc(V.fig[2]), linc(V.fig[1]), 0.3f * (kk % 2));
                mat = V.figMat;
            }
        } else if (part == S_VASE) {
            const vec2 vc = out.lay.vase[std::max(0, t.side)];
            const float y = t.center.y - vc.y;
            if (kk == 0 && !fill) { c = linc(V.vase[0]); mat = MAT_GLASS; }
            else if (std::fabs(y - 0.1f) < 0.17f || std::fabs(y - 1.08f) < 0.1f) { c = linc(V.vase[2]); mat = MAT_GLASS; }
            else { c = linc(V.vase[1]); mat = V.vaseMat; }
            if (V.vaseMat == MAT_STONE) mat = MAT_STONE;
        } else if (part == S_STEM) {
            c = mix3(linc(V.leaf[0]), linc(V.leaf[1]), 0.45f);
            mat = V.fieldMat == MAT_STONE ? MAT_STONE : MAT_GLASS;
        } else if (part == S_LEAF) {
            c = (kk == 0 && !fill) ? linc(V.leaf[0]) : mix3(linc(V.leaf[1]), linc(V.leaf[2]), smoothstepf(1.f, 2.5f, (float)kk));
            mat = V.fieldMat == MAT_STONE ? MAT_STONE : MAT_GLASS;
        } else { // grapes
            c = linc(V.grape) * (0.75f + 0.5f * r2);
            if (r3 < 0.18f) c = mix3(c, vec3(1.f), 0.35f);
            mat = V.fieldMat == MAT_STONE ? MAT_STONE : MAT_GLASS;
        }
        // every tessera its own: value and a touch of hue
        const float val = mat == MAT_STONE ? 0.22f : mat == MAT_GOLD ? 0.18f : 0.16f;
        c = c * (1.f + val * (t.rnd - 0.5f) * 2.f);
        c = vec3(c.x * (1.f + 0.06f * (r3 - 0.5f)), c.y, c.z * (1.f + 0.06f * (r2 - 0.5f)));
        if (mat == MAT_GOLD) c = mix3(c, vec3(c.x * 1.12f, c.y * 0.92f, c.z * 0.8f), r3 * 0.6f); // red golds and lemon golds
        t.color = vec3(std::max(0.f, c.x), std::max(0.f, c.y), std::max(0.f, c.z));
        t.mat = mat;
        const float amp = mat == MAT_GOLD ? 0.26f : mat == MAT_SILVER ? 0.22f : mat == MAT_GLASS ? 0.12f : 0.05f;
        const float a = rng.range(0.f, TAU), m = amp * std::sqrt(rng.uniform());
        t.tilt = v2(std::cos(a) * m, std::sin(a) * m);
    }

    // ---- The order the figures are laid in (per side): figure, ground, vase, stems, leaves, grapes; each from its
    // outline in, a run at a time.
    for (int side = 0; side < 2; side++) {
        std::vector<int>& o = out.order[side];
        for (int i = 0; i < (int)T.size(); i++)
            if (T[i].side == side && T[i].group > 0) o.push_back(i);
        std::stable_sort(o.begin(), o.end(), [&](int a, int b) {
            const Tessera &A = T[a], &B = T[b];
            if (A.group != B.group) return A.group < B.group;
            if (A.k != B.k) return A.k < B.k;
            if (A.stroke != B.stroke) return A.stroke < B.stroke;
            return A.idx < B.idx;
        });
    }
    for (int i = 0; i < (int)T.size(); i++)
        if (T[i].group == 0) out.field.push_back(i);
    return out;
}

} // namespace tiles
