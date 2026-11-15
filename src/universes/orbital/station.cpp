// ORBITAL STATION: the station's plan (one module per cleared line) and the geometry of every module, tug, drone
// and of the robot arm (see orbital.cpp for the staging).
//
// The station's frame: meters, y away from the planet, the camera on the +z side. The capsule sits at the origin, its
// nose toward -z; the core (nodes, labs) runs toward -z, the truss crosses it along x above, the habitat wheel turns
// about the z axis behind the core, and the great forest ring lies under it all, horizontal, 9 km across, hung from a
// spindle under the core.
#include <algorithm>
#include <cmath>

#include "universes/orbital/builder.hpp"

using namespace orbital;

namespace orbital {
vec3 rotateAxis(const vec3& v, const vec3& k, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.f - c));
}
} // namespace orbital

namespace {
// a frame from its z axis and a y hint (right-handed)
Xf frameZ(const vec3& o, const vec3& z, const vec3& yh = vec3(0, 1, 0)) {
    Xf f;
    f.o = o;
    f.z = normalize(z);
    f.x = normalize(cross(yh, f.z));
    f.y = cross(f.z, f.x);
    return f;
}
// a frame from its x axis and a y hint
Xf frameX(const vec3& o, const vec3& x, const vec3& yh = vec3(0, 1, 0)) {
    Xf f;
    f.o = o;
    f.x = normalize(x);
    f.z = normalize(cross(f.x, yh));
    f.y = cross(f.z, f.x);
    return f;
}
constexpr float DEG = PI / 180.f;
inline float sq(float x) { return x * x; }
float hash1(float n) { return std::fmod(std::fabs(std::sin(n * 127.1f + 0.37f) * 43758.5453f), 1.f); }
} // namespace

// =====================================================================================================
// The plan
// =====================================================================================================

void OrbitalUniverse::buildPlan() {
    plan_.clear();
    auto add = [&](int kind, const Xf& f, const vec3& dim, const vec3& approach, float extent, bool armOk = false) -> Module& {
        Module m;
        m.kind = kind;
        m.f = f;
        m.dim = dim;
        m.approach = normalize(approach);
        m.extent = extent;
        m.armOk = armOk;
        m.seed = hash1((float)plan_.size() * 1.37f + 0.5f);
        m.state = 0;
        m.present = 0.f;
        plan_.push_back(m);
        return plan_.back();
    };
    const vec3 Zm(0, 0, -1), Xp(1, 0, 0), Xm(-1, 0, 0), Yp(0, 1, 0), Ym(0, -1, 0);
    const float TY = 6.4f, TZ = -22.2f;
    // 0 the capsule, alone at the start
    add(K_CAPSULE, frameZ(vec3(0, 0, 0), Zm), vec3(1.f), Zm, 7.f);
    // the core: nodes and labs along -z
    add(K_NODE, frameZ(vec3(0, 0, -5.5f), Zm), vec3(2.2f, 1.f, 2.3f), Zm, 3.5f);                      // 1
    add(K_LAB, frameZ(vec3(0, 0, -13.8f), Zm), vec3(2.2f, 1.f, 6.0f), Zm, 6.5f);                      // 2 (the arm on it)
    add(K_NODE, frameZ(vec3(0, 0, TZ), Zm), vec3(2.3f, 2.f, 2.4f), Zm, 3.6f, true);                    // 3
    add(K_LAB, frameZ(vec3(8.4f, 0, TZ), Xp), vec3(2.1f, 0.f, 6.2f), Xp, 6.6f, true);                 // 4
    add(K_LAB, frameZ(vec3(-8.4f, 0, TZ), Xm), vec3(2.1f, 2.f, 6.2f), Xm, 6.6f, true);                // 5
    // the truss across, its first solar arrays
    add(K_TRUSS, frameX(vec3(0, TY, TZ), Xp), vec3(7.f, 1.5f, 1.f), Yp, 7.5f, true);                  // 6
    add(K_TRUSS, frameX(vec3(14.f, TY, TZ), Xp), vec3(7.f, 1.5f, 0.f), Xp, 7.5f, true);               // 7
    add(K_TRUSS, frameX(vec3(-14.f, TY, TZ), Xm), vec3(7.f, 1.5f, 0.f), Xm, 7.5f, true);              // 8
    { Module& m = add(K_ARRAY, frameX(vec3(21.f, TY, TZ), Xp), vec3(3.5f, 16.f, 0.f), Xp, 19.f); m.pivot = vec3(5.7f, 0, 0); } // 9
    { Module& m = add(K_ARRAY, frameX(vec3(-21.f, TY, TZ), Xm), vec3(3.5f, 16.f, 0.f), Xm, 19.f); m.pivot = vec3(5.7f, 0, 0); } // 10
    add(K_CUPOLA, frameZ(vec3(0, -2.2f, -5.5f), Ym, vec3(0, 0, -1)), vec3(1.5f), Ym, 2.f, true);      // 11
    // the habitat wheel: its hub, three pairs of spokes, six arcs
    add(K_HUB, frameZ(vec3(0, 0, -39.1f), Zm), vec3(3.0f, 0.f, 14.5f), Zm, 15.f);                     // 12
    wheelFirst_ = (int)plan_.size();
    for (int k = 0; k < 3; k++) {
        Module& m = add(K_SPOKE, frameZ(WHEEL_C, WHEEL_N), vec3(1.f), Zm, WHEEL_R);
        m.a0 = k * 60.f * DEG;
        m.ring = 1;
    }
    for (int k = 0; k < 6; k++) {
        const float mid = k * 60.f * DEG;
        Module& m = add(K_ARC, frameZ(WHEEL_C, WHEEL_N), vec3(1.f), vec3(std::cos(mid), std::sin(mid), 0.f), WHEEL_R * 0.5f + WHEEL_TUBE);
        m.a0 = mid - 30.f * DEG;
        m.al = 60.f * DEG;
        m.ring = 1;
        m.pivot = vec3(std::cos(mid), std::sin(mid), 0.f) * WHEEL_R;
        m.tugs = 2;
    }
    wheelLast_ = (int)plan_.size() - 1;
    // a visiting ship, more truss and arrays, radiators, a dish
    { Module& m = add(K_SHIP, frameZ(vec3(18.9f, 0, TZ), Xm), vec3(1.f), Xp, 4.5f); m.tugs = 0; }
    add(K_TRUSS, frameX(vec3(38.f, TY, TZ), Xp), vec3(7.f, 1.5f, 0.f), Xp, 7.5f);
    add(K_TRUSS, frameX(vec3(-38.f, TY, TZ), Xm), vec3(7.f, 1.5f, 0.f), Xm, 7.5f);
    { Module& m = add(K_ARRAY, frameX(vec3(45.f, TY, TZ), Xp), vec3(3.5f, 16.f, 0.f), Xp, 19.f); m.pivot = vec3(5.7f, 0, 0); }
    { Module& m = add(K_ARRAY, frameX(vec3(-45.f, TY, TZ), Xm), vec3(3.5f, 16.f, 0.f), Xm, 19.f); m.pivot = vec3(5.7f, 0, 0); }
    { Module& m = add(K_RADIATOR, frameZ(vec3(14.f, TY, TZ - 1.5f), Zm), vec3(3.2f, 2.f, 0.f), Zm, 8.f, true); m.pivot = vec3(0, 0, 6.5f); }
    { Module& m = add(K_RADIATOR, frameZ(vec3(-14.f, TY, TZ - 1.5f), Zm), vec3(3.2f, 2.f, 0.f), Zm, 8.f, true); m.pivot = vec3(0, 0, 6.5f); }
    { Module& m = add(K_DISH, frameZ(vec3(-14.6f, 0, TZ), Xm), vec3(1.f), Xm, 4.f); m.pivot = vec3(0, 0, 2.f); }
    // the great forest ring: its spindle and hub, four pairs of spokes, sixteen arcs, mirrors
    {
        Module& m = add(K_AXLE, frameZ(GREAT_C, vec3(0, 0, 1)), vec3(1.f), Ym, 330.f);
        m.pivot = vec3(0, 60.f, 0);
        m.tugs = 3;
    }
    greatFirst_ = (int)plan_.size();
    for (int k = 0; k < 16; k++) {
        // from the far side of the ring (its forest facing the camera) round both ways, closing in front
        static const int ORDER[16] = {5, 6, 4, 7, 3, 8, 2, 9, 1, 10, 0, 11, 15, 12, 14, 13};
        const int j = ORDER[k];
        const float mid = j * 22.5f * DEG;
        const vec3 rad(std::cos(mid), 0.f, -std::sin(mid));
        Module& m = add(K_BIGARC, frameZ(GREAT_C, vec3(0, 0, 1)), vec3(1.f), rad, GREAT_R * std::sin(11.25f * DEG) * 0.75f + GREAT_HW * 0.7f);
        m.a0 = mid - 11.25f * DEG;
        m.al = 22.5f * DEG;
        m.ring = 2;
        m.pivot = vec3(std::cos(mid), 0.f, -std::sin(mid)) * GREAT_R;
        m.tugs = 4;
    }
    for (int k = 0; k < 4; k++) {
        Module& m = add(K_BIGSPOKE, frameZ(GREAT_C, vec3(0, 0, 1)), vec3(1.f), Yp, GREAT_R);
        m.a0 = k * 45.f * DEG + 22.5f * DEG;
        m.ring = 2;
        m.tugs = 4;
    }
    greatLast_ = (int)plan_.size() - 1;
    for (int k = 0; k < 12; k++) {
        const float mid = (k * 30.f + 15.f) * DEG;
        Module& m = add(K_MIRROR, frameZ(GREAT_C, vec3(0, 0, 1)), vec3(1.f), vec3(0.f, 1.f, 0.f), GREAT_R * 0.07f);
        m.a0 = mid;
        m.ring = 2;
        m.pivot = vec3(std::cos(mid) * (GREAT_R - GREAT_WALL * 0.5f), GREAT_HW + GREAT_R * 0.06f, -std::sin(mid) * (GREAT_R - GREAT_WALL * 0.5f));
        m.tugs = 2;
    }
    // ---- Then each stage an order of magnitude bigger.
    auto ringDef = [&](int r, const vec3& c, const vec3& n, float baseW, float peakW, float dropW, float heads, float turn) {
        Ring& R = rings_[r];
        R.c = c;
        R.n = normalize(n);
        R.ref = normalize(cross(R.n, std::fabs(R.n.z) < 0.9f ? vec3(0, 0, 1) : vec3(1, 0, 0)));
        R.baseW = baseW;
        R.peakW = peakW;
        R.dropW = dropW;
        R.heads = heads;
        R.turn = turn;
    };
    ringDef(1, WHEEL_C, WHEEL_N, 0.05f, 0.08f, 0.24f, 2.f, 8.f);
    rings_[1].ref = vec3(1, 0, 0);
    rings_[1].first = wheelFirst_;
    rings_[1].last = wheelLast_;
    ringDef(2, GREAT_C, GREAT_N, 0.004f, 0.003f, 0.02f, 4.f, 16.f);
    rings_[2].ref = vec3(1, 0, 0);
    rings_[2].first = greatFirst_;
    rings_[2].last = greatLast_;
    auto arcs = [&](int kind, int r, float R, float hw, int n, int tugs, float flag) {
        Ring& RG = rings_[r];
        RG.first = (int)plan_.size();
        const vec3 cr = cross(RG.n, RG.ref);
        for (int k = 0; k < n; k++) {
            const int j = (k % 2 == 0) ? k / 2 : n - 1 - k / 2;     // from one side round both ways
            const float al = TAU / n, mid = (j + 0.5f) * al;
            const vec3 e = RG.ref * std::cos(mid) + cr * std::sin(mid);
            Module& m = add(kind, frameZ(RG.c, vec3(0, 0, 1)), vec3(R, hw, flag), e, R * std::sin(al * 0.5f) * 0.8f + hw);
            m.a0 = mid - al * 0.5f;
            m.al = al;
            m.ring = r;
            m.pivot = e * R;
            m.tugs = tugs;
        }
        RG.last = (int)plan_.size() - 1;
    };
    // nested rings around the forest ring, tilted, counter-rotating: a band of land, a tube of windows, a wider band
    stageFirst_[0] = (int)plan_.size();
    ringDef(3, GREAT_C, vec3(0.35f, 1.f, 0.55f), 0.012f, 0.01f, 0.05f, 4.f, 16.f);
    ringDef(4, GREAT_C, vec3(1.f, 0.45f, -0.35f), -0.008f, -0.008f, -0.04f, 2.f, 8.f);
    ringDef(5, GREAT_C, vec3(-0.5f, 1.f, -0.25f), 0.005f, 0.005f, 0.025f, 4.f, 16.f);
    arcs(K_NRING, 3, 26000.f, 2400.f, 6, 5, 0.f);
    arcs(K_NRING, 4, 44000.f, 1800.f, 6, 5, 1.f);
    arcs(K_NRING, 5, 72000.f, 5200.f, 6, 6, 0.f);
    // shipyards around them, each building a huge ship
    stageFirst_[1] = (int)plan_.size();
    for (int k = 0; k < 4; k++) {
        const float a = (k * 90.f + 30.f) * DEG;
        const vec3 rr(std::cos(a), 0.12f * (k % 2 ? 1.f : -1.f), -std::sin(a));
        Module& m = add(K_YARD, frameX(GREAT_C + rr * 190000.f, vec3(-std::sin(a), 0.f, -std::cos(a))), vec3(1.f), normalize(rr), 95000.f);
        m.tugs = 6;
        m.a0 = k * 9.7f;
    }
    // a space elevator: a tether from the ground through the station's height to a counterweight
    stageFirst_[2] = (int)plan_.size();
    {
        const vec3 up = normalize(vec3(-1.6e5f, R_PLANET + ALT, 0.f));
        for (int k = 0; k < 6; k++) {
            Module& m = add(K_TETHER, frameZ(EARTH_C, vec3(0, 0, 1)), vec3(up.x, up.y, up.z), vec3(-0.6f, 0.2f, 0.5f), 4.e5f);
            // segments (radii from the planet's center): the port, down, to the ground, up, the counterweight, climbers
            static const float A[6] = {ALT - 4.e4f, 1.5e5f, 0.f, ALT + 4.e4f, 1.0e6f, 0.f};
            static const float B[6] = {ALT + 4.e4f, ALT - 4.e4f, 1.5e5f, 1.0e6f, 2.2e6f, 2.2e6f};
            m.a0 = A[k];
            m.al = B[k];
            m.pivot = up * (R_PLANET + 0.5f * (A[k] + B[k]));
            m.extent = std::max(1.e5f, 0.5f * (B[k] - A[k]));
            m.tugs = k == 5 ? 0 : 4;
        }
    }
    // a ring round the whole planet, over the station
    stageFirst_[3] = (int)plan_.size();
    ringDef(6, EARTH_C, vec3(1.f, 0.f, 0.35f), 0.0015f, 0.001f, 0.006f, 4.f, 16.f);
    rings_[6].ref = vec3(0, 1, 0);
    arcs(K_ERING, 6, R_PLANET + ALT + 4.5e5f, 9.e4f, 12, 6, 0.f);
    // the ring becomes a wheel round the planet: more elevators, spokes in its plane, then a rim through their
    // counterweights, and two rings crossed over it
    stageFirst_[4] = (int)plan_.size();
    {
        // an elevator along a ring's plane, from the ground up to that ring (its counterweight on the ring)
        auto elevator = [&](int r, float a, float top, int k) {
            const Ring& RG = rings_[r];
            const vec3 up = RG.ref * std::cos(a) + cross(RG.n, RG.ref) * std::sin(a);
            Module& m = add(K_ELEVATOR, frameZ(EARTH_C, vec3(0, 0, 1)), up, up, 0.25f * top);
            m.pivot = up * (R_PLANET + 0.5f * top);
            m.a0 = (float)k;
            m.al = top;
            m.tugs = 4;
        };
        const Ring& R6 = rings_[6];
        const vec3 up0 = normalize(vec3(-1.6e5f, R_PLANET + ALT, 0.f));
        const float a6 = std::atan2(dot(up0, cross(R6.n, R6.ref)), dot(up0, R6.ref));
        elevator(6, a6 + TAU * 0.25f, 2.2e6f, 1);
        elevator(6, a6 - TAU * 0.25f, 2.2e6f, 2);
        elevator(6, a6 + TAU * 0.5f, 2.2e6f, 3);
        ringDef(9, EARTH_C, R6.n, 0.0015f, 0.001f, 0.006f, 4.f, 16.f);
        rings_[9].ref = R6.ref;
        arcs(K_ERING, 9, R_PLANET + 2.2e6f, 1.3e5f, 10, 6, 0.f);
        ringDef(10, EARTH_C, vec3(0.3f, 1.f, -0.6f), -0.001f, -0.0008f, -0.005f, 2.f, 8.f);
        arcs(K_ERING, 10, R_PLANET * 1.75f, 1.6e5f, 10, 6, 1.f);
        for (int k = 0; k < 4; k++) elevator(10, 0.4f + k * TAU / 4.f, R_PLANET * 0.75f, 4 + k);
        ringDef(11, EARTH_C, vec3(-0.7f, 0.4f, 1.f), 0.0008f, 0.0006f, 0.004f, 4.f, 16.f);
        arcs(K_ERING, 11, R_PLANET * 2.1f, 2.2e5f, 10, 6, 0.f);
        for (int k = 0; k < 4; k++) elevator(11, 1.2f + k * TAU / 4.f, R_PLANET * 1.1f, 8 + k);
    }
    // a fleet of O'Neill cylinders
    stageFirst_[5] = (int)plan_.size();
    for (int k = 0; k < 6; k++) {
        const float a = (k * 52.f - 110.f) * DEG;
        const vec3 n = normalize(vec3(0.2f, 1.f, -0.1f));
        vec3 u, v;
        basisOf(n, u, v);
        const vec3 p = EARTH_C + (u * std::cos(a) + v * std::sin(a)) * (R_PLANET * 3.1f) + n * ((k % 3) - 1.f) * 2.e6f;
        Module& m = add(K_FLEET, frameX(p, normalize(cross(n, normalize(p - EARTH_C)))), vec3(1.f), normalize(p - EARTH_C), 1.6e6f);
        m.tugs = 0;
    }
    // the Moon taken in hand: rings round it, its works lit, shells, a mass driver
    stageFirst_[6] = (int)plan_.size();
    for (int k = 0; k < 6; k++) {
        Module& m = add(K_MOONWORK, frameZ(MOON_C, vec3(0, 0, 1)), vec3(1.f), normalize(vec3(0.4f, 0.6f, 0.7f)), R_MOON * 1.4f);
        m.a0 = (float)k;
        m.tugs = 6;
    }
    // the finale: two rings dwarfing the planet, round the planet and the Moon, crossed, turning against each other
    stageFirst_[7] = (int)plan_.size();
    ringDef(7, EARTH_C, vec3(0.12f, 1.f, 0.22f), 0.0006f, 0.0004f, 0.003f, 4.f, 16.f);
    ringDef(8, EARTH_C, vec3(1.f, 0.55f, 0.65f), -0.0008f, -0.0005f, -0.004f, 4.f, 16.f);
    arcs(K_COLOSSUS, 7, 1.45e8f, 1.5e7f, 8, 8, 0.f);
    arcs(K_COLOSSUS, 8, 1.15e8f, 2.6e6f, 8, 8, 1.f);
    stageFirst_[8] = (int)plan_.size();

    // The framing: each module's bounds (spheres on its seat), the center of everything in after module i.
    frameA_.assign(plan_.size(), vec3(0.f));
    vec3 lo(1e30f), hi(-1e30f);
    for (size_t i = 0; i < plan_.size(); i++) {
        Module& m = plan_[i];
        const vec3 c = m.f.at(m.pivot);
        m.nsph = 1;
        m.sph[0] = vec4(c, m.extent);
        if (m.kind == K_SPOKE || m.kind == K_BIGSPOKE) {
            const bool big = m.kind == K_BIGSPOKE;
            const float R = big ? GREAT_R : WHEEL_R;
            const vec3 rr = big ? vec3(std::cos(m.a0), 0.f, -std::sin(m.a0)) : vec3(std::cos(m.a0), std::sin(m.a0), 0.f);
            const vec3 o = big ? GREAT_C : WHEEL_C;
            m.nsph = 2;
            m.sph[0] = vec4(o + rr * (R * 0.5f), R * 0.5f);
            m.sph[1] = vec4(o - rr * (R * 0.5f), R * 0.5f);
        }
        if ((int)i >= stageFirst_[3] && m.kind != K_MOONWORK) m.sph[m.nsph++] = vec4(EARTH_C, R_PLANET * 1.05f);
        if (m.kind == K_MOONWORK) m.sph[m.nsph++] = vec4(MOON_C, R_MOON * 1.6f);
        if (m.kind == K_ERING || m.kind == K_COLOSSUS || m.kind == K_NRING) {
            const Ring& RG = rings_[m.ring];
            const float R = m.dim.x;
            m.sph[0] = vec4(RG.c + (RG.ref * std::cos(m.a0 + m.al * 0.5f) + cross(RG.n, RG.ref) * std::sin(m.a0 + m.al * 0.5f)) * R, m.extent);
        }
        for (int s = 0; s < m.nsph; s++) {
            const vec3 p(m.sph[s].x, m.sph[s].y, m.sph[s].z);
            const float e = m.sph[s].w;
            lo = vec3(std::min(lo.x, p.x - e), std::min(lo.y, p.y - e), std::min(lo.z, p.z - e));
            hi = vec3(std::max(hi.x, p.x + e), std::max(hi.y, p.y + e), std::max(hi.z, p.z + e));
        }
        frameA_[i] = (lo + hi) * 0.5f;
    }
}

float OrbitalUniverse::flightA(const Module& m) const {
    if (m.kind >= K_NRING) return 5.2f;
    if (m.kind == K_BIGARC || m.kind == K_BIGSPOKE || m.kind == K_MIRROR || m.kind == K_AXLE) return 4.6f;
    if (m.kind == K_SHIP || m.kind == K_LINER) return 4.4f;
    return 3.4f;
}
float OrbitalUniverse::flightB(const Module& m) const {
    if (m.kind >= K_NRING) return 2.6f;
    if (m.kind == K_BIGARC || m.kind == K_BIGSPOKE || m.kind == K_MIRROR || m.kind == K_AXLE) return 2.2f;
    return 1.6f;
}

// A ring's turn: about its axis through its center.
Xf OrbitalUniverse::ringXf(int ring) const {
    Xf r;
    if (ring <= 0 || ring >= RINGS) return r;
    const vec3 c = rings_[ring].c, n = rings_[ring].n;
    const float a = rings_[ring].spin;
    r.x = rotateAxis(vec3(1, 0, 0), n, a);
    r.y = rotateAxis(vec3(0, 1, 0), n, a);
    r.z = rotateAxis(vec3(0, 0, 1), n, a);
    r.o = c - r.dir(c);
    return r;
}

// Where a module is: on its seat (turning with its ring), or on its way in (carried by its tugs to a point off its
// seat, then eased home), rolled a little as it comes.
Xf OrbitalUniverse::moduleXf(const Module& m, float* tugAccel, vec3* tugPos, bool* tugOn) const {
    const Xf R = ringXf(m.ring);
    Xf seat;
    seat.o = R.at(m.f.o);
    seat.x = R.dir(m.f.x);
    seat.y = R.dir(m.f.y);
    seat.z = R.dir(m.f.z);
    if (tugOn) *tugOn = false;
    if (m.state != 1) return seat;
    const vec3 dir = R.dir(m.approach);
    const vec3 piv = seat.at(m.pivot);
    const float TA = flightA(m), TB = flightB(m);
    const float stand = m.extent * 1.1f + 2.f;
    const vec3 p1 = piv + dir * stand;
    const vec3 ctl = p1 + (m.p0 - p1) * 0.45f + dir * (length(m.p0 - p1) * 0.25f);
    vec3 cur;
    float roll = 0.f;
    float acc = 0.f;
    if (m.t < TA) {
        const float u = std::max(0.f, m.t / TA);
        const float s = u * u * u * (u * (u * 6.f - 15.f) + 10.f);   // smootherstep
        const vec3 a = lerp(m.p0, ctl, s), b = lerp(ctl, p1, s);
        cur = lerp(a, b, s);
        roll = m.roll0 * (1.f - s);
        // the smootherstep's acceleration: positive early, braking late
        acc = 60.f * u * (1.f - u) * (1.f - 2.f * u) / 5.8f;
        if (tugPos) *tugPos = cur;
        if (tugOn) *tugOn = true;
    } else {
        const float u = saturate((m.t - TA) / TB);
        const float s = u * u * (3.f - 2.f * u);
        cur = lerp(p1, piv, s);
        if (tugPos) *tugPos = p1;
        if (tugOn) *tugOn = false;
    }
    if (tugAccel) *tugAccel = acc;
    // rotate the seat frame about the approach axis (through the pivot), then move the pivot to cur
    Xf f;
    f.x = rotateAxis(seat.x, dir, roll);
    f.y = rotateAxis(seat.y, dir, roll);
    f.z = rotateAxis(seat.z, dir, roll);
    f.o = cur - f.dir(m.pivot);
    return f;
}

void OrbitalUniverse::startFlight(int i, float delay) {
    Module& m = plan_[i];
    m.state = 1;
    m.t = -delay;
    m.present = 1.f;
    m.latch = 0.f;
    m.fading = false;
    m.fadeT = 0.f;
    m.armed = false;
    m.roll0 = (m.seed - 0.5f) * 1.4f;
    // it comes from far off its side, never from behind the camera
    const Xf R = ringXf(m.ring);
    const vec3 seatP = R.at(m.f.at(m.pivot));
    vec3 d = R.dir(m.approach);
    const float toward = dot(d, look_ * -1.f);
    if (toward > 0.f) d = d + look_ * (toward * 1.3f);
    d = normalize(d + camU_ * 0.35f + camR_ * ((m.seed - 0.5f) * 0.6f));
    const float far = std::max(std::max(camF_, 8.f) * 2.6f, m.extent * 7.f + 20.f);
    m.p0 = seatP + d * far;
}

// =====================================================================================================
// Lights
// =====================================================================================================

// A lamp's level: its group on with the song's level, all of them switched on in a sequence on the drop (from the
// core outward), off from the rim inward at game over (and the same around a song: powered up, then down).
float OrbitalUniverse::lampK(int group, float order) const {
    float k = groupOn_[std::clamp(group, 0, 2)];
    k = std::max(k, smoothstepf(order - 0.06f, order + 0.06f, dropSeq_));
    k *= 1.f - smoothstepf(1.f - order - 0.1f, 1.f - order + 0.1f, std::max(over_, powerDown_) * 1.25f);
    k *= smoothstepf(order - 0.1f, order + 0.1f, powerUp_ * 1.25f - 0.1f);
    // a quad: a wave of light running out from the core
    if (quadT_ < 2.5f) k += 1.6f * std::exp(-sq((order - quadT_ * 0.6f) / 0.1f)) * (1.f - quadT_ / 2.5f);
    return k;
}

// The drop's chase around a ring: a few heads of light sweeping round on the beat grid (one turn every two bars on
// the wheel, every four on the great ring), each with a tail.
float OrbitalUniverse::chaseAt(float ang, int ring) const {
    const float heads = rings_[std::clamp(ring, 1, RINGS - 1)].heads;
    const float turn = rings_[std::clamp(ring, 1, RINGS - 1)].turn;
    const float head = (float)std::fmod(beat_ / turn, 1.0) * TAU;
    float x = (head - ang) / TAU * heads;
    x -= std::floor(x);
    return std::exp(-x * 9.f) + 0.6f * std::exp(-(1.f - x) * 40.f);
}

// =====================================================================================================
// Modules
// =====================================================================================================

void OrbitalUniverse::emitModule(Builder& b, const Module& m, int idx) {
    const Palette& p = pal_;
    const vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
    const float sd = m.seed;
    const float order = m.order;
    // nav lights: blinking every two beats (or on a slow clock in a calm part), red and green at the extremities
    const float beatPh = (float)std::fmod(beat_ * 0.5 + sd * 0.37, 1.0);
    const float slowPh = std::fmod(time_ * 0.45f + sd, 1.f);
    const float blink = lerpf(std::exp(-slowPh * 9.f), std::exp(-beatPh * 8.f), std::max(wm_, life_));
    const float navK = lampK(0, order);
    const float lamp1 = lampK(1, order), lamp2 = lampK(2, order);
    const float strobe = blink * navK;
    switch (m.kind) {
    case K_CAPSULE: {
        // a service module with its wings and engine, the descent capsule, the orbital sphere, the docking probe
        b.cyl(vec3(0, 0, -1.6f), Z, 1.35f, 1.1f, M_HULL, p.hull2, 1.f, false, sd);
        b.cyl(vec3(0, 0, -0.62f), Z, 1.38f, 0.12f, M_GOLD, p.gold, 1.f, false, sd);
        b.cyl(vec3(0, 0, -3.0f), Z, 0.62f, 0.32f, M_DARK, p.dark, 0.5f);
        b.cyl(vec3(0, 0, 0.3f), Z, 1.15f, 0.75f, M_HULL, p.hull, 0.55f, false, sd + 0.3f);
        b.sphere(vec3(0, 0, 1.85f), 1.05f, M_HULL, p.hull2 * 0.95f, sd + 0.6f);
        b.cyl(vec3(0, 0, 3.0f), Z, 0.38f, 0.16f, M_SILVER, vec3(0.8f));
        for (int s = -1; s <= 1; s += 2) {
            b.beam(vec3(s * 1.3f, 0, -1.6f), vec3(s * 1.9f, 0, -1.6f), 0.06f, M_DARK, p.dark);
            for (int k = 0; k < 3; k++)
                b.boxA(vec3(s * (2.75f + k * 1.7f), 0, -1.6f), vec3(0.8f, 0.025f, 0.62f), M_SOLAR, p.solar, sd + k);
            b.light(vec3(s * 6.6f, 0.f, -1.6f), 0.35f, s < 0 ? p.navR : p.navG, (0.4f + 1.2f * blink) * navK);
        }
        b.light(vec3(0, 1.45f, -2.4f), 0.3f, vec3(1.f), strobe * 1.5f);
        b.light(vec3(0.7f, 0.55f, 0.55f), 0.22f, p.window, 0.8f * b.emitK);
        break;
    }
    case K_NODE: {
        const float r = m.dim.x, hl = m.dim.z;
        b.cyl(vec3(0), Z, r, hl, M_HULL, p.hull, 1.f, false, sd);
        b.cyl(vec3(0, 0, hl - 0.15f), Z, r * 0.62f, 0.3f, M_SILVER, vec3(0.75f));
        b.cyl(vec3(0, 0, -hl + 0.15f), Z, r * 0.62f, 0.3f, M_SILVER, vec3(0.75f));
        b.cyl(vec3(0), Z, r + 0.04f, 0.35f, M_GOLD, p.gold, 1.f, false, sd);
        // radial ports
        const vec3 dirs[4] = {X, X * -1.f, Y, Y * -1.f};
        for (int k = 0; k < 4; k++) {
            if (m.dim.y < 1.5f && k == 3) continue;
            b.cyl(dirs[k] * (r + 0.3f), dirs[k], 0.95f, 0.32f, M_HULL, p.hull * 0.9f, 1.f, false, sd + k);
            b.cyl(dirs[k] * (r + 0.66f), dirs[k], 0.72f, 0.05f, M_SILVER, vec3(0.7f));
        }
        b.light(vec3(r + 0.7f, 0.9f, 0), 0.3f, p.lamp, 0.8f * lamp1);
        b.light(vec3(-r - 0.7f, 0.9f, 0), 0.3f, p.lamp, 0.8f * lamp1);
        break;
    }
    case K_LAB: {
        const float r = m.dim.x, hl = m.dim.z;
        b.cyl(vec3(0), Z, r, hl - 0.6f, M_HULL, p.hull, 1.f, false, sd);
        b.cyl(vec3(0, 0, hl - 0.3f), Z, r, 0.3f, M_HULL, p.hull * 0.92f, 0.72f, false, sd);
        b.cyl(vec3(0, 0, -hl + 0.3f), Z * -1.f, r, 0.3f, M_HULL, p.hull * 0.92f, 0.72f, false, sd);
        b.cyl(vec3(0, 0, hl), Z, r * 0.55f, 0.12f, M_SILVER, vec3(0.75f));
        b.cyl(vec3(0, 0, -hl), Z, r * 0.55f, 0.12f, M_SILVER, vec3(0.75f));
        b.cyl(vec3(0, 0, hl * 0.25f), Z, r + 0.03f, hl * 0.22f, m.dim.y > 1.5f ? M_GOLD : M_HULL, m.dim.y > 1.5f ? p.gold : p.trim, 1.f, false, sd);
        // equipment on its flanks, a window, a handrail
        b.boxA(vec3(r + 0.25f, -0.6f, -hl * 0.4f), vec3(0.3f, 0.5f, 0.9f), M_DARK, p.dark, sd);
        b.boxA(vec3(-r - 0.2f, 0.4f, hl * 0.55f), vec3(0.25f, 0.45f, 0.7f), M_GOLD, p.gold, sd);
        b.beam(vec3(0.6f, r + 0.12f, -hl * 0.8f), vec3(0.6f, r + 0.12f, hl * 0.8f), 0.04f, M_GOLD, p.gold);
        b.light(vec3(0.f, -r - 0.02f, -hl * 0.2f), 0.25f, p.window, 0.9f * b.emitK);
        b.light(vec3(r * 0.75f, r * 0.75f, hl * 0.6f), 0.35f, p.lamp, 0.9f * lamp2);
        // the robot arm's base on the first lab
        if (m.dim.y > 0.5f && m.dim.y < 1.5f) {
            b.cyl(vec3(0, r + 0.25f, 0), Y, 0.7f, 0.25f, M_DARK, p.dark);
        }
        break;
    }
    case K_TRUSS: {
        const float hl = m.dim.x, w = m.dim.y;
        b.boxA(vec3(0), vec3(hl, w, w), M_TRUSS, p.hull * 0.85f, sd);
        b.beam(vec3(-hl, w + 0.15f, 0.6f), vec3(hl, w + 0.15f, 0.6f), 0.1f, M_SILVER, vec3(0.7f));
        b.boxA(vec3(-hl * 0.35f, -w - 0.5f, 0.2f), vec3(1.2f, 0.45f, 0.8f), M_HULL, p.hull, sd);
        b.boxA(vec3(hl * 0.45f, -w - 0.45f, -0.2f), vec3(0.9f, 0.4f, 0.9f), M_GOLD, p.gold, sd);
        b.boxA(vec3(hl * 0.1f, w + 0.4f, -0.6f), vec3(0.7f, 0.3f, 0.5f), M_DARK, p.dark, sd);
        if (m.dim.z > 0.5f) b.beam(vec3(0, -w, 0), vec3(0, -w - 1.6f, 0), 0.45f, M_HULL, p.hull * 0.9f);
        b.light(vec3(hl, w + 0.2f, w + 0.2f), 0.4f, vec3(1.f), strobe * 1.4f);
        b.light(vec3(0, -w - 1.0f, w + 0.2f), 0.4f, p.lamp, 0.7f * lamp1);
        break;
    }
    case K_ARRAY: {
        // a rotary joint, a mast through the wings' root; two long wings turned to the sun about the truss' axis
        const float W = m.dim.x, L = m.dim.y;
        b.cyl(vec3(1.f, 0, 0), X, 1.35f, 1.0f, M_DARK, p.dark);
        b.cyl(vec3(1.f, 0, 0), X, 1.42f, 0.18f, M_SILVER, vec3(0.75f));
        b.beam(vec3(2.f, 0, 0), vec3(10.f, 0, 0), 0.32f, M_TRUSS, p.hull * 0.8f, sd);
        // the sun in the module's frame -> the wings' angle
        const vec3 sun = p.sunDir;
        const Xf& f = b.xf;
        const float sy = dot(sun, f.y), sz = dot(sun, f.z);
        const float th = std::atan2(-sy, sz) + 0.02f * std::sin(time_ * 0.2f + sd * 6.f);
        const vec3 w(0.f, std::cos(th), std::sin(th));
        const vec3 n = cross(X, w);
        for (int s = -1; s <= 1; s += 2) {
            const vec3 ws = w * (float)s;
            const vec3 c = vec3(5.7f, 0, 0) + ws * (1.6f + L * 0.5f);
            b.boxO(c, X * W, ws * (L * 0.5f), n * 0.05f, M_SOLAR, p.solar, sd + s);
            b.boxO(vec3(5.7f, 0, 0) + ws * 1.1f, X * (W * 0.95f), ws * 0.5f, n * 0.35f, M_GOLD, p.gold, sd);
            b.boxO(vec3(5.7f, 0, 0) + ws * (2.2f + L), X * (W * 0.9f), ws * 0.22f, n * 0.12f, M_HULL, p.hull * 0.8f, sd);
            b.beam(vec3(5.7f, 0, 0) + ws * 1.4f, vec3(5.7f, 0, 0) + ws * (1.9f + L), 0.07f, M_SILVER, vec3(0.8f));
            b.light(vec3(5.7f, 0, 0) + ws * (2.4f + L), 0.5f, s < 0 ? p.navR : p.navG, (0.4f + 1.4f * blink) * navK);
        }
        break;
    }
    case K_CUPOLA: {
        b.cyl(vec3(0, 0, 0.0f), Z, 1.5f, 0.55f, M_HULL, p.hull, 0.78f, false, sd);
        b.cyl(vec3(0, 0, 0.62f), Z, 0.85f, 0.07f, M_GLASS, vec3(0.3f), 1.f, false, sd);
        for (int k = 0; k < 6; k++) {
            const float a = k * TAU / 6.f;
            const vec3 rr(std::cos(a), std::sin(a), 0.f);
            const vec3 t(-std::sin(a), std::cos(a), 0.f);
            b.boxO(rr * 1.26f + vec3(0, 0, 0.05f), t * 0.42f, (rr * 0.38f + Z * 0.92f) * 0.42f, normalize(rr - Z * 0.4f) * 0.05f, M_GLASS, vec3(0.3f), sd + k);
        }
        b.light(vec3(0, 0, 0.75f), 0.9f, p.window, 0.35f * b.emitK);
        break;
    }
    case K_HUB: {
        const float r = m.dim.x, hl = m.dim.z;
        b.cyl(vec3(0), Z, r, hl, M_HULL, p.hull, 1.f, false, sd);
        for (int k = -2; k <= 2; k++) b.cyl(vec3(0, 0, k * hl * 0.36f), Z, r + 0.05f, hl * 0.08f, M_GOLD, p.gold, 1.f, false, sd + k);
        // the wheel's bearing, around the plane of the wheel
        const float zb = 10.9f;
        b.cyl(vec3(0, 0, zb), Z, r + 1.1f, 1.6f, M_DARK, p.dark);
        b.cyl(vec3(0, 0, zb), Z, r + 1.2f, 0.25f, M_SILVER, vec3(0.75f));
        b.cyl(vec3(0, 0, hl + 0.2f), Z, r * 0.6f, 0.3f, M_SILVER, vec3(0.75f));
        for (int k = 0; k < 4; k++) {
            const float a = k * TAU / 4.f + 0.4f;
            b.light(vec3(std::cos(a) * (r + 0.1f), std::sin(a) * (r + 0.1f), -hl * 0.6f), 0.35f, p.lamp, 0.8f * lamp1);
        }
        break;
    }
    case K_SPOKE: {
        for (int s = 0; s < 2; s++) {
            const float a = m.a0 + s * PI;
            const vec3 rr(std::cos(a), std::sin(a), 0.f);
            const vec3 t(-std::sin(a), std::cos(a), 0.f);
            const float r0 = 4.4f, r1 = WHEEL_R - WHEEL_TUBE * 0.6f;
            b.boxO(rr * ((r0 + r1) * 0.5f), t * 1.1f, rr * ((r1 - r0) * 0.5f), Z * 1.1f, M_TRUSS, p.hull * 0.85f, sd + s);
            b.tube(rr * r0 + t * 1.9f, rr * r1 + t * 1.9f, 0.75f, M_HULL, p.hull, sd);
            for (int k = 1; k < 5; k++)
                b.light(rr * lerpf(r0, r1, k / 5.f) + Z * 1.3f, 0.3f, p.lamp, 0.6f * lamp1);
        }
        break;
    }
    case K_ARC: {
        const float R = WHEEL_R, r = WHEEL_TUBE;
        b.torus(vec3(0), Z, X, R, r, m.a0, m.al, M_HAB, p.hull, sd);
        // ribs and a gold band along the inside
        for (int k = 0; k < 3; k++) {
            const float a = m.a0 + m.al * (k + 0.5f) / 3.f;
            b.torus(vec3(0), Z, X, R, r * 1.06f, a - 0.012f, 0.024f, M_DARK, p.dark, sd + k);
        }
        b.torus(vec3(0), Z, X, R - r * 0.98f, r * 0.12f, m.a0, m.al, M_GOLD, p.gold, sd);
        // the rim lamps: the drop's chase runs round them
        const int N = 16;
        for (int k = 0; k < N; k++) {
            const float a = m.a0 + m.al * (k + 0.5f) / N;
            const float aw = a + wheelSpin_;
            const float ch = chaseAt(aw, 1) * life_;
            const vec3 e(std::cos(a), std::sin(a), 0.f);
            const float base = 0.25f + 0.55f * lamp1 + 0.25f * kickGlow_;
            for (int s = -1; s <= 1; s += 2)
                b.light(e * R + Z * (r * 1.04f * s), 1.1f, lerp(p.lamp, p.chase, saturate(ch * 2.f)), base * navK * 0.6f + ch * 2.2f);
        }
        break;
    }
    case K_SHIP: {
        // a crew ship: its trunk with solar cells, the capsule, the nose
        b.cyl(vec3(0, 0, -2.2f), Z, 1.8f, 1.3f, M_SOLAR, p.solar, 1.f, false, sd);
        b.cyl(vec3(0, 0, -3.55f), Z, 1.85f, 0.08f, M_HULL, p.hull, 1.f, false, sd);
        b.cyl(vec3(0, 0, -0.85f), Z, 1.82f, 0.08f, M_SHIELD, p.dark * 1.5f, 1.f, false, sd);
        b.cyl(vec3(0, 0, 0.45f), Z, 1.82f, 1.2f, M_HULL, p.hull, 0.52f, false, sd);
        b.cyl(vec3(0, 0, 1.75f), Z, 0.7f, 0.1f, M_SILVER, vec3(0.75f));
        for (int s = -1; s <= 1; s += 2) {
            b.boxA(vec3(s * 2.6f, 0, -2.2f), vec3(0.8f, 0.04f, 1.0f), M_SOLAR, p.solar, sd + s);
            b.light(vec3(s * 3.5f, 0, -2.2f), 0.3f, s < 0 ? p.navR : p.navG, (0.4f + blink) * navK);
        }
        for (int k = 0; k < 4; k++) {
            const float a = k * TAU / 4.f + 0.785f;
            b.light(vec3(std::cos(a) * 1.4f, std::sin(a) * 1.4f, 0.6f), 0.22f, p.window, 0.6f * b.emitK);
        }
        break;
    }
    case K_RADIATOR: {
        const float hw = m.dim.x, hp = m.dim.y;
        b.beam(vec3(0, 0, 0), vec3(0, 0, 13.f), 0.22f, M_DARK, p.dark);
        for (int k = 0; k < 3; k++) {
            const float zc = 2.3f + k * 4.3f;
            const float tilt = 0.35f;
            const vec3 ax(1, 0, 0), ay(0, std::cos(tilt), std::sin(tilt)), az = cross(ax, ay);
            b.boxO(vec3(0, 0, zc), ax * hw, ay * 0.05f, az * hp, M_RADIATOR, p.radiator, sd + k);
        }
        b.light(vec3(0, 0, 13.2f), 0.35f, vec3(1.f), strobe);
        break;
    }
    case K_DISH: {
        b.tube(vec3(0, 0, 0), vec3(0, 0, 2.6f), 0.18f, M_DARK, p.dark);
        b.tube(vec3(0, 0, 2.6f), vec3(0, -1.0f, 2.6f), 0.14f, M_DARK, p.dark);
        b.cyl(vec3(0, -1.35f, 2.6f), vec3(0, -1, 0), 0.18f, 0.35f, M_HULL, p.hull, 9.f, false, sd);
        b.cyl(vec3(0, -2.0f, 2.6f), vec3(0, -1, 0), 0.12f, 0.45f, M_GOLD, p.gold, 0.5f);
        b.light(vec3(0, -2.5f, 2.6f), 0.25f, p.navR, strobe);
        break;
    }
    case K_AXLE: {
        // the spindle down from the core to the great hub, the hub's drum, its collars and docking bays
        const vec3 top = vec3(0.f, -GREAT_C.y - 2.6f, 0.f);    // the core's underside, in the module's frame
        b.tube(top, vec3(0, 60.f, 0), 3.2f, M_HULL, p.hull, sd);
        b.boxO(vec3(0, (top.y + 60.f) * 0.5f, 5.f), X * 1.4f, Y * ((top.y - 60.f) * 0.5f), Z * 1.4f, M_TRUSS, p.hull * 0.85f, sd);
        for (int k = 1; k < 6; k++) b.light(vec3(0, lerpf(60.f, top.y, k / 6.f), 7.f), 1.5f, p.lamp, 0.4f + 0.5f * lamp1);
        b.cyl(vec3(0, 0, 0), Y, 300.f, 60.f, M_HULL, p.hull, 1.f, false, sd);
        b.cyl(vec3(0, 66.f, 0), Y, 230.f, 8.f, M_DARK, p.dark, 0.6f, false, sd);
        b.cyl(vec3(0, -66.f, 0), Y * -1.f, 230.f, 8.f, M_DARK, p.dark, 0.6f, false, sd);
        b.cyl(vec3(0, 61.f, 0), Y, 300.5f, 3.f, M_GOLD, p.gold, 1.f, false, sd);
        for (int k = 0; k < 8; k++) {
            const float a = k * TAU / 8.f;
            b.boxO(vec3(std::cos(a), 0.f, std::sin(a)) * 180.f + Y * 62.f, vec3(std::cos(a), 0.f, std::sin(a)) * 110.f, Y * 4.f, vec3(-std::sin(a), 0.f, std::cos(a)) * 9.f, M_SILVER, vec3(0.8f), sd);
        }
        b.cyl(vec3(0, 0, 0), Y, 306.f, 12.f, M_DARK, p.dark, 1.f, false, sd);
        b.cyl(vec3(0, -110.f, 0), Y, 70.f, 50.f, M_HULL, p.hull * 0.9f, 0.4f, false, sd);
        for (int k = 0; k < 24; k++) {
            const float a = k * TAU / 24.f;
            const vec3 e(std::cos(a), 0.f, std::sin(a));
            b.light(e * 308.f + Y * 32.f, 8.f, p.lamp, 0.5f + 0.5f * lamp1);
            b.light(e * 308.f - Y * 32.f, 8.f, p.window, 0.7f * navK);
        }
        break;
    }
    case K_BIGSPOKE: {
        for (int s = 0; s < 2; s++) {
            const float a = m.a0 + s * PI;
            const vec3 rr(std::cos(a), 0.f, -std::sin(a));
            const vec3 t(-std::sin(a), 0.f, -std::cos(a));
            const float r0 = 300.f, r1 = GREAT_R - 30.f;
            b.boxO(rr * ((r0 + r1) * 0.5f), t * 45.f, rr * ((r1 - r0) * 0.5f), Y * 45.f, M_TRUSS, p.hull * 0.85f, sd + s);
            b.tube(rr * r0 + Y * 70.f, rr * r1 + Y * 70.f, 22.f, M_HULL, p.hull, sd);
            for (int k = 1; k < 12; k++)
                b.light(rr * lerpf(r0, r1, k / 12.f) + Y * 96.f, 22.f, p.lamp, 0.4f + 0.5f * lamp1);
        }
        break;
    }
    case K_BIGARC: {
        const float R = GREAT_R, hw = GREAT_HW;
        int built = 0;
        for (int i = greatFirst_; i <= greatLast_; i++)
            if (plan_[i].kind == K_BIGARC && plan_[i].state == 2) built++;
        const float towns = saturate(0.15f + 0.04f * built + b.towns);
        b.band(vec3(0), Y, X, R, hw, m.a0, m.al, GREAT_WALL, M_LAND, p.hull * 0.75f, sd, b.emitK, towns);
        // ribs across its outside, its rims
        for (int k = 0; k < 4; k++) {
            const float a = m.a0 + m.al * (k + 0.5f) / 4.f;
            b.band(vec3(0), Y, X, R + 12.f, hw * 1.02f, a - 0.0016f, 0.0032f, 0.f, M_DARK, p.dark, sd + k, 0.f, 0.f);
        }
        b.torus(vec3(0), Y, X, R - GREAT_WALL * 0.5f, 22.f, m.a0, m.al, M_HULL, p.hull, sd);
        // lamps along the walls' tops: the drop's chase runs round them
        const int N = 12;
        for (int k = 0; k < N; k++) {
            const float a = m.a0 + m.al * (k + 0.5f) / N;
            const float aw = a + greatSpin_;
            const float ch = chaseAt(aw, 2) * life_;
            const vec3 e(std::cos(a), 0.f, -std::sin(a));
            const float base = 0.3f + 0.5f * lamp1 + 0.2f * kickGlow_;
            for (int s = -1; s <= 1; s += 2)
                b.light(e * (R - GREAT_WALL) + Y * (hw * s), 70.f, lerp(p.lamp, p.chase, saturate(ch * 2.f)), base * navK + ch * 2.6f);
        }
        break;
    }
    case K_MIRROR: {
        // a spire on the rim: a slender lattice mast, a pod, a beacon at its tip
        const float a = m.a0;
        const vec3 rr(std::cos(a), 0.f, -std::sin(a));
        const vec3 base = rr * (GREAT_R - GREAT_WALL * 0.5f) + Y * GREAT_HW;
        const float H = GREAT_R * 0.12f;
        const vec3 t(-std::sin(a), 0.f, -std::cos(a));
        b.boxO(base + Y * (H * 0.5f), t * 22.f, Y * (H * 0.5f), rr * 22.f, M_TRUSS, p.hull * 0.85f, sd);
        b.cyl(base + Y * 30.f, Y, 70.f, 30.f, M_DARK, p.dark, 0.6f, false, sd);
        b.cyl(base + Y * (H * 0.72f), Y, 60.f, 55.f, M_HULL, p.hull, 1.f, false, sd);
        b.cyl(base + Y * (H * 0.72f), Y, 63.f, 10.f, M_GOLD, p.gold, 1.f, false, sd);
        b.cyl(base + Y * (H * 0.72f + 70.f), Y, 45.f, 15.f, M_HULL, p.hull * 0.9f, 0.3f, false, sd);
        b.tube(base + Y * (H * 0.72f + 80.f), base + Y * (H + 40.f), 6.f, M_SILVER, vec3(0.8f), sd);
        const float ch = chaseAt(a + greatSpin_, 2) * life_;
        b.light(base + Y * (H + 45.f), 60.f, lerp(p.navR, p.chase, saturate(ch * 2.f)), (0.5f + 1.2f * blink) * navK + ch * 3.f);
        for (int k = 0; k < 4; k++) {
            const float ang = k * TAU / 4.f;
            b.light(base + Y * (H * 0.72f) + (rr * std::cos(ang) + t * std::sin(ang)) * 64.f, 14.f, p.window, 0.8f * b.emitK);
        }
        break;
    }
    case K_LINER: {
        // a liner moored off the great ring: a long hull, its drive, rows of windows
        const float a = m.a0;
        const vec3 rr(std::cos(a), 0.f, -std::sin(a));
        const vec3 t(-std::sin(a), 0.f, -std::cos(a));
        const vec3 c = m.pivot;
        b.cyl(c, t, 34.f, 150.f, M_HAB, p.hull, 1.f, false, sd);
        b.cyl(c + t * 165.f, t, 34.f, 18.f, M_HULL, p.hull * 0.95f, 0.4f, false, sd);
        b.cyl(c - t * 160.f, t * -1.f, 30.f, 16.f, M_DARK, p.dark, 0.6f, false, sd);
        b.tube(c - rr * 34.f, c - rr * 220.f, 8.f, M_HULL, p.hull * 0.85f, sd);
        b.boxO(c + Y * 40.f, t * 90.f, Y * 4.f, rr * 26.f, M_SOLAR, p.solar, sd);
        b.light(c - t * 185.f, 26.f, p.thrust, 0.6f * navK);
        b.light(c + t * 185.f + Y * 20.f, 12.f, vec3(1.f), strobe * 1.4f);
        break;
    }
    case K_NRING:
    case K_COLOSSUS: {
        // a ring arc: a band of land (flag 0) or a tube of windows (flag 1); lamps along it, the drop's chase
        const Ring& RG = rings_[m.ring];
        const float R = m.dim.x, hw = m.dim.y;
        const vec3 n = RG.n, ref = RG.ref, cr = cross(n, ref);
        const bool tube = m.dim.z > 0.5f;
        if (m.kind == K_COLOSSUS) {
            // a spoke of lattice and light from the arc down toward the planet
            const float mid = m.a0 + m.al * 0.5f;
            const vec3 e = ref * std::cos(mid) + cr * std::sin(mid);
            b.beam(e * (R_PLANET * 2.5f), e * (R * 0.97f), R * 0.004f, M_TRUSS, p.hull * 0.8f, sd);
            for (int i = 1; i < 16; i++)
                b.light(e * lerpf(R_PLANET * 2.5f, R * 0.97f, i / 16.f), R * 0.004f, i % 2 ? p.chase : p.lamp,
                        0.6f + 0.4f * lamp1 + 2.f * life_ * chaseAt(i / 16.f * TAU, m.ring));
        }
        if (tube) {
            b.torus(vec3(0), n, ref, R, hw, m.a0, m.al, M_HAB, m.kind == K_COLOSSUS ? p.hull * 0.55f : p.hull, sd);
            for (int k = 0; k < 3; k++)
                b.torus(vec3(0), n, ref, R, hw * 1.05f, m.a0 + m.al * (k + 0.5f) / 3.f - 0.004f, 0.008f, M_DARK, p.dark, sd + k);
        } else {
            b.band(vec3(0), n, ref, R, hw, m.a0, m.al, hw * 0.18f, M_LAND, p.hull * 0.75f, sd, b.emitK, 0.6f);
            for (int k = 0; k < 4; k++) {
                const float a = m.a0 + m.al * (k + 0.5f) / 4.f;
                b.band(vec3(0), n, ref, R * 1.0015f, hw * 1.02f, a - 0.002f, 0.004f, 0.f, M_DARK, p.dark, sd + k, 0.f, 0.f);
            }
            b.torus(vec3(0), n, ref, R - hw * 0.09f, hw * 0.02f, m.a0, m.al, M_HULL, p.hull, sd);
        }
        const int N = 14;
        for (int k = 0; k < N; k++) {
            const float a = m.a0 + m.al * (k + 0.5f) / N;
            const float ch = chaseAt(a + RG.spin, m.ring) * life_;
            const vec3 e = ref * std::cos(a) + cr * std::sin(a);
            const float base = 0.3f + 0.5f * lamp1 + 0.2f * kickGlow_;
            const vec3 col = lerp(p.lamp, p.chase, saturate(ch * 2.f));
            if (tube) b.light(e * (R + hw * 1.02f), hw * 0.5f, col, base * navK + ch * 2.4f);
            else
                for (int s = -1; s <= 1; s += 2) b.light(e * (R - hw * 0.18f) + n * (hw * s), hw * 0.06f, col, base * navK + ch * 2.4f);
        }
        break;
    }
    case K_YARD: {
        // a shipyard: a lattice cradle, a ship growing in it, sparks at its growing end; done, it sails, a new one starts
        const float L = 9.e4f, W = 1.6e4f;
        for (int s = -1; s <= 1; s += 2) b.boxA(vec3(0, W * s, 0), vec3(L, 1400.f, 1400.f), M_TRUSS, p.hull * 0.85f, sd + s);
        b.boxA(vec3(0, 0, -W), vec3(L, 1400.f, 1400.f), M_TRUSS, p.hull * 0.85f, sd);
        for (int k = 0; k <= 8; k++) {
            const float x = -L + 2.f * L * k / 8.f;
            b.beam(vec3(x, W, 0), vec3(x, W, -W), 900.f, M_TRUSS, p.hull * 0.8f, sd);
            b.beam(vec3(x, -W, 0), vec3(x, -W, -W), 900.f, M_TRUSS, p.hull * 0.8f, sd);
            b.beam(vec3(x, -W, -W), vec3(x, W, -W), 900.f, M_TRUSS, p.hull * 0.8f, sd);
            b.light(vec3(x, W + 1500.f, 0), 1500.f, p.lamp, 0.5f + 0.5f * lamp1);
            b.light(vec3(x, -W - 1500.f, 0), 1500.f, p.lamp, 0.5f + 0.5f * lamp1);
        }
        const float g = std::fmod(time_ / 46.f + m.a0, 1.f);
        const float len = 1.55e5f * smoothstepf(0.f, 1.f, std::min(1.f, g / 0.72f));
        const float go = g > 0.78f ? sq((g - 0.78f) / 0.22f) * 4.e6f : 0.f;
        const float x0 = -L * 0.9f + go;
        if (len > 2000.f) {
            const float r = 1.05e4f;
            b.cyl(vec3(x0 + len * 0.5f, 0, -W * 0.45f), X, r, len * 0.5f, M_HAB, p.hull, 1.f, false, sd);
            if (g > 0.72f) {
                b.cyl(vec3(x0 + len + 9000.f, 0, -W * 0.45f), X, r, 9000.f, M_HULL, p.hull * 0.95f, 0.25f, false, sd);
                b.cyl(vec3(x0 - 6000.f, 0, -W * 0.45f), X * -1.f, r * 0.8f, 6000.f, M_DARK, p.dark, 0.6f, false, sd);
                b.boxA(vec3(x0 + len * 0.4f, 0, -W * 0.45f + r * 1.1f), vec3(len * 0.25f, 600.f, 3000.f), M_GOLD, p.gold, sd);
            }
            if (go > 0.f) b.light(vec3(x0 - 13000.f, 0, -W * 0.45f), 9000.f, p.thrust, 1.6f, 2, X * -1.f);
            else {
                // the welders at the growing end
                for (int k = 0; k < 6; k++) {
                    const float fl = 0.5f + 0.5f * std::sin(time_ * (13.f + k * 3.1f) + k * 2.f);
                    const float ang = k * TAU / 6.f + time_ * 0.7f;
                    b.light(vec3(x0 + len, std::cos(ang) * r * 1.1f, -W * 0.45f + std::sin(ang) * r * 1.1f), 1800.f,
                            k % 2 ? vec3(1.f, 0.9f, 0.7f) : vec3(0.7f, 0.85f, 1.f), 1.4f * fl, 3);
                }
            }
        }
        break;
    }
    case K_TETHER: {
        // a segment of the space elevator (radii from the planet's center along its line), its lights
        const vec3 up(m.dim.x, m.dim.y, m.dim.z);
        const float r0 = R_PLANET + m.a0, r1 = R_PLANET + m.al;
        const int seg = (m.a0 < 1.f && m.al > 2.e6f) ? 5 : m.a0 < 1.f ? 2 : m.a0 > 9.e5f ? 4 : m.a0 > ALT ? 3 : m.a0 < 2.e5f ? 1 : 0;
        if (seg != 5) b.tube(up * std::min(r0, r1), up * std::max(r0, r1), 3500.f, M_SILVER, vec3(0.85f), sd);
        if (seg == 0) {
            b.cyl(up * (R_PLANET + ALT), up, 3.2e4f, 1.2e4f, M_HULL, p.hull, 1.f, false, sd);
            b.cyl(up * (R_PLANET + ALT), up, 3.4e4f, 2500.f, M_GOLD, p.gold, 1.f, false, sd);
            vec3 u, v;
            basisOf(up, u, v);
            b.torus(up * (R_PLANET + ALT), up, u, 6.e4f, 3000.f, 0.f, TAU, M_HAB, p.hull, sd);
            for (int i = 0; i < 12; i++) {
                const float a = i * TAU / 12.f;
                b.light(up * (R_PLANET + ALT) + (u * std::cos(a) + v * std::sin(a)) * 6.3e4f, 4000.f, p.lamp, 0.5f + 0.6f * lamp1);
            }
        }
        if (seg == 2) {
            b.cyl(up * (R_PLANET + 6000.f), up, 2.5e4f, 6000.f, M_DARK, p.dark, 0.3f, false, sd);
            b.light(up * (R_PLANET + 8000.f), 4.e4f, p.city, 1.2f);
        }
        if (seg == 4) {
            b.cyl(up * (R_PLANET + 2.2e6f), up, 6.e4f, 9.e4f, M_HAB, p.hull, 1.f, false, sd);
            vec3 u, v;
            basisOf(up, u, v);
            b.boxO(up * (R_PLANET + 2.2e6f), u * 2.6e5f, v * 6.e4f, up * 1500.f, M_SOLAR, p.solar, sd);
            b.light(up * (R_PLANET + 2.3e6f), 3.e4f, p.navR, (0.5f + blink) * navK);
        }
        if (seg != 5) {
            const int n = std::clamp((int)(std::fabs(r1 - r0) / 4.e4f), 2, 50);
            for (int i = 0; i <= n; i++)
                b.light(up * lerpf(r0, r1, i / (float)n), 5000.f, p.lamp, 0.35f + 0.4f * lamp1);
        } else {
            // the climbers, up and down the line
            vec3 u, v;
            basisOf(up, u, v);
            for (int i = 0; i < 14; i++) {
                const float ph = std::fmod(time_ * (i % 2 ? 0.012f : -0.012f) + i / 14.f + 10.f, 1.f);
                const float r = R_PLANET + 2.e4f + ph * 2.15e6f;
                b.cyl(up * r + u * 5000.f, up, 7000.f, 1.4e4f, M_HAB, p.hull, 1.f, false, sd + i);
                b.light(up * r + u * 5000.f, 1.2e4f, i % 2 ? p.chase : p.lamp, 0.9f + 0.6f * lamp1);
            }
        }
        break;
    }
    case K_ERING: {
        const Ring& RG = rings_[m.ring];
        const float R = m.dim.x, hw = m.dim.y;
        const vec3 n = RG.n, ref = RG.ref, cr = cross(n, ref);
        b.torus(vec3(0), n, ref, R, hw, m.a0, m.al, M_HAB, p.hull, sd);
        b.torus(vec3(0), n, ref, R - hw * 1.6f, hw * 0.25f, m.a0, m.al, M_GOLD, p.gold, sd);
        for (int k = 0; k < 4; k++)
            b.torus(vec3(0), n, ref, R, hw * 1.06f, m.a0 + m.al * (k + 0.5f) / 4.f - 0.0015f, 0.003f, M_DARK, p.dark, sd + k);
        const int N = 18;
        for (int k = 0; k < N; k++) {
            const float a = m.a0 + m.al * (k + 0.5f) / N;
            const float ch = chaseAt(a + RG.spin, m.ring) * life_;
            const vec3 e = ref * std::cos(a) + cr * std::sin(a);
            b.light(e * (R + hw * 1.05f), hw * 0.9f, lerp(p.lamp, p.chase, saturate(ch * 2.f)), (0.3f + 0.5f * lamp1) * navK + ch * 2.4f);
        }
        break;
    }
    case K_ELEVATOR: {
        // a whole space elevator along its spoke: the anchor on the ground, the tether up to the counterweight on
        // its ring, a port at the station's height, climbers up and down
        const vec3 up(m.dim.x, m.dim.y, m.dim.z);
        vec3 u, v;
        basisOf(up, u, v);
        const float rTop = R_PLANET + m.al, rPort = R_PLANET + ALT + 4.5e5f;
        b.tube(up * R_PLANET, up * rTop, 1.4e4f, M_SILVER, vec3(0.85f), sd);
        b.tube(up * R_PLANET + u * 3.e4f, up * rTop + u * 3.e4f, 5000.f, M_GOLD, p.gold, sd);
        b.cyl(up * (R_PLANET + 2.e4f), up, 9.e4f, 2.e4f, M_DARK, p.dark, 0.3f, false, sd);
        b.light(up * (R_PLANET + 3.e4f), 1.4e5f, p.city, 1.4f);
        b.cyl(up * rPort, up, 1.2e5f, 5.e4f, M_HULL, p.hull, 1.f, false, sd);
        b.cyl(up * rPort, up, 1.3e5f, 9000.f, M_GOLD, p.gold, 1.f, false, sd);
        b.torus(up * rPort, up, u, 2.2e5f, 1.2e4f, 0.f, TAU, M_HAB, p.hull, sd);
        for (int i = 0; i < 10; i++) {
            const float a = i * TAU / 10.f;
            b.light(up * rPort + (u * std::cos(a) + v * std::sin(a)) * 2.3e5f, 1.6e4f, p.lamp, 0.5f + 0.6f * lamp1);
        }
        b.cyl(up * rTop, up, 1.8e5f, 2.4e5f, M_HAB, p.hull, 1.f, false, sd);
        b.boxO(up * rTop, u * 7.e5f, v * 1.6e5f, up * 4000.f, M_SOLAR, p.solar, sd);
        b.light(up * (rTop + 2.6e5f), 9.e4f, p.navR, (0.5f + blink) * navK);
        const int n = std::clamp((int)(m.al / 5.e4f), 20, 140);
        for (int i = 0; i <= n; i++)
            b.light(up * lerpf(R_PLANET, rTop, i / (float)n), 1.6e4f, p.lamp, 0.35f + 0.4f * lamp1);
        for (int i = 0; i < 10; i++) {
            const float ph = std::fmod(time_ * (i % 2 ? 0.02f : -0.02f) + i / 10.f + m.a0 * 0.13f + 10.f, 1.f);
            const float r = R_PLANET + 4.e4f + ph * (m.al - 3.e5f);
            b.cyl(up * r + u * 1.6e4f, up, 2.4e4f, 4.e4f, M_HAB, p.hull, 1.f, false, sd + i);
            b.light(up * r + u * 1.6e4f, 4.e4f, i % 2 ? p.chase : p.lamp, 1.1f + 0.6f * lamp1);
        }
        break;
    }
    case K_FLEET: {
        // a pair of O'Neill cylinders, turning against each other: windows strips, mirrors, end caps, drives
        for (int s = -1; s <= 1; s += 2) {
            const vec3 c(0.f, s * 7.5e5f, 0.f);
            const float r = 2.8e5f, hl = 1.25e6f;
            const float ang = time_ * 0.05f * s + sd * 6.f;
            const vec3 u(0.f, std::cos(ang), std::sin(ang)), v(0.f, -std::sin(ang), std::cos(ang));
            b.put(r * b.s < b.minR ? b.coarse : b.fine, S_CYL, c, u * r, X * hl, v * r, M_HULL, p.hull, 1.f, 1.f, sd + s);
            b.cyl(c + X * (hl + 6.e4f), X, r * 0.9f, 6.e4f, M_DARK, p.dark, 0.5f, false, sd);
            b.cyl(c - X * (hl + 5.e4f), X * -1.f, r * 0.7f, 5.e4f, M_DARK, p.dark, 0.6f, false, sd);
            for (int k = 0; k < 3; k++) {
                const float a = k * TAU / 3.f;
                const vec3 e = u * std::cos(a) + v * std::sin(a), t = u * -std::sin(a) + v * std::cos(a);
                b.boxO(c + e * (r * 1.002f), X * (hl * 0.95f), e * 2000.f, t * (r * 0.28f), M_EMIT, p.window, sd + k, 1.2f * b.emitK);
                b.boxO(c + e * (r * 1.3f) + X * (hl * 0.02f), X * (hl * 0.93f), (e * 0.5f + t * 0.866f) * 3000.f, (t * 0.5f - e * 0.866f) * (r * 0.32f), M_SILVER, vec3(0.9f), sd + k);
            }
            b.light(c - X * (hl + 1.2e5f), 1.6e5f, p.thrust, m.state == 1 ? 1.6f : 0.35f, 2, X * -1.f);
            b.light(c + X * (hl + 1.3e5f), 4.e4f, vec3(1.f), strobe * 1.5f);
        }
        break;
    }
    case K_MOONWORK: {
        const int k = (int)m.a0;
        const float Rm = R_MOON;
        if (k == 0 || k == 2) {
            const vec3 n = k == 0 ? normalize(vec3(0.3f, 1.f, 0.2f)) : normalize(vec3(1.f, 0.3f, -0.5f));
            vec3 u, v;
            basisOf(n, u, v);
            const float R = Rm * (k == 0 ? 1.35f : 1.62f);
            b.torus(vec3(0), n, u, R, Rm * 0.035f, 0.f, TAU, M_HAB, p.hull, sd);
            for (int i = 0; i < 36; i++) {
                const float a = i * TAU / 36.f;
                b.light((u * std::cos(a) + v * std::sin(a)) * (R + Rm * 0.04f), Rm * 0.02f, i % 2 ? p.chase : p.lamp, 0.4f + 0.5f * lamp1 + 1.5f * life_ * chaseAt(a, 2));
            }
        } else if (k == 1) {
            // mining spires over the surface
            Rng r(0x300Dull);
            for (int i = 0; i < 9; i++) {
                const vec3 d = normalize(vec3(r.range(-1.f, 1.f), r.range(-1.f, 1.f), r.range(-1.f, 1.f)));
                b.beam(d * (Rm * 0.98f), d * (Rm * 1.25f), Rm * 0.012f, M_TRUSS, p.hull * 0.85f, sd + i);
                b.light(d * (Rm * 1.26f), Rm * 0.02f, p.navR, (0.5f + blink) * navK);
            }
        } else if (k == 3 || k == 4) {
            b.band(vec3(0), vec3(0, 1, 0), vec3(1, 0, 0), Rm * 1.1f, Rm * 0.55f, (k - 3) * PI, PI, 0.f, M_TRUSS, p.hull * 0.8f, sd, 0.f, 0.f);
        } else {
            // a mass driver flinging glowing loads toward the planet
            const vec3 d = normalize(EARTH_C - MOON_C);
            b.tube(d * (Rm * 0.95f), d * (Rm * 9.f), Rm * 0.02f, M_SILVER, vec3(0.85f), sd);
            for (int i = 0; i < 40; i++) {
                const float ph = std::fmod(time_ * 0.04f + i / 40.f, 1.f);
                const float dd = Rm + ph * (length(EARTH_C - MOON_C) - R_PLANET * 1.3f - Rm);
                b.light(d * dd, Rm * 0.025f, lerp(p.lamp, p.chase, 0.4f), 0.9f * smoothstepf(0.f, 0.05f, ph) * (1.f - smoothstepf(0.9f, 1.f, ph)));
            }
        }
        break;
    }
    default: break;
    }
    (void)idx;
}

// A tug: a squat craft with tanks and thrusters, pushing what it carries (its face toward it); its plumes along
// `exhaust`, as strong as `thrust`.
void OrbitalUniverse::emitTug(Builder& b, const vec3& pos, const vec3& face, float size, const vec3& exhaust, float thrust,
                              float k, float seed) {
    const Palette& p = pal_;
    b.xf = frameZ(pos, face, std::fabs(face.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0));
    b.pivot = vec3(0.f);
    b.s = size * k;
    b.glowK = 1.f;
    const vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
    b.boxA(vec3(0), vec3(0.45f, 0.32f, 0.55f), M_HULL, p.hull, seed);
    b.boxA(vec3(0, 0.33f, -0.1f), vec3(0.3f, 0.05f, 0.35f), M_GOLD, p.gold, seed);
    b.boxA(vec3(0, 0, 0.62f), vec3(0.22f, 0.22f, 0.08f), M_DARK, p.dark, seed);
    b.sphere(vec3(0.52f, 0, -0.1f), 0.22f, M_HULL, p.trim, seed);
    b.sphere(vec3(-0.52f, 0, -0.1f), 0.22f, M_HULL, p.trim, seed);
    for (int i = 0; i < 4; i++) {
        const float sx = (i & 1) ? 0.25f : -0.25f, sy = (i & 2) ? 0.18f : -0.18f;
        b.cyl(vec3(sx, sy, -0.62f), Z * -1.f, 0.09f, 0.07f, M_DARK, p.dark, 0.6f);
    }
    const float blink = std::exp(-std::fmod(time_ * 1.1f + seed * 3.f, 1.f) * 8.f);
    b.light(vec3(0.62f, 0, 0), 0.08f, p.navG, 0.5f + blink);
    b.light(vec3(-0.62f, 0, 0), 0.08f, p.navR, 0.5f + blink);
    b.light(vec3(0, 0.1f, 0.7f), 0.12f, vec3(1.f, 0.95f, 0.85f), 0.8f);
    if (thrust > 0.02f) {
        // plumes from the nozzles facing the exhaust's way
        const Xf& f = b.xf;
        const vec3 el(dot(exhaust, f.x), dot(exhaust, f.y), dot(exhaust, f.z));
        const float back = el.z < 0.f ? 1.f : -1.f;
        for (int i = 0; i < 4; i++) {
            const float sx = (i & 1) ? 0.25f : -0.25f, sy = (i & 2) ? 0.18f : -0.18f;
            const vec3 at = back > 0.f ? vec3(sx, sy, -0.7f) : vec3(sx * 1.8f, sy * 1.6f, 0.55f);
            b.light(at, 0.16f, p.thrust, thrust * 1.6f, 2, el);
        }
    }
}

// The robot arm on the first lab: its tip eased between a folded rest and the module it brings in (two booms,
// solved as a plane triangle, the elbow up).
void OrbitalUniverse::emitArm(Builder& b, float dt) {
    if ((int)plan_.size() < 3) return;
    const Module& lab = plan_[2];
    if (lab.state != 2 || lab.present <= 0.01f) return;
    const Palette& p = pal_;
    const Xf lf = moduleXf(lab, nullptr, nullptr, nullptr);
    const vec3 baseW = lf.at(vec3(0, lab.dim.x + 0.5f, 0));
    const vec3 sh = baseW + vec3(0, 0.9f, 0);
    const float L1 = 8.5f, L2 = 8.5f;
    const vec3 rest = sh + vec3(1.5f, 1.2f, 7.5f);
    vec3 target = rest;
    for (const Module& m : plan_) {
        if (m.state != 1 || !m.armed) continue;
        const float TA = flightA(m), TB = flightB(m);
        const float w = smoothstepf(TA - 1.8f, TA - 0.4f, m.t) * (1.f - smoothstepf(TA + TB + 0.1f, TA + TB + 1.3f, m.t));
        const Xf mf = moduleXf(m, nullptr, nullptr, nullptr);
        const vec3 c = mf.at(m.pivot);
        const vec3 g = c + normalize(sh - c) * (m.extent * 0.55f);
        target = lerp(rest, g, w);
        break;
    }
    // released modules: the arm lingers on the seat a moment
    if (!armInit_) { armTip_ = target; armInit_ = true; }
    const float k = 1.f - std::exp(-dt * 7.f);
    armTip_ = armTip_ + (target - armTip_) * k;
    vec3 d = armTip_ - sh;
    float dl = length(d);
    const vec3 dirT = dl > 1e-3f ? d / dl : vec3(0, 1, 0);
    dl = std::clamp(dl, 1.0f, L1 + L2 - 0.05f);
    const float a = (L1 * L1 - L2 * L2 + dl * dl) / (2.f * dl);
    const float hgt = std::sqrt(std::max(0.f, L1 * L1 - a * a));
    vec3 hint = vec3(1.f, 0.7f, 0.f);
    vec3 hp = hint - dirT * dot(hint, dirT);
    if (length(hp) < 1e-3f) hp = vec3(1, 0, 0);
    hp = normalize(hp);
    const vec3 el = sh + dirT * a + hp * hgt;
    const vec3 wr = sh + dirT * dl;
    b.xf = Xf();
    b.pivot = vec3(0.f);
    b.s = lab.present;
    b.glowK = 1.f;
    b.cyl(baseW + vec3(0, 0.45f, 0), vec3(0, 1, 0), 0.5f, 0.45f, M_HULL, p.hull);
    b.sphere(sh, 0.55f, M_DARK, p.dark);
    b.tube(sh, el, 0.34f, M_HULL, p.hull);
    b.cyl(lerp(sh, el, 0.5f), normalize(el - sh), 0.37f, 0.5f, M_GOLD, p.gold);
    b.sphere(el, 0.5f, M_DARK, p.dark);
    b.tube(el, wr, 0.31f, M_HULL, p.hull);
    const vec3 ed = normalize(wr - el);
    b.sphere(wr, 0.45f, M_DARK, p.dark);
    b.cyl(wr + ed * 0.6f, ed, 0.42f, 0.5f, M_HULL, p.trim);
    b.light(wr + ed * 1.15f, 0.3f, vec3(1.f, 0.95f, 0.85f), 0.8f);
}

// Drones about the station: a few on their rounds at mid, more at a peak; on a drop, before the wheel turns, they
// gather into a halo around the station and their lights chase round it on the beat.
void OrbitalUniverse::emitDrones(Builder& b) {
    const Palette& p = pal_;
    const int nAmb = (int)std::lround(1.f + 3.f * wm_ + 4.f * wp_);
    const bool wheel = plan_.size() > (size_t)wheelLast_ && plan_[wheelLast_].state == 2;
    const float haloK = wheel ? 0.f : halo_;
    const int nHalo = 14;
    const int n = std::max(nAmb, haloK > 0.01f ? nHalo : 0);
    const float F = std::max(camF_, 6.f);
    const float size = std::clamp(F * 0.018f, 0.5f, 240.f);
    const vec3 C = camA_;
    for (int i = 0; i < n; i++) {
        const float h = hash1(i * 3.1f + 0.7f), h2 = hash1(i * 7.3f + 1.1f);
        const float vis = std::max(i < nAmb ? 1.f : 0.f, haloK);
        if (vis <= 0.01f) continue;
        auto posAt = [&](float t) {
            // its round: a tilted loop about the station
            const vec3 ax = normalize(vec3(h - 0.5f, 1.f, h2 - 0.5f));
            vec3 u, v;
            basisOf(ax, u, v);
            const float r = F * (0.75f + 0.45f * h);
            const float th = t * (0.07f + 0.05f * h2) * (i % 2 ? 1.f : -1.f) + h * TAU;
            const vec3 wander = C + (u * std::cos(th) + v * std::sin(th)) * r + ax * (std::sin(t * 0.13f + h * 9.f) * r * 0.15f);
            // the halo: evenly round a tilted circle
            const vec3 hn = normalize(vec3(0.15f, 1.f, 0.35f));
            vec3 hu, hv;
            basisOf(hn, hu, hv);
            const float ha = TAU * i / nHalo + t * 0.12f;
            const vec3 halo = C + (hu * std::cos(ha) + hv * std::sin(ha)) * (F * 1.05f);
            return lerp(wander, halo, i < nHalo ? haloK : 0.f);
        };
        const vec3 pos = posAt(time_), prev = posAt(time_ - 0.1f);
        vec3 vel = (pos - prev) * 10.f;
        const float sp = length(vel);
        const vec3 fwd = sp > 1e-4f ? vel / sp : vec3(1, 0, 0);
        b.xf = frameZ(pos, fwd, std::fabs(fwd.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0));
        b.pivot = vec3(0.f);
        b.s = size * vis;
        b.glowK = 1.f;
        b.boxA(vec3(0), vec3(0.32f, 0.16f, 0.45f), M_HULL, p.hull, h);
        b.boxA(vec3(0, 0.17f, 0), vec3(0.2f, 0.02f, 0.3f), M_GOLD, p.gold, h);
        b.boxA(vec3(0.55f, 0, 0), vec3(0.25f, 0.015f, 0.18f), M_SOLAR, p.solar, h);
        b.boxA(vec3(-0.55f, 0, 0), vec3(0.25f, 0.015f, 0.18f), M_SOLAR, p.solar, h);
        float lk = 0.6f + 0.6f * std::exp(-std::fmod(time_ * 0.9f + h * 5.f, 1.f) * 7.f);
        if (i < nHalo && haloK > 0.f) {
            const float ha = std::fmod(TAU * i / nHalo + time_ * 0.12f, TAU);
            lk = lerpf(lk, 0.4f + 2.5f * chaseAt(ha, 1) * life_, haloK);
        }
        b.light(vec3(0, 0.2f, 0), i < nHalo && haloK > 0.f ? 0.55f : 0.3f, i % 3 == 0 || haloK > 0.5f ? p.chase : p.lamp, lk);
        b.light(vec3(0, 0, -0.5f), 0.14f, p.thrust, 0.5f + 0.5f * std::min(1.f, sp / (F * 0.2f)), 2, vec3(0, 0, -1));
    }
}

// The lines -> how many modules: two a line at first, then ever more, each stage an order of magnitude bigger.
int OrbitalUniverse::wantOf(float lines) const {
    struct P { float l, n; };
    // (lines on the 50-line scale: paceLines() rescales the real lines to the song's length, 50 for a 3:20 song)
    // (the finale done by 50 lines, one song: the wheel by ~5, the forest ring by ~11, then a new stage every four
    // lines or so; the wheel round the planet with its elevators, the biggest in modules, gets six)
    const P T[] = {{0, 1}, {2.5f, 6}, {5, 22}, {11, 51}, {13.5f, 63}, {18.5f, (float)stageFirst_[1]}, {22, (float)stageFirst_[2]},
                   {26, (float)stageFirst_[3]}, {30, (float)stageFirst_[4]}, {36, (float)stageFirst_[5]}, {39.5f, (float)stageFirst_[6]},
                   {43.5f, (float)stageFirst_[7]}, {50, (float)stageFirst_[8]}};
    const int n = (int)(sizeof(T) / sizeof(T[0]));
    const float L = std::max(0.f, lines);
    if (L >= T[n - 1].l) return (int)plan_.size();
    int i = 0;
    while (i + 2 < n && T[i + 1].l <= L) i++;
    const float f = (L - T[i].l) / (T[i + 1].l - T[i].l);
    return (int)std::floor(lerpf(T[i].n, T[i + 1].n, f) + 1e-3f);
}

// The Moon, always there (brought in close): grey seas and craters; as it is taken in hand, its works light up (a
// grid of glowing seams, the great pit of its hollowing).
void OrbitalUniverse::emitMoon(Builder& b) {
    b.xf = Xf();
    b.pivot = vec3(0.f);
    b.s = 1.f;
    b.glowK = 1.f;
    b.chaseOff = 0.f;
    b.put(b.fine, S_SPHERE, MOON_C, vec3(R_MOON, 0, 0), vec3(0, R_MOON, 0), vec3(0, 0, R_MOON), M_MOON, vec3(0.62f, 0.62f, 0.66f), 0.f, 0.f, 0.3f, 0.f,
          0.f, moonWork_ * (0.4f + 0.6f * groupOn_[1]) + 0.6f * life_ * moonWork_);
}

// Traffic: convoys and lone ships on their courses round the works, at the scale of the frame, their drives glowing.
void OrbitalUniverse::emitTraffic(Builder& b) {
    const Palette& p = pal_;
    const float F = std::max(camF_, 6.f);
    if (F < 60.f) return;
    b.xf = Xf();
    b.pivot = vec3(0.f);
    b.s = 1.f;
    b.glowK = 1.f;
    const float more = smoothstepf(std::log(60.f), std::log(3000.f), std::log(F));
    const int convoys = (int)std::lround((2.f + 3.f * wm_ + 3.f * wp_ + 3.f * life_) * more);
    const float sz = F * 0.0035f;
    for (int c = 0; c < convoys; c++) {
        const float h = hash1(c * 5.3f + 1.f), h2 = hash1(c * 2.9f + 7.f);
        const vec3 ax = normalize(vec3(h - 0.5f, 1.f, h2 - 0.5f) + vec3(0.f, 0.f, 0.f));
        vec3 u, v;
        basisOf(ax, u, v);
        const float R = F * (0.45f + 0.6f * h2);
        const float sp = (0.05f + 0.04f * h) * (c % 2 ? 1.f : -1.f);
        for (int j = 0; j < 9; j++) {
            const float th = time_ * sp + h * TAU - j * 0.035f * (sp > 0 ? 1.f : -1.f);
            const vec3 pos = camA_ + (u * std::cos(th) + v * (std::sin(th) * 0.55f)) * R + ax * (R * 0.12f * std::sin(th * 2.f + h));
            const vec3 vel = (u * -std::sin(th) + v * (std::cos(th) * 0.55f)) * (sp > 0 ? 1.f : -1.f);
            b.light(pos, sz, vec3(1.f, 0.97f, 0.9f), 0.9f);
            b.light(pos, sz * 0.9f, p.thrust, 1.1f, 2, normalize(vel) * -1.f);
        }
    }
    const int loners = (int)std::lround((6.f + 8.f * wm_ + 10.f * wp_) * more);
    for (int i = 0; i < loners; i++) {
        const float h = hash1(i * 3.7f + 11.f), h2 = hash1(i * 8.1f + 3.f);
        const float per = 9.f + 8.f * h;
        const float ph = std::fmod(time_ / per + h2, 1.f);
        const vec3 a = camA_ + normalize(vec3(h - 0.5f, h2 - 0.3f, hash1(i * 1.3f) - 0.5f)) * F * 1.4f;
        const vec3 c = camA_ + normalize(vec3(h2 - 0.5f, 0.2f - h, hash1(i * 4.3f) - 0.5f)) * F * 0.2f;
        const vec3 pos = lerp(a, c, ph);
        const float k = smoothstepf(0.f, 0.1f, ph) * (1.f - smoothstepf(0.85f, 1.f, ph));
        b.light(pos, sz * 0.8f, vec3(1.f), 0.8f * k);
        b.light(pos, sz * 0.8f, p.thrust, 1.1f * k, 2, normalize(a - c));
    }
}
