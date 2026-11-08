// GIANT TREE: the tree's generation (see gianttree.cpp for the world).
//
// One tree per game, from a seed. Its growth is G = ln(height in meters), and it is built in three spaces that the
// shaders place against G (nothing is rebuilt while it grows):
//  - the trunk: rings at heights spaced in log space from the roots to some two million kilometers; the shader puts
//    each ring on the trunk's winding axis with the radius it has at the current G (thick at the foot, slender under
//    the crown), and gathers the rings above the growing tip into it. Vine leaves spiral up it, sprouting as the tip
//    passes them, sized on the trunk's radius where they sit;
//  - the crown, in crown space (unit: the crown's scale, origin at the trunk's tip): a round dome of foliage puffs,
//    then a handful of limbs born one after the other over the first lines (the sapling's lollipop head spreading
//    into a broad crown), each ending in a pom-pom of puffs, twigs with smaller ones, and fruits appearing later;
//  - the world (meters): a few round trees in the meadow around the sapling, for scale.
#include <algorithm>
#include <cmath>

#include "universes/gianttree/gianttree.hpp"

namespace {

struct Gen {
    Rng rng{1};
    GiantTreeMesh* m = nullptr;

    vec3 randUnit() {
        for (;;) {
            vec3 v(rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1));
            const float l = dot(v, v);
            if (l > 0.01f && l <= 1.f) return v / std::sqrt(l);
        }
    }
    static vec3 perp(const vec3& t) {
        const vec3 a = std::fabs(t.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
        return normalize(cross(t, a));
    }

    void vertex(const vec3& c, float r, const vec3& n, float u, const vec3& base, float space, float tb, float dur,
                float path, float phase, float bu, float bv, float seed, float death) {
        const float v[20] = {c.x, c.y, c.z, r, n.x, n.y, n.z, u, base.x, base.y, base.z, space,
                             tb, dur, path, phase, bu, bv, seed, death};
        m->bark.insert(m->bark.end(), v, v + 20);
    }
    // A puff: space 0 crown, 1 vine, 2 world; look 0 foliage, 1 fruit, 2 vine leaf.
    void puff(const vec3& p, float r, float space, float birth, float look, const vec3& clump, float path,
              float death = 99.f, float sway = 1.f) {
        const float v[16] = {p.x, p.y, p.z, r, space, birth, rng.uniform(), look,
                             clump.x, clump.y, clump.z, path, death, rng.uniform(), sway, 0.f};
        m->puffs.insert(m->puffs.end(), v, v + 16);
    }

    // A tapered tube along a curve (crown or world space), born from its base to its tip over [tb, tb + dur].
    std::vector<vec3> tube(const vec3& p0, vec3 dir, float len, float r0, float r1, int segs, int sides, float space,
                           float tb, float dur, float path0, float pathLen, float up, float wander) {
        std::vector<vec3> P(segs + 1), T(segs + 1);
        P[0] = p0;
        vec3 d = normalize(dir);
        for (int i = 1; i <= segs; i++) {
            d = normalize(d + vec3(0, up, 0) + randUnit() * wander);
            P[i] = P[i - 1] + d * (len / segs);
        }
        for (int i = 0; i <= segs; i++) T[i] = normalize(P[std::min(segs, i + 1)] - P[std::max(0, i - 1)]);
        vec3 N = perp(T[0]);
        const uint32_t first = (uint32_t)(m->bark.size() / 20);
        const float phase = rng.range(0, TAU), seed = rng.uniform();
        for (int i = 0; i <= segs; i++) {
            N = normalize(N - T[i] * dot(N, T[i]));
            const vec3 B = cross(T[i], N);
            const float u = (float)i / segs;
            const float r = lerpf(r0, r1, std::pow(u, 0.8f));
            for (int j = 0; j <= sides; j++) {
                const float a = (float)j / sides * TAU;
                const vec3 n = N * std::cos(a) + B * std::sin(a);
                vertex(P[i], r, n, u, p0, space, tb, dur, path0 + pathLen * u, phase, (float)j / sides * 3.f, u * len / std::max(r0, 1e-3f) * 0.15f,
                       seed, 99.f);
            }
        }
        for (int i = 0; i < segs; i++)
            for (int j = 0; j < sides; j++) {
                const uint32_t a = first + i * (sides + 1) + j, c = a + sides + 1;
                const uint32_t q[6] = {a, c, a + 1, a + 1, c, c + 1};
                m->index.insert(m->index.end(), q, q + 6);
            }
        return P;
    }

    // A pom-pom of puffs around c (radius rho), popping open from birth on (the inner ones first).
    void pompom(const vec3& c, float rho, float space, float birth, float path, int n) {
        puff(c, rho * 0.62f, space, birth, 0.f, c, path);
        for (int i = 0; i < n; i++) {
            // spread over the upper part of a sphere, the top fuller
            const float a = (i + rng.uniform() * 0.5f) / n * TAU;
            const float el = rng.range(-0.35f, 0.75f);
            const vec3 o(std::cos(a) * std::cos(el), std::sin(el), std::sin(a) * std::cos(el));
            puff(c + o * rho * rng.range(0.42f, 0.58f), rho * rng.range(0.4f, 0.55f), space, birth + 0.03f + 0.05f * rng.uniform(), 0.f, c,
                 path + 0.05f);
        }
    }
};

} // namespace

GiantTreeMesh buildGiantTree(uint64_t seed) {
    GiantTreeMesh mesh;
    Gen gen;
    gen.rng = Rng(seed ^ 0x7EE5ull);
    gen.m = &mesh;
    Rng& rng = gen.rng;
    mesh.wob[0] = rng.range(0.025f, 0.045f);
    mesh.wob[1] = rng.range(0.6f, 0.95f);
    mesh.wob[2] = rng.range(0.f, TAU);
    mesh.wob[3] = rng.range(0.f, TAU);

    // ---- The trunk: rings in log space (the shader gives them their place and radius at the current growth).
    {
        const int sides = 16;
        std::vector<float> ys = {-0.6f, -0.2f, 0.f};
        for (float l = std::log(0.015f); l < gtree::G_MAX + 0.2f; l += 0.034f) ys.push_back(std::exp(l));
        const uint32_t first = (uint32_t)(mesh.bark.size() / 20);
        for (float y : ys)
            for (int j = 0; j <= sides; j++) {
                const float a = (float)j / sides * TAU;
                const vec3 n(std::cos(a), 0.f, std::sin(a));
                gen.vertex(vec3(0.f, y, 0.f), 0.f, n, 0.f, vec3(0.f), 0.f, 0.f, 0.f, 0.f, 0.f, (float)j / sides * 7.f, 0.f, 0.f, 99.f);
            }
        for (size_t i = 0; i + 1 < ys.size(); i++)
            for (int j = 0; j < sides; j++) {
                const uint32_t a = first + (uint32_t)i * (sides + 1) + j, c = a + sides + 1;
                const uint32_t q[6] = {a, c, a + 1, a + 1, c, c + 1};
                mesh.index.insert(mesh.index.end(), q, q + 6);
            }
    }
    // ---- Vine leaves spiraling up the trunk (lnY, angle, size and offset on the trunk's radius).
    for (int h = 0; h < 2; h++) {
        const float off = h * PI + rng.range(-0.3f, 0.3f);
        for (float l = std::log(2.f) + h * 0.03f; l < gtree::G_MAX; l += 0.06f) {
            const float ang = l * 3.6f + off + rng.range(-0.25f, 0.25f);
            gen.puff(vec3(l, ang, rng.range(0.6f, 0.85f)), 1.f, 1.f, std::max(l + 0.03f, 3.4f + 0.5f * rng.uniform()), 2.f, vec3(0.f), 0.f);
        }
    }

    // ---- The crown (crown space).
    // the dome: the sapling's round head, always there
    {
        const vec3 c(0.f, 0.05f, 0.f);
        gen.puff(c, 0.2f, 0.f, -9.f, 0.f, c, 1.0f);
        for (int i = 0; i < 9; i++) {
            const float a = (i + 0.3f * rng.uniform()) / 9.f * TAU;
            const float el = i % 3 == 0 ? 0.65f : rng.range(-0.25f, 0.3f);
            const vec3 o(std::cos(a) * std::cos(el), std::sin(el), std::sin(a) * std::cos(el));
            gen.puff(c + o * rng.range(0.13f, 0.17f), rng.range(0.11f, 0.14f), 0.f, -9.f, 0.f, c, 1.05f);
        }
        gen.puff(c + vec3(0.f, 0.17f, 0.f), 0.12f, 0.f, -9.f, 0.f, c, 1.1f);
    }
    // the limbs, born one after the other over the first lines, each ending in a pom-pom, twigs with smaller ones
    std::vector<std::pair<vec3, float>> clumps;
    {
        const int n = 7;
        float az = rng.range(0, TAU);
        for (int k = 0; k < n; k++) {
            az += 2.39996f + rng.range(-0.25f, 0.25f);
            const float tb = 0.35f + 0.42f * k + rng.range(-0.05f, 0.05f);
            const float yb = -0.36f + 0.3f * (float)k / (n - 1) + rng.range(-0.03f, 0.03f);
            const float el = rng.range(0.35f, 0.65f);
            const vec3 dir(std::cos(az) * std::cos(el), std::sin(el), std::sin(az) * std::cos(el));
            const float len = rng.range(0.34f, 0.44f) * (k == n - 1 ? 0.8f : 1.f);
            const float dur = 0.32f;
            const vec3 p0(0.f, yb, 0.f);
            std::vector<vec3> P = gen.tube(p0, dir, len, 0.042f, 0.013f, 8, 8, 1.f, tb, dur, 1.f, 0.6f, 0.035f, 0.08f);
            const vec3 end = P.back();
            const float rho = rng.range(0.15f, 0.19f);
            const vec3 cc = end + vec3(0.f, rho * 0.35f, 0.f);
            gen.pompom(cc, rho, 0.f, tb + dur * 0.35f, 1.6f, 8);
            clumps.push_back({cc, rho});
            // twigs
            for (int t = 0; t < 2; t++) {
                const float u = rng.range(0.45f, 0.7f);
                const int i = std::min(7, (int)(u * 8));
                const vec3 bp = P[i];
                const vec3 side = normalize(cross(dir, vec3(0, 1, 0))) * (t == 0 ? 1.f : -1.f);
                const vec3 td = normalize(dir * 0.5f + side * 0.7f + vec3(0, 0.55f, 0));
                const float tl = len * rng.range(0.38f, 0.5f);
                const float ttb = tb + dur * u + 0.05f;
                std::vector<vec3> Q = gen.tube(bp, td, tl, 0.016f, 0.006f, 5, 6, 1.f, ttb, 0.25f, 1.f + 0.6f * u, 0.3f, 0.03f, 0.1f);
                const float r2 = rng.range(0.1f, 0.13f);
                const vec3 c2 = Q.back() + vec3(0.f, r2 * 0.3f, 0.f);
                gen.pompom(c2, r2, 0.f, ttb + 0.08f, 1.5f, 6);
                clumps.push_back({c2, r2});
            }
        }
    }
    // fruits (and blossoms at the tips), appearing as the tree grows grand
    for (int i = 0; i < 34; i++) {
        const auto& cl = clumps[(size_t)rng.irange(0, (int)clumps.size() - 1)];
        vec3 o = gen.randUnit();
        o.y = std::fabs(o.y) * 0.6f - 0.2f;
        o = normalize(o);
        const float birth = 2.5f + 12.f * std::pow(rng.uniform(), 1.3f);
        gen.puff(cl.first + o * cl.second * 0.98f, rng.range(0.022f, 0.034f), 0.f, birth, 1.f, cl.first, 1.7f);
    }
    // the crown's extent (for the camera)
    {
        vec3 c(0.f);
        float n = 0.f;
        for (size_t i = 0; i < mesh.puffs.size(); i += 16)
            if (mesh.puffs[i + 4] == 0.f) { c += vec3(mesh.puffs[i], mesh.puffs[i + 1], mesh.puffs[i + 2]); n += 1.f; }
        c = c / std::max(n, 1.f);
        float r = 0.f;
        for (size_t i = 0; i < mesh.puffs.size(); i += 16)
            if (mesh.puffs[i + 4] == 0.f) {
                const vec3 d = vec3(mesh.puffs[i], mesh.puffs[i + 1], mesh.puffs[i + 2]) - c;
                r = std::max(r, std::sqrt(d.x * d.x + d.z * d.z) + mesh.puffs[i + 3]);
            }
        mesh.crownC = c;
        mesh.crownR = r;
    }

    // ---- Round trees in the meadow around the sapling (world space, meters), none on the camera's side.
    for (int i = 0; i < 46; i++) {
        float a = rng.range(0.f, TAU);
        if (std::fabs(std::remainder(a - 0.3f, TAU)) < 1.1f) a += PI;
        const bool far = i >= 30;
        const float d = far ? rng.range(500.f, 2600.f) : rng.range(22.f, 190.f);
        const float h = far ? rng.range(14.f, 24.f) : rng.range(3.5f, 7.f);
        const vec3 base(std::cos(a) * d, 0.f, std::sin(a) * d);
        const float r0 = h * 0.06f;
        gen.tube(base - vec3(0.f, 0.3f, 0.f), vec3(rng.range(-0.1f, 0.1f), 1.f, rng.range(-0.1f, 0.1f)), h * 0.8f, r0, r0 * 0.6f, 3, 7, 2.f,
                 -9.f, 0.01f, 0.f, 0.f, 0.f, 0.f);
        const vec3 top = base + vec3(0.f, h * 0.78f, 0.f);
        const float rho = h * rng.range(0.32f, 0.4f);
        gen.puff(top, rho * 0.75f, 2.f, -9.f, 0.f, top, 0.f, 99.f, 0.3f);
        for (int k = 0; k < 6; k++) {
            const float b = (k + rng.uniform() * 0.4f) / 6.f * TAU;
            const float el = k == 5 ? 1.2f : rng.range(-0.2f, 0.4f);
            const vec3 o(std::cos(b) * std::cos(el), std::sin(el), std::sin(b) * std::cos(el));
            gen.puff(top + o * rho * 0.6f, rho * rng.range(0.45f, 0.58f), 2.f, -9.f, 0.f, top, 0.f, 99.f, 0.3f);
        }
    }
    return mesh;
}
