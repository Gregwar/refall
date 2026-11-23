#pragma once
// TERRARIUM's mesh builders (world.cpp, lab.cpp): a vertex builder, parametric surfaces with exact normals,
// ellipsoids, tubes with parallel-transported frames, lathes.
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "universes/terrarium/world.hpp"

namespace terra {
namespace bld {

inline float fr(float v) { return v - std::floor(v); }
inline float sat(float x) { return std::min(1.f, std::max(0.f, x)); }
inline float sstep(float a, float b, float x) {
    const float t = sat((x - a) / (b - a));
    return t * t * (3.f - 2.f * t);
}
inline vec3 V3(float x, float y, float z) { return vec3(x, y, z); }
inline vec2 hash2(float x, float y) {
    float a = fr(x * 0.1031f), b = fr(y * 0.1030f), c = fr(x * 0.0973f);
    const float d = a * (b + 33.33f) + b * (c + 33.33f) + c * (a + 33.33f);
    a += d; b += d; c += d;
    return vec2{fr((a + b) * c), fr((a + c) * b)};
}
// GLSL's rot(a) * v (mat2(c, s, -s, c)) and its inverse, on (x, z)
inline vec2 rotF(float a, float x, float z) { const float c = std::cos(a), s = std::sin(a); return vec2{c * x - s * z, s * x + c * z}; }
inline vec2 rotI(float a, float x, float z) { const float c = std::cos(a), s = std::sin(a); return vec2{c * x + s * z, -s * x + c * z}; }
// a local point (rotated by rot(a) on xz, as the shaders' q.xz = rot(a) * q.xz) back to the parent frame
inline vec3 unrot(float a, const vec3& q) { const vec2 r = rotI(a, q.x, q.z); return V3(r.x, q.y, r.y); }

constexpr float PI_ = 3.14159265f, TAU_ = 6.2831853f;

// ---------------------------------------------------------------- the builder
struct B {
    std::vector<Vtx>* v;
    float mat = 1.f, ao = 1.f, seed = 0.f, two = 0.f;
    float sw[4] = {0, 0, 0, 0};
    float gr[4] = {0, 0, 0, -1000.f}; // the growth: pivot, appear line (-1000: none; see world.hpp)
    void vert(const vec3& p, const vec3& n, const vec4& f) {
        Vtx x;
        x.p[0] = p.x; x.p[1] = p.y; x.p[2] = p.z;
        x.n[0] = n.x; x.n[1] = n.y; x.n[2] = n.z;
        x.m[0] = mat; x.m[1] = ao; x.m[2] = seed; x.m[3] = two;
        x.f[0] = f.x; x.f[1] = f.y; x.f[2] = f.z; x.f[3] = f.w;
        for (int i = 0; i < 4; i++) x.s[i] = sw[i];
        for (int i = 0; i < 4; i++) x.g[i] = gr[i];
        v->push_back(x);
    }
    void noSway() { sw[0] = sw[1] = sw[2] = sw[3] = 0.f; }
};

// A parametric surface over (u, v) in [0, 1]^2: nu x nv quads, exact normals from the function's derivatives,
// turned away from `out` when it is given (closed shapes), left as they are otherwise (two-sided leaves).
template <class P, class F>
inline void surf(B& b, int nu, int nv, P pos, F inf, const vec3* out) {
    const int W = nu + 1, H = nv + 1;
    std::vector<vec3> ps(W * H), ns(W * H);
    std::vector<vec4> fs(W * H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) ps[j * W + i] = pos((float)i / nu, (float)j / nv);
    // (closed around u: the seam's neighbours wrap)
    bool wrapU = true;
    for (int j = 0; j < H && wrapU; j++) {
        const vec3 d = ps[j * W] - ps[j * W + nu];
        wrapU = dot(d, d) < 1e-12f;
    }
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            const float u = (float)i / nu, v = (float)j / nv;
            const vec3 p = ps[j * W + i];
            const float e = 1e-3f;
            // the derivatives from the grid's neighbours (exactly at the poles, where they vanish)
            int i0 = i - 1, i1 = i + 1;
            if (wrapU) { if (i0 < 0) i0 = nu - 1; if (i1 > nu) i1 = 1; }
            else { i0 = std::max(i0, 0); i1 = std::min(i1, nu); }
            const vec3 du = ps[j * W + i1] - ps[j * W + i0];
            const vec3 dv = ps[std::min(j + 1, nv) * W + i] - ps[std::max(j - 1, 0) * W + i];
            vec3 n = cross(du, dv);
            if (dot(n, n) < 1e-16f) {
                // a pole: from a little way off it
                const float u2 = u < 0.5f ? u + 0.02f : u - 0.02f, v2 = v < 0.5f ? v + 0.02f : v - 0.02f;
                const vec3 a = pos(std::min(u2 + e, 1.f), v2) - pos(std::max(u2 - e, 0.f), v2);
                const vec3 c = pos(u2, std::min(v2 + e, 1.f)) - pos(u2, std::max(v2 - e, 0.f));
                n = cross(a, c);
                if (dot(n, n) < 1e-14f) n = out ? p - *out : vec3(0, 1, 0);
            }
            n = normalize(n);
            if (out && dot(n, p - *out) < 0.f) n = n * -1.f;
            ns[j * W + i] = n;
            fs[j * W + i] = inf(p, u, v);
        }
    for (int j = 0; j < nv; j++)
        for (int i = 0; i < nu; i++) {
            const int a = j * W + i, bb = a + 1, c = a + W + 1, d = a + W;
            for (int k : {a, bb, c, a, c, d}) b.vert(ps[k], ns[k], fs[k]);
        }
}

// an ellipsoid (center, radii, its axes), displaced along its normal by disp(q) (q: the point in the ellipsoid's
// frame, from its center), from its top down to the polar angle thMax
template <class D, class F>
inline void ellipsoid(B& b, const vec3& c, const vec3& r, const vec3& ex, const vec3& ey, const vec3& ez, int nu, int nv, D disp, F inf,
               float thMax = 3.14159265f) {
    auto local = [&](float u, float v) {
        const float th = v * thMax, ph = u * TAU_;
        const vec3 d(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
        const vec3 q(d.x * r.x, d.y * r.y, d.z * r.z);
        const vec3 gn = normalize(vec3(q.x / (r.x * r.x), q.y / (r.y * r.y), q.z / (r.z * r.z)));
        return q - gn * disp(q);
    };
    auto pos = [&](float u, float v) { const vec3 q = local(u, v); return c + ex * q.x + ey * q.y + ez * q.z; };
    auto fi = [&](const vec3&, float u, float v) { return inf(local(u, v)); };
    surf(b, nu, nv, pos, fi, &c);
}
template <class F>
inline void ellipsoidS(B& b, const vec3& c, const vec3& r, int nu, int nv, F inf) {
    ellipsoid(b, c, r, V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), nu, nv, [](const vec3&) { return 0.f; }, inf);
}
inline void sphere(B& b, const vec3& c, float r, int nu, int nv, const vec4& f) {
    ellipsoidS(b, c, vec3(r), nu, nv, [&](const vec3&) { return f; });
}

// a tube through points (radii, parameters), parallel-transported frames, round ends
inline void tube(B& b, const std::vector<vec3>& pts, const std::vector<float>& rad, const std::vector<vec4>& inf, int sides, bool caps = true) {
    const int n = (int)pts.size();
    if (n < 2) return;
    std::vector<vec3> T(n), N(n), Bn(n);
    for (int i = 0; i < n; i++) T[i] = normalize(pts[std::min(i + 1, n - 1)] - pts[std::max(i - 1, 0)]);
    vec3 up = std::fabs(T[0].y) < 0.9f ? V3(0, 1, 0) : V3(1, 0, 0);
    N[0] = normalize(cross(cross(T[0], up), T[0]));
    for (int i = 1; i < n; i++) {
        vec3 nn = N[i - 1] - T[i] * dot(N[i - 1], T[i]);
        if (dot(nn, nn) < 1e-10f) nn = N[i - 1];
        N[i] = normalize(nn);
    }
    for (int i = 0; i < n; i++) Bn[i] = cross(T[i], N[i]);
    auto P = [&](int i, int s) {
        const float a = TAU_ * s / sides;
        return N[i] * std::cos(a) + Bn[i] * std::sin(a);
    };
    for (int i = 0; i + 1 < n; i++)
        for (int s = 0; s < sides; s++) {
            const vec3 d00 = P(i, s), d01 = P(i, s + 1), d10 = P(i + 1, s), d11 = P(i + 1, s + 1);
            const vec3 a = pts[i] + d00 * rad[i], bb = pts[i] + d01 * rad[i], c = pts[i + 1] + d11 * rad[i + 1], d = pts[i + 1] + d10 * rad[i + 1];
            b.vert(a, d00, inf[i]); b.vert(bb, d01, inf[i]); b.vert(c, d11, inf[i + 1]);
            b.vert(a, d00, inf[i]); b.vert(c, d11, inf[i + 1]); b.vert(d, d10, inf[i + 1]);
        }
    if (caps) {
        const int rs = std::max(2, sides / 3);
        for (int e = 0; e < 2; e++) {
            const int i = e == 0 ? 0 : n - 1;
            const vec3 ax = T[i] * (e == 0 ? -1.f : 1.f);
            for (int k = 0; k < rs; k++)
                for (int s = 0; s < sides; s++) {
                    const float t0 = (float)k / rs * PI_ * 0.5f, t1 = (float)(k + 1) / rs * PI_ * 0.5f;
                    auto Q = [&](float t, int ss) { return P(i, ss) * std::cos(t) + ax * std::sin(t); };
                    const vec3 a = Q(t0, s), bb = Q(t0, s + 1), c = Q(t1, s + 1), d = Q(t1, s);
                    for (const vec3* q : {&a, &bb, &c, &a, &c, &d}) b.vert(pts[i] + *q * rad[i], *q, inf[i]);
                }
        }
    }
}
// a tapered capsule a -> b (radii ra, rb)
inline void capsule(B& b, const vec3& a, const vec3& c, float ra, float rb, const vec4& f, int sides = 8, int segs = 2) {
    std::vector<vec3> pts;
    std::vector<float> rad;
    std::vector<vec4> inf;
    for (int i = 0; i <= segs; i++) {
        const float t = (float)i / segs;
        pts.push_back(lerp(a, c, t));
        rad.push_back(lerpf(ra, rb, t));
        inf.push_back(f);
    }
    tube(b, pts, rad, inf, sides);
}

// a revolved profile (r, y) from the bottom up, with its normals (nr, ny), segs around
inline void latheN(B& b, const std::vector<vec2>& prof, const std::vector<vec2>& nrm, int segs, const std::function<vec4(const vec3&)>& inf) {
    for (size_t i = 0; i + 1 < prof.size(); i++)
        for (int s = 0; s < segs; s++) {
            const float t0 = TAU_ * s / segs, t1 = TAU_ * (s + 1) / segs;
            const float c0 = std::cos(t0), s0 = std::sin(t0), c1 = std::cos(t1), s1 = std::sin(t1);
            auto P = [&](size_t k, float c, float sn) { return V3(prof[k].x * c, prof[k].y, prof[k].x * sn); };
            auto N = [&](size_t k, float c, float sn) { return normalize(V3(nrm[k].x * c, nrm[k].y, nrm[k].x * sn)); };
            const vec3 a = P(i, c0, s0), bb = P(i, c1, s1), c = P(i + 1, c1, s1), d = P(i + 1, c0, s0);
            const vec3 na = N(i, c0, s0), nb = N(i, c1, s1), nc = N(i + 1, c1, s1), nd = N(i + 1, c0, s0);
            b.vert(a, na, inf(a)); b.vert(bb, nb, inf(bb)); b.vert(c, nc, inf(c));
            b.vert(a, na, inf(a)); b.vert(c, nc, inf(c)); b.vert(d, nd, inf(d));
        }
}

} // namespace bld
} // namespace terra
