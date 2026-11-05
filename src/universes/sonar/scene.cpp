// SONAR's sunken world in 3D: one static mesh (the seabed as a terrain grid, rocks, a wreck, the city's buildings)
// and the closed loop the camera glides along. The buildings are placed along the loop itself, so that the glide
// passes down a colonnade, under two arches, beside a tower and a dome, over the wreck and along a street of the
// city, and none of them ever stands in the camera's way. Every polygon carries the distances to its own edges, so
// the shader can draw its outline as a fine line of light (the ribs and strakes of the hull, the courses of the
// pyramid, the columns' flutes, the dome's grid).
#include "universes/sonar/scene.hpp"

#include <cmath>

namespace sonar_scene {

namespace {

float hash2(int x, int z) {
    uint64_t h = splitmix64((uint64_t)(uint32_t)x * 0x9E3779B1ull ^ ((uint64_t)(uint32_t)z << 32));
    return (float)((h >> 40) & 0xFFFFFF) / 16777216.f;
}
float vnoise(float x, float z) {
    const int ix = (int)std::floor(x), iz = (int)std::floor(z);
    float fx = x - ix, fz = z - iz;
    fx = fx * fx * (3 - 2 * fx);
    fz = fz * fz * (3 - 2 * fz);
    const float a = hash2(ix, iz), b = hash2(ix + 1, iz), c = hash2(ix, iz + 1), d = hash2(ix + 1, iz + 1);
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fz);
}
float fbm(float x, float z) {
    float s = 0, a = 0.5f;
    for (int i = 0; i < 4; i++) {
        s += a * vnoise(x, z);
        const float nx = 0.8f * x + 0.6f * z, nz = -0.6f * x + 0.8f * z;
        x = nx * 2.03f + 0.37f;
        z = nz * 2.03f + 0.37f;
        a *= 0.5f;
    }
    return s / 0.9375f;
}
vec3 rotAbout(const vec3& v, const vec3& axis, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1 - c));
}

} // namespace

Fr Fr::rotX(float a) const { Fr f = *this; f.y = rotAbout(y, x, a); f.z = rotAbout(z, x, a); return f; }
Fr Fr::rotY(float a) const { Fr f = *this; f.x = rotAbout(x, y, a); f.z = rotAbout(z, y, a); return f; }
Fr Fr::rotZ(float a) const { Fr f = *this; f.x = rotAbout(x, z, a); f.y = rotAbout(y, z, a); return f; }

// =====================================================================================================
// The loop
// =====================================================================================================

vec3 Scene::spline(float u) const {
    const int n = (int)ctrl_.size();
    float fu = u - std::floor(u / n) * n;
    const int i = (int)fu % n;
    const float t = fu - std::floor(fu);
    const vec3& p0 = ctrl_[(i + n - 1) % n];
    const vec3& p1 = ctrl_[i];
    const vec3& p2 = ctrl_[(i + 1) % n];
    const vec3& p3 = ctrl_[(i + 2) % n];
    const float t2 = t * t, t3 = t2 * t;
    return (p1 * 2.f + (p2 - p0) * t + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2 + (p1 * 3.f - p0 - p2 * 3.f + p3) * t3) * 0.5f;
}

void Scene::buildPath() {
    // a slow loop around the city (x, camera height, z), counterclockwise seen from above: the city's heart on
    // the right
    ctrl_ = {{0, 6.6f, -34},   {20, 6.4f, -32}, {34, 6.8f, -18}, {37, 7.4f, 2},   {25, 6.6f, 16}, {9, 6.2f, 22},
             {-8, 6.8f, 30},   {-26, 7.4f, 28}, {-37, 6.8f, 12}, {-35, 6.4f, -8}, {-21, 6.5f, -26}};
    const int n = (int)ctrl_.size(), STEPS = 4096;
    tabS_.assign(STEPS + 1, 0.f);
    tabU_.assign(STEPS + 1, 0.f);
    vec3 prev = spline(0.f);
    float s = 0.f;
    for (int k = 1; k <= STEPS; k++) {
        const float u = (float)n * k / STEPS;
        const vec3 p = spline(u);
        s += length(p - prev);
        prev = p;
        tabS_[k] = s;
        tabU_[k] = u;
    }
    len_ = s;
}

vec3 Scene::path(float s) const {
    s = s - std::floor(s / len_) * len_;
    // the arc length table is monotonic: a binary search, then the spline itself (smooth)
    size_t lo = 0, hi = tabS_.size() - 1;
    while (hi - lo > 1) {
        const size_t mid = (lo + hi) / 2;
        if (tabS_[mid] <= s) lo = mid;
        else hi = mid;
    }
    const float t = (s - tabS_[lo]) / std::max(1e-6f, tabS_[hi] - tabS_[lo]);
    return spline(lerpf(tabU_[lo], tabU_[hi], t));
}

float Scene::pathDist(float x, float z, float& y) const {
    float best = 1e9f;
    y = 5.f;
    const int N = 600;
    for (int k = 0; k < N; k++) {
        const vec3 p = path(len_ * k / N);
        const float d = (p.x - x) * (p.x - x) + (p.z - z) * (p.z - z);
        if (d < best) { best = d; y = p.y; }
    }
    return std::sqrt(best);
}

// =====================================================================================================
// The seabed
// =====================================================================================================

float Scene::rawHeight(float x, float z) const {
    // long, soft dunes, a slow swell of the ground under them
    const float u = 0.8f * x + 0.6f * z, v = -0.6f * x + 0.8f * z;
    float h = 0.5f * std::sin(0.16f * u + 0.6f * std::sin(0.05f * v)) + 0.6f * std::sin(0.045f * x + 0.7f) * std::cos(0.05f * z - 0.3f);
    h += (fbm(x * 0.035f + 3.1f, z * 0.035f - 1.7f) - 0.5f) * 2.4f;
    const float r = std::sqrt(x * x + z * z);
    const float wall = smoothstepf(52.f, 92.f, r);
    h += 18.f * wall * wall + 3.f * wall * (fbm(x * 0.05f, z * 0.05f) - 0.4f);
    return h;
}

void Scene::buildTerrain() {
    hgt_.assign((GN + 1) * (GN + 1), 0.f);
    const float cell = 2.f * GEXT / GN;
    for (int j = 0; j <= GN; j++)
        for (int i = 0; i <= GN; i++) {
            const float x = -GEXT + i * cell, z = -GEXT + j * cell;
            float h = rawHeight(x, z);
            if (std::fabs(x) < 50.f && std::fabs(z) < 46.f) {
                // a gentle avenue under the loop, always well below the camera
                float py;
                const float d = pathDist(x, z, py);
                h -= 0.7f * std::exp(-d * d / 40.f);
                const float cap = py - 3.6f + 0.3f * std::max(0.f, d - 2.f);
                h = std::min(h, cap);
            }
            hgt_[j * (GN + 1) + i] = h;
        }
    const uint32_t base = (uint32_t)verts.size();
    for (int j = 0; j <= GN; j++)
        for (int i = 0; i <= GN; i++) {
            const int i0 = std::max(0, i - 1), i1 = std::min(GN, i + 1), j0 = std::max(0, j - 1), j1 = std::min(GN, j + 1);
            const float dx = (hgt_[j * (GN + 1) + i1] - hgt_[j * (GN + 1) + i0]) / ((i1 - i0) * cell);
            const float dz = (hgt_[j1 * (GN + 1) + i] - hgt_[j0 * (GN + 1) + i]) / ((j1 - j0) * cell);
            const vec3 n = normalize(vec3(-dx, 1.f, -dz));
            Vtx v;
            v.p[0] = -GEXT + i * cell; v.p[1] = hgt_[j * (GN + 1) + i]; v.p[2] = -GEXT + j * cell;
            v.n[0] = n.x; v.n[1] = n.y; v.n[2] = n.z;
            for (float& e : v.e) e = 100.f;
            v.i[0] = (float)BED; v.i[1] = (float)CONTOURS; v.i[2] = 0.f; v.i[3] = 0.f;
            verts.push_back(v);
        }
    for (int j = 0; j < GN; j++)
        for (int i = 0; i < GN; i++) {
            const uint32_t a = base + j * (GN + 1) + i, b = a + 1, c = a + (GN + 1), d = c + 1;
            const uint32_t q[6] = {a, c, b, b, c, d};
            idx.insert(idx.end(), q, q + 6);
        }
}

float Scene::height(float x, float z) const {
    const float cell = 2.f * GEXT / GN;
    const float fx = clampf((x + GEXT) / cell, 0.f, GN - 0.001f), fz = clampf((z + GEXT) / cell, 0.f, GN - 0.001f);
    const int i = (int)fx, j = (int)fz;
    const float tx = fx - i, tz = fz - j;
    const float a = hgt_[j * (GN + 1) + i], b = hgt_[j * (GN + 1) + i + 1], c = hgt_[(j + 1) * (GN + 1) + i],
                d = hgt_[(j + 1) * (GN + 1) + i + 1];
    return lerpf(lerpf(a, b, tx), lerpf(c, d, tx), tz);
}

// =====================================================================================================
// Geometry
// =====================================================================================================

// A planar convex polygon of 3 or 4 vertices; edge k runs from p[k] to p[k+1] and is outlined if edgeMask has bit k.
void Scene::poly(const vec3* p, int n, const vec3* nrm, int layer, int flags, unsigned edgeMask) {
    vec3 fn = cross(p[1] - p[0], p[n - 1] - p[0]);
    fn = normalize(fn);
    const vec3 u = normalize(p[1] - p[0]), v = cross(fn, u);
    const uint32_t base = (uint32_t)verts.size();
    for (int j = 0; j < n; j++) {
        Vtx t;
        t.p[0] = p[j].x; t.p[1] = p[j].y; t.p[2] = p[j].z;
        const vec3 nn = nrm ? nrm[j] : fn;
        t.n[0] = nn.x; t.n[1] = nn.y; t.n[2] = nn.z;
        for (int k = 0; k < 4; k++) {
            if (k >= n || !((edgeMask >> k) & 1u)) { t.e[k] = 100.f; continue; }
            const vec3 a = p[k], dir = normalize(p[(k + 1) % n] - a);
            t.e[k] = length(cross(p[j] - a, dir));
        }
        t.i[0] = (float)layer; t.i[1] = (float)flags; t.i[2] = dot(p[j] - p[0], u); t.i[3] = dot(p[j] - p[0], v);
        verts.push_back(t);
    }
    for (int k = 1; k + 1 < n; k++) {
        idx.push_back(base);
        idx.push_back(base + k);
        idx.push_back(base + k + 1);
    }
}

void Scene::quad(const vec3& a, const vec3& b, const vec3& c, const vec3& d, int layer, int flags, unsigned edges) {
    const vec3 p[4] = {a, b, c, d};
    poly(p, 4, nullptr, layer, flags, edges);
}

void Scene::box(const Fr& f, float hx, float hy, float hz, int layer, int flags) {
    auto face = [&](const vec3& N, float hn, const vec3& U, float hu, const vec3& V, float hv, int fl) {
        const vec3 c = f.o + N * hn;
        quad(c - U * hu - V * hv, c + U * hu - V * hv, c + U * hu + V * hv, c - U * hu + V * hv, layer, fl);
    };
    const int side = flags & WINDOWS;
    face(f.z, hz, f.x, hx, f.y, hy, side);
    face(f.z * -1.f, hz, f.x * -1.f, hx, f.y, hy, side);
    face(f.x, hx, f.z * -1.f, hz, f.y, hy, side);
    face(f.x * -1.f, hx, f.z, hz, f.y, hy, side);
    face(f.y, hy, f.x, hx, f.z * -1.f, hz, 0);
    face(f.y * -1.f, hy, f.x, hx, f.z, hz, 0);
}

// A cylinder standing on f.o along f.y.
void Scene::cylinder(const Fr& f, float r, float h, int seg, int layer, bool flutes, bool caps) {
    for (int i = 0; i < seg; i++) {
        const float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg;
        const vec3 d0 = f.x * std::cos(a0) + f.z * std::sin(a0), d1 = f.x * std::cos(a1) + f.z * std::sin(a1);
        const vec3 p[4] = {f.o + d1 * r, f.o + d0 * r, f.o + d0 * r + f.y * h, f.o + d1 * r + f.y * h};
        const vec3 nn[4] = {d1, d0, d0, d1};
        poly(p, 4, nn, layer, 0, flutes ? 15u : 5u);
        if (caps) {
            const vec3 top = f.o + f.y * h;
            const vec3 t[3] = {top, top + d0 * r, top + d1 * r};
            poly(t, 3, nullptr, layer, 0, 2u);
        }
    }
}

// A hemisphere on f.o, its grid of meridians and rings outlined.
void Scene::dome(const Fr& f, float r, int mer, int rings, int layer) {
    auto pt = [&](int i, int j) {
        const float a = TAU * i / mer, ph = 0.5f * PI * j / rings;
        return (f.x * std::cos(a) + f.z * std::sin(a)) * (r * std::cos(ph)) + f.y * (r * std::sin(ph));
    };
    for (int j = 0; j < rings; j++)
        for (int i = 0; i < mer; i++) {
            if (j == rings - 1) {
                const vec3 nn[3] = {normalize(pt(i + 1, j)), normalize(pt(i, j)), f.y};
                const vec3 p[3] = {f.o + pt(i + 1, j), f.o + pt(i, j), f.o + f.y * r};
                poly(p, 3, nn, layer, 0, 7u);
                continue;
            }
            const vec3 a = pt(i + 1, j), b = pt(i, j), c = pt(i, j + 1), d = pt(i + 1, j + 1);
            const vec3 nn[4] = {normalize(a), normalize(b), normalize(c), normalize(d)};
            const vec3 p[4] = {f.o + a, f.o + b, f.o + c, f.o + d};
            poly(p, 4, nn, layer, 0, 15u);
        }
}

// A faceted boulder half sunk in the sand.
void Scene::rock(const vec3& c, float r, float squash, uint64_t seed) {
    const int M = 7, R = 5;
    vec3 g[R + 1][M];
    for (int j = 0; j <= R; j++)
        for (int i = 0; i < M; i++) {
            const float a = TAU * (i + 0.5f * (j % 2)) / M, ph = -0.5f * PI + PI * j / R;
            const float k = (j == 0 || j == R) ? 1.f : 0.78f + 0.4f * (float)((splitmix64(seed + j * 31 + i * 7) >> 40) & 0xFFFF) / 65536.f;
            const float rr = (j == 0 || j == R) ? 0.f : std::cos(ph);
            g[j][i] = c + vec3(std::cos(a) * rr * r * k, std::sin(ph) * r * squash * (0.8f + 0.2f * k), std::sin(a) * rr * r * k);
        }
    for (int j = 0; j < R; j++)
        for (int i = 0; i < M; i++) {
            const vec3 &p00 = g[j][i], &p10 = g[j][(i + 1) % M], &p01 = g[j + 1][i], &p11 = g[j + 1][(i + 1) % M];
            if (j > 0) { const vec3 t[3] = {p00, p10, p11}; poly(t, 3, nullptr, ROCK, 0, 3u); }
            if (j < R - 1) { const vec3 t[3] = {p00, p11, p01}; poly(t, 3, nullptr, ROCK, 0, 6u); }
        }
    anchor(c + vec3(0, r * squash * 0.6f, 0), ROCK);
}

// The wreck: an open hull (its ribs and strakes outlined, a breach in its side), deck beams, a cabin and a broken
// mast. f: the hull's middle, x across the beam, y up through the deck, z toward the bow.
void Scene::wreck(const Fr& f) {
    const float L = 13.f, W = 2.1f, H = 1.5f;
    const int NS = 10, NP = 7;
    const float px[NP] = {-1.f, -0.94f, -0.62f, 0.f, 0.62f, 0.94f, 1.f};
    const float py[NP] = {1.f, 0.15f, -0.68f, -1.f, -0.68f, 0.15f, 1.f};
    vec3 g[NS][NP];
    for (int k = 0; k < NS; k++) {
        const float t = (float)k / (NS - 1);
        float sx = t < 0.55f ? 1.f : 1.f - 0.97f * std::pow((t - 0.55f) / 0.45f, 1.6f);
        if (t < 0.1f) sx *= 0.9f + t;
        const float lift = 0.55f * t * t;
        for (int m = 0; m < NP; m++) g[k][m] = f.at(vec3(px[m] * W * sx, py[m] * H + lift * (py[m] > 0.5f ? 1.f : 0.6f), (t - 0.5f) * L));
    }
    for (int k = 0; k + 1 < NS; k++)
        for (int m = 0; m + 1 < NP; m++) {
            if (k >= 5 && k <= 6 && m >= 1 && m <= 2) continue; // the breach
            const vec3 p[4] = {g[k][m], g[k + 1][m], g[k + 1][m + 1], g[k][m + 1]};
            // the bow's sections pinch: keep the quads planar enough by splitting them
            const vec3 t0[3] = {p[0], p[1], p[2]}, t1[3] = {p[0], p[2], p[3]};
            poly(t0, 3, nullptr, WRECK, 0, 3u);
            poly(t1, 3, nullptr, WRECK, 0, 6u);
        }
    // the transom
    vec3 c(0, 0, 0);
    for (int m = 0; m < NP; m++) c += g[0][m];
    c = c / (float)NP;
    for (int m = 0; m < NP; m++) {
        const vec3 t[3] = {c, g[0][m], g[0][(m + 1) % NP]};
        poly(t, 3, nullptr, WRECK, 0, 2u);
    }
    // deck beams, a cabin, the mast broken off
    for (int b = 0; b < 4; b++) box(f.moved(vec3(0, H * 0.92f, -L * 0.32f + b * L * 0.19f)), W * 0.95f, 0.07f, 0.12f, WRECK);
    box(f.moved(vec3(0, H + 0.45f, -L * 0.3f)), 0.75f, 0.45f, 0.95f, WRECK);
    cylinder(f.moved(vec3(0, -0.4f, L * 0.08f)).rotX(-0.12f), 0.13f, 5.6f, 8, WRECK, false);
    box(f.moved(vec3(0.2f, 3.9f, L * 0.08f + 0.3f)).rotZ(0.35f), 1.4f, 0.05f, 0.05f, WRECK);
    anchor(f.at(vec3(0, H + 0.8f, 0)), WRECK);
    anchor(f.at(vec3(0, H + 0.8f, L * 0.35f)), WRECK);
    anchor(f.at(vec3(0, 4.f, L * 0.08f)), WRECK);
}

// =====================================================================================================
// The city along the loop
// =====================================================================================================

Fr Scene::station(float frac) const {
    const float s = frac * len_;
    const vec3 p = path(s), q = path(s + 0.5f), b = path(s - 0.5f);
    vec3 t = q - b;
    t.y = 0.f;
    t = normalize(t);
    Fr f;
    f.o = vec3(p.x, 0.f, p.z);
    f.z = t;
    f.y = vec3(0, 1, 0);
    f.x = normalize(cross(t, f.y));
    return f;
}

float Scene::ground(const Fr& f, float x, float z, float r) const {
    float g = 1e9f;
    for (int i = 0; i < 9; i++) {
        const float a = TAU * i / 8;
        const vec3 p = f.at(vec3(x + (i ? r * std::cos(a) : 0.f), 0.f, z + (i ? r * std::sin(a) : 0.f)));
        g = std::min(g, height(p.x, p.z));
    }
    return g;
}

void Scene::buildCity() {
    Rng rng(0x5EA51DEull);
    // a building's frame on the seabed: lateral offset x (right: the city's heart), z along the loop
    auto site = [&](float frac, float x, float z, float r, float sink) {
        Fr f = station(frac);
        const float g = ground(f, x, z, r) - sink;
        f.o = f.at(vec3(x, 0.f, z));
        f.o.y = g;
        return f;
    };
    // the side of a building the loop passes: where its echoes come from
    auto facing = [&](const Fr& f, float frac, float r, float y) {
        const vec3 p = station(frac).o;
        vec3 d = p - f.o;
        d.y = 0.f;
        d = normalize(d);
        anchor(f.o + d * (r + 1.2f) + vec3(0, y, 0), CITY);
    };
    auto column = [&](const Fr& f, float h, bool capital) {
        box(f.moved(vec3(0, 0.15f, 0)), 0.62f, 0.3f, 0.62f, CITY);
        cylinder(f.moved(vec3(0, 0.45f, 0)), 0.42f, h - 0.45f, 12, CITY, true, !capital);
        if (capital) box(f.moved(vec3(0, h + 0.15f, 0)), 0.6f, 0.15f, 0.6f, CITY);
    };
    auto tower = [&](const Fr& f, float w, float h, float d) {
        box(f.moved(vec3(0, h * 0.5f, 0)), w, h * 0.5f, d, CITY, WINDOWS);
        box(f.moved(vec3(0, h + 1.f, 0)), w * 0.62f, 1.f, d * 0.62f, CITY);
        cylinder(f.moved(vec3(0, h + 2.f, 0)), 0.08f, 3.2f, 6, CITY, false);
    };
    auto archway = [&](const Fr& f, float half, float ph, int broken) {
        for (int s = -1; s <= 1; s += 2) box(f.moved(vec3(s * (half + 0.65f), ph * 0.5f, 0)), 0.65f, ph * 0.5f, 0.75f, CITY);
        const int N = 9;
        const float rm = half + 0.65f;
        for (int i = 0; i < N; i++) {
            if (broken && (i >= N - broken)) continue;
            const float a = PI * (i + 0.5f) / N;
            Fr v = f.moved(vec3(std::cos(a) * rm, ph + std::sin(a) * rm, 0));
            v.x = f.x * std::cos(a) + f.y * std::sin(a);
            v.y = f.x * -std::sin(a) + f.y * std::cos(a);
            box(v, 0.65f, rm * PI / N * 0.5f * 0.97f, 0.75f, CITY);
        }
        anchor(f.at(vec3(-half - 1.f, ph * 0.6f, -1.6f)), CITY);
        anchor(f.at(vec3(half + 1.f, ph * 0.6f, -1.6f)), CITY);
        anchor(f.at(vec3(0, ph + rm * 0.8f, -1.8f)), CITY);
    };

    // 1. A colonnade: the loop runs down an avenue of columns, an architrave over some of them.
    {
        const int N = 11;
        for (int s = -1; s <= 1; s += 2) {
            vec3 prevTop;
            bool prevOk = false;
            for (int i = 0; i < N; i++) {
                const float frac = 0.02f + i * 3.4f / len_;
                Fr f = site(frac, s * 5.4f, 0.f, 0.7f, 0.3f);
                f.x = vec3(1, 0, 0); f.z = vec3(0, 0, 1);
                const bool broken = rng.chance(0.28f);
                const float h = broken ? rng.range(1.4f, 4.2f) : 7.2f;
                column(f, h, !broken);
                const vec3 top = f.o + vec3(0, h + 0.3f, 0);
                if (!broken && prevOk && rng.chance(0.75f)) {
                    Fr b;
                    b.o = (top + prevTop) * 0.5f + vec3(0, 0.25f, 0);
                    b.z = normalize(top - prevTop);
                    b.y = vec3(0, 1, 0);
                    b.x = normalize(cross(b.y, b.z));
                    b.y = cross(b.z, b.x);
                    box(b, 0.5f, 0.25f, length(top - prevTop) * 0.5f + 0.5f, CITY);
                }
                prevTop = top;
                prevOk = !broken;
                if (i % 3 == 1) anchor(f.o + vec3(0, 3.f, 0) + station(frac).x * (-s * 1.2f), CITY);
            }
        }
    }
    // 2. A tower with windows, on the right.
    { const float fr = 0.165f; Fr f = site(fr, 11.5f, 0.f, 3.5f, 0.5f); tower(f, 2.6f, 17.f, 2.6f); facing(f, fr, 2.6f, 4.f); facing(f, fr, 2.6f, 10.f); facing(f, fr, 2.6f, 15.f); }
    // 3. An obelisk on the left.
    {
        const float fr = 0.215f;
        Fr f = site(fr, -6.5f, 0.f, 1.5f, 0.4f);
        box(f.moved(vec3(0, 0.4f, 0)), 1.6f, 0.4f, 1.6f, CITY);
        const float b = 0.85f, t = 0.55f, h = 9.f;
        const vec3 c0 = f.o + vec3(0, 0.8f, 0), c1 = c0 + vec3(0, h, 0);
        const vec3 X = f.x, Z = f.z;
        const vec3 lo[4] = {c0 - X * b - Z * b, c0 + X * b - Z * b, c0 + X * b + Z * b, c0 - X * b + Z * b};
        const vec3 hi[4] = {c1 - X * t - Z * t, c1 + X * t - Z * t, c1 + X * t + Z * t, c1 - X * t + Z * t};
        const vec3 apex = c1 + vec3(0, 1.1f, 0);
        for (int i = 0; i < 4; i++) {
            const int j = (i + 1) % 4;
            quad(lo[j], lo[i], hi[i], hi[j], CITY);
            const vec3 tri[3] = {hi[j], hi[i], apex};
            poly(tri, 3, nullptr, CITY, 0, 7u);
        }
        facing(f, fr, 1.f, 5.f);
    }
    // 4. An arch over the loop.
    { const float fr = 0.29f; Fr f = site(fr, 0.f, 0.f, 4.5f, 0.3f); archway(f, 4.f, 7.f, 0); }
    // 5. A dome on its drum, on the left.
    {
        const float fr = 0.36f;
        Fr f = site(fr, -14.f, 0.f, 5.5f, 0.4f);
        const float r = 5.f, h = 4.2f;
        cylinder(f, r, h, 24, CITY, true, false);
        cylinder(f.moved(vec3(0, h, 0)), r * 1.07f, 0.4f, 24, CITY, false, false);
        dome(f.moved(vec3(0, h + 0.4f, 0)), r, 16, 5, CITY);
        cylinder(f.moved(vec3(0, h + 0.4f + r - 0.2f, 0)), 0.7f, 1.4f, 10, CITY, true, false);
        dome(f.moved(vec3(0, h + 0.4f + r + 1.2f, 0)), 0.7f, 10, 3, CITY);
        facing(f, fr, r, 2.5f);
        facing(f, fr, r * 0.8f, h + 3.f);
    }
    // 6. Low ruins: broken walls on both sides, a fallen column.
    for (int k = 0; k < 6; k++) {
        const float fr = 0.405f + 0.012f * k;
        const float s = (k % 2) ? 1.f : -1.f;
        Fr f = site(fr, s * rng.range(5.5f, 8.f), 0.f, 2.f, 0.3f).rotY(rng.range(-0.3f, 0.3f));
        const float hgt = rng.range(1.2f, 3.4f);
        box(f.moved(vec3(0, hgt * 0.5f, 0)), 0.3f, hgt * 0.5f, rng.range(1.2f, 2.4f), CITY);
        if (k == 2) anchor(f.o + vec3(0, hgt + 0.8f, 0), CITY);
    }
    {
        Fr f = site(0.44f, 5.f, 0.f, 3.f, 0.15f).rotY(0.5f).rotZ(PI * 0.5f);
        for (int d = 0; d < 3; d++) cylinder(f.moved(vec3(0, -3.f + d * 1.65f, 0)), 0.45f, 1.45f, 12, CITY, true);
    }
    // 7. The wreck, beside the loop and under it.
    {
        const float fr = 0.5f;
        Fr st = station(fr);
        Fr f = st.moved(vec3(3.2f, 0.f, 0.f)).rotY(0.35f).rotZ(-0.42f);
        f.o.y = ground(st, 3.2f, 0.f, 4.f) + 0.6f;
        wreck(f);
    }
    // 8. A stepped pyramid on the left.
    {
        const float fr = 0.6f;
        Fr f = site(fr, -17.f, 0.f, 8.f, 0.6f).rotY(0.25f);
        const int N = 6;
        for (int i = 0; i < N; i++) box(f.moved(vec3(0, 0.75f + 1.5f * i, 0)), 7.5f - 1.15f * i, 0.75f, 7.5f - 1.15f * i, CITY);
        box(f.moved(vec3(0, 1.5f * N + 1.1f, 0)), 1.3f, 1.1f, 1.3f, CITY);
        facing(f, fr, 7.f, 2.f);
        facing(f, fr, 4.f, 6.f);
        facing(f, fr, 1.5f, 10.5f);
    }
    // 9. A street of the city on the right: towers of several heights.
    {
        const float fr[5] = {0.665f, 0.685f, 0.705f, 0.725f, 0.745f};
        const float off[5] = {10.5f, 15.f, 11.f, 16.f, 11.5f};
        const float hh[5] = {9.f, 15.f, 7.f, 19.f, 11.f};
        const float ww[5] = {2.4f, 3.f, 2.2f, 3.2f, 2.6f};
        for (int i = 0; i < 5; i++) {
            Fr f = site(fr[i], off[i], 0.f, ww[i] * 1.4f, 0.5f).rotY(rng.range(-0.15f, 0.15f));
            tower(f, ww[i], hh[i], ww[i] * rng.range(0.9f, 1.3f));
            facing(f, fr[i], ww[i], 3.f);
            facing(f, fr[i], ww[i], hh[i] * 0.7f);
        }
        // the same street's left side: lower houses
        for (int i = 0; i < 4; i++) {
            const float f0 = 0.67f + 0.022f * i;
            Fr f = site(f0, -rng.range(8.5f, 10.5f), 0.f, 2.5f, 0.4f);
            const float h = rng.range(2.5f, 4.5f);
            box(f.moved(vec3(0, h * 0.5f, 0)), rng.range(1.5f, 2.2f), h * 0.5f, rng.range(1.6f, 2.4f), CITY, WINDOWS);
            if (i % 2 == 0) facing(f, f0, 2.f, h * 0.6f);
        }
    }
    // 10. A round temple on the right.
    {
        const float fr = 0.79f;
        Fr f = site(fr, 12.f, 0.f, 5.f, 0.3f);
        cylinder(f, 5.f, 0.7f, 28, CITY, false);
        const int N = 10;
        for (int i = 0; i < N; i++) {
            const float a = TAU * i / N;
            if (i == 3) continue; // one column gone
            column(f.moved(vec3(std::cos(a) * 4.2f, 0.7f, std::sin(a) * 4.2f)), 5.2f, true);
        }
        cylinder(f.moved(vec3(0, 6.2f, 0)), 4.7f, 0.6f, 28, CITY, false, false);
        dome(f.moved(vec3(0, 6.8f, 0)), 4.7f, 14, 4, CITY);
        facing(f, fr, 4.7f, 3.f);
        facing(f, fr, 4.f, 8.f);
    }
    // 11. A broken arch over the loop.
    { const float fr = 0.865f; Fr f = site(fr, 0.f, 0.f, 4.5f, 0.3f).rotY(0.08f); archway(f, 4.f, 6.5f, 3); }
    // 12. Fallen columns.
    for (int k = 0; k < 3; k++) {
        const float fr = 0.92f + 0.018f * k;
        Fr f = site(fr, (k % 2 ? 1.f : -1.f) * rng.range(5.f, 7.f), 0.f, 3.f, 0.1f).rotY(rng.range(0.f, TAU)).rotZ(PI * 0.5f);
        for (int d = 0; d < 2 + k % 2; d++) cylinder(f.moved(vec3(0, -2.f + d * 1.7f, 0)), 0.42f, 1.5f, 12, CITY, true);
        anchor(f.o + vec3(0, 1.2f, 0), CITY);
    }
    // 13. The city's heart, inside the loop: towers and a great dome, seen across the water.
    {
        struct T { float x, z, w, h; };
        const T ts[4] = {{-4.f, 3.f, 3.5f, 24.f}, {6.f, -4.f, 4.f, 17.f}, {2.f, 9.f, 3.f, 13.f}, {-9.f, -6.f, 2.6f, 10.f}};
        for (const T& t : ts) {
            Fr f;
            f.o = vec3(t.x, height(t.x, t.z) - 0.6f, t.z);
            f = f.rotY(t.x * 0.1f);
            tower(f, t.w, t.h, t.w);
            anchor(f.o + vec3(t.w + 1.f, t.h * 0.5f, 0), CITY);
            anchor(f.o + vec3(0, t.h * 0.8f, -t.w - 1.f), CITY);
        }
        Fr f;
        f.o = vec3(8.f, height(8.f, 8.f) - 0.5f, 8.f);
        cylinder(f, 6.f, 4.5f, 28, CITY, true, false);
        dome(f.moved(vec3(0, 4.5f, 0)), 6.f, 18, 6, CITY);
        anchor(f.o + vec3(-5.f, 8.f, -5.f), CITY);
    }
    // the seabed's points of echo, and its rocks (never on the loop)
    for (int k = 0; k < 90; k++) {
        const float x = rng.range(-60.f, 60.f), z = rng.range(-55.f, 55.f);
        float py;
        const float d = pathDist(x, z, py);
        if (d < 4.5f) continue;
        const float r = rng.range(0.5f, 1.8f) * (d > 12.f ? 1.4f : 1.f);
        if (d < r + 4.f) continue;
        rock(vec3(x, height(x, z) + r * 0.15f, z), r, rng.range(0.45f, 0.8f), rng.next());
    }
}

void Scene::build() {
    verts.clear();
    idx.clear();
    anchors.clear();
    buildPath();
    buildTerrain();
    buildCity();
}

} // namespace sonar_scene
