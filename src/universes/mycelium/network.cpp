// MYCELIUM's network: a fungal network grown once per game from a seed, in log space (every step a fixed fraction of
// its distance from the spore, so it has the same density of threads at every scale it reaches: a millimeter around
// the spore, a web around the Earth at the end). Tips wander outward and fan out, branch, fuse into the threads they
// meet (anastomoses, bright knots) and die out where it is crowded; fine hairs fuzz the frontier. It grows on a plane
// of geodesic polar coordinates around the spore, mapped onto the planet (myc::toWorld). Each thread carries the
// growth at which it is born, so the shaders reveal it against the growth.
#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "universes/mycelium/mycelium.hpp"

namespace {

// The journey: lines -> the network's reach (meters), interpolated in log space.
// The pacing lives here alone (to rescale it, scale the lines column). The whole journey fits in some 50 lines (one
// song): something new every five lines or so, the first millimeters a little quicker (some 2x farther a line), then
// about 1.6x a line, the last lines slowly wrapping the globe.
struct Stage { float lines, reach; };
const Stage STAGES[] = {
    {0, 0.002f}, {1.2f, 0.005f}, {2.6f, 0.014f}, {4.2f, 0.045f}, {6, 0.18f}, {8, 0.9f}, {10.3f, 4.5f}, {13, 22.f},
    {16, 110.f}, {19.5f, 550.f}, {22.5f, 2800.f}, {26, 1.4e4f}, {29.5f, 7.5e4f}, {34, 4.e5f}, {38, 2.e6f},
    {43, 6.5e6f}, {46, 1.2e7f}, {48.5f, 1.45e7f}, {50, 1.6e7f},
};
constexpr int STAGE_COUNT = (int)(sizeof(STAGES) / sizeof(STAGES[0]));

float gauss(Rng& r) { return (r.uniform() + r.uniform() + r.uniform() - 1.5f) * 1.41f; }
float wrapAng(float a) { return std::remainder(a, TAU); }

} // namespace

float myc::growthOf(int lines) {
    const float l = (float)std::max(0, lines);
    float g;
    if (l >= STAGES[STAGE_COUNT - 1].lines) {
        g = std::log(STAGES[STAGE_COUNT - 1].reach) + (l - STAGES[STAGE_COUNT - 1].lines) * 0.004f;
    } else {
        int i = 0;
        while (i + 2 < STAGE_COUNT && STAGES[i + 1].lines <= l) i++;
        const float t = (l - STAGES[i].lines) / (STAGES[i + 1].lines - STAGES[i].lines);
        g = lerpf(std::log(STAGES[i].reach), std::log(STAGES[i + 1].reach), t);
    }
    return std::min(g, G_MAX);
}

MycNet buildMycNet(uint64_t seed) {
    MycNet net;
    Rng r(seed ^ 0xF00DF00Dull);
    const float RMAX = std::exp(myc::G_MAX) * 1.02f;
    const float SPORE_R = 0.00012f;
    const float LN_CELL = 0.12f, A_CELL = 0.06f;
    auto keyOf = [](int i, int j) { return ((int64_t)i << 32) ^ (int64_t)(uint32_t)j; };
    auto cellKey = [&](float x, float z) {
        const float rho = std::max(std::sqrt(x * x + z * z), 1e-5f);
        return keyOf((int)std::floor(std::log(rho) / LN_CELL), (int)std::floor(std::atan2(z, x) / A_CELL));
    };
    std::unordered_map<int64_t, std::vector<int>> grid;
    struct S { float ax, az, bx, bz; int lineage; };
    std::vector<S> segs;

    struct Tip {
        float x, z, ang, w, path, lag, g, curl;
        int lineage, parent, steps;
    };
    std::vector<Tip> tips;
    int lineages = 0;
    const int tubes = 4;
    const float a0 = r.range(0.f, TAU);
    for (int i = 0; i < tubes; i++) {
        const float a = a0 + i * TAU / tubes + r.range(-0.35f, 0.35f);
        Tip t;
        t.x = std::cos(a) * SPORE_R;
        t.z = std::sin(a) * SPORE_R;
        t.ang = a;
        t.w = 1.f;
        t.path = 0.f;
        t.lag = r.range(0.f, 0.12f);
        t.g = std::log(SPORE_R) + t.lag;
        t.curl = r.range(-0.06f, 0.06f);
        t.lineage = lineages++;
        t.parent = -1;
        t.steps = 0;
        tips.push_back(t);
    }
    std::vector<float> lineageHash;
    for (int i = 0; i < 4096; i++) lineageHash.push_back(r.uniform());

    struct Raw { float ax, az, bx, bz, w, g0, g1, kind, p0, p1, hash, cls; };
    std::vector<Raw> raw;
    raw.reserve(170000);
    int guard = 0;
    const float STEP = 0.06f;
    while (!tips.empty() && guard++ < 300000 && raw.size() < 170000) {
        // the tip nearest the spore (in growth) steps first: the frontier advances evenly
        int ti = 0;
        float best = 1e30f;
        for (int i = 0; i < (int)tips.size(); i++)
            if (tips[i].g < best) { best = tips[i].g; ti = i; }
        Tip t = tips[ti];
        const float rho = std::max(std::sqrt(t.x * t.x + t.z * t.z), SPORE_R);
        if (rho > RMAX) { tips.erase(tips.begin() + ti); continue; }
        // steer outward, wandering (more freely at the planet's scale: a web rather than a star)
        const float outward = std::atan2(t.z, t.x);
        const float kW = smoothstepf(std::log(1.5e5f), std::log(3.e6f), std::log(rho));
        const float cl = lerpf(0.06f, 0.14f, kW);
        t.curl = std::clamp(t.curl + gauss(r) * 0.02f, -cl, cl);
        t.ang += wrapAng(outward - t.ang) * lerpf(0.045f, 0.02f, kW) + t.curl + gauss(r) * 0.13f;
        const float len = STEP * rho * r.range(0.8f, 1.25f);
        float nx = t.x + std::cos(t.ang) * len, nz = t.z + std::sin(t.ang) * len;
        const float rhoN = std::max(std::sqrt(nx * nx + nz * nz), SPORE_R);
        // a fusion: the tip meets a thread of another lineage
        int hitSeg = -1;
        float hitD = 0.03f * rhoN, hx = 0.f, hz = 0.f;
        {
            const int ci = (int)std::floor(std::log(rhoN) / LN_CELL);
            const int cj = (int)std::floor(std::atan2(nz, nx) / A_CELL);
            for (int di = -1; di <= 1; di++)
                for (int dj = -1; dj <= 1; dj++) {
                    auto it = grid.find(keyOf(ci + di, cj + dj));
                    if (it == grid.end()) continue;
                    for (int si : it->second) {
                        const S& s = segs[si];
                        if (s.lineage == t.lineage || (t.steps < 4 && s.lineage == t.parent)) continue;
                        const float ex = s.bx - s.ax, ez = s.bz - s.az;
                        const float ll = ex * ex + ez * ez;
                        float u = ll > 0.f ? ((nx - s.ax) * ex + (nz - s.az) * ez) / ll : 0.f;
                        u = std::clamp(u, 0.f, 1.f);
                        const float px = s.ax + ex * u, pz = s.az + ez * u;
                        const float d = std::sqrt((nx - px) * (nx - px) + (nz - pz) * (nz - pz));
                        if (d < hitD) { hitD = d; hitSeg = si; hx = px; hz = pz; }
                    }
                }
        }
        const bool fuse = hitSeg >= 0 && t.steps > 3 && r.chance(0.3f);
        if (fuse) { nx = hx; nz = hz; }
        const float stepLen = std::sqrt((nx - t.x) * (nx - t.x) + (nz - t.z) * (nz - t.z));
        const float rhoMid = std::max(std::sqrt((nx + t.x) * (nx + t.x) + (nz + t.z) * (nz + t.z)) * 0.5f, SPORE_R);
        const float path1 = t.path + stepLen / rhoMid;
        const float g1 = std::max(t.g + 0.004f, std::log(std::max(std::sqrt(nx * nx + nz * nz), SPORE_R)) + t.lag);
        const float wWorld = t.w * 0.0055f * rhoMid * lerpf(1.f, 0.5f, kW);
        const int kind = fuse ? 3 : (t.w >= 0.6f ? 0 : 1);
        const float hsh = lineageHash[t.lineage % 4096];
        raw.push_back({t.x, t.z, nx, nz, wWorld, t.g, g1, (float)kind, t.path, path1, hsh, t.w});
        const int si = (int)segs.size();
        segs.push_back({t.x, t.z, nx, nz, t.lineage});
        grid[cellKey(nx, nz)].push_back(si);
        grid[cellKey((nx + t.x) * 0.5f, (nz + t.z) * 0.5f)].push_back(si);
        if (r.chance(0.3f)) net.sites.push_back({nx, nz, g1, path1});
        if (fuse) {
            raw.push_back({nx, nz, nx, nz, wWorld * 1.7f, g1, g1 + 0.05f, 4.f, path1, path1, hsh, 1.f});
            tips.erase(tips.begin() + ti);
            continue;
        }
        // fine hairs off the thread (they fuzz the frontier)
        for (int h = 0; h < 2; h++) {
            if (!r.chance(h == 0 ? 0.22f : 0.06f)) continue;
            const float u = r.range(0.2f, 0.9f);
            const float sx = t.x + (nx - t.x) * u, sz = t.z + (nz - t.z) * u;
            const float a = t.ang + (r.chance(0.5f) ? 1.f : -1.f) * r.range(0.5f, 1.3f);
            const float hl = stepLen * r.range(0.25f, 0.6f);
            const float ex = sx + std::cos(a) * hl, ez = sz + std::sin(a) * hl;
            const float hg0 = t.g + (g1 - t.g) * u + r.range(0.03f, 0.25f);
            const float pp = t.path + (path1 - t.path) * u;
            raw.push_back({sx, sz, ex, ez, wWorld * 0.35f, hg0, hg0 + 0.08f, 2.f, pp, pp + hl / rhoMid, hsh, 0.2f});
        }
        // crowding: the tip dies where the threads are dense
        int crowd = 0;
        {
            auto it = grid.find(cellKey(nx, nz));
            if (it != grid.end()) crowd = (int)it->second.size();
        }
        Tip nt = t;
        nt.x = nx; nt.z = nz; nt.path = path1; nt.g = g1; nt.steps++;
        tips[ti] = nt;
        if ((crowd > 14 && r.chance(0.3f)) || r.chance(0.001f)) {
            tips.erase(tips.begin() + ti);
            continue;
        }
        // branching
        const int maxTips = (int)lerpf(130.f, 260.f, smoothstepf(std::log(1.e5f), std::log(2.e6f), std::log(rho)));
        const float pb = tips.size() < 24 ? 0.4f : (int)tips.size() < maxTips ? 0.12f : 0.f;
        if (r.chance(pb)) {
            Tip c = nt;
            const float side = r.chance(0.5f) ? 1.f : -1.f;
            c.ang = nt.ang + side * r.range(0.45f, 0.95f);
            tips[ti].ang -= side * r.range(0.05f, 0.25f);
            c.w = std::max(0.3f, nt.w * 0.7f);
            tips[ti].w = std::max(0.3f, nt.w * 0.97f);
            c.lag = std::clamp(nt.lag + r.range(-0.03f, 0.1f), -0.15f, 0.35f);
            c.parent = nt.lineage;
            c.curl = -nt.curl * 0.5f + r.range(-0.04f, 0.04f);
            c.lineage = lineages++;
            c.steps = 0;
            tips.push_back(c);
            if (r.chance(0.5f)) raw.push_back({nx, nz, nx, nz, wWorld * 1.2f, g1, g1 + 0.05f, 4.f, path1, path1, lineageHash[c.lineage % 4096], 0.5f});
        }
    }
    // the pulse coordinate: the path's length (in local distances) fitted to ln(distance), so a pulse keeps to the
    // scale on screen
    const float lnRho0 = std::log(SPORE_R);
    double num = 0.0, den = 0.0;
    for (const Raw& s : raw) {
        if (s.kind >= 2.f) continue;
        const double l = std::log(std::max(std::sqrt(s.bx * s.bx + s.bz * s.bz), SPORE_R)) - lnRho0;
        num += s.p1 * l;
        den += l * l;
    }
    const float a = den > 0.0 ? (float)(num / den) : 1.f;
    auto qOf = [&](float p) { return lnRho0 + p / std::max(a, 0.1f); };
    net.seg.reserve(raw.size() * 16);
    for (const Raw& s : raw) {
        // on the sphere: lifted a little over the ground (and by the chord's sag, so a long thread never dips under it)
        const double L = std::sqrt((double)(s.bx - s.ax) * (s.bx - s.ax) + (double)(s.bz - s.az) * (s.bz - s.az));
        const double lift = 0.7 * s.w + L * L / (8.0 * myc::R_EARTH);
        const vec3 A = myc::toWorld(s.ax, s.az, lift), B = myc::toWorld(s.bx, s.bz, lift);
        const float lnr = std::log(std::max(std::sqrt(s.bx * s.bx + s.bz * s.bz), SPORE_R));
        const float v[16] = {A.x, A.y, A.z, s.w, B.x, B.y, B.z, s.kind, s.g0, s.g1, qOf(s.p0), qOf(s.p1), s.hash, s.cls, lnr, 0.f};
        net.seg.insert(net.seg.end(), v, v + 16);
    }
    for (MycNet::Site& s : net.sites) s.q = qOf(s.q);
    return net;
}
