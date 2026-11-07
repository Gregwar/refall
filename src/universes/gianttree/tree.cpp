// GIANT TREE: the tree's generation (see gianttree.cpp for the world).
//
// One tree per game, from a seed, in tree units (the trunk about 100 long): recursive branching of tapered
// generalized cylinders (a trunk with buttressed roots, a dozen great limbs from its upper half that spread wide and
// curve up, then two finer orders of branches), each ring of each branch a ring of vertices along a parallel
// transport frame. Every part carries the growth at which it is born (a branch grows from its base to its tip over
// its duration, its children starting from where it has reached), so the whole tree is generated once and the
// shaders reveal it against the growth uniform: nothing is rebuilt while it grows. Parts can also die: the lower
// limbs (and the sapling's first shoots) grow while the tree is young and wither as it grows tall, like a real
// tree's self-pruning, so it keeps a young tree's proportions at every stage.
//
// The foliage is clusters of leaves (billboards): the outer branches carry the permanent ones, gathered into
// flattened pads at the ends of the limbs; every younger branch also carries "juvenile" clusters along it, which
// live until the branches growing out of it have their own leaves and then fall: so the sapling is a little bush of
// big leaves on a thin stem, and the leaves move outward as the tree ramifies, until only the crown's pads remain.
#include <algorithm>
#include <cmath>

#include "universes/gianttree/gianttree.hpp"

namespace {

struct Leaf {
    vec3 p;
    float size;
    vec3 n;        // filled once the crown is known (clump normal)
    float birth, death, path, seed, kind;
    vec3 clump;    // the clump it belongs to (its normal points away from it)
    float sway;
};

struct Gen {
    Rng rng{1};
    GiantTreeMesh* m = nullptr;
    std::vector<Leaf> leaves;
    struct Ring { float birth, y; };
    std::vector<Ring> rings;
    float maxPath = 1.f;

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
    static vec3 rotateAround(const vec3& v, const vec3& axis, float ang) {
        const float c = std::cos(ang), s = std::sin(ang);
        return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.f - c));
    }

    struct Branch {
        vec3 p0, dir;
        float len, r0, r1;
        int level;       // 0 trunk, 1 limb, 2 branch, 3 outer branch; 9 root
        float tb, dur, path0;
        float phase;
        vec3 clump;      // for outer branches: the clump center their leaves shape
        float death = 99.f; // a sapling's shoot withers away at this growth
        bool shoot = false;
    };

    void grow(const Branch& b, int depth);
};

constexpr int SEGS[4] = {30, 12, 7, 5};
constexpr int SIDES[4] = {20, 10, 7, 5};

void Gen::grow(const Branch& b, int depth) {
    const bool root = b.level == 9;
    const int L = root ? 1 : b.level;
    const int segs = root ? 9 : SEGS[L], sides = root ? 8 : SIDES[L];
    const float step = b.len / segs;
    // ---- The curve.
    std::vector<vec3> P(segs + 1), T(segs + 1);
    P[0] = b.p0;
    vec3 d = normalize(b.dir);
    const float a1 = rng.range(0, TAU), a2 = rng.range(0, TAU);
    for (int i = 1; i <= segs; i++) {
        const float t = (float)i / segs;
        if (b.level == 0) {
            // the trunk: upright, a gentle S and a slight lean
            d = normalize(vec3(0.16f * std::sin(t * 3.1f + a1) + 0.05f, 1.f, 0.14f * std::cos(t * 2.4f + a2)));
        } else if (root) {
            d = normalize(d + vec3(0, -0.11f, 0) + randUnit() * 0.12f);
        } else {
            const float up = L == 1 ? 0.075f : L == 2 ? 0.06f : 0.05f;
            // out first, then curving up toward the light; a little wander
            d = normalize(d + vec3(0, up * (0.4f + t), 0) + randUnit() * (L == 1 ? 0.07f : 0.13f));
        }
        P[i] = P[i - 1] + d * step;
    }
    for (int i = 0; i <= segs; i++) T[i] = normalize(P[std::min(segs, i + 1)] - P[std::max(0, i - 1)]);
    // ---- The rings (parallel transport).
    vec3 N = perp(T[0]);
    const uint32_t first = (uint32_t)(m->bark.size() / 20);
    auto radiusAt = [&](float u) { return lerpf(b.r0, b.r1, std::pow(u, 0.75f)); };
    float arc = 0.f;
    for (int i = 0; i <= segs; i++) {
        if (i > 0) {
            N = normalize(N - T[i] * dot(N, T[i]));
            arc += length(P[i] - P[i - 1]);
        }
        const vec3 B = cross(T[i], N);
        const float u = (float)i / segs;
        const float r = radiusAt(u);
        const float path = root ? b.path0 + (b.len - arc) : b.path0 + arc;
        maxPath = std::max(maxPath, path);
        rings.push_back({b.tb + b.dur * u, P[i].y + r});
        for (int j = 0; j <= sides; j++) {
            const float ang = (float)j / sides * TAU;
            const vec3 n = N * std::cos(ang) + B * std::sin(ang);
            float rr = r;
            if (b.level == 0) {
                // buttresses: the foot flares into lobes
                rr *= 1.f + 0.22f * std::sin(5.f * ang + a1) * std::exp(-u * 9.f);
            }
            const float circ = TAU * b.r0;
            const float v[20] = {P[i].x, P[i].y, P[i].z, rr,
                                 n.x, n.y, n.z, u,
                                 b.p0.x, b.p0.y, b.p0.z, (float)(root ? 1 : b.level),
                                 b.tb, b.dur, path, b.phase,
                                 (float)j / sides * std::max(1.f, std::round(circ / 3.f)), arc, root ? 1.f : 0.f, b.death};
            m->bark.insert(m->bark.end(), v, v + 20);
        }
    }
    for (int i = 0; i < segs; i++)
        for (int j = 0; j < sides; j++) {
            const uint32_t a = first + i * (sides + 1) + j, c = a + sides + 1;
            const uint32_t q[6] = {a, c, a + 1, a + 1, c, c + 1};
            m->index.insert(m->index.end(), q, q + 6);
        }
    if (root) return;

    auto pointAt = [&](float u, vec3* tan) {
        const float x = u * segs;
        const int i = std::min(segs - 1, (int)x);
        const float f = x - i;
        if (tan) *tan = normalize(lerp(T[i], T[i + 1], f));
        return lerp(P[i], P[i + 1], f);
    };
    auto pathAt = [&](float u) { return b.path0 + b.len * u; };

    // ---- Children.
    struct Child { float u, tb, dur; };
    std::vector<Child> kids;
    if (L <= 2 && !b.shoot) {
        // the trunk: limbs all along it; the lower ones grow while the tree is young and are shed as it grows
        // tall (so it has a young tree's shape at every stage), the upper ones spread wide and end in pads of
        // foliage. A limb's branches grow toward its end, around its pad.
        const int n = L == 0 ? 17 : L == 1 ? 7 : 5;
        const float u0 = L == 0 ? 0.07f : L == 1 ? 0.38f : 0.3f;
        const float phyllo = 2.39996f;
        float az = rng.range(0, TAU);
        for (int k = 0; k < n; k++) {
            const float t = (k + 0.3f + 0.5f * rng.uniform()) / n;
            const float u = std::min(0.985f, u0 + (1.f - u0) * t);
            vec3 tan;
            const vec3 p = pointAt(u, &tan);
            az += phyllo + rng.range(-0.25f, 0.25f);
            Branch c;
            c.death = b.death;
            float theta, len, rk;
            if (L == 0) {
                if (u < 0.42f) {
                    theta = rng.range(0.85f, 1.1f);
                    len = 100.f * lerpf(0.11f, 0.26f, u / 0.42f);
                    rk = 0.4f;
                    c.death = b.tb + b.dur * std::min(1.f, u + 0.42f) + 0.04f;
                } else {
                    const float tt = (u - 0.42f) / 0.58f;
                    theta = lerpf(1.3f, 0.45f, tt);
                    len = 100.f * lerpf(0.58f, 0.24f, tt);
                    rk = lerpf(0.6f, 0.42f, tt);
                }
            } else if (L == 1) {
                theta = lerpf(0.95f, 0.65f, t);
                len = b.len * lerpf(0.38f, 0.26f, t);
                rk = lerpf(0.7f, 0.5f, t);
            } else {
                theta = lerpf(0.9f, 0.65f, t);
                len = b.len * lerpf(0.62f, 0.45f, t);
                rk = lerpf(0.72f, 0.5f, t);
            }
            theta *= rng.range(0.88f, 1.12f);
            const vec3 side = rotateAround(perp(tan), tan, az);
            vec3 dir = normalize(tan * std::cos(theta) + side * std::sin(theta));
            if (L >= 1 && dir.y < -0.2f) {
                // no branch growing down into the crown: mirror it upward
                dir.y = -dir.y * 0.4f;
                dir = normalize(dir);
            }
            c.level = L + 1;
            c.p0 = p;
            c.dir = dir;
            c.len = len * rng.range(0.86f, 1.12f);
            c.r0 = radiusAt(u) * rk;
            c.r1 = c.r0 * (L == 2 ? 0.25f : 0.16f);
            c.tb = b.tb + b.dur * u + (L == 0 ? 0.01f : 0.014f);
            c.dur = L == 0 ? 0.2f * std::sqrt(c.len / 45.f) : L == 1 ? 0.11f : 0.075f;
            c.path0 = pathAt(u);
            c.phase = rng.range(0, TAU);
            // the pad of foliage a limb ends in (its branches and leaves gather around it)
            c.clump = L == 0 ? p + dir * c.len * 0.8f + vec3(0.f, c.len * 0.16f, 0.f) : b.clump;
            kids.push_back({u, c.tb, c.dur});
            grow(c, depth + 1);
        }
    }

    // ---- Leaves.
    if (b.shoot) {
        // a sapling's shoot: a tuft of big young leaves toward its end
        for (int k = 0; k < 5; k++) {
            const float u = 0.35f + 0.65f * (k + rng.uniform() * 0.5f) / 4.5f;
            vec3 tan;
            const vec3 p = pointAt(u, &tan);
            const vec3 off = normalize(randUnit() + vec3(0, 0.5f, 0));
            Leaf lf;
            lf.p = p + off * 1.6f;
            lf.size = rng.range(6.f, 8.f);
            lf.birth = b.tb + b.dur * u + 0.004f;
            lf.death = b.death - 0.02f;
            lf.path = pathAt(u);
            lf.seed = rng.uniform();
            lf.kind = 0.f;
            lf.clump = p - off * 4.f;
            lf.sway = 1.f;
            leaves.push_back(lf);
        }
    } else if (L <= 2) {
        // juvenile clusters along the branch, falling once the branches growing out of it have leaves
        const float spacing = L == 0 ? 3.2f : L == 1 ? 2.4f : 2.2f;
        const float size = L == 0 ? 8.f : L == 1 ? 7.f : 5.6f;
        const int n = std::max(2, (int)(b.len / spacing));
        for (int k = 0; k < n; k++) {
            const float u = std::min(1.f, 0.1f + 0.9f * (k + rng.uniform() * 0.6f) / n);
            vec3 tan;
            const vec3 p = pointAt(u, &tan);
            float death = 9.f;
            for (const Child& c : kids)
                if (c.u >= u - 0.12f) death = std::min(death, c.tb + c.dur * 0.55f);
            if (death > 8.f) death = b.tb + b.dur + 0.15f;
            const float birth = b.tb + b.dur * u + 0.006f;
            // the young stem keeps its leaves near its growing tip only
            if (L == 0) death = std::min(death, birth + 0.05f);
            death = std::min(death, b.death - 0.03f);
            if (death <= birth + 0.02f) continue;
            const vec3 off = normalize(rotateAround(perp(tan), tan, rng.range(0, TAU)) + vec3(0, 0.35f, 0));
            Leaf lf;
            lf.p = p + off * (radiusAt(u) * 0.5f + size * 0.25f);
            lf.size = size * rng.range(0.8f, 1.15f);
            lf.birth = birth;
            lf.death = death;
            lf.path = pathAt(u);
            lf.seed = rng.uniform();
            lf.kind = 0.f;
            lf.clump = p - off * size * 0.6f;
            lf.sway = 0.4f + 0.3f * L;
            leaves.push_back(lf);
        }
    } else {
        // the crown's clumps: clusters along the outer half of every outer branch and at its tip
        const int n = 13;
        for (int k = 0; k < n; k++) {
            const float u = k == n - 1 ? 1.f : 0.25f + 0.75f * (k + rng.uniform()) / (n - 1);
            vec3 tan;
            const vec3 p = pointAt(u, &tan);
            const vec3 off = randUnit();
            Leaf lf;
            lf.p = p + off * rng.range(0.8f, 3.f) + vec3(0, 0.8f, 0);
            lf.p.y = lerpf(lf.p.y, b.clump.y + 1.f, 0.3f);
            lf.size = rng.range(5.f, 7.f);
            lf.birth = b.tb + b.dur * u + 0.02f + 0.03f * rng.uniform();
            lf.death = b.death < 50.f ? b.death - 0.03f : 99.f;
            lf.path = pathAt(u) + 1.f;
            lf.seed = rng.uniform();
            lf.kind = 1.f;
            lf.clump = b.clump;
            lf.sway = 1.f;
            leaves.push_back(lf);
        }
    }
    // ---- Lanterns hanging under the branches (they light once the tree is grand).
    if (b.death > 50.f && ((L == 2 && rng.chance(0.4f)) || (L == 1 && rng.chance(0.6f)))) {
        const float u = rng.range(0.35f, 0.85f);
        const vec3 p = pointAt(u, nullptr);
        const float birth = std::max(0.42f + 0.25f * rng.uniform(), b.tb + b.dur * u + 0.05f);
        const float v[8] = {p.x, p.y - radiusAt(u) - rng.range(1.f, 2.5f), p.z, rng.range(0.9f, 1.5f),
                            birth, rng.uniform(), pathAt(u), 0.f};
        m->lanterns.insert(m->lanterns.end(), v, v + 8);
    }
}

} // namespace

float GiantTreeMesh::sample(const float* t, float g) const {
    const float x = saturate(g) * (TABLE - 1);
    const int i = std::min(TABLE - 2, (int)x);
    return lerpf(t[i], t[i + 1], x - i);
}

GiantTreeMesh buildGiantTree(uint64_t seed, int style) {
    (void)style;
    GiantTreeMesh mesh;
    Gen gen;
    gen.rng = Rng(seed ^ 0x7EE5ull);
    gen.m = &mesh;
    Rng& rng = gen.rng;

    // ---- The trunk, its roots.
    Gen::Branch trunk;
    trunk.level = 0;
    trunk.p0 = vec3(0, -1.5f, 0);
    trunk.dir = vec3(0, 1, 0);
    trunk.len = 100.f;
    trunk.r0 = 8.5f;
    trunk.r1 = 1.0f;
    trunk.tb = 0.f;
    trunk.dur = 0.45f;
    trunk.phase = 0.f;
    const float rootLen = 26.f;
    trunk.path0 = rootLen;
    mesh.trunkR = trunk.r0;
    gen.grow(trunk, 0);
    // the sapling's shoots
    {
        const int n = 3;
        float az = rng.range(0, TAU);
        for (int i = 0; i < n; i++) {
            az += 2.39996f + rng.range(-0.3f, 0.3f);
            const float u = 0.03f + 0.05f * (i + rng.uniform() * 0.5f) / n;
            Gen::Branch b;
            b.level = 2;
            b.shoot = true;
            b.p0 = vec3(0.f, -1.5f + 100.f * u, 0.f);
            b.dir = normalize(vec3(std::cos(az), 0.9f + 0.3f * rng.uniform(), std::sin(az)));
            b.len = rng.range(8.f, 12.f) * (1.f - 0.3f * i / n);
            b.r0 = 0.9f;
            b.r1 = 0.25f;
            b.tb = 0.45f * u + 0.004f;
            b.dur = 0.03f;
            b.path0 = rootLen + 100.f * u;
            b.phase = rng.range(0, TAU);
            b.death = rng.range(0.16f, 0.24f);
            gen.grow(b, 1);
        }
    }
    const int nRoots = 7;
    for (int i = 0; i < nRoots; i++) {
        const float az = (i + rng.range(-0.3f, 0.3f)) / nRoots * TAU;
        const vec3 out(std::cos(az), 0, std::sin(az));
        Gen::Branch r;
        r.level = 9;
        r.p0 = out * (trunk.r0 * 0.55f) + vec3(0, rng.range(2.5f, 5.f), 0);
        r.dir = normalize(out + vec3(0, -0.28f, 0));
        r.len = rootLen * rng.range(0.7f, 1.05f);
        r.r0 = trunk.r0 * rng.range(0.34f, 0.44f);
        r.r1 = 0.35f;
        r.tb = 0.03f + 0.25f * (float)i / nRoots;
        r.dur = 0.35f;
        r.path0 = rootLen - r.len;
        r.phase = 0.f;
        gen.grow(r, 1);
    }

    // ---- The crown, then the leaves' clump normals (away from their clump, out of the crown, a little up).
    vec3 crown(0.f);
    float cw = 0.f;
    for (const Leaf& l : gen.leaves)
        if (l.kind > 0.5f) { crown += l.p; cw += 1.f; }
    crown = cw > 0.f ? crown / cw : vec3(0, 80, 0);
    float crownR = 1.f;
    for (const Leaf& l : gen.leaves)
        if (l.kind > 0.5f) crownR += length(l.p - crown);
    crownR = cw > 0.f ? crownR / cw * 1.35f : 40.f;
    for (Leaf& l : gen.leaves) {
        vec3 d = l.p - l.clump;
        if (l.kind > 0.5f) d.y *= 2.4f;
        vec3 n = normalize(d) * 0.75f + (l.p - crown) / crownR * 0.3f + vec3(0, 0.2f, 0);
        l.n = normalize(n);
        const float v[16] = {l.p.x, l.p.y, l.p.z, l.size, l.n.x, l.n.y, l.n.z, l.birth,
                             l.death, l.path, l.seed, l.kind, l.sway, 0.f, 0.f, 0.f};
        mesh.leaves.insert(mesh.leaves.end(), v, v + 16);
    }
    mesh.maxPath = gen.maxPath;

    // ---- Growth tables: the height of what is born, the foliage's center and spread.
    for (int k = 0; k < GiantTreeMesh::TABLE; k++) {
        const float g = (float)k / (GiantTreeMesh::TABLE - 1);
        float h = 0.5f;
        for (const Gen::Ring& r : gen.rings)
            if (r.birth <= g) h = std::max(h, r.y);
        vec3 c(0.f);
        float n = 0.f;
        for (const Leaf& l : gen.leaves)
            if (l.birth <= g && l.death > g) { c += l.p; n += 1.f; h = std::max(h, l.p.y + l.size * 0.4f); }
        c = n > 0.f ? c / n : vec3(0, h * 0.6f, 0);
        float r = 0.f;
        for (const Leaf& l : gen.leaves)
            if (l.birth <= g && l.death > g) r += dot(l.p - c, l.p - c);
        r = n > 0.f ? std::sqrt(r / n) * 1.3f + 2.f : h * 0.4f;
        mesh.height[k] = h;
        mesh.crownX[k] = c.x;
        mesh.crownY[k] = c.y;
        mesh.crownZ[k] = c.z;
        mesh.crownR[k] = r;
    }
    return mesh;
}
