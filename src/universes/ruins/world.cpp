// RUINS' world as meshes (see world.hpp): the builders (boxes with chamfered edges, prisms, lofts, lathes, fluted
// drums, trunks with buttress roots, tubes, the ground), the scene of each variant, and the restoration's
// animation.
#include "universes/ruins/world.hpp"

#include <algorithm>
#include <cmath>

namespace ruins {

namespace {

float fr(float v) { return v - std::floor(v); }
float sstep(float a, float b, float x) {
    const float t = saturate((x - a) / (b - a));
    return t * t * (3.f - 2.f * t);
}
vec3 V3(float x, float y, float z) { return vec3(x, y, z); }
const vec3 TC(0.f, 0.f, -52.f); // the temple

// ---------------------------------------------------------------- builders
struct B {
    std::vector<Vtx>* v;
    vec3 o{0, 0, 0};
    float mat = 1.f, ao = 1.f, flag = 0.f, seed = 0.f;
    void vert(const vec3& p, const vec3& n) {
        const vec3 q = p + o;
        v->push_back({{q.x, q.y, q.z}, {n.x, n.y, n.z}, {mat, ao, flag, seed}});
    }
    void tri(const vec3& a, const vec3& b, const vec3& c, const vec3& n) { vert(a, n); vert(b, n); vert(c, n); }
    void quad(const vec3& a, const vec3& b, const vec3& c, const vec3& d, const vec3& n) { tri(a, b, c, n); tri(a, c, d, n); }
    // a quad whose normal points away from `in`
    void quadOut(const vec3& a, const vec3& b, const vec3& c, const vec3& d, const vec3& in) {
        vec3 n = normalize(cross(b - a, d - a));
        if (dot(n, (a + c) * 0.5f - in) < 0.f) n = n * -1.f;
        quad(a, b, c, d, n);
    }
    void box(const vec3& c, const vec3& h, bool bottom = false) {
        const vec3 X(h.x, 0, 0), Y(0, h.y, 0), Z(0, 0, h.z);
        quad(c + X - Y - Z, c + X + Y - Z, c + X + Y + Z, c + X - Y + Z, vec3(1, 0, 0));
        quad(c - X - Y - Z, c - X + Y - Z, c - X + Y + Z, c - X - Y + Z, vec3(-1, 0, 0));
        quad(c - X + Y - Z, c + X + Y - Z, c + X + Y + Z, c - X + Y + Z, vec3(0, 1, 0));
        if (bottom) quad(c - X - Y - Z, c + X - Y - Z, c + X - Y + Z, c - X - Y + Z, vec3(0, -1, 0));
        quad(c - X - Y + Z, c + X - Y + Z, c + X + Y + Z, c - X + Y + Z, vec3(0, 0, 1));
        quad(c - X - Y - Z, c + X - Y - Z, c + X + Y - Z, c - X + Y - Z, vec3(0, 0, -1));
    }
    // a box with chamfered edges and corners (b: the chamfer)
    void bevelBox(const vec3& c, const vec3& h, float b) {
        const float H[3] = {h.x, h.y, h.z};
        auto P = [&](int ax, float s, const float cc[3]) {
            float p[3];
            for (int k = 0; k < 3; k++) p[k] = k == ax ? s * H[k] : cc[k] * (H[k] - b);
            return c + vec3(p[0], p[1], p[2]);
        };
        auto E = [](int ax, float s) { float e[3] = {0, 0, 0}; e[ax] = s; return vec3(e[0], e[1], e[2]); };
        for (int ax = 0; ax < 3; ax++)
            for (float s : {-1.f, 1.f}) {
                const int u = (ax + 1) % 3, w = (ax + 2) % 3;
                vec3 q[4];
                const float cs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
                for (int k = 0; k < 4; k++) {
                    float cc[3] = {0, 0, 0};
                    cc[u] = cs[k][0];
                    cc[w] = cs[k][1];
                    q[k] = P(ax, s, cc);
                }
                quad(q[0], q[1], q[2], q[3], E(ax, s));
            }
        for (int i = 0; i < 3; i++)
            for (int j = i + 1; j < 3; j++) {
                const int k = 3 - i - j;
                for (float si : {-1.f, 1.f})
                    for (float sj : {-1.f, 1.f}) {
                        float c0[3] = {0, 0, 0}, c1[3] = {0, 0, 0}, d0[3] = {0, 0, 0}, d1[3] = {0, 0, 0};
                        c0[j] = sj; c0[k] = -1; c1[j] = sj; c1[k] = 1;
                        d0[i] = si; d0[k] = -1; d1[i] = si; d1[k] = 1;
                        quad(P(i, si, c0), P(i, si, c1), P(j, sj, d1), P(j, sj, d0), normalize(E(i, si) + E(j, sj)));
                    }
            }
        for (float sx : {-1.f, 1.f})
            for (float sy : {-1.f, 1.f})
                for (float sz : {-1.f, 1.f}) {
                    const float a[3] = {0, sy, sz}, bb[3] = {sx, 0, sz}, cc[3] = {sx, sy, 0};
                    tri(P(0, sx, a), P(1, sy, bb), P(2, sz, cc), normalize(vec3(sx, sy, sz)));
                }
    }
    // a loft between two convex polygons (xz, same count) at y0 and y1, its caps
    void loft(const std::vector<vec2>& p0, float y0, const std::vector<vec2>& p1, float y1, bool top = true, bool bottom = false) {
        const int n = (int)p0.size();
        vec2 c0{0, 0};
        for (auto& p : p0) { c0.x += p.x / n; c0.y += p.y / n; }
        const vec3 in(c0.x, (y0 + y1) * 0.5f, c0.y);
        for (int i = 0; i < n; i++) {
            const int j = (i + 1) % n;
            quadOut(V3(p0[i].x, y0, p0[i].y), V3(p0[j].x, y0, p0[j].y), V3(p1[j].x, y1, p1[j].y), V3(p1[i].x, y1, p1[i].y), in);
        }
        auto cap = [&](const std::vector<vec2>& p, float y, float s) {
            for (int i = 1; i + 1 < n; i++) tri(V3(p[0].x, y, p[0].y), V3(p[i].x, y, p[i].y), V3(p[i + 1].x, y, p[i + 1].y), vec3(0, s, 0));
        };
        if (top) cap(p1, y1, 1.f);
        if (bottom) cap(p0, y0, -1.f);
    }
    void prism(const std::vector<vec2>& p, float y0, float y1, bool top = true) { loft(p, y0, p, y1, top); }
    // a convex profile (z, y) extruded along x from x0 to x1
    void extrudeX(const std::vector<vec2>& p, float x0, float x1) {
        const int n = (int)p.size();
        vec2 c{0, 0};
        for (auto& q : p) { c.x += q.x / n; c.y += q.y / n; }
        const vec3 in((x0 + x1) * 0.5f, c.y, c.x);
        for (int i = 0; i < n; i++) {
            const int j = (i + 1) % n;
            quadOut(V3(x0, p[i].y, p[i].x), V3(x0, p[j].y, p[j].x), V3(x1, p[j].y, p[j].x), V3(x1, p[i].y, p[i].x), in);
        }
        for (int i = 1; i + 1 < n; i++) {
            tri(V3(x0, p[0].y, p[0].x), V3(x0, p[i].y, p[i].x), V3(x0, p[i + 1].y, p[i + 1].x), vec3(-1, 0, 0));
            tri(V3(x1, p[0].y, p[0].x), V3(x1, p[i].y, p[i].x), V3(x1, p[i + 1].y, p[i + 1].x), vec3(1, 0, 0));
        }
    }
    // a lathe: a profile (r, y) from the bottom up, around (0, 0, 0)
    void lathe(const std::vector<vec2>& prof, int segs) {
        for (size_t i = 0; i + 1 < prof.size(); i++) {
            const vec2 a = prof[i], b = prof[i + 1];
            float nr = b.y - a.y, ny = -(b.x - a.x);
            const float l = std::sqrt(nr * nr + ny * ny);
            if (l < 1e-6f) continue;
            nr /= l; ny /= l;
            for (int s = 0; s < segs; s++) {
                const float t0 = TAU * s / segs, t1 = TAU * (s + 1) / segs;
                const vec3 d0(std::cos(t0), 0, std::sin(t0)), d1(std::cos(t1), 0, std::sin(t1));
                const vec3 n0 = d0 * nr + vec3(0, ny, 0), n1 = d1 * nr + vec3(0, ny, 0);
                const vec3 A = d0 * a.x + vec3(0, a.y, 0), Bv = d1 * a.x + vec3(0, a.y, 0);
                const vec3 C = d1 * b.x + vec3(0, b.y, 0), D = d0 * b.x + vec3(0, b.y, 0);
                vert(A, n0); vert(Bv, n1); vert(C, n1);
                vert(A, n0); vert(C, n1); vert(D, n0);
            }
        }
    }
};

std::vector<vec2> octagon(float s, float c) {
    const float k = c - s;
    return {{s, -k}, {s, k}, {k, s}, {-k, s}, {-s, k}, {-s, -k}, {-k, -s}, {k, -s}};
}
std::vector<vec2> square(float hx, float hz) { return {{hx, -hz}, {hx, hz}, {-hx, hz}, {-hx, -hz}}; }
std::vector<vec2> scaled(const std::vector<vec2>& p, float k) {
    std::vector<vec2> r = p;
    for (auto& q : r) { q.x *= k; q.y *= k; }
    return r;
}

// the fluted Doric drum: radius 1, y -0.5 .. 0.5, twenty flutes with sharp arrises, its edges chamfered
void flutedDrum(B& b) {
    const int F = 20, S = 6;
    auto R = [](float a) { return 1.f - 0.081f * std::sqrt(std::fabs(std::sin(a * 10.f))); };
    auto P = [&](float a) { return vec2{R(a) * std::cos(a), R(a) * std::sin(a)}; };
    const float ch = 0.035f, k = 0.965f;
    for (int f = 0; f < F; f++) {
        std::vector<vec2> pts, nrm;
        for (int j = 0; j <= S; j++) {
            const float a = (f + (float)j / S) * TAU / F;
            pts.push_back(P(a));
            const float e = 0.004f;
            const float a0 = j == 0 ? a + e * 0.1f : a - e, a1 = j == S ? a - e * 0.1f : a + e;
            const vec2 t{P(a1).x - P(a0).x, P(a1).y - P(a0).y};
            const float l = std::sqrt(t.x * t.x + t.y * t.y);
            nrm.push_back({t.y / l, -t.x / l});
        }
        for (int j = 0; j < S; j++) {
            const vec2 p0 = pts[j], p1 = pts[j + 1], n0 = nrm[j], n1 = nrm[j + 1];
            const vec3 N0(n0.x, 0, n0.y), N1(n1.x, 0, n1.y);
            auto at = [](const vec2& p, float s, float y) { return vec3(p.x * s, y, p.y * s); };
            const float yl = -0.5f + ch, yh = 0.5f - ch;
            b.vert(at(p0, 1, yl), N0); b.vert(at(p1, 1, yl), N1); b.vert(at(p1, 1, yh), N1);
            b.vert(at(p0, 1, yl), N0); b.vert(at(p1, 1, yh), N1); b.vert(at(p0, 1, yh), N0);
            for (float sy : {-1.f, 1.f}) {
                const vec3 M0 = normalize(N0 + vec3(0, sy, 0)), M1 = normalize(N1 + vec3(0, sy, 0));
                const float y0 = sy * (0.5f - ch), y1 = sy * 0.5f;
                b.vert(at(p0, 1, y0), M0); b.vert(at(p1, 1, y0), M1); b.vert(at(p1, k, y1), M1);
                b.vert(at(p0, 1, y0), M0); b.vert(at(p1, k, y1), M1); b.vert(at(p0, k, y1), M0);
                b.tri(vec3(0, y1, 0), at(p0, k, y1), at(p1, k, y1), vec3(0, sy, 0));
            }
        }
    }
}

// a trunk with its buttress roots (GLSL's trunk(): x, z, radius, seed), from below the ground up to `top`
void trunkMesh(B& b, float tx, float tz, float r, float seed, float buttK, float top) {
    const int A = 56;
    std::vector<float> ys;
    for (float y = -0.8f; y < top; y += std::min(0.25f + y * 0.12f, 3.f)) ys.push_back(y);
    ys.push_back(top);
    auto P = [&](float a, float y) {
        const float flare = std::exp(-std::max(y, 0.f) * 0.55f);
        const float butt = std::pow(std::fabs(std::cos(a * 2.5f + seed * 6.f)), 6.f) * flare * r * 2.2f * buttK;
        const float rr = r * (1.f + 0.5f * flare) + butt + 0.05f * std::sin(y * 1.3f + a * 3.f);
        return vec3(tx - 0.06f * y + rr * std::cos(a), y, tz + 0.04f * y + rr * std::sin(a));
    };
    auto N = [&](float a, float y) {
        const float e = 0.01f;
        const vec3 da = P(a + e, y) - P(a - e, y), dy = P(a, y + e) - P(a, y - e);
        return normalize(cross(dy, da));
    };
    // (each point of the grid once)
    const int ny = (int)ys.size();
    std::vector<vec3> gp(ny * (A + 1)), gn(gp.size());
    for (int i = 0; i < ny; i++)
        for (int s = 0; s <= A; s++) {
            gp[i * (A + 1) + s] = P(TAU * s / A, ys[i]);
            gn[i * (A + 1) + s] = N(TAU * s / A, ys[i]);
        }
    for (int i = 0; i + 1 < ny; i++)
        for (int s = 0; s < A; s++) {
            const int a = i * (A + 1) + s, c = (i + 1) * (A + 1) + s;
            const vec3 p00 = gp[a], p10 = gp[a + 1], p11 = gp[c + 1], p01 = gp[c];
            const vec3 n00 = gn[a], n10 = gn[a + 1], n11 = gn[c + 1], n01 = gn[c];
            b.vert(p00, n00); b.vert(p10, n10); b.vert(p11, n11);
            b.vert(p00, n00); b.vert(p11, n11); b.vert(p01, n01);
        }
    // its cut top (the jungle's trees)
    const vec3 c(tx - 0.06f * top, top, tz + 0.04f * top);
    for (int s = 0; s < A; s++) b.tri(c, P(TAU * s / A, top), P(TAU * (s + 1) / A, top), vec3(0, 1, 0));
}

// a tube along a polyline (smoothed), radius per point
void tube(B& b, const std::vector<vec3>& pts, const std::vector<float>& rad, int segs) {
    const int n = (int)pts.size();
    if (n < 2) return;
    std::vector<vec3> T(n), N1(n), N2(n);
    for (int i = 0; i < n; i++) T[i] = normalize(pts[std::min(n - 1, i + 1)] - pts[std::max(0, i - 1)]);
    // (parallel transport: each ring's frame from the previous one's)
    vec3 n1 = normalize(cross(T[0], std::fabs(T[0].y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    for (int i = 0; i < n; i++) {
        n1 = normalize(n1 - T[i] * dot(n1, T[i]));
        N1[i] = n1;
        N2[i] = cross(T[i], n1);
    }
    for (int i = 0; i + 1 < n; i++)
        for (int s = 0; s < segs; s++) {
            const float a0 = TAU * s / segs, a1 = TAU * (s + 1) / segs;
            auto ring = [&](int k, float a) { return N1[k] * std::cos(a) + N2[k] * std::sin(a); };
            const vec3 d00 = ring(i, a0), d10 = ring(i, a1), d01 = ring(i + 1, a0), d11 = ring(i + 1, a1);
            b.vert(pts[i] + d00 * rad[i], d00); b.vert(pts[i] + d10 * rad[i], d10); b.vert(pts[i + 1] + d11 * rad[i + 1], d11);
            b.vert(pts[i] + d00 * rad[i], d00); b.vert(pts[i + 1] + d11 * rad[i + 1], d11); b.vert(pts[i + 1] + d01 * rad[i + 1], d01);
        }
}

float groundH(float x, float z) {
    const float e = sstep(5.6f, 7.5f, std::fabs(x));
    return (0.32f * gnoise(x * 0.16f, z * 0.16f) + 0.12f * gnoise(x * 0.6f, z * 0.6f) - 0.12f) * e;
}

const vec4 TREES[10] = {vec4(-36.0f, -2.0f, 1.6f, 0.1f), vec4(37.0f, -6.0f, 1.5f, 0.7f), vec4(-34.0f, -22.0f, 1.8f, 0.3f),
                        vec4(35.0f, -26.0f, 1.7f, 0.9f), vec4(-40.0f, 6.0f, 2.1f, 0.5f), vec4(41.0f, 2.0f, 2.0f, 0.2f),
                        vec4(-27.0f, -50.0f, 1.4f, 0.6f), vec4(29.0f, -52.0f, 1.5f, 0.4f), vec4(-46.0f, -14.0f, 2.2f, 0.8f),
                        vec4(47.0f, -16.0f, 2.0f, 0.15f)};
const vec4 JTREES[6] = {vec4(3.0f, -9.0f, 1.2f, 0.3f), vec4(-4.5f, -19.0f, 1.4f, 0.8f), vec4(11.0f, -30.0f, 1.6f, 0.5f),
                        vec4(-13.0f, -33.0f, 1.8f, 0.1f), vec4(15.0f, -13.0f, 1.1f, 0.6f), vec4(-1.0f, -27.5f, 1.3f, 0.9f)};
// the braziers (on the ground, or on a terrace), four per variant
const vec3 BRZ[12] = {vec3(-4.8f, 21.0f, -48.0f), vec3(4.8f, 21.0f, -48.0f), vec3(-5.6f, 0.0f, -24.5f), vec3(5.6f, 0.0f, -24.5f),
                      vec3(-5.0f, 4.0f, -33.4f), vec3(5.0f, 4.0f, -33.4f), vec3(-7.0f, 4.0f, -40.0f), vec3(7.0f, 4.0f, -40.0f),
                      vec3(-6.0f, 2.7f, -42.0f), vec3(6.0f, 2.7f, -42.0f), vec3(-12.5f, 0.0f, -38.5f), vec3(12.5f, 0.0f, -38.5f)};

// the rotation of a tumbled piece: about y, then x, then z, each scaled by k
Aff tumble(const vec3& r, float k) { return Aff::rotZ(r.z * k) * Aff::rotX(r.x * k) * Aff::rotY(r.y * k); }

} // namespace

float ghash(float x, float y) {
    float a = fr(x * 0.1031f), b = fr(y * 0.1031f), c = fr(x * 0.1031f);
    const float d = a * (b + 33.33f) + b * (c + 33.33f) + c * (a + 33.33f);
    a += d; b += d; c += d;
    return fr((a + b) * c);
}
float gnoise(float x, float y) {
    const float ix = std::floor(x), iy = std::floor(y), fx = x - ix, fy = y - iy;
    const float ux = fx * fx * (3.f - 2.f * fx), uy = fy * fy * (3.f - 2.f * fy);
    const float a = ghash(ix, iy), b = ghash(ix + 1, iy), c = ghash(ix, iy + 1), d = ghash(ix + 1, iy + 1);
    return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy);
}

Aff Aff::rotX(float a) { Aff r; r.c1 = vec3(0, std::cos(a), std::sin(a)); r.c2 = vec3(0, -std::sin(a), std::cos(a)); return r; }
Aff Aff::rotY(float a) { Aff r; r.c0 = vec3(std::cos(a), 0, -std::sin(a)); r.c2 = vec3(std::sin(a), 0, std::cos(a)); return r; }
Aff Aff::rotZ(float a) { Aff r; r.c0 = vec3(std::cos(a), std::sin(a), 0); r.c1 = vec3(-std::sin(a), std::cos(a), 0); return r; }

// =====================================================================================================
// The scene
// =====================================================================================================

void Scene::build(int kind) {
    kind_ = kind;
    verts_.clear();
    verts_.reserve(520000);
    pieces_.clear();
    vis_.clear();
    figK_ = -1.f;
    B b{&verts_};

    // ---------------------------------------------------------------- the kits
    auto beginKit = [&](int k) { kit_[k].first = (int)verts_.size(); };
    auto endKit = [&](int k) { kit_[k].count = (int)verts_.size() - kit_[k].first; };
    b.mat = 1.f; b.flag = 1.f;
    beginKit(K_BLOCK); b.bevelBox(vec3(0, 0, 0), vec3(1, 1, 1), 0.07f); endKit(K_BLOCK);
    b.mat = 3.f; b.flag = 2.f;
    beginKit(K_DRUM0); b.bevelBox(vec3(0, 0, 0), vec3(1, 0.5f, 0.78f), 0.05f); endKit(K_DRUM0);
    beginKit(K_DRUM1);
    {
        const auto o = octagon(1.f, 1.42f);
        b.loft(scaled(o, 0.88f), -0.5f, o, -0.36f, false, true);
        b.loft(o, -0.36f, o, 0.36f, false);
        b.loft(o, 0.36f, scaled(o, 0.88f), 0.5f, true);
    }
    endKit(K_DRUM1);
    beginKit(K_DRUM2); flutedDrum(b); endKit(K_DRUM2);
    beginKit(K_ECHINUS);
    {
        std::vector<vec2> pr;
        for (int i = 0; i <= 8; i++) {
            const float t = i / 8.f;
            pr.push_back({0.95f + 0.42f * std::sqrt(t) * (0.6f + 0.4f * t), 0.36f * t});
        }
        pr.push_back({0.f, 0.36f});
        b.lathe(pr, 40);
    }
    endKit(K_ECHINUS);
    beginKit(K_BALUSTER);
    {
        // a turned baluster, 1 high
        std::vector<vec2> pr = {{0.f, 0.f}, {0.15f, 0.f}, {0.15f, 0.08f}, {0.11f, 0.1f}, {0.1f, 0.2f}, {0.14f, 0.24f},
                                {0.1f, 0.28f}, {0.09f, 0.5f}, {0.13f, 0.53f}, {0.09f, 0.56f}, {0.1f, 0.72f}, {0.14f, 0.76f},
                                {0.1f, 0.8f}, {0.11f, 0.9f}, {0.15f, 0.92f}, {0.15f, 1.f}, {0.f, 1.f}};
        b.lathe(pr, 16);
    }
    endKit(K_BALUSTER);
    b.mat = 4.f; b.flag = 1.f;
    beginKit(K_BRAZIER);
    {
        std::vector<vec2> pr = {{0.f, 0.f}, {0.38f, 0.f}, {0.38f, 0.12f}, {0.16f, 0.14f}, {0.14f, 0.3f}, {0.14f, 0.82f}, {0.2f, 0.84f}};
        for (int i = 0; i <= 8; i++) {
            const float a = -1.45f + i / 8.f * (1.45f + 0.11f);
            pr.push_back({0.45f * std::cos(a), 1.25f + 0.45f * std::sin(a)});
        }
        pr.push_back({0.37f, 1.3f});
        for (int i = 0; i <= 6; i++) {
            const float a = 0.27f - i / 6.f * (0.27f + 1.5708f);
            pr.push_back({0.38f * std::cos(a) * (i == 6 ? 0.f : 1.f), 1.4f + 0.38f * std::sin(a)});
        }
        b.lathe(pr, 24);
    }
    endKit(K_BRAZIER);
    beginKit(K_ROOF);
    {
        b.mat = 1.f; b.flag = 0.f;
        // KHMER's gallery roof, 3 long (its profile across: a pitched roof over the eaves)
        b.extrudeX({{-1.8f, -0.1f}, {1.8f, -0.1f}, {1.8f, 0.28f}, {0.33f, 1.6f}, {-0.33f, 1.6f}, {-1.8f, 0.28f}}, -1.5f, 1.5f);
    }
    endKit(K_ROOF);

    const int firstStatic = (int)verts_.size();
    std::vector<Piece> parts; // pieces with their own ranges (built into pv, appended after the static mesh)
    std::vector<Vtx> pv;
    auto addKit = [&](int kit, const vec3& c, const vec3& s, float seed, int role = R_STATIC) {
        Piece p;
        p.kit = kit;
        p.c = c;
        p.s = s;
        p.seed = seed;
        p.role = role;
        pieces_.push_back(p);
        return (int)pieces_.size() - 1;
    };

    // ---------------------------------------------------------------- the ground
    b.mat = 0.f; b.flag = 0.f; b.o = vec3(0, 0, 0);
    {
        const float x0 = -84.f, x1 = 84.f, z0 = -176.f, z1 = 56.f, st = 1.f;
        const int nx = (int)((x1 - x0) / st), nz = (int)((z1 - z0) / st);
        // (each grid point once: its height, its normal)
        std::vector<vec3> gp((nx + 1) * (nz + 1)), gn(gp.size());
        for (int k = 0; k <= nz; k++)
            for (int i = 0; i <= nx; i++) {
                const float x = x0 + i * st, z = z0 + k * st, e = 0.3f;
                gp[k * (nx + 1) + i] = vec3(x, groundH(x, z), z);
                gn[k * (nx + 1) + i] = normalize(vec3(groundH(x - e, z) - groundH(x + e, z), 2.f * e, groundH(x, z - e) - groundH(x, z + e)));
            }
        auto P = [&](int i, int k) { return gp[k * (nx + 1) + i]; };
        auto N = [&](int i, int k) { return gn[k * (nx + 1) + i]; };
        for (int k = 0; k < nz; k++)
            for (int i = 0; i < nx; i++) {
                b.vert(P(i, k), N(i, k)); b.vert(P(i + 1, k), N(i + 1, k)); b.vert(P(i + 1, k + 1), N(i + 1, k + 1));
                b.vert(P(i, k), N(i, k)); b.vert(P(i + 1, k + 1), N(i + 1, k + 1)); b.vert(P(i, k + 1), N(i, k + 1));
            }
    }

    // ---------------------------------------------------------------- the avenue
    b.mat = 3.f; b.flag = 0.f;
    for (float sd : {-1.f, 1.f}) {
        b.box(vec3(sd * 7.5f, 0.25f, -14.f), vec3(1.9f, 0.25f, 13.5f));
        b.box(vec3(sd * 7.5f, 0.62f, -14.f), vec3(1.45f, 0.13f, 13.f));
    }
    const int style = kind;
    const float H = kind == 0 ? 5.4f : (kind == 1 ? 4.8f : 7.6f);
    const float R = kind == 0 ? 0.62f : (kind == 1 ? 0.55f : 0.68f);
    for (int j = 0; j < 4; j++)
        for (float sd : {-1.f, 1.f}) {
            const float zc = -3.5f - 7.f * j;
            const int ci = j * 2 + (sd > 0.f ? 1 : 0);
            const vec3 base(sd * 7.5f, 0.75f, zc);
            const float h0 = ghash(j * 3.1f + 0.7f, sd * 1.7f + 2.f), h1 = ghash(j * 1.3f + 5.1f, sd * 3.3f + 0.4f);
            const bool broken = h0 < 0.62f;
            const float hb0 = !broken ? H : (h0 < 0.28f ? 0.7f + h1 * 0.5f : H * (0.32f + 0.5f * h1));
            // the column's elements (q space: its foot at 0), and where it breaks
            struct El { int kit; vec3 c, s; };
            std::vector<El> els;
            float y0, y1;
            if (style == 0) { els.push_back({K_BLOCK, V3(0, 0.18f, 0), V3(1.25f * R, 0.18f, 1.05f * R)}); y0 = 0.36f; y1 = H - 0.4f; }
            else if (style == 1) { els.push_back({K_BLOCK, V3(0, 0.25f, 0), V3(1.3f * R, 0.25f, 1.3f * R)}); y0 = 0.5f; y1 = H - 0.3f; }
            else { y0 = 0.f; y1 = H - 0.62f; }
            const int n = std::max(2, (int)std::lround((y1 - y0) / 1.15f));
            const float dh = (y1 - y0) / n;
            for (int k = 0; k < n; k++) {
                const float yc = y0 + dh * (k + 0.5f), tp = 1.f - 0.07f * yc / H;
                els.push_back({K_DRUM0 + style, V3(0, yc, 0), V3(R * tp, dh, R * tp)});
            }
            if (style == 0) els.push_back({K_BLOCK, V3(0, H - 0.2f, 0), V3(1.3f * R, 0.2f, 1.1f * R)});
            else if (style == 1) els.push_back({K_BLOCK, V3(0, H - 0.15f, 0), V3(1.35f * R, 0.15f, 1.35f * R)});
            else {
                els.push_back({K_ECHINUS, V3(0, H - 0.62f, 0), V3(R, 1.f, R)});
                els.push_back({K_BLOCK, V3(0, H - 0.13f, 0), V3(1.42f * R, 0.13f, 1.42f * R)});
            }
            // the break: at the joint nearest hb0 (at least the foot stays)
            float yb = H;
            if (broken) {
                yb = style == 2 ? 0.f : y0;
                for (int k = 0; k < n; k++)
                    if (y0 + dh * (k + 1) <= hb0 + 0.3f) yb = y0 + dh * (k + 1);
            }
            for (const El& e : els) {
                const vec3 c = base + vec3(sd * e.c.x, e.c.y, e.c.z);
                const int pi = addKit(e.kit, c, e.s, h1 * 7.f + e.c.y, e.c.y > yb ? R_COLTOP : R_STATIC);
                Piece& p = pieces_[pi];
                p.mat = 3.f;
                if (p.role == R_COLTOP) {
                    p.idx = ci;
                    p.a = yb;
                    p.b = (h1 - 0.5f) * 1.6f;
                    p.from = V3(2.4f, -0.75f + R, 0.4f + h1);
                    p.rot = V3(sd, 0, 0); // (the side)
                    p.e0 = 0.05f; p.e1 = 0.6f;
                }
            }
            if (h1 > 0.45f) {
                const int pi = addKit(K_BLOCK, base + vec3(0, H + 0.42f, 0), V3(0.95f, 0.42f, 0.62f), h1 * 3.f, R_FLY);
                Piece& p = pieces_[pi];
                p.mat = 3.f;
                p.src = 1 + ci;
                p.e0 = 0.55f; p.e1 = 1.f;
                p.from = base + vec3(sd * 3.9f, -0.35f, -3.f);
                p.rot = V3(0.f, h1 * 5.f * sd, (h1 - 0.7f) * 0.5f * sd);
            }
        }

    // ---------------------------------------------------------------- the temple
    b.mat = 1.f; b.flag = 0.f; b.o = TC;
    auto blockAt = [&](const vec3& lo, const vec3& hi, float seed, int role, int group, float a = 0.f, float bb = 0.f) {
        const int pi = addKit(K_BLOCK, TC + (lo + hi) * 0.5f, (hi - lo) * 0.5f, seed, role);
        pieces_[pi].group = group;
        pieces_[pi].a = a;
        pieces_[pi].b = bb;
        return pi;
    };
    // a wall of coursed blocks between x0 and x1 (along x, or along z when alongZ), z0..z1 thick, y0..y1 high
    auto wallX = [&](float x0, float x1, float z0, float z1, float y0, float y1, float ch, float bl, int group,
                     float seed, bool alongZ, float doorHalf = 0.f, float doorTop = 0.f) {
        int row = 0;
        for (float y = y0; y < y1 - 0.01f; y += ch, row++) {
            const float yt = std::min(y + ch, y1);
            float x = x0;
            float first = bl * (0.35f + 0.5f * ghash(row * 1.7f + seed, seed * 3.1f));
            while (x < x1 - 0.01f) {
                float xe = std::min(x1, x + (x == x0 ? first : bl));
                if (x1 - xe < bl * 0.3f) xe = x1;
                if (doorHalf > 0.f && yt <= doorTop + 0.01f) {
                    // (the door: the blocks end at its jambs)
                    if (x < -doorHalf && xe > -doorHalf) xe = -doorHalf;
                    if (x >= -doorHalf - 0.01f && x < doorHalf) { x = doorHalf; continue; }
                }
                const float h = ghash(x * 3.7f + seed, y * 5.3f);
                const vec3 lo = alongZ ? V3(z0, y, x) : V3(x, y, z0), hi = alongZ ? V3(z1, yt, xe) : V3(xe, yt, z1);
                const float cx = alongZ ? (z0 + z1) * 0.5f : (x + xe) * 0.5f, cz = alongZ ? (x + xe) * 0.5f : (z0 + z1) * 0.5f;
                blockAt(lo, hi, h * 9.f, group == G_NONE ? R_STATIC : R_SHOW, group, cx, cz);
                x = xe;
            }
        }
    };

    if (kind == 0) {
        // ---- MAYA: seven terraces (a talud, a vertical band under a cornice), the grand staircase between its
        // ramps, the shrine with its roof comb (in blocks: its top ruined), the collapsed corner
        for (int k = 0; k < 7; k++) {
            const float s = 22.f - k * 2.6f, y0 = 3.f * k;
            b.loft(square(s, s), y0, square(s - 0.7f, s - 0.7f), y0 + 2.1f, false);
            b.prism(square(s - 0.7f, s - 0.7f), y0 + 2.1f, y0 + 2.82f, false);
            b.loft(square(s - 0.62f, s - 0.62f), y0 + 2.82f, square(s - 0.58f, s - 0.58f), y0 + 3.f, true);
        }
        const float tr = 0.58f, ri = 0.6f;
        for (int k = 0; k < 35; k++) {
            const float zf = 26.f - k * tr, zb = 26.f - (k + 1) * tr, yt = (k + 1) * ri;
            b.quad(V3(-3.6f, yt, zf), V3(3.6f, yt, zf), V3(3.6f, yt, zb), V3(-3.6f, yt, zb), vec3(0, 1, 0));
            b.quad(V3(-3.6f, yt - ri, zf), V3(3.6f, yt - ri, zf), V3(3.6f, yt - 0.05f, zf), V3(-3.6f, yt - 0.05f, zf), vec3(0, 0, 1));
            b.quad(V3(-3.6f, yt - 0.05f, zf), V3(3.6f, yt - 0.05f, zf), V3(3.6f, yt, zf - 0.05f), V3(-3.6f, yt, zf - 0.05f),
                   normalize(vec3(0, 1, 1)));
            for (float sx : {-1.f, 1.f})
                b.quad(V3(sx * 3.6f, 0, zf), V3(sx * 3.6f, yt, zf), V3(sx * 3.6f, yt, zb), V3(sx * 3.6f, 0, zb), vec3(sx, 0, 0));
        }
        for (float sx : {-1.f, 1.f}) {
            // the ramps (alfardas) beside the stairs
            const float u1 = (21.3f - 0.95f) / 1.0345f;
            std::vector<vec2> pr = {{26.f, 0.f}, {26.f, 0.95f}, {26.f - u1, 21.3f}, {0.f, 21.3f}, {0.f, 0.f}};
            b.extrudeX(pr, sx > 0 ? 3.6f : -4.7f, sx > 0 ? 4.7f : -3.6f);
        }
        // the shrine, in blocks (a core per course behind its walls), its door, the roof slab, the comb
        const float sch = 0.65f;
        for (int c = 0; c < 8; c++) {
            const float y = 21.f + c * sch, yt = y + sch;
            const bool door = yt <= 23.61f;
            wallX(-4.4f, 4.4f, 2.2f, 3.1f, y, yt, sch, 1.3f, G_SHRINE, 1.f + c, false, door ? 0.95f : 0.f, 23.61f);
            wallX(-4.4f, 4.4f, -4.1f, -3.2f, y, yt, sch, 1.3f, G_SHRINE, 2.f + c, false);
            wallX(-3.2f, 2.2f, 3.5f, 4.4f, y, yt, sch, 1.3f, G_SHRINE, 3.f + c, true);
            wallX(-3.2f, 2.2f, -4.4f, -3.5f, y, yt, sch, 1.3f, G_SHRINE, 4.f + c, true);
            blockAt(V3(-3.5f, y, -3.2f), V3(3.5f, yt, 2.2f), 5.f + c, R_SHOW, G_SHRINE, 0.f, -0.5f);
        }
        for (int i = 0; i < 4; i++)
            blockAt(V3(-4.75f + i * 2.375f, 26.2f, -4.45f), V3(-4.75f + (i + 1) * 2.375f, 26.7f, 3.45f), 20.f + i, R_SHOW, G_SHRINE,
                    -4.75f + (i + 0.5f) * 2.375f, -0.5f);
        {
            const float zc0 = -1.95f, zc1 = -1.25f;
            const float rows[5][2] = {{26.7f, 27.4f}, {28.3f, 29.f}, {29.9f, 30.4f}, {30.4f, 31.1f}, {0, 0}};
            for (int r = 0; r < 4; r++) {
                const float w = r == 3 ? 2.6f : 3.5f;
                blockAt(V3(-w, rows[r][0], zc0), V3(w, rows[r][1], zc1), 30.f + r, R_SHOW, G_SHRINE, 0.f, -1.6f);
            }
            const float posts[5] = {-3.25f, -1.7f, 0.f, 1.7f, 3.25f}, half[5] = {0.25f, 0.4f, 0.4f, 0.4f, 0.25f};
            for (int r = 0; r < 2; r++)
                for (int i = 0; i < 5; i++) {
                    const float y0 = r == 0 ? 27.4f : 29.f, y1 = r == 0 ? 28.3f : 29.9f;
                    blockAt(V3(posts[i] - half[i], y0, zc0 + 0.05f), V3(posts[i] + half[i], y1, zc1 - 0.05f), 40.f + r * 5 + i,
                            R_SHOW, G_SHRINE, posts[i], -1.6f);
                }
        }
        // the collapsed corner: tumbled blocks fly back up into it, rubble at its foot
        for (int i = 0; i < 3; i++) {
            const float fi = (float)i;
            const int pi = blockAt(V3(13.f + fi * 2.f - 1.f, 4.f + fi * 2.2f - 0.45f, 19.5f - fi * 0.8f - 0.7f),
                                   V3(13.f + fi * 2.f + 1.f, 4.f + fi * 2.2f + 0.45f, 19.5f - fi * 0.8f + 0.7f), 60.f + fi, R_FLY, G_NONE);
            Piece& p = pieces_[pi];
            p.src = 0;
            p.e0 = fi * 0.15f;
            p.e1 = 0.17f + fi * 0.15f;
            p.from = TC + V3(10.f + fi * 3.5f, 0.45f, 26.f - fi * 1.4f);
            p.rot = V3(0.15f - fi * 0.2f, 0.6f + fi, 0.1f * fi);
        }
        for (int i = 0; i < 9; i++) {
            const float h = ghash(i * 2.3f, 7.1f), h2 = ghash(i * 1.7f, 3.3f);
            const vec3 c = V3(9.f + 9.f * h, 0.3f + 0.15f * h2, 22.f + 6.f * h2);
            const int pi = blockAt(c - V3(0.5f, 0.3f, 0.35f), c + V3(0.5f, 0.3f, 0.35f), 70.f + i, R_GONE, G_CORNER, 0.04f + 0.045f * i);
            pieces_[pi].rot = V3(0.3f * (h - 0.5f), h2 * 6.f, 0.4f * (h2 - 0.5f));
        }
    } else if (kind == 1) {
        // ---- KHMER: the terrace and its stairs, the gallery (blocks, balusters, the roof), the gate tower, the
        // central prang and the side towers
        b.prism(square(25.f, 16.f), 0.f, 3.5f, false);
        b.o = TC + V3(0, 0, -2.f);
        b.prism(square(25.4f, 16.4f), 3.5f, 4.f, true);
        b.o = TC;
        for (int k = 0; k < 8; k++) {
            const float zf = 18.4f - k * 0.55f, zb = zf - 0.55f, yt = (k + 1) * 0.5f;
            b.quad(V3(-4.f, yt, zf), V3(4.f, yt, zf), V3(4.f, yt, zb), V3(-4.f, yt, zb), vec3(0, 1, 0));
            b.quad(V3(-4.f, yt - 0.5f, zf), V3(4.f, yt - 0.5f, zf), V3(4.f, yt, zf), V3(-4.f, yt, zf), vec3(0, 0, 1));
            for (float sx : {-1.f, 1.f})
                b.quad(V3(sx * 4.f, 0, zf), V3(sx * 4.f, yt, zf), V3(sx * 4.f, yt, zb), V3(sx * 4.f, 0, zb), vec3(sx, 0, 0));
        }
        // (the stairs' balustrade walls)
        for (float sx : {-1.f, 1.f}) b.extrudeX({{18.4f, 0.f}, {18.4f, 0.9f}, {14.f, 4.5f}, {13.6f, 4.5f}, {13.6f, 0.f}}, sx * 4.f - 0.35f, sx * 4.f + 0.35f);
        // the gallery: windows every 3 along x, the door in the middle; in blocks (its fallen stretch rebuilds)
        const float gz = 11.f, gy = 6.8f;
        auto gal = [&](float x0, float x1, float y0, float y1, float seed) {
            const float xc = (x0 + x1) * 0.5f;
            const bool fallen = xc > 4.f && xc < 20.f;
            blockAt(V3(x0, y0, gz - 1.4f), V3(x1, y1, gz + 1.4f), seed, fallen ? R_SHOW : R_STATIC, fallen ? G_GALLERY : G_NONE, xc);
        };
        for (int n = -8; n < 8; n++) {
            const float xa = 3.f * n, xb = xa + 3.f;
            // the sill course (cut by the door), the piers between the windows, two lintel courses
            if (n == -1) gal(xa, -1.7f, gy - 2.8f, gy - 1.25f, 1.f + n);
            else if (n == 0) gal(1.7f, xb, gy - 2.8f, gy - 1.25f, 1.f + n);
            else gal(xa, xb, gy - 2.8f, gy - 1.25f, 1.f + n);
            float p0 = xa + 0.85f, p1 = xb - 0.85f;
            if (n == -8) p0 = xa;
            if (n == 7) p1 = xb;
            if (n == -1) p1 = std::min(p1, -1.7f);
            if (n == 0) p0 = std::max(p0, 1.7f);
            gal(p0, p1, gy - 1.25f, gy + 0.85f, 30.f + n);
            gal(xa + (n % 2 ? 0.f : 0.7f) * 0.f, xb, gy + 0.85f, gy + 1.85f, 60.f + n);
            gal(xa, xb, gy + 1.85f, gy + 2.8f, 90.f + n);
            const float xc = xa + 1.5f;
            const bool fallen = xc > 4.f && xc < 20.f;
            const int ri = addKit(K_ROOF, TC + V3(xc, gy + 2.8f, gz), V3(1.f, 1.f, 1.f), 5.f + n, fallen ? R_SHOW : R_STATIC);
            pieces_[ri].group = fallen ? G_GALLERY : G_NONE;
            pieces_[ri].a = xc;
            pieces_[ri].mat = 1.f;
            // the balusters in the window at xb (not the door's)
            if (n < 7 && n != -1)
                for (int k = -1; k <= 1; k++) {
                    const float bx = xb + k * 0.5f;
                    const bool f2 = bx > 4.f && bx < 20.f;
                    const int bi = addKit(K_BALUSTER, TC + V3(bx, gy - 1.25f, gz), V3(1.f, 2.1f, 1.f), bx, f2 ? R_SHOW : R_STATIC);
                    pieces_[bi].group = f2 ? G_GALLERY : G_NONE;
                    pieces_[bi].a = bx;
                    pieces_[bi].c = TC + V3(bx, gy - 1.25f + 1.05f, gz);
                    pieces_[bi].mat = 1.f;
                }
        }
        // its fallen blocks lying in front of it, gone as it rebuilds
        for (int i = 0; i < 8; i++) {
            const float h = ghash(i * 3.1f, 1.3f), h2 = ghash(i * 0.7f, 9.1f);
            const vec3 c = V3(6.f + 13.f * h, 0.3f + 4.f + 0.1f * h2, 13.6f + 3.f * h2);
            const int pi = blockAt(c - V3(0.6f, 0.3f, 0.4f), c + V3(0.6f, 0.3f, 0.4f), 80.f + i, R_GONE, G_GALLERY, 0.1f + 0.1f * i);
            pieces_[pi].rot = V3(0.2f * (h2 - 0.5f), h * 5.f, 0.3f * (h - 0.5f));
        }
        // the towers: a base (an octagon, a porch with its door), six redented tiers, the lotus bud
        struct Tower { vec3 c; float sc; };
        const Tower T4[4] = {{V3(0, 9.6f, 11.f), 0.5f}, {V3(0, 4.f, -6.f), 1.35f}, {V3(-15.f, 4.f, -1.f), 0.85f}, {V3(15.f, 4.f, -1.f), 0.85f}};
        for (int t = 0; t < 4; t++) {
            const float sc = T4[t].sc;
            b.o = TC + T4[t].c;
            auto S = [&](const std::vector<vec2>& p) { return scaled(p, sc); };
            if (t > 0) {
                b.prism(S(octagon(4.5f, 7.6f)), 0.f, 6.f * sc, true);
                b.box(V3(-1.125f, 1.6f, 4.6f) * sc, V3(0.375f, 1.6f, 0.9f) * sc);
                b.box(V3(1.125f, 1.6f, 4.6f) * sc, V3(0.375f, 1.6f, 0.9f) * sc);
                b.box(V3(0, 3.8f, 4.6f) * sc, V3(1.5f, 0.6f, 0.9f) * sc);
                b.box(V3(0, 4.55f, 4.4f) * sc, V3(1.2f, 0.15f, 0.7f) * sc);
            }
            float y0 = 6.f;
            for (int i = 0; i < 6; i++) {
                const float s = 4.1f * std::pow(0.84f, (float)i), h = 2.f * std::pow(0.9f, (float)i);
                Piece p;
                b.v = &pv;
                p.r.first = (int)pv.size();
                b.loft(S(octagon(s, 1.55f * s)), y0 * sc, S(octagon(s * 0.95f, 1.55f * s * 0.95f)), (y0 + h) * sc, true);
                b.prism(S(octagon(s + 0.3f, 1.6f * s + 0.3f)), y0 * sc, (y0 + 0.32f) * sc, true);
                // its antefixes: a small flame at each corner of its top
                const auto oc = octagon(s * 0.9f, 1.55f * s * 0.9f);
                for (const vec2& q : oc) {
                    const vec3 c = V3(q.x, y0 + h, q.y) * sc;
                    const float w = 0.1f * s * sc, ht = 0.32f * s * sc;
                    const vec3 o0 = b.o;
                    b.o = o0 + vec3(c.x, 0, c.z);
                    b.loft(square(w, w), c.y, square(0.02f * sc, 0.02f * sc), c.y + ht, false);
                    b.o = o0;
                }
                p.r.count = (int)pv.size() - p.r.first;
                b.v = &verts_;
                p.role = R_SHOW;
                p.group = G_TIER;
                p.a = (float)t;
                p.b = (float)i;
                p.c = TC + T4[t].c + V3(0, (y0 + h * 0.5f) * sc, 0);
                parts.push_back(p);
                y0 += h;
            }
            {
                Piece p;
                b.v = &pv;
                p.r.first = (int)pv.size();
                std::vector<vec2> pr;
                for (int i = 0; i <= 14; i++) {
                    const float u = i / 14.f, a = -0.5f + u * 1.5f;
                    float r = 1.25f * std::cos(a * 1.0472f * 1.f);
                    r *= std::pow(std::max(0.f, 1.f - std::pow(u, 2.5f)), 0.6f);
                    pr.push_back({r * sc, (y0 - 0.6f + 3.6f * u) * sc});
                }
                pr.back().x = 0.f;
                b.mat = 1.f;
                b.lathe(pr, 24);
                p.r.count = (int)pv.size() - p.r.first;
                b.v = &verts_;
                p.role = R_SHOW;
                p.group = G_TIER;
                p.a = (float)t;
                p.b = 6.f;
                p.c = TC + T4[t].c + V3(0, (y0 + 1.f) * sc, 0);
                parts.push_back(p);
            }
        }
        b.o = TC;
    } else {
        // ---- GREEK: the stylobate's three steps, the cella (blocks, its ruined top), the colonnade (fluted drums
        // re-forming up to their capitals), the entablature and the pediment closing from the left, the pieces
        // fallen on the ground
        b.box(V3(0, 0.45f, 0), V3(15.6f, 0.45f, 11.6f));
        b.box(V3(0, 1.35f, 0), V3(14.8f, 0.45f, 10.8f));
        b.box(V3(0, 2.25f, 0), V3(14.f, 0.45f, 10.f));
        const float cch = 0.96f;
        wallX(-8.f, 8.f, 4.7f, 5.5f, 2.7f, 12.3f, cch, 1.8f, G_CELLA, 1.f, false, 1.6f, 2.7f + cch * 6.f);
        wallX(-8.f, 8.f, -7.5f, -6.7f, 2.7f, 12.3f, cch, 1.8f, G_CELLA, 2.f, false);
        wallX(-6.7f, 4.7f, 7.2f, 8.f, 2.7f, 12.3f, cch, 1.8f, G_CELLA, 3.f, true);
        wallX(-6.7f, 4.7f, -8.f, -7.2f, 2.7f, 12.3f, cch, 1.8f, G_CELLA, 4.f, true);
        blockAt(V3(-1.6f, 2.7f + cch * 6.f, 4.6f), V3(1.6f, 2.7f + cch * 7.f, 5.6f), 9.f, R_SHOW, G_CELLA, 0.f, 5.f); // the lintel
        // the columns: six in front, four down each side
        const float CH = 10.f, r = 0.8f, sh = CH - 0.62f;
        const int nd = 7;
        const float dh = sh / nd;
        struct GC { float x, z, hb; float g0, g1; int fly; };
        std::vector<GC> cols;
        for (int i = -3; i <= 2; i++) {
            const float hb = i == 2 ? 5.2f : (i == 1 ? 2.4f : CH);
            cols.push_back({4.f * i + 2.f, 8.f, hb, 0.25f, 0.5f, i == 2 ? 1 : (i == 1 ? 2 : 0)});
        }
        for (int j = 1; j <= 4; j++)
            for (float sd : {-1.f, 1.f}) {
                const float hb = sd > 0.f ? (j < 3 ? 3.6f : 1.4f + j * 1.6f) : CH;
                cols.push_back({sd * 10.f, 8.f - 4.f * j, hb, 0.25f, 0.5f, 0});
            }
        for (int ci = 0; ci < (int)cols.size(); ci++) {
            GC& g = cols[ci];
            // the ruin's height, at a joint; a column a drum flies back to grows only once it has landed
            float hbR = std::floor(g.hb / dh + 0.35f) * dh;
            if (g.hb >= CH) hbR = CH;
            if (g.fly) g.g0 = 0.34f;
            const vec3 base = TC + V3(g.x, 2.7f, g.z);
            for (int k = 0; k < nd; k++) {
                const float yc = dh * (k + 0.5f), tp = 1.f - 0.07f * yc / CH;
                const int pi = addKit(K_DRUM2, base + V3(0, yc, 0), V3(r * tp, dh, r * tp), ci * 1.3f + k, R_SHOW);
                Piece& p = pieces_[pi];
                p.mat = 3.f;
                p.group = G_GCOL;
                p.a = hbR + (g.fly ? dh : 0.f);
                p.b = dh * (k + 1);
                p.e0 = g.g0;
                p.e1 = g.g1;
                if (g.fly && std::fabs(dh * k - hbR) < 0.01f) {
                    // the drum that lay on the ground flies back to its place
                    p.role = R_FLY;
                    p.src = 0;
                    p.e0 = 0.25f;
                    p.e1 = 0.33f;
                    p.from = TC + (g.fly == 1 ? V3(13.5f, 0.8f, 11.f) : V3(6.8f, 0.8f, 13.6f));
                    p.rot = g.fly == 1 ? V3(0.f, 1.2f, 1.5708f) : V3(0.f, -0.4f, 1.5708f);
                }
            }
            for (int e = 0; e < 2; e++) {
                const int pi = e == 0 ? addKit(K_ECHINUS, base + V3(0, sh, 0), V3(r, 1.f, r), ci * 1.3f + 9.f, R_SHOW)
                                      : addKit(K_BLOCK, base + V3(0, CH - 0.13f, 0), V3(1.42f * r, 0.13f, 1.42f * r), ci * 1.3f + 11.f, R_SHOW);
                Piece& p = pieces_[pi];
                p.mat = 3.f;
                p.group = G_GCOL;
                p.a = hbR + (g.fly ? dh : 0.f);
                p.b = CH;
                p.e0 = g.g0;
                p.e1 = g.g1;
                if (e == 0) p.c = base + V3(0, sh + 0.18f, 0);
            }
        }
        // the entablature: the front (architrave and frieze, the cornice) in spans, the sides in spans; one span
        // lies on the ground and flies back
        const float xs[7] = {-11.2f, -8.f, -4.f, 0.f, 4.f, 8.f, 11.2f};
        for (int i = 0; i < 6; i++) {
            const float x0 = xs[i], x1 = xs[i + 1];
            const bool fly = i == 5;
            for (int k = 0; k < 3; k++) {
                const vec3 lo = k == 0 ? V3(x0, 12.7f, 7.f) : (k == 1 ? V3(x0, 13.75f, 7.05f) : V3(x0, 14.89f, 6.8f));
                const vec3 hi = k == 0 ? V3(x1, 13.75f, 9.f) : (k == 1 ? V3(x1, 14.89f, 8.95f) : V3(x1, 15.21f, 9.3f));
                const int pi = blockAt(lo, hi, 100.f + i * 3 + k, fly ? R_FLY : R_SHOW, fly ? G_NONE : G_ENT, (x0 + x1) * 0.5f);
                if (fly) {
                    Piece& p = pieces_[pi];
                    p.src = 0;
                    p.e0 = 0.5f;
                    p.e1 = 0.7f;
                    p.from = TC + V3(10.5f, 1.1f + (lo.y - 12.7f), 14.2f) + V3(0, (hi.y - lo.y) * 0.5f, 0);
                    p.rot = V3(0.f, 0.35f, -0.12f);
                }
            }
        }
        for (float sd : {-1.f, 1.f})
            for (int i = 0; i < 4; i++) {
                const float z0 = -9.f + i * 4.5f, z1 = z0 + 4.5f;
                for (int k = 0; k < 3; k++) {
                    const vec3 lo = k == 0 ? V3(sd * 10.f - 1.f, 12.7f, z0) : (k == 1 ? V3(sd * 10.f - 0.95f, 13.75f, z0) : V3(sd * 10.f - 1.2f, 14.89f, z0));
                    const vec3 hi = k == 0 ? V3(sd * 10.f + 1.f, 13.75f, z1) : (k == 1 ? V3(sd * 10.f + 0.95f, 14.89f, z1) : V3(sd * 10.f + 1.2f, 15.21f, z1));
                    blockAt(lo, hi, 120.f + i * 3 + k + sd * 20.f, R_SHOW, G_ENT, sd * 10.f);
                }
            }
        // the pediment, in slices (a raking cornice on its slopes)
        for (int i = 0; i < 16; i++) {
            const float x0 = -11.2f + i * 1.4f, x1 = x0 + 1.4f;
            auto top = [](float x) { return 15.21f + (11.2f - std::fabs(x)) * 0.27f; };
            Piece p;
            b.v = &pv;
            p.r.first = (int)pv.size();
            b.mat = 1.f;
            const vec3 a0 = V3(x0, 15.21f, 7.3f), a1 = V3(x1, 15.21f, 7.3f);
            const float t0 = top(x0), t1 = top(x1);
            // (front, back, top; the slices meet at the peak)
            b.quad(V3(x0, 15.21f, 8.7f), V3(x1, 15.21f, 8.7f), V3(x1, t1, 8.7f), V3(x0, t0, 8.7f), vec3(0, 0, 1));
            b.quad(a0, a1, V3(x1, t1, 7.3f), V3(x0, t0, 7.3f), vec3(0, 0, -1));
            if (x0 < 0.f && x1 > 0.f) {
                const float tp = top(0.f);
                b.quad(V3(x0, t0, 8.7f), V3(0, tp, 8.7f), V3(0, 15.21f, 8.7f), V3(x0, 15.21f, 8.7f), vec3(0, 0, 1));
                b.quad(V3(0, tp, 8.7f), V3(x1, t1, 8.7f), V3(x1, 15.21f, 8.7f), V3(0, 15.21f, 8.7f), vec3(0, 0, 1));
            }
            // the raking cornice
            const float sgn = (x0 + x1) < 0.f ? 1.f : -1.f;
            const vec3 nr = normalize(vec3(-sgn * 0.27f, 1.f, 0.f));
            auto rk = [&](float xa, float xb) {
                const float ta = top(xa), tb = top(xb);
                b.quad(V3(xa, ta + 0.3f, 6.9f), V3(xb, tb + 0.3f, 6.9f), V3(xb, tb + 0.3f, 9.1f), V3(xa, ta + 0.3f, 9.1f), nr);
                b.quad(V3(xa, ta, 9.1f), V3(xb, tb, 9.1f), V3(xb, tb + 0.3f, 9.1f), V3(xa, ta + 0.3f, 9.1f), vec3(0, 0, 1));
                b.quad(V3(xa, ta, 6.9f), V3(xb, tb, 6.9f), V3(xb, tb + 0.3f, 6.9f), V3(xa, ta + 0.3f, 6.9f), vec3(0, 0, -1));
                b.quad(V3(xa, ta, 6.9f), V3(xb, tb, 6.9f), V3(xb, tb, 9.1f), V3(xa, ta, 9.1f), nr * -1.f);
            };
            if (x0 < 0.f && x1 > 0.f) { rk(x0, 0.f); rk(0.f, x1); }
            else rk(x0, x1);
            p.r.count = (int)pv.size() - p.r.first;
            b.v = &verts_;
            p.role = R_SHOW;
            p.group = G_PED;
            p.a = (x0 + x1) * 0.5f;
            p.c = TC + V3((x0 + x1) * 0.5f, 16.f, 8.f);
            parts.push_back(p);
        }
    }
    b.o = vec3(0, 0, 0);

    // ---------------------------------------------------------------- the braziers
    for (int i = 0; i < 4; i++) {
        const int pi = addKit(K_BRAZIER, BRZ[kind * 4 + i], V3(1, 1, 1), (float)i, R_BRAZIER);
        pieces_[pi].c = BRZ[kind * 4 + i];
    }

    // ---------------------------------------------------------------- the trees
    b.mat = 2.f; b.flag = 0.f;
    const int nt = kind == 2 ? 6 : 10;
    for (int i = 0; i < nt; i++) {
        const vec4& t = TREES[i];
        b.seed = t.w;
        trunkMesh(b, t.x, t.y, t.z * (kind == 2 ? 0.55f : 1.f), t.w, kind == 2 ? 0.2f : 1.f, 64.f);
    }
    const int staticEnd = (int)verts_.size();
    for (int i = 0; i < 6; i++) {
        const vec4& t = JTREES[i];
        Piece p;
        b.v = &pv;
        p.r.first = (int)pv.size();
        b.seed = t.w;
        trunkMesh(b, t.x, t.y, t.z, t.w, kind == 2 ? 0.4f : 1.f, 34.f);
        p.r.count = (int)pv.size() - p.r.first;
        b.v = &verts_;
        p.role = R_SINK;
        p.from = V3(t.x, 0.f, t.y);
        parts.push_back(p);
    }
    b.seed = 0.f;

    // the static mesh: one piece drawn as is
    {
        Piece p;
        p.r.first = firstStatic;
        p.r.count = staticEnd - firstStatic;
        p.role = R_STATIC;
        parts.insert(parts.begin(), p);
    }
    // the kits' pieces grouped by kit, then the parts
    std::stable_sort(pieces_.begin(), pieces_.end(), [](const Piece& a, const Piece& c) { return a.kit < c.kit; });
    const int off = (int)verts_.size();
    verts_.insert(verts_.end(), pv.begin(), pv.end());
    for (size_t i = 1; i < parts.size(); i++) parts[i].r.first += off;
    for (Piece& p : parts) pieces_.push_back(p);
    vis_.assign(pieces_.size(), -1.f);
    instTris_ = 0;
    for (const Piece& p : pieces_) instTris_ += (p.kit >= 0 ? kit_[p.kit].count : p.r.count) / 3;
    if (kind == 1) buildFig(0.4f);
    else { fig_.clear(); figChanged_ = true; }
}

// KHMER's strangler fig: its trunk straddling the gallery, its roots pouring over it, each withdrawing from its tip
// back toward the fig as rk falls (GLSL's roots() of the ray-marched version)
void Scene::buildFig(float rk) {
    fig_.clear();
    figChanged_ = true;
    figK_ = rk;
    if (rk <= 0.02f) return;
    B b{&fig_};
    b.mat = 2.f;
    b.seed = 0.7f;
    b.flag = 0.f;
    const float w0 = 0.25f + 0.75f * rk;
    {
        std::vector<vec3> pts;
        std::vector<float> rad;
        for (int i = 0; i <= 12; i++) {
            const float u = i / 12.f;
            pts.push_back(lerp(V3(-10.f, 8.f, -41.f), V3(-12.4f, 64.f, -43.f), u));
            rad.push_back(lerpf(1.8f, 1.f, std::min(1.f, u * 2.f)) * w0);
        }
        tube(b, pts, rad, 20);
    }
    for (int i = 0; i < 6; i++) {
        const float fi = (float)i;
        const float reach = saturate(rk * 1.6f - fi * 0.08f);
        if (reach <= 0.02f) continue;
        const float x0 = -3.6f + fi * 1.45f + 0.4f * std::sin(fi * 2.7f);
        const float w = (0.62f - 0.05f * std::fabs(fi - 2.5f)) * w0;
        const vec3 K[6] = {V3(-10.f + x0 * 0.3f, 10.2f, -40.6f), V3(-10.f + x0 * 0.8f, 7.f, -39.25f),
                           V3(-10.f + x0 + 0.3f * std::sin(fi * 1.9f), 4.3f, -39.f), V3(-10.f + x0 * 1.15f, 3.8f, -37.75f),
                           V3(-10.f + x0 * 1.3f, 0.25f, -37.3f), V3(-10.f + x0 * 1.6f, 0.1f, -34.f + 0.8f * std::sin(fi * 3.3f))};
        const float rr[6] = {w, w * 0.8f, w * 0.65f, w * 0.55f, w * 0.45f, w * 0.18f};
        // (the points along the root, Catmull-Rom between the knots, up to its reach)
        std::vector<vec3> pts;
        std::vector<float> rad;
        const float total = reach * 5.f;
        const int N = 40;
        for (int s = 0; s <= N; s++) {
            const float u = total * s / N;
            const int k = std::min(4, (int)u);
            const float t = u - k;
            const vec3 p0 = K[std::max(0, k - 1)], p1 = K[k], p2 = K[k + 1], p3 = K[std::min(5, k + 2)];
            const float t2 = t * t, t3 = t2 * t;
            const vec3 p = (p1 * 2.f + (p2 - p0) * t + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2 + (p1 * 3.f - p0 - p2 * 3.f + p3) * t3) * 0.5f;
            pts.push_back(p);
            float r = lerpf(rr[k], rr[k + 1], t);
            r *= std::min(1.f, (total - u) / 0.35f + 0.08f);
            rad.push_back(r);
        }
        tube(b, pts, rad, 10);
    }
}

// =====================================================================================================
// The restoration, each frame
// =====================================================================================================

void Scene::animate(const State& st, float dt, bool snap, std::vector<Inst>& inst, std::vector<Draw>& draws) {
    inst.clear();
    draws.clear();
    const float T = st.temple;
    // the groups' conditions
    auto tiers = [&](int t) {
        const float tc = sstep(0.78f, 1.f, T);
        if (t == 0) return lerpf(1.f, 7.f, tc);
        if (t == 1) return lerpf(4.2f, 7.f, tc);
        if (t == 2) return lerpf(2.f, 7.f, sstep(0.55f, 0.78f, T));
        return lerpf(1.4f, 7.f, sstep(0.33f, 0.55f, T));
    };
    auto shown = [&](const Piece& p) -> bool {
        switch (p.group) {
        case G_SHRINE: {
            const float ru = 1.f - sstep(0.5f, 1.f, T);
            const float cut = lerpf(31.5f, 23.4f + 1.8f * gnoise(p.a * 0.8f, p.b * 0.8f) + 0.06f * p.a, ru);
            return p.c.y - TC.y < cut;
        }
        case G_CORNER: return T < p.a;
        case G_GALLERY: {
            const float fall = 1.f - sstep(0.f, 0.33f, T);
            if (p.role == R_GONE) return fall > 1.f - p.a * 1.2f + 0.05f && fall > 0.02f;
            if (fall <= 0.f) return true;
            const float top = 4.6f - 6.5f * fall + 0.8f * fall * gnoise(p.a * 1.1f, 1.1f * 0.f);
            const bool inside = std::fabs(p.a - 12.f) < 6.f * std::sqrt(fall) + 0.5f && (p.c.y - 6.8f) > top;
            return !inside;
        }
        case G_TIER: return tiers((int)p.a) - p.b >= 0.5f;
        case G_CELLA: {
            const float wc = sstep(0.f, 0.25f, T);
            const vec3 q = p.c - TC;
            const float cut = lerpf(7.f + 4.5f * gnoise(q.x * 0.3f, q.z * 0.3f) + 1.2f * std::sin(q.x * 0.4f), 12.4f, wc);
            return q.y < cut;
        }
        case G_GCOL: {
            const float hb = lerpf(p.a, 10.f, sstep(p.e0, p.e1, T));
            return p.b <= hb + 0.01f;
        }
        case G_ENT: return p.a < lerpf(2.5f, 12.f, sstep(0.5f, 0.75f, T));
        case G_PED: return p.a < lerpf(-1.6f, 12.f, sstep(0.75f, 1.f, T));
        default: return true;
        }
    };
    const float rate = 1.2f * dt;
    auto emit = [&](const Piece& p, const Aff& D, float dissolve, float glow) {
        const Aff Rr = p.kit >= 0 ? Aff::tr(p.c) * Aff::sc(p.s) : Aff();
        const Aff M = D * Rr;
        Inst in;
        const vec3 cols[4] = {M.c0, M.c1, M.c2, M.t};
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 4; c++) {
                const vec3& v = cols[c];
                in.m[r * 4 + c] = r == 0 ? v.x : (r == 1 ? v.y : v.z);
            }
        const vec3 c = p.kit >= 0 ? p.c : vec3(0, 0, 0), s = p.kit >= 0 ? p.s : vec3(1, 1, 1);
        in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = p.seed;
        in.s[0] = s.x; in.s[1] = s.y; in.s[2] = s.z; in.s[3] = dissolve;
        in.p[0] = glow; in.p[1] = p.mat; in.p[2] = p.kit >= 0 ? 0.f : 1.f; in.p[3] = 0.f;
        inst.push_back(in);
    };
    int curKit = -2, kitFirst = 0;
    auto flush = [&]() {
        if (curKit >= 0 && (int)inst.size() > kitFirst) draws.push_back({kit_[curKit], kitFirst, (int)inst.size() - kitFirst, false});
    };
    for (size_t i = 0; i < pieces_.size(); i++) {
        const Piece& p = pieces_[i];
        if (p.kit != curKit) {
            flush();
            curKit = p.kit;
            kitFirst = (int)inst.size();
        }
        Aff D;
        float target = 0.f;
        switch (p.role) {
        case R_COLTOP: {
            const float e = sstep(p.e0, p.e1, st.col[p.idx]);
            if (e < 1.f) {
                const float sd = p.rot.x, zc = -3.5f - 7.f * (p.idx / 2);
                const Aff base = Aff::tr(V3(sd * 7.5f, 0.75f, zc)) * Aff::sc(V3(sd, 1, 1));
                const float a = (1.f - e) * 1.5708f;
                const vec3 pos = lerp(p.from, V3(0, p.a, 0), e) + V3(0, std::sin(PI * e) * 1.6f, 0);
                Aff Tq;
                Tq.c0 = V3(std::cos(a), -std::sin(a), 0);
                Tq.c1 = V3(std::sin(a), std::cos(a), 0);
                Tq.c2 = V3(0, 0, 1);
                Tq = Aff::tr(pos) * Aff::rotY(p.b * (1.f - e)) * Tq * Aff::tr(V3(0, -p.a, 0));
                const Aff inv = Aff::sc(V3(sd, 1, 1)) * Aff::tr(V3(-sd * 7.5f, -0.75f, -zc));
                D = base * Tq * inv;
            }
            break;
        }
        case R_FLY: {
            const float src = p.src == 0 ? T : st.col[p.src - 1];
            const float e = sstep(p.e0, p.e1, src);
            if (e < 1.f) {
                const vec3 pos = lerp(p.from, p.c, e) + V3(0, std::sin(PI * e) * 2.f, 0);
                D = Aff::tr(pos) * tumble(p.rot, 1.f - e) * Aff::tr(p.c * -1.f);
            }
            break;
        }
        case R_SHOW: target = shown(p) ? 0.f : 1.f; break;
        case R_GONE: {
            target = shown(p) ? 0.f : 1.f;
            D = Aff::tr(p.c) * tumble(p.rot, 1.f) * Aff::tr(p.c * -1.f);
            break;
        }
        case R_BRAZIER:
            target = st.prime > 0.01f ? 0.f : 1.f;
            D = Aff::tr(V3(0, -1.4f * (1.f - st.prime), 0));
            break;
        case R_SINK: {
            const float k = sstep(0.f, 1.f, st.jungle);
            if (k < 0.01f) target = 1.f;
            else D = Aff::tr(p.from) * Aff::sc(V3(std::sqrt(k), k, std::sqrt(k))) * Aff::tr(p.from * -1.f);
            break;
        }
        default: break;
        }
        float& v = vis_[i];
        if (snap || v < 0.f || p.role == R_SINK || p.role == R_BRAZIER) v = target;
        else v = v < target ? std::min(target, v + rate) : std::max(target, v - rate);
        if (v >= 0.999f) continue;
        const float glow = p.role == R_SHOW || p.role == R_GONE ? v * (1.f - v) * 4.f : 0.f;
        if (p.kit < 0) {
            emit(p, D, v, glow);
            draws.push_back({p.r, (int)inst.size() - 1, 1, false});
            kitFirst = (int)inst.size();
        } else emit(p, D, v, glow);
    }
    flush();
    // the fig's roots, withdrawing (rebuilt when they changed enough)
    figChanged_ = false;
    if (kind_ == 1) {
        const float rk = saturate(std::max(st.jungle, st.wear * 1.2f - 0.1f));
        if (std::fabs(rk - figK_) > 0.004f || (rk <= 0.02f && figK_ > 0.02f)) buildFig(rk);
        if (!fig_.empty()) {
            Piece p;
            emit(p, Aff(), 0.f, 0.f);
            draws.push_back({Range{0, (int)fig_.size()}, (int)inst.size() - 1, 1, true});
        }
    }
}

} // namespace ruins
