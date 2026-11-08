// MEGACITY: the city's plan, generated once per game from a seed. Every building carries the frontier at which it
// rises (its organic distance D, see mcity::field): the shaders raise it out of the ground as the frontier passes it,
// so nothing is uploaded while the city grows. Some carry a death too: the old town sinks back where the downtowns'
// towers rise.
//
// The hut stands on the riverbank at the origin. The village around it is laid out by hand (houses along the river
// and the road, a church, gardens filling in later); a medieval wall follows the town's edge where the frontier
// stood at ~700 m. Past it the lots are square, ever larger with D (the older the district, the smaller its
// buildings), on grids turned district by district (a patchwork of street grids, with plazas where they meet), each
// built in the style of its era: town houses with steep roofs, stone perimeter blocks around courtyards, modern
// mid-rise, megablocks, then arcologies kilometers high. Highways, the river, the bay, parks and green fingers stay
// free. The downtowns are planted on their own: the main one by the old town, more in the satellite towns along the
// highways, each a cluster of towers rising over many lines, taller and taller (setbacks, tapers, twists, octagons,
// slabs, spires, lit crowns), then megatalls.
#include <algorithm>
#include <cmath>
#include <numeric>

#include "universes/megacity/megacity.hpp"

namespace {

using namespace mcity;

enum Kind { K_HUT = 0, K_HOUSE, K_TOWN, K_CHURCH, K_WALL, K_CLASSIC, K_GLASS, K_SUPER, K_ARCO, K_ELEVATOR, K_GEAR, K_BRIDGE };
constexpr int FL = 20;   // floats a building

struct Builder {
    MegaCityPlan& plan;
    std::vector<float> births;
    std::vector<float> rows;
    std::vector<float> beacons;

    // A building at (x, z) on the sphere (flat coordinates: geodesic polar around the hut), turned by yaw, raised by
    // y0 (a setback's tier).
    // A world point at (x, z) on the ground, raised by y.
    static vec3 world(float x, float z, float y) {
        const float r = std::sqrt(x * x + z * z) + 1e-4f, phi = std::atan2(z, x), th = r / R_EARTH;
        const float st = std::sin(th), ct = std::cos(th), s2 = std::sin(th * 0.5f);
        const vec3 n(st * std::cos(phi), ct, st * std::sin(phi));
        return vec3(R_EARTH * st * std::cos(phi), -2.f * R_EARTH * s2 * s2, R_EARTH * st * std::sin(phi)) + n * y;
    }
    void add(float x, float z, float yaw, float hw, float hd, float h, float roofH, float taper, int kind, float seed,
             float y0, float birth, float death = 1e30f, float twist = 0.f) {
        const float r = std::sqrt(x * x + z * z) + 1e-4f;
        const float phi = std::atan2(z, x);
        const float th = r / R_EARTH;
        const float st = std::sin(th), ct = std::cos(th), sp = std::sin(phi), cp = std::cos(phi);
        const vec3 n(st * cp, ct, st * sp);
        const float s2 = std::sin(th * 0.5f);
        const vec3 base(R_EARTH * st * cp, -2.f * R_EARTH * s2 * s2, R_EARTH * st * sp);
        // the axes: x along the flat map's x direction turned by yaw, projected on the tangent plane
        vec3 ax(std::cos(yaw), 0.f, std::sin(yaw));
        ax = normalize(ax - n * dot(ax, n));
        const float ks = 0.12f * birth + 40.f;
        const float key = vnL(x / ks, z / ks);
        const float q[FL] = {base.x, base.y, base.z, birth, ax.x, ax.y, ax.z, hw, hd, h, roofH, taper,
                             (float)kind, seed, key, y0, death, twist, 0.f, 0.f};
        rows.insert(rows.end(), q, q + FL);
        births.push_back(birth);
        if (h + y0 > 120.f && (kind == K_GLASS || kind == K_SUPER || kind == K_ARCO) && taper > 0.9f) {
            const vec3 top = base + n * (y0 + h + roofH);
            const float b[4] = {top.x, top.y, top.z, birth};
            beacons.insert(beacons.end(), b, b + 4);
        }
    }
};

// The camera's side of the hut early on (it stands back toward -x, -z): kept as an open meadow near the hut, so
// nothing rises between the camera and the hut.
bool inView(float x, float z, float rMax) {
    const float r = std::sqrt(x * x + z * z);
    if (r > rMax || r < 1e-3f) return false;
    return (x * -0.86f + z * -0.51f) / r > std::cos(1.0f);
}
bool inRiver(float x, float z, float margin) {
    if (std::fabs(z) > 80000.f) return false;
    return std::fabs(x - riverX(z)) < riverHalfW(z) + margin;
}
float roadDist(float x, float z) {
    const float r = std::sqrt(x * x + z * z) + 1e-3f, ph = std::atan2(z, x);
    float d = 1e30f;
    for (int k = 0; k < ROADS; k++) {
        float a = ph - roadAngle(k, r);
        a -= TAU_F * std::floor(a / TAU_F + 0.5f);
        d = std::min(d, std::fabs(a) * r);
    }
    return d;
}

// A point on a building's footprint: (u, v) in [-1, 1] of its half sizes, turned by yaw.
void local(float x, float z, float yaw, float u, float v, float& ox, float& oz) {
    ox = x + std::cos(yaw) * u - std::sin(yaw) * v;
    oz = z + std::sin(yaw) * u + std::cos(yaw) * v;
}
// A house's chimney (its smoke is drawn from plan.chimneys): on the slope, rising past the ridge.
void addChimney(Builder& b, Rng& rng, float x, float z, float yaw, float hw, float hd, float h, float roofH, float birth, float death) {
    if (!rng.chance(0.6f)) return;
    float cx, cz;
    const float u = rng.range(-0.6f, 0.6f) * hw, v = (rng.chance(0.5f) ? 0.45f : -0.45f) * hd;
    local(x, z, yaw, u, v, cx, cz);
    const float ch = roofH * 0.75f + 0.9f;
    b.add(cx, cz, yaw, 0.32f, 0.32f, ch, 0.f, 0.f, K_GEAR, 0.1f + 0.15f * rng.uniform(), h, birth, death);
    const vec3 top = Builder::world(cx, cz, h + ch);
    const float q[4] = {top.x, top.y, top.z, birth};
    b.plan.chimneys.insert(b.plan.chimneys.end(), q, q + 4);
}
// Rooftop plant on a flat roof: air conditioning units, a water tank or two, an antenna.
void addRoofGear(Builder& b, Rng& rng, float x, float z, float yaw, float hw, float hd, float top, float birth, float death, float scale) {
    const int n = rng.irange(1, 3);
    for (int i = 0; i < n; i++) {
        float gx, gz;
        local(x, z, yaw, rng.range(-0.6f, 0.6f) * hw, rng.range(-0.6f, 0.6f) * hd, gx, gz);
        const float t = rng.uniform();
        if (t < 0.5f) b.add(gx, gz, yaw, 1.6f * scale, 1.1f * scale, 1.4f * scale, 0.f, 0.f, K_GEAR, 0.35f + 0.2f * rng.uniform(), top, birth, death);
        else if (t < 0.8f) b.add(gx, gz, yaw + 0.785f, 1.3f * scale, 1.3f * scale, 2.6f * scale, 1.1f * scale, 0.f, K_GEAR, 0.62f + 0.15f * rng.uniform(), top, birth, death);
        else b.add(gx, gz, yaw, 0.15f * scale, 0.15f * scale, 9.f * scale, 0.f, 0.7f, K_GEAR, 0.4f, top, birth, death);
    }
}

// A tower of one of the silhouettes, its base at (x, z): setbacks, a taper, a twist, an octagon, a slab, a pyramid
// top; most with a spire.
void addTower(Builder& b, Rng& rng, float x, float z, float hw, float h, float birth, int type, float death = 1e30f) {
    const int kind = h > 260.f ? K_SUPER : K_GLASS;
    const float sd = rng.uniform();
    const float yaw = rng.range(0.f, 1.5708f);
    const float hd = hw * rng.range(0.75f, 1.f);
    const bool spire = h > 150.f && rng.chance(0.75f);
    float top = h;
    switch (type) {
    case 0:   // setbacks
        b.add(x, z, yaw, hw, hd, h * 0.5f, 0.f, 0.03f, kind, sd, 0.f, birth, death);
        b.add(x, z, yaw, hw * 0.78f, hd * 0.78f, h * 0.28f, 0.f, 0.04f, kind, sd, h * 0.5f, birth, death);
        b.add(x, z, yaw, hw * 0.55f, hd * 0.55f, h * 0.16f, 0.f, 0.08f, kind, sd, h * 0.78f, birth, death);
        top = h * 0.94f;
        break;
    case 1:   // tapered
        b.add(x, z, yaw, hw * 1.15f, hd * 1.15f, h * 0.92f, 0.f, 0.42f, kind, sd, 0.f, birth, death);
        top = h * 0.92f;
        break;
    case 2:   // twisted
        b.add(x, z, yaw, hw, hw, h * 0.95f, 0.f, 0.22f, kind, sd, 0.f, birth, death, rng.range(0.9f, 1.6f) * (rng.chance(0.5f) ? 1.f : -1.f));
        top = h * 0.95f;
        break;
    case 3:   // octagonal (two prisms turned an eighth of a turn), a crown
        b.add(x, z, yaw, hw, hw, h * 0.85f, 0.f, 0.06f, kind, sd, 0.f, birth, death);
        b.add(x, z, yaw + 0.7854f, hw, hw, h * 0.85f, 0.f, 0.06f, kind, sd, 0.f, birth, death);
        b.add(x, z, yaw, hw * 0.62f, hw * 0.62f, h * 0.1f, 0.f, 0.2f, kind, sd, h * 0.85f, birth, death);
        top = h * 0.95f;
        break;
    case 4:   // a slab
        b.add(x, z, yaw, hw * 1.7f, hd * 0.65f, h * 0.8f, 0.f, 0.f, kind, sd, 0.f, birth, death);
        top = h * 0.8f;
        break;
    default:  // a pyramid top
        b.add(x, z, yaw, hw, hd, h * 0.78f, 0.f, 0.02f, kind, sd, 0.f, birth, death);
        b.add(x, z, yaw, hw * 0.98f, hd * 0.98f, h * 0.2f, 0.f, 1.f, kind, sd, h * 0.78f, birth, death);
        top = h * 0.98f;
        break;
    }
    if (spire && type != 4 && type != 5) b.add(x, z, yaw, hw * 0.1f, hw * 0.1f, h * 0.16f, 0.f, 1.f, kind, sd, top, birth, death);
    // plant on the roof of the flat-topped ones
    if (type == 0 || type == 4) addRoofGear(b, rng, x, z, yaw, type == 0 ? hw * 0.55f : hw * 1.7f, type == 0 ? hd * 0.55f : hd * 0.65f, top, birth, death, std::max(1.f, hw / 18.f));
    // a crane on the site while the tower rises: a mast beside it, its jib out over it (gone once it is built)
    if (rng.chance(0.4f) && h > 80.f) {
        float mx, mz;
        local(x, z, yaw, hw * 1.25f, 0.f, mx, mz);
        const float mh = h * 1.08f, ms = std::max(1.2f, h * 0.006f);
        const float d = std::min(death, birth * 1.13f);
        b.add(mx, mz, yaw, ms, ms, mh, 0.f, 0.f, K_GEAR, 0.9f, 0.f, birth * 0.995f, d);
        float jx, jz;
        local(x, z, yaw, hw * 1.25f - h * 0.12f, 0.f, jx, jz);
        b.add(jx, jz, yaw, h * 0.17f, ms * 0.7f, ms * 1.2f, 0.f, 0.f, K_GEAR, 0.9f, mh - ms * 2.f, birth * 0.995f, d);
    }
}

// A downtown: towers around a center, founded when the frontier reaches F0, rising until it is k times farther, the
// cluster spreading as it goes; the tallest near the core.
void addDowntown(Builder& b, Rng& rng, float cx, float cz, float F0, float k, int n, float r0, float r1, std::vector<vec3>& placed) {
    for (int j = 0; j < n; j++) {
        const float t = (j + rng.uniform()) / n;
        const float birth = F0 * std::exp(t * std::log(k));
        const float rmax = r0 + (r1 - r0) * std::pow(t, 0.7f);
        float x = 0.f, z = 0.f, h = 0.f, hw = 0.f;
        bool ok = false;
        for (int tries = 0; tries < 12 && !ok; tries++) {
            const float rho = rmax * std::sqrt(rng.uniform()), a = rng.range(0.f, TAU);
            x = cx + rho * std::cos(a);
            z = cz + rho * std::sin(a);
            if (inRiver(x, z, 40.f) || atSea(x, z) || roadDist(x, z) < 30.f) continue;
            const float g = std::exp(rng.range(-0.45f, 0.35f));
            h = towerH(birth) * g * (1.25f - 0.55f * rho / std::max(rmax, 1.f));
            hw = std::min(std::max(0.075f * h, 13.f), 95.f) * rng.range(0.85f, 1.15f);
            ok = true;
            for (const vec3& p : placed) {
                const float dx = p.x - x, dz = p.y - z;
                if (dx * dx + dz * dz < (p.z + hw) * (p.z + hw) * 1.6f) { ok = false; break; }
            }
        }
        if (!ok) continue;
        placed.push_back(vec3(x, z, hw));
        const float u = rng.uniform();
        const int type = u < 0.3f ? 0 : u < 0.5f ? 1 : u < 0.66f ? 2 : u < 0.8f ? 3 : u < 0.9f ? 4 : 5;
        addTower(b, rng, x, z, hw, h, birth, type);
    }
}

} // namespace

MegaCityPlan buildMegaCity(uint64_t seed) {
    MegaCityPlan plan;
    Builder b{plan, {}, {}, {}};
    Rng rng(seed ^ 0xC17Eull);
    satellites(plan.sats);
    const Sat* S = plan.sats;
    auto D = [&](float x, float z) { return field(x, z, S); };
    const float dcx = plan.downtown[0], dcz = plan.downtown[1];
    // the old town makes way for the downtown's towers, from its core outward
    auto deathAt = [&](float x, float z) {
        const float d = std::hypot(x - dcx, z - dcz);
        if (d > 2400.f) return 1e30f;
        return SKY_F0 * (1.05f + 2.5f * d / 2400.f);
    };

    // ---- The hut: on the bank, facing the river (+x).
    b.add(0.f, 0.f, 0.75f, 2.0f, 1.55f, 2.2f, 1.7f, 0.f, K_HUT, 0.5f, 0.f, 0.f);

    // ---- The village: houses along the river and the road west, born with their organic distance.
    std::vector<vec2> placed;
    placed.push_back(vec2{0.f, 0.f});
    {
        int tries = 0;
        while ((int)placed.size() < 230 && tries < 9000) {
            tries++;
            const float r = 7.f + 260.f * std::pow(rng.uniform(), 1.15f);
            const float a = rng.range(0.f, TAU);
            const float x = r * std::cos(a), z = r * std::sin(a);
            if (inRiver(x, z, 5.f) || inView(x, z, 220.f)) continue;
            const float d = D(x, z);
            if (d > 230.f) continue;
            if (std::fabs(x + 40.f) < 14.f && std::fabs(z - 26.f) < 14.f) continue;   // the church
            bool ok = true;
            for (const vec2& p : placed)
                if ((p.x - x) * (p.x - x) + (p.y - z) * (p.y - z) < 9.5f * 9.5f) { ok = false; break; }
            if (!ok) continue;
            placed.push_back(vec2{x, z});
            const float hw = rng.range(2.3f, 3.6f), hd = rng.range(2.6f, 4.4f);
            const float h = rng.range(2.6f, 4.8f) * (d > 60.f ? 1.25f : 1.f);
            const float yaw = rng.range(0.f, TAU);
            const float rh = hw * rng.range(0.75f, 1.0f), hb = std::max({5.f, d, 0.7f * r});
            b.add(x, z, yaw, hw, hd, h, rh, 0.f, K_HOUSE, rng.uniform(), 0.f, hb, deathAt(x, z));
            addChimney(b, rng, x, z, yaw, hw, hd, h, rh, hb, deathAt(x, z));
        }
        // as the town grows around it, the village fills in
        tries = 0;
        int infill = 0;
        while (infill < 330 && tries < 12000) {
            tries++;
            const float r = 10.f + 250.f * std::sqrt(rng.uniform());
            const float a = rng.range(0.f, TAU);
            const float x = r * std::cos(a), z = r * std::sin(a);
            if (inRiver(x, z, 4.f) || inView(x, z, 120.f)) continue;
            if (D(x, z) > 230.f) continue;
            if (std::fabs(x + 40.f) < 14.f && std::fabs(z - 26.f) < 14.f) continue;
            bool ok = true;
            for (const vec2& p : placed)
                if ((p.x - x) * (p.x - x) + (p.y - z) * (p.y - z) < 7.2f * 7.2f) { ok = false; break; }
            if (!ok) continue;
            placed.push_back(vec2{x, z});
            infill++;
            const float hw = rng.range(2.2f, 3.2f), hd = rng.range(2.4f, 3.8f);
            const float yw = rng.range(0.f, TAU), hh = rng.range(3.5f, 7.f), rh = hw * rng.range(0.8f, 1.05f), hb = rng.range(240.f, 480.f);
            b.add(x, z, yw, hw, hd, hh, rh, 0.f, K_TOWN, rng.uniform(), 0.f, hb);
            addChimney(b, rng, x, z, yw, hw, hd, hh, rh, hb, 1e30f);
        }
        // the church: a nave and a bell tower with its spire
        const float cb = std::max(60.f, D(-40.f, 26.f));
        b.add(-40.f, 26.f, 0.3f, 4.5f, 10.f, 9.f, 4.5f, 0.f, K_CHURCH, 0.3f, 0.f, cb);
        b.add(-36.5f, 36.5f, 0.3f, 2.6f, 2.6f, 19.f, 0.f, 0.f, K_CHURCH, 0.6f, 0.f, cb);
        b.add(-36.5f, 36.5f, 0.3f, 2.7f, 2.7f, 14.f, 0.f, 1.f, K_CHURCH, 0.7f, 19.f, cb);
    }

    // ---- The medieval wall: along the town's edge where the frontier stood at ~700 m, towers, gates on the roads.
    {
        const float WD = 700.f;
        const int N = 110;
        std::vector<vec2> pts(N);
        for (int i = 0; i < N; i++) {
            const float a = i * TAU / N;
            float r = 60.f;
            while (r < 4000.f && D(r * std::cos(a), r * std::sin(a)) < WD) r *= 1.03f;
            pts[i] = vec2{r * std::cos(a), r * std::sin(a)};
        }
        for (int i = 0; i < N; i++) {
            const vec2 p = pts[i], q = pts[(i + 1) % N];
            const float mx = (p.x + q.x) * 0.5f, mz = (p.y + q.y) * 0.5f;
            const float len = std::hypot(q.x - p.x, q.y - p.y);
            if (len > 220.f || inRiver(mx, mz, 6.f) || atSea(mx, mz)) continue;
            const float yaw = std::atan2(q.y - p.y, q.x - p.x);
            if (roadDist(mx, mz) < 18.f) {
                // a gate: two towers
                for (int s = -1; s <= 1; s += 2) {
                    const float gx = mx + s * 12.f * std::cos(yaw), gz = mz + s * 12.f * std::sin(yaw);
                    b.add(gx, gz, yaw, 4.5f, 4.5f, 17.f, 0.f, 0.f, K_WALL, 0.2f, 0.f, WD * 1.02f);
                    b.add(gx, gz, yaw, 4.8f, 4.8f, 7.f, 0.f, 1.f, K_WALL, 0.25f, 17.f, WD * 1.02f);
                }
                continue;
            }
            b.add(mx, mz, yaw, len * 0.5f + 0.6f, 2.2f, 10.f, 0.f, 0.f, K_WALL, 0.f, 0.f, WD * 1.02f, deathAt(mx, mz));
            if (i % 4 == 0) {
                b.add(p.x, p.y, yaw, 4.f, 4.f, 15.f, 0.f, 0.f, K_WALL, 0.4f, 0.f, WD * 1.02f, deathAt(p.x, p.y));
                b.add(p.x, p.y, yaw, 4.3f, 4.3f, 6.f, 0.f, 1.f, K_WALL, 0.45f, 15.f, WD * 1.02f, deathAt(p.x, p.y));
            }
        }
    }

    // ---- The lots: square, on grids turned district by district, sized by D (an octave of D a level).
    const float L0 = 14.f;
    for (int l = 0; l < 14; l++) {
        const float Ll = L0 * std::pow(2.f, (float)l);
        const float d0 = Ll / 0.07f, d1 = 2.f * d0;
        if (d0 > 1.3e6f) break;
        const float C = 12.f * Ll;
        const float ext = std::min(d1 * 5.5f, 4.0e6f);
        const int nc = (int)std::ceil(ext / C);
        for (int cxi = -nc; cxi < nc; cxi++)
            for (int czi = -nc; czi < nc; czi++) {
                const float ccx = (cxi + 0.5f) * C, ccz = (czi + 0.5f) * C;
                // a quick look at the district: skip it when its D is far from this level's range
                const float dc = D(ccx, ccz);
                if (dc < d0 * 0.45f - C || dc > d1 * 2.2f + C) continue;
                const float ang = (hashLot(cxi + 31.f * l, czi - 17.f * l) - 0.5f) * 1.3f;
                const float ca = std::cos(ang), sa = std::sin(ang);
                for (int i = -6; i < 6; i++)
                    for (int j = -6; j < 6; j++) {
                        const float lx = (i + 0.5f) * Ll, lz = (j + 0.5f) * Ll;
                        const float x = ccx + ca * lx - sa * lz, z = ccz + sa * lx + ca * lz;
                        if (std::floor(x / C) != (float)cxi || std::floor(z / C) != (float)czi) continue;
                        const float d = D(x, z);
                        if (d < d0 || d >= d1) continue;
                        if (atSea(x, z) || inRiver(x, z, Ll * 0.6f)) continue;
                        if (roadDist(x, z) < std::max(10.f, 0.004f * std::hypot(x, z)) + Ll * 0.5f) continue;
                        if (ofbm(x / (Ll * 9.f) + 31.f, z / (Ll * 9.f)) > 0.7f) continue;   // a park, a green finger
                        if (d < 300.f && std::hypot(x, z) < 280.f) continue;                  // the village own
                        if (d < 700.f && inView(x, z, 450.f)) continue;                       // nothing between the early camera and the hut
                        const float birth = d * rng.range(0.98f, 1.02f);
                        const float death = deathAt(x, z);
                        if (death < birth * 1.02f) continue;
                        const float yaw = ang;
                        if (d < 1300.f) {
                            // town houses with steep roofs, now and then a tower
                            const int k = Ll >= 28.f ? 2 : 1;
                            const float sub = Ll / k;
                            for (int a = 0; a < k; a++)
                                for (int c = 0; c < k; c++) {
                                    const float ox = ((a + 0.5f) / k - 0.5f) * Ll, oz = ((c + 0.5f) / k - 0.5f) * Ll;
                                    const float hx = x + ca * ox - sa * oz, hz = z + sa * ox + ca * oz;
                                    const float hw = sub * rng.range(0.3f, 0.4f), hd = sub * rng.range(0.32f, 0.42f);
                                    const float h = rng.range(6.f, 12.f) * std::min(1.6f, std::max(1.f, sub / 14.f));
                                    const float rh = std::min(hw, 7.f) * rng.range(0.8f, 1.1f);
                                    b.add(hx, hz, yaw, hw, hd, h, rh, 0.f, K_TOWN, rng.uniform(), 0.f, birth, death);
                                    if (d < 900.f) addChimney(b, rng, hx, hz, yaw, hw, hd, h, rh, birth, death);
                                }
                            if (rng.chance(0.025f)) {
                                const float tw = std::min(Ll * 0.12f, 5.f), th = rng.range(24.f, 40.f);
                                b.add(x, z, yaw, tw, tw, th, 0.f, 0.f, K_CHURCH, rng.uniform(), 0.f, birth, death);
                                b.add(x, z, yaw, tw * 1.05f, tw * 1.05f, th * 0.45f, 0.f, 1.f, K_CHURCH, rng.uniform(), th, birth, death);
                            }
                        } else if (d < SKY_F0) {
                            // stone perimeter blocks around a courtyard, mansards, now and then a dome
                            const float w = Ll * 0.4f, dep = Ll * rng.range(0.1f, 0.14f);
                            const float h = rng.range(18.f, 34.f) * (1.f + 0.5f * smoothstepf(1300.f, SKY_F0, d));
                            const float sd = rng.uniform();
                            for (int s = 0; s < 4; s++) {
                                const float sx = s == 0 ? w - dep : s == 1 ? -(w - dep) : 0.f;
                                const float sz = s == 2 ? w - dep : s == 3 ? -(w - dep) : 0.f;
                                const float hx = x + ca * sx - sa * sz, hz = z + sa * sx + ca * sz;
                                const bool alongX = s >= 2;
                                b.add(hx, hz, yaw, alongX ? w : dep, alongX ? dep : w - 2.f * dep, h * rng.range(0.9f, 1.1f), std::min(dep, 6.f) * 0.8f, 0.f,
                                      K_CLASSIC, sd, 0.f, birth, death);
                            }
                            if (rng.chance(0.04f)) b.add(x, z, yaw, w * 0.45f, w * 0.45f, h * 1.6f, 0.f, 0.7f, K_CLASSIC, 0.97f, 0.f, birth, death);
                            else if (rng.chance(0.5f)) {
                                // chimney stacks along the mansards
                                for (int c = 0; c < 3; c++) {
                                    float cx, cz;
                                    local(x, z, yaw, rng.range(-0.8f, 0.8f) * w, (rng.chance(0.5f) ? 1.f : -1.f) * (w - dep), cx, cz);
                                    b.add(cx, cz, yaw, 0.9f, 0.5f, std::min(dep, 6.f) * 0.8f + 1.4f, 0.f, 0.f, K_GEAR, 0.15f, h, birth, death);
                                }
                            }
                        } else if (d < 40000.f) {
                            // modern mid-rise, two or three a lot, taller with the era
                            const int m = rng.chance(0.5f) ? 3 : 2;
                            for (int a = 0; a < m; a++) {
                                const float ox = rng.range(-0.25f, 0.25f) * Ll, oz = rng.range(-0.25f, 0.25f) * Ll;
                                const float hx = x + ca * ox - sa * oz, hz = z + sa * ox + ca * oz;
                                const float h = towerH(d) * rng.range(0.15f, 0.45f);
                                const float bw = Ll * rng.range(0.1f, 0.18f), bd = Ll * rng.range(0.08f, 0.16f);
                                const bool tp = rng.chance(0.2f);
                                b.add(hx, hz, yaw, bw, bd, h, 0.f, tp ? 0.15f : 0.f, h > 260.f ? K_SUPER : K_GLASS, rng.uniform(), 0.f, birth, death);
                                if (!tp && Ll < 3000.f) addRoofGear(b, rng, hx, hz, yaw, bw, bd, h, birth, death, std::max(1.f, bw / 18.f));
                            }
                        } else if (d < 150000.f) {
                            // megablocks: a podium and its towers
                            const float sd = rng.uniform();
                            const float ph = towerH(d) * rng.range(0.06f, 0.1f);
                            b.add(x, z, yaw, Ll * 0.36f, Ll * 0.36f, ph, 0.f, 0.1f, K_GLASS, sd, 0.f, birth, death);
                            const int m = rng.irange(1, 3);
                            for (int a = 0; a < m; a++) {
                                const float ox = rng.range(-0.22f, 0.22f) * Ll, oz = rng.range(-0.22f, 0.22f) * Ll;
                                const float hx = x + ca * ox - sa * oz, hz = z + sa * ox + ca * oz;
                                const float h = towerH(d) * rng.range(0.25f, 0.6f);
                                addTower(b, rng, hx, hz, std::max(0.06f * h, 20.f), h, birth, rng.irange(0, 5), death);
                            }
                        } else {
                            // arcologies: pyramids and ziggurats kilometers high
                            const float hw = Ll * rng.range(0.34f, 0.42f);
                            const float h = std::min(Ll * rng.range(0.6f, 1.0f), 40000.f);
                            const float sd = rng.uniform();
                            if (rng.chance(0.55f)) {
                                b.add(x, z, yaw, hw, hw, h, 0.f, rng.range(0.7f, 0.9f), K_ARCO, sd, 0.f, birth, death);
                            } else {
                                b.add(x, z, yaw, hw, hw, h * 0.4f, 0.f, 0.25f, K_ARCO, sd, 0.f, birth, death);
                                b.add(x, z, yaw, hw * 0.66f, hw * 0.66f, h * 0.35f, 0.f, 0.3f, K_ARCO, sd, h * 0.4f, birth, death);
                                b.add(x, z, yaw, hw * 0.4f, hw * 0.4f, h * 0.35f, 0.f, 0.85f, K_ARCO, sd, h * 0.75f, birth, death);
                            }
                        }
                    }
            }
    }

    // ---- The downtowns: the main one by the old town, then in the satellite towns; then the megatalls.
    {
        std::vector<vec3> placed3;
        addDowntown(b, rng, dcx, dcz, SKY_F0, 14.f, 900, 350.f, 2600.f, placed3);
        // midtowns across the view (left of the main one, farther), so that the skyline spans the whole screen
        {
            std::vector<vec3> pl;
            addDowntown(b, rng, 1700.f, -5100.f, SKY_F0 * 1.35f, 12.f, 600, 300.f, 2000.f, pl);
        }
        {
            std::vector<vec3> pl;
            addDowntown(b, rng, 5100.f, -8900.f, SKY_F0 * 1.9f, 10.f, 450, 300.f, 2200.f, pl);
        }
        {
            std::vector<vec3> pl;
            addDowntown(b, rng, 6800.f, 1800.f, SKY_F0 * 2.4f, 9.f, 350, 300.f, 1600.f, pl);
        }
        for (int j = 1; j < 6; j++) {
            std::vector<vec3> pl;
            const float F0 = std::max(S[j].a * 2.2f, SKY_F0 * (1.6f + 0.5f * j));
            addDowntown(b, rng, S[j].x, S[j].z, F0, 9.f, 260, 250.f, 300.f + 0.18f * S[j].a, pl);
        }
        // megatalls: the tallest of all, km high, in the main downtown and the first satellites
        for (int m = 0; m < 16; m++) {
            const float birth = 40000.f * std::exp((m + rng.uniform()) / 16.f * std::log(4.f));
            const int site = m % 3;
            const float cx = site == 0 ? dcx : S[site].x, cz = site == 0 ? dcz : S[site].z;
            for (int tries = 0; tries < 20; tries++) {
                const float rho = rng.range(200.f, 2500.f), a = rng.range(0.f, TAU);
                const float x = cx + rho * std::cos(a), z = cz + rho * std::sin(a);
                if (inRiver(x, z, 80.f) || atSea(x, z)) continue;
                const float h = towerH(birth) * rng.range(1.1f, 1.4f);
                addTower(b, rng, x, z, 0.055f * h, h, birth, rng.irange(0, 3));
                break;
            }
        }
    }

    // ---- Bridges over the river: stone arches near the old town, longer spans downstream; lamps along them.
    {
        const float zs[] = {-180.f, 160.f, 520.f, -650.f, 1300.f, -1700.f, 2600.f, -3300.f, 4800.f, -6200.f, 8500.f, -11000.f, 15000.f, -19000.f, 26000.f};
        for (float bz : zs) {
            const float bx = riverX(bz);
            const float hwR = riverHalfW(bz);
            if (hwR < 3.f || atSea(bx, bz)) continue;
            // across the river: the river runs along z (its slope dx/dz from the centerline)
            const float slope = (riverX(bz + 5.f) - riverX(bz - 5.f)) / 10.f;
            const float yaw = std::atan(-slope) ;
            const float L = hwR + 14.f, wdt = std::min(7.f + 0.002f * std::fabs(bz), 16.f);
            const float hgt = std::min(6.f + 0.0015f * std::fabs(bz), 28.f);
            const float birth = std::max(field(bx, bz, S) * 1.02f, 90.f);
            b.add(bx, bz, yaw, L, wdt, hgt, 0.f, 0.f, K_BRIDGE, rng.uniform(), 0.f, birth);
            for (int i = -6; i <= 6; i++) {
                for (int sd = -1; sd <= 1; sd += 2) {
                    float lx, lz;
                    local(bx, bz, yaw, L * i / 6.5f, sd * wdt * 0.95f, lx, lz);
                    const vec3 p = Builder::world(lx, lz, hgt + 1.6f);
                    const float q[4] = {p.x, p.y, p.z, birth};
                    plan.lamps.insert(plan.lamps.end(), q, q + 4);
                }
            }
        }
    }

    // ---- Sort by birth.
    const size_t n = b.births.size();
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), size_t(0));
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t c) { return b.births[a] < b.births[c]; });
    plan.inst.resize(n * FL);
    plan.births.resize(n);
    for (size_t k = 0; k < n; k++) {
        std::copy(b.rows.begin() + order[k] * FL, b.rows.begin() + order[k] * FL + FL, plan.inst.begin() + k * FL);
        plan.births[k] = b.births[order[k]];
    }
    {
        const size_t m = b.beacons.size() / 4;
        std::vector<size_t> bo(m);
        std::iota(bo.begin(), bo.end(), size_t(0));
        std::stable_sort(bo.begin(), bo.end(), [&](size_t a, size_t c) { return b.beacons[a * 4 + 3] < b.beacons[c * 4 + 3]; });
        plan.beacons.resize(m * 4);
        for (size_t k = 0; k < m; k++)
            std::copy(b.beacons.begin() + bo[k] * 4, b.beacons.begin() + bo[k] * 4 + 4, plan.beacons.begin() + k * 4);
    }

    // ---- The space elevator's foot: a great tapering tower (its tether is drawn as light, see megacity.cpp).
    {
        const float er = 180000.f, ephi = 2.4f;
        const float ex = er * std::cos(ephi), ez = er * std::sin(ephi);
        const float th = er / R_EARTH;
        const float s2 = std::sin(th * 0.5f);
        plan.elevator = vec3(R_EARTH * std::sin(th) * std::cos(ephi), -2.f * R_EARTH * s2 * s2, R_EARTH * std::sin(th) * std::sin(ephi));
        plan.elevatorUp = normalize(plan.elevator + vec3(0.f, R_EARTH, 0.f));
        const float eb = 6.e5f;
        plan.fixedFirst = (int)plan.births.size();
        Builder eb2{plan, {}, {}, {}};
        eb2.add(ex, ez, 0.f, 9000.f, 9000.f, 16000.f, 0.f, 0.8f, K_ELEVATOR, 0.5f, 0.f, eb);
        eb2.add(ex, ez, 0.f, 1800.f, 1800.f, 30000.f, 0.f, 0.7f, K_ELEVATOR, 0.6f, 16000.f, eb);
        plan.inst.insert(plan.inst.end(), eb2.rows.begin(), eb2.rows.end());
        plan.births.insert(plan.births.end(), eb2.births.size(), 1e30f);
    }

    // ---- Trees around the hut, cut down as the town reaches them.
    for (int t = 0; t < 260; t++) {
        const float r = 5.f + 260.f * std::pow(rng.uniform(), 0.9f);
        const float a = rng.range(0.f, TAU);
        const float x = r * std::cos(a), z = r * std::sin(a);
        if (inRiver(x, z, 2.f)) continue;
        if ((x < -2.f && r < 48.f) || inView(x, z, 160.f)) continue;
        const float sz = rng.range(2.2f, 4.2f) * (r < 20.f ? 0.9f : 1.f);
        const float q[4] = {x, z, sz, rng.uniform()};
        plan.trees.insert(plan.trees.end(), q, q + 4);
    }
    return plan;
}
