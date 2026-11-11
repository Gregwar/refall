// CORAL REEF's procedural world: the seabed's height (the same function as REEF_GLSL in shaders.hpp), and the
// meshes, built once: the coral forms (rounded brain corals, antler-like staghorn branches with pale tips, table
// corals on a stem, lacy sea fans, clusters of tube sponges, anemones with swaying tentacles, sea whips), the fish
// (a lathe body per species, a tail and fins as thin sheets) and the manta. Everything is smooth and organic:
// tapered tubes with rounded tips, lathes and parametric sheets with their normals taken from the surface itself.
#include "universes/coralreef/coralreef.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace reef {

namespace {
float sq(float x) { return x * x; }
float smooth01(float a, float b, float x) { return smoothstepf(a, b, x); }

// One outcrop: a rounded rock mound with an irregular outline and lumps on its top.
float moundH(float x, float z, const Mound& m) {
    const float dx = x - m.x, dz = z - m.z;
    const float d = std::sqrt(dx * dx + dz * dz) / m.r;
    if (d > 1.6f) return 0.f;
    const float a = std::atan2(dz, dx);
    const float edge = 1.f + 0.18f * std::sin(3.f * a + m.seed * 6.f) + 0.1f * std::sin(5.f * a + m.seed * 11.f);
    const float q = d / edge;
    const float prof = 1.f - smooth01(0.15f, 1.f, q);
    const float lumps = 1.f + 0.16f * std::sin(dx * 2.3f / m.r + m.seed * 3.f) * std::sin(dz * 1.9f / m.r + m.seed * 5.f) +
                        0.09f * std::sin(dx * 5.3f / m.r + m.seed * 7.f) * std::sin(dz * 4.7f / m.r + m.seed * 2.f) * smooth01(1.f, 0.4f, q);
    return m.h * prof * lumps;
}
} // namespace

float groundH(float x, float z, const Mound* m, int n) {
    float h = std::max(-0.045f * std::max(0.f, -z - 4.f), -3.5f);
    h += 0.22f * std::sin(0.23f * x + 0.11f * z + 0.5f) * std::sin(0.19f * z - 0.08f * x + 1.3f) +
         0.1f * std::sin(0.53f * x + 0.9f) * std::sin(0.41f * z + 0.2f);
    for (int i = 0; i < n; i++) h += moundH(x, z, m[i]);
    return h;
}

float rockAt(float x, float z, const Mound* m, int n) {
    float s = 0.f;
    for (int i = 0; i < n; i++) s += moundH(x, z, m[i]);
    return smooth01(0.1f, 0.4f, s);
}

// =====================================================================================================
// Mesh helpers
// =====================================================================================================

namespace {

void vert(Mesh& m, const vec3& p, const vec3& n, float along, float param, float part, float phase) {
    const float f[10] = {p.x, p.y, p.z, n.x, n.y, n.z, along, param, part, phase};
    m.v.insert(m.v.end(), f, f + 10);
}

// A parametric surface on a (U+1) x (V+1) grid of u, v in [0, 1]: its normals from the surface itself (central
// differences), facing cross(dP/du, dP/dv) (or the other way with flip, the winding following).
using PosFn = std::function<vec3(float, float)>;
using AttrFn = std::function<vec4(float, float)>; // along, param, part, phase
void surface(Mesh& m, int U, int V, const PosFn& P, const AttrFn& A, bool flip) {
    const uint32_t base = (uint32_t)m.vertices();
    const float e = 1e-3f;
    for (int j = 0; j <= V; j++)
        for (int i = 0; i <= U; i++) {
            const float u = (float)i / U, v = (float)j / V;
            const float vc = std::clamp(v, 2.f * e, 1.f - 2.f * e), uc = std::clamp(u, 2.f * e, 1.f - 2.f * e);
            const vec3 du = P(uc + e, vc) - P(uc - e, vc);
            const vec3 dv = P(uc, vc + e) - P(uc, vc - e);
            vec3 n = normalize(cross(du, dv));
            if (flip) n = n * -1.f;
            const vec4 a = A(u, v);
            vert(m, P(u, v), n, a.x, a.y, a.z, a.w);
        }
    for (int j = 0; j < V; j++)
        for (int i = 0; i < U; i++) {
            const uint32_t a = base + j * (U + 1) + i, b = a + 1, c = a + (U + 1), d = c + 1;
            if (!flip) { m.idx.insert(m.idx.end(), {a, b, c, b, d, c}); }
            else { m.idx.insert(m.idx.end(), {a, c, b, b, c, d}); }
        }
}

// A tube along a path (tapering radii), rounded at its end; `along` per point.
void tube(Mesh& m, const std::vector<vec3>& pts, const std::vector<float>& rad, const std::vector<float>& along, int sides,
          float part, float phase, bool cap) {
    const int n = (int)pts.size();
    if (n < 2) return;
    const uint32_t base = (uint32_t)m.vertices();
    vec3 t0 = normalize(pts[1] - pts[0]);
    vec3 N = normalize(cross(t0, std::fabs(t0.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    int rings = 0;
    vec3 lastT = t0;
    for (int i = 0; i < n; i++) {
        const vec3 t = normalize(i == 0 ? pts[1] - pts[0] : i == n - 1 ? pts[n - 1] - pts[n - 2] : pts[i + 1] - pts[i - 1]);
        // parallel transport of the frame
        N = normalize(N - t * dot(N, t));
        const vec3 B = cross(t, N);
        for (int s = 0; s <= sides; s++) {
            const float a = TAU * s / sides;
            const vec3 d = N * std::cos(a) + B * std::sin(a);
            vert(m, pts[i] + d * rad[i], d, along[i], (float)s / sides, part, phase);
        }
        rings++;
        lastT = t;
    }
    if (cap) {
        const vec3 t = lastT;
        N = normalize(N - t * dot(N, t));
        const vec3 B = cross(t, N);
        const float r = rad[n - 1];
        for (int k = 1; k <= 3; k++) {
            const float ph = (PI * 0.5f) * k / 3.f;
            const float rr = r * std::cos(ph);
            for (int s = 0; s <= sides; s++) {
                const float a = TAU * s / sides;
                const vec3 d = N * std::cos(a) + B * std::sin(a);
                const vec3 nn = normalize(d * std::cos(ph) + t * std::sin(ph));
                vert(m, pts[n - 1] + t * (r * std::sin(ph)) + d * rr, nn, along[n - 1], (float)s / sides, part, phase);
            }
            rings++;
        }
    }
    for (int r = 0; r + 1 < rings; r++)
        for (int s = 0; s < sides; s++) {
            const uint32_t a = base + r * (sides + 1) + s, b = a + 1, c = a + (sides + 1), d = c + 1;
            m.idx.insert(m.idx.end(), {a, b, c, b, d, c});
        }
}

// Rotates p (around the origin) so that +y points along `up`.
vec3 tiltTo(const vec3& p, const vec3& up) {
    const vec3 y(0, 1, 0);
    const vec3 ax = cross(y, up);
    const float s = length(ax), c = dot(y, up);
    if (s < 1e-5f) return p;
    const vec3 k = ax / s;
    // Rodrigues
    return p * c + cross(k, p) * s + k * (dot(k, p) * (1.f - c));
}

// ---------------------------------------------------------------------------------------------- corals

// A brain coral: a dome of rounded lobes (the meandering grooves are drawn by the shader on its surface).
void brain(Mesh& m, Rng& r, int variant) {
    const float wide = variant == 1 ? 1.35f : 1.f, lobe = variant == 2 ? 0.16f : 0.06f;
    const float p1 = r.range(0.f, TAU), p2 = r.range(0.f, TAU);
    surface(m, 48, 22,
            [&](float u, float v) {
                const float th = u * TAU, ph = lerpf(-0.32f * PI, 0.5f * PI, v);
                const float rr = 0.5f * (1.f + lobe * std::sin(2.f * th + p1) * std::cos(ph) + 0.05f * std::sin(3.f * th + 2.f * ph + p2));
                return vec3(std::cos(ph) * std::cos(th) * rr * wide, std::sin(ph) * rr * 0.95f + 0.15f, std::cos(ph) * std::sin(th) * rr);
            },
            [&](float, float v) { return vec4(v, 0.f, 0.f, 0.f); }, true);
}

// A staghorn coral: antler-like branches forking up and out, tapering to rounded pale tips.
void stagBranch(Mesh& m, Rng& r, vec3 p, vec3 dir, float len, float r0, int depth, float along0, float alongK) {
    const int n = 6;
    std::vector<vec3> pts;
    std::vector<float> rad, al;
    const float r1 = depth >= 2 ? std::max(0.024f, r0 * 0.62f) : r0 * 0.75f;
    pts.push_back(p);
    rad.push_back(r0);
    al.push_back(along0);
    const vec3 bend(r.range(-0.12f, 0.12f), 0.f, r.range(-0.12f, 0.12f));
    for (int i = 1; i <= n; i++) {
        dir = normalize(dir + vec3(0.f, 0.07f, 0.f) + bend * 0.5f + vec3(r.range(-0.06f, 0.06f), 0.f, r.range(-0.06f, 0.06f)));
        p = p + dir * (len / n);
        pts.push_back(p);
        rad.push_back(lerpf(r0, r1, (float)i / n));
        al.push_back(along0 + alongK * len * i / n);
    }
    tube(m, pts, rad, al, 8, 0.f, r.uniform(), true);
    if (depth >= 2) return;
    const int kids = r.chance(0.35f) ? 3 : 2;
    for (int k = 0; k < kids; k++) {
        const int at = r.irange(3, n - 1);
        vec3 axis = normalize(cross(dir, vec3(r.range(-1.f, 1.f), r.range(-0.3f, 0.3f), r.range(-1.f, 1.f))));
        const float ang = r.range(0.45f, 0.8f);
        // rotate dir about axis
        const vec3 d2 = normalize(dir * std::cos(ang) + cross(axis, dir) * std::sin(ang) + axis * (dot(axis, dir) * (1.f - std::cos(ang))));
        stagBranch(m, r, pts[at], normalize(d2 + vec3(0.f, 0.25f, 0.f)), len * r.range(0.55f, 0.75f), rad[at] * 0.85f, depth + 1, al[at],
                   alongK);
    }
}
void staghorn(Mesh& m, Rng& r, int variant) {
    const int trunks = 6 + variant + r.irange(0, 2);
    for (int i = 0; i < trunks; i++) {
        const float a = TAU * (i + r.range(-0.3f, 0.3f)) / trunks;
        const float lean = r.range(0.25f, 0.75f);
        const vec3 dir = normalize(vec3(std::cos(a) * std::sin(lean), std::cos(lean), std::sin(a) * std::sin(lean)));
        stagBranch(m, r, vec3(std::cos(a) * 0.06f, -0.05f, std::sin(a) * 0.06f), dir, r.range(0.4f, 0.55f), 0.07f, 0, 0.f, 1.f / 1.1f);
    }
}

// A table coral: a broad plate with a wavy rim on a short stem.
void table(Mesh& m, Rng& r, int variant) {
    const float p1 = r.range(0.f, TAU), p2 = r.range(0.f, TAU), tilt = 0.05f + 0.04f * variant;
    // the stem
    surface(m, 16, 6,
            [&](float u, float v) {
                const float th = u * TAU, rr = lerpf(0.13f, 0.09f, v) + 0.08f * sq(v);
                return vec3(std::cos(th) * rr, -0.05f + v * 0.42f, std::sin(th) * rr);
            },
            [&](float, float v) { return vec4(0.1f * v, 0.f, 1.f, 0.f); }, true);
    // the plate: v 0 .. 0.5 its top from the middle out to the rim, 0.5 .. 1 back under it
    auto R = [&](float th) { return 1.f + 0.1f * std::sin(5.f * th + p1) + 0.06f * std::sin(9.f * th + p2) + 0.08f * std::sin(2.f * th + p2); };
    surface(m, 72, 20,
            [&](float u, float v) {
                const float th = u * TAU;
                const bool top = v <= 0.5f;
                const float rr = (top ? v * 2.f : (1.f - v) * 2.f);
                const float rad = R(th) * (0.06f + 0.94f * std::sqrt(rr));
                float y = 0.4f + 0.09f * std::pow(rr, 1.6f) + tilt * std::cos(th) * rr + 0.02f * std::sin(7.f * th + p1) * rr;
                if (!top) y -= 0.05f * (1.f - 0.6f * rr);
                return vec3(std::cos(th) * rad, y, std::sin(th) * rad);
            },
            [&](float u, float v) {
                const bool top = v <= 0.5f;
                return vec4(top ? v * 2.f : (1.f - v) * 2.f, u, top ? 0.f : 1.f, 0.f);
            },
            true);
}

// A sea fan: a broad lacy sheet on a short stem (the shader cuts its lattice), gently curved.
void seafan(Mesh& m, Rng& r, int variant) {
    const float curve = r.range(0.1f, 0.22f) * (r.chance(0.5f) ? 1.f : -1.f), spread = 0.9f + 0.12f * variant;
    surface(m, 28, 18,
            [&](float u, float v) {
                const float a = (u - 0.5f) * 2.f * spread;
                const float rr = 0.06f + 0.94f * v;
                const float R = 1.f - 0.22f * sq(a / spread);
                return vec3(std::sin(a) * rr * R * 0.9f, 0.08f + std::cos(a) * rr * R, curve * sq(rr) + 0.04f * std::sin(a * 3.f) * rr);
            },
            [&](float u, float v) { return vec4(0.06f + 0.94f * v, (u - 0.5f) * 2.f * spread, 0.f, 0.f); }, false);
    // the stem
    std::vector<vec3> pts = {{0.f, -0.05f, 0.f}, {0.f, 0.05f, 0.f}, {0.f, 0.14f, curve * 0.005f}};
    tube(m, pts, {0.03f, 0.026f, 0.02f}, {0.f, 0.03f, 0.08f}, 8, 1.f, 0.f, true);
}

// A cluster of tube sponges: barrels with a thick lip, dark inside.
void sponges(Mesh& m, Rng& r, int variant) {
    const int n = 3 + variant + r.irange(0, 1);
    for (int i = 0; i < n; i++) {
        const float a = TAU * i / n + r.range(-0.4f, 0.4f), d = i == 0 ? 0.f : r.range(0.12f, 0.2f);
        const vec3 at(std::cos(a) * d, -0.06f, std::sin(a) * d);
        const vec3 up = normalize(vec3(std::cos(a) * d * 1.3f, 1.f, std::sin(a) * d * 1.3f));
        const float H = (i == 0 ? 1.f : r.range(0.55f, 0.85f)) * 0.95f, R0 = r.range(0.085f, 0.11f);
        const float ph = r.range(0.f, TAU);
        surface(m, 24, 26,
                [&](float u, float v) {
                    const float th = u * TAU;
                    float rr, y;
                    if (v < 0.72f) { // outside, from the foot up to the lip
                        const float t = v / 0.72f;
                        rr = R0 * (0.8f + 0.25f * std::sin(t * PI * 0.8f) + 0.18f * t);
                        rr *= 1.f + 0.07f * std::sin(6.f * th + 9.f * t + ph);
                        y = t * H;
                    } else if (v < 0.84f) { // over the lip
                        const float t = (v - 0.72f) / 0.12f;
                        const float R1 = R0 * 1.18f, lip = R0 * 0.22f;
                        rr = R1 - lip + lip * std::cos(t * PI);
                        y = H + lip * std::sin(t * PI) * 0.7f;
                        rr *= 1.f + 0.07f * std::sin(6.f * th + 9.f + ph) * (1.f - t);
                    } else { // inside, down into the dark
                        const float t = (v - 0.84f) / 0.16f;
                        rr = R0 * 1.18f - R0 * 0.44f - R0 * 0.1f * t;
                        y = H - t * H * 0.55f;
                    }
                    return at + tiltTo(vec3(std::cos(th) * rr, y, std::sin(th) * rr), up);
                },
                [&](float, float v) { return vec4(v < 0.84f ? std::min(v / 0.72f, 1.f) : 1.f, 0.f, v < 0.8f ? 0.f : 1.f, 0.f); }, true);
    }
}

// An anemone: a short column, its disc, and a crown of long tentacles that sway with the water.
void anemone(Mesh& m, Rng& r, int variant) {
    surface(m, 24, 8,
            [&](float u, float v) {
                const float th = u * TAU;
                if (v < 0.6f) { const float t = v / 0.6f; const float rr = 0.15f + 0.03f * t; return vec3(std::cos(th) * rr, -0.04f + 0.16f * t, std::sin(th) * rr); }
                const float t = (v - 0.6f) / 0.4f;
                const float rr = 0.18f * (1.f - t);
                return vec3(std::cos(th) * rr, 0.12f + 0.025f * std::sin(t * PI), std::sin(th) * rr);
            },
            [&](float, float v) { return vec4(0.f, v, 0.f, 0.f); }, true);
    const int n = 44 + 10 * variant;
    for (int i = 0; i < n; i++) {
        const float a = r.range(0.f, TAU), d = 0.03f + 0.14f * std::sqrt(r.uniform());
        vec3 p(std::cos(a) * d, 0.13f, std::sin(a) * d);
        vec3 dir = normalize(vec3(std::cos(a) * (0.25f + d * 3.f), 1.f, std::sin(a) * (0.25f + d * 3.f)));
        const float len = r.range(0.17f, 0.25f) * (variant == 2 ? 1.2f : 1.f);
        std::vector<vec3> pts;
        std::vector<float> rad, al;
        const int seg = 6;
        for (int s = 0; s <= seg; s++) {
            const float t = (float)s / seg;
            pts.push_back(p);
            // a fat tentacle swelling into a bulb at its tip (a bubble-tip anemone)
            rad.push_back(0.026f * (1.f - 0.3f * t) + 0.012f * smoothstepf(0.6f, 1.f, t));
            al.push_back(t);
            dir = normalize(dir + vec3(std::cos(a), -0.3f, std::sin(a)) * 0.1f);
            p = p + dir * (len / seg);
        }
        tube(m, pts, rad, al, 7, 1.f, r.uniform(), true);
    }
}

// Sea whips: a few long, slender rods rising from one foot, swaying.
void seawhips(Mesh& m, Rng& r, int variant) {
    const int n = 6 + 2 * variant;
    for (int i = 0; i < n; i++) {
        const float a = r.range(0.f, TAU), lean = r.range(0.05f, 0.4f);
        vec3 dir = normalize(vec3(std::cos(a) * std::sin(lean), std::cos(lean), std::sin(a) * std::sin(lean)));
        vec3 p(std::cos(a) * 0.03f, -0.04f, std::sin(a) * 0.03f);
        const float len = r.range(0.75f, 1.1f);
        std::vector<vec3> pts;
        std::vector<float> rad, al;
        const int seg = 10;
        const vec3 bend(r.range(-0.05f, 0.05f), 0.f, r.range(-0.05f, 0.05f));
        for (int s = 0; s <= seg; s++) {
            const float t = (float)s / seg;
            pts.push_back(p);
            rad.push_back(lerpf(0.03f, 0.014f, t));
            al.push_back(t);
            dir = normalize(dir + bend);
            p = p + dir * (len / seg);
        }
        tube(m, pts, rad, al, 7, 1.f, r.uniform(), true);
    }
}

// A smooth 3D value noise (for the boulders' lumps).
float h3(int x, int y, int z) {
    uint64_t h = splitmix64((uint64_t)(uint32_t)x * 73856093ull ^ (uint64_t)(uint32_t)y * 19349663ull ^ (uint64_t)(uint32_t)z * 83492791ull);
    return (h >> 40) * (1.f / 16777216.f);
}
float noise3(vec3 p) {
    const int ix = (int)std::floor(p.x), iy = (int)std::floor(p.y), iz = (int)std::floor(p.z);
    float fx = p.x - ix, fy = p.y - iy, fz = p.z - iz;
    fx = fx * fx * (3.f - 2.f * fx); fy = fy * fy * (3.f - 2.f * fy); fz = fz * fz * (3.f - 2.f * fz);
    auto L = [](float a, float b, float t) { return a + (b - a) * t; };
    const float a = L(h3(ix, iy, iz), h3(ix + 1, iy, iz), fx), b = L(h3(ix, iy + 1, iz), h3(ix + 1, iy + 1, iz), fx);
    const float c = L(h3(ix, iy, iz + 1), h3(ix + 1, iy, iz + 1), fx), d = L(h3(ix, iy + 1, iz + 1), h3(ix + 1, iy + 1, iz + 1), fx);
    return L(L(a, b, fy), L(c, d, fy), fz);
}

// A boulder of reef rock: a lumpy, rounded stone, flattened, its foot in the sand.
void boulder(Mesh& m, Rng& r, int variant) {
    const vec3 o(r.range(0.f, 50.f), r.range(0.f, 50.f), r.range(0.f, 50.f));
    const float sq0 = 0.55f + 0.12f * variant;
    surface(m, 48, 24,
            [&](float u, float v) {
                const float th = u * TAU, ph = lerpf(-0.35f * PI, 0.5f * PI, v);
                const vec3 d(std::cos(ph) * std::cos(th), std::sin(ph), std::cos(ph) * std::sin(th));
                const float n = 0.22f * noise3(d * 1.6f + o) + 0.1f * noise3(d * 3.7f + o * 1.3f) + 0.04f * noise3(d * 8.f + o);
                const float rr = 0.5f * (0.82f + n);
                return vec3(d.x * rr * 1.15f, d.y * rr * sq0 * 1.4f + 0.1f, d.z * rr);
            },
            [&](float, float v) { return vec4(v, 0.f, 0.f, 0.f); }, true);
}

} // namespace

float motu(float a, float seed) {
    return 0.62f + 0.22f * std::sin(3.f * a + seed * 9.f) + 0.16f * std::sin(7.f * a + seed * 4.f) + 0.08f * std::sin(13.f * a + seed * 2.f);
}

// A palm: a slender curved trunk, a crown of drooping fronds.
Mesh buildPalm(int variant) {
    Mesh m;
    Rng r(0x9A1Bull + (uint64_t)variant * 41ull);
    const float lean = r.range(0.12f, 0.3f);
    std::vector<vec3> pts;
    std::vector<float> rad, al;
    for (int i = 0; i <= 10; i++) {
        const float t = i / 10.f;
        pts.push_back(vec3(lean * t * t, t, 0.f));
        rad.push_back(lerpf(0.035f, 0.022f, t));
        al.push_back(t);
    }
    tube(m, pts, rad, al, 7, 0.f, 0.f, false);
    const vec3 top = pts.back();
    const int fronds = 8 + variant;
    for (int k = 0; k < fronds; k++) {
        const float a = TAU * (k + r.range(-0.2f, 0.2f)) / fronds, droop = r.range(0.25f, 0.5f), len = r.range(0.38f, 0.5f);
        const vec3 dir(std::cos(a), 0.f, std::sin(a));
        const vec3 side(-dir.z, 0.f, dir.x);
        surface(m, 10, 2,
                [&](float u, float v) {
                    const float w = 0.07f * std::sin(PI * std::pow(u, 0.7f)) * (v * 2.f - 1.f);
                    return top + dir * (u * len) + vec3(0.f, 0.12f * u - droop * u * u, 0.f) + side * w + vec3(0.f, -std::fabs(w) * 0.4f, 0.f);
                },
                [&](float u, float) { return vec4(u, 0.f, 1.f, r.uniform()); }, false);
    }
    return m;
}

Mesh buildRock(int variant) {
    Mesh m;
    Rng r(0xB01Dull + (uint64_t)variant * 77ull);
    boulder(m, r, variant);
    return m;
}

Mesh buildCoral(int kind, int variant) {
    Mesh m;
    Rng r(0xC0A1ull + (uint64_t)kind * 977ull + (uint64_t)variant * 131ull);
    switch (kind) {
    case K_BRAIN: brain(m, r, variant); break;
    case K_STAG: staghorn(m, r, variant); break;
    case K_TABLE: table(m, r, variant); break;
    case K_FAN: seafan(m, r, variant); break;
    case K_SPONGE: sponges(m, r, variant); break;
    case K_ANEMONE: anemone(m, r, variant); break;
    default: seawhips(m, r, variant); break;
    }
    return m;
}

// =====================================================================================================
// Fish
// =====================================================================================================

namespace {
struct FishShape { float height, thick, nose, tail, tailSpread, fork, dorsal, dorsalLen; };
const FishShape SHAPES[F_COUNT] = {
    // height, thickness, nose bluntness, tail length, tail spread, forked, dorsal height, dorsal length
    {0.42f, 0.36f, 0.55f, 0.24f, 0.36f, 1.f, 0.12f, 0.45f},  // chromis
    {0.34f, 0.34f, 0.5f, 0.3f, 0.42f, 1.f, 0.1f, 0.55f},     // anthias (a lyre tail)
    {0.7f, 0.22f, 0.35f, 0.2f, 0.36f, 0.f, 0.16f, 0.6f},     // yellow tang (a disc)
    {0.56f, 0.25f, 0.45f, 0.22f, 0.38f, 0.3f, 0.12f, 0.6f},  // blue tang
    {0.72f, 0.2f, 0.3f, 0.18f, 0.3f, 0.f, 0.55f, 0.35f},     // moorish idol (a long streamer)
    {0.42f, 0.42f, 0.8f, 0.24f, 0.36f, 0.4f, 0.08f, 0.6f},   // parrotfish
    {0.44f, 0.42f, 0.75f, 0.18f, 0.3f, 0.f, 0.14f, 0.5f},    // clownfish
};
} // namespace

Mesh buildFish(int species) {
    Mesh m;
    const FishShape& s = SHAPES[std::clamp(species, 0, F_COUNT - 1)];
    // the profile along the body (v 0 the tail's root .. 1 the nose)
    auto H = [&](float v) {
        const float body = std::pow(std::sin(PI * std::pow(std::clamp(v, 0.f, 1.f), 0.8f)), 0.75f);
        const float root = 0.16f;
        return s.height * 0.5f * lerpf(root, 1.f, body) * (v > 0.85f ? lerpf(1.f, s.nose + 0.25f, smoothstepf(0.85f, 1.f, v)) : 1.f);
    };
    surface(m, 16, 20,
            [&](float u, float v) {
                const float th = u * TAU;
                const float h = H(v) * (v > 0.98f ? (1.f - v) / 0.02f * 0.6f + 0.4f : 1.f);
                return vec3(-0.5f + v, std::sin(th) * h, std::cos(th) * h * s.thick);
            },
            [&](float u, float v) { return vec4(v, std::sin(u * TAU), 0.f, 0.f); }, true);
    // the tail: a thin sheet behind the body
    surface(m, 8, 6,
            [&](float u, float v) {
                const float x = -0.5f - u * s.tail;
                const float spread = 0.06f + s.tailSpread * 0.5f * std::pow(u, 0.9f);
                return vec3(x + 0.01f, (v * 2.f - 1.f) * spread, 0.f);
            },
            [&](float u, float v) { return vec4(-u, v * 2.f - 1.f, 1.f, s.fork); }, false);
    // the dorsal and anal fins
    for (int k = 0; k < 2; k++) {
        const float sign = k == 0 ? 1.f : -1.f, hgt = k == 0 ? s.dorsal : s.dorsal * 0.6f, len = k == 0 ? s.dorsalLen : s.dorsalLen * 0.55f;
        surface(m, 10, 4,
                [&](float u, float v) {
                    const float x = lerpf(-0.42f, -0.42f + len, u);
                    const float vb = x + 0.5f;
                    const float base = H(vb) * 0.9f;
                    const float tall = hgt * std::sin(PI * std::pow(u, 0.6f)) * (k == 0 && species == F_IDOL ? (1.f + 2.5f * smoothstepf(0.5f, 0.9f, u) * (1.f - u)) : 1.f);
                    return vec3(x - v * tall * 0.4f, sign * (base + v * tall), 0.f);
                },
                [&](float u, float v) { return vec4(u, v, 2.f, 0.f); }, k == 1);
    }
    // the pectoral fins: two little paddles
    for (int k = 0; k < 2; k++) {
        const float side = k == 0 ? 1.f : -1.f;
        surface(m, 4, 3,
                [&](float u, float v) {
                    const float x = 0.12f - u * 0.14f;
                    const float y = -0.05f + (v - 0.5f) * 0.07f * (1.f - 0.4f * u);
                    return vec3(x, y, side * (H(0.62f) * s.thick * 0.9f + u * 0.06f));
                },
                [&](float u, float v) { return vec4(u, v, 3.f, 0.f); }, k == 1);
    }
    return m;
}

Mesh buildManta() {
    Mesh m;
    auto zf = [](float ax) { return 0.4f + 0.06f * std::exp(-sq((ax - 0.12f) / 0.05f)) - 0.62f * std::pow(std::max(ax - 0.08f, 0.f), 1.15f); };
    auto zb = [](float ax) { return -0.45f + 0.27f * smoothstepf(0.08f, 1.f, ax); };
    surface(m, 64, 14,
            [&](float u, float v) {
                const float x = u * 2.f - 1.f, ax = std::fabs(x);
                const float z = lerpf(zf(ax), zb(ax), v);
                const float y = 0.1f * std::exp(-sq(x / 0.2f)) * std::sin(PI * v) + 0.02f * std::sin(PI * v) * (1.f - ax);
                return vec3(x, y, z);
            },
            [&](float u, float v) { return vec4(std::fabs(u * 2.f - 1.f), v, 0.f, 0.f); }, false);
    std::vector<vec3> pts;
    std::vector<float> rad, al;
    for (int i = 0; i <= 8; i++) {
        const float t = i / 8.f;
        pts.push_back(vec3(0.f, 0.f, -0.42f - 0.7f * t));
        rad.push_back(lerpf(0.035f, 0.006f, t));
        al.push_back(t);
    }
    tube(m, pts, rad, al, 6, 1.f, 0.f, true);
    return m;
}

} // namespace reef
