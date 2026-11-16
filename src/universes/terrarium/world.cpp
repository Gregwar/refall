// TERRARIUM's world as meshes (see world.hpp): the builders (parametric surfaces with exact normals, lathes,
// tubes), the glass and the cork from the jar's signed distance, the room, and every plant and creature from the
// shapes the ray march used (the same parameters, hashes and growth), now explicit geometry.
#include "universes/terrarium/world.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace terra {

namespace {

float fr(float v) { return v - std::floor(v); }
float sat(float x) { return std::min(1.f, std::max(0.f, x)); }
float sstep(float a, float b, float x) {
    const float t = sat((x - a) / (b - a));
    return t * t * (3.f - 2.f * t);
}
vec3 V3(float x, float y, float z) { return vec3(x, y, z); }
vec2 hash2(float x, float y) {
    float a = fr(x * 0.1031f), b = fr(y * 0.1030f), c = fr(x * 0.0973f);
    const float d = a * (b + 33.33f) + b * (c + 33.33f) + c * (a + 33.33f);
    a += d; b += d; c += d;
    return vec2{fr((a + b) * c), fr((a + c) * b)};
}
// GLSL's rot(a) * v (mat2(c, s, -s, c)) and its inverse, on (x, z)
vec2 rotF(float a, float x, float z) { const float c = std::cos(a), s = std::sin(a); return vec2{c * x - s * z, s * x + c * z}; }
vec2 rotI(float a, float x, float z) { const float c = std::cos(a), s = std::sin(a); return vec2{c * x + s * z, -s * x + c * z}; }
// a local point (rotated by rot(a) on xz, as the shaders' q.xz = rot(a) * q.xz) back to the parent frame
vec3 unrot(float a, const vec3& q) { const vec2 r = rotI(a, q.x, q.z); return V3(r.x, q.y, r.y); }

const float PI_ = 3.14159265f, TAU_ = 6.2831853f;

// ---------------------------------------------------------------- the builder
struct B {
    std::vector<Vtx>* v;
    float mat = 1.f, ao = 1.f, seed = 0.f, two = 0.f;
    float sw[4] = {0, 0, 0, 0};
    void vert(const vec3& p, const vec3& n, const vec4& f) {
        Vtx x;
        x.p[0] = p.x; x.p[1] = p.y; x.p[2] = p.z;
        x.n[0] = n.x; x.n[1] = n.y; x.n[2] = n.z;
        x.m[0] = mat; x.m[1] = ao; x.m[2] = seed; x.m[3] = two;
        x.f[0] = f.x; x.f[1] = f.y; x.f[2] = f.z; x.f[3] = f.w;
        for (int i = 0; i < 4; i++) x.s[i] = sw[i];
        v->push_back(x);
    }
    void noSway() { sw[0] = sw[1] = sw[2] = sw[3] = 0.f; }
};

// A parametric surface over (u, v) in [0, 1]^2: nu x nv quads, exact normals from the function's derivatives,
// turned away from `out` when it is given (closed shapes), left as they are otherwise (two-sided leaves).
template <class P, class F>
void surf(B& b, int nu, int nv, P pos, F inf, const vec3* out) {
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
void ellipsoid(B& b, const vec3& c, const vec3& r, const vec3& ex, const vec3& ey, const vec3& ez, int nu, int nv, D disp, F inf,
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
void ellipsoidS(B& b, const vec3& c, const vec3& r, int nu, int nv, F inf) {
    ellipsoid(b, c, r, V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), nu, nv, [](const vec3&) { return 0.f; }, inf);
}
void sphere(B& b, const vec3& c, float r, int nu, int nv, const vec4& f) {
    ellipsoidS(b, c, vec3(r), nu, nv, [&](const vec3&) { return f; });
}

// a tube through points (radii, parameters), parallel-transported frames, round ends
void tube(B& b, const std::vector<vec3>& pts, const std::vector<float>& rad, const std::vector<vec4>& inf, int sides, bool caps = true) {
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
void capsule(B& b, const vec3& a, const vec3& c, float ra, float rb, const vec4& f, int sides = 8, int segs = 2) {
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
void latheN(B& b, const std::vector<vec2>& prof, const std::vector<vec2>& nrm, int segs, const std::function<vec4(const vec3&)>& inf) {
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

// ---------------------------------------------------------------- the jar's signed distance (as the shaders')
float sdBox2(float px, float py, float bx, float by) {
    const float qx = std::fabs(px) - bx, qy = std::fabs(py) - by;
    const float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f);
}
float sdRoundBox2(float px, float py, float bx, float by, float rTop, float rBot) {
    const float r = py > 0.f ? rTop : rBot;
    const float qx = std::fabs(px) - bx + r, qy = std::fabs(py) - by + r;
    const float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
    return std::min(std::max(qx, qy), 0.f) + std::sqrt(ox * ox + oy * oy) - r;
}
float smin(float a, float b, float k) {
    const float h = sat(0.5f + 0.5f * (b - a) / k);
    return lerpf(b, a, h) - k * h * (1.f - h);
}
float jarOuter(float r, float y) {
    const float body = sdRoundBox2(r, y - JHB * 0.5f, JR, JHB * 0.5f, 0.36f, 0.12f);
    const float neck = sdBox2(r, y - (JHB - 0.2f + JHN) * 0.5f, JRN, (JHN - JHB + 0.2f) * 0.5f);
    float d = smin(body, neck, 0.07f);
    const float lx = r - JRN + 0.004f, ly = y - JHN + 0.012f;
    d = std::min(d, std::sqrt(lx * lx + ly * ly) - 0.02f);
    return d;
}
float corkD(float r, float y) {
    const float qy = y - (JHN + 0.02f);
    const float rr = JRN - 0.03f + 0.03f * sat(qy / 0.12f);
    return sdRoundBox2(r, qy, rr, 0.11f, 0.03f, 0.f);
}

// a revolved profile from a 2D distance f(r, y): its zero set, seen from (0, cy) at n angles from straight down to
// straight up (bisection along each ray), its normals from f's gradient
void profileOf(const std::function<float(float, float)>& f, float cy, int n, std::vector<vec2>& prof, std::vector<vec2>& nrm) {
    prof.clear();
    nrm.clear();
    for (int i = 0; i <= n; i++) {
        const float th = -PI_ * 0.5f + PI_ * i / n;
        const float dx = std::cos(th), dy = std::sin(th);
        float lo = 0.f, hi = 1.6f;
        for (int k = 0; k < 40; k++) {
            const float m = (lo + hi) * 0.5f;
            if (f(std::max(dx * m, 0.f), cy + dy * m) < 0.f) lo = m;
            else hi = m;
        }
        const float t = (lo + hi) * 0.5f;
        const float r = std::max(dx * t, 0.f), y = cy + dy * t;
        const float e = 1e-3f;
        float gx = f(r + e, y) - f(std::max(r - e, 0.f), y), gy = f(r, y + e) - f(r, y - e);
        if (i == 0) { gx = 0.f; gy = -1.f; }
        if (i == n) { gx = 0.f; gy = 1.f; }
        const float l = std::sqrt(gx * gx + gy * gy) + 1e-9f;
        prof.push_back(vec2{r, y});
        nrm.push_back(vec2{gx / l, gy / l});
    }
}
float cavityR(float y) {
    // the cavity's radius at height y (above its floor)
    float lo = 0.f, hi = 1.1f;
    for (int k = 0; k < 40; k++) {
        const float m = (lo + hi) * 0.5f;
        if (jarOuter(m, y) + JT < 0.f) lo = m;
        else hi = m;
    }
    return lo;
}

} // namespace

// ---------------------------------------------------------------- noise (GLSL's, on the CPU)
float ghash(float x, float y) {
    float a = fr(x * 0.1031f), b = fr(y * 0.1031f), c = fr(x * 0.1031f);
    const float d = a * (b + 33.33f) + b * (c + 33.33f) + c * (a + 33.33f);
    a += d; b += d; c += d;
    return fr((a + b) * c);
}
float ghash1(float n) { return ghash(n, n * 0.371f + 17.13f); }
float ghash3(const vec3& p0) {
    float a = fr(p0.x * 0.1031f), b = fr(p0.y * 0.1031f), c = fr(p0.z * 0.1031f);
    const float d = a * (c + 31.32f) + b * (b + 31.32f) + c * (a + 31.32f);
    a += d; b += d; c += d;
    return fr((a + b) * c);
}
float gnoise3(const vec3& p) {
    const vec3 i(std::floor(p.x), std::floor(p.y), std::floor(p.z));
    const vec3 f = p - i;
    const vec3 u(f.x * f.x * (3 - 2 * f.x), f.y * f.y * (3 - 2 * f.y), f.z * f.z * (3 - 2 * f.z));
    auto h = [&](float x, float y, float z) { return ghash3(i + V3(x, y, z)); };
    const float a = lerpf(lerpf(h(0, 0, 0), h(1, 0, 0), u.x), lerpf(h(0, 1, 0), h(1, 1, 0), u.x), u.y);
    const float bb = lerpf(lerpf(h(0, 0, 1), h(1, 0, 1), u.x), lerpf(h(0, 1, 1), h(1, 1, 1), u.x), u.y);
    return lerpf(a, bb, u.z);
}

float soilH(float x, float z, const Pond& pd) {
    float h = 0.2f;
    const float ax = x + 0.5f, az = z + 0.45f, bx = x - 0.55f, bz = z + 0.5f, cx = x, cz = z + 0.72f;
    h += 0.055f * std::exp(-(ax * ax + az * az) * 5.f) + 0.045f * std::exp(-(bx * bx + bz * bz) * 6.f) +
         0.035f * std::exp(-(cx * cx + cz * cz) * 4.f);
    h -= 0.02f * z;
    h += 0.006f * std::sin(x * 9.f + 1.f) * std::sin(z * 8.f + 2.f);
    const float px = x - pd.x, pz = z - pd.z;
    const float dp = std::sqrt(px * px + pz * pz) / pd.r;
    h -= pd.depth * (1.f - sstep(0.5f, 1.15f, dp));
    return h;
}

// ---------------------------------------------------------------- the fixed meshes
void buildGlass(std::vector<Vtx>& out, bool inner) {
    B b{&out};
    b.mat = inner ? 31.f : 30.f;
    std::vector<vec2> prof, nrm;
    profileOf([&](float r, float y) { return jarOuter(r, y) + (inner ? JT : 0.f); }, 0.45f, 180, prof, nrm);
    latheN(b, prof, nrm, 112, [](const vec3&) { return vec4(0, 0, 0, 0); });
}

void buildCork(std::vector<Vtx>& out) {
    B b{&out};
    b.mat = 19.f;
    std::vector<vec2> prof, nrm;
    profileOf(corkD, JHN + 0.02f, 60, prof, nrm);
    latheN(b, prof, nrm, 72, [](const vec3&) { return vec4(0, 0, 0, 0); });
}

void buildRoom(std::vector<Vtx>& out) {
    B b{&out};
    const float ZW = -2.3f, SF = 1.3f, X = 60.f;
    auto quad = [&](const vec3& a, const vec3& bb, const vec3& c, const vec3& d, const vec3& n) {
        const vec4 f(0, 0, 0, 0);
        b.vert(a, n, f); b.vert(bb, n, f); b.vert(c, n, f);
        b.vert(a, n, f); b.vert(c, n, f); b.vert(d, n, f);
    };
    b.mat = 1.f; // the sill's top
    quad(V3(-X, 0, ZW), V3(X, 0, ZW), V3(X, 0, SF), V3(-X, 0, SF), V3(0, 1, 0));
    b.mat = 2.f; // its front
    quad(V3(-X, -40, SF), V3(X, -40, SF), V3(X, 0, SF), V3(-X, 0, SF), V3(0, 0, 1));
    b.mat = 3.f; // the window wall
    quad(V3(-X, 0, ZW), V3(X, 0, ZW), V3(X, 40, ZW), V3(-X, 40, ZW), V3(0, 0, 1));
    // the shelves beside and above the window (as the old shelfA: top, x0, x1)
    const float S[3][3] = {{2.95f, -6.6f, 6.6f}, {1.35f, -7.0f, -2.75f}, {1.35f, 2.75f, 7.0f}};
    b.mat = 4.f;
    for (auto& s : S) {
        const float y1 = s[0], y0 = s[0] - 0.09f, x0 = s[1], x1 = s[2], z0 = ZW, z1 = ZW + 0.72f;
        quad(V3(x0, y1, z0), V3(x1, y1, z0), V3(x1, y1, z1), V3(x0, y1, z1), V3(0, 1, 0));
        quad(V3(x0, y0, z0), V3(x1, y0, z0), V3(x1, y0, z1), V3(x0, y0, z1), V3(0, -1, 0));
        quad(V3(x0, y0, z1), V3(x1, y0, z1), V3(x1, y1, z1), V3(x0, y1, z1), V3(0, 0, 1));
        quad(V3(x0, y0, z0), V3(x0, y0, z1), V3(x0, y1, z1), V3(x0, y1, z0), V3(-1, 0, 0));
        quad(V3(x1, y0, z0), V3(x1, y0, z1), V3(x1, y1, z1), V3(x1, y1, z0), V3(1, 0, 0));
    }
}

void buildTube(std::vector<Vtx>& out, const vec3& a, const vec3& c, const vec3& bq, float r) {
    B b{&out};
    b.mat = 40.f;
    const int N = 72, S = 24;
    auto P = [&](float t) { return lerp(lerp(a, c, t), lerp(c, bq, t), t); };
    std::vector<vec3> pts;
    std::vector<float> rad;
    std::vector<vec4> inf;
    for (int i = 0; i <= N; i++) {
        const float t = (float)i / N;
        pts.push_back(P(t));
        rad.push_back(r);
        const vec3 T = normalize((c - a) * (2.f * (1.f - t)) + (bq - c) * (2.f * t));
        inf.push_back(vec4(t, T.x, T.y, T.z));
    }
    tube(b, pts, rad, inf, S, false);
}

// ---------------------------------------------------------------- the soil and the pond
void buildSoil(std::vector<Vtx>& out, const Pond& pd, int kind) {
    (void)kind;
    B b{&out};
    b.mat = 1.f;
    const float R = cavityR(0.2f) + 0.004f;
    const int NR = 44, NA = 160;
    auto P = [&](float rr, float a) {
        const float x = std::cos(a) * rr, z = std::sin(a) * rr;
        return V3(x, soilH(x, z, pd), z);
    };
    auto Nn = [&](float x, float z) {
        const float e = 0.002f;
        const float hx = soilH(x + e, z, pd) - soilH(x - e, z, pd), hz = soilH(x, z + e, pd) - soilH(x, z - e, pd);
        return normalize(V3(-hx, 2.f * e, -hz));
    };
    const vec4 f0(0, 0, 0, 0);
    for (int i = 0; i < NR; i++)
        for (int k = 0; k < NA; k++) {
            const float r0 = R * i / NR, r1 = R * (i + 1) / NR;
            const float a0 = TAU_ * k / NA, a1 = TAU_ * (k + 1) / NA;
            const vec3 p00 = P(r0, a0), p01 = P(r0, a1), p11 = P(r1, a1), p10 = P(r1, a0);
            for (const vec3* p : {&p00, &p11, &p01, &p00, &p10, &p11}) b.vert(*p, Nn(p->x, p->z), f0);
        }
    // the section against the glass: the cavity's own profile, up to the soil
    b.mat = 2.f;
    for (int k = 0; k < NA; k++) {
        const float a0 = TAU_ * k / NA, a1 = TAU_ * (k + 1) / NA;
        auto col = [&](float a, int i) {
            // row i of the column at angle a: from the floor up to the soil's height at the glass
            const float top = soilH(std::cos(a) * R, std::sin(a) * R, pd);
            const float y = JT + 0.0005f + (top - JT - 0.0005f) * i / 14.f;
            const float r = cavityR(y);
            return V3(std::cos(a) * r, y, std::sin(a) * r);
        };
        for (int i = 0; i < 14; i++) {
            const vec3 p00 = col(a0, i), p01 = col(a1, i), p11 = col(a1, i + 1), p10 = col(a0, i + 1);
            const vec3 n0 = normalize(V3(std::cos(a0), 0, std::sin(a0))), n1 = normalize(V3(std::cos(a1), 0, std::sin(a1)));
            b.vert(p00, n0, f0); b.vert(p01, n1, f0); b.vert(p11, n1, f0);
            b.vert(p00, n0, f0); b.vert(p11, n1, f0); b.vert(p10, n0, f0);
        }
    }
}

void buildPond(std::vector<Vtx>& out, const Pond& pd, float level) {
    B b{&out};
    b.mat = 12.f;
    const float R = pd.r * 1.1f;
    const int NR = 10, NA = 64;
    const vec3 n(0, 1, 0);
    const vec4 f0(0, 0, 0, 0);
    for (int i = 0; i < NR; i++)
        for (int k = 0; k < NA; k++) {
            const float r0 = R * i / NR, r1 = R * (i + 1) / NR;
            const float a0 = TAU_ * k / NA, a1 = TAU_ * (k + 1) / NA;
            auto P = [&](float r, float a) { return V3(pd.x + std::cos(a) * r, level, pd.z + std::sin(a) * r); };
            const vec3 p00 = P(r0, a0), p01 = P(r0, a1), p11 = P(r1, a1), p10 = P(r1, a0);
            for (const vec3* p : {&p00, &p11, &p01, &p00, &p10, &p11}) b.vert(*p, n, f0);
        }
}

// ---------------------------------------------------------------- the plants
namespace {

// petals: 5 around n (the flower's facing) at c, radius pr (the old petals())
void petals(B& b, const vec3& c, const vec3& n, float pr, bool big) {
    const vec3 t1 = normalize(cross(n, V3(0, 0, 1)));
    const vec3 t2 = cross(n, t1);
    const float cup = 0.35f / std::max(pr, 1e-3f) * (big ? 1.6f : 1.f);
    const float wy = pr * (big ? 0.42f : 0.33f);
    const float m0 = b.mat;
    b.mat = 9.f;
    b.two = 1.f;
    for (int k = 0; k < 5; k++) {
        const float ak = k * TAU_ / 5.f;
        // a petal along +x in its own plane, turned to its place
        auto pos = [&](float u, float v) {
            const float rho = u, ph = v * TAU_;
            const float px = pr * 0.55f + pr * 0.55f * rho * std::cos(ph), py = wy * rho * std::sin(ph);
            const float lx = std::cos(ak) * px - std::sin(ak) * py, ly = std::sin(ak) * px + std::cos(ak) * py;
            const float r2 = lx * lx + ly * ly;
            return c + t1 * lx + t2 * ly + n * (cup * r2);
        };
        auto inf = [&](const vec3& p, float u, float v) {
            const float rho = u, ph = v * TAU_;
            const float px = pr * 0.55f + pr * 0.55f * rho * std::cos(ph), py = wy * rho * std::sin(ph);
            (void)p;
            return vec4(std::sqrt(px * px + py * py) / pr, py / pr, 0, 0);
        };
        surf(b, 4, 14, pos, inf, nullptr);
    }
    b.two = 0.f;
    b.mat = 10.f;
    sphere(b, c + n * 0.004f, pr * 0.24f, 10, 6, vec4(0, 0, 0, 0));
    b.mat = m0;
}

void frond(B& b, const vec3& B0, float phi, float L, float g, float th0h) {
    const float unf = sstep(0.25f, 1.f, g);
    const float S = L * (0.3f + 0.7f * sstep(0.f, 0.6f, g));
    const float Rf = L * 1.25f;
    const float th0 = 0.2f + 0.75f * th0h;
    const float Rc = 0.011f * L / 0.35f * (1.f + 0.5f * (1.f - unf));
    const float sc = std::min(S * (1.f - unf), Rc * 5.4f);
    const float s1 = S - sc;
    const float psi = sc / Rc;
    const float c1x = Rf * std::cos(th0), c1y = -Rf * std::sin(th0);
    const float a0 = PI_ - th0;
    const vec3 U(std::cos(phi), 0, std::sin(phi)), W(-std::sin(phi), 0, std::cos(phi)), Y(0, 1, 0);
    auto world = [&](float u, float v, float w) { return B0 + U * u + Y * v + W * w; };
    auto arcP = [&](float t, float& px, float& py, float& nx, float& ny) {
        const float ang = a0 - t / Rf;
        nx = std::cos(ang); ny = std::sin(ang);
        px = c1x + Rf * nx; py = c1y + Rf * ny;
    };
    // the rachis
    b.mat = 4.f;
    if (s1 > 1e-4f) {
        std::vector<vec3> pts;
        std::vector<float> rad;
        std::vector<vec4> inf;
        const int n = 12;
        for (int i = 0; i <= n; i++) {
            const float t = s1 * i / n;
            float px, py, nx, ny;
            arcP(t, px, py, nx, ny);
            pts.push_back(world(px, py, 0.f));
            rad.push_back(0.0045f * (1.f - 0.6f * t / std::max(L, 1e-3f)));
            inf.push_back(vec4(t / L, 0, 0, 0));
        }
        tube(b, pts, rad, inf, 5, false);
    }
    // the pinnae: along the open part, longest a third of the way up, young ones small near the curl
    const float dl = 0.034f * L / 0.35f;
    b.two = 1.f;
    for (int k = 1; k * dl <= s1 + dl * 0.5f; k++) {
        const float tk = k * dl;
        const float x = tk / L;
        const float xs = std::clamp(x - 0.03f, 0.f, 1.f);
        float Lp = L * 0.34f * std::sqrt(sat(4.f * xs * (1.f - xs))) * (1.f - 0.3f * x);
        Lp *= sstep(0.f, 0.18f * L, s1 - tk) * (tk >= 0.04f * L ? 1.f : 0.f);
        if (Lp <= 0.004f) continue;
        for (float side : {-1.f, 1.f}) {
            auto pos = [&](float u, float v) {
                const float aw = u * Lp;
                const float lob = fr(aw / Lp * 3.f) - 0.5f;
                const float hw = dl * 0.36f * (0.75f + 0.5f * lob * lob);
                const float e = 2.f * aw / Lp - 1.f;
                const float ha = hw * std::sqrt(std::max(0.f, 1.f - e * e));
                const float a2 = (v * 2.f - 1.f) * ha;
                const float t = tk + 0.4f * aw + a2;
                float px, py, nx, ny;
                arcP(t, px, py, nx, ny);
                const float rad = 0.02f * aw - 0.5f * aw * aw / L;
                return world(px + nx * rad, py + ny * rad, side * aw);
            };
            auto inf = [&](const vec3&, float u, float) { return vec4(std::min(tk, s1) / L, u, 0, 0); };
            surf(b, 12, 1, pos, inf, nullptr);
        }
    }
    b.two = 0.f;
    // the fiddlehead: the rest, still curled
    if (psi > 0.05f) {
        const float a1 = a0 - s1 / Rf;
        const float P1x = c1x + Rf * std::cos(a1), P1y = c1y + Rf * std::sin(a1);
        const float c2x = P1x + (c1x - P1x) * (Rc / Rf), c2y = P1y + (c1y - P1y) * (Rc / Rf);
        const float rt = 0.0075f * L / 0.35f;
        std::vector<vec3> pts;
        std::vector<float> rad;
        std::vector<vec4> inf;
        const int n = 14;
        for (int i = 0; i <= n; i++) {
            const float th = psi * i / n;
            pts.push_back(world(c2x + Rc * std::cos(a1 - th), c2y + Rc * std::sin(a1 - th), 0.f));
            rad.push_back(rt * (1.f - 0.4f * th / std::max(psi, 1e-3f)));
            inf.push_back(vec4(1, 0, 1, 0));
        }
        tube(b, pts, rad, inf, 6, true);
    }
}

void fern(B& b, const SlotDraw& s) {
    const int N = 7;
    for (int ii = 0; ii < N; ii++) {
        const float h = ghash1(s.seed * 31.f + ii);
        const float phi = s.rot + (ii + 0.5f + 0.3f * (h - 0.5f)) * (TAU_ / N);
        const float L = s.size * (0.72f + 0.4f * ghash1(s.seed * 17.f + ii * 3.1f));
        const float gi = sat((s.g - 0.5f * ii / N) / 0.5f);
        if (gi <= 0.001f) continue;
        b.sw[0] = s.base.x; b.sw[1] = s.base.z; b.sw[2] = ii * 1.7f + s.seed * 5.f; b.sw[3] = 0.04f;
        frond(b, s.base, phi, L, gi, ghash1(s.seed * 13.f + ii * 5.3f));
    }
    b.noSway();
}

void mushrooms(B& b, const SlotDraw& s) {
    for (int j = 0; j < 3; j++) {
        const float fj = (float)j;
        const vec2 hh = hash2(s.seed * 7.f + fj, fj * 3.7f + 1.f);
        const float gj = sat((s.g - fj * 0.22f) / 0.56f);
        if (gj <= 0.001f) continue;
        const float k = j == 0 ? 0.25f : 0.9f;
        const float offx = (hh.x - 0.5f) * s.size * k, offz = (hh.y - 0.5f) * s.size * k;
        const float H = s.size * (0.55f + 0.45f * hh.y) * (j == 0 ? 1.15f : 0.8f) * sstep(0.f, 0.6f, gj);
        const float Rc = s.size * (0.32f + 0.16f * hh.x) * (j == 0 ? 1.1f : 0.85f) * sstep(0.2f, 1.f, gj);
        const vec3 O = s.base + V3(offx, -0.01f, offz);
        const float lx = (hh.y - 0.5f) * 0.5f, lz = (hh.x - 0.5f) * 0.5f;
        auto axis = [&](float y) { const float c = y * y / (s.size * 0.8f); return O + V3(lx * c, y, lz * c); };
        // the stem, leaning
        b.mat = 7.f;
        {
            std::vector<vec3> pts;
            std::vector<float> rad;
            std::vector<vec4> inf;
            for (int i = 0; i <= 6; i++) {
                const float t = (float)i / 6;
                pts.push_back(axis(H * t));
                rad.push_back(lerpf(0.16f * Rc + 0.006f, 0.11f * Rc + 0.004f, t));
                inf.push_back(vec4(t, 0, 0, 0));
            }
            if (H > 1e-4f) tube(b, pts, rad, inf, 8, true);
        }
        if (Rc <= 0.003f) continue;
        // the cap: an ellipsoid above -0.08 Rc, its gills beneath
        b.mat = 6.f;
        const vec3 C = axis(H);
        const float ry = 0.62f * Rc, cut = -0.08f * Rc;
        const float thMax = std::acos(std::clamp(cut / ry, -1.f, 1.f));
        const vec4 w(0, 0, 0, fj + s.seed);
        ellipsoid(b, C, V3(Rc, ry, Rc), V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), 20, 8, [](const vec3&) { return 0.f; },
                  [&](const vec3& q) { return vec4(q.x / Rc, q.y / Rc, q.z / Rc, w.w); }, thMax);
        const float rr = Rc * std::sin(thMax);
        const int S = 20;
        for (int i = 0; i < S; i++) {
            const float a0 = TAU_ * i / S, a1 = TAU_ * (i + 1) / S;
            const vec3 c0 = C + V3(0, cut, 0);
            const vec3 p0 = c0 + V3(std::cos(a0) * rr, 0, std::sin(a0) * rr), p1 = c0 + V3(std::cos(a1) * rr, 0, std::sin(a1) * rr);
            auto f = [&](const vec3& p) { const vec3 q = p - C; return vec4(q.x / Rc, q.y / Rc, q.z / Rc, w.w); };
            b.vert(c0, V3(0, -1, 0), f(c0)); b.vert(p1, V3(0, -1, 0), f(p1)); b.vert(p0, V3(0, -1, 0), f(p0));
        }
    }
}

void flowers(B& b, const SlotDraw& s) {
    const bool big = s.param > 0.5f;
    const int n = big ? 3 : 5;
    for (int j = 0; j < n; j++) {
        const float fj = (float)j;
        const vec2 hh = hash2(s.seed * 5.f + fj, fj * 1.3f + 7.f);
        const float gj = sat((s.g - fj * 0.1f) / 0.6f);
        if (gj <= 0.001f) continue;
        const vec3 O = s.base + V3((hh.x - 0.5f) * s.size * 0.9f, -0.005f, (hh.y - 0.5f) * s.size * 0.9f);
        const float H = s.size * (0.55f + 0.45f * hh.y) * (big ? 1.4f : 1.f) * sstep(0.f, 0.55f, gj);
        const float lx = (hh.y - 0.5f) * 0.6f, lz = (hh.x - 0.5f) * 0.6f + 0.15f;
        const vec3 top = O + V3(lx * H * 0.5f, H, lz * H * 0.5f), mid = O + V3(lx * H * 0.1f, H * 0.55f, lz * H * 0.1f);
        b.mat = 5.f;
        if (H > 1e-4f) {
            tube(b, {O, mid, top}, {0.0032f, 0.003f, 0.0028f}, {vec4(0, 0, 0, 0), vec4(0, 0, 0, 0), vec4(0, 0, 0, 0)}, 6, true);
        }
        // two leaves at the foot
        const float lk = sstep(0.f, 0.4f, gj);
        if (lk > 0.01f) {
            const float ra = hh.x * 6.f;
            for (float sd : {-1.f, 1.f}) {
                auto pos = [&](float u, float v) {
                    const float th = v * PI_, ph = u * TAU_;
                    const float ex = std::sin(th) * std::cos(ph), ey = std::cos(th), ez = std::sin(th) * std::sin(ph);
                    const float ax = s.size * 0.12f + ex * s.size * 0.13f * lk;
                    const float y = ey * 0.004f * lk + ax * 0.35f, z = ez * s.size * 0.05f * lk;
                    return O + V3(0, 0.012f, 0) + unrot(ra, V3(sd * ax, y, z));
                };
                const vec3 cc = O + V3(0, 0.012f, 0) + unrot(ra, V3(sd * s.size * 0.12f, s.size * 0.042f, 0));
                surf(b, 12, 6, pos, [](const vec3&, float, float) { return vec4(0, 0, 0, 0); }, &cc);
            }
        }
        const float pr = s.size * (big ? 0.3f : 0.17f) * sstep(0.5f, 1.f, gj);
        if (pr > 0.003f) petals(b, top, normalize(V3(lx * 0.8f, 1.f, 0.7f + lz)), pr, big);
    }
}

void sapling(B& b, const SlotDraw& s) {
    const float gg = sstep(0.f, 1.f, s.g);
    const float ra = s.seed * 6.f;
    auto W = [&](const vec3& q) { return s.base + unrot(ra, q); };
    const float sz = s.size;
    const vec3 P1 = V3(0.05f, 0.42f, 0.02f) * (sz * gg), P2 = V3(-0.06f, 0.85f, -0.03f) * (sz * gg), P3 = V3(0.32f, 0.7f, 0.05f) * (sz * gg);
    const vec3 P4 = V3(-0.25f, 1.0f, 0.04f) * (sz * gg);
    const float r0 = 0.028f * sz * (0.4f + 0.6f * gg);
    b.mat = 8.f;
    auto branch = [&](const vec3& a, const vec3& c, float rA, float rB) {
        std::vector<vec3> pts;
        std::vector<float> rad;
        std::vector<vec4> inf;
        for (int i = 0; i <= 4; i++) {
            const float t = i / 4.f;
            const vec3 q = lerp(a, c, t);
            pts.push_back(W(q));
            rad.push_back(lerpf(rA, rB, t));
            inf.push_back(vec4(q.y / std::max(sz, 1e-3f), 0, 0, 0));
        }
        tube(b, pts, rad, inf, 8, true);
    };
    if (gg > 1e-3f) {
        branch(V3(0, -0.02f, 0), P1, r0, r0 * 0.75f);
        branch(P1, P2, r0 * 0.75f, r0 * 0.45f);
        branch(P1 + (P2 - P1) * 0.2f, P3, r0 * 0.5f, r0 * 0.3f);
        branch(P2, P4, r0 * 0.4f, r0 * 0.25f);
    }
    if (s.param > 0.5f) {
        const float gl = sstep(0.35f, 1.f, s.g);
        b.mat = 3.5f;
        for (int j = 0; j < 3; j++) {
            const vec3 c = j == 0 ? P2 : (j == 1 ? P3 : P4);
            const float R = sz * (j == 0 ? 0.24f : 0.18f) * gl;
            if (R < 0.003f) continue;
            const vec3 cl = c + V3(0, R * 0.35f, 0);
            const float sd = s.seed * 9.f + j;
            // (in the sapling's frame: the noise and the parameters as the old local qc)
            auto pos = [&](float u, float v) {
                const float th = v * PI_, ph = u * TAU_;
                const vec3 d(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
                vec3 q(d.x * R, d.y * R * 0.7f, d.z * R);
                const float disp = R * 0.12f * (gnoise3(q / R * 3.f + vec3(sd)) - 0.5f);
                q = q - normalize(V3(q.x, q.y / 0.49f, q.z)) * disp;
                return W(cl + q);
            };
            const vec3 cw = W(cl);
            auto inf = [&](const vec3& p, float, float) {
                const vec3 qw = p - cw;
                const vec3 q = V3(rotF(ra, qw.x, qw.z).x, qw.y, rotF(ra, qw.x, qw.z).y);
                return vec4(q.x / R, q.y / R, q.z / R, 1.f);
            };
            surf(b, 18, 10, pos, inf, &cw);
        }
    }
}

void rosette(B& b, const SlotDraw& s) {
    const float style = s.param;
    const int rings = style > 1.5f ? 1 : (style > 0.5f ? 2 : 3);
    b.mat = 13.f;
    for (int k = 0; k < rings; k++) {
        const float fk = (float)k;
        int n;
        float el, len, wid, th, r0;
        if (style < 0.5f) {
            n = k == 0 ? 10 : (k == 1 ? 8 : 5); el = k == 0 ? 0.32f : (k == 1 ? 0.75f : 1.15f);
            len = s.size * (k == 0 ? 0.95f : (k == 1 ? 0.7f : 0.42f)); wid = len * 0.5f; th = len * 0.2f; r0 = s.size * 0.05f;
        } else if (style < 1.5f) {
            n = k == 0 ? 9 : 6; el = k == 0 ? 0.62f : 1.05f; len = s.size * (k == 0 ? 1.45f : 1.05f); wid = len * 0.17f; th = len * 0.09f;
            r0 = s.size * 0.04f;
        } else { n = 6; el = 0.95f; len = s.size * 1.5f; wid = len * 0.42f; th = 0.006f; r0 = s.size * 0.05f; }
        float gk = sat((s.g - (2.f - fk) * 0.12f) / 0.7f);
        if (style > 1.5f) gk = s.g;
        if (gk <= 0.001f) continue;
        len *= 0.25f + 0.75f * sstep(0.f, 1.f, gk);
        wid *= 0.4f + 0.6f * sstep(0.f, 1.f, gk);
        th *= 0.5f + 0.5f * gk;
        const float stepA = TAU_ / n;
        const float off = s.rot + fk * stepA * 0.5f;
        for (int ii = 0; ii < n; ii++) {
            const float a = off + (ii + 0.5f) * stepA + 0.15f * (ghash1(s.seed + ii * 7.f + fk) - 0.5f);
            const float e = el + 0.12f * (ghash1(s.seed * 3.f + ii + fk * 11.f) - 0.5f);
            const vec3 dir(std::cos(a), 0, std::sin(a));
            const vec3 A = dir * std::cos(e) + V3(0, std::sin(e), 0);
            const vec3 Wv(-std::sin(a), 0, std::cos(a));
            const vec3 Nn = cross(A, Wv);
            const vec3 O = s.base + dir * r0;
            const float kc = style > 1.5f ? -1.2f : 0.45f;
            auto P = [&](float x, float y, float z) { return O + A * x + Wv * y + Nn * (z - kc * x * x / std::max(len, 1e-3f)); };
            auto inf = [&](float x, float y) { return vec4(sat(x / std::max(len, 1e-3f)), y / std::max(wid, 1e-3f), fk, style); };
            if (style > 1.5f) { b.sw[0] = s.base.x; b.sw[1] = s.base.z; b.sw[2] = (float)ii; b.sw[3] = 0.05f; }
            if (style < 0.5f) {
                // an echeveria's fleshy leaf: an ellipsoid, narrowing toward its tip
                auto pos = [&](float u, float v) {
                    const float tt = v * PI_, ph = u * TAU_;
                    const float x = len * 0.5f - std::cos(tt) * len * 0.5f;
                    const float wy = wid * 0.5f * (1.f - 0.3f * sat(x / len));
                    return P(x, std::sin(tt) * std::cos(ph) * wy, std::sin(tt) * std::sin(ph) * th * 0.5f);
                };
                const vec3 cc = P(len * 0.5f, 0, 0);
                surf(b, 12, 10, pos, [&](const vec3&, float u, float v) {
                    const float tt = v * PI_, ph = u * TAU_;
                    const float x = len * 0.5f - std::cos(tt) * len * 0.5f;
                    return inf(x, std::sin(tt) * std::cos(ph) * wid * 0.5f);
                }, &cc);
            } else if (style < 1.5f) {
                // an aloe's: a flattened tapering spike
                auto pos = [&](float u, float v) {
                    const float ph = u * TAU_;
                    const float x = v * len;
                    float rr = lerpf(wid * 0.5f, 0.002f, v);
                    float xx = x;
                    if (v > 0.95f) { const float t = (v - 0.95f) / 0.05f; xx = len + 0.002f * t; rr = 0.002f * std::sqrt(std::max(0.f, 1.f - t * t)); }
                    return P(xx, std::cos(ph) * rr, std::sin(ph) * rr * th / wid);
                };
                const vec3 cc = P(len * 0.3f, 0, 0);
                surf(b, 10, 10, pos, [&](const vec3&, float u, float v) {
                    return inf(v * len, std::cos(u * TAU_) * lerpf(wid * 0.5f, 0.002f, v));
                }, &cc);
            } else {
                // a broad leaf on a short stalk, folded along its midrib
                const float stem = len * 0.3f;
                b.mat = 5.f;
                capsule(b, P(0, 0, 0), P(stem, 0, 0), 0.004f, 0.004f, vec4(0, 0, 0, 0), 5, 2);
                b.mat = 13.f;
                b.two = 1.f;
                auto pos = [&](float u, float v) {
                    const float x = stem + u * (len - stem);
                    const float e2 = (u - 0.5f) * 2.f;
                    const float hwid = wid * 0.5f * std::sqrt(std::max(0.f, 1.f - e2 * e2));
                    const float y = (v * 2.f - 1.f) * hwid;
                    return P(x, y, -0.06f * std::fabs(y));
                };
                surf(b, 14, 4, pos, [&](const vec3&, float u, float v) {
                    const float x = stem + u * (len - stem);
                    const float e2 = (u - 0.5f) * 2.f;
                    return inf(x, (v * 2.f - 1.f) * wid * 0.5f * std::sqrt(std::max(0.f, 1.f - e2 * e2)));
                }, nullptr);
                b.two = 0.f;
            }
        }
        b.noSway();
    }
}

void cactus(B& b, const SlotDraw& s) {
    const float gg = sstep(0.f, 1.f, s.g);
    const float rc = s.size * 0.5f * (0.35f + 0.65f * gg);
    const float Hc = s.size * (0.5f + 0.9f * gg) * (0.8f + 0.4f * ghash1(s.seed));
    const float y0 = rc * 0.7f, y1 = std::max(Hc - rc, rc * 0.7f);
    b.mat = 14.f;
    // a capsule with eleven ribs
    auto pos = [&](float u, float v) {
        const float ang = u * TAU_ - PI_;
        const float rib = 0.5f + 0.5f * std::cos(11.f * ang + s.seed * 3.f);
        const float rr = rc * (1.f - 0.07f * rib);
        // v: 0 the bottom pole .. 1 the top pole (the half spheres and the side between them)
        const float L = PI_ * 0.5f * rc * 2.f + (y1 - y0);
        const float d = v * L;
        float r, y;
        const float q = PI_ * 0.5f * rc;
        if (d < q) { const float t = d / rc; r = std::sin(t); y = y0 - std::cos(t) * rc; }
        else if (d < q + (y1 - y0)) { r = 1.f; y = y0 + (d - q); }
        else { const float t = (d - q - (y1 - y0)) / rc; r = std::cos(t); y = y1 + std::sin(t) * rc; }
        return s.base + V3(std::cos(ang) * rr * r, y, std::sin(ang) * rr * r);
    };
    auto inf = [&](const vec3& p, float u, float) {
        const float ang = u * TAU_ - PI_;
        const float rib = 0.5f + 0.5f * std::cos(11.f * ang + s.seed * 3.f);
        return vec4(rib, (p.y - s.base.y) / std::max(Hc, 1e-3f), ang, 0);
    };
    const vec3 cc = s.base + V3(0, (y0 + y1) * 0.5f, 0);
    surf(b, 66, 16, pos, inf, &cc);
    const float pr = s.size * 0.32f * sstep(0.7f, 1.f, s.g);
    if (pr > 0.003f) petals(b, s.base + V3(0, y1 + rc * 0.95f, 0), normalize(V3(0, 1, 0.25f)), pr, false);
}

void moss(B& b, const SlotDraw& s) {
    const float r = std::max(s.size * s.g, 0.004f);
    const vec3 c = s.base - V3(0, 0.25f * r, 0);
    b.mat = 3.f;
    ellipsoid(b, c, V3(r, 0.55f * r, r * 0.9f), V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), 28, 14,
              [&](const vec3& q) { return r * 0.16f * (gnoise3(q / r * 3.5f + vec3(s.seed * 10.f)) - 0.5f); },
              [&](const vec3& q) { return vec4(q.x / r, q.y / r, q.z / r, s.seed); }, 1.95f);
}

void pebble(B& b, const SlotDraw& s) {
    const float r = s.size * (0.3f + 0.7f * sstep(0.f, 1.f, s.g));
    b.mat = 11.f;
    const vec3 ex = unrot(s.rot, V3(1, 0, 0)), ez = unrot(s.rot, V3(0, 0, 1));
    const vec3 c = s.base + V3(0, 0.1f * r, 0);
    ellipsoid(b, c, V3(r, 0.62f * r, 0.8f * r), ex, V3(0, 1, 0), ez, 24, 12,
              [&](const vec3& q) { return -0.006f * (gnoise3((q + V3(0, 0.1f * r, 0)) * 30.f) - 0.5f); },
              [&](const vec3& q) { return vec4(q.x / r, (q.y + 0.1f * r) / r, q.z / r, s.seed); });
}

void rock(B& b, const SlotDraw& s, const PondState& ps) {
    const float r = s.size * (0.35f + 0.65f * sstep(0.f, 1.f, s.g));
    b.mat = 21.f;
    const vec3 ex = unrot(s.rot, V3(1, 0, 0)), ez = unrot(s.rot, V3(0, 0, 1));
    const vec3 c = s.base + V3(0, 0.12f * r, 0);
    ellipsoid(b, c, V3(r, 0.78f * r, 0.85f * r), ex, V3(0, 1, 0), ez, 28, 14,
              [&](const vec3& q) { const vec3 qq = q + V3(0, 0.12f * r, 0); return r * 0.12f * (gnoise3(qq / r * 3.f + vec3(s.seed)) - 0.5f); },
              [&](const vec3& q) { return vec4(q.x / r, (q.y + 0.12f * r) / r, q.z / r, s.seed); });
    if (s.param > 0.5f && ps.fill > 0.3f && s.g > 0.5f) {
        // the trickle from its lip into the pond
        float dx = ps.pd.x - s.base.x, dz = ps.pd.z - s.base.z;
        const float dl = std::sqrt(dx * dx + dz * dz) + 1e-6f;
        dx /= dl; dz /= dl;
        const vec3 a = s.base + V3(dx * r * 0.62f, r * 0.72f, dz * r * 0.62f);
        const vec3 e = V3(s.base.x + dx * (r * 0.95f + 0.035f), ps.level, s.base.z + dz * (r * 0.95f + 0.035f));
        const vec3 ba = e - a;
        const vec3 side = normalize(cross(ba, V3(0, 1, 0)));
        const float w = 0.0125f * sstep(0.5f, 0.9f, s.g);
        b.mat = 22.f;
        b.two = 1.f;
        auto pos = [&](float u, float v) {
            // a little bulge out over the lip, then straight down
            const vec3 p = lerp(a, e, u) + V3(dx, 0, dz) * (0.012f * std::sin(u * PI_) * (1.f - u));
            return p + side * ((v * 2.f - 1.f) * w);
        };
        surf(b, 10, 1, pos, [&](const vec3&, float u, float v) { return vec4(u, (v * 2.f - 1.f) * w, 0, 0); }, nullptr);
        b.two = 0.f;
    }
}

} // namespace

void buildSlot(std::vector<Vtx>& out, const SlotDraw& s, const PondState& ps) {
    if (s.g <= 0.001f) return;
    B b{&out};
    b.seed = s.seed;
    switch (s.type) {
    case 0: moss(b, s); break;
    case 1: fern(b, s); break;
    case 2: mushrooms(b, s); break;
    case 3: sapling(b, s); break;
    case 4: flowers(b, s); break;
    case 5: rosette(b, s); break;
    case 6: cactus(b, s); break;
    case 8: rock(b, s, ps); break;
    default: pebble(b, s); break;
    }
}

void buildSprout(std::vector<Vtx>& out, const vec3& at, float g, int k, int kind) {
    if (g <= 0.001f) return;
    B b{&out};
    const float sd = ghash1((float)k * 7.31f + std::floor(at.x * 97.f));
    const float ra = sd * 6.28f;
    auto W = [&](const vec3& q) { return at + unrot(ra, q); };
    const float gg = sstep(0.f, 1.f, g);
    if (kind == 1) {
        // a little round succulent pup
        const float R = 0.02f * gg * (0.8f + 0.5f * sd);
        b.mat = 14.f;
        const vec3 c = W(V3(0, R * 0.7f, 0));
        auto pos = [&](float u, float v) {
            const float th = v * PI_, ph = u * TAU_ - PI_;
            const float rr = R * (1.f - 0.08f * (0.5f + 0.5f * std::cos(ph * 8.f)));
            return W(V3(0, R * 0.7f, 0) + V3(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph)) * rr);
        };
        surf(b, 32, 8, pos, [&](const vec3&, float u, float) {
            const float ph = u * TAU_ - PI_;
            return vec4(0.5f + 0.5f * std::cos(ph * 8.f), 0.5f, ph, 0);
        }, &c);
        return;
    }
    const int ty = sd < 0.45f ? 0 : (sd < 0.75f ? 1 : 2);
    const float H = 0.06f * gg * (0.75f + 0.5f * fr(sd * 13.f));
    if (ty == 1) {
        // a tuft of grass
        b.mat = 18.f;
        for (int bl = 0; bl < 3; bl++) {
            const float ab = bl * 2.1f + sd * 3.f;
            const vec3 tip = V3(std::cos(ab) * 0.022f, H * (1.1f + 0.2f * bl), std::sin(ab) * 0.022f) * (0.4f + 0.6f * gg);
            std::vector<vec3> pts;
            std::vector<float> rad;
            std::vector<vec4> inf;
            for (int i = 0; i <= 4; i++) {
                const float t = i / 4.f;
                pts.push_back(W(tip * t));
                rad.push_back(lerpf(0.0035f, 0.0008f, t));
                inf.push_back(vec4(tip.y * t / std::max(H, 1e-3f), 0, 0, 0));
            }
            tube(b, pts, rad, inf, 5, true);
        }
        return;
    }
    const vec3 top = V3(0.004f, H, 0);
    b.mat = 5.f;
    capsule(b, W(V3(0, 0, 0)), W(top), 0.0024f, 0.0024f, vec4(0, 0, 0, 0), 5, 2);
    const float L = 0.016f * sstep(0.2f, 1.f, g);
    if (L < 0.002f) return;
    b.mat = 18.f;
    const vec4 f1(1, 0, 0, 0);
    auto leaf = [&](float ang, float cx, float rx, float ry, float rz, float lift) {
        // a flat leaf along +x (turned by ang about the stem's top), its middle at cx, rising along its length
        auto pos = [&](float u, float v) {
            const float th = v * PI_, ph = u * TAU_;
            const float lx = cx + std::sin(th) * std::cos(ph) * rx;
            const float ly = std::cos(th) * ry + std::fabs(lx) * lift;
            const float lz = std::sin(th) * std::sin(ph) * rz;
            const vec2 r = rotI(ang, lx, lz);
            return W(top + V3(r.x, ly, r.y));
        };
        const vec2 rc = rotI(ang, cx, 0.f);
        const vec3 cc = W(top + V3(rc.x, std::fabs(cx) * lift - 0.01f, rc.y));
        surf(b, 12, 6, pos, [&](const vec3&, float, float) { return f1; }, &cc);
    };
    if (ty == 0) {
        leaf(0.f, L * 0.9f, L, 0.0028f, L * 0.62f, 0.3f);
        leaf(PI_, L * 0.9f, L, 0.0028f, L * 0.62f, 0.3f);
    } else {
        for (int c = 0; c < 3; c++) leaf(c * TAU_ / 3.f, L * 0.75f, L * 0.8f, 0.0026f, L * 0.7f, 0.25f);
    }
}

void buildBeetle(std::vector<Vtx>& out, const vec3& at, float head, float walk, float mv, float look, float time) {
    B b{&out};
    const float bob = 0.0012f * mv * std::fabs(std::sin(walk * 2.f));
    // q (the beetle's own frame, forward +z, units of 1/BS) -> world
    auto W = [&](const vec3& q) { const vec3 s = V3(q.x, q.y + bob, q.z) * BS; return at + unrot(-head, s); };
    auto ell = [&](const vec3& c, const vec3& r, int nu, int nv, float thMax, std::function<vec4(const vec3&)> inf) {
        auto pos = [&](float u, float v) {
            const float th = v * thMax, ph = u * TAU_;
            const vec3 q(std::sin(th) * std::cos(ph) * r.x, std::cos(th) * r.y, std::sin(th) * std::sin(ph) * r.z);
            return W(c + q);
        };
        const vec3 cw = W(c);
        surf(b, nu, nv, pos, [&](const vec3&, float u, float v) {
            const float th = v * thMax, ph = u * TAU_;
            return inf(V3(std::sin(th) * std::cos(ph) * r.x, std::cos(th) * r.y, std::sin(th) * std::sin(ph) * r.z));
        }, &cw);
    };
    // the shell (cut flat below), its underside
    b.mat = 15.f;
    {
        const vec3 c(0, 0.028f, -0.006f);
        const float thMax = std::acos(std::clamp((0.012f - 0.028f) / 0.031f, -1.f, 1.f));
        ell(c, V3(0.041f, 0.031f, 0.05f), 32, 14, thMax, [](const vec3& q) { return vec4(q.x, q.y, q.z, 0); });
    }
    b.mat = 16.f;
    ell(V3(0, 0.017f, 0), V3(0.034f, 0.013f, 0.046f), 20, 10, PI_, [](const vec3&) { return vec4(0, 0, 0, 0); });
    // the head, turned to look around
    const vec3 hp0(0, 0.021f, 0.04f);
    auto H = [&](const vec3& hq) {
        const vec3 hp = hq + V3(0, 0.004f, 0.019f);
        return hp0 + unrot(-look, hp);
    };
    b.mat = 17.f;
    {
        const vec3 r(0.031f, 0.027f, 0.025f);
        auto pos = [&](float u, float v) {
            const float th = v * PI_, ph = u * TAU_;
            return W(H(V3(std::sin(th) * std::cos(ph) * r.x, std::cos(th) * r.y, std::sin(th) * std::sin(ph) * r.z)));
        };
        const vec3 cw = W(H(V3(0, 0, 0)));
        surf(b, 28, 16, pos, [&](const vec3&, float u, float v) {
            const float th = v * PI_, ph = u * TAU_;
            return vec4(std::sin(th) * std::cos(ph) * r.x, std::cos(th) * r.y, std::sin(th) * std::sin(ph) * r.z, 0);
        }, &cw);
    }
    // antennae
    b.mat = 16.f;
    for (int s = -1; s <= 1; s += 2) {
        const float fs = (float)s;
        const float wig = 0.004f * std::sin(time * 2.3f + fs) + 0.003f * std::sin(time * 5.1f + fs * 2.f);
        const vec3 a0(fs * 0.01f, 0.02f, 0.008f), a1(fs * 0.024f + wig, 0.042f, 0.022f);
        capsule(b, W(H(a0)), W(H(a1)), 0.0026f * BS, 0.0026f * BS, vec4(0, 0, 0, 0), 5, 2);
        sphere(b, W(H(a1)), 0.0062f * BS, 8, 5, vec4(0, 0, 0, 0));
    }
    // six legs, a tripod gait
    for (int k = 0; k < 3; k++) {
        const float zk = 0.02f - 0.02f * k;
        const float fz = k == 0 ? 0.046f : (k == 1 ? 0.f : -0.044f);
        for (int s = -1; s <= 1; s += 2) {
            const float fs = (float)s;
            const float c = fr(walk / TAU_ + (((k == 1) == (s > 0)) ? 0.f : 0.5f));
            const float A = 0.009f * mv;
            float off, lift;
            if (c < 0.5f) { off = A - 4.f * A * c; lift = 0.f; }
            else { const float u = (c - 0.5f) * 2.f; off = -A + 2.f * A * sstep(0.f, 1.f, u); lift = std::sin(PI_ * u) * 0.007f * mv; }
            const vec3 hip(fs * 0.026f, 0.015f, zk);
            const vec3 foot(fs * (k == 1 ? 0.066f : 0.06f), 0.0015f + lift, fz + off);
            const vec3 knee(fs * 0.052f, 0.028f + lift * 0.6f, lerpf(zk, foot.z, 0.55f));
            tube(b, {W(hip), W(knee), W(foot)}, {0.005f * BS, 0.0046f * BS, 0.0042f * BS},
                 {vec4(0, 0, 0, 0), vec4(0, 0, 0, 0), vec4(0, 0, 0, 0)}, 6, true);
        }
    }
}

void buildSnail(std::vector<Vtx>& out, float time, const Pond& pd) {
    B b{&out};
    const float a = time * 0.035f;
    const float cx = 0.05f + std::cos(a) * 0.5f, cz = 0.12f + std::sin(a) * 0.36f;
    const float tx = -std::sin(a) * 0.5f, tz = std::cos(a) * 0.36f;
    const vec3 sp(cx, soilH(cx, cz, pd), cz);
    const float w = std::atan2(-tx, tz);
    auto W = [&](const vec3& q) { return sp + unrot(-w, q); };
    const float crawl = std::sin(time * 1.3f) * 0.004f;
    b.mat = 23.f;
    {
        std::vector<vec3> pts;
        std::vector<float> rad;
        std::vector<vec4> inf;
        for (int i = 0; i <= 6; i++) {
            const float t = i / 6.f;
            const vec3 q = lerp(V3(0, 0.012f, -0.07f), V3(0, 0.018f, 0.06f + crawl), t);
            pts.push_back(W(q));
            rad.push_back(lerpf(0.012f, 0.02f, t));
            inf.push_back(vec4(q.x, q.y, q.z, 0));
        }
        tube(b, pts, rad, inf, 12, true);
        const vec3 hq(0, 0.03f, 0.065f + crawl);
        const vec3 hc = W(hq);
        ellipsoidS(b, hc, vec3(0.019f), 16, 10, [&](const vec3& q) { return vec4(q.x, q.y + 0.03f, q.z, 0); });
    }
    for (int s = -1; s <= 1; s += 2) {
        const float fs = (float)s;
        const vec3 a0(fs * 0.008f, 0.04f, 0.072f + crawl), b0(fs * 0.02f, 0.075f + 0.004f * std::sin(time * 1.7f + fs), 0.085f + crawl);
        b.mat = 23.f;
        capsule(b, W(a0), W(b0), 0.004f, 0.003f, vec4(0, 0, 0, 0), 6, 2);
        b.mat = 25.f;
        sphere(b, W(b0), 0.008f, 10, 6, vec4(0, 0, 0, 0));
    }
    b.mat = 24.f;
    {
        const vec3 c(0, 0.052f, -0.02f);
        const vec3 r(0.044f / 1.4f, 0.044f, 0.044f);
        auto pos = [&](float u, float v) {
            const float th = v * PI_, ph = u * TAU_;
            return W(c + V3(std::sin(th) * std::cos(ph) * r.x, std::cos(th) * r.y, std::sin(th) * std::sin(ph) * r.z));
        };
        const vec3 cw = W(c);
        surf(b, 24, 14, pos, [&](const vec3&, float u, float v) {
            const float th = v * PI_, ph = u * TAU_;
            return vec4(std::sin(th) * std::cos(ph) * r.x, std::cos(th) * r.y, std::sin(th) * std::sin(ph) * r.z, 0);
        }, &cw);
    }
}

} // namespace terra
