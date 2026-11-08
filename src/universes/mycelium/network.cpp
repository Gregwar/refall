// MYCELIUM's network: a fungal network grown once per game from a seed, in log space (every step a fixed fraction of
// its distance from the spore, so it has the same density of threads at every scale it reaches: a millimeter
// around the spore, a kilometer at the end). Tips wander outward and fan out, branch, fuse into the threads they
// meet (anastomoses) and die out where it is crowded; along the surface they leave the sites where mushrooms come
// up. Each thread carries the growth at which it is born, so the shaders reveal it against the growth.
// The roots in the cut are generated here too: clusters at every scale (a grass tuft's rootlets to a tree's).
#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "universes/mycelium/mycelium.hpp"

namespace {

struct Stage { float lines, reach; };
const Stage STAGES[] = {
    {0, 0.0013f}, {1, 0.0035f}, {2, 0.007f}, {4, 0.018f}, {7, 0.05f}, {10, 0.12f}, {14, 0.33f}, {19, 1.f},
    {25, 2.8f}, {32, 8.f}, {40, 21.f}, {48, 48.f}, {57, 95.f}, {68, 170.f}, {82, 300.f}, {100, 520.f}, {125, 900.f},
};
constexpr int STAGE_COUNT = (int)(sizeof(STAGES) / sizeof(STAGES[0]));

float gauss(Rng& r) { return (r.uniform() + r.uniform() + r.uniform() - 1.5f) * 1.41f; }
float wrapAng(float a) { return std::remainder(a, TAU); }

} // namespace

float myc::growthOf(int lines) {
    const float l = (float)std::max(0, lines);
    float g;
    if (l >= STAGES[STAGE_COUNT - 1].lines) {
        g = std::log(STAGES[STAGE_COUNT - 1].reach) + (l - STAGES[STAGE_COUNT - 1].lines) * 0.018f;
    } else {
        int i = 0;
        while (i + 2 < STAGE_COUNT && STAGES[i + 1].lines <= l) i++;
        const float t = (l - STAGES[i].lines) / (STAGES[i + 1].lines - STAGES[i].lines);
        g = lerpf(std::log(STAGES[i].reach), std::log(STAGES[i + 1].reach), t);
    }
    return std::min(g, G_MAX);
}

// The framing: close on the spore at the right of the well at first (the soil filling the lower part of the
// screen), then pulling back and drifting to the middle as the network spreads and the mushrooms tower up, the
// cut's edge lower in the frame.
myc::View myc::frame(float G, float aspect, float bh) {
    (void)aspect;
    const float R = std::exp(G);
    const float k = smoothstepf(std::log(0.0013f), std::log(30.f), G);
    const float kx = smoothstepf(std::log(0.004f), std::log(4.f), G);
    View v;
    v.vh = R * lerpf(1.8f, 0.52f, k);
    const float gby = lerpf(-3.4f, -4.6f, smoothstepf(std::log(0.01f), std::log(40.f), G));
    const float obx = lerpf(14.2f, 2.5f, kx);
    v.cx = -obx * v.vh / bh;
    v.cy = -gby * v.vh / bh;
    v.ex = v.cx + 0.1f * v.vh;
    v.ey = v.cy + lerpf(0.12f, 0.2f, k) * v.vh;
    v.D = 3.2f * v.vh;
    return v;
}

MycNet buildMycNet(uint64_t seed) {
    MycNet net;
    Rng r(seed ^ 0xF00DF00Dull);
    const float y0 = myc::SPORE_Y;
    const float K = myc::DEPTH_K;
    const float RMAX = std::exp(myc::G_MAX) * 1.15f;
    auto rhoOf = [&](float x, float y) { return std::sqrt(x * x + ((y - y0) / K) * ((y - y0) / K)); };
    auto cellKey = [&](float x, float y) {
        const float rho = std::max(rhoOf(x, y), 1e-4f);
        const float a = std::atan2((y - y0) / K, x);
        const int i = (int)std::floor(std::log(rho) / 0.08f);
        const int j = (int)std::floor(a / 0.055f);
        return ((int64_t)i << 32) ^ (int64_t)(uint32_t)j;
    };
    auto keyOf = [](int i, int j) { return ((int64_t)i << 32) ^ (int64_t)(uint32_t)j; };
    std::unordered_map<int64_t, std::vector<int>> grid;
    struct S { float ax, ay, bx, by; int lineage; };
    std::vector<S> segs;
    std::vector<float> segPath0, segPath1;

    struct Tip {
        float x, y, ang, w, path, lag, g;
        int lineage, parent, steps;
    };
    std::vector<Tip> tips;
    int lineages = 0;
    const float SPORE_R = 0.00008f;
    // the spore and its germ tubes
    {
        const float n[8] = {0.f, y0, SPORE_R, std::log(SPORE_R) - 0.6f, 0.f, 0.f, 2.f, 0.f};
        net.nodes.insert(net.nodes.end(), n, n + 8);
        const int tubes = 3;
        for (int i = 0; i < tubes; i++) {
            const float a = -PI * 0.5f + (i - 1) * 1.15f + r.range(-0.25f, 0.25f);
            Tip t;
            t.x = std::cos(a) * SPORE_R;
            t.y = y0 + std::sin(a) * SPORE_R;
            t.ang = a;
            t.w = 1.f;
            t.path = 0.f;
            t.lag = r.range(0.f, 0.12f);
            t.g = std::log(SPORE_R) + t.lag;
            t.lineage = lineages++;
            t.parent = -1;
            t.steps = 0;
            tips.push_back(t);
        }
    }
    std::vector<float> lineageHash;
    for (int i = 0; i < 4096; i++) lineageHash.push_back(r.uniform());

    const float lnRho0 = std::log(SPORE_R);
    struct Raw { float ax, ay, bx, by, w, g0, g1, kind, p0, p1, hash, cls; };
    std::vector<Raw> raw;
    raw.reserve(40000);
    int guard = 0;
    while (!tips.empty() && guard++ < 160000 && raw.size() < 90000) {
        // the tip nearest the spore steps first (the frontier advances evenly)
        int ti = 0;
        float best = 1e30f;
        for (int i = 0; i < (int)tips.size(); i++) {
            const float g = tips[i].g;
            if (g < best) { best = g; ti = i; }
        }
        Tip t = tips[ti];
        const float rho = std::max(rhoOf(t.x, t.y), SPORE_R);
        if (rho > RMAX) { tips.erase(tips.begin() + ti); continue; }
        // steer: outward (across the ellipse), wandering, away from the surface
        const vec3 outw = normalize(vec3(t.x, (t.y - y0) / (K * K), 0.f));
        const float target = std::atan2(outw.y, outw.x);
        t.ang += wrapAng(target - t.ang) * 0.055f + gauss(r) * 0.25f;
        const float len = 0.05f * rho * r.range(0.8f, 1.25f);
        float nx = t.x + std::cos(t.ang) * len, ny = t.y + std::sin(t.ang) * len;
        const float rhoN = std::max(rhoOf(nx, ny), SPORE_R);
        const float top = myc::surf(nx) - 0.008f * rhoN - 0.00012f;
        bool surface = false;
        if (ny > top - 0.02f * rhoN) {
            surface = true;
            // along the surface, a little downward
            const float along = std::cos(t.ang) >= 0.f ? -0.35f : PI + 0.35f;
            if (std::sin(t.ang) > -0.3f) t.ang += wrapAng(along - t.ang) * 0.7f;
            ny = std::min(ny, top);
        }
        // a fusion: the tip meets a thread of another lineage
        int hitSeg = -1;
        float hitD = 0.022f * rhoN, hx = 0.f, hy = 0.f;
        {
            const float a = std::atan2((ny - y0) / K, nx);
            const int ci = (int)std::floor(std::log(std::max(rhoN, 1e-4f)) / 0.08f);
            const int cj = (int)std::floor(a / 0.055f);
            for (int di = -1; di <= 1; di++)
                for (int dj = -1; dj <= 1; dj++) {
                    auto it = grid.find(keyOf(ci + di, cj + dj));
                    if (it == grid.end()) continue;
                    for (int si : it->second) {
                        const S& s = segs[si];
                        if (s.lineage == t.lineage || (t.steps < 4 && s.lineage == t.parent)) continue;
                        const float ex = s.bx - s.ax, ey = s.by - s.ay;
                        const float ll = ex * ex + ey * ey;
                        float u = ll > 0.f ? ((nx - s.ax) * ex + (ny - s.ay) * ey) / ll : 0.f;
                        u = std::clamp(u, 0.f, 1.f);
                        const float px = s.ax + ex * u, py = s.ay + ey * u;
                        const float d = std::sqrt((nx - px) * (nx - px) + (ny - py) * (ny - py));
                        if (d < hitD) { hitD = d; hitSeg = si; hx = px; hy = py; }
                    }
                }
        }
        const bool fuse = hitSeg >= 0 && t.steps > 3 && r.chance(0.3f);
        if (fuse) { nx = hx; ny = hy; }
        const float stepLen = std::sqrt((nx - t.x) * (nx - t.x) + (ny - t.y) * (ny - t.y));
        const float rhoMid = std::max(rhoOf((nx + t.x) * 0.5f, (ny + t.y) * 0.5f), SPORE_R);
        const float path1 = t.path + stepLen / rhoMid;
        const float g1 = std::max(t.g + 0.004f, std::log(std::max(rhoOf(nx, ny), SPORE_R)) + t.lag);
        const float wWorld = t.w * 0.0045f * rhoMid;
        const int kind = fuse ? 3 : (t.w >= 0.6f ? 0 : 1);
        raw.push_back({t.x, t.y, nx, ny, wWorld, t.g, g1, (float)kind, t.path, path1,
                       lineageHash[t.lineage % 4096], t.w});
        const int si = (int)segs.size();
        segs.push_back({t.x, t.y, nx, ny, t.lineage});
        grid[cellKey(nx, ny)].push_back(si);
        grid[cellKey((nx + t.x) * 0.5f, (ny + t.y) * 0.5f)].push_back(si);
        if (surface && ny > myc::surf(nx) - 0.05f * rhoN) net.sites.push_back({nx, g1, path1});
        if (fuse) {
            const float n[8] = {nx, ny, wWorld * 2.2f, g1, path1, lineageHash[t.lineage % 4096], 1.f, 0.f};
            net.nodes.insert(net.nodes.end(), n, n + 8);
            tips.erase(tips.begin() + ti);
            continue;
        }
        // fine hairs off the thread (they fill the frontier with fuzz)
        for (int h = 0; h < 2; h++) {
            if (!r.chance(h == 0 ? 0.4f : 0.1f)) continue;
            const float u = r.range(0.2f, 0.9f);
            const float sx = t.x + (nx - t.x) * u, sy = t.y + (ny - t.y) * u;
            const float a = t.ang + (r.chance(0.5f) ? 1.f : -1.f) * r.range(0.5f, 1.3f);
            const float hl = stepLen * r.range(0.25f, 0.6f);
            float ex = sx + std::cos(a) * hl, ey = sy + std::sin(a) * hl;
            ey = std::min(ey, myc::surf(ex) - 0.004f * rhoMid);
            const float hg0 = t.g + (g1 - t.g) * u + r.range(0.03f, 0.25f);
            const float pp = t.path + (path1 - t.path) * u;
            raw.push_back({sx, sy, ex, ey, wWorld * 0.35f, hg0, hg0 + 0.08f, 2.f, pp, pp + hl / rhoMid,
                           lineageHash[t.lineage % 4096], 0.2f});
        }
        // crowding: the tip dies where the threads are dense
        int crowd = 0;
        {
            auto it = grid.find(cellKey(nx, ny));
            if (it != grid.end()) crowd = (int)it->second.size();
        }
        Tip nt = t;
        nt.x = nx; nt.y = ny; nt.path = path1; nt.g = g1; nt.steps++;
        tips[ti] = nt;
        if ((crowd > 9 && r.chance(0.4f)) || r.chance(0.002f)) {
            tips.erase(tips.begin() + ti);
            continue;
        }
        // branching
        const float pb = tips.size() < 24 ? 0.5f : tips.size() < 125 ? 0.15f : 0.f;
        if (r.chance(pb)) {
            Tip c = nt;
            const float side = r.chance(0.5f) ? 1.f : -1.f;
            c.ang = nt.ang + side * r.range(0.45f, 0.95f);
            tips[ti].ang -= side * r.range(0.05f, 0.25f);
            c.w = std::max(0.3f, nt.w * 0.7f);
            tips[ti].w = std::max(0.3f, nt.w * 0.97f);
            c.lag = std::clamp(nt.lag + r.range(-0.03f, 0.1f), -0.15f, 0.35f);
            c.parent = nt.lineage;
            c.lineage = lineages++;
            c.steps = 0;
            tips.push_back(c);
            const float n[8] = {nx, ny, wWorld * 1.5f, g1, path1, lineageHash[c.lineage % 4096], 0.f, 0.f};
            net.nodes.insert(net.nodes.end(), n, n + 8);
        }
    }
    // the pulse coordinate: the path's length (in local distances), fitted to ln(distance) so that pulses keep to
    // the visible scale
    double num = 0.0, den = 0.0;
    for (const Raw& s : raw) {
        if (s.kind == 2.f) continue;
        const double l = std::log(std::max(rhoOf(s.bx, s.by), SPORE_R)) - lnRho0;
        num += s.p1 * l;
        den += l * l;
    }
    const float a = den > 0.0 ? (float)(num / den) : 1.f;
    auto qOf = [&](float p) { return lnRho0 + p / std::max(a, 0.1f); };
    net.seg.reserve(raw.size() * 12);
    for (const Raw& s : raw) {
        const float v[12] = {s.ax, s.ay, s.bx, s.by, s.w, s.g0, s.g1, s.kind, qOf(s.p0), qOf(s.p1), s.hash, s.cls};
        net.seg.insert(net.seg.end(), v, v + 12);
    }
    for (size_t i = 0; i < net.nodes.size(); i += 8) net.nodes[i + 4] = qOf(net.nodes[i + 4]);
    for (MycNet::Site& s : net.sites) s.q = qOf(s.q);

    // ---- Roots: clusters at every scale, each under a plant on the cut's edge.
    Rng rr(seed ^ 0x5007ull);
    for (int k = 0; k <= 8; k++) {
        const float s = 0.003f * std::pow(3.f, (float)k);
        const float spacing = 2.4f * s;
        for (int i = -30; i <= 30; i++) {
            const float x = (i + rr.range(-0.35f, 0.35f)) * spacing;
            if (std::fabs(x) < (k >= 6 ? 1.3f : 0.7f) * s) continue;
            if (k >= 7 && rr.chance(0.35f)) continue;
            const float seedP = rr.uniform();
            net.plants.push_back({x, s, seedP, k});
            const float y = myc::surf(x);
            const int mains = rr.irange(3, k >= 7 ? 7 : 5);
            const float r0 = s * (k >= 7 ? 0.03f : 0.02f);
            for (int m = 0; m < mains; m++) {
                float ang = -PI * 0.5f + (m - (mains - 1) * 0.5f) / std::max(1.f, (mains - 1) * 0.5f) * rr.range(0.7f, 1.2f);
                float px = x + rr.range(-0.05f, 0.05f) * s, py = y;
                const float total = s * rr.range(0.45f, 1.15f) * (std::fabs(ang + PI * 0.5f) > 0.8f ? 0.8f : 1.f);
                const int n = 7;
                float rad = r0 * rr.range(0.6f, 1.15f);
                const float shade = rr.range(0.7f, 1.2f);
                for (int j = 0; j < n; j++) {
                    ang += gauss(rr) * 0.22f + wrapAng(-PI * 0.5f - ang) * 0.08f;
                    const float l = total / n;
                    float qx = px + std::cos(ang) * l, qy = py + std::sin(ang) * l;
                    qy = std::min(qy, myc::surf(qx) - 0.02f * s);
                    const float rad1 = rad * 0.8f;
                    const float v[8] = {px, py, qx, qy, rad, rad1, shade, (float)k};
                    net.roots.insert(net.roots.end(), v, v + 8);
                    if (j >= 1 && j <= 5 && rr.chance(0.55f)) {
                        // a lateral root
                        float la = ang + (rr.chance(0.5f) ? 1.f : -1.f) * rr.range(0.5f, 1.2f);
                        float lx = qx, ly = qy, lr = rad1 * 0.55f;
                        const float ltot = total * rr.range(0.2f, 0.45f);
                        for (int q = 0; q < 4; q++) {
                            la += gauss(rr) * 0.3f + wrapAng(-PI * 0.5f - la) * 0.05f;
                            float mx = lx + std::cos(la) * ltot / 4.f, my = ly + std::sin(la) * ltot / 4.f;
                            my = std::min(my, myc::surf(mx) - 0.02f * s);
                            const float w[8] = {lx, ly, mx, my, lr, lr * 0.75f, shade * 0.95f, (float)k};
                            net.roots.insert(net.roots.end(), w, w + 8);
                            lx = mx; ly = my; lr *= 0.75f;
                        }
                    }
                    px = qx; py = qy; rad = rad1;
                }
            }
        }
    }
    return net;
}
