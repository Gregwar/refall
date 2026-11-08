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
    // then liners keep docking along the great ring, for ever (up to a cap)
    for (int k = 0; k < 80; k++) {
        const float a = k * 137.508f * DEG;
        Module& m = add(K_LINER, frameZ(GREAT_C, vec3(0, 0, 1)), vec3(1.f), vec3(std::cos(a), 0.3f, -std::sin(a)), 260.f);
        m.a0 = a;
        m.al = (k % 2 ? 1.f : -1.f);
        m.ring = 2;
        m.pivot = vec3(std::cos(a) * (GREAT_R + 220.f), m.al * GREAT_HW * 0.45f, -std::sin(a) * (GREAT_R + 220.f));
        m.tugs = 0;
    }

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
    if (m.kind == K_BIGARC || m.kind == K_BIGSPOKE || m.kind == K_MIRROR || m.kind == K_AXLE) return 4.6f;
    if (m.kind == K_SHIP || m.kind == K_LINER) return 4.4f;
    return 3.4f;
}
float OrbitalUniverse::flightB(const Module& m) const {
    if (m.kind == K_BIGARC || m.kind == K_BIGSPOKE || m.kind == K_MIRROR || m.kind == K_AXLE) return 2.2f;
    return 1.6f;
}

// A ring's turn: about its axis through its center.
Xf OrbitalUniverse::ringXf(int ring) const {
    Xf r;
    if (ring == 0) return r;
    const vec3 c = ring == 1 ? WHEEL_C : GREAT_C;
    const vec3 n = ring == 1 ? WHEEL_N : GREAT_N;
    const float a = ring == 1 ? wheelSpin_ : greatSpin_;
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
// core outward), off from the rim inward at game over.
float OrbitalUniverse::lampK(int group, float order) const {
    float k = groupOn_[std::clamp(group, 0, 2)];
    k = std::max(k, smoothstepf(order - 0.06f, order + 0.06f, dropSeq_));
    k *= 1.f - smoothstepf(1.f - order - 0.1f, 1.f - order + 0.1f, over_ * 1.25f);
    // a Tetris: a wave of light running out from the core
    if (tetrisT_ < 2.5f) k += 1.6f * std::exp(-sq((order - tetrisT_ * 0.6f) / 0.1f)) * (1.f - tetrisT_ / 2.5f);
    return k;
}

// The drop's chase around a ring: a few heads of light sweeping round on the beat grid (one turn every two bars on
// the wheel, every four on the great ring), each with a tail.
float OrbitalUniverse::chaseAt(float ang, int ring) const {
    const float heads = ring == 1 ? 2.f : 4.f;
    const float turn = ring == 1 ? 8.f : 16.f;
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
