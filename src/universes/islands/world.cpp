// ISLANDS' world: the archipelago, the islands' shapes and content, the route (see world.hpp).
#include "universes/islands/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace isl {

// =====================================================================================================
// Helpers
// =====================================================================================================

uint32_t hashu(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
float hashf(uint32_t x) { return (hashu(x) & 0xFFFFFF) / 16777216.f; }
float smooth01(float t) {
    t = saturate(t);
    return t * t * (3.f - 2.f * t);
}
static float ss(float a, float b, float x) { return smoothstepf(a, b, x); } // (works for a > b too)

static inline float lat(int x, int y, uint32_t s) {
    return (hashu((uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u) & 0xFFFFFF) / 16777216.f;
}
float noise2(float x, float y, uint32_t seed) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int ix = (int)fx, iy = (int)fy;
    float u = x - fx, v = y - fy;
    u = u * u * (3.f - 2.f * u);
    v = v * v * (3.f - 2.f * v);
    const float a = lat(ix, iy, seed), b = lat(ix + 1, iy, seed), c = lat(ix, iy + 1, seed), d = lat(ix + 1, iy + 1, seed);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
float fbm2(float x, float y, uint32_t seed, int oct) {
    float s = 0.f, a = 0.5f, n = 0.f;
    for (int i = 0; i < oct; i++) {
        s += a * noise2(x, y, seed + (uint32_t)i * 101u);
        n += a;
        x = x * 2.03f + 17.1f;
        y = y * 2.03f + 3.7f;
        a *= 0.5f;
    }
    return s / n;
}

Q qAxis(const vec3& axis, float a) {
    const vec3 n = normalize(axis);
    const float s = std::sin(a * 0.5f);
    return {n.x * s, n.y * s, n.z * s, std::cos(a * 0.5f)};
}
Q qMul(const Q& a, const Q& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
vec3 qRot(const Q& q, const vec3& v) {
    const vec3 u(q.x, q.y, q.z);
    const vec3 t = cross(u, v) * 2.f;
    return v + t * q.w + cross(u, t);
}

Inst inst(int mat, const vec3& p, const vec3& s, const Q& q, const vec3& col, float param, float seed) {
    Inst i;
    i.a[0] = p.x; i.a[1] = p.y; i.a[2] = p.z; i.a[3] = seed;
    i.b[0] = s.x; i.b[1] = s.y; i.b[2] = s.z; i.b[3] = (float)mat;
    i.c[0] = col.x; i.c[1] = col.y; i.c[2] = col.z; i.c[3] = param;
    i.d[0] = q.x; i.d[1] = q.y; i.d[2] = q.z; i.d[3] = q.w;
    return i;
}

// =====================================================================================================
// The object meshes
// =====================================================================================================

namespace {

struct MB {
    std::vector<float>& v;
    void vert(const vec3& p, const vec3& n, float a = 0.f, float b = 0.f) {
        const float f[8] = {p.x, p.y, p.z, n.x, n.y, n.z, a, b};
        v.insert(v.end(), f, f + 8);
    }
    // a triangle with a flat normal (from its winding)
    void tri(const vec3& a, const vec3& b, const vec3& c, float ax = 0.f, float ay = 0.f) {
        const vec3 n = normalize(cross(b - a, c - a));
        vert(a, n, ax, ay); vert(b, n, ax, ay); vert(c, n, ax, ay);
    }
    void quad(const vec3& a, const vec3& b, const vec3& c, const vec3& d, float ax = 0.f, float ay = 0.f) {
        tri(a, b, c, ax, ay);
        tri(a, c, d, ax, ay);
    }
};

void sphere(MB& m, int su, int sv, float y0 = -1.f) {
    auto P = [&](int i, int j) {
        const float th = PI * j / sv, ph = TAU * i / su;
        return vec3(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
    };
    for (int j = 0; j < sv; j++)
        for (int i = 0; i < su; i++) {
            const vec3 a = P(i, j), b = P(i + 1, j), c = P(i + 1, j + 1), d = P(i, j + 1);
            if (y0 > -1.f && a.y < y0 - 1e-4f && c.y < y0 - 1e-4f) continue;
            for (const vec3& p : {a, b, c, a, c, d}) m.vert(p * 0.5f, p);
        }
}

// a tube along a polyline of centers and radii (a palm's trunk), aux = (part, t)
void tube(MB& m, const std::vector<vec3>& c, const std::vector<float>& r, int sides, float part) {
    const int n = (int)c.size();
    std::vector<vec3> ring((size_t)n * sides), nr((size_t)n * sides);
    for (int i = 0; i < n; i++) {
        const vec3 t = normalize(i + 1 < n ? c[i + 1] - c[i] : c[i] - c[i - 1]);
        const vec3 a = normalize(cross(t, std::fabs(t.y) > 0.9f ? vec3(1, 0, 0) : vec3(0, 1, 0)));
        const vec3 b = cross(t, a);
        for (int k = 0; k < sides; k++) {
            const float an = TAU * k / sides;
            const vec3 d = a * std::cos(an) + b * std::sin(an);
            ring[(size_t)i * sides + k] = c[i] + d * r[i];
            nr[(size_t)i * sides + k] = d;
        }
    }
    for (int i = 0; i + 1 < n; i++)
        for (int k = 0; k < sides; k++) {
            const int k1 = (k + 1) % sides;
            const size_t A = (size_t)i * sides + k, B = (size_t)i * sides + k1, C = (size_t)(i + 1) * sides + k1, D = (size_t)(i + 1) * sides + k;
            const float t0 = (float)i / (n - 1), t1 = (float)(i + 1) / (n - 1);
            m.vert(ring[A], nr[A], part, t0); m.vert(ring[C], nr[C], part, t1); m.vert(ring[B], nr[B], part, t0);
            m.vert(ring[A], nr[A], part, t0); m.vert(ring[D], nr[D], part, t1); m.vert(ring[C], nr[C], part, t1);
        }
}

// A palm: a curved trunk and a crown of arching fronds, each with its leaflets (aux.x: 0 trunk, 1 frond,
// 2 coconut; aux.y: along the trunk / the frond). About 10 m tall at scale 1, leaning along +x.
void palm(MB& m, int variant) {
    const uint32_t s = 911u + (uint32_t)variant * 7919u;
    const float H = 9.5f + variant * 0.6f, lean = 0.12f + 0.12f * variant;
    std::vector<vec3> c;
    std::vector<float> r;
    const int TN = 16;
    for (int i = 0; i <= TN; i++) {
        const float t = (float)i / TN;
        // the trunk curves: leaning out, then rising toward the light at the top
        const float x = H * lean * (t * t * 0.6f + t * 0.5f) - H * 0.04f * t * t * t * (variant == 2 ? 2.f : 1.f);
        c.push_back(vec3(x, H * t, 0.f));
        r.push_back(0.24f - 0.09f * t + (i == 0 ? 0.06f : 0.f) + 0.01f * std::sin(t * 31.f));
    }
    tube(m, c, r, 9, 0.f);
    const vec3 top = c.back();
    const int FN = 12 + variant;
    for (int f = 0; f < FN; f++) {
        const float az = TAU * f / FN + 0.4f * (hashf(s + f) - 0.5f);
        const bool upper = f % 3 == 0;
        const float el = upper ? 0.55f + 0.2f * hashf(s + 40 + f) : 0.15f - 0.35f * hashf(s + 80 + f);
        const float L = (upper ? 3.6f : 4.6f) + 0.8f * hashf(s + 120 + f);
        const float droop = upper ? 0.35f : 0.75f;
        const vec3 hd(std::cos(az), 0.f, std::sin(az));
        const int SN = 14;
        std::vector<vec3> sp;
        for (int i = 0; i <= SN; i++) {
            const float t = (float)i / SN;
            sp.push_back(top + hd * (L * t * std::cos(el)) + vec3(0, L * t * std::sin(el) - droop * L * t * t, 0));
        }
        // the rachis (a thin strip) and the leaflets (pinnae): narrow blades hanging down from it in a V
        for (int i = 0; i < SN; i++) {
            const float t0 = (float)i / SN, t1 = (float)(i + 1) / SN;
            const vec3 a = sp[i], b = sp[i + 1];
            const vec3 dir = normalize(b - a);
            const vec3 side = normalize(cross(dir, vec3(0, 1, 0)));
            const vec3 up = normalize(cross(side, dir));
            const float w = 0.05f;
            m.tri(a - side * w, b - side * w * 0.6f, b + side * w * 0.6f, 1.f, t0);
            m.tri(a - side * w, b + side * w * 0.6f, a + side * w, 1.f, t0);
            if (i == 0) continue;
            const float len = (0.25f + 1.05f * std::sin(PI * std::min(1.f, t0 * 1.15f))) * (0.85f + 0.3f * hashf(s + f * 31 + i));
            for (int sd = -1; sd <= 1; sd += 2) {
                for (int k = 0; k < 2; k++) {
                    const float tt = t0 + (t1 - t0) * (0.25f + 0.5f * k);
                    const vec3 base = a + (b - a) * (0.25f + 0.5f * k);
                    // the leaflet: outward, forward along the frond and down (a V fold), curving at its tip
                    const vec3 out = normalize(side * (float)sd * 0.85f + dir * 0.45f - up * 0.42f);
                    const vec3 tip = base + out * len - up * (0.18f * len);
                    const vec3 mid = base + out * (len * 0.5f) - up * (0.04f * len);
                    const vec3 wv = dir * (0.07f + 0.05f * std::sin(PI * t0));
                    m.tri(base - wv, mid, base + wv, 1.f, tt);
                    m.tri(base + wv, mid, mid + wv * 0.8f, 1.f, tt);
                    m.tri(mid - wv * 0.1f, tip, mid + wv * 0.8f, 1.f, tt);
                }
            }
        }
    }
    // coconuts under the crown
    for (int k = 0; k < 5; k++) {
        const float az = TAU * k / 5.f + 0.3f;
        const vec3 cc = top + vec3(std::cos(az) * 0.28f, -0.35f - 0.12f * (k % 2), std::sin(az) * 0.28f);
        const int SU = 6, SV = 4;
        for (int j = 0; j < SV; j++)
            for (int i = 0; i < SU; i++) {
                auto P = [&](int ii, int jj) {
                    const float th = PI * jj / SV, ph = TAU * ii / SU;
                    return vec3(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
                };
                const vec3 a = P(i, j), b = P(i + 1, j), c2 = P(i + 1, j + 1), d = P(i, j + 1);
                for (const vec3& p : {a, b, c2, a, c2, d}) m.vert(cc + p * 0.17f, p, 2.f, 0.f);
            }
    }
}

// A hull: x -0.5 (transom) .. 0.5 (bow), the deck at y 0 (a slight sheer), the keel at y -1, z -0.5..0.5.
// aux.x: 0 the topsides, 1 the deck.
void hull(MB& m) {
    const int NS = 22, NP = 8;
    auto beam = [](float u) { return 0.5f * (0.8f + 0.2f * u) * (1.f - std::pow(std::max(0.f, (u - 0.42f) / 0.58f), 2.f)); };
    auto depth = [](float u) { return 1.f - 0.6f * std::pow(u, 2.5f) - 0.35f * std::pow(1.f - u, 4.f); };
    auto sheer = [](float u) { return 0.1f * u * u; };
    auto P = [&](int i, int k, float side) {
        const float u = (float)i / NS, th = (float)k / NP * (PI * 0.5f);
        const float b = beam(u), d = depth(u), sh = sheer(u);
        const float z = b * std::pow(std::cos(th), 0.55f);
        const float y = sh - (sh + d) * std::pow(std::sin(th), 1.3f);
        return vec3(u - 0.5f, y, z * side);
    };
    for (int side = -1; side <= 1; side += 2)
        for (int i = 0; i < NS; i++)
            for (int k = 0; k < NP; k++) {
                const vec3 a = P(i, k, (float)side), b = P(i + 1, k, (float)side), c = P(i + 1, k + 1, (float)side), d = P(i, k + 1, (float)side);
                if (side > 0) m.quad(a, d, c, b, 0.f, (float)k / NP);
                else m.quad(a, b, c, d, 0.f, (float)k / NP);
            }
    // the deck
    for (int i = 0; i < NS; i++) {
        const vec3 a = P(i, 0, -1), b = P(i + 1, 0, -1), c = P(i + 1, 0, 1), d = P(i, 0, 1);
        m.quad(a, d, c, b, 1.f, 0.f);
    }
    // the transom
    for (int k = 0; k < NP; k++) {
        const vec3 a = P(0, k, -1), b = P(0, k + 1, -1), c = P(0, k + 1, 1), d = P(0, k, 1);
        m.quad(a, d, c, b, 0.f, 0.5f);
    }
}

// A sail: the mainsail (luff up the mast at x 0, foot along -x, a little roach and camber), or the jib.
void sail(MB& m, bool jib) {
    const int N = 9;
    auto P = [&](float u, float v) {
        // u along the foot (0 luff .. 1 clew), v up (0 foot .. 1 head)
        const float w = (1.f - v);
        float x, y;
        if (!jib) { x = -u * w * (1.f + 0.12f * std::sin(PI * v)); y = v; }
        else { x = -0.42f * v - u * w * 0.62f; y = v * 1.f + 0.04f * u * w; }
        const float camber = 0.09f * std::sin(PI * u) * (0.4f + 0.6f * w);
        return vec3(x, y, camber);
    };
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N - j; i++) {
            const float v0 = (float)j / N, v1 = (float)(j + 1) / N;
            // barycentric grid of the triangle: (i, j) -> u = i / (N - j), so the rows shorten upward
            auto Q2 = [&](int ii, int jj) {
                const float vv = (float)jj / N;
                const int cols = N - jj;
                const float uu = cols > 0 ? (float)ii / cols : 0.f;
                return P(uu, vv);
            };
            const vec3 p00 = Q2(i, j), p10 = Q2(i + 1, j), p01 = Q2(i, j + 1);
            m.tri(p00, p10, p01, 0.f, v0);
            if (i + 1 < N - j) {
                const vec3 p11 = Q2(i + 1, j + 1);
                if (i + 1 <= N - j - 1) m.tri(p10, p11, p01, 0.f, v1);
            }
        }
}

// A seabird: a small body, two wings (aux.x 1, aux.y the span 0..1, flapped in the shader), a tail. Wingspan 1.
void bird(MB& m) {
    // body
    const vec3 nose(0.22f, 0.f, 0.f), tail(-0.24f, 0.01f, 0.f);
    const vec3 t1(0.f, 0.04f, 0.f), t2(0.f, -0.035f, 0.035f), t3(0.f, -0.035f, -0.035f);
    m.tri(nose, t1, t2); m.tri(nose, t3, t1); m.tri(nose, t2, t3);
    m.tri(tail, t2, t1); m.tri(tail, t1, t3); m.tri(tail, t3, t2);
    m.tri(tail + vec3(0, 0, 0.07f), tail + vec3(0, 0, -0.07f), tail + vec3(0.12f, 0.01f, 0.f));
    // wings: two panels each, a gull's crank (the outer panel swept back)
    for (int sd = -1; sd <= 1; sd += 2) {
        const float s = (float)sd;
        const vec3 r0(0.08f, 0.01f, 0.f), r1(-0.07f, 0.01f, 0.f);
        const vec3 m0(0.06f, 0.03f, 0.24f * s), m1(-0.06f, 0.03f, 0.24f * s);
        const vec3 tip(-0.1f, 0.0f, 0.5f * s);
        auto V = [&](const vec3& p, float span) { m.vert(p, vec3(0, 1, 0), 1.f, span); };
        V(r0, 0.f); V(m0, 0.5f); V(m1, 0.5f);
        V(r0, 0.f); V(m1, 0.5f); V(r1, 0.f);
        V(m0, 0.5f); V(tip, 1.f); V(m1, 0.5f);
    }
}

// A dolphin, 1 long along +x (the beak forward): a tapered body, a curved dorsal fin, flippers, flukes.
void dolphin(MB& m) {
    const int NS = 18, NR = 10;
    auto R = [](float u) { // u 0 tail .. 1 beak
        const float body = std::pow(std::sin(PI * std::min(1.f, u * 1.08f)), 0.75f) * 0.085f;
        return u > 0.92f ? body * 0.55f + 0.012f : body + 0.006f;
    };
    auto P = [&](int i, int k) {
        const float u = (float)i / NS, a = TAU * k / NR;
        const float r = R(u);
        return vec3(u - 0.5f, std::sin(a) * r * 1.05f + 0.01f * std::sin(PI * u), std::cos(a) * r * 0.9f);
    };
    for (int i = 0; i < NS; i++)
        for (int k = 0; k < NR; k++) {
            const vec3 a = P(i, k), b = P(i + 1, k), c = P(i + 1, k + 1), d = P(i, k + 1);
            const float belly = std::sin(TAU * k / NR) < -0.2f ? 1.f : 0.f;
            m.quad(a, b, c, d, belly, (float)i / NS);
        }
    // the dorsal fin, the flippers, the flukes (both faces)
    auto tri2 = [&](const vec3& a, const vec3& b, const vec3& c) { m.tri(a, b, c); m.tri(a, c, b); };
    tri2(vec3(0.02f, 0.07f, 0), vec3(-0.14f, 0.07f, 0), vec3(-0.12f, 0.2f, 0));
    for (int sd = -1; sd <= 1; sd += 2) {
        tri2(vec3(0.18f, -0.04f, 0.05f * sd), vec3(0.08f, -0.05f, 0.06f * sd), vec3(0.06f, -0.1f, 0.17f * sd));
        tri2(vec3(-0.46f, 0.f, 0.f), vec3(-0.56f, 0.f, 0.15f * sd), vec3(-0.5f, 0.f, 0.02f * sd));
        tri2(vec3(-0.44f, 0.f, 0.f), vec3(-0.56f, 0.f, 0.15f * sd), vec3(-0.47f, 0.f, 0.f));
    }
}

} // namespace

std::vector<float> buildMesh(int mesh) {
    std::vector<float> v;
    MB m{v};
    switch (mesh) {
    case M_BOX: {
        const float N[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (auto& n : N) {
            const vec3 nv(n[0], n[1], n[2]);
            const vec3 u = std::fabs(nv.y) > 0.5f ? vec3(1, 0, 0) : normalize(cross(vec3(0, 1, 0), nv));
            const vec3 w = cross(nv, u);
            const float c[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
            for (auto& k : c) m.vert(nv * 0.5f + u * (0.5f * k[0]) + w * (0.5f * k[1]), nv);
        }
        break;
    }
    case M_SPHERE: sphere(m, 18, 12); break;
    case M_LUMP: sphere(m, 14, 9); break;
    case M_DOME: sphere(m, 18, 10, 0.f); break;
    case M_CYL:
    case M_CONE:
    case M_FRUSTUM: {
        const int S = 16;
        const float rt = mesh == M_CYL ? 0.5f : mesh == M_CONE ? 0.f : 0.32f;
        const float slope = (0.5f - rt);
        for (int i = 0; i < S; i++) {
            const float a0 = TAU * i / S, a1 = TAU * (i + 1) / S;
            const vec3 d0(std::cos(a0), 0, std::sin(a0)), d1(std::cos(a1), 0, std::sin(a1));
            const vec3 n0 = normalize(d0 + vec3(0, slope, 0)), n1 = normalize(d1 + vec3(0, slope, 0));
            const vec3 b0 = d0 * 0.5f + vec3(0, -0.5f, 0), b1 = d1 * 0.5f + vec3(0, -0.5f, 0);
            const vec3 t0 = d0 * rt + vec3(0, 0.5f, 0), t1 = d1 * rt + vec3(0, 0.5f, 0);
            m.vert(b0, n0, 0, 0); m.vert(t1, n1, 0, 1); m.vert(b1, n1, 0, 0);
            m.vert(b0, n0, 0, 0); m.vert(t0, n0, 0, 1); m.vert(t1, n1, 0, 1);
            m.vert(vec3(0, -0.5f, 0), vec3(0, -1, 0)); m.vert(b1, vec3(0, -1, 0)); m.vert(b0, vec3(0, -1, 0));
            if (rt > 0.f) { m.vert(vec3(0, 0.5f, 0), vec3(0, 1, 0), 1, 1); m.vert(t0, vec3(0, 1, 0), 1, 1); m.vert(t1, vec3(0, 1, 0), 1, 1); }
        }
        break;
    }
    case M_GABLE: {
        // a roof: ridge along x at y 1, eaves at y 0 (z -0.5, 0.5)
        const vec3 a(-0.5f, 0, -0.5f), b(0.5f, 0, -0.5f), c(0.5f, 0, 0.5f), d(-0.5f, 0, 0.5f);
        const vec3 r0(-0.5f, 1, 0), r1(0.5f, 1, 0);
        m.quad(a, r0, r1, b, 0, 0);
        m.quad(d, c, r1, r0, 0, 0);
        m.tri(a, d, r0, 1, 0);
        m.tri(b, r1, c, 1, 0);
        m.quad(a, b, c, d, 2, 0);
        break;
    }
    case M_HIP: {
        const vec3 a(-0.5f, 0, -0.5f), b(0.5f, 0, -0.5f), c(0.5f, 0, 0.5f), d(-0.5f, 0, 0.5f);
        const vec3 r0(-0.12f, 1, 0), r1(0.12f, 1, 0);
        m.quad(a, r0, r1, b);
        m.quad(d, c, r1, r0);
        m.tri(a, d, r0);
        m.tri(b, r1, c);
        m.quad(a, b, c, d, 2, 0);
        break;
    }
    case M_HULL: hull(m); break;
    case M_SAIL: sail(m, false); break;
    case M_JIB: sail(m, true); break;
    case M_PALM0: palm(m, 0); break;
    case M_PALM1: palm(m, 1); break;
    case M_PALM2: palm(m, 2); break;
    case M_BIRD: bird(m); break;
    case M_DOLPHIN: dolphin(m); break;
    default: break;
    }
    return v;
}

namespace {
std::mutex graveMu;
std::vector<unsigned> graveVao, graveVbo;
}
Island::~Island() {
    if (!vao && !vbo) return;
    std::lock_guard<std::mutex> l(graveMu);
    if (vao) graveVao.push_back(vao);
    if (vbo) graveVbo.push_back(vbo);
}
void takeGraveyard(std::vector<unsigned>& vaos, std::vector<unsigned>& vbos) {
    std::lock_guard<std::mutex> l(graveMu);
    vaos.swap(graveVao);
    vbos.swap(graveVbo);
    graveVao.clear();
    graveVbo.clear();
}

// =====================================================================================================
// The islands' shapes
// =====================================================================================================

namespace {
// cliffs along some sectors of the coast (0..1), from the direction of a point
float cliffAt(const Island& I, float ca, float sa) {
    return ss(0.44f, 0.6f, fbm2(ca * 1.6f + 1.3f, sa * 1.6f + 9.1f, I.seed + 21u, 3)) * I.cliffK;
}
}

Ground groundAt(const Island& I, float lx, float lz) {
    Ground g{-45.f, 1.f, 0.f, 0.f, 0.f};
    const float R = I.R;
    const uint32_t s = I.seed;
    const float r = std::sqrt(lx * lx + lz * lz);
    const float ca = r > 1e-3f ? lx / r : 1.f, sa = r > 1e-3f ? lz / r : 0.f;
    const float lobe = fbm2(ca * 1.1f + 3.1f, sa * 1.1f + 7.7f, s, 3) - 0.5f;
    const float warp = fbm2(lx / R * 1.7f + 11.f, lz / R * 1.7f + 5.f, s + 7u, 3) - 0.5f;
    auto reefPatch = [&](float k) {
        // coral patches in the shallows: blobs of reef rising to just under the surface
        const float n = fbm2(lx * k + 3.3f, lz * k + 1.7f, s + 55u, 3);
        return ss(0.56f, 0.68f, n);
    };

    switch (I.type) {
    case T_CAY: {
        const float d = r / R / (1.f + 0.3f * lobe) + 0.12f * warp;
        if (d < 1.f) {
            g.h = 1.6f * (1.f - d * d) + 0.4f * (fbm2(lx * 0.08f, lz * 0.08f, s + 3u, 2) - 0.5f) * (1.f - d);
            g.veg = R > 38.f ? ss(0.55f, 0.3f, d) * ss(0.45f, 0.6f, fbm2(lx * 0.06f, lz * 0.06f, s + 9u, 3)) : 0.f;
        } else {
            g.h = -0.4f * ss(1.f, 1.35f, d) - 1.7f * ss(1.25f, 2.9f, d) - 42.f * ss(3.0f, 4.4f, d);
            const float band = ss(1.6f, 2.0f, d) * ss(3.4f, 2.9f, d);
            const float rf = reefPatch(0.035f) * band;
            g.h += rf * 1.3f;
            g.reef = rf;
        }
        break;
    }
    case T_ATOLL: {
        const float rr = r / (1.f + 0.1f * lobe);
        const float wr = 0.13f * R;
        const float dr = (rr - R) / wr + warp * 1.6f;
        const float motu = ss(0.47f, 0.6f, fbm2(ca * 2.6f + 5.1f, sa * 2.6f + 2.2f, s + 5u, 3));
        const float ringH = -0.5f + 3.0f * motu;
        const float lag = -3.6f - 2.6f * fbm2(lx * 0.012f, lz * 0.012f, s + 8u, 3);
        float h;
        if (dr < 0.f) h = lerpf(lag, ringH, ss(-2.6f, -0.25f, dr));
        else h = lerpf(ringH, -42.f, ss(0.35f, 2.6f, dr));
        // land on the motus: a gentle dome
        if (h > 0.f) h = h * (0.8f + 0.2f * fbm2(lx * 0.05f, lz * 0.05f, s + 4u, 2));
        // coral heads in the lagoon
        if (dr < -1.2f && h < -1.f) {
            const float rf = reefPatch(0.03f);
            h = lerpf(h, -0.9f, rf * 0.8f);
            g.reef = rf;
        }
        if (dr > 0.f && dr < 1.4f && h < 0.f) g.reef = std::max(g.reef, 0.6f * ss(0.f, 0.5f, dr) * ss(1.4f, 0.9f, dr));
        // a central peak (Polynesia): a jungle mountain in the lagoon, ringed by its own beach
        if (I.peak) {
            const float cr = r / (0.4f * R * (1.f + 0.25f * lobe));
            if (cr < 1.4f) {
                const float core = saturate(1.f - cr);
                const float spire = fbm2(lx / R * 4.f, lz / R * 4.f, s + 11u, 4);
                const float hp = I.H * std::pow(core, 1.25f) * (0.65f + 0.6f * spire) + 2.f * ss(1.f, 0.9f, cr);
                const float shore = lerpf(lag, 0.f, ss(1.4f, 1.f, cr));
                h = std::max(h, cr < 1.f ? hp : shore);
                if (cr < 1.f) g.veg = ss(0.95f, 0.85f, cr);
                g.rock = ss(0.55f, 0.85f, hp / std::max(1.f, I.H)) * 0.8f;
            }
        }
        g.h = h;
        if (h > 0.5f && !I.peak) g.veg = ss(0.6f, 2.f, h) * ss(0.4f, 0.65f, fbm2(lx * 0.04f, lz * 0.04f, s + 6u, 3));
        break;
    }
    case T_ISLET: {
        const float d = r / R / (1.f + 0.35f * lobe) + 0.1f * warp;
        if (d < 1.f) {
            float h = I.H * std::pow(saturate(1.f - d), 0.6f) * (0.82f + 0.36f * fbm2(lx * 0.12f, lz * 0.12f, s + 2u, 3));
            const float cap = I.H * 0.8f;
            h = h > cap ? cap + (h - cap) * 0.15f : h;
            g.h = h;
            g.rock = 1.f - 0.6f * ss(0.4f, 0.2f, d);
            g.veg = ss(0.45f, 0.25f, d) * 0.7f;
            g.sand = 0.f;
        } else {
            g.h = -5.f * ss(1.f, 1.25f, d) - 36.f * ss(1.15f, 2.2f, d) + 2.f * fbm2(lx * 0.2f, lz * 0.2f, s + 3u, 2);
            g.rock = ss(1.6f, 1.f, d);
            g.reef = 0.5f * ss(1.05f, 1.2f, d) * ss(1.8f, 1.4f, d) * reefPatch(0.08f);
        }
        break;
    }
    default: { // T_VOLCANO, T_VILLAGE, T_VILLA
        const float d = r / R / (1.f + (I.type == T_VILLA ? 0.25f : 0.42f) * lobe) + 0.2f * warp;
        // the summit, a little off center, its ridges and spires
        const float sx = std::cos((float)(s % 628) * 0.01f) * 0.18f * R, sz = std::sin((float)(s % 628) * 0.01f) * 0.18f * R;
        const float rs = std::sqrt((lx - sx) * (lx - sx) + (lz - sz) * (lz - sz)) / (R * 0.95f);
        const float core = saturate(1.f - std::max(d * 0.85f, rs));
        const float ang = std::atan2(lz, lx);
        const float gully = 0.5f + 0.5f * std::sin(ang * (6.f + (float)(s % 5)) + 3.f * fbm2(lx / R * 2.f, lz / R * 2.f, s + 3u, 2));
        const float spire = fbm2(lx / R * 3.4f, lz / R * 3.4f, s + 11u, 4);
        const float exK = I.type == T_VOLCANO ? 1.35f : 1.1f;
        const float hp = I.H * std::pow(core, exK) * (0.7f + 0.3f * gully * core) * (0.72f + 0.56f * spire);
        const float cl = I.type == T_VILLA ? 0.f : cliffAt(I, ca, sa);
        const float beachW = I.type == T_VOLCANO ? 0.05f : 0.08f;
        const float beach = 2.2f * ss(1.f, 1.f - beachW, d);
        const float cliffH = I.H * 0.14f + 14.f;
        const float coast = lerpf(beach, cliffH * ss(1.f, 0.972f, d), cl);
        if (d < 1.f) {
            g.h = coast + hp * ss(1.f, 0.9f, d);
            g.rock = std::max(cl * ss(1.f, 0.965f, d) * ss(0.9f, 0.96f, d), ss(0.62f, 0.9f, hp / std::max(1.f, I.H)) * (I.type == T_VOLCANO ? 0.9f : 0.3f));
            g.veg = ss(1.2f, 3.5f, g.h);
            g.sand = (1.f - ss(1.6f, 3.4f, g.h)) * (1.f - cl);
        } else {
            float w = I.shelfW * lerpf(0.34f, 0.07f, cl);
            if (I.type == T_VILLAGE || I.type == T_VILLA) {
                const float facing = std::cos(ang - I.face);
                w *= 1.f + 1.4f * std::max(0.f, facing);
            }
            g.h = -2.2f * ss(1.f, 1.f + w * 0.5f, d) - 40.f * ss(1.f + w * 0.42f, 1.f + w * 1.25f + 0.06f, d);
            const float band = ss(1.f + w * 0.12f, 1.f + w * 0.3f, d) * ss(1.f + w * 0.9f, 1.f + w * 0.6f, d);
            const float rf = reefPatch(0.022f) * band;
            g.h += rf * 1.4f;
            g.reef = rf;
            g.rock = cl * ss(1.08f, 1.f, d);
        }
        // the villa's pad: the terrain levelled under the house, its deck and its garden
        if (I.padHW > 0.f) {
            const float dx = lx - I.padX, dz = lz - I.padZ;
            const float c = std::cos(I.padA), sn = std::sin(I.padA);
            const float u = dx * c + dz * sn, v = -dx * sn + dz * c;
            const float q = std::max(std::fabs(u) / I.padHW, std::fabs(v) / I.padHD);
            const float m = ss(1.25f, 0.9f, q);
            if (m > 0.f) {
                g.h = lerpf(g.h, I.padH, m);
                g.sand *= 1.f - m;
                g.rock *= 1.f - m;
                g.veg = lerpf(g.veg, 0.55f, m);
            }
        }
        break;
    }
    }
    if (g.h < 0.f) { g.sand = 1.f; g.veg = 0.f; }
    return g;
}

std::vector<float> buildTerrain(const Island& I, int n) {
    std::vector<float> h((size_t)n * n);
    std::vector<Ground> gr((size_t)n * n);
    const float step = 2.f * I.ext / (n - 1);
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            const float x = -I.ext + i * step, z = -I.ext + j * step;
            Ground g = groundAt(I, x, z);
            // the grid's border sinks into the deep, so no edge shows
            const float e = std::max(std::fabs(x), std::fabs(z)) / I.ext;
            if (g.h < 0.f) g.h = lerpf(g.h, -45.f, ss(0.82f, 1.f, e));
            gr[(size_t)j * n + i] = g;
            h[(size_t)j * n + i] = g.h;
        }
    std::vector<float> v((size_t)n * n * TERRAIN_FLOATS);
    auto H = [&](int i, int j) { return h[(size_t)std::clamp(j, 0, n - 1) * n + std::clamp(i, 0, n - 1)]; };
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            const float x = -I.ext + i * step, z = -I.ext + j * step;
            const Ground& g = gr[(size_t)j * n + i];
            const vec3 nr = normalize(vec3(H(i - 1, j) - H(i + 1, j), 2.f * step, H(i, j - 1) - H(i, j + 1)));
            // a cavity term: lower than the neighbourhood is darker
            const float avg = (H(i - 2, j) + H(i + 2, j) + H(i, j - 2) + H(i, j + 2)) * 0.25f;
            const float ao = g.h > -0.5f ? 0.55f + 0.45f * saturate(1.f - std::max(0.f, avg - g.h) / (step * 1.2f)) : 1.f;
            float rock = g.rock, veg = g.veg, sand = g.sand;
            if (g.h > 0.8f) {
                const float steep = ss(0.42f, 0.7f, 1.f - nr.y);
                rock = std::max(rock, steep);
                veg *= 1.f - steep * 0.85f;
            }
            float* o = &v[((size_t)j * n + i) * TERRAIN_FLOATS];
            o[0] = x; o[1] = g.h; o[2] = z;
            o[3] = nr.x; o[4] = nr.y; o[5] = nr.z;
            o[6] = sand; o[7] = veg; o[8] = rock; o[9] = g.reef;
            o[10] = ao;
        }
    return v;
}

// =====================================================================================================
// Content
// =====================================================================================================

namespace {

struct Furn {
    Island& I;
    const Style& st;
    uint32_t k = 0;
    float rnd() { return hashf(I.seed * 7919u + (k++) * 104729u + 17u); }
    float range(float a, float b) { return a + (b - a) * rnd(); }
    float groundW(float wx, float wz) { return heightAt(I, wx - I.x, wz - I.z); }
    void add(int mesh, int mat, const vec3& p, const vec3& s, const Q& q, const vec3& col, float param = 0.f) {
        I.parts.push_back({mesh, inst(mat, p, s, q, col, param, rnd())});
    }
    void obst(float x, float z, float r, float top) { I.obst.push_back({x, z, r, top}); I.top = std::max(I.top, top); }
    // a local frame on the island: origin o, x along t, z along w (outward), its rotation
    struct Frame {
        vec3 o, t, w;
        Q q;
        vec3 at(float u, float y, float v) const { return o + t * u + w * v + vec3(0, y, 0); }
    };
    Frame frame(const vec3& o, float outA) {
        Frame f;
        f.o = o;
        f.w = vec3(std::cos(outA), 0, std::sin(outA));
        f.t = vec3(-std::sin(outA), 0, std::cos(outA));
        // local +z -> w, +x -> t: a yaw about y
        f.q = qYaw(std::atan2(f.w.x, f.w.z));
        return f;
    }
    void palmAt(float wx, float wz, float outA, float sc) {
        const float g = groundW(wx, wz);
        if (g < 0.4f) return;
        const int mesh = M_PALM0 + (int)(rnd() * 2.99f);
        const float lean = outA + range(-0.6f, 0.6f);
        // the mesh leans along +x: turn +x to the lean direction (yaw by -lean)
        const Q q = qMul(qYaw(-lean), qAxis(vec3(0, 1, 0), 0.f));
        add(mesh, MAT_PALM, vec3(wx, g - 0.3f, wz), vec3(sc), q, vec3(1.f), rnd());
        const float off = 1.6f * sc;
        obst(wx + std::cos(lean) * off, wz + std::sin(lean) * off, 4.5f * sc, g + 10.5f * sc);
    }
    void treeAt(float wx, float wz, float sc, bool olive) {
        const float g = groundW(wx, wz);
        if (g < 1.f) return;
        if (olive) {
            add(M_CYL, MAT_WOOD, vec3(wx, g + 0.9f * sc, wz), vec3(0.35f * sc, 1.9f * sc, 0.35f * sc), qYaw(rnd() * 6.f), st.accentWood * 0.6f);
            add(M_LUMP, MAT_FOLIAGE, vec3(wx, g + 2.6f * sc, wz), vec3(4.2f * sc, 2.8f * sc, 4.f * sc), qYaw(rnd() * 6.f), vec3(0.3f, 0.36f, 0.22f), 1.f);
            obst(wx, wz, 2.5f * sc, g + 4.2f * sc);
        } else {
            add(M_LUMP, MAT_FOLIAGE, vec3(wx, g + 2.4f * sc, wz), vec3(8.f * sc, 7.f * sc, 8.f * sc), qYaw(rnd() * 6.f), vec3(1.f), 0.f);
            obst(wx, wz, 4.f * sc, g + 6.f * sc);
        }
    }
    void cypressAt(float wx, float wz, float sc) {
        const float g = groundW(wx, wz);
        if (g < 1.f) return;
        add(M_LUMP, MAT_FOLIAGE, vec3(wx, g + 4.5f * sc, wz), vec3(2.2f * sc, 10.f * sc, 2.2f * sc), qYaw(rnd() * 6.f), vec3(0.16f, 0.24f, 0.12f), 2.f);
        obst(wx, wz, 1.5f * sc, g + 9.5f * sc);
    }
    void umbrella(const vec3& p, const vec3& col) {
        add(M_CYL, MAT_METAL, p + vec3(0, 1.1f, 0), vec3(0.05f, 2.2f, 0.05f), Q{}, vec3(0.9f));
        add(M_CONE, MAT_CANVAS, p + vec3(0, 2.25f, 0), vec3(2.6f, 0.55f, 2.6f), qYaw(rnd() * 6.f), col);
    }
    void lounger(const vec3& p, const Q& q) {
        add(M_BOX, MAT_PAINT, p + vec3(0, 0.25f, 0), vec3(0.7f, 0.12f, 1.9f), q, vec3(0.92f, 0.9f, 0.86f));
        add(M_BOX, MAT_PAINT, p + qRot(q, vec3(0, 0.45f, -0.75f)), vec3(0.7f, 0.1f, 0.55f), qMul(q, qAxis(vec3(1, 0, 0), -0.7f)), vec3(0.92f, 0.9f, 0.86f));
    }
    // where the land ends along a direction from a local point: the distance to the waterline
    float toShore(float lx, float lz, float dx, float dz, float maxD) {
        for (float t = 0.f; t < maxD; t += 1.f)
            if (heightAt(I, lx + dx * t, lz + dz * t) < 0.f) return t;
        return maxD;
    }
    void beachRing(float spacing, float palmP, float sc) {
        // walk the coast: palms on the beach, the shore's points for lanterns and birds
        const float circ = TAU * I.R;
        const int n = std::max(8, (int)(circ / spacing));
        for (int i = 0; i < n; i++) {
            const float a = TAU * (i + rnd() * 0.6f) / n;
            const float ca = std::cos(a), sa = std::sin(a);
            const float ci = I.type == T_VILLA || I.type == T_CAY ? 0.f : cliffAt(I, ca, sa);
            if (ci > 0.35f) continue;
            // from outside in: the first point on dry sand
            float t = I.R * 1.6f;
            while (t > 1.f && heightAt(I, ca * t, sa * t) < 0.15f) t -= 1.5f;
            if (t <= 1.f) continue;
            const float lx = ca * t, lz = sa * t;
            const vec3 shoreP(I.x + lx, 0.6f, I.z + lz);
            if (i % 2 == 0 && I.shore.size() < 80) I.shore.push_back(shoreP);
            if (i % 7 == 0 && I.birdSpots.size() < 12) I.birdSpots.push_back(shoreP + vec3(0, 2, 0));
            // (the villa's frontage stays open: its deck looks out over the beach to the sea)
            const bool front = I.padHW > 0.f && std::cos(a - I.padA) > 0.86f;
            if (rnd() < palmP && !front) {
                const float back = range(3.f, 9.f);
                palmAt(I.x + lx - ca * back, I.z + lz - sa * back, a, sc * range(0.85f, 1.2f));
            }
        }
    }
    void jungle(float spacing, float p, bool dry) {
        const float e = I.R * 1.05f;
        for (float z = -e; z < e; z += spacing)
            for (float x = -e; x < e; x += spacing) {
                if (rnd() > p) continue;
                const float lx = x + range(-0.4f, 0.4f) * spacing, lz = z + range(-0.4f, 0.4f) * spacing;
                const Ground g = groundAt(I, lx, lz);
                if (g.h < 3.f || g.veg < 0.6f || g.rock > 0.4f) continue;
                if (I.padHW > 0.f) {
                    const float dx = lx - I.padX, dz = lz - I.padZ;
                    if (dx * dx + dz * dz < (I.padHW + 10.f) * (I.padHW + 10.f)) continue;
                }
                if (dry) {
                    if (rnd() < 0.18f) cypressAt(I.x + lx, I.z + lz, range(0.8f, 1.2f));
                    else treeAt(I.x + lx, I.z + lz, range(0.7f, 1.2f), true);
                } else treeAt(I.x + lx, I.z + lz, range(0.75f, 1.35f), false);
            }
    }
};

void boatPartsStatic(Furn& f, int kind, const vec3& p, float head, const Style& st);

void lighthouse(Furn& f, const vec3& base) {
    const vec3 red(0.75f, 0.12f, 0.1f);
    const float H = 15.f;
    f.add(M_FRUSTUM, MAT_TOWER, base + vec3(0, H * 0.5f, 0), vec3(4.6f, H, 4.6f), Q{}, vec3(0.95f), 0.f);
    f.add(M_CYL, MAT_METAL, base + vec3(0, H + 0.15f, 0), vec3(4.4f, 0.3f, 4.4f), Q{}, vec3(0.12f));
    f.add(M_CYL, MAT_GLASS, base + vec3(0, H + 1.4f, 0), vec3(2.4f, 2.2f, 2.4f), Q{}, vec3(1.f), 2.f);
    f.add(M_DOME, MAT_PAINT, base + vec3(0, H + 2.5f, 0), vec3(2.8f, 2.2f, 2.8f), Q{}, red);
    f.I.lights.push_back({base + vec3(0, H + 1.4f, 0), 3.f, vec3(1.f, 0.85f, 0.6f), 3});
    f.obst(base.x, base.z, 5.f, base.y + H + 4.f);
    // the keeper's house beside it
    const float a = f.range(0.f, TAU);
    const vec3 hp = base + vec3(std::cos(a) * 6.5f, 0.f, std::sin(a) * 6.5f);
    const float g = f.groundW(hp.x, hp.z);
    const Q q = qYaw(a);
    f.add(M_BOX, MAT_RENDER, vec3(hp.x, g + 1.2f, hp.z), vec3(5.5f, 3.4f, 4.f), q, vec3(0.95f, 0.94f, 0.9f));
    f.add(M_GABLE, MAT_TILE, vec3(hp.x, g + 2.9f, hp.z), vec3(6.f, 1.6f, 4.6f), q, red * 0.9f);
    f.obst(hp.x, hp.z, 4.f, g + 5.f);
}

// a small house: walls, a roof (gable tiles or tin, a hip of thatch, or flat for the white cubes), a door
void house(Furn& f, const vec3& p, float yaw, float w, float d, float hgt, const vec3& wall, int roof, const vec3& roofCol) {
    const Q q = qYaw(yaw);
    const float g = p.y;
    f.add(M_BOX, MAT_RENDER, vec3(p.x, g + hgt * 0.5f - 0.8f, p.z), vec3(w, hgt + 1.6f, d), q, wall);
    if (roof == 0) f.add(M_GABLE, MAT_TILE, vec3(p.x, g + hgt - 0.05f, p.z), vec3(w + 0.6f, hgt * 0.45f, d + 0.6f), q, roofCol);
    else if (roof == 1) f.add(M_HIP, MAT_THATCH, vec3(p.x, g + hgt - 0.05f, p.z), vec3(w + 1.4f, hgt * 0.7f, d + 1.4f), q, roofCol);
    else f.add(M_BOX, MAT_RENDER, vec3(p.x, g + hgt + 0.1f, p.z), vec3(w + 0.1f, 0.25f, d + 0.1f), q, wall);
    // a door and a window on the front (+z)
    const vec3 fr = qRot(q, vec3(0, 0, d * 0.5f + 0.03f));
    const vec3 sd = qRot(q, vec3(1, 0, 0));
    f.add(M_BOX, MAT_PAINT, vec3(p.x, g + 0.95f, p.z) + fr + sd * (-w * 0.2f), vec3(0.9f, 1.9f, 0.08f), q, roof == 2 ? vec3(0.06f, 0.2f, 0.62f) : roofCol * 0.7f);
    f.add(M_BOX, MAT_GLASS, vec3(p.x, g + 1.5f, p.z) + fr + sd * (w * 0.22f), vec3(1.0f, 0.9f, 0.08f), q, vec3(1.f), 1.f);
    f.I.lights.push_back({vec3(p.x, g + 1.5f, p.z) + fr * 1.15f + sd * (w * 0.22f), 1.2f, vec3(1.f, 0.7f, 0.4f), 0});
    f.obst(p.x, p.z, std::max(w, d) * 0.7f, g + hgt + 2.5f);
}

void bungalow(Furn& f, const vec3& p, float yaw) {
    const Q q = qYaw(yaw);
    const float deckY = 1.7f;
    const float sb = f.groundW(p.x, p.z);
    const float postH = deckY - sb + 0.2f;
    for (int i = 0; i < 4; i++) {
        const vec3 o = qRot(q, vec3(i % 2 ? 2.8f : -2.8f, 0, i < 2 ? -2.8f : 2.8f));
        f.add(M_CYL, MAT_WOOD, vec3(p.x + o.x, sb + postH * 0.5f, p.z + o.z), vec3(0.28f, postH, 0.28f), Q{}, f.st.accentWood * 0.55f);
    }
    f.add(M_BOX, MAT_DECK, vec3(p.x, deckY, p.z), vec3(7.2f, 0.25f, 7.2f), q, f.st.accentWood);
    f.add(M_BOX, MAT_WOOD, p + vec3(0, deckY + 1.4f, 0) + qRot(q, vec3(0, 0, -0.8f)), vec3(5.f, 2.6f, 4.4f), q, f.st.accentWood * 0.85f);
    f.add(M_HIP, MAT_THATCH, p + vec3(0, deckY + 2.65f, 0) + qRot(q, vec3(0, 0, -0.6f)), vec3(7.4f, 2.6f, 6.6f), q, vec3(0.62f, 0.48f, 0.3f));
    const vec3 fr = qRot(q, vec3(0, 0, 1.45f));
    f.add(M_BOX, MAT_GLASS, p + vec3(0, deckY + 1.3f, 0) + fr, vec3(3.f, 1.8f, 0.08f), q, vec3(1.f), 1.f);
    f.I.lights.push_back({p + vec3(0, deckY + 1.3f, 0) + fr * 1.3f, 1.8f, vec3(1.f, 0.68f, 0.36f), 0});
    f.obst(p.x, p.z, 4.5f, deckY + 5.6f);
}

void boardwalk(Furn& f, const vec3& a, const vec3& b, float y, float w) {
    const vec3 d = b - a;
    const float L = std::sqrt(d.x * d.x + d.z * d.z);
    if (L < 0.5f) return;
    const float yaw = std::atan2(d.x, d.z);
    const Q q = qYaw(yaw);
    f.add(M_BOX, MAT_DECK, vec3((a.x + b.x) * 0.5f, y, (a.z + b.z) * 0.5f), vec3(w, 0.22f, L), q, f.st.accentWood);
    const int np = std::max(1, (int)(L / 4.f));
    for (int i = 0; i <= np; i++) {
        const float t = (float)i / np;
        const vec3 c = a + d * t;
        const float sb = f.groundW(c.x, c.z);
        if (sb > y - 0.3f) continue;
        for (int s = -1; s <= 1; s += 2) {
            const vec3 o = qRot(q, vec3(s * (w * 0.5f - 0.1f), 0, 0));
            f.add(M_CYL, MAT_WOOD, vec3(c.x + o.x, (sb + y) * 0.5f, c.z + o.z), vec3(0.18f, y - sb, 0.18f), Q{}, f.st.accentWood * 0.5f);
        }
    }
    f.obst((a.x + b.x) * 0.5f, (a.z + b.z) * 0.5f, std::max(2.f, L * 0.5f), y + 0.5f);
}

void villa(Furn& f) {
    Island& I = f.I;
    const float gpad = I.padH;
    const vec3 o(I.x + I.padX, gpad, I.z + I.padZ);
    const auto F = f.frame(o, I.padA);
    const vec3 white(0.96f, 0.95f, 0.92f), frameC(0.16f, 0.15f, 0.14f);
    auto box = [&](int mat, float u, float y, float v, float sx, float sy, float sz, const vec3& col, float param = 0.f) {
        const float K = 1.3f; // (a generous villa: everything at 1.3 times its plan)
        f.add(M_BOX, mat, F.at(u * K, y * K, v * K), vec3(sx * K, sy * K, sz * K), F.q, col, param);
    };
    // the house: a long white volume, an upper box over it (cantilevered toward the sea), deep roof slabs
    box(MAT_RENDER, 0.f, 1.2f, -4.f, 16.f, 4.4f, 9.f, white);
    box(MAT_GLASS, 0.6f, 1.55f, 0.48f, 14.4f, 2.9f, 0.14f, vec3(1.f), 1.f);
    for (int i = 0; i < 5; i++) box(MAT_PAINT, -6.6f + i * 3.3f, 1.55f, 0.56f, 0.12f, 2.9f, 0.16f, frameC);
    box(MAT_RENDER, 0.f, 3.55f, -3.f, 17.6f, 0.32f, 11.4f, white);
    box(MAT_RENDER, -3.f, 5.25f, -4.6f, 10.f, 3.1f, 7.6f, white);
    box(MAT_GLASS, -3.f, 5.2f, -0.78f, 9.f, 2.5f, 0.14f, vec3(1.f), 1.f);
    box(MAT_RENDER, -3.f, 6.92f, -4.2f, 11.4f, 0.3f, 9.6f, white);
    box(MAT_DECK, 5.4f, 5.25f, -4.4f, 0.2f, 3.1f, 7.2f, f.st.accentWood);  // a timber wall at the upper box's end
    // the deck around the pool (four pieces), the pool, its coping
    const vec3 wood = f.st.accentWood;
    box(MAT_DECK, 1.f, 0.15f, 2.6f, 22.f, 0.3f, 4.2f, wood);
    box(MAT_DECK, 1.f, 0.15f, 9.9f, 22.f, 0.3f, 2.6f, wood);
    box(MAT_DECK, -6.5f, 0.15f, 6.7f, 7.f, 0.3f, 3.8f, wood);
    box(MAT_DECK, 10.f, 0.15f, 6.7f, 4.f, 0.3f, 3.8f, wood);
    box(MAT_POOL, 5.f, -0.1f, 6.7f, 11.f, 0.62f, 3.8f, vec3(1.f));
    box(MAT_RENDER, 5.f, 0.22f, 4.68f, 11.6f, 0.18f, 0.3f, white);
    box(MAT_RENDER, 5.f, 0.22f, 8.72f, 11.6f, 0.18f, 0.3f, white);
    I.lights.push_back({F.at(6.5f, 0.6f, 8.7f), 7.f, vec3(0.3f, 1.f, 0.95f), 2});
    // loungers and a parasol by the pool
    for (int i = 0; i < 3; i++) f.lounger(F.at((-1.f + i * 2.2f) * 1.3f, 0.39f, 13.f), qMul(F.q, qYaw(PI)));
    f.umbrella(F.at(-5.2f, 0.39f, 12.7f), f.st.canvas[0]);
    // steps down to the garden and the beach
    for (int i = 0; i < 3; i++) box(MAT_DECK, 1.f, -0.15f - i * 0.3f, 11.5f + i * 0.9f, 6.f, 0.3f, 0.9f, wood);
    // garden: hedges along the sides
    for (int i = 0; i < 4; i++) {
        f.add(M_LUMP, MAT_FOLIAGE, F.at(-15.f, 0.4f, (-6.f + i * 4.f) * 1.3f), vec3(2.4f, 1.6f, 3.2f), F.q, vec3(1.f), 3.f);
        f.add(M_LUMP, MAT_FOLIAGE, F.at(16.2f, 0.4f, (-6.f + i * 4.f) * 1.3f), vec3(2.4f, 1.6f, 3.2f), F.q, vec3(1.f), 3.f);
    }
    // the lights on the deck
    for (int i = 0; i < 4; i++) I.lights.push_back({F.at((-9.f + i * 6.f) * 1.3f, 0.8f, 14.3f), 0.6f, vec3(1.f, 0.75f, 0.45f), 1});
    f.obst(o.x, o.z, 15.f, gpad + 9.5f);
    f.obst(F.at(0, 0, 9).x, F.at(0, 0, 9).z, 15.f, gpad + 3.f);
    I.focus = F.at(2.6f, 1.3f, 7.8f);
    // the palms around it
    for (int i = 0; i < 6; i++) {
        const float u = (i % 2 ? 1.f : -1.f) * f.range(19.f, 27.f), v = f.range(-8.f, 16.f);
        const vec3 p = F.at(u, 0, v);
        f.palmAt(p.x, p.z, I.padA + (u > 0 ? -0.5f : 0.5f), f.range(0.9f, 1.25f));
    }
    // the jetty: from the beach out over the water, a moored speedboat
    const float vs = f.toShore(I.padX, I.padZ, F.w.x, F.w.z, 120.f);
    const vec3 j0 = F.at(6.f, 0, vs - 6.f), j1 = F.at(6.f, 0, vs + 30.f);
    boardwalk(f, j0, j1, 1.2f, 2.2f);
    boardwalk(f, F.at(2.f, 0, vs + 30.f), F.at(10.f, 0, vs + 30.f), 1.2f, 3.2f);
    for (int i = 0; i < 4; i++) I.lights.push_back({F.at(6.f + (i % 2 ? 1.2f : -1.2f), 1.9f, vs + i * 9.f), 0.5f, vec3(1.f, 0.75f, 0.45f), 1});
    {
        const vec3 bp = F.at(9.f, 0, vs + 24.f);
        boatPartsStatic(f, 2, bp, std::atan2(F.w.z, F.w.x), f.st);
    }
    // umbrellas and loungers on the beach
    for (int i = 0; i < 3; i++) {
        const vec3 p = F.at(-14.f + i * 6.f, 0, vs - 4.f);
        const float g = f.groundW(p.x, p.z);
        if (g < 0.2f) continue;
        f.umbrella(vec3(p.x, g, p.z), f.st.canvas[i % 3]);
        f.lounger(vec3(p.x + F.t.x * 1.2f, g, p.z + F.t.z * 1.2f), qMul(F.q, qYaw(PI)));
    }
}

void village(Furn& f, bool cubes) {
    Island& I = f.I;
    const float ca = std::cos(I.face), sa = std::sin(I.face);
    // the houses on the slope behind the beach, facing the sea
    const int n = cubes ? 26 : 13;
    int placed = 0;
    for (int tries = 0; tries < n * 6 && placed < n; tries++) {
        const float a = I.face + f.range(-0.75f, 0.75f) * (cubes ? 0.7f : 1.f);
        const float dd = f.range(cubes ? 0.35f : 0.55f, 0.9f) * I.R;
        const float lx = std::cos(a) * dd, lz = std::sin(a) * dd;
        const Ground g = groundAt(I, lx, lz);
        if (g.h < 2.f || g.rock > 0.6f) continue;
        bool clash = false;
        const vec3 wp(I.x + lx, g.h, I.z + lz);
        for (const Obst& o : I.obst)
            if ((o.x - wp.x) * (o.x - wp.x) + (o.z - wp.z) * (o.z - wp.z) < (cubes ? 36.f : 64.f)) { clash = true; break; }
        if (clash) continue;
        const float yaw = std::atan2(std::cos(a), std::sin(a)) + f.range(-0.2f, 0.2f);
        if (cubes) house(f, wp, yaw, f.range(4.f, 6.5f), f.range(4.f, 6.f), f.range(3.f, 4.4f), vec3(0.97f, 0.96f, 0.94f), 2, vec3(0.1f));
        else {
            const bool thatchRoof = f.st.thatch;
            house(f, wp, yaw, f.range(4.f, 6.f), f.range(4.5f, 6.5f), f.range(2.8f, 3.4f), f.st.villageWalls[placed % 4], thatchRoof ? 1 : 0,
                  thatchRoof ? vec3(0.6f, 0.47f, 0.3f) : f.st.roof);
        }
        placed++;
    }
    if (cubes) {
        // the chapel with its blue dome, above the village
        const float lx = ca * I.R * 0.3f, lz = sa * I.R * 0.3f;
        const float g = heightAt(I, lx, lz);
        if (g > 2.f) {
            const vec3 p(I.x + lx, g, I.z + lz);
            f.add(M_BOX, MAT_RENDER, p + vec3(0, 1.6f, 0), vec3(7.f, 5.f, 7.f), qYaw(I.face), vec3(0.97f));
            f.add(M_CYL, MAT_RENDER, p + vec3(0, 4.4f, 0), vec3(4.6f, 1.f, 4.6f), Q{}, vec3(0.97f));
            f.add(M_DOME, MAT_PAINT, p + vec3(0, 4.85f, 0), vec3(4.6f, 4.f, 4.6f), Q{}, vec3(0.05f, 0.24f, 0.7f));
            f.add(M_BOX, MAT_RENDER, p + vec3(3.4f, 3.2f, 0), vec3(1.6f, 6.6f, 1.6f), qYaw(I.face), vec3(0.97f));
            f.obst(p.x, p.z, 6.f, g + 9.f);
        }
    }
    // the bungalows on stilts in the lagoon, along a boardwalk from the beach
    if (!cubes) {
        const float vs = f.toShore(0.f, 0.f, ca, sa, I.R * 2.f);
        const vec3 a0(I.x + ca * (vs - 8.f), 0, I.z + sa * (vs - 8.f));
        const vec3 a1(I.x + ca * (vs + 40.f), 0, I.z + sa * (vs + 40.f));
        boardwalk(f, a0, a1, 1.5f, 2.2f);
        const vec3 t(-sa, 0, ca);
        const int nb = 6 + (int)(f.rnd() * 4.f);
        const vec3 b0 = a1 - t * (nb * 6.5f);
        const vec3 b1 = a1 + t * (nb * 6.5f);
        boardwalk(f, b0, b1, 1.5f, 2.2f);
        for (int i = 0; i < nb; i++) {
            const float u = (i - (nb - 1) * 0.5f) * 13.f;
            for (int sd = -1; sd <= 1; sd += 2) {
                if ((i + (sd > 0)) % 2) continue;
                const vec3 p = a1 + t * u + vec3(ca, 0, sa) * (sd * 8.f);
                bungalow(f, p, std::atan2(ca * sd, sa * sd));
                boardwalk(f, a1 + t * u, a1 + t * u + vec3(ca, 0, sa) * (sd * 4.5f), 1.5f, 1.6f);
            }
        }
        for (int i = 0; i < 6; i++) I.lights.push_back({a0 + (a1 - a0) * (i / 5.f) + vec3(0, 2.2f, 0), 0.5f, vec3(1.f, 0.72f, 0.4f), 1});
        I.focus = a1 + vec3(0, 3.f, 0);
    } else {
        // a little harbour wall
        const float vs = f.toShore(0.f, 0.f, ca, sa, I.R * 2.f);
        boardwalk(f, vec3(I.x + ca * (vs - 4.f), 0, I.z + sa * (vs - 4.f)), vec3(I.x + ca * (vs + 22.f), 0, I.z + sa * (vs + 22.f)), 1.1f, 3.f);
        I.focus = vec3(I.x + ca * vs * 0.7f, 8.f, I.z + sa * vs * 0.7f);
    }
}

} // namespace

void furnish(Island& I, const Style& st) {
    Furn f{I, st};
    I.top = std::max(0.f, I.H);
    const float ca = std::cos(I.face), sa = std::sin(I.face);
    I.focus = vec3(I.x + ca * I.R * 0.9f, std::max(2.f, I.H * 0.3f), I.z + sa * I.R * 0.9f);
    switch (I.type) {
    case T_CAY: {
        const int np = I.R < 30.f ? 1 + (int)(f.rnd() * 2.f) : 3 + (int)(f.rnd() * 4.f);
        for (int i = 0; i < np; i++) {
            const float a = f.range(0.f, TAU), d = f.range(0.f, 0.5f) * I.R;
            f.palmAt(I.x + std::cos(a) * d, I.z + std::sin(a) * d, a, f.range(0.85f, 1.15f));
        }
        if (f.rnd() < 0.6f) {
            const float a = I.face + f.range(-0.5f, 0.5f), d = I.R * 0.55f;
            const float g = heightAt(I, std::cos(a) * d, std::sin(a) * d);
            if (g > 0.2f) f.umbrella(vec3(I.x + std::cos(a) * d, g, I.z + std::sin(a) * d), st.canvas[(int)(f.rnd() * 2.99f)]);
        }
        for (int i = 0; i < 3; i++) {
            const float a = f.range(0.f, TAU);
            I.shore.push_back(vec3(I.x + std::cos(a) * I.R, 0.5f, I.z + std::sin(a) * I.R));
        }
        I.birdSpots.push_back(vec3(I.x, 3.f, I.z));
        I.focus = vec3(I.x + ca * I.R * 0.6f, 2.f, I.z + sa * I.R * 0.6f);
        break;
    }
    case T_ATOLL: {
        // palms on the motus, densely
        const float wr = 0.13f * I.R;
        const int n = (int)(TAU * I.R / 7.f);
        for (int i = 0; i < n; i++) {
            const float a = TAU * (i + f.rnd()) / n, d = I.R + f.range(-1.2f, 1.2f) * wr;
            const float lx = std::cos(a) * d, lz = std::sin(a) * d;
            if (heightAt(I, lx, lz) < 1.f) continue;
            if (f.rnd() < 0.7f) f.palmAt(I.x + lx, I.z + lz, a + (f.rnd() < 0.5f ? 0.f : PI), f.range(0.8f, 1.2f));
            if (i % 5 == 0 && I.shore.size() < 60) I.shore.push_back(vec3(I.x + lx, 0.8f, I.z + lz));
            if (i % 23 == 0) I.birdSpots.push_back(vec3(I.x + lx, 3.f, I.z + lz));
        }
        if (I.peak) f.jungle(16.f, 0.55f, false);
        if (st.thatch) {
            // a few bungalows in the lagoon off a motu, facing the route
            const float d = I.R - wr * 1.4f;
            for (int i = 0; i < 5; i++) {
                const float a = I.face + (i - 2) * 0.035f;
                const vec3 p(I.x + std::cos(a) * d, 0, I.z + std::sin(a) * d);
                if (heightAt(I, p.x - I.x, p.z - I.z) > -0.8f) continue;
                bungalow(f, p, std::atan2(-std::cos(a), -std::sin(a)));
            }
            I.focus = vec3(I.x + ca * d, 3.f, I.z + sa * d);
        } else I.focus = vec3(I.x + ca * I.R, 2.f, I.z + sa * I.R);
        break;
    }
    case T_ISLET: {
        lighthouse(f, vec3(I.x, heightAt(I, 0.f, 0.f), I.z));
        for (int i = 0; i < 5; i++) {
            const float a = f.range(0.f, TAU), d = f.range(0.85f, 1.25f) * I.R;
            const float g = heightAt(I, std::cos(a) * d, std::sin(a) * d);
            f.add(M_LUMP, MAT_ROCK, vec3(I.x + std::cos(a) * d, g, I.z + std::sin(a) * d), vec3(f.range(3.f, 6.f), f.range(2.f, 4.f), f.range(3.f, 6.f)), qYaw(a), vec3(1.f), 1.f);
        }
        I.birdSpots.push_back(vec3(I.x, I.H + 4.f, I.z));
        I.focus = vec3(I.x, I.H * 0.7f + 6.f, I.z);
        break;
    }
    case T_VILLAGE:
        village(f, st.dry);
        if (!st.dry) f.beachRing(9.f, 0.65f, 1.f);
        else f.beachRing(12.f, 0.f, 1.f);
        f.jungle(st.dry ? 18.f : 15.f, st.dry ? 0.35f : 0.5f, st.dry);
        break;
    case T_VILLA:
        villa(f);
        f.beachRing(st.dry ? 14.f : 9.f, st.dry ? 0.f : 0.5f, 1.f);
        f.jungle(st.dry ? 16.f : 13.f, 0.45f, st.dry);
        break;
    default: // T_VOLCANO
        f.beachRing(st.dry ? 14.f : 10.f, st.dry ? 0.f : 0.6f, 1.05f);
        f.jungle(st.dry ? 20.f : 15.f, st.dry ? 0.32f : 0.55f, st.dry);
        if (I.fallW > 0.f) {
            const float a = I.fallA;
            I.focus = vec3(I.x + std::cos(a) * I.R, I.H * 0.12f + 8.f, I.z + std::sin(a) * I.R);
        }
        break;
    }
}

// =====================================================================================================
// Boats (static parts: the moored ones)
// =====================================================================================================

namespace {
void boatPartsStatic(Furn& f, int kind, const vec3& p, float head, const Style& st) {
    (void)kind;
    const Q q = qYaw(-head);
    auto at = [&](float x, float y, float z) { return p + qRot(q, vec3(x, y, z)); };
    f.add(M_HULL, MAT_HULL, at(0, 0.75f, 0), vec3(7.5f, 1.2f, 2.6f), q, vec3(0.95f), 1.f);
    f.add(M_BOX, MAT_GLASS, at(0.6f, 1.15f, 0), vec3(0.5f, 0.55f, 1.9f), qMul(q, qAxis(vec3(0, 0, 1), 0.6f)), vec3(1.f), 0.f);
    f.add(M_BOX, MAT_PAINT, at(-1.2f, 1.0f, 0), vec3(2.f, 0.45f, 1.9f), q, st.canvas[1]);
}
} // namespace

// =====================================================================================================
// The world: route, heroes, fillers, streaming
// =====================================================================================================

World::World() { worker_ = std::thread([this] { work(); }); }
World::~World() {
    {
        std::lock_guard<std::mutex> l(mu_);
        quit_ = true;
        jobs_.clear();
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void World::work() {
    for (;;) {
        Job j;
        {
            std::unique_lock<std::mutex> l(mu_);
            cv_.wait(l, [&] { return quit_ || !jobs_.empty(); });
            if (quit_) return;
            j = jobs_.front();
            jobs_.pop_front();
        }
        std::vector<float> v = buildTerrain(*j.isl, LOD_N[j.lod]);
        std::lock_guard<std::mutex> l(mu_);
        done_.push_back({j.isl, {j.lod, std::move(v)}});
    }
}

void World::requestMesh(const std::shared_ptr<Island>& I, int lod) {
    if (I->busy) return;
    I->busy = true;
    I->jobLod = lod;
    std::lock_guard<std::mutex> l(mu_);
    // nearer requests first: big ones go to the back, small ones to the front
    if (lod <= 1) jobs_.push_front({I, lod});
    else jobs_.push_back({I, lod});
    cv_.notify_one();
}

void World::collectReady(std::vector<std::shared_ptr<Island>>& out) {
    std::lock_guard<std::mutex> l(mu_);
    for (auto& d : done_) {
        d.first->ready = std::move(d.second.second);
        d.first->readyLod = d.second.first;
        d.first->busy = false;
        out.push_back(d.first);
    }
    done_.clear();
}

uint32_t World::nextSeed() { return hashu((uint32_t)seed_ * 2654435761u + (seedCounter_++) * 40503u + 7u); }

void World::reset(uint64_t seed, const Style& st) {
    {
        std::lock_guard<std::mutex> l(mu_);
        jobs_.clear();
    }
    st_ = st;
    seed_ = seed ? seed : 1;
    seedCounter_ = 1;
    islands.clear();
    heroes.clear();
    pts.clear();
    cells_.clear();
    ox = oz = 0;
    g_ = Gen{};
    g_.base = 0.f;
    g_.head = 0.f;
    g_.p1 = hashf((uint32_t)seed_) * TAU;
    g_.p2 = hashf((uint32_t)seed_ + 9u) * TAU;
    g_.nextHero = 380.0;
    g_.bag = (int)(hashf((uint32_t)seed_ + 3u) * 3.f);
    pts.push_back({0.f, 0.f, 0.f, 0.f, 0.0});
    extendRoute(3000.0);
}

void World::stepRoute() {
    const float ds = 8.f;
    float kT;
    if (g_.orbitLeft > 0) {
        kT = g_.orbitK;
        g_.orbitLeft--;
    } else {
        const double s = g_.s;
        const float desired = g_.base + 0.55f * std::sin((float)(s / 2600.0) + g_.p1) + 0.3f * std::sin((float)(s / 1150.0) + g_.p2);
        float err = desired - g_.head;
        while (err > PI) err -= TAU;
        while (err < -PI) err += TAU;
        kT = std::clamp(err * 0.0035f, -1.f / 800.f, 1.f / 800.f);
    }
    // the curvature eases in and out (over ~70 m in the loops, ~120 m elsewhere): no kink in the turns
    const float maxD = ds * std::max(std::max(std::fabs(g_.curv), std::fabs(kT)) / 70.f, 1.f / 800.f / 120.f);
    g_.curv += std::clamp(kT - g_.curv, -maxD, maxD);
    g_.head += g_.curv * ds;
    g_.x += std::cos(g_.head) * ds;
    g_.z += std::sin(g_.head) * ds;
    g_.s += ds;
    pts.push_back({g_.x, g_.z, g_.head, g_.curv, g_.s});
}

bool World::clearOf(float x, float z, float r, bool routeToo) const {
    for (const auto& I : islands) {
        const float dx = I->x - x, dz = I->z - z;
        const float rr = r + I->R * (I->type == T_CAY ? 2.4f : 1.25f) + 40.f;
        if (dx * dx + dz * dz < rr * rr) return false;
    }
    if (routeToo)
        for (size_t i = 0; i < pts.size(); i += 4) {
            const float dx = pts[i].x - x, dz = pts[i].z - z;
            if (dx * dx + dz * dz < r * r) return false;
        }
    return true;
}

std::shared_ptr<Island> World::makeIsland(int type, float x, float z, uint32_t seed, bool hero, float face, float sizeK, bool content) {
    auto I = std::make_shared<Island>();
    I->type = type;
    I->x = x;
    I->z = z;
    I->seed = seed;
    I->hero = hero;
    I->face = face;
    I->dry = st_.dry;
    auto r = [&](uint32_t k) { return hashf(seed * 31u + k); };
    switch (type) {
    case T_VOLCANO:
        I->R = lerpf(240.f, 520.f, r(1)) * sizeK;
        I->H = lerpf(140.f, 380.f, r(2)) * sizeK * (st_.dry ? 0.55f : 1.f);
        I->cliffK = st_.dry ? 1.2f : 0.95f;
        I->shelfW = st_.dry ? 0.45f : 1.f;
        I->ext = I->R * 1.65f;
        break;
    case T_CAY:
        I->R = lerpf(18.f, 60.f, r(1)) * sizeK;
        I->H = 1.6f;
        I->ext = I->R * 4.6f;
        break;
    case T_ATOLL:
        I->R = lerpf(220.f, 430.f, r(1)) * sizeK;
        I->H = lerpf(90.f, 200.f, r(2));
        I->peak = st_.thatch && r(3) < 0.75f;
        I->ext = I->R * 1.5f;
        break;
    case T_ISLET:
        I->R = lerpf(22.f, 38.f, r(1));
        I->H = lerpf(13.f, 24.f, r(2));
        I->ext = I->R * 2.5f;
        break;
    case T_VILLAGE:
        I->R = lerpf(150.f, 230.f, r(1)) * sizeK;
        I->H = lerpf(26.f, 60.f, r(2)) * (st_.dry ? 1.4f : 1.f);
        I->cliffK = st_.dry ? 0.8f : 0.35f;
        I->shelfW = st_.dry ? 0.6f : 1.2f;
        I->ext = I->R * 1.85f;
        break;
    default: // T_VILLA
        I->R = lerpf(80.f, 120.f, r(1)) * sizeK;
        I->H = lerpf(8.f, 16.f, r(2));
        I->cliffK = 0.f;
        I->shelfW = st_.dry ? 0.7f : 1.3f;
        I->ext = I->R * 2.2f;
        break;
    }
    if (type == T_VILLA) {
        // the pad: on the face side, near the beach
        const float d = I->R * 0.62f;
        I->padX = std::cos(face) * d;
        I->padZ = std::sin(face) * d;
        I->padA = face;
        I->padHW = 21.f;
        I->padHD = 18.f;
        I->padH = 0.f;
        I->padH = std::max(1.5f, heightAt(*I, I->padX, I->padZ));
        I->padHW = 21.f;
    }
    if (type == T_VOLCANO) {
        // a waterfall down the cliff nearest the face
        float best = 0.f, ba = face;
        for (int k = -16; k <= 16; k++) {
            const float a = face + k * 0.045f;
            const float c = cliffAt(*I, std::cos(a), std::sin(a));
            if (c > best) { best = c; ba = a; }
        }
        if (best > 0.65f) {
            I->fallA = ba;
            I->fallW = 7.f / I->R;
        }
    }
    if (content) furnish(*I, st_);
    return I;
}

void World::placeHero(int kind) {
    const float hx = g_.x, hz = g_.z, hh = g_.head;
    const vec3 nrm(-std::sin(hh), 0, std::cos(hh)); // left of the heading
    const float side = (hashf(nextSeed()) < 0.5f) ? 1.f : -1.f;
    Hero h;
    h.side = side;
    h.sc = g_.s;
    auto faceTo = [&](float ix, float iz) { return std::atan2(hz - iz, hx - ix); };
    if (kind == 200) {
        // a gap: two islands either side, the route between
        const uint32_t s1 = nextSeed(), s2 = nextSeed();
        auto mk = [&](uint32_t sd, float sgn) {
            auto tmp = makeIsland(T_VOLCANO, 0, 0, sd, true, 0.f, 0.8f, false);
            const float off = tmp->R * 1.08f + 55.f;
            const float ix = hx + nrm.x * off * sgn, iz = hz + nrm.z * off * sgn;
            return makeIsland(T_VOLCANO, ix, iz, sd, true, faceTo(ix, iz), 0.8f);
        };
        h.isl = mk(s1, 1.f);
        h.isl2 = mk(s2, -1.f);
        h.gap = true;
        islands.push_back(h.isl);
        islands.push_back(h.isl2);
        const float L = std::max(h.isl->R, h.isl2->R);
        h.s0 = g_.s - L * 1.2 - 300.0;
        h.s1 = g_.s + L * 1.2 + 200.0;
        heroes.push_back(h);
        g_.nextHero = g_.s + L * 1.4 + 700.0;
        return;
    }
    const bool orbit = kind >= 100;
    const int type = kind % 100;
    const uint32_t sd = nextSeed();
    auto tmp = makeIsland(type, 0, 0, sd, true, 0.f, type == T_VOLCANO ? 0.75f : 1.f, false);
    float off;
    if (orbit) {
        const float land = tmp->R * (type == T_CAY ? 1.6f : type == T_ATOLL ? 1.12f : 1.05f);
        const float r = land + (type == T_ISLET ? 170.f : 200.f);
        off = r;
        g_.orbitK = side / r;
        g_.orbitLeft = (int)(r * TAU * 0.62f / 8.f);
        h.orbit = true;
    } else {
        switch (type) {
        case T_VILLA: off = tmp->R * 0.62f + 42.f; break;
        case T_CAY: off = tmp->R * 0.5f; break;
        case T_ATOLL: off = tmp->R * 0.4f; break;
        case T_ISLET: off = tmp->R + 45.f; break;
        case T_VILLAGE: off = tmp->R * 1.15f + 70.f; break;
        default: off = tmp->R * 1.08f + 110.f; break;
        }
    }
    const float ix = hx + nrm.x * off * side, iz = hz + nrm.z * off * side;
    h.isl = makeIsland(type, ix, iz, sd, true, faceTo(ix, iz), type == T_VOLCANO ? 0.75f : 1.f);
    if (type == T_ATOLL || type == T_CAY) {
        // nothing tall right on the route: palms in the corridor go
        auto& parts = h.isl->parts;
        auto& ob = h.isl->obst;
        auto nearRoute = [&](float x, float z) {
            // the route here is nearly straight along the heading
            const float rx = x - hx, rz = z - hz;
            const float lat = std::fabs(rx * nrm.x + rz * nrm.z);
            return lat < 16.f;
        };
        parts.erase(std::remove_if(parts.begin(), parts.end(), [&](const Part& p) { return p.mesh >= M_PALM0 && p.mesh <= M_PALM2 && nearRoute(p.in.a[0], p.in.a[2]); }), parts.end());
        ob.erase(std::remove_if(ob.begin(), ob.end(), [&](const Obst& o) { return nearRoute(o.x, o.z); }), ob.end());
    }
    islands.push_back(h.isl);
    const float L = h.isl->ext;
    if (orbit) {
        h.s0 = g_.s - 400.0;
        h.s1 = g_.s + off * TAU * 0.62f + 150.0;
        g_.nextHero = h.s1 + 600.0;
    } else {
        h.s0 = g_.s - std::max(L, 250.f) - 250.0;
        h.s1 = g_.s + std::max(L, 150.f) * 0.6f + 120.0;
        g_.nextHero = g_.s + std::max(L, 200.f) + 650.0 + 300.0 * hashf(sd + 5u);
    }
    heroes.push_back(h);
}

void World::extendRoute(double toS) {
    while (g_.s < toS) {
        // hold the heading while passing a hero (so the route does not turn into it)
        bool hold = false;
        for (int i = (int)heroes.size() - 1; i >= 0 && i >= (int)heroes.size() - 2; i--)
            if (!heroes[i].orbit && g_.s < heroes[i].s1 && g_.s > heroes[i].sc) hold = true;
        if (hold && g_.orbitLeft <= 0) {
            g_.curv *= 0.97f;
            g_.head += g_.curv * 8.f;
            g_.x += std::cos(g_.head) * 8.f;
            g_.z += std::sin(g_.head) * 8.f;
            g_.s += 8.0;
            pts.push_back({g_.x, g_.z, g_.head, g_.curv, g_.s});
        } else stepRoute();
        if (g_.s >= g_.nextHero && g_.orbitLeft <= 0) {
            int kind = -1;
            for (int t = 0; t < 8 && kind < 0; t++) {
                kind = st_.heroBag[g_.bag % 8];
                g_.bag++;
            }
            if (kind < 0) kind = T_VILLA;
            placeHero(kind);
        }
    }
}

RoutePt World::route(double s) const {
    if (pts.empty()) return {0, 0, 0, 0, s};
    if (s <= pts.front().s) return pts.front();
    if (s >= pts.back().s) return pts.back();
    const double ds = pts[1].s - pts[0].s;
    size_t i = (size_t)((s - pts.front().s) / ds);
    i = std::min(i, pts.size() - 2);
    const RoutePt& a = pts[i];
    const RoutePt& b = pts[i + 1];
    const float t = (float)((s - a.s) / std::max(1e-6, b.s - a.s));
    // a cubic Hermite through the points with their headings (C1 positions)
    const float L = (float)(b.s - a.s);
    const float t2 = t * t, t3 = t2 * t;
    const float h00 = 2 * t3 - 3 * t2 + 1, h10 = t3 - 2 * t2 + t, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
    RoutePt r;
    r.x = h00 * a.x + h10 * L * std::cos(a.head) + h01 * b.x + h11 * L * std::cos(b.head);
    r.z = h00 * a.z + h10 * L * std::sin(a.head) + h01 * b.z + h11 * L * std::sin(b.head);
    r.head = lerpf(a.head, b.head, t);
    r.curv = lerpf(a.curv, b.curv, t);
    r.s = s;
    return r;
}

float World::terrain(float x, float z) const {
    float h = -45.f;
    for (const auto& I : islands) {
        const float lx = x - I->x, lz = z - I->z;
        if (std::fabs(lx) > I->ext || std::fabs(lz) > I->ext) continue;
        h = std::max(h, heightAt(*I, lx, lz));
    }
    return h;
}

float World::obstacle(float x, float z) const {
    float h = 0.f;
    for (const auto& I : islands) {
        const float lx = x - I->x, lz = z - I->z;
        if (std::fabs(lx) > I->ext || std::fabs(lz) > I->ext) continue;
        h = std::max(h, heightAt(*I, lx, lz));
        for (const Obst& o : I->obst) {
            const float dx = o.x - x, dz = o.z - z;
            if (dx * dx + dz * dz < o.r * o.r) h = std::max(h, o.top);
        }
    }
    return h;
}

void World::rebase(float dx, float dz) {
    ox += dx;
    oz += dz;
    for (auto& I : islands) {
        I->x -= dx;
        I->z -= dz;
        I->focus.x -= dx;
        I->focus.z -= dz;
        for (Part& p : I->parts) { p.in.a[0] -= dx; p.in.a[2] -= dz; }
        for (Obst& o : I->obst) { o.x -= dx; o.z -= dz; }
        for (Light& l : I->lights) { l.p.x -= dx; l.p.z -= dz; }
        for (vec3& p : I->shore) { p.x -= dx; p.z -= dz; }
        for (vec3& p : I->birdSpots) { p.x -= dx; p.z -= dz; }
    }
    for (RoutePt& p : pts) { p.x -= dx; p.z -= dz; }
    g_.x -= dx;
    g_.z -= dz;
}

const Hero* World::heroNear(double s, double ahead, double behind) const {
    const Hero* best = nullptr;
    double bd = 1e18;
    for (const Hero& h : heroes) {
        if (s < h.s0 - ahead || s > h.s1 + behind) continue;
        const double d = std::fabs(h.sc - s);
        if (d < bd) { bd = d; best = &h; }
    }
    return best;
}

std::shared_ptr<Island> World::nearestIsland(const vec3& p, float maxDist) const {
    std::shared_ptr<Island> best;
    float bd = maxDist * maxDist;
    for (const auto& I : islands) {
        const float dx = I->x - p.x, dz = I->z - p.z;
        const float d = std::max(0.f, std::sqrt(dx * dx + dz * dz) - I->R);
        if (d * d < bd) { bd = d * d; best = I; }
    }
    return best;
}

void World::fillCells(const vec3& cam, float radius) {
    const double C = 1150.0;
    const double ax = cam.x + ox, az = cam.z + oz;
    const long long i0 = (long long)std::floor((ax - radius) / C), i1 = (long long)std::floor((ax + radius) / C);
    const long long j0 = (long long)std::floor((az - radius) / C), j1 = (long long)std::floor((az + radius) / C);
    int made = 0;
    for (long long j = j0; j <= j1; j++)
        for (long long i = i0; i <= i1; i++) {
            const auto key = std::make_pair(i, j);
            if (cells_.count(key)) continue;
            const double cx = (i + 0.5) * C - ox, cz = (j + 0.5) * C - oz;
            if ((cx - cam.x) * (cx - cam.x) + (cz - cam.z) * (cz - cam.z) > (double)radius * radius) continue;
            if (made >= 6) return; // a few per frame
            cells_.insert(key);
            const uint32_t hs = hashu((uint32_t)(i * 73856093LL) ^ (uint32_t)(j * 19349663LL) ^ (uint32_t)seed_);
            if (hashf(hs) > st_.fillP) continue;
            // the type, by the variant's weights
            float sum = 0.f;
            for (float w : st_.w) sum += w;
            float pick = hashf(hs + 1u) * sum;
            int type = 0;
            for (int t = 0; t < T_COUNT; t++) {
                pick -= st_.w[t];
                if (pick <= 0.f) { type = t; break; }
            }
            const float x = (float)(cx + (hashf(hs + 2u) - 0.5f) * C * 0.7), z = (float)(cz + (hashf(hs + 3u) - 0.5f) * C * 0.7);
            const float sizeK = 0.8f + 0.4f * hashf(hs + 4u);
            const float probeR = type == T_VOLCANO ? 520.f * sizeK : type == T_ATOLL ? 430.f * sizeK : type == T_VILLAGE ? 230.f : type == T_VILLA ? 120.f : 60.f;
            const float probeLand = type == T_CAY ? probeR * 1.8f : type == T_ATOLL ? probeR * 1.15f : probeR * 1.1f;
            if (!clearOf(x, z, probeLand + 90.f, true)) continue;
            // the face: toward the camera's general direction of travel (the route)
            auto I = makeIsland(type, x, z, hs, false, hashf(hs + 6u) * TAU, sizeK);
            islands.push_back(I);
            made++;
        }
}

void World::update(const vec3& cam, double s, float viewDist) {
    extendRoute(s + 32000.0);
    // forget the route far behind
    while (pts.size() > 2 && pts.front().s < s - 4000.0) pts.pop_front();
    while (!heroes.empty() && heroes.front().s1 < s - 6000.0) heroes.erase(heroes.begin());
    fillCells(cam, std::max(9000.f, viewDist));
    // drop the islands far away (not the heroes ahead)
    const float far = std::max(12000.f, viewDist * 1.3f);
    islands.erase(std::remove_if(islands.begin(), islands.end(), [&](const std::shared_ptr<Island>& I) {
                      if (I->busy) return false;
                      const float dx = I->x - cam.x, dz = I->z - cam.z;
                      if (dx * dx + dz * dz < far * far) return false;
                      return !I->hero || I.use_count() <= 1; // (a hero stays while the route still holds it)
                  }),
                  islands.end());
    // the cells far away can be visited again
    if (cells_.size() > 4000) {
        const double ax = cam.x + ox, az = cam.z + oz;
        for (auto it = cells_.begin(); it != cells_.end();) {
            const double cx = (it->first + 0.5) * 1150.0, cz = (it->second + 0.5) * 1150.0;
            if ((cx - ax) * (cx - ax) + (cz - az) * (cz - az) > 30000.0 * 30000.0) it = cells_.erase(it);
            else ++it;
        }
    }
}

} // namespace isl
