// MEGACITY: the city's plan, generated once per game from a seed. Every building carries the city radius at which it
// rises (its distance from the hut, a little jittered): the shaders raise it out of the ground as the city's edge
// passes it, so nothing is uploaded while the city grows.
//
// The hut stands on the riverbank at the origin. The village around it is laid out by hand (houses scattered along
// the river and the road, a church); past LOT_R0 the lots follow a log-polar grid around the hut (SECTORS lots a
// ring, square, their size growing with the distance), and each ring is built in the style of the era the city
// reaches it in: medieval town houses with steep roofs inside a walled ring, stone blocks with mansards, glass
// towers, supertalls with setbacks and spires, then arcologies, kilometers high and wide. Radial avenues and ring
// boulevards are left free (the ground's shader lights them), and so are a few parks and the river.
#include <algorithm>
#include <cmath>
#include <numeric>

#include "universes/megacity/megacity.hpp"

namespace {

using namespace mcity;

enum Kind { K_HUT = 0, K_HOUSE, K_TOWN, K_CHURCH, K_WALL, K_CLASSIC, K_GLASS, K_SUPER, K_ARCO, K_ELEVATOR };

struct Builder {
    MegaCityPlan& plan;
    std::vector<float> births;
    std::vector<float> rows;   // 16 floats each, before sorting
    std::vector<float> beacons;

    // A building on the sphere at the geodesic polar coordinates (r, phi) around the hut, turned by yaw from facing
    // the hut, raised by y0 (a setback's tier).
    void add(float r, float phi, float yaw, float hw, float hd, float h, float roofH, float taper, int kind, float seed,
             float key, float y0, float birth) {
        const float th = r / R_EARTH;
        const float st = std::sin(th), ct = std::cos(th), sp = std::sin(phi), cp = std::cos(phi);
        const vec3 n(st * cp, ct, st * sp);
        const float s2 = std::sin(th * 0.5f);
        const vec3 base(R_EARTH * st * cp, -2.f * R_EARTH * s2 * s2, R_EARTH * st * sp);
        const vec3 ePhi(-sp, 0.f, cp), eTh(ct * cp, -st, ct * sp);
        const vec3 ax = ePhi * std::cos(yaw) + eTh * std::sin(yaw);
        (void)n;
        const float q[16] = {base.x, base.y, base.z, birth, ax.x, ax.y, ax.z, hw, hd, h, roofH, taper,
                             (float)kind, seed, key, y0};
        rows.insert(rows.end(), q, q + 16);
        births.push_back(birth);
        if (h + y0 > 90.f && kind != K_WALL) {
            const vec3 top = base + n * (y0 + h + roofH);
            const float b[4] = {top.x, top.y, top.z, birth};
            beacons.insert(beacons.end(), b, b + 4);
        }
    }
    void addXZ(float x, float z, float yaw, float hw, float hd, float h, float roofH, float taper, int kind, float seed,
               float key, float y0, float birth) {
        const float r = std::sqrt(x * x + z * z);
        add(r, std::atan2(z, x), yaw, hw, hd, h, roofH, taper, kind, seed, key, y0, birth);
    }
};

bool inRiver(float x, float z, float margin) {
    if (std::fabs(z) > 60000.f) return false;
    return std::fabs(x - riverX(z)) < riverHalfW(z) + margin;
}

} // namespace

MegaCityPlan buildMegaCity(uint64_t seed) {
    MegaCityPlan plan;
    Builder b{plan, {}, {}, {}};
    Rng rng(seed ^ 0xC17Eull);

    // ---- The hut: on the bank, facing the river (+x).
    b.addXZ(0.f, 0.f, 0.75f, 2.0f, 1.55f, 2.2f, 1.7f, 0.f, K_HUT, 0.5f, 0.f, 0.f, 0.f);

    // ---- The village: houses along the river and the road west, born with their distance from the hut.
    {
        std::vector<vec2> placed;
        placed.push_back(vec2{0.f, 0.f});
        int tries = 0;
        while ((int)placed.size() < 230 && tries < 9000) {
            tries++;
            const float r = 7.f + 195.f * std::pow(rng.uniform(), 1.15f);
            const float a = rng.range(0.f, TAU);
            const float x = r * std::cos(a), z = r * std::sin(a);
            if (inRiver(x, z, 5.f)) continue;
            // along the road by the river (x ~ riverX - 20) and the road west (z ~ 0): likelier
            const float dRoad = std::min(std::fabs(x - (riverX(z) - riverHalfW(z) - 14.f)), std::fabs(z - 2.f) + (x > 0.f ? 50.f : 0.f));
            if (rng.uniform() > 0.25f + 0.75f * std::exp(-dRoad / 14.f)) continue;
            if (std::fabs(x + 40.f) < 14.f && std::fabs(z - 26.f) < 14.f) continue;   // the church
            bool ok = true;
            for (const vec2& p : placed)
                if ((p.x - x) * (p.x - x) + (p.y - z) * (p.y - z) < 9.5f * 9.5f) { ok = false; break; }
            if (!ok) continue;
            placed.push_back(vec2{x, z});
            const float hw = rng.range(2.3f, 3.6f), hd = rng.range(2.6f, 4.4f);
            const float h = rng.range(2.6f, 4.8f) * (r > 60.f ? 1.25f : 1.f);
            // facing the road
            const float yaw = rng.range(-0.25f, 0.25f) + (rng.chance(0.5f) ? 1.5708f : 0.f);
            b.addXZ(x, z, yaw, hw, hd, h, hw * rng.range(0.75f, 1.0f), 0.f, K_HOUSE, rng.uniform(), mcity::hashLot(x * 0.05f, z * 0.05f),
                    0.f, std::max(3.5f, r * rng.range(0.9f, 1.05f)));
        }
        // as the town grows around it, the village fills in: houses in its gardens, packed along its lanes
        tries = 0;
        int infill = 0;
        while (infill < 330 && tries < 12000) {
            tries++;
            const float r = 10.f + 192.f * std::sqrt(rng.uniform());
            const float a = rng.range(0.f, TAU);
            const float x = r * std::cos(a), z = r * std::sin(a);
            if (inRiver(x, z, 4.f)) continue;
            if (std::fabs(x + 40.f) < 14.f && std::fabs(z - 26.f) < 14.f) continue;
            bool ok = true;
            for (const vec2& p : placed)
                if ((p.x - x) * (p.x - x) + (p.y - z) * (p.y - z) < 7.2f * 7.2f) { ok = false; break; }
            if (!ok) continue;
            placed.push_back(vec2{x, z});
            infill++;
            const float hw = rng.range(2.2f, 3.2f), hd = rng.range(2.4f, 3.8f);
            const float h = rng.range(3.5f, 7.f);
            const float yaw = rng.range(-0.3f, 0.3f) + (rng.chance(0.5f) ? 1.5708f : 0.f);
            b.addXZ(x, z, yaw, hw, hd, h, hw * rng.range(0.8f, 1.05f), 0.f, K_TOWN, rng.uniform(), mcity::hashLot(x * 0.05f, z * 0.05f),
                    0.f, rng.range(240.f, 480.f));
        }
        // the church: a nave and a bell tower with its spire
        const float cb = 85.f;
        b.addXZ(-40.f, 26.f, 0.3f, 4.5f, 10.f, 9.f, 4.5f, 0.f, K_CHURCH, 0.3f, 0.2f, 0.f, cb);
        b.addXZ(-36.5f, 36.5f, 0.3f, 2.6f, 2.6f, 19.f, 0.f, 0.f, K_CHURCH, 0.6f, 0.2f, 0.f, cb);
        b.addXZ(-36.5f, 36.5f, 0.3f, 2.7f, 2.7f, 14.f, 0.f, 1.f, K_CHURCH, 0.7f, 0.2f, 19.f, cb);
    }

    // ---- The medieval wall around the old town, its towers and gates.
    const float WALL_R = 640.f;
    {
        const float wb = WALL_R * 1.02f;
        for (int j = 0; j < SECTORS; j++) {
            const float phi = (j + 0.5f) * LOT_D;
            const float x = WALL_R * std::cos(phi), z = WALL_R * std::sin(phi);
            if (inRiver(x, z, 4.f)) continue;
            if (j % 10 == 0) {
                // a gate: two towers flanking the avenue
                for (int s = -1; s <= 1; s += 2) {
                    const float ph = j * LOT_D + s * 0.012f;
                    b.add(WALL_R, ph, 0.f, 4.5f, 4.5f, 17.f, 0.f, 0.f, K_WALL, 0.2f, 0.f, 0.f, wb);
                    b.add(WALL_R, ph, 0.f, 4.8f, 4.8f, 7.f, 0.f, 1.f, K_WALL, 0.25f, 0.f, 17.f, wb);
                }
                continue;
            }
            b.add(WALL_R, phi, 0.f, WALL_R * LOT_D * 0.5f + 0.4f, 2.2f, 10.f, 0.f, 0.f, K_WALL, 0.f, 0.f, 0.f, wb);
            if (j % 5 == 0) {
                b.add(WALL_R, phi, 0.f, 4.f, 4.f, 15.f, 0.f, 0.f, K_WALL, 0.4f, 0.f, 0.f, wb);
                b.add(WALL_R, phi, 0.f, 4.3f, 4.3f, 6.f, 0.f, 1.f, K_WALL, 0.45f, 0.f, 15.f, wb);
            }
        }
    }

    // ---- The lots: a log-polar grid, each ring built in its era's style.
    const float R_MAX = 1.3e6f;
    for (int i = 0;; i++) {
        const float rIn = LOT_R0 * std::exp(i * LOT_D), rOut = rIn * std::exp(LOT_D);
        if (rIn > R_MAX) break;
        const float rc = std::sqrt(rIn * rOut);
        const float s = rc * LOT_D;   // the lot's size (meters)
        const bool boulevard = i % 12 == 11;
        if (boulevard) continue;
        if (std::fabs(rc - WALL_R) < s * 0.5f + 8.f) continue;
        for (int j = 0; j < SECTORS; j++) {
            if (j % 10 == 0) continue;   // a radial avenue
            const float phi = (j + 0.5f) * LOT_D;
            const float hl = hashLot((float)i, (float)j);
            if (hl < 0.05f) continue;    // a park, a square
            const float x = rc * std::cos(phi), z = rc * std::sin(phi);
            if (inRiver(x, z, s * 0.55f)) continue;
            if (rc > 100000.f && std::hypot(x - 180000.f * std::cos(0.3f), z - 180000.f * std::sin(0.3f)) < 15000.f) continue;   // the elevator's square
            const float key = hashLot((float)(i / 2) + 0.5f, (float)(j / 2) + 7.f);
            auto birthOf = [&](float r) { return r * rng.range(0.97f, 1.03f); };
            if (rc < 1500.f) {
                // the medieval town: a cluster of steep-roofed town houses, now and then a tower
                const int k = s < 24.f ? 1 : 2;
                const float sub = s / k;
                for (int a = 0; a < k; a++)
                    for (int c = 0; c < k; c++) {
                        const float rr = rc * std::exp(((a + 0.5f) / k - 0.5f) * LOT_D);
                        const float ph = phi + ((c + 0.5f) / k - 0.5f) * LOT_D;
                        const float hw = sub * rng.range(0.3f, 0.4f), hd = sub * rng.range(0.32f, 0.42f);
                        const float h = rng.range(6.f, 12.f) * std::min(1.6f, std::max(1.f, sub / 14.f));
                        b.add(rr, ph, rng.range(-0.04f, 0.04f), hw, hd, h, std::min(hw, 7.f) * rng.range(0.8f, 1.1f), 0.f, K_TOWN, rng.uniform(), key, 0.f, birthOf(rr));
                    }
                if (rng.chance(0.03f)) {
                    const float tw = std::min(s * 0.12f, 5.f);
                    const float th = rng.range(24.f, 40.f);
                    b.add(rc, phi, 0.f, tw, tw, th, 0.f, 0.f, K_CHURCH, rng.uniform(), key, 0.f, birthOf(rc));
                    b.add(rc, phi, 0.f, tw * 1.05f, tw * 1.05f, th * 0.45f, 0.f, 1.f, K_CHURCH, rng.uniform(), key, th, birthOf(rc));
                }
            } else if (rc < 5000.f) {
                // the stone city: blocks with mansard roofs, a courtyard's worth of variety in their heights
                const float hw = s * rng.range(0.36f, 0.42f), hd = s * rng.range(0.36f, 0.42f);
                const float h = rng.range(14.f, 30.f) * (1.f + 0.4f * smoothstepf(2500.f, 5000.f, rc));
                b.add(rc, phi, 0.f, hw, hd, h, std::min(hw * 0.12f, 5.f), 0.f, K_CLASSIC, rng.uniform(), key, 0.f, birthOf(rc));
                if (rng.chance(0.04f)) {
                    // a dome
                    b.add(rc, phi, 0.f, hw * 0.4f, hw * 0.4f, h * 0.9f, 0.f, 0.75f, K_CLASSIC, 0.95f, key, h, birthOf(rc));
                }
            } else if (rc < 16000.f) {
                // glass towers, one to four a lot
                const int k = rng.chance(0.55f) ? 2 : 1;
                const float sub = s / k;
                const float med = lerpf(70.f, 380.f, smoothstepf(5000.f, 16000.f, rc));
                for (int a = 0; a < k; a++)
                    for (int c = 0; c < k; c++) {
                        if (k == 2 && rng.chance(0.2f)) continue;
                        const float rr = rc * std::exp(((a + 0.5f) / k - 0.5f) * LOT_D);
                        const float ph = phi + ((c + 0.5f) / k - 0.5f) * LOT_D;
                        const float hw = sub * rng.range(0.24f, 0.36f), hd = sub * rng.range(0.24f, 0.36f);
                        const float h = std::min(med * std::exp(rng.range(-0.6f, 0.9f)), 1100.f);
                        const float bb = birthOf(rr);
                        b.add(rr, ph, 0.f, hw, hd, h, 0.f, rng.chance(0.15f) ? 0.25f : 0.f, K_GLASS, rng.uniform(), key, 0.f, bb);
                        if (h > 160.f && rng.chance(0.5f)) b.add(rr, ph, 0.f, hw * 0.6f, hd * 0.6f, h * 0.25f, 0.f, 0.2f, K_GLASS, rng.uniform(), key, h, bb);
                    }
            } else if (rc < 60000.f) {
                // supertalls: setbacks and spires
                const int n = rng.irange(1, 3);
                const float med = lerpf(700.f, 2600.f, smoothstepf(16000.f, 60000.f, rc));
                for (int t = 0; t < n; t++) {
                    const float rr = rc * std::exp(rng.range(-0.3f, 0.3f) * LOT_D);
                    const float ph = phi + rng.range(-0.3f, 0.3f) * LOT_D;
                    const float w = s * rng.range(0.1f, 0.16f) / std::sqrt((float)n);
                    const float h = std::min(med * std::exp(rng.range(-0.5f, 0.6f)), 5000.f);
                    const float bb = birthOf(rr);
                    const float sd = rng.uniform();
                    b.add(rr, ph, 0.f, w, w * rng.range(0.8f, 1.2f), h * 0.55f, 0.f, 0.05f, K_SUPER, sd, key, 0.f, bb);
                    b.add(rr, ph, 0.f, w * 0.72f, w * 0.72f, h * 0.3f, 0.f, 0.1f, K_SUPER, sd, key, h * 0.55f, bb);
                    b.add(rr, ph, 0.f, w * 0.45f, w * 0.45f, h * 0.15f, 0.f, 0.6f, K_SUPER, sd, key, h * 0.85f, bb);
                }
                // and a podium of lower towers around them
                if (rng.chance(0.7f)) {
                    const float h = rng.range(120.f, 400.f);
                    b.add(rc, phi + 0.3f * LOT_D, 0.f, s * 0.2f, s * 0.14f, h, 0.f, 0.f, K_GLASS, rng.uniform(), key, 0.f, birthOf(rc));
                }
            } else {
                // arcologies: pyramids and ziggurats kilometers high, glowing bands
                const float bb = birthOf(rc);
                const float hw = s * rng.range(0.34f, 0.42f);
                const float h = std::min(s * rng.range(0.6f, 1.1f), 40000.f);
                const float sd = rng.uniform();
                if (rng.chance(0.55f)) {
                    b.add(rc, phi, 0.f, hw, hw, h, 0.f, rng.range(0.7f, 0.9f), K_ARCO, sd, key, 0.f, bb);
                } else {
                    b.add(rc, phi, 0.f, hw, hw, h * 0.4f, 0.f, 0.25f, K_ARCO, sd, key, 0.f, bb);
                    b.add(rc, phi, 0.f, hw * 0.66f, hw * 0.66f, h * 0.35f, 0.f, 0.3f, K_ARCO, sd, key, h * 0.4f, bb);
                    b.add(rc, phi, 0.f, hw * 0.4f, hw * 0.4f, h * 0.35f, 0.f, 0.85f, K_ARCO, sd, key, h * 0.75f, bb);
                }
            }
        }
    }

    // ---- Sort by birth.
    const size_t n = b.births.size();
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), size_t(0));
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t c) { return b.births[a] < b.births[c]; });
    plan.inst.resize(n * 16);
    plan.births.resize(n);
    for (size_t k = 0; k < n; k++) {
        std::copy(b.rows.begin() + order[k] * 16, b.rows.begin() + order[k] * 16 + 16, plan.inst.begin() + k * 16);
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
        const float er = 180000.f, ephi = 0.3f;
        const float th = er / R_EARTH;
        const float s2 = std::sin(th * 0.5f);
        plan.elevator = vec3(R_EARTH * std::sin(th) * std::cos(ephi), -2.f * R_EARTH * s2 * s2, R_EARTH * std::sin(th) * std::sin(ephi));
        plan.elevatorUp = normalize(plan.elevator + vec3(0.f, R_EARTH, 0.f));
        const float eb = 5.5e5f;
        plan.fixedFirst = (int)plan.births.size();
        Builder eb2{plan, {}, {}, {}};
        eb2.add(er, ephi, 0.f, 9000.f, 9000.f, 16000.f, 0.f, 0.8f, K_ELEVATOR, 0.5f, 0.f, 0.f, eb);
        eb2.add(er, ephi, 0.f, 1800.f, 1800.f, 30000.f, 0.f, 0.7f, K_ELEVATOR, 0.6f, 0.f, 16000.f, eb);
        plan.inst.insert(plan.inst.end(), eb2.rows.begin(), eb2.rows.end());
        plan.births.insert(plan.births.end(), eb2.births.size(), 1e30f);
    }

    // ---- Trees around the hut, cut down as the village reaches them.
    for (int t = 0; t < 260; t++) {
        const float r = 5.f + 260.f * std::pow(rng.uniform(), 0.9f);
        const float a = rng.range(0.f, TAU);
        const float x = r * std::cos(a), z = r * std::sin(a);
        if (inRiver(x, z, 2.f)) continue;
        if (x < -2.f && r < 48.f) continue;   // keep the camera's side of the hut open
        const float sz = rng.range(2.2f, 4.2f) * (r < 20.f ? 0.9f : 1.f);
        const float q[4] = {x, z, sz, rng.uniform()};
        plan.trees.insert(plan.trees.end(), q, q + 4);
    }
    return plan;
}
