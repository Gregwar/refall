// TERRARIUM's lab and city as meshes (see world.hpp): the lab room on the third floor of a concrete building (its
// benches, cabinets, the reagent shelf and its bottles, the microscope, the test tube rack, the petri dishes, the
// notebook, the steel shelving and its specimen jars, the second bench and its monitor, the ceiling's tubes, the
// windows), the jar's specimen tag; the glassware laid over as glass; the city all around (the street, its
// buildings with their floors, ledges, parapets and roof clutter, street lamps, abandoned cars, neon signs); and the
// growth that escapes the jar: vines, roots, leaf clumps, flowers and trees, each part appearing at its own line
// (the vertex shader grows it out of its pivot), so the whole of it is one static mesh built once.
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "universes/terrarium/build.hpp"
#include "universes/terrarium/world.hpp"

namespace terra {

using namespace bld;
using namespace lab;

namespace {

// ---------------------------------------------------------------- primitives
void tri(B& b, const vec3& a, const vec3& c, const vec3& d, const vec3& n, const vec4& f) {
    b.vert(a, n, f); b.vert(c, n, f); b.vert(d, n, f);
}
void quad(B& b, const vec3& a, const vec3& c, const vec3& d, const vec3& e, const vec3& n, const vec4& f = vec4(0, 0, 0, 0)) {
    tri(b, a, c, d, n, f);
    tri(b, a, d, e, n, f);
}
// a box, its faces (-x, +x, -y, +y, -z, +z) each with its own material (< 0: left out)
void boxM(B& b, const vec3& lo, const vec3& hi, const float mt[6], const vec4& f = vec4(0, 0, 0, 0)) {
    const float m0 = b.mat;
    const vec3 p000(lo.x, lo.y, lo.z), p100(hi.x, lo.y, lo.z), p010(lo.x, hi.y, lo.z), p110(hi.x, hi.y, lo.z);
    const vec3 p001(lo.x, lo.y, hi.z), p101(hi.x, lo.y, hi.z), p011(lo.x, hi.y, hi.z), p111(hi.x, hi.y, hi.z);
    if (mt[0] >= 0.f) { b.mat = mt[0]; quad(b, p000, p001, p011, p010, V3(-1, 0, 0), f); }
    if (mt[1] >= 0.f) { b.mat = mt[1]; quad(b, p100, p110, p111, p101, V3(1, 0, 0), f); }
    if (mt[2] >= 0.f) { b.mat = mt[2]; quad(b, p000, p100, p101, p001, V3(0, -1, 0), f); }
    if (mt[3] >= 0.f) { b.mat = mt[3]; quad(b, p010, p011, p111, p110, V3(0, 1, 0), f); }
    if (mt[4] >= 0.f) { b.mat = mt[4]; quad(b, p000, p010, p110, p100, V3(0, 0, -1), f); }
    if (mt[5] >= 0.f) { b.mat = mt[5]; quad(b, p001, p101, p111, p011, V3(0, 0, 1), f); }
    b.mat = m0;
}
void box(B& b, const vec3& lo, const vec3& hi, const vec4& f = vec4(0, 0, 0, 0)) {
    const float m[6] = {b.mat, b.mat, b.mat, b.mat, b.mat, b.mat};
    boxM(b, lo, hi, m, f);
}
// a colored, painted thing (material 58: its linear color and its gloss in f)
vec4 paint(float r, float g, float bl, float gloss = 0.3f) { return vec4(r, g, bl, gloss); }
void pbox(B& b, const vec3& lo, const vec3& hi, const vec4& col) {
    const float m0 = b.mat;
    b.mat = 58.f;
    box(b, lo, hi, col);
    b.mat = m0;
}
// a revolved profile (r, y) about the vertical through base, its normals from the profile (a repeated point: a crisp
// edge)
void lathe(B& b, const vec3& base, const std::vector<vec2>& pr, int segs, const vec4& f, float sx = 1.f) {
    const int n = (int)pr.size();
    std::vector<vec2> nr(n);
    for (int i = 0; i < n; i++) {
        vec2 t;
        const bool dupB = i > 0 && pr[i].x == pr[i - 1].x && pr[i].y == pr[i - 1].y;
        const bool dupF = i + 1 < n && pr[i].x == pr[i + 1].x && pr[i].y == pr[i + 1].y;
        const vec2 a = pr[std::max(i - 1, 0)], c = pr[std::min(i + 1, n - 1)];
        if (dupB) t = vec2{c.x - pr[i].x, c.y - pr[i].y};
        else if (dupF) t = vec2{pr[i].x - a.x, pr[i].y - a.y};
        else t = vec2{c.x - a.x, c.y - a.y};
        const float l = std::sqrt(t.x * t.x + t.y * t.y) + 1e-9f;
        nr[i] = vec2{t.y / l, -t.x / l};
    }
    for (int i = 0; i + 1 < n; i++) {
        if (pr[i].x == pr[i + 1].x && pr[i].y == pr[i + 1].y) continue;
        for (int s = 0; s < segs; s++) {
            const float a0 = TAU_ * s / segs, a1 = TAU_ * (s + 1) / segs;
            auto P = [&](int k, float a) { return base + V3(std::cos(a) * pr[k].x * sx, pr[k].y, std::sin(a) * pr[k].x); };
            auto N = [&](int k, float a) { return normalize(V3(std::cos(a) * nr[k].x / sx, nr[k].y, std::sin(a) * nr[k].x)); };
            const vec3 p0 = P(i, a0), p1 = P(i, a1), p2 = P(i + 1, a1), p3 = P(i + 1, a0);
            const vec3 n0 = N(i, a0), n1 = N(i, a1), n2 = N(i + 1, a1), n3 = N(i + 1, a0);
            b.vert(p0, n0, f); b.vert(p1, n1, f); b.vert(p2, n2, f);
            b.vert(p0, n0, f); b.vert(p2, n2, f); b.vert(p3, n3, f);
        }
    }
}
void cyl(B& b, const vec3& base, float r, float h, int segs, const vec4& f) {
    lathe(b, base, {vec2{0, 0}, vec2{r, 0}, vec2{r, 0}, vec2{r, h}, vec2{r, h}, vec2{0, h}}, segs, f);
}
void pipe(B& b, const std::vector<vec3>& pts, float r, const vec4& f, int sides = 8) {
    std::vector<float> rad(pts.size(), r);
    std::vector<vec4> inf(pts.size(), f);
    tube(b, pts, rad, inf, sides, true);
}
vec3 rotY(float a, const vec3& p) { const float c = std::cos(a), s = std::sin(a); return V3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z); }
float h1(float n) { return ghash1(n); }

// ---------------------------------------------------------------- glassware: a flask's outline (r, y)
std::vector<vec2> erlenmeyer(float r, float h, float neckR, float neckH) {
    return {vec2{0, 0}, vec2{r * 0.92f, 0}, vec2{r, h * 0.05f}, vec2{neckR * 1.05f, h}, vec2{neckR, h + neckH * 0.1f},
            vec2{neckR, h + neckH}, vec2{neckR * 1.15f, h + neckH}, vec2{neckR * 1.15f, h + neckH * 1.06f}, vec2{0, h + neckH * 1.06f}};
}
std::vector<vec2> beaker(float r, float h) {
    return {vec2{0, 0}, vec2{r * 0.96f, 0}, vec2{r, r * 0.06f}, vec2{r, h}, vec2{r * 1.05f, h * 1.01f}, vec2{0, h * 1.01f}};
}
std::vector<vec2> bottle(float r, float h, float neckR, float neckH) {
    std::vector<vec2> p{vec2{0, 0}, vec2{r * 0.95f, 0}, vec2{r, r * 0.08f}, vec2{r, h * 0.72f}};
    for (int i = 1; i <= 5; i++) {
        const float a = i / 5.f * 1.5708f;
        p.push_back(vec2{neckR + (r - neckR) * std::cos(a), h * 0.72f + (h * 0.28f) * std::sin(a)});
    }
    p.push_back(vec2{neckR, h + neckH});
    p.push_back(vec2{0, h + neckH});
    return p;
}
// the liquid inside a revolved vessel: its outline scaled in, cut at the level
std::vector<vec2> liquid(const std::vector<vec2>& vessel, float inset, float level) {
    std::vector<vec2> p;
    for (size_t i = 0; i < vessel.size(); i++) {
        const vec2 v = vessel[i];
        if (v.y > level) {
            if (i > 0 && vessel[i - 1].y <= level) {
                const vec2 a = vessel[i - 1];
                const float t = (level - a.y) / std::max(v.y - a.y, 1e-5f);
                p.push_back(vec2{std::max(0.f, a.x + (v.x - a.x) * t - inset), level});
            }
            break;
        }
        p.push_back(vec2{std::max(0.f, v.x - inset), std::max(v.y, inset * 0.6f)});
    }
    p.push_back(vec2{0, level});
    return p;
}

// ---------------------------------------------------------------- the neon letters: strokes in a unit cell
using Stroke = std::vector<vec2>;
void arc(Stroke& s, float cx, float cy, float rx, float ry, float a0, float a1, int n) {
    for (int i = 0; i <= n; i++) {
        const float a = (a0 + (a1 - a0) * i / n) * PI_ / 180.f;
        s.push_back(vec2{cx + rx * std::cos(a), cy + ry * std::sin(a)});
    }
}
std::vector<Stroke> glyph(char c) {
    std::vector<Stroke> g;
    auto L = [&](std::initializer_list<vec2> p) { g.push_back(Stroke(p)); };
    switch (c) {
    case 'A': L({{0, 0}, {0.5f, 1}, {1, 0}}); L({{0.22f, 0.42f}, {0.78f, 0.42f}}); break;
    case 'B': { Stroke s{{0.f, 0.5f}, {0, 1}, {0.55f, 1}}; arc(s, 0.55f, 0.75f, 0.35f, 0.25f, 90, -90, 8); s.push_back({0, 0.5f});
                s.push_back({0.6f, 0.5f}); arc(s, 0.6f, 0.25f, 0.38f, 0.25f, 90, -90, 8); s.push_back({0, 0}); s.push_back({0, 0.5f}); g.push_back(s); break; }
    case 'C': { Stroke s; arc(s, 0.55f, 0.5f, 0.48f, 0.5f, 45, 315, 16); g.push_back(s); break; }
    case 'D': { Stroke s{{0, 0}, {0, 1}, {0.45f, 1}}; arc(s, 0.45f, 0.5f, 0.5f, 0.5f, 90, -90, 12); s.push_back({0, 0}); g.push_back(s); break; }
    case 'E': L({{1, 1}, {0, 1}, {0, 0}, {1, 0}}); L({{0, 0.5f}, {0.75f, 0.5f}}); break;
    case 'F': L({{1, 1}, {0, 1}, {0, 0}}); L({{0, 0.5f}, {0.75f, 0.5f}}); break;
    case 'H': L({{0, 0}, {0, 1}}); L({{1, 0}, {1, 1}}); L({{0, 0.5f}, {1, 0.5f}}); break;
    case 'I': L({{0.5f, 0}, {0.5f, 1}}); break;
    case 'K': L({{0, 0}, {0, 1}}); L({{1, 1}, {0, 0.4f}}); L({{0.3f, 0.62f}, {1, 0}}); break;
    case 'L': L({{0, 1}, {0, 0}, {1, 0}}); break;
    case 'M': L({{0, 0}, {0, 1}, {0.5f, 0.4f}, {1, 1}, {1, 0}}); break;
    case 'N': L({{0, 0}, {0, 1}, {1, 0}, {1, 1}}); break;
    case 'O': { Stroke s; arc(s, 0.5f, 0.5f, 0.5f, 0.5f, 0, 360, 24); g.push_back(s); break; }
    case 'P': { Stroke s{{0, 0}, {0, 1}, {0.55f, 1}}; arc(s, 0.55f, 0.75f, 0.4f, 0.25f, 90, -90, 8); s.push_back({0, 0.5f}); g.push_back(s); break; }
    case 'R': { Stroke s{{0, 0}, {0, 1}, {0.55f, 1}}; arc(s, 0.55f, 0.75f, 0.4f, 0.25f, 90, -90, 8); s.push_back({0, 0.5f}); g.push_back(s);
                L({{0.45f, 0.5f}, {1, 0}}); break; }
    case 'S': { Stroke s; arc(s, 0.5f, 0.75f, 0.45f, 0.25f, 20, 270, 10); arc(s, 0.5f, 0.25f, 0.48f, 0.25f, 90, -160, 10); g.push_back(s); break; }
    case 'T': L({{0, 1}, {1, 1}}); L({{0.5f, 1}, {0.5f, 0}}); break;
    case 'U': { Stroke s{{0, 1}, {0, 0.4f}}; arc(s, 0.5f, 0.4f, 0.5f, 0.4f, 180, 360, 12); s.push_back({1, 1}); g.push_back(s); break; }
    case 'V': L({{0, 1}, {0.5f, 0}, {1, 1}}); break;
    case 'Y': L({{0, 1}, {0.5f, 0.5f}, {1, 1}}); L({{0.5f, 0.5f}, {0.5f, 0}}); break;
    case '2': { Stroke s; arc(s, 0.5f, 0.72f, 0.45f, 0.28f, 160, -25, 10); s.push_back({0, 0}); s.push_back({1, 0}); g.push_back(s); break; }
    case '4': L({{0.75f, 0}, {0.75f, 1}, {0, 0.3f}, {1, 0.3f}}); break;
    case '+': L({{0.35f, 0}, {0.65f, 0}, {0.65f, 0.35f}, {1, 0.35f}, {1, 0.65f}, {0.65f, 0.65f}, {0.65f, 1}, {0.35f, 1}, {0.35f, 0.65f},
                 {0, 0.65f}, {0, 0.35f}, {0.35f, 0.35f}, {0.35f, 0}}); break;
    default: break;
    }
    return g;
}

// ---------------------------------------------------------------- growth: vines, roots, leaves, flowers, trees
struct PP { vec3 p, n; }; // a path point and the surface's normal there
// a path from where it is to b over a surface of normal n, k points, wiggling sideways (zero at both ends)
void walk(std::vector<PP>& path, const vec3& b, const vec3& n, float wig, float freq, float ph, int k) {
    const vec3 a = path.empty() ? b : path.back().p;
    const vec3 na = path.empty() ? n : path.back().n;
    const vec3 d = b - a;
    const float L = length(d);
    if (L < 1e-5f) { if (path.empty()) path.push_back({b, n}); return; }
    for (int i = path.empty() ? 0 : 1; i <= k; i++) {
        const float t = (float)i / k;
        const vec3 nn = normalize(lerp(na, n, std::min(1.f, t * 3.f)));
        const vec3 side = normalize(cross(d, nn));
        const float w = wig * std::sin(t * PI_) * (std::sin(t * freq * TAU_ + ph) + 0.35f * std::sin(t * freq * 2.7f * TAU_ + ph * 1.9f));
        path.push_back({a + d * t + side * w, nn});
    }
}
struct VineStyle {
    float line0 = 20.f, speed = 1.f;   // the line it starts at, its length grown a line
    float r0 = 0.02f, rEnd = 0.35f;    // its radius at the root, and at its tip (a fraction)
    float leaf = 0.08f, gap = 1.f;     // the leaves' length, the gap between two (in leaf lengths); 0: none
    float flower = 0.f;                // the chance of a flower at a leaf
    float mat = 70.f;                  // 70 a green stem, 73 a root, 74 bark
    float lift = 0.5f;                 // how far the leaves stand off the surface
    float sway = 0.05f;
    uint32_t seed = 1;
};
// Emit a leaf: attached at a (growing from there at line ln), along d, its face's normal nl, length l, width w. A
// small one (the lab's) is an ovate blade with a heart-shaped base, folded along its midrib, its edges curling; a big
// one (the city's, seen from afar) four triangles.
void leaf(B& b, const vec3& a, const vec3& d0, const vec3& nl0, float l, float w, float ln, float seed, float mat = 71.f, int fine = -1) {
    const vec3 d = normalize(d0);
    const vec3 side = normalize(cross(nl0, d));
    const vec3 nl = normalize(cross(d, side));
    const float m0 = b.mat, t0 = b.two;
    b.mat = mat;
    b.two = 1.f;
    b.gr[0] = a.x; b.gr[1] = a.y; b.gr[2] = a.z; b.gr[3] = ln;
    auto V = [&](const vec3& p, const vec3& n, float u, float v) { b.vert(p, n, vec4(u, v, seed, 0)); };
    if (fine < 0) fine = l < 0.2f ? 1 : 0;
    if (!fine || mat > 71.5f) {
        const vec3 base = a + d * (l * 0.1f), tip = a + d * l + nl * (l * 0.08f);
        const vec3 lf = a + d * (l * 0.42f) + side * (w * 0.5f) + nl * (l * 0.1f), rt = a + d * (l * 0.42f) - side * (w * 0.5f) + nl * (l * 0.1f);
        const vec3 mid = a + d * (l * 0.5f) + nl * (l * 0.02f);
        const vec3 nlL = normalize(nl + side * 0.6f), nlR = normalize(nl - side * 0.6f);
        V(base, nl, 0.1f, 0); V(lf, nlL, 0.42f, 1); V(mid, nl, 0.5f, 0);
        V(base, nl, 0.1f, 0); V(mid, nl, 0.5f, 0); V(rt, nlR, 0.42f, -1);
        V(mid, nl, 0.5f, 0); V(lf, nlL, 0.42f, 1); V(tip, nl, 1, 0);
        V(mid, nl, 0.5f, 0); V(tip, nl, 1, 0); V(rt, nlR, 0.42f, -1);
    } else {
        // the midrib (bending down toward its tip) and the edges at four stations: a heart's lobe, the widest, the
        // narrowing, near the tip
        const float bend = 0.18f * l;
        auto M = [&](float u) { return a + d * (l * u) + nl * (bend * (u * 0.9f - u * u)); };
        const float us[4] = {0.18f, 0.42f, 0.68f, 0.88f}, ws[4] = {0.36f, 0.5f, 0.38f, 0.16f};
        vec3 mp[6], ep[2][4], en[2][4];
        mp[0] = M(0.06f);
        for (int k = 0; k < 4; k++) mp[k + 1] = M(us[k]);
        mp[5] = M(1.f);
        for (int s = 0; s < 2; s++) {
            const float sg = s ? -1.f : 1.f;
            for (int k = 0; k < 4; k++) {
                const float curl = 0.12f * ws[k];
                ep[s][k] = M(us[k] - 0.05f * (k == 0 ? -1.f : 1.f)) + side * (sg * w * ws[k]) + nl * (l * curl);
                en[s][k] = normalize(nl + side * (sg * 0.55f));
            }
        }
        for (int s = 0; s < 2; s++) {
            const float sg = s ? -1.f : 1.f;
            V(mp[0], nl, 0.06f, 0); V(ep[s][0], en[s][0], us[0], sg * ws[0] * 2.f); V(mp[1], nl, us[0], 0);
            for (int k = 0; k < 3; k++) {
                V(mp[k + 1], nl, us[k], 0); V(ep[s][k], en[s][k], us[k], sg * ws[k] * 2.f); V(ep[s][k + 1], en[s][k + 1], us[k + 1], sg * ws[k + 1] * 2.f);
                V(mp[k + 1], nl, us[k], 0); V(ep[s][k + 1], en[s][k + 1], us[k + 1], sg * ws[k + 1] * 2.f); V(mp[k + 2], nl, us[k + 1], 0);
            }
            V(mp[4], nl, us[3], 0); V(ep[s][3], en[s][3], us[3], sg * ws[3] * 2.f); V(mp[5], nl, 1.f, 0);
        }
    }
    b.mat = m0;
    b.two = t0;
}
// a flower: five petals around n at c
void flower(B& b, const vec3& c, const vec3& n0, float r, float ln, float seed) {
    const vec3 n = normalize(n0);
    const vec3 t1 = normalize(cross(n, std::fabs(n.y) < 0.9f ? V3(0, 1, 0) : V3(1, 0, 0))), t2 = cross(n, t1);
    for (int k = 0; k < 5; k++) {
        const float a = k * TAU_ / 5.f + seed * 3.f;
        const vec3 d = t1 * std::cos(a) + t2 * std::sin(a) + n * 0.25f;
        leaf(b, c, d, n, r, r * 0.9f, ln, seed + k * 0.1f, 72.f);
    }
}
// A vine (or a root, or a branch) along a path: a tube whose rings grow out of the ring before (its tip creeping
// along), with leaves on alternate sides and now and then a flower. Returns its length.
float vine(B& b, const std::vector<PP>& path0, const VineStyle& st, std::vector<Bloom>* blooms = nullptr) {
    if (path0.size() < 2) return 0.f;
    // resample by arc length
    std::vector<float> s0(path0.size(), 0.f);
    for (size_t i = 1; i < path0.size(); i++) s0[i] = s0[i - 1] + length(path0[i].p - path0[i - 1].p);
    const float L = s0.back();
    if (L < 1e-4f) return 0.f;
    const float ds = std::max(st.r0 * (st.r0 > 0.2f ? 3.5f : 2.2f), L / (st.r0 > 0.2f ? 40.f : (st.r0 > 0.05f ? 48.f : 80.f)));
    const int n = std::max(2, (int)std::ceil(L / ds) + 1);
    std::vector<PP> path(n);
    std::vector<float> s(n);
    size_t j = 0;
    for (int i = 0; i < n; i++) {
        const float si = L * i / (n - 1);
        while (j + 2 < path0.size() && s0[j + 1] < si) j++;
        const float t = saturate((si - s0[j]) / std::max(s0[j + 1] - s0[j], 1e-6f));
        path[i] = {lerp(path0[j].p, path0[j + 1].p, t), normalize(lerp(path0[j].n, path0[j + 1].n, t))};
        s[i] = si;
    }
    // the tube (its points lifted off the surface by the radius)
    auto rad = [&](float si) { return st.r0 * lerpf(1.f, st.rEnd, std::pow(si / L, 0.8f)); };
    auto appear = [&](float si) { return st.line0 + si / st.speed; };
    std::vector<vec3> C(n), T(n), N(n), Bn(n);
    for (int i = 0; i < n; i++) C[i] = path[i].p + path[i].n * rad(s[i]) * 0.9f;
    for (int i = 0; i < n; i++) T[i] = normalize(C[std::min(i + 1, n - 1)] - C[std::max(i - 1, 0)]);
    {
        vec3 up = std::fabs(T[0].y) < 0.9f ? V3(0, 1, 0) : V3(1, 0, 0);
        N[0] = normalize(cross(cross(T[0], up), T[0]));
        for (int i = 1; i < n; i++) {
            vec3 nn = N[i - 1] - T[i] * dot(N[i - 1], T[i]);
            if (dot(nn, nn) < 1e-10f) nn = N[i - 1];
            N[i] = normalize(nn);
        }
        for (int i = 0; i < n; i++) Bn[i] = cross(T[i], N[i]);
    }
    const int sides = st.r0 > 0.2f ? 5 : (st.r0 > 0.05f ? 6 : 5);
    const float m0 = b.mat;
    b.mat = st.mat;
    b.two = 0.f;
    for (int i = 0; i + 1 < n; i++) {
        for (int k = 0; k < sides; k++) {
            const float a0 = TAU_ * k / sides, a1 = TAU_ * (k + 1) / sides;
            auto D = [&](int r, float a) { return N[r] * std::cos(a) + Bn[r] * std::sin(a); };
            auto put = [&](int r, float a) {
                const vec3 d = D(r, a);
                const vec3 pv = C[std::max(r - 1, 0)];
                b.gr[0] = pv.x; b.gr[1] = pv.y; b.gr[2] = pv.z; b.gr[3] = appear(s[r]);
                b.vert(C[r] + d * rad(s[r]), d, vec4(s[r] / L, s[r], st.seed * 0.37f, 0));
            };
            put(i, a0); put(i, a1); put(i + 1, a1);
            put(i, a0); put(i + 1, a1); put(i + 1, a0);
        }
    }
    b.mat = m0;
    // the leaves (and flowers)
    if (st.leaf > 0.f && st.gap > 0.f) {
        const float gap = st.leaf * st.gap;
        int k = 0;
        for (float sl = gap * (0.5f + 0.5f * h1(st.seed * 1.3f)); sl < L; sl += gap * (0.7f + 0.6f * h1(st.seed + k * 3.1f)), k++) {
            int i = std::min(n - 2, (int)(sl / L * (n - 1)));
            const vec3 p = C[i], t = T[i], nn = path[i].n;
            const float side = (k % 2) ? 1.f : -1.f;
            const float hk = h1(st.seed * 7.7f + k * 1.71f);
            const vec3 sd = normalize(cross(nn, t)) * side;
            const float sz = st.leaf * (0.7f + 0.5f * hk) * lerpf(1.f, 0.6f, sl / L);
            const vec3 d = normalize(sd * 0.9f + t * 0.45f + nn * st.lift * (0.6f + 0.6f * hk));
            const float ln = appear(sl) + 0.35f;
            const float sw0 = b.sw[3];
            b.sw[0] = p.x; b.sw[1] = p.z; b.sw[2] = hk * 12.f; b.sw[3] = st.sway;
            if (st.flower > 0.f && h1(st.seed * 3.3f + k * 0.77f) < st.flower) {
                flower(b, p + nn * sz * 0.25f + sd * sz * 0.3f, normalize(nn + sd * 0.4f), sz * 0.45f, ln + 0.3f, hk);
                if (blooms) blooms->push_back({p + nn * sz * 0.4f, ln + 0.5f, sz});
            }
            leaf(b, p, d, normalize(nn + t * 0.2f), sz, sz * 0.62f, ln, hk);
            b.sw[3] = sw0;
            b.noSway();
        }
    }
    b.gr[3] = -1000.f;
    return L;
}
// a clump of leaves swallowing what is under it: around c on a surface of normal n, radius R, from line ln
void clump(B& b, const vec3& c, const vec3& n0, float R, int count, float leafL, float ln, float span, uint32_t seed, std::vector<Bloom>* blooms = nullptr,
           float flowers = 0.1f) {
    const vec3 n = normalize(n0);
    const vec3 t1 = normalize(cross(n, std::fabs(n.y) < 0.9f ? V3(0, 1, 0) : V3(1, 0, 0))), t2 = cross(n, t1);
    for (int i = 0; i < count; i++) {
        const float h = h1(seed * 0.71f + i * 1.37f), h2 = h1(seed * 1.9f + i * 2.11f), h3 = h1(seed * 3.3f + i * 0.53f);
        const float a = h * TAU_, r = std::sqrt(h2) * R;
        const vec3 radial = t1 * std::cos(a) + t2 * std::sin(a);
        const vec3 at = c + radial * r + n * (R * 0.35f * (1.f - r / R) * h3);
        const vec3 d = normalize(radial * (0.5f + r / R) + n * (0.5f + 0.6f * h3) + t1 * (h3 - 0.5f) * 0.6f);
        const float l = leafL * (0.75f + 0.5f * h3);
        const float lnI = ln + span * (r / R) * 0.8f + span * 0.2f * h;
        b.sw[0] = at.x; b.sw[1] = at.z; b.sw[2] = h * 9.f; b.sw[3] = 0.06f;
        if (h3 < flowers) {
            flower(b, at + n * l * 0.3f, normalize(n + radial * 0.3f), l * 0.4f, lnI + 0.4f, h);
            if (blooms) blooms->push_back({at + n * l * 0.3f, lnI + 0.6f, l});
        } else leaf(b, at, d, normalize(n * 0.8f + radial * 0.3f), l, l * 0.64f, lnI, h, 71.f, leafL < 0.6f ? 1 : 0);
    }
    b.noSway();
}
// a moss cushion on a surface (lumpy, growing out of its foot)
void mossPad(B& b, const vec3& c, float r, float ln) {
    b.gr[0] = c.x; b.gr[1] = c.y; b.gr[2] = c.z; b.gr[3] = ln;
    b.mat = 76.f;
    ellipsoid(b, c - V3(0, 0.3f * r, 0), V3(r, 0.5f * r, r * 0.85f), V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), 12, 6,
              [&](const vec3& q) { return r * 0.18f * (gnoise3(q / r * 3.f + vec3(c.x)) - 0.5f); },
              [&](const vec3& q) { return vec4(q.x / r, q.y / r, q.z / r, c.x * 0.1f); }, 1.9f);
    b.gr[3] = -1000.f;
}
// a tree bursting out (a trunk, its branches, crowns of leaves): base, height, the line it starts at, its seed
void tree(B& b, const vec3& base, float H, float ln, float span, uint32_t seed, int detail, std::vector<Bloom>* blooms = nullptr) {
    VineStyle tr;
    tr.mat = 74.f; tr.leaf = 0.f; tr.r0 = H * 0.045f; tr.rEnd = 0.45f;
    tr.line0 = ln; tr.speed = H * 0.75f / (span * 0.45f);
    tr.seed = seed;
    std::vector<PP> p;
    const float lean = (h1(seed * 0.3f) - 0.5f) * 0.5f;
    const vec3 top = base + V3(lean * H * 0.3f, H * 0.62f, (h1(seed * 0.7f) - 0.5f) * H * 0.12f);
    p.push_back({base, V3(0, 0, 1)});
    walk(p, top, V3(0, 0, 1), H * 0.03f, 1.f, seed * 0.1f, 8);
    vine(b, p, tr);
    const int nb = 4 + (int)(h1(seed * 1.1f) * 2.f);
    for (int k = 0; k < nb; k++) {
        const float hk = h1(seed * 2.3f + k * 1.7f);
        const float a = (k + hk * 0.6f) * TAU_ / nb;
        const float at = 0.55f + 0.45f * (float)k / nb;
        const vec3 from = lerp(base, top, at);
        const vec3 dir = normalize(V3(std::cos(a), 0.55f + 0.5f * hk, std::sin(a) * 0.8f));
        const vec3 to = from + dir * H * (0.3f + 0.2f * hk);
        VineStyle br = tr;
        br.r0 = tr.r0 * 0.5f; br.line0 = ln + span * 0.35f * at + span * 0.05f; br.speed = H * 0.35f / (span * 0.25f);
        br.seed = seed * 3 + k;
        std::vector<PP> q;
        q.push_back({from, V3(0, 0, 1)});
        walk(q, to, V3(0, 0, 1), H * 0.02f, 1.f, hk * 5.f, 5);
        vine(b, q, br);
        // its crown: lumpy clusters of leaves, fringed with real leaves (its silhouette)
        const int nc = detail > 0 ? 3 : 1;
        for (int c = 0; c < nc; c++) {
            const float R = H * (0.14f + 0.06f * h1(seed + k * 4.f + c)) * (detail > 0 ? 1.f : 1.6f);
            const float ca = h1(seed * 5.f + k + c) * TAU_;
            const vec3 cc = to + V3(std::cos(ca) * R * 0.7f * (c > 0), R * 0.3f * c - R * 0.1f, std::sin(ca) * R * 0.7f * (c > 0));
            const float ln0 = br.line0 + span * 0.3f + c * span * 0.08f;
            b.gr[0] = to.x; b.gr[1] = to.y; b.gr[2] = to.z; b.gr[3] = ln0;
            b.mat = 75.f;
            const float sd = seed * 0.37f + k + c * 3.f;
            const int nu = detail > 0 ? 11 : 8, nv = detail > 0 ? 6 : 4;
            ellipsoid(b, cc, V3(R, R * 0.72f, R), V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), nu, nv,
                      [&](const vec3& q2) { return R * 0.25f * (gnoise3(q2 / R * 2.6f + vec3(sd)) - 0.5f); },
                      [&](const vec3& q2) { return vec4(q2.x / R, q2.y / R, q2.z / R, sd); });
            if (detail > 0) {
                const int nl = 16;
                for (int i = 0; i < nl; i++) {
                    const float u = h1(sd * 3.1f + i * 1.3f), v = h1(sd * 1.7f + i * 2.9f);
                    const float th = std::acos(1.f - 1.4f * v), ph = u * TAU_;
                    const vec3 dir(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
                    const vec3 at = cc + V3(dir.x * R * 0.92f, dir.y * R * 0.66f, dir.z * R * 0.92f);
                    const vec3 tg = normalize(cross(dir, V3(0.3f, 1.f, 0.2f)));
                    b.sw[0] = at.x; b.sw[1] = at.z; b.sw[2] = u * 9.f; b.sw[3] = 0.03f;
                    leaf(b, at, normalize(dir + tg * 0.6f), dir, R * 0.42f, R * 0.3f, ln0 + span * 0.1f * u, u);
                }
                b.noSway();
            }
            if (blooms) blooms->push_back({cc + V3(0, R * 0.6f, 0), ln0 + 1.f, R * 0.3f});
        }
    }
    b.gr[3] = -1000.f;
}

// ---------------------------------------------------------------- the lab's furniture
// a lab bench: a dark resin top (its front edge rounded), the cabinet under it (doors, drawers, handles), the plinth
void bench(B& b, float x0, float x1, float z0, float z1) {
    b.mat = 50.f;
    box(b, V3(x0, -0.45f, z0), V3(x1, 0.f, z1));
    b.mat = 50.f;
    pipe(b, {V3(x0, -0.225f, z1), V3(x1, -0.225f, z1)}, 0.225f, vec4(0, 0, 0, 0), 10);
    const float cz1 = z1 - 0.35f;
    b.mat = 51.f;
    box(b, V3(x0 + 0.2f, FLOOR + 0.6f, z0 + 0.2f), V3(x1 - 0.2f, -0.45f, cz1));
    pbox(b, V3(x0 + 0.4f, FLOOR, z0 + 0.4f), V3(x1 - 0.4f, FLOOR + 0.6f, cz1 - 0.4f), paint(0.025f, 0.027f, 0.03f, 0.1f));
    // handles: a drawer's bar, a door's
    b.mat = 52.f;
    const int nd = std::max(1, (int)std::lround((x1 - x0 - 0.4f) / 3.9f));
    const float dw = (x1 - x0 - 0.4f) / nd;
    for (int i = 0; i < nd; i++) {
        const float xc = x0 + 0.2f + dw * (i + 0.5f);
        pipe(b, {V3(xc - 0.6f, -1.05f, cz1 + 0.12f), V3(xc + 0.6f, -1.05f, cz1 + 0.12f)}, 0.06f, vec4(0, 0, 0, 0), 6);
        pipe(b, {V3(xc + dw * 0.5f - 0.45f, -2.4f, cz1 + 0.12f), V3(xc + dw * 0.5f - 0.45f, -3.4f, cz1 + 0.12f)}, 0.06f, vec4(0, 0, 0, 0), 6);
    }
}
// a microscope at o, turned by a
void microscope(B& b, const vec3& o, float a) {
    auto P = [&](float x, float y, float z) { return o + rotY(a, V3(x, y, z)); };
    const vec4 cream = paint(0.62f, 0.6f, 0.53f, 0.55f), black = paint(0.02f, 0.02f, 0.022f, 0.6f), steel = paint(0.5f, 0.52f, 0.55f, 0.8f);
    b.mat = 58.f;
    // the foot: a rounded slab
    lathe(b, P(0, 0, 0.1f), {vec2{0, 0}, vec2{0.95f, 0}, vec2{1.f, 0.06f}, vec2{1.f, 0.26f}, vec2{0.9f, 0.34f}, vec2{0, 0.34f}}, 28, cream, 0.85f);
    // the arm: a C rising from the back
    pipe(b, {P(0, 0.3f, -0.6f), P(0, 1.1f, -0.85f), P(0, 1.9f, -0.75f), P(0, 2.45f, -0.35f), P(0, 2.6f, 0.05f)}, 0.3f, cream, 10);
    // the stage and its clips
    box(b, P(-0.75f, 1.0f, -0.45f), P(0.75f, 1.12f, 0.75f), black);
    pipe(b, {P(-0.45f, 1.16f, 0.15f), P(-0.45f, 1.16f, 0.55f)}, 0.03f, steel, 5);
    pipe(b, {P(0.45f, 1.16f, 0.15f), P(0.45f, 1.16f, 0.55f)}, 0.03f, steel, 5);
    // the body tube, the turret and its objectives
    cyl(b, P(0, 1.95f, 0.15f), 0.22f, 0.8f, 16, black);
    lathe(b, P(0, 1.78f, 0.15f), {vec2{0, 0}, vec2{0.3f, 0}, vec2{0.4f, 0.12f}, vec2{0.3f, 0.2f}, vec2{0, 0.2f}}, 18, steel);
    for (int k = 0; k < 3; k++) {
        const float ak = k * 2.1f + 0.3f;
        const vec3 top = P(std::cos(ak) * 0.2f, 1.8f, 0.15f + std::sin(ak) * 0.2f);
        const vec3 bot = P(std::cos(ak) * 0.26f, 1.38f, 0.15f + std::sin(ak) * 0.26f);
        pipe(b, {top, bot}, 0.07f, k == 0 ? steel : black, 8);
    }
    // the head and its two eyepieces, angled toward us
    box(b, P(-0.32f, 2.6f, -0.2f), P(0.32f, 2.95f, 0.45f), cream);
    for (int s = -1; s <= 1; s += 2) {
        pipe(b, {P(s * 0.14f, 2.85f, 0.25f), P(s * 0.16f, 3.35f, 0.75f)}, 0.1f, black, 10);
    }
    // the focus knobs
    cyl(b, P(-0.62f, 0.85f, -0.62f), 0.0f, 0.f, 4, black);
    for (int s = -1; s <= 1; s += 2) pipe(b, {P(s * 0.28f, 1.0f, -0.65f), P(s * 0.5f, 1.0f, -0.65f)}, 0.2f, black, 12);
    // the lamp under the stage
    cyl(b, P(0, 0.34f, 0.15f), 0.2f, 0.12f, 12, black);
}
// a rack of test tubes at o (their glass: rackGlass)
void rackSolid(B& b, const vec3& o) {
    const vec4 blue = paint(0.05f, 0.16f, 0.45f, 0.5f);
    b.mat = 58.f;
    box(b, o + V3(-1.3f, 0, -0.35f), o + V3(1.3f, 0.08f, 0.35f), blue);
    box(b, o + V3(-1.3f, 0.85f, -0.35f), o + V3(1.3f, 0.95f, 0.35f), blue);
    box(b, o + V3(-1.3f, 0, -0.3f), o + V3(-1.2f, 0.95f, 0.3f), blue);
    box(b, o + V3(1.2f, 0, -0.3f), o + V3(1.3f, 0.95f, 0.3f), blue);
}
const vec3 RACK(3.3f, 0.f, -0.5f);
const float TUBE_LIQ[6][4] = {{0.7f, 0.08f, 0.2f, 0.75f}, {0.1f, 0.55f, 0.2f, 0.5f}, {0.85f, 0.45f, 0.05f, 0.95f},
                              {0.35f, 0.1f, 0.6f, 0.6f}, {0.05f, 0.5f, 0.6f, 0.85f}, {0.8f, 0.75f, 0.2f, 0.4f}};
std::vector<vec2> testTube() {
    std::vector<vec2> p{vec2{0, 0}};
    for (int i = 1; i <= 4; i++) { const float a = i / 4.f * 1.5708f; p.push_back(vec2{0.12f * std::sin(a), 0.12f - 0.12f * std::cos(a)}); }
    p.push_back(vec2{0.12f, 1.55f});
    p.push_back(vec2{0.14f, 1.58f});
    p.push_back(vec2{0, 1.58f});
    return p;
}
// the jar's specimen tag: hanging from a twine round the neck, resting on the shoulder (its print: the shader's)
constexpr float TAG_A = 0.85f;
void tag(B& b) {
    const vec3 dir(std::sin(TAG_A), 0, std::cos(TAG_A)), up(0, 1, 0);
    const vec4 twine = paint(0.42f, 0.32f, 0.18f, 0.05f);
    // the twine round the neck
    std::vector<vec3> ring;
    for (int i = 0; i <= 48; i++) {
        const float a = TAU_ * i / 48;
        ring.push_back(V3(std::sin(a) * 0.634f, 0.985f + 0.004f * std::sin(a * 3.f), std::cos(a) * 0.634f));
    }
    b.mat = 58.f;
    pipe(b, ring, 0.009f, twine, 5);
    // the card
    const float tilt = 0.5f;
    const vec3 nT = normalize(dir * std::cos(tilt) + up * std::sin(tilt));
    const vec3 C = dir * 1.0f + V3(0, 0.6f, 0) + nT * 0.045f;
    const vec3 tx = normalize(cross(up, nT)), ty = cross(nT, tx);
    const float W = 0.15f, H = 0.09f;
    const vec3 c00 = C - tx * W - ty * H, c10 = C + tx * W - ty * H, c11 = C + tx * W + ty * H, c01 = C - tx * W + ty * H;
    b.mat = 57.f;
    b.vert(c00, nT, vec4(0, 0, 0, 0)); b.vert(c10, nT, vec4(1, 0, 0, 0)); b.vert(c11, nT, vec4(1, 1, 0, 0));
    b.vert(c00, nT, vec4(0, 0, 0, 0)); b.vert(c11, nT, vec4(1, 1, 0, 0)); b.vert(c01, nT, vec4(0, 1, 0, 0));
    // the twine down to its eyelet
    const vec3 eye = C + ty * (H * 0.7f) - tx * (W * 0.78f) + nT * 0.004f;
    const vec3 neck = dir * 0.64f + V3(0, 0.985f, 0);
    std::vector<vec3> s;
    for (int i = 0; i <= 10; i++) {
        const float t = i / 10.f;
        s.push_back(lerp(neck, eye, t) + nT * (0.05f * std::sin(t * PI_)) - V3(0, 0.03f * std::sin(t * PI_), 0));
    }
    pipe(b, s, 0.006f, twine, 5);
}
const vec3 TAG_C = [] {
    const vec3 dir(std::sin(TAG_A), 0, std::cos(TAG_A));
    const vec3 nT = normalize(dir * std::cos(0.5f) + V3(0, 1, 0) * std::sin(0.5f));
    return dir * 1.0f + V3(0, 0.6f, 0) + nT * 0.05f;
}();

// a steel shelving unit (x0..x1, z0..z1) with its shelves' heights
void shelving(B& b, float x0, float x1, float z0, float z1, const std::vector<float>& ys) {
    const vec4 steel = paint(0.42f, 0.44f, 0.46f, 0.7f);
    b.mat = 58.f;
    for (int i = 0; i < 4; i++) {
        const float x = (i & 1) ? x1 - 0.2f : x0 + 0.2f, z = (i & 2) ? z1 - 0.2f : z0 + 0.2f;
        pipe(b, {V3(x, FLOOR, z), V3(x, ys.back() + 0.6f, z)}, 0.16f, steel, 8);
    }
    b.mat = 52.f;
    for (float y : ys) box(b, V3(x0, y - 0.12f, z0), V3(x1, y, z1));
}

} // namespace

// =====================================================================================================
// The lab
// =====================================================================================================

void buildLab(std::vector<Vtx>& out) {
    B b{&out};
    const float CON = 60.f, WAL = 53.f, FLR = 54.f, CEI = 55.f;
    const vec4 fac(0.31f, STOREY, 0.f, 1.f); // our building's facade: its seed, storey, ribbon windows, on the street
    // ---- the building around the lab: below it, above it, beside it (their faces into the room the lab's walls)
    {
        const float lower[6] = {CON, CON, -1, FLR, CON, CON};
        boxM(b, V3(-BX, GROUND, BACK), V3(BX, FLOOR, FRONT), lower, fac);
        const float upper[6] = {CON, CON, CEI, CON, CON, CON};
        boxM(b, V3(-BX, CEIL, BACK), V3(BX, ROOF, FRONT), upper, fac);
        const float left[6] = {CON, WAL, -1, -1, CON, CON}, right[6] = {WAL, CON, -1, -1, CON, CON};
        boxM(b, V3(-BX, FLOOR, BACK), V3(-RX, CEIL, FRONT), left, fac);
        boxM(b, V3(RX, FLOOR, BACK), V3(BX, CEIL, FRONT), right, fac);
        // the back wall and its two windows; the street wall and its long window
        const float back[6] = {WAL, WAL, WAL, WAL, CON, WAL};
        boxM(b, V3(-RX, FLOOR, BACK), V3(RX, WIN_Y0, RZ0), back, fac);
        boxM(b, V3(-RX, WIN_Y1, BACK), V3(RX, CEIL, RZ0), back, fac);
        boxM(b, V3(-RX, WIN_Y0, BACK), V3(-BWIN_X1, WIN_Y1, RZ0), back, fac);
        boxM(b, V3(-BWIN_X0, WIN_Y0, BACK), V3(BWIN_X0, WIN_Y1, RZ0), back, fac);
        boxM(b, V3(BWIN_X1, WIN_Y0, BACK), V3(RX, WIN_Y1, RZ0), back, fac);
        const float front[6] = {WAL, WAL, WAL, WAL, WAL, CON};
        boxM(b, V3(-RX, FLOOR, RZ1), V3(RX, WIN_Y0, FRONT), front, fac);
        boxM(b, V3(-RX, WIN_Y1, RZ1), V3(RX, CEIL, FRONT), front, fac);
        boxM(b, V3(-RX, WIN_Y0, RZ1), V3(-FWIN_X, WIN_Y1, FRONT), front, fac);
        boxM(b, V3(FWIN_X, WIN_Y0, RZ1), V3(RX, WIN_Y1, FRONT), front, fac);
    }
    // ---- the windows' frames
    {
        const vec4 alu = paint(0.16f, 0.19f, 0.19f, 0.5f);
        for (int s = -1; s <= 1; s += 2) {
            const float xc = s * (BWIN_X0 + BWIN_X1) * 0.5f;
            pbox(b, V3(xc - 0.3f, WIN_Y0, RZ0 - 1.f), V3(xc + 0.3f, WIN_Y1, RZ0 - 0.5f), alu);
            pbox(b, V3(s * BWIN_X0 - (s > 0 ? 0 : 0.0f) - 0.3f, 6.5f, RZ0 - 1.f), V3(s * BWIN_X0 + 0.3f, 6.9f, RZ0 - 0.5f), alu);
            pbox(b, V3(std::min(s * BWIN_X0, s * BWIN_X1), 6.5f, RZ0 - 1.f), V3(std::max(s * BWIN_X0, s * BWIN_X1), 6.9f, RZ0 - 0.5f), alu);
            for (float x : {BWIN_X0, BWIN_X1}) pbox(b, V3(s * x - 0.35f, WIN_Y0, RZ0 - 1.f), V3(s * x + 0.35f, WIN_Y1, RZ0 - 0.5f), alu);
            pbox(b, V3(std::min(s * BWIN_X0, s * BWIN_X1), WIN_Y0 - 0.1f, RZ0 - 1.2f), V3(std::max(s * BWIN_X0, s * BWIN_X1), WIN_Y0 + 0.35f, RZ0 + 0.5f), alu);
            pbox(b, V3(std::min(s * BWIN_X0, s * BWIN_X1), WIN_Y1 - 0.35f, RZ0 - 1.f), V3(std::max(s * BWIN_X0, s * BWIN_X1), WIN_Y1, RZ0 - 0.5f), alu);
        }
        // the street window: its mullions (none at the middle: the camera leaves through it), a sill outside
        for (float x : {-24.f, -16.f, -8.f, 8.f, 16.f, 24.f}) pbox(b, V3(x - 0.4f, WIN_Y0, FRONT - 0.9f), V3(x + 0.4f, WIN_Y1, FRONT - 0.3f), alu);
        pbox(b, V3(-FWIN_X, WIN_Y1 - 0.4f, FRONT - 0.9f), V3(FWIN_X, WIN_Y1, FRONT - 0.3f), alu);
        pbox(b, V3(-FWIN_X - 1.f, WIN_Y0 - 0.5f, FRONT - 0.5f), V3(FWIN_X + 1.f, WIN_Y0, FRONT + 1.2f), paint(0.32f, 0.31f, 0.29f, 0.1f));
        // inside: a radiator under each back window
        for (int s = -1; s <= 1; s += 2) {
            const float xa = s > 0 ? BWIN_X0 + 1.f : -BWIN_X1 + 1.f;
            for (int k = 0; k < 14; k++) pbox(b, V3(xa + k * 0.8f, FLOOR + 1.f, RZ0 + 0.3f), V3(xa + k * 0.8f + 0.5f, WIN_Y0 - 0.6f, RZ0 + 1.0f), paint(0.62f, 0.62f, 0.6f, 0.4f));
        }
    }
    // ---- the ceiling's lights: housings and their diffusers (each its id: they fail one by one)
    {
        int id = 0;
        for (float z : {-10.f, 6.f, 22.f})
            for (float x : {-16.f, 0.f, 16.f}) {
                pbox(b, V3(x - 4.5f, CEIL - 0.6f, z - 1.2f), V3(x + 4.5f, CEIL, z + 1.2f), paint(0.55f, 0.56f, 0.57f, 0.4f));
                b.mat = 56.f;
                quad(b, V3(x - 4.2f, CEIL - 0.62f, z - 0.95f), V3(x + 4.2f, CEIL - 0.62f, z - 0.95f), V3(x + 4.2f, CEIL - 0.62f, z + 0.95f),
                     V3(x - 4.2f, CEIL - 0.62f, z + 0.95f), V3(0, -1, 0), vec4((float)id, 0, 0, 0));
                id++;
            }
    }
    // ---- the jar's bench, its reagent shelf and what stands on them
    bench(b, -12.f, 12.f, -3.6f, 1.6f);
    {
        const vec4 steel = paint(0.45f, 0.47f, 0.5f, 0.7f);
        b.mat = 58.f;
        for (float x : {-11.f, -3.7f, 3.7f, 11.f}) pipe(b, {V3(x, 0.f, -3.3f), V3(x, 7.2f, -3.3f)}, 0.12f, steel, 8);
        for (float y : {3.1f, 5.9f}) pbox(b, V3(-11.5f, y, -3.6f), V3(11.5f, y + 0.15f, -2.2f), paint(0.68f, 0.68f, 0.66f, 0.3f));
        // reagent bottles: amber, cobalt, white; caps
        const float bx[] = {-10.4f, -9.4f, -8.2f, -6.9f, -5.8f, -2.6f, -1.5f, 1.2f, 2.4f, 5.2f, 6.4f, 7.5f, 8.9f, 10.1f};
        for (int i = 0; i < 14; i++) {
            for (int sh = 0; sh < 2; sh++) {
                const float hh = h1(i * 3.1f + sh * 7.f);
                if (hh < 0.18f) continue;
                const float y = sh == 0 ? 3.25f : 6.05f;
                const float r = 0.32f + 0.18f * h1(i + sh * 11.f), hgt = 0.9f + 0.7f * h1(i * 1.7f + sh);
                const vec3 at(bx[i] + (hh - 0.5f) * 0.3f, y, -2.9f + (h1(i * 5.f + sh) - 0.5f) * 0.5f);
                vec4 col = hh < 0.62f ? paint(0.16f, 0.05f, 0.01f, 0.85f) : (hh < 0.82f ? paint(0.02f, 0.05f, 0.2f, 0.85f) : paint(0.42f, 0.43f, 0.42f, 0.3f));
                b.mat = 58.f;
                lathe(b, at, bottle(r, hgt, r * 0.42f, 0.18f), 18, col);
                cyl(b, at + V3(0, hgt + 0.16f, 0), r * 0.48f, 0.2f, 14, hh < 0.9f ? paint(0.02f, 0.02f, 0.02f, 0.4f) : paint(0.3f, 0.05f, 0.04f, 0.4f));
                // a white label
                if (hh < 0.82f) pbox(b, at + V3(-r * 0.7f, hgt * 0.25f, r * 0.72f), at + V3(r * 0.7f, hgt * 0.55f, r * 1.01f), paint(0.8f, 0.78f, 0.72f, 0.1f));
            }
        }
    }
    // on the bench: the microscope, the rack of test tubes and their liquids, petri dishes' agar, a notebook, the
    // liquids in the flasks behind the jar
    microscope(b, V3(-4.3f, 0.f, -1.0f), 0.35f);
    rackSolid(b, RACK);
    {
        const std::vector<vec2> tt = testTube();
        for (int i = 0; i < 6; i++) {
            const vec3 at = RACK + V3(-1.0f + 0.4f * i, 0.12f, 0.f);
            const float lv = 0.5f + 0.7f * TUBE_LIQ[i][3];
            b.mat = 58.f;
            lathe(b, at, liquid(tt, 0.022f, lv), 12, paint(TUBE_LIQ[i][0], TUBE_LIQ[i][1], TUBE_LIQ[i][2], 0.9f));
        }
        // the petri dishes' agar (their lids: glass)
        for (int k = 0; k < 3; k++) {
            const vec3 at = V3(2.2f + 0.5f * k, 0.f, 1.0f - 0.3f * k) + V3(0, k == 2 ? 0.12f : 0.f, 0);
            b.mat = 66.f;
            cyl(b, at + V3(0, 0.01f, 0), 0.46f, 0.05f, 28, vec4((float)k, 0, 0, 0));
        }
        if (true) {
            // flasks behind the jar: green, blue, a graduated cylinder (their liquids)
            b.mat = 58.f;
            lathe(b, V3(-1.9f, 0.f, -2.4f), liquid(erlenmeyer(0.55f, 0.9f, 0.16f, 0.55f), 0.025f, 0.45f), 22, paint(0.04f, 0.2f, 0.07f, 0.9f));
            lathe(b, V3(2.0f, 0.f, -2.5f), liquid(beaker(0.45f, 1.0f), 0.025f, 0.55f), 22, paint(0.02f, 0.08f, 0.22f, 0.9f));
            lathe(b, V3(-2.9f, 0.f, -2.8f), liquid(beaker(0.2f, 1.6f), 0.02f, 1.1f), 14, paint(0.28f, 0.03f, 0.08f, 0.9f));
        }
        // a notebook and its pencil
        pbox(b, V3(-3.0f, 0.f, 0.35f), V3(-1.55f, 0.08f, 1.4f), paint(0.03f, 0.03f, 0.04f, 0.2f));
        b.mat = 57.f;
        quad(b, V3(-2.95f, 0.085f, 0.4f), V3(-2.95f, 0.085f, 1.35f), V3(-1.6f, 0.085f, 1.35f), V3(-1.6f, 0.085f, 0.4f), V3(0, 1, 0), vec4(0, 0, 1, 0));
        tag(b);
    }
    // ---- the second bench (back left): a centrifuge, a monitor, beakers
    bench(b, -28.f, -14.f, -14.f, -8.5f);
    {
        b.mat = 58.f;
        const vec3 c(-25.f, 0.f, -11.5f);
        lathe(b, c, {vec2{0, 0}, vec2{1.6f, 0}, vec2{1.7f, 0.2f}, vec2{1.7f, 1.3f}, vec2{1.5f, 1.45f}, vec2{0, 1.45f}}, 28, paint(0.75f, 0.76f, 0.78f, 0.5f));
        lathe(b, c + V3(0, 1.45f, 0), {vec2{0, 0}, vec2{1.3f, 0}, vec2{1.25f, 0.25f}, vec2{0, 0.35f}}, 28, paint(0.05f, 0.18f, 0.32f, 0.7f));
        // the monitor: a stand, a screen (lit)
        const vec3 m(-19.f, 0.f, -12.5f);
        pbox(b, m + V3(-0.8f, 0, -0.6f), m + V3(0.8f, 0.12f, 0.6f), paint(0.04f, 0.04f, 0.045f, 0.4f));
        pbox(b, m + V3(-0.18f, 0.1f, -0.25f), m + V3(0.18f, 1.6f, -0.05f), paint(0.04f, 0.04f, 0.045f, 0.4f));
        pbox(b, m + V3(-2.4f, 1.3f, -0.15f), m + V3(2.4f, 4.3f, 0.12f), paint(0.03f, 0.03f, 0.035f, 0.5f));
        b.mat = 59.f;
        quad(b, m + V3(-2.2f, 1.5f, 0.125f), m + V3(2.2f, 1.5f, 0.125f), m + V3(2.2f, 4.1f, 0.125f), m + V3(-2.2f, 4.1f, 0.125f), V3(0, 0, 1), vec4(0, 0, 0, 0));
        // a keyboard
        pbox(b, m + V3(-1.6f, 0, 1.0f), m + V3(1.6f, 0.12f, 2.0f), paint(0.06f, 0.06f, 0.065f, 0.3f));
        b.mat = 58.f;
        lathe(b, V3(-16.5f, 0.f, -10.f), liquid(beaker(0.5f, 1.1f), 0.025f, 0.6f), 20, paint(0.3f, 0.16f, 0.02f, 0.9f));
        lathe(b, V3(-22.f, 0.f, -9.8f), liquid(erlenmeyer(0.6f, 1.f, 0.18f, 0.6f), 0.025f, 0.4f), 20, paint(0.14f, 0.03f, 0.22f, 0.9f));
    }
    // ---- the steel shelving (back right): specimen jars, boxes, bottles
    shelving(b, 14.f, 27.f, -19.8f, -16.4f, {-4.5f, -0.5f, 3.5f, 7.5f});
    {
        for (int sh = 0; sh < 4; sh++) {
            const float y = sh == 0 ? -4.5f : (sh == 1 ? -0.5f : (sh == 2 ? 3.5f : 7.5f));
            float x = 14.8f;
            int k = 0;
            while (x < 26.f) {
                const float hh = h1(sh * 13.f + k * 3.7f);
                if (hh < 0.4f) {
                    // a cardboard box
                    const float w = 1.6f + 1.4f * h1(k + sh * 5.f), hgt = 1.2f + 1.3f * hh;
                    pbox(b, V3(x, y, -19.4f), V3(x + w, y + hgt, -17.f), paint(0.36f, 0.24f, 0.12f, 0.05f));
                    pbox(b, V3(x + 0.2f, y + hgt * 0.4f, -16.99f), V3(x + w - 0.2f, y + hgt * 0.7f, -16.98f), paint(0.75f, 0.73f, 0.68f, 0.05f));
                    x += w + 0.3f;
                } else {
                    // a specimen jar's contents (its glass: buildLabGlass): pale shapes in amber liquid
                    const float r = 0.7f + 0.3f * h1(k * 2.f + sh);
                    const vec3 at(x + r, y, -18.f);
                    b.mat = 58.f;
                    lathe(b, at, liquid(beaker(r, 2.2f), 0.06f, 1.7f), 20, paint(0.45f, 0.32f, 0.06f, 0.8f));
                    lathe(b, at + V3(0, 2.2f, 0), {vec2{0, 0}, vec2{r * 1.04f, 0}, vec2{r * 1.04f, 0.3f}, vec2{0, 0.3f}}, 20, paint(0.08f, 0.08f, 0.09f, 0.4f));
                    x += r * 2.f + 0.4f;
                }
                k++;
            }
        }
    }
}

// the lab's glass: the flasks, beakers and test tubes, the petri dishes' lids, the specimen jars, the back windows' panes
void buildLabGlass(std::vector<Vtx>& out) {
    B b{&out};
    b.mat = 40.f;
    const vec4 f0(0, 0, 0, 0);
    lathe(b, V3(-1.9f, 0.f, -2.4f), erlenmeyer(0.55f, 0.9f, 0.16f, 0.55f), 28, f0);
    lathe(b, V3(2.0f, 0.f, -2.5f), beaker(0.45f, 1.0f), 28, f0);
    lathe(b, V3(-2.9f, 0.f, -2.8f), beaker(0.2f, 1.6f), 16, f0);
    for (int i = 0; i < 6; i++) lathe(b, RACK + V3(-1.0f + 0.4f * i, 0.12f, 0.f), testTube(), 14, f0);
    for (int k = 0; k < 3; k++) {
        const vec3 at = V3(2.2f + 0.5f * k, 0.f, 1.0f - 0.3f * k) + V3(0, k == 2 ? 0.12f : 0.f, 0);
        lathe(b, at, {vec2{0, 0}, vec2{0.5f, 0}, vec2{0.5f, 0.1f}, vec2{0.52f, 0.12f}, vec2{0, 0.12f}}, 28, f0);
    }
    lathe(b, V3(-16.5f, 0.f, -10.f), beaker(0.5f, 1.1f), 24, f0);
    lathe(b, V3(-22.f, 0.f, -9.8f), erlenmeyer(0.6f, 1.f, 0.18f, 0.6f), 24, f0);
    for (int sh = 0; sh < 4; sh++) {
        const float y = sh == 0 ? -4.5f : (sh == 1 ? -0.5f : (sh == 2 ? 3.5f : 7.5f));
        float x = 14.8f;
        int k = 0;
        while (x < 26.f) {
            const float hh = h1(sh * 13.f + k * 3.7f);
            if (hh < 0.4f) x += 1.6f + 1.4f * h1(k + sh * 5.f) + 0.3f;
            else {
                const float r = 0.7f + 0.3f * h1(k * 2.f + sh);
                lathe(b, V3(x + r, y, -18.f), beaker(r, 2.2f), 24, f0);
                x += r * 2.f + 0.4f;
            }
            k++;
        }
    }
    // the back windows' panes
    for (int s = -1; s <= 1; s += 2) {
        const float xa = std::min(s * BWIN_X0, s * BWIN_X1), xb = std::max(s * BWIN_X0, s * BWIN_X1);
        quad(b, V3(xa, WIN_Y0, RZ0 - 0.75f), V3(xb, WIN_Y0, RZ0 - 0.75f), V3(xb, WIN_Y1, RZ0 - 0.75f), V3(xa, WIN_Y1, RZ0 - 0.75f), V3(0, 0, 1), f0);
    }
}

// =====================================================================================================
// The city
// =====================================================================================================

namespace {
struct Bld { float x0, x1, z0, z1, h, storey; int style; float seed; int rank; };

// neon text on a board facing +z: its strokes from x (left), y (baseline), letter height h, the tube's radius
void neonText(B& b, const std::string& t, vec3 o, float h, bool vertical, int color, float sign, std::vector<NeonPt>& neon) {
    const float r = h * 0.055f, w = h * 0.62f, adv = h * 0.86f;
    const vec4 f((float)color, sign, 0, 0);
    for (size_t i = 0; i < t.size(); i++) {
        const vec3 at = vertical ? o + V3(-w * 0.5f, -(float)i * h * 1.25f, 0) : o + V3((float)i * adv, 0, 0);
        for (const Stroke& s : glyph(t[i])) {
            std::vector<vec3> pts;
            for (const vec2& q : s) pts.push_back(at + V3(q.x * (t[i] == '+' ? h : w), q.y * h, 0));
            b.mat = 63.f;
            pipe(b, pts, r, f, 6);
            for (size_t k = 0; k + 1 < pts.size(); k++) {
                const float L = length(pts[k + 1] - pts[k]);
                const int n = std::max(1, (int)(L / (h * 0.35f)));
                for (int j = 0; j < n; j++) neon.push_back({lerp(pts[k], pts[k + 1], (j + 0.5f) / n), color, sign});
            }
        }
    }
}
float textW(const std::string& t, float h) { return (float)t.size() * h * 0.86f - h * 0.24f; }
} // namespace

void buildCity(std::vector<Vtx>& out, std::vector<NeonPt>& neon, std::vector<CityLight>& lights, std::vector<Bloom>& blooms) {
    B b{&out};
    Rng r(0xC17E5EEDull);
    std::vector<Bld> blds;
    // the street's row (facing +z at FRONT), both ways from ours
    for (int side = -1; side <= 1; side += 2) {
        float x = BX;
        int k = 0;
        while (x < 1100.f) {
            const float w = r.range(60.f, 120.f);
            Bld bd;
            bd.storey = r.range(21.f, 25.f);
            const int floors = k == 0 ? (side < 0 ? 8 : 6) : r.irange(3, 10);
            bd.h = bd.storey * floors + r.range(0.f, 4.f);
            bd.style = k == 0 ? (side < 0 ? 1 : 0) : r.irange(0, 2);
            bd.z1 = FRONT - (r.chance(0.3f) ? r.range(0.f, 6.f) : 0.f);
            bd.z0 = FRONT - r.range(80.f, 130.f);
            bd.x0 = side > 0 ? x : -x - w;
            bd.x1 = side > 0 ? x + w : -x;
            bd.seed = r.uniform();
            bd.rank = 0;
            blds.push_back(bd);
            x += w + (r.chance(0.25f) ? 16.f : 0.f);
            k++;
        }
    }
    // the rows behind ours, taller and taller
    for (int row = 0; row < 8; row++) {
        const float zf = BACK - 70.f - row * 200.f;
        const float span = 500.f + row * 220.f;
        float x = -span;
        while (x < span) {
            const float w = r.range(70.f, 150.f + row * 20.f);
            Bld bd;
            bd.storey = r.range(21.f, 25.f);
            const float centre = 1.f - std::min(1.f, std::fabs(x + w * 0.5f) / span);
            const int floors = std::min(r.chance(0.06f) ? 34 : 20, std::max(3, (int)(r.range(3.f, 8.f) + row * 1.6f * r.range(0.3f, 1.2f) + centre * row * 1.2f)));
            bd.h = bd.storey * floors;
            bd.style = r.irange(0, 2);
            if (floors > 14 && r.chance(0.6f)) bd.style = 2;
            bd.x0 = x;
            bd.x1 = x + w;
            bd.z1 = zf - r.range(0.f, 10.f);
            bd.z0 = zf - r.range(90.f, 160.f);
            bd.seed = r.uniform();
            bd.rank = row + 1;
            blds.push_back(bd);
            x += w + (r.chance(0.3f) ? r.range(30.f, 45.f) : r.range(0.f, 6.f));
        }
    }
    // ---- the ground: asphalt everywhere, the sidewalk along the street's row
    b.mat = 61.f;
    quad(b, V3(-4000, GROUND, -4000), V3(4000, GROUND, -4000), V3(4000, GROUND, 1500), V3(-4000, GROUND, 1500), V3(0, 1, 0));
    b.mat = 62.f;
    box(b, V3(-1200.f, GROUND, BACK - 14.f), V3(1200.f, GROUND + 0.9f, FRONT + 15.f));
    // ---- the buildings
    for (const Bld& bd : blds) {
        const float top = GROUND + bd.h;
        const vec4 f(bd.seed, bd.storey, (float)bd.style, bd.rank == 0 ? 1.f : 0.f);
        b.mat = 60.f;
        box(b, V3(bd.x0, GROUND, bd.z0), V3(bd.x1, top, bd.z1), f);
        const bool near = bd.rank == 0 || (bd.rank == 1 && std::fabs(bd.x0 + bd.x1) < 900.f);
        const float w = bd.x1 - bd.x0, d = bd.z1 - bd.z0;
        // a setback on the tall ones
        float roof = top;
        if (bd.h > 200.f && bd.seed > 0.35f) {
            const float inset = std::min(w, d) * 0.18f;
            const float h2 = bd.storey * (2 + (int)(bd.seed * 5.f));
            box(b, V3(bd.x0 + inset, top, bd.z0 + inset), V3(bd.x1 - inset, top + h2, bd.z1 - inset), f);
            roof = top + h2;
        }
        // the parapet
        const float pw = 0.8f, ph = 2.2f;
        const vec4 pf(bd.seed, bd.storey, 9.f, 0.f);
        b.mat = 60.f;
        box(b, V3(bd.x0, top, bd.z0), V3(bd.x1, top + ph, bd.z0 + pw), pf);
        box(b, V3(bd.x0, top, bd.z1 - pw), V3(bd.x1, top + ph, bd.z1), pf);
        box(b, V3(bd.x0, top, bd.z0), V3(bd.x0 + pw, top + ph, bd.z1), pf);
        box(b, V3(bd.x1 - pw, top, bd.z0), V3(bd.x1, top + ph, bd.z1), pf);
        // the floors' ledges on the street's face (the near ones)
        if (near && bd.style != 2) {
            for (float y = GROUND + bd.storey; y < top - 2.f; y += bd.storey)
                box(b, V3(bd.x0 - 0.3f, y - 0.5f, bd.z1), V3(bd.x1 + 0.3f, y + 0.4f, bd.z1 + 1.2f), pf);
        }
        // the roof's clutter: a water tank on legs, air conditioners, a stair hut, an antenna
        if (near || bd.rank <= 3) {
            Rng q((uint64_t)(bd.seed * 1e6f) + 17);
            const float cx = bd.x0 + w * q.range(0.25f, 0.75f), cz = bd.z0 + d * q.range(0.25f, 0.75f);
            if (q.chance(0.7f)) {
                const vec4 wood = paint(0.24f, 0.15f, 0.09f, 0.1f);
                const float R = q.range(4.f, 6.f), H = q.range(7.f, 10.f);
                b.mat = 58.f;
                for (int k = 0; k < 4; k++) pipe(b, {V3(cx + (k & 1 ? R : -R) * 0.7f, roof, cz + (k & 2 ? R : -R) * 0.7f), V3(cx + (k & 1 ? R : -R) * 0.7f, roof + 5.f, cz + (k & 2 ? R : -R) * 0.7f)}, 0.35f, paint(0.1f, 0.1f, 0.1f, 0.3f), 5);
                lathe(b, V3(cx, roof + 5.f, cz), {vec2{0, 0}, vec2{R, 0}, vec2{R, 0}, vec2{R, H}, vec2{R, H}, vec2{R * 1.05f, H}, vec2{0, H + R * 0.6f}}, 12, wood);
            }
            const int nac = q.irange(1, 4);
            for (int k = 0; k < nac; k++) {
                const float ax = bd.x0 + w * q.range(0.1f, 0.9f), az = bd.z0 + d * q.range(0.1f, 0.9f);
                pbox(b, V3(ax - 3.f, roof, az - 2.f), V3(ax + 3.f, roof + 3.5f, az + 2.f), paint(0.5f, 0.5f, 0.48f, 0.3f));
            }
            if (q.chance(0.6f)) {
                const float hx = bd.x0 + w * q.range(0.15f, 0.4f), hz = bd.z0 + d * q.range(0.15f, 0.4f);
                b.mat = 60.f;
                box(b, V3(hx - 6.f, roof, hz - 5.f), V3(hx + 6.f, roof + 12.f, hz + 5.f), vec4(bd.seed, 50.f, 9.f, 0.f));
            }
            if (q.chance(0.4f)) {
                b.mat = 58.f;
                pipe(b, {V3(cx + 8.f, roof, cz - 6.f), V3(cx + 8.f, roof + q.range(20.f, 45.f), cz - 6.f)}, 0.4f, paint(0.2f, 0.2f, 0.22f, 0.5f), 5);
            }
        }
    }
    // our building's roof: a parapet, a water tank, air conditioners
    {
        b.mat = 60.f;
        const vec4 pf(0.31f, STOREY, 9.f, 0.f);
        box(b, V3(-BX, ROOF, BACK), V3(BX, ROOF + 2.2f, BACK + 0.8f), pf);
        box(b, V3(-BX, ROOF, FRONT - 0.8f), V3(BX, ROOF + 2.2f, FRONT), pf);
        box(b, V3(-BX, ROOF, BACK), V3(-BX + 0.8f, ROOF + 2.2f, FRONT), pf);
        box(b, V3(BX - 0.8f, ROOF, BACK), V3(BX, ROOF + 2.2f, FRONT), pf);
        for (int k = 0; k < 4; k++) pbox(b, V3(-40.f + k * 9.f, ROOF, 0.f), V3(-34.f + k * 9.f, ROOF + 3.5f, 4.f), paint(0.5f, 0.5f, 0.48f, 0.3f));
        // the floors' ledges on its street face
        for (float y = GROUND + STOREY; y < ROOF - 2.f; y += STOREY) {
            if (std::fabs(y - (FLOOR + 0.f)) < 1.f || std::fabs(y - CEIL - 3.5f) < 1.f) {}
            box(b, V3(-BX - 0.3f, y - 0.5f + 4.f - 0.4f, FRONT), V3(BX + 0.3f, y + 4.f - 0.4f, FRONT + 1.4f), pf);
        }
    }
    // ---- the street: lamps along the sidewalk, abandoned cars on the road
    for (float x = -420.f; x <= 420.f; x += 84.f) {
        const float px = x + 30.f, pz = FRONT + 12.f;
        b.mat = 58.f;
        const vec4 pole = paint(0.08f, 0.09f, 0.09f, 0.5f);
        pipe(b, {V3(px, GROUND, pz), V3(px, GROUND + 36.f, pz), V3(px, GROUND + 40.f, pz + 3.f), V3(px, GROUND + 40.5f, pz + 9.f)}, 0.45f, pole, 6);
        pbox(b, V3(px - 1.3f, GROUND + 39.2f, pz + 6.5f), V3(px + 1.3f, GROUND + 40.6f, pz + 11.f), pole);
        b.mat = 64.f;
        quad(b, V3(px - 1.1f, GROUND + 39.15f, pz + 6.8f), V3(px + 1.1f, GROUND + 39.15f, pz + 6.8f), V3(px + 1.1f, GROUND + 39.15f, pz + 10.7f),
             V3(px - 1.1f, GROUND + 39.15f, pz + 6.8f + 3.9f), V3(0, -1, 0));
        lights.push_back({V3(px, GROUND + 36.f, pz + 9.f), 55.f, 3, 0.f});
    }
    {
        Rng q(0xCA25);
        const float cars[][3] = {{-120.f, 62.f, 0.1f}, {-35.f, 88.f, 3.0f}, {70.f, 64.f, -0.15f}, {150.f, 86.f, 3.2f}, {-230.f, 70.f, 0.3f}, {260.f, 66.f, 0.05f}};
        for (auto& c : cars) {
            const vec3 o(c[0], GROUND, FRONT + c[1] - 35.f + 0.f);
            const float a = c[2];
            const vec4 col = q.chance(0.5f) ? paint(0.25f, 0.06f, 0.04f, 0.4f) : (q.chance(0.5f) ? paint(0.1f, 0.18f, 0.22f, 0.4f) : paint(0.55f, 0.5f, 0.38f, 0.4f));
            auto P = [&](float x, float y, float z) { return o + rotY(a, V3(x, y, z)); };
            b.mat = 58.f;
            // body and cabin as two lathes squashed (rounded): boxes, turned
            auto rbox = [&](float x0, float x1, float y0, float y1, float z0, float z1, const vec4& cc) {
                const vec3 cs[8] = {P(x0, y0, z0), P(x1, y0, z0), P(x0, y1, z0), P(x1, y1, z0), P(x0, y0, z1), P(x1, y0, z1), P(x0, y1, z1), P(x1, y1, z1)};
                const vec3 nx = rotY(a, V3(1, 0, 0)), nz = rotY(a, V3(0, 0, 1));
                const float m0 = b.mat; b.mat = 58.f;
                quad(b, cs[0], cs[4], cs[6], cs[2], nx * -1.f, cc); quad(b, cs[1], cs[3], cs[7], cs[5], nx, cc);
                quad(b, cs[2], cs[6], cs[7], cs[3], V3(0, 1, 0), cc); quad(b, cs[0], cs[2], cs[3], cs[1], nz * -1.f, cc);
                quad(b, cs[4], cs[5], cs[7], cs[6], nz, cc);
                b.mat = m0;
            };
            rbox(-15.f, 15.f, 1.6f, 6.f, -6.f, 6.f, col);
            rbox(-7.f, 7.f, 6.f, 10.f, -5.4f, 5.4f, col);
            rbox(-6.6f, 6.6f, 6.4f, 9.6f, -5.5f, 5.5f, paint(0.03f, 0.04f, 0.05f, 0.9f));
            for (int k = 0; k < 4; k++) {
                const float wx = (k & 1) ? 9.5f : -9.5f, wz = (k & 2) ? 6.f : -6.f;
                std::vector<vec3> ax{P(wx, 2.4f, wz - (k & 2 ? 1.4f : -1.4f)), P(wx, 2.4f, wz)};
                pipe(b, ax, 2.4f, paint(0.02f, 0.02f, 0.02f, 0.2f), 12);
            }
        }
    }
    // ---- neon signs on the street's facades: their boards, their letters, their light
    {
        int sign = 0;
        auto board = [&](float x0, float x1, float y0, float y1, float z) {
            pbox(b, V3(x0, y0, z), V3(x1, y1, z + 1.2f), paint(0.025f, 0.025f, 0.03f, 0.5f));
        };
        auto horiz = [&](const std::string& t, float xc, float y, float h, float z, int color) {
            const float W = textW(t, h);
            board(xc - W * 0.5f - h * 0.4f, xc + W * 0.5f + h * 0.4f, y - h * 0.4f, y + h * 1.4f, z);
            neonText(b, t, V3(xc - W * 0.5f, y, z + 1.2f + h * 0.08f), h, false, color, (float)sign, neon);
            lights.push_back({V3(xc, y + h * 0.5f, z + 6.f), std::max(30.f, W * 0.9f), color, (float)sign});
            sign++;
        };
        auto vert = [&](const std::string& t, float xc, float ytop, float h, float z, int color) {
            const float H = (t.size() - 1) * h * 1.25f + h;
            board(xc - h * 0.65f, xc + h * 0.65f, ytop - H + h - h * 0.4f, ytop + h * 1.4f, z);
            neonText(b, t, V3(xc, ytop, z + 1.2f + h * 0.08f), h, true, color, (float)sign, neon);
            lights.push_back({V3(xc, ytop - H * 0.5f + h, z + 6.f), std::max(35.f, H * 0.7f), color, (float)sign});
            sign++;
        };
        // ours: a pharmacy on the ground floor, its green cross
        horiz("PHARMACY", -8.f, GROUND + 15.f, 4.5f, FRONT + 0.2f, 1);
        neonText(b, "+", V3(32.f, GROUND + 12.f, FRONT + 1.6f), 9.f, false, 1, (float)sign, neon);
        pbox(b, V3(31.f, GROUND + 11.f, FRONT + 0.2f), V3(42.f, GROUND + 22.f, FRONT + 1.4f), paint(0.025f, 0.025f, 0.03f, 0.5f));
        lights.push_back({V3(36.f, GROUND + 16.f, FRONT + 6.f), 30.f, 1, (float)sign});
        sign++;
        // the neighbours
        const Bld* L1 = nullptr; const Bld* R1 = nullptr; const Bld* L2 = nullptr; const Bld* R2 = nullptr;
        for (const Bld& bd : blds) {
            if (bd.rank != 0) continue;
            if (bd.x1 <= -BX + 0.1f && (!L1 || bd.x1 > L1->x1)) L1 = &bd;
            if (bd.x0 >= BX - 0.1f && (!R1 || bd.x0 < R1->x0)) R1 = &bd;
        }
        for (const Bld& bd : blds) {
            if (bd.rank != 0) continue;
            if (L1 && bd.x1 <= L1->x0 + 0.1f && (!L2 || bd.x1 > L2->x1)) L2 = &bd;
            if (R1 && bd.x0 >= R1->x1 - 0.1f && (!R2 || bd.x0 < R2->x0)) R2 = &bd;
        }
        if (L1) {
            vert("HOTEL", L1->x1 - 12.f, GROUND + std::min(L1->h - 20.f, 120.f), 9.f, L1->z1 + 0.2f, 0);
            horiz("BAR", (L1->x0 + L1->x1) * 0.5f - 10.f, GROUND + 14.f, 5.f, L1->z1 + 0.2f, 2);
        }
        if (R1) {
            horiz("NOODLES", (R1->x0 + R1->x1) * 0.5f, GROUND + 15.f, 4.5f, R1->z1 + 0.2f, 2);
            horiz("OPEN 24H", R1->x0 + 20.f, GROUND + 33.f, 3.f, R1->z1 + 0.2f, 0);
            vert("KARAOKE", R1->x1 - 10.f, GROUND + std::min(R1->h - 15.f, 140.f), 7.f, R1->z1 + 0.2f, 1);
        }
        if (L2) {
            horiz("LIVE", (L2->x0 + L2->x1) * 0.5f, GROUND + 38.f, 7.f, L2->z1 + 0.2f, 1);
            horiz("CAFE", (L2->x0 + L2->x1) * 0.5f + 12.f, GROUND + 14.f, 4.5f, L2->z1 + 0.2f, 0);
        }
        if (R2) {
            horiz("HOTEL", (R2->x0 + R2->x1) * 0.5f, GROUND + R2->h + 4.f, 11.f, R2->z1 - 8.f, 2);
            horiz("BAR", (R2->x0 + R2->x1) * 0.5f, GROUND + 14.f, 5.f, R2->z1 + 0.2f, 0);
        }
    }
    // ---- the overgrowth of the neighbours (from 41): vines down from their roofs and up from the street, clumps on
    // their faces, trees on their roofs; crowns of trees on the roofs behind
    {
        VineStyle cv;
        cv.r0 = 0.3f; cv.rEnd = 0.4f; cv.leaf = 2.2f; cv.gap = 0.75f; cv.flower = 0.05f; cv.lift = 0.45f; cv.sway = 0.03f;
        const vec3 up(0, 1, 0), fz(0, 0, 1);
        int k = 0;
        for (const Bld& n : blds) {
            const float cx = (n.x0 + n.x1) * 0.5f;
            const float top = GROUND + n.h;
            if (n.rank == 0 && std::fabs(cx) < 650.f) {
                const float dist = std::fabs(cx) / 650.f;
                const int nv = 3 + (int)(h1(k * 3.f) * 4.f);
                for (int j = 0; j < nv; j++) {
                    const float hj = h1(k * 11.f + j * 1.3f);
                    const float x = n.x0 + (n.x1 - n.x0) * (j + 0.5f) / nv + (hj - 0.5f) * 8.f;
                    std::vector<PP> p;
                    if (j % 2 == 0) {
                        p.push_back({V3(x, top + 2.f, n.z1 - 0.4f), up});
                        p.push_back({V3(x, top + 1.f, n.z1 + 0.6f), fz});
                        walk(p, V3(x + (hj - 0.5f) * 25.f, top - n.h * (0.4f + 0.55f * hj), n.z1 + 0.6f), fz, 3.f, 1.5f, hj * 4.f, 30);
                    } else {
                        p.push_back({V3(x, GROUND + 0.9f, n.z1 + 3.f), up});
                        p.push_back({V3(x, GROUND + 1.5f, n.z1 + 0.6f), fz});
                        walk(p, V3(x + (hj - 0.5f) * 25.f, GROUND + n.h * (0.35f + 0.5f * hj), n.z1 + 0.6f), fz, 3.f, 1.5f, hj * 4.f, 30);
                    }
                    VineStyle v = cv; v.line0 = 41.f + dist * 4.f + hj * 2.f; v.speed = 14.f; v.seed = 700 + k * 10 + j;
                    vine(b, p, v, &blooms);
                }
                for (int j = 0; j < 5; j++) {
                    const float hj = h1(k * 5.f + j * 2.7f);
                    const float x = n.x0 + 6.f + (n.x1 - n.x0 - 12.f) * hj;
                    const float y = GROUND + 4.f + (n.h - 10.f) * h1(k * 9.f + j);
                    clump(b, V3(x, y, n.z1 + 1.f), normalize(fz + up * 0.6f), 6.f, 22, 2.6f, 42.f + dist * 4.f + hj * 2.f, 2.f, 800 + k * 10 + j, &blooms);
                }
                if (h1(k * 13.f) < 0.7f)
                    tree(b, V3(cx + (h1((float)k) - 0.5f) * 30.f, top, n.z1 - 25.f), 30.f + 30.f * h1(k * 2.f), 43.f + dist * 4.f, 4.f, 900 + k, 1, &blooms);
            } else if (n.rank > 0 && h1(k * 1.9f + 0.3f) < 0.55f) {
                // crowns on a roof behind (simpler the farther)
                const float d = std::min(1.f, std::fabs(n.z1) / 1500.f);
                const int nc = 1 + (int)(h1(k * 2.3f) * 3.f);
                for (int c = 0; c < nc; c++) {
                    const float R = std::min(n.x1 - n.x0, n.z1 - n.z0) * (0.12f + 0.1f * h1(k * 3.1f + c));
                    const float x = n.x0 + R + (n.x1 - n.x0 - 2.f * R) * h1(k * 4.3f + c);
                    const float z = n.z0 + R + (n.z1 - n.z0 - 2.f * R) * h1(k * 5.7f + c);
                    b.gr[0] = x; b.gr[1] = top; b.gr[2] = z; b.gr[3] = 42.f + d * 6.f + 1.5f * h1(k * 6.1f + c);
                    b.mat = 75.f;
                    const float sd = h1(k * 7.3f + c) * 10.f;
                    const int nu = n.rank <= 2 ? 12 : 8, nvv = n.rank <= 2 ? 7 : 4;
                    ellipsoid(b, V3(x, top + R * 0.45f, z), V3(R, R * 0.7f, R), V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), nu, nvv,
                              [&](const vec3& q) { return R * 0.25f * (gnoise3(q / R * 2.5f + vec3(sd)) - 0.5f); },
                              [&](const vec3& q) { return vec4(q.x / R, q.y / R, q.z / R, sd); });
                    b.gr[3] = -1000.f;
                    if (n.rank <= 2) blooms.push_back({V3(x, top + R, z), 44.f + d * 6.f, R * 0.3f});
                }
            }
            k++;
        }
    }
}

// =====================================================================================================
// The growth
// =====================================================================================================

void buildGrowth(std::vector<Vtx>& out, std::vector<Bloom>& blooms, size_t* benchEnd) {
    B b{&out};
    const float cu = CRACK_U;
    const vec3 cdir(std::sin(cu), 0.f, std::cos(cu));          // toward the crack
    const vec3 ctan(std::cos(cu), 0.f, -std::sin(cu));         // along the glass (toward +u)
    const vec3 E = cdir * 1.0f + V3(0, 0.24f, 0);              // the crack's foot, at the soil
    const vec3 up(0, 1, 0), fz(0, 0, 1), fx(1, 0, 0);
    // ---------------- out of the jar, over the bench (on its own clock, from 17.5)
    // the soil spilled out of the crack
    b.gr[0] = E.x; b.gr[1] = 0.f; b.gr[2] = E.z; b.gr[3] = 17.6f;
    b.mat = 65.f;
    ellipsoid(b, cdir * 1.1f + V3(0, -0.01f, 0), V3(0.22f, 0.055f, 0.14f), ctan, up, cdir, 18, 7,
              [&](const vec3& q) { return 0.04f * (gnoise3(q * 9.f) - 0.5f); }, [](const vec3& q) { return vec4(q.x, q.y, q.z, 0); }, 1.7f);
    b.gr[3] = -1000.f;
    // the glass at the crack: a path down its outside to the bench
    auto downGlass = [&](std::vector<PP>& p, float du) {
        const vec3 d(std::sin(cu + du), 0, std::cos(cu + du));
        p.push_back({d * 0.99f + V3(0, 0.26f, 0), d});
        p.push_back({d * 1.02f + V3(0, 0.15f, 0), normalize(d + up * 0.2f)});
        p.push_back({d * 1.0f + V3(0, 0.06f, 0), normalize(d + up * 0.5f)});
        p.push_back({d * 1.1f + V3(0, 0.0f, 0), up});
    };
    // roots: thin, jagged, branching, flat on the bench
    for (int k = 0; k < 7; k++) {
        const float hk = h1(k * 2.3f + 0.4f);
        std::vector<PP> p;
        downGlass(p, (k - 3) * 0.06f);
        const float a = cu + (k - 3) * 0.28f + (hk - 0.5f) * 0.2f;
        const vec3 to = V3(std::sin(a), 0, std::cos(a)) * (1.9f + 1.2f * hk) + V3(0, 0.f, 0);
        walk(p, to, up, 0.12f, 2.5f + hk * 2.f, hk * 7.f, 26);
        VineStyle st;
        st.mat = 73.f; st.leaf = 0.f; st.r0 = 0.012f + 0.006f * hk; st.rEnd = 0.25f;
        st.line0 = 17.6f + 0.3f * hk; st.speed = 0.55f; st.seed = 100 + k;
        vine(b, p, st);
        // a branch or two
        for (int j = 0; j < 2; j++) {
            const float t = 0.45f + 0.3f * j;
            const PP& q0 = p[(size_t)(t * (p.size() - 1))];
            const float a2 = a + (j ? 0.6f : -0.6f) * (0.5f + hk);
            std::vector<PP> q{q0};
            walk(q, q0.p + V3(std::sin(a2), 0, std::cos(a2)) * (0.5f + 0.6f * h1(k + j * 9.f)), up, 0.06f, 3.f, hk * 3.f + j, 10);
            VineStyle s2 = st;
            s2.r0 *= 0.55f; s2.seed = 200 + k * 3 + j;
            s2.line0 = st.line0 + (t * 2.f) / st.speed * 0.5f;
            vine(b, q, s2);
        }
    }
    VineStyle vs;
    vs.r0 = 0.022f; vs.rEnd = 0.4f; vs.leaf = 0.075f; vs.gap = 0.9f; vs.flower = 0.08f; vs.lift = 0.6f; vs.sway = 0.05f;
    // V1: off the front of the bench, down the cabinet, onto the floor
    {
        std::vector<PP> p;
        downGlass(p, -0.05f);
        walk(p, V3(-1.35f, 0.f, 1.35f), up, 0.1f, 1.5f, 0.3f, 16);
        walk(p, V3(-1.7f, 0.f, 1.62f), up, 0.04f, 1.f, 0.f, 4);
        p.push_back({V3(-1.75f, -0.2f, 1.86f), normalize(up + fz)});
        walk(p, V3(-1.85f, -0.6f, 1.36f), fz, 0.0f, 1.f, 0.f, 3);
        walk(p, V3(-2.4f, -3.2f, 1.36f), fz, 0.25f, 1.2f, 1.f, 16);
        walk(p, V3(-2.6f, -5.9f, 1.4f), fz, 0.2f, 1.f, 2.f, 14);
        walk(p, V3(-3.4f, FLOOR, 3.5f), up, 0.3f, 1.f, 0.5f, 12);
        VineStyle s = vs; s.line0 = 18.0f; s.speed = 1.15f; s.seed = 1;
        vine(b, p, s, &blooms);
    }
    // V2: across the bench to the microscope, up its arm to its eyepieces
    {
        std::vector<PP> p;
        downGlass(p, 0.07f);
        walk(p, V3(-2.0f, 0.f, 0.35f), up, 0.12f, 1.2f, 1.1f, 14);
        walk(p, V3(-3.5f, 0.f, -0.2f), up, 0.1f, 1.3f, 0.2f, 12);
        // up the foot and the arm (the microscope at (-4.3, 0, -1.0), turned 0.35)
        auto M = [&](float x, float y, float z) { return V3(-4.3f, 0.f, -1.0f) + rotY(0.35f, V3(x, y, z)); };
        p.push_back({M(0.6f, 0.36f, -0.2f), up});
        p.push_back({M(0.3f, 0.36f, -0.55f), up});
        p.push_back({M(0.25f, 0.9f, -0.9f), normalize(M(1, 0, 0) - M(0, 0, 0))});
        p.push_back({M(0.2f, 1.6f, -1.1f), normalize(M(0, 0, -1) - M(0, 0, 0))});
        p.push_back({M(-0.15f, 2.2f, -1.05f), normalize(M(0, 0, -1) - M(0, 0, 0))});
        p.push_back({M(-0.3f, 2.7f, -0.6f), up});
        p.push_back({M(-0.32f, 3.0f, -0.1f), up});
        p.push_back({M(-0.2f, 3.15f, 0.5f), up});
        VineStyle s = vs; s.line0 = 18.4f; s.speed = 0.95f; s.seed = 2; s.flower = 0.15f;
        vine(b, p, s, &blooms);
        clump(b, M(0.f, 3.f, 0.1f), up, 0.45f, 26, 0.12f, 24.5f, 1.5f, 77, &blooms, 0.15f);
    }
    // V3: round the jar's foot to the right, to the rack of test tubes, up its frame
    {
        std::vector<PP> p;
        downGlass(p, 0.1f);
        for (int i = 1; i <= 14; i++) {
            const float a = cu + 0.2f + i * 0.11f;
            p.push_back({V3(std::sin(a), 0, std::cos(a)) * (1.1f + 0.05f * std::sin(i * 1.7f)), up});
        }
        walk(p, V3(1.6f, 0.f, 0.75f), up, 0.1f, 1.2f, 0.4f, 8);
        walk(p, RACK + V3(-1.35f, 0.f, 0.4f), up, 0.1f, 1.f, 0.3f, 10);
        walk(p, RACK + V3(-1.32f, 0.95f, 0.36f), fz, 0.04f, 1.f, 0.f, 6);
        walk(p, RACK + V3(0.2f, 0.98f, 0.3f), up, 0.06f, 1.5f, 0.f, 10);
        walk(p, RACK + V3(1.0f, 1.4f, 0.1f), up, 0.05f, 1.f, 0.f, 6);
        VineStyle s = vs; s.line0 = 19.2f; s.speed = 0.95f; s.seed = 3;
        vine(b, p, s, &blooms);
        // and down the rack's far side to the petri dishes
        std::vector<PP> q;
        q.push_back({RACK + V3(0.2f, 0.98f, 0.3f), up});
        walk(q, RACK + V3(1.0f, 0.95f, 0.36f), up, 0.03f, 1.f, 0.f, 4);
        walk(q, RACK + V3(1.2f, 0.f, 0.45f), fz, 0.05f, 1.f, 0.f, 6);
        walk(q, V3(2.9f, 0.08f, 0.75f), up, 0.1f, 1.f, 1.f, 8);
        VineStyle s2 = vs; s2.line0 = 23.f; s2.speed = 0.8f; s2.seed = 31; s2.flower = 0.2f;
        vine(b, q, s2, &blooms);
    }
    // V4: up the jar's own glass, round its shoulder to the cork: the jar swallowed
    for (int k = 0; k < 2; k++) {
        std::vector<PP> p;
        const float a0 = cu + (k ? -0.2f : 0.25f);
        const vec3 d0(std::sin(a0), 0, std::cos(a0));
        p.push_back({d0 * 1.0f + V3(0, 0.3f, 0), d0});
        for (int i = 1; i <= 18; i++) {
            const float t = i / 18.f;
            const float a = a0 + (k ? -1.f : 1.f) * t * 0.9f + 0.08f * std::sin(t * 9.f);
            const vec3 d(std::sin(a), 0, std::cos(a));
            const float y = 0.3f + t * 0.72f;
            // the jar's outer radius at y (its body, shoulder, neck)
            const float rr = y < 0.44f ? 1.0f : (y < 0.8f ? 0.64f + std::sqrt(std::max(0.f, 0.1296f - (y - 0.44f) * (y - 0.44f))) : 0.63f);
            const vec3 nn = y < 0.44f ? d : normalize(d * (y < 0.8f ? 1.f : 1.f) + up * (y < 0.8f ? (y - 0.44f) / 0.36f * 1.2f : 0.f));
            p.push_back({d * (rr + 0.012f) + V3(0, std::min(y, 1.0f), 0), nn});
        }
        p.push_back({V3(std::sin(a0 + (k ? -0.9f : 0.9f)) * 0.4f, 1.18f, std::cos(a0 + (k ? -0.9f : 0.9f)) * 0.4f), up});
        VineStyle s = vs; s.line0 = 20.f + k * 1.5f; s.speed = 0.5f; s.seed = 4 + k; s.leaf = 0.07f; s.r0 = 0.018f; s.flower = 0.12f;
        vine(b, p, s, &blooms);
    }
    clump(b, V3(0.05f, 1.18f, 0.f), up, 0.3f, 16, 0.1f, 24.f, 1.2f, 91, &blooms, 0.2f);
    // leaf clumps at the crack and on the bench's edge
    clump(b, E + cdir * 0.08f - V3(0, 0.05f, 0), cdir, 0.16f, 14, 0.08f, 19.f, 1.5f, 11, &blooms, 0.12f);
    clump(b, V3(-1.75f, 0.0f, 1.65f), normalize(up + fz), 0.3f, 18, 0.1f, 22.f, 1.5f, 12, &blooms, 0.1f);
    for (int k = 0; k < 6; k++) {
        const float a = cu + (k - 2.5f) * 0.4f;
        mossPad(b, V3(std::sin(a), 0, std::cos(a)) * (1.25f + 0.5f * h1(k * 3.f)), 0.1f + 0.08f * h1(k * 7.f), 19.f + k * 0.6f);
    }
    if (benchEnd) *benchEnd = out.size();
    // ---------------- the room (from 26.5): the floor, the shelves, the windows, the ceiling, out of the street window
    VineStyle rv = vs;
    rv.r0 = 0.07f; rv.leaf = 0.28f; rv.gap = 0.85f; rv.flower = 0.06f; rv.lift = 0.5f; rv.sway = 0.04f;
    // over the floor from V1's end: to the steel shelving (right), to the second bench (left), toward the street wall
    const vec3 F0 = V3(-3.4f, FLOOR, 3.5f);
    {
        std::vector<PP> p{{F0, up}};
        walk(p, V3(8.f, FLOOR, 4.f), up, 1.2f, 1.3f, 0.f, 20);
        walk(p, V3(15.f, FLOOR, -10.f), up, 1.f, 1.2f, 1.f, 20);
        walk(p, V3(15.f, FLOOR, -16.2f), up, 0.3f, 1.f, 0.f, 8);
        // up the shelving's front upright, along its shelves
        walk(p, V3(14.25f, -4.4f, -16.25f), fz, 0.1f, 1.f, 0.f, 6);
        walk(p, V3(14.25f, 3.6f, -16.3f), fz, 0.2f, 1.6f, 0.f, 14);
        walk(p, V3(26.f, 3.62f, -16.4f), up, 0.5f, 2.f, 0.4f, 20);
        walk(p, V3(26.75f, 8.1f, -16.3f), fz, 0.15f, 1.f, 0.f, 8);
        walk(p, V3(15.f, 7.62f, -16.4f), up, 0.4f, 2.f, 1.4f, 18);
        VineStyle s = rv; s.line0 = 26.6f; s.speed = 6.5f; s.seed = 41;
        vine(b, p, s, &blooms);
        std::vector<PP> q{{V3(14.25f, -0.5f, -16.3f), fz}};
        walk(q, V3(26.f, -0.38f, -16.4f), up, 0.5f, 2.2f, 2.f, 18);
        walk(q, V3(26.7f, -6.f, -16.2f), fz, 0.2f, 1.f, 0.f, 8);
        VineStyle s2 = rv; s2.line0 = 28.2f; s2.speed = 5.f; s2.seed = 42;
        vine(b, q, s2, &blooms);
        // hanging off the shelves
        for (int k = 0; k < 6; k++) {
            const float x = 15.f + k * 2.f + h1(k * 5.f);
            const float y0 = k % 2 ? 7.5f : 3.5f;
            std::vector<PP> h{{V3(x, y0 - 0.1f, -16.35f), fz}};
            walk(h, V3(x + 0.3f, y0 - 2.5f - 2.f * h1(k * 3.f), -16.f), fz, 0.15f, 1.f, k * 1.f, 10);
            VineStyle s3 = rv; s3.line0 = 30.f + k * 0.4f; s3.speed = 3.f; s3.seed = 50 + k; s3.r0 = 0.04f; s3.leaf = 0.22f;
            vine(b, h, s3, &blooms);
        }
        clump(b, V3(26.5f, 7.6f, -17.5f), up, 1.6f, 40, 0.4f, 30.f, 2.f, 43, &blooms);
        clump(b, V3(15.f, 3.6f, -17.f), up, 1.2f, 30, 0.35f, 30.5f, 2.f, 44, &blooms);
    }
    {
        std::vector<PP> p{{F0, up}};
        walk(p, V3(-10.f, FLOOR, 1.f), up, 1.f, 1.f, 0.4f, 16);
        walk(p, V3(-20.f, FLOOR, -7.4f), up, 1.f, 1.2f, 1.f, 16);
        walk(p, V3(-21.f, -0.6f, -7.75f), fz, 0.4f, 1.2f, 0.f, 12);
        p.push_back({V3(-21.2f, -0.1f, -8.2f), normalize(up + fz)});
        walk(p, V3(-21.5f, 0.f, -9.f), up, 0.1f, 1.f, 0.f, 4);
        walk(p, V3(-19.f, 0.2f, -12.f), up, 0.4f, 1.f, 0.f, 10);
        walk(p, V3(-18.5f, 4.3f, -12.4f), fz, 0.4f, 1.2f, 0.f, 12);
        walk(p, V3(-24.f, 1.5f, -11.5f), up, 0.5f, 1.f, 0.f, 14);
        VineStyle s = rv; s.line0 = 27.f; s.speed = 6.f; s.seed = 45;
        vine(b, p, s, &blooms);
        clump(b, V3(-19.f, 4.3f, -12.4f), up, 1.3f, 36, 0.35f, 31.f, 2.f, 46, &blooms);
        clump(b, V3(-14.2f, 0.f, -8.6f), normalize(up + fz + fx), 1.f, 26, 0.32f, 30.f, 2.f, 47, &blooms);
        clump(b, V3(-25.f, 1.5f, -11.5f), up, 1.0f, 22, 0.3f, 31.5f, 2.f, 48, &blooms);
    }
    // the reagent shelf and the bench swallowed: clumps along the boards, vines hanging off them, bushes on the bench
    for (int k = 0; k < 10; k++) {
        const float hk = h1(k * 3.7f + 0.2f);
        for (int sh = 0; sh < 2; sh++) {
            const float y = sh ? 6.05f : 3.25f;
            if (h1(k * 5.1f + sh) < 0.25f) continue;
            const float x = -10.5f + k * 2.3f + (hk - 0.5f) * 1.2f;
            if (std::fabs(x) < 1.2f && sh == 0) continue;
            clump(b, V3(x, y, -2.5f), normalize(up + fz * 0.6f), 0.9f, 16, 0.26f, 27.f + hk * 3.f + sh * 0.8f, 1.5f, 1000 + k * 2 + sh, &blooms, 0.12f);
            std::vector<PP> h{{V3(x + 0.3f, y - 0.05f, -2.2f), fz}};
            walk(h, V3(x + 0.6f + (hk - 0.5f), y - 1.5f - 2.f * hk, -2.05f), fz, 0.15f, 1.f, hk * 4.f, 10);
            VineStyle v = rv; v.line0 = 28.f + hk * 3.f; v.speed = 2.5f; v.seed = 1100 + k * 2 + sh; v.r0 = 0.03f; v.leaf = 0.2f;
            vine(b, h, v, &blooms);
        }
    }
    for (int k = 0; k < 8; k++) {
        const float hk = h1(k * 2.3f + 0.9f);
        const float x = (k < 4 ? -11.f + k * 1.9f : 5.f + (k - 4) * 1.9f) + (hk - 0.5f);
        clump(b, V3(x, 0.f, -0.6f + (hk - 0.5f) * 2.f), up, 1.1f + 0.4f * hk, 22, 0.3f, 26.8f + hk * 2.5f, 1.8f, 1200 + k, &blooms, 0.1f);
    }
    // the back windows swallowed: vines up the wall, round the frames, across the panes; clumps at the corners
    for (int s = -1; s <= 1; s += 2) {
        const float xa = std::min(s * BWIN_X0, s * BWIN_X1), xb = std::max(s * BWIN_X0, s * BWIN_X1);
        const float z = RZ0 + 0.15f;
        for (int k = 0; k < 4; k++) {
            const float x = k < 2 ? xa + 0.6f + k * 0.8f : xb - 0.6f - (k - 2) * 0.8f;
            std::vector<PP> p{{V3(x + s * 2.f, FLOOR, z + 4.f), up}};
            walk(p, V3(x, FLOOR, z + 0.1f), up, 0.4f, 1.f, k * 1.f, 6);
            walk(p, V3(x, WIN_Y0 + 0.4f, z), fz, 0.2f, 1.f, 0.f, 8);
            walk(p, V3(x + (k < 2 ? 0.4f : -0.4f), WIN_Y1 - 0.4f, z - 0.4f), fz, 0.5f, 2.f, k * 0.7f, 22);
            walk(p, V3((xa + xb) * 0.5f + (k - 1.5f) * 1.2f, WIN_Y1 - 0.2f, z - 0.4f), V3(0, -1, 0.3f), 0.4f, 2.f, k * 1.3f, 16);
            VineStyle v = rv; v.line0 = 28.5f + k * 0.4f + (s > 0 ? 0.6f : 0.f); v.speed = 5.f; v.seed = 60 + k + (s > 0 ? 10 : 0);
            v.leaf = 0.35f;
            vine(b, p, v, &blooms);
        }
        // tendrils across the panes from the head
        for (int k = 0; k < 5; k++) {
            const float x = xa + 1.f + (xb - xa - 2.f) * (k + 0.5f) / 5.f;
            std::vector<PP> p{{V3(x, WIN_Y1 - 0.3f, z - 0.5f), fz}};
            walk(p, V3(x + (h1(k * 3.f) - 0.5f) * 2.f, WIN_Y1 - 4.f - 4.f * h1(k * 7.f + s), z - 0.45f), fz, 0.3f, 1.2f, k * 1.f, 12);
            VineStyle v = rv; v.line0 = 31.f + k * 0.3f; v.speed = 3.f; v.seed = 80 + k + (s > 0 ? 10 : 0); v.r0 = 0.04f; v.leaf = 0.3f;
            vine(b, p, v, &blooms);
        }
        for (int c = 0; c < 4; c++) {
            const float x = c & 1 ? xb : xa, y = c & 2 ? WIN_Y1 : WIN_Y0;
            clump(b, V3(x, y, z), normalize(fz + V3(0, c & 2 ? -0.3f : 0.6f, 0)), 1.6f, 34, 0.45f, 30.f + c * 0.6f, 2.f, 100 + c + (s > 0 ? 10 : 0), &blooms);
        }
        clump(b, V3((xa + xb) * 0.5f, WIN_Y1 - 0.3f, z), normalize(fz - up * 0.3f), 2.2f, 40, 0.45f, 31.f, 2.5f, 120 + (s > 0 ? 1 : 0), &blooms);
    }
    // up the back wall to the ceiling, along it round the lights, hanging down from them
    {
        for (int k = 0; k < 3; k++) {
            const float x = k == 0 ? -3.f : (k == 1 ? 2.5f : -26.f);
            std::vector<PP> p{{V3(x, 7.2f, -3.3f), fz}};
            if (k == 2) p = {{V3(-26.f, FLOOR, -18.f), up}};
            walk(p, V3(x * 1.1f, CEIL - 0.1f, RZ0 + 0.1f), fz, 0.8f, 1.3f, k * 1.f, 20);
            walk(p, V3(x * 0.7f + 4.f, CEIL - 0.08f, -10.f), V3(0, -1, 0), 0.8f, 1.6f, k * 0.5f, 20);
            walk(p, V3(x * 0.5f - 3.f, CEIL - 0.08f, 6.f), V3(0, -1, 0), 1.f, 2.f, k * 2.f, 20);
            walk(p, V3(x * 0.3f + 5.f, CEIL - 0.08f, 22.f), V3(0, -1, 0), 1.f, 2.f, k * 2.f, 20);
            VineStyle v = rv; v.line0 = 30.f + k * 0.6f; v.speed = 7.f; v.seed = 130 + k;
            vine(b, p, v, &blooms);
        }
        int k = 0;
        for (float z : {-10.f, 6.f, 22.f})
            for (float x : {-16.f, 0.f, 16.f}) {
                for (int j = 0; j < 3; j++) {
                    const float hx = x - 4.f + j * 4.f + h1(k * 3.f + j) * 1.f, hz = z + (j - 1) * 1.f;
                    std::vector<PP> h{{V3(hx, CEIL - 0.65f, hz + 1.25f), fz}};
                    walk(h, V3(hx + 0.4f, CEIL - 3.f - 5.f * h1(k * 5.f + j), hz + 1.6f), fz, 0.3f, 1.f, j * 1.f, 14);
                    VineStyle v = rv; v.line0 = 32.f + h1(k * 7.f + j) * 2.5f; v.speed = 3.5f; v.seed = 150 + k * 3 + j; v.r0 = 0.045f; v.leaf = 0.28f;
                    vine(b, h, v, &blooms);
                }
                clump(b, V3(x, CEIL - 0.65f, z), V3(0, -1, 0.2f), 2.f, 26, 0.4f, 32.f + h1(k * 1.3f), 2.f, 160 + k, &blooms);
                k++;
            }
    }
    // toward the street window and out over its sill (where the city's torrent starts)
    {
        for (int k = 0; k < 4; k++) {
            const float x = -18.f + k * 11.f + h1(k * 3.f) * 3.f;
            std::vector<PP> p{{F0 + V3(k * 2.f, 0, 0), up}};
            walk(p, V3(x, FLOOR, RZ1 - 0.2f), up, 1.f, 1.5f, k * 1.f, 20);
            walk(p, V3(x + 0.5f, WIN_Y0 - 0.2f, RZ1 - 0.1f), V3(0, 0, -1), 0.3f, 1.f, 0.f, 8);
            walk(p, V3(x + 0.8f, WIN_Y0 + 0.1f, FRONT + 0.6f), up, 0.2f, 1.f, 0.f, 6);
            VineStyle v = rv; v.line0 = 33.f + k * 0.5f; v.speed = 7.f; v.seed = 170 + k; v.r0 = 0.12f;
            vine(b, p, v, &blooms);
        }
        clump(b, V3(-10.f, FLOOR, 30.f), up, 2.5f, 40, 0.45f, 33.f, 2.f, 180, &blooms);
        clump(b, V3(10.f, FLOOR, 30.f), up, 2.5f, 40, 0.45f, 33.5f, 2.f, 181, &blooms);
        clump(b, V3(-12.f, -0.1f, -3.4f), normalize(up + fz), 1.4f, 30, 0.35f, 29.f, 2.f, 182, &blooms);
        clump(b, V3(12.f, -0.1f, 1.f), normalize(up + fz + fx), 1.4f, 30, 0.35f, 29.5f, 2.f, 183, &blooms);
    }
    // ---------------- the city (from 38): the torrent down our facade, trees, the neighbours, the neon smothered
    VineStyle cv = vs;
    cv.r0 = 0.5f; cv.leaf = 2.2f; cv.gap = 0.75f; cv.flower = 0.05f; cv.lift = 0.45f; cv.sway = 0.03f;
    // the torrent: thick vines from the sill down the facade to the street, spreading over the sidewalk
    for (int k = 0; k < 12; k++) {
        const float hk = h1(k * 4.1f + 0.3f);
        const float x = -22.f + k * 4.f + (hk - 0.5f) * 2.f;
        std::vector<PP> p{{V3(x, WIN_Y0 + 0.2f, FRONT + 0.8f), up}};
        walk(p, V3(x + (hk - 0.5f) * 6.f, WIN_Y0 - 1.f, FRONT + 1.4f), fz, 0.f, 1.f, 0.f, 3);
        walk(p, V3(x + (hk - 0.5f) * 20.f, GROUND + 3.f, FRONT + 0.6f), fz, 3.f, 1.f + hk, hk * 6.f, 30);
        walk(p, V3(x * 1.5f + (hk - 0.5f) * 30.f, GROUND + 0.95f, FRONT + 8.f + 6.f * hk), up, 2.f, 1.f, hk * 3.f, 10);
        VineStyle v = cv; v.line0 = 38.5f + hk * 1.5f; v.speed = 14.f; v.seed = 300 + k; v.r0 = 0.35f + 0.3f * hk;
        vine(b, p, v, &blooms);
    }
    // up from the window to the roof, across the facade
    for (int k = 0; k < 8; k++) {
        const float hk = h1(k * 5.3f + 1.1f);
        const float x = (k < 4 ? -FWIN_X : FWIN_X) + (k % 4) * (k < 4 ? -8.f : 8.f) + (hk - 0.5f) * 4.f;
        std::vector<PP> p{{V3(x * 0.8f, WIN_Y1, FRONT + 0.4f), fz}};
        walk(p, V3(x + (hk - 0.5f) * 20.f, ROOF + 1.f, FRONT + 0.5f), fz, 4.f, 1.5f, hk * 5.f, 30);
        p.push_back({V3(x + (hk - 0.5f) * 20.f, ROOF + 2.4f, FRONT - 0.8f), up});
        walk(p, V3(x * 0.6f, ROOF + 0.1f, FRONT - 20.f - 20.f * hk), up, 3.f, 1.f, hk, 10);
        VineStyle v = cv; v.line0 = 40.f + hk * 2.f; v.speed = 12.f; v.seed = 320 + k; v.r0 = 0.3f;
        vine(b, p, v, &blooms);
    }
    // across the lower floors, sideways
    for (int k = 0; k < 6; k++) {
        const float hk = h1(k * 2.9f + 0.7f);
        const float y = GROUND + 8.f + k * 9.f;
        std::vector<PP> p{{V3(-30.f + hk * 60.f, y, FRONT + 0.5f), fz}};
        walk(p, V3((k % 2 ? 1.f : -1.f) * (BX + 40.f + hk * 60.f), y + (hk - 0.5f) * 20.f, FRONT + 0.5f), fz, 4.f, 2.f, hk * 7.f, 40);
        VineStyle v = cv; v.line0 = 41.f + hk * 2.f; v.speed = 16.f; v.seed = 340 + k; v.r0 = 0.25f;
        vine(b, p, v, &blooms);
    }
    // clumps over the window's edges and over the facade's ledges
    for (int k = 0; k < 9; k++) {
        const float x = -FWIN_X + k * (2.f * FWIN_X / 8.f);
        clump(b, V3(x, WIN_Y0 - 0.5f, FRONT + 1.2f), normalize(fz + up * 0.4f), 4.f, 24, 2.f, 38.5f + h1(k * 1.7f) * 1.5f, 1.5f, 400 + k, &blooms);
        clump(b, V3(x + 2.f, WIN_Y1 + 0.5f, FRONT + 0.4f), normalize(fz - up * 0.3f), 3.f, 14, 1.8f, 39.5f + h1(k * 2.3f) * 2.f, 1.5f, 410 + k, &blooms);
    }
    for (int k = 0; k < 16; k++) {
        const float hk = h1(k * 7.7f);
        const float y = GROUND + STOREY * (1 + k % 4) + 3.6f;
        const float x = -BX + 6.f + hk * (2.f * BX - 12.f);
        clump(b, V3(x, y, FRONT + 1.4f), normalize(fz + up), 6.f + 4.f * hk, 30, 2.4f, 41.f + hk * 4.f, 2.f, 420 + k, &blooms);
    }
    // our roof: two trees, shrubs
    tree(b, V3(-30.f, ROOF, 10.f), 55.f, 42.f, 5.f, 501, 1, &blooms);
    tree(b, V3(35.f, ROOF, -2.f), 42.f, 43.f, 5.f, 502, 1, &blooms);
    for (int k = 0; k < 8; k++) {
        const float hk = h1(k * 3.3f);
        clump(b, V3(-50.f + k * 13.f, ROOF + 2.2f, FRONT - 1.f), normalize(up + fz * 0.5f), 7.f, 26, 2.6f, 42.f + hk * 3.f, 2.f, 520 + k, &blooms);
    }
    // street trees bursting from the sidewalk and the road
    {
        const float tx[] = {-95.f, -40.f, 48.f, 105.f, -170.f, 180.f, -260.f, 255.f, -350.f, 340.f};
        for (int k = 0; k < 10; k++) {
            const vec3 base(tx[k], GROUND + 0.9f, FRONT + (k % 3 == 2 ? 30.f : 9.f));
            tree(b, base, 70.f + 35.f * h1(k * 9.f), 40.f + k * 0.5f, 6.f, 600 + k, 1, &blooms);
            clump(b, base, up, 9.f, 30, 3.f, 40.f + k * 0.5f, 3.f, 620 + k, &blooms);
        }
    }
    // the neon smothered: clumps over parts of the signs (where buildCity puts them: ours and the first neighbours)
    {
        clump(b, V3(-30.f, GROUND + 18.f, FRONT + 1.6f), fz, 6.f, 26, 2.2f, 43.f, 2.f, 950, &blooms);
        clump(b, V3(40.f, GROUND + 22.f, FRONT + 1.6f), fz, 5.f, 20, 2.f, 44.f, 2.f, 951, &blooms);
        clump(b, V3(10.f, GROUND + 13.f, FRONT + 1.6f), normalize(fz + up), 4.f, 16, 2.f, 45.f, 2.f, 952, &blooms);
    }
}

} // namespace terra
