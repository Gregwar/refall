// PLANKS' village, plank by plank. Everything is built from one plank (LC x LC/5 x LC/15) the way a child builds
// with wooden plank blocks: log-cabin courses (planks on edge, crossing at every course, leaving rectangular
// openings), decks of planks laid side by side, upright planks for columns and chimneys, rafters leaning from a
// beam to a ridge, rails laid end to end on sleepers. Each build is sorted bottom-up (a plank is only laid once
// what carries it is there); each side lays its own builds one after the other.
//
// Left: a tall tower with floors and a gable roof, the left half of the track, a pagoda far back, then an endless
// tower behind it all. Right: a temple (a core tower, two porches on columns under lean-to roofs, a stepped
// top), the right half of the track, the train (a locomotive and a carriage, which shuttles along the track once
// built), a long house with a gable roof, then an endless tower.
#include "universes/planks/build.hpp"

#include <algorithm>

namespace planks_build {

Q qBasis(const vec3& X, const vec3& Y, const vec3& Z) {
    // rotation matrix columns X, Y, Z
    const float m00 = X.x, m10 = X.y, m20 = X.z, m01 = Y.x, m11 = Y.y, m21 = Y.z, m02 = Z.x, m12 = Z.y, m22 = Z.z;
    Q q;
    const float tr = m00 + m11 + m22;
    if (tr > 0.f) {
        const float s = std::sqrt(tr + 1.f) * 2.f;
        q.w = 0.25f * s; q.x = (m21 - m12) / s; q.y = (m02 - m20) / s; q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.f + m00 - m11 - m22) * 2.f;
        q.w = (m21 - m12) / s; q.x = 0.25f * s; q.y = (m01 + m10) / s; q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = std::sqrt(1.f + m11 - m00 - m22) * 2.f;
        q.w = (m02 - m20) / s; q.x = (m01 + m10) / s; q.y = 0.25f * s; q.z = (m12 + m21) / s;
    } else {
        const float s = std::sqrt(1.f + m22 - m00 - m11) * 2.f;
        q.w = (m10 - m01) / s; q.x = (m02 + m20) / s; q.y = (m12 + m21) / s; q.z = 0.25f * s;
    }
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}

namespace {

// The plank's poses (its local axes: x the length, y the width, z the thickness).
const Q EX{};                                                    // on edge, along x
const Q EZ = qYaw(PI * 0.5f);                                    // on edge, along z
const Q FX = qAxis(vec3(1, 0, 0), PI * 0.5f);                    // flat, along x
const Q FZ = qMul(qYaw(PI * 0.5f), qAxis(vec3(1, 0, 0), PI * 0.5f)); // flat, along z
const Q UP = qAxis(vec3(0, 0, 1), PI * 0.5f);                    // upright (a column), its face to z
const Q UPZ = qMul(qYaw(PI * 0.5f), qAxis(vec3(0, 0, 1), PI * 0.5f)); // upright, its face to x

// One build: a site on the table (its origin and turn); planks are added in local coordinates (y from the table).
struct Build {
    std::vector<PDef> v;
    vec3 origin{0, 0, 0};
    float yaw = 0.f;
    int group = 0;
    void add(const vec3& p, const Q& q) {
        PDef d;
        d.c = group ? p : origin + qRot(qYaw(yaw), p) + vec3(0.f, TABLE, 0.f);
        d.q = group ? q : qMul(qYaw(yaw), q);
        d.group = group;
        d.h = p.y;
        v.push_back(d);
    }
    // bottom-up (stable: planks at one height keep their authored order)
    void sort() {
        std::stable_sort(v.begin(), v.end(), [](const PDef& a, const PDef& b) { return a.h < b.h - 0.05f; });
    }
};

// Log-cabin courses: even courses two rows of nx planks on edge along x (front and back faces), odd courses nx + 1
// planks on edge along z crossing them at every joint. From x0 (the left end), z0 (the middle), y0.
float logCabin(Build& b, float x0, float z0, int nx, int c0, int c1, float y0, int parity = 0) {
    float y = y0;
    for (int k = c0; k < c1; k++) {
        if ((k + parity) % 2 == 0) {
            for (int i = 0; i < nx; i++)
                for (int s = 0; s < 2; s++)
                    b.add(vec3(x0 + (i + 0.5f) * LC, y + WC * 0.5f, z0 + (s ? -1.f : 1.f) * (LC * 0.5f - TC * 0.5f)), EX);
        } else {
            for (int i = 0; i <= nx; i++) b.add(vec3(x0 + i * LC, y + WC * 0.5f, z0), EZ);
        }
        y += WC;
    }
    return y;
}

// A deck: planks laid flat across (along z), side by side, covering nx plank lengths.
float deck(Build& b, float x0, float z0, int count, float y) {
    for (int j = 0; j < count; j++) b.add(vec3(x0 + (j + 0.5f) * WC, y + TC * 0.5f, z0), FZ);
    return y + TC;
}

// A gable roof: rafters leaning from the front and back eaves to the ridge over z0, side by side along x, resting on
// the walls' top (yTop) at z0 +- halfD.
void gable(Build& b, float x0, float x1, float z0, float halfD, float yTop, float angle) {
    const float c = std::cos(angle), s = std::sin(angle), t = std::tan(angle);
    const float run = LC * c, rise = LC * s;
    const int n = std::max(1, (int)std::floor((x1 - x0) / WC + 0.5f));
    const float xs = 0.5f * (x0 + x1) - n * WC * 0.5f;
    // the underside's height at the ridge: on the walls' top at z0 +- halfD
    const float yRidge = yTop + halfD * t;
    for (int side = 0; side < 2; side++) {
        const float sg = side ? -1.f : 1.f;
        const vec3 len(0.f, s, -c * sg);
        const vec3 wid(-sg, 0.f, 0.f);
        const vec3 thk = cross(len, wid);
        const Q q = qBasis(len, wid, thk);
        for (int j = 0; j < n; j++) {
            const float x = xs + (j + 0.5f) * WC;
            // the rafter's top end at the ridge, its foot out at the eave
            const vec3 top(x, yRidge, z0);
            const vec3 mid = top - len * (LC * 0.5f) + thk * (TC * 0.5f);
            b.add(mid, q);
        }
    }
    (void)run;
    (void)rise;
}

// ---- The left front: a tall tower with floors and a gable roof.
Build tower(const vec3& origin = vec3(-20.7f, 0.f, -12.6f), float yaw = 0.18f, int sections = 3) {
    Build b;
    b.origin = origin;
    b.yaw = yaw;
    float y = 0.f;
    for (int sec = 0; sec < sections; sec++) {
        y = logCabin(b, -LC * 0.5f, 0.f, 1, 0, 8, y);
        if (sec < sections - 1) y = deck(b, -LC * 0.5f, 0.f, 5, y);
    }
    // the roof's tie: one more course on edge along x, then the rafters
    gable(b, -LC * 0.5f - 0.5f, LC * 0.5f + 0.5f, 0.f, LC * 0.5f, y, 0.62f);
    b.sort();
    return b;
}

// ---- The right front: a temple, a core tower between two porches.
Build temple() {
    Build b;
    b.origin = vec3(21.5f, 0.f, -13.f);
    b.yaw = -0.2f;
    const float h = LC * 0.5f;
    float y = logCabin(b, -h, 0.f, 1, 0, 16, 0.f);
    // the stepped top: a deck, then pairs of flat planks stepping in, and a ridge on edge
    float yt = deck(b, -h, 0.f, 5, y);
    for (int k = 0; k < 4; k++) {
        const float z = h - WC * 0.5f - k * WC * 0.5f;
        b.add(vec3(0.f, yt + TC * 0.5f, z), FX);
        b.add(vec3(0.f, yt + TC * 0.5f, -z), FX);
        yt += TC;
    }
    b.add(vec3(0.f, yt + WC * 0.5f, 0.f), EX);
    // the porches
    const float ang = 0.38f, ca = std::cos(ang), sa = std::sin(ang);
    for (int side = 0; side < 2; side++) {
        const float sg = side ? -1.f : 1.f;
        // two feet on edge, a deck on them
        for (int s = 0; s < 2; s++) b.add(vec3(sg * (h + LC * 0.5f), WC * 0.5f, (s ? -1.f : 1.f) * (h - WC)), EX);
        for (int j = 0; j < 5; j++) b.add(vec3(sg * (h + (j + 0.5f) * WC), WC + TC * 0.5f, 0.f), FZ);
        // columns at the outer corners, a beam on them
        const float xc = sg * (h + LC - WC * 0.5f - 0.05f);
        const float yd = WC + TC;
        for (int s = 0; s < 2; s++) b.add(vec3(xc, yd + LC * 0.5f, (s ? -1.f : 1.f) * (h - TC)), UP);
        const float yb = yd + LC;
        b.add(vec3(xc, yb + WC * 0.5f, 0.f), EZ);
        // the lean-to roof: rafters from the beam up to the core
        const vec3 len(-sg * ca, sa, 0.f), wid(0.f, 0.f, sg);
        const vec3 thk = cross(len, wid);
        const Q q = qBasis(len, wid, thk);
        const vec3 foot(xc + sg * 0.12f, yb + WC - 0.12f * sa / ca, 0.f);
        for (int j = 0; j < 6; j++) {
            const float z = -3.f * WC + (j + 0.5f) * WC;
            b.add(foot + len * (LC * 0.5f) + thk * (TC * 0.5f) + vec3(0.f, 0.f, z), q);
        }
    }
    b.sort();
    return b;
}

// ---- The track.
// (close behind the well: under the HUD's columns it stays low on screen, below their lowest line, and it ends
// before the corner labels)
const float TRACK_PTS[][2] = {{-16.5f, -4.4f}, {-12.f, -3.4f}, {-6.f, -2.85f}, {0.f, -2.7f},
                              {6.f, -2.85f},   {12.f, -3.4f},  {16.5f, -4.4f}};
constexpr int TRACK_N = (int)(sizeof(TRACK_PTS) / sizeof(TRACK_PTS[0]));
struct TrackTable {
    std::vector<vec3> p;   // samples
    std::vector<float> s;  // their arc length
    TrackTable() {
        auto cr = [&](int i) {
            i = std::clamp(i, 0, TRACK_N - 1);
            return vec3(TRACK_PTS[i][0], TABLE, TRACK_PTS[i][1]);
        };
        float acc = 0.f;
        vec3 prev = cr(0);
        for (int i = 0; i < TRACK_N - 1; i++)
            for (int k = 0; k < 32; k++) {
                const float t = k / 32.f;
                const vec3 p0 = cr(i - 1), p1 = cr(i), p2 = cr(i + 1), p3 = cr(i + 2);
                const float t2 = t * t, t3 = t2 * t;
                const vec3 q = (p1 * 2.f + (p2 - p0) * t + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2 +
                                (p1 * 3.f - p0 - p2 * 3.f + p3) * t3) * 0.5f;
                acc += length(q - prev);
                prev = q;
                p.push_back(q);
                s.push_back(acc);
            }
        p.push_back(cr(TRACK_N - 1));
        acc += length(p.back() - prev);
        s.push_back(acc);
        s[0] = 0.f;
    }
};
const TrackTable& track() {
    static const TrackTable t;
    return t;
}

// The rails and sleepers between arc lengths s0 and s1 (the left half is laid from the middle outward).
Build rails(float s0, float s1, bool backward) {
    Build b;
    const float gauge = 0.31f * LC;
    std::vector<float> marks;
    const int n = (int)std::floor((s1 - s0) / LC + 0.5f);
    for (int i = 0; i <= n; i++) marks.push_back(s0 + (s1 - s0) * i / std::max(1, n));
    if (backward) std::reverse(marks.begin(), marks.end());
    for (size_t i = 0; i + 1 < marks.size(); i++) {
        const vec3 a = trackPoint(marks[i], nullptr), c = trackPoint(marks[i + 1], nullptr);
        const vec3 dir = normalize(c - a);
        const vec3 nrm(-dir.z, 0.f, dir.x);
        const float yaw = std::atan2(-dir.z, dir.x);
        // a sleeper across at the chord's start, the next one in its middle, then the two rails along it
        for (int k = 0; k < 2; k++) {
            const vec3 sp = lerp(a, c, 0.25f + 0.5f * k);
            b.add(sp - vec3(0.f, TABLE, 0.f) + vec3(0.f, TC * 0.5f, 0.f), qMul(qYaw(yaw + PI * 0.5f), FX));
        }
        const vec3 m = (a + c) * 0.5f - vec3(0.f, TABLE, 0.f);
        for (int s = 0; s < 2; s++)
            b.add(m + nrm * ((s ? -1.f : 1.f) * gauge) + vec3(0.f, TC * 1.5f, 0.f), qMul(qYaw(yaw), FX));
    }
    return b; // (laid along the track, not sorted by height)
}

// ---- The train, in its cars' frames (x forward along the track, y up from the rails' top, z to its left).
Build loco() {
    Build b;
    b.group = 1;
    // the chassis: two planks on edge along, a buffer beam across at each end, a deck of planks across
    for (int s = 0; s < 2; s++) b.add(vec3(0.f, WC * 0.5f, (s ? -1.f : 1.f) * 0.3f * LC), EX);
    for (int s = 0; s < 2; s++) b.add(vec3((s ? -1.f : 1.f) * (LC * 0.5f + TC * 0.5f), WC * 0.5f, 0.f), EZ);
    float y = WC;
    for (int j = 0; j < 5; j++) b.add(vec3(-LC * 0.5f + (j + 0.5f) * WC, y + TC * 0.5f, 0.f), FZ);
    y += TC;
    // the boiler in front: a narrow block of planks laid flat along it, stacked, the smokebox on edge across its nose
    for (int k = 0; k < 4; k++)
        for (int j = 0; j < 2; j++) b.add(vec3(0.12f * LC, y + (k + 0.5f) * TC, (j ? -0.5f : 0.5f) * WC), FX);
    b.add(vec3(0.5f * LC - TC * 0.5f, y + WC * 0.5f, 0.f), qMul(qYaw(PI * 0.5f), qAxis(vec3(0, 0, 1), 0.f)));
    // the cab at the back: four columns, a roof of planks across overhanging it
    for (int s = 0; s < 4; s++)
        b.add(vec3((s & 1) ? -0.46f * LC : -0.12f * LC, y + LC * 0.5f * 0.8f, (s & 2) ? -0.36f * LC : 0.36f * LC), UPZ);
    const float yr = y + LC * 0.8f;
    for (int j = 0; j < 4; j++) b.add(vec3(-0.56f * LC + (j + 0.5f) * WC, yr + TC * 0.5f, 0.f), FZ);
    // the chimney standing on the boiler's nose
    b.add(vec3(0.36f * LC, y + 4.f * TC + LC * 0.3f, 0.f), UP);
    b.sort();
    return b;
}
Build carriage() {
    Build b;
    b.group = 2;
    for (int s = 0; s < 2; s++) b.add(vec3(0.f, WC * 0.5f, (s ? -1.f : 1.f) * 0.3f * LC), EX);
    for (int s = 0; s < 2; s++) b.add(vec3((s ? -1.f : 1.f) * (LC * 0.5f + TC * 0.5f), WC * 0.5f, 0.f), EZ);
    float y = WC;
    for (int j = 0; j < 5; j++) b.add(vec3(-LC * 0.5f + (j + 0.5f) * WC, y + TC * 0.5f, 0.f), FZ);
    y += TC;
    // a low wall on edge each side, three columns each side, a roof of planks across overhanging both ends
    for (int s = 0; s < 2; s++) b.add(vec3(0.f, y + WC * 0.5f, (s ? -1.f : 1.f) * (0.42f * LC)), EX);
    for (int s = 0; s < 6; s++)
        b.add(vec3((-0.42f + (s % 3) * 0.42f) * LC, y + LC * 0.4f, (s < 3 ? 1.f : -1.f) * 0.36f * LC), UPZ);
    const float yr = y + LC * 0.8f;
    for (int j = 0; j < 6; j++) b.add(vec3(-3.f * WC + (j + 0.5f) * WC, yr + TC * 0.5f, 0.f), FZ);
    b.sort();
    return b;
}

// ---- The left back: a pagoda: a wide log-cabin base, lean-to skirts on its sides, a core tower, a gable roof and a
// spire.
Build pagoda(const vec3& origin = vec3(-41.f, 0.f, -40.f), float yaw = 0.1f) {
    Build b;
    b.origin = origin;
    b.yaw = yaw;
    const float h = LC * 0.5f;
    float y = logCabin(b, -LC, 0.f, 2, 0, 6, 0.f);
    y = deck(b, -LC, 0.f, 10, y);
    // the skirts: rafters from the eaves up to the core, left and right
    const float ang = 0.62f, ca = std::cos(ang), sa = std::sin(ang);
    for (int side = 0; side < 2; side++) {
        const float sg = side ? -1.f : 1.f;
        const vec3 len(-sg * ca, sa, 0.f), wid(0.f, 0.f, sg);
        const vec3 thk = cross(len, wid);
        const Q q = qBasis(len, wid, thk);
        const vec3 foot(sg * (LC + 0.45f), y - 0.45f * sa / ca, 0.f);
        for (int j = 0; j < 5; j++)
            b.add(foot + len * (LC * 0.5f) + thk * (TC * 0.5f) + vec3(0.f, 0.f, -2.5f * WC + (j + 0.5f) * WC), q);
    }
    y = logCabin(b, -h, 0.f, 1, 0, 9, y, 1);
    y = deck(b, -h, 0.f, 5, y);
    gable(b, -h - 0.4f, h + 0.4f, 0.f, h, y, 0.66f);
    b.add(vec3(0.f, y + h * std::tan(0.66f) + LC * 0.5f - 0.3f, 0.f), UP);
    b.sort();
    return b;
}

// ---- The right back: a long house, a gable roof.
Build house(const vec3& origin = vec3(41.f, 0.f, -40.f), float yaw = -0.15f, int courses = 11) {
    Build b;
    b.origin = origin;
    b.yaw = yaw;
    const int nx = 2;
    const float x0 = -nx * LC * 0.5f;
    const float y = logCabin(b, x0, 0.f, nx, 0, courses, 0.f);
    gable(b, x0 - 0.4f, -x0 + 0.4f, 0.f, LC * 0.5f, y, 0.6f);
    b.sort();
    return b;
}

} // namespace

float trackLength() { return track().s.back(); }

vec3 trackPoint(float s, vec3* tangent) {
    const TrackTable& t = track();
    s = std::clamp(s, 0.f, t.s.back());
    const size_t i = std::min(t.s.size() - 2, (size_t)(std::lower_bound(t.s.begin(), t.s.end(), s) - t.s.begin()) - (s > 0.f ? 1 : 0));
    const float k = saturate((s - t.s[i]) / std::max(1e-5f, t.s[i + 1] - t.s[i]));
    if (tangent) *tangent = normalize(t.p[i + 1] - t.p[i]);
    return lerp(t.p[i], t.p[i + 1], k);
}

float trainStation() { return trackLength() * 0.5f + 10.5f; }

std::vector<PDef> program(int side) {
    std::vector<PDef> out;
    auto append = [&](const Build& b) { out.insert(out.end(), b.v.begin(), b.v.end()); };
    const float half = trackLength() * 0.5f;
    if (side == 0) {
        append(tower());
        append(rails(0.f, half, true));
        append(pagoda());
    } else {
        append(temple());
        append(rails(half, trackLength(), false));
        append(loco());
        append(carriage());
        append(house());
    }
    return out;
}

// Beyond the program: more builds farther and farther back on each side, a village receding into the studio's haze
// (a house, a tower, a pagoda, in turn), each where its side's margin is, left and right of the HUD's columns.
std::vector<PDef> extra(int side, int n) {
    const float z = -56.f - 15.f * n;
    const float s = 34.6f / (34.6f - z);                     // its scale on screen (the reference camera)
    const float xs = (n % 2 == 0) ? 14.6f : 19.2f;          // where it stands on screen
    const vec3 o((side ? 1.f : -1.f) * xs / s, 0.f, z);
    const float yaw = (side ? -1.f : 1.f) * (0.1f + 0.15f * (n % 3));
    switch (n % 3) {
    case 0: return house(o, yaw, 7 + (n % 2) * 2).v;
    case 1: return tower(o, yaw, 2).v;
    default: return pagoda(o, yaw).v;
    }
}

} // namespace planks_build
