// BRUTALIST: building the place (the monument and its slot, the plaza and its steps, the civic building on its
// pilotis, the growing pile of modules and its tower crane, the far city, the weeds).
#include <algorithm>
#include <cmath>

#include "universes/brutalist/brutalist.hpp"
#include "universes/brutalist/world.hpp"

using namespace brutalist_world;

namespace {
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// A box between two corners.
void span(std::vector<BrutalistUniverse::Box>& out, const vec3& a, const vec3& b, const vec3& col, int mat, float p0 = 0, float p1 = 0,
          float p2 = 0, float seed = 0) {
    const vec3 c = (a + b) * 0.5f;
    const vec3 h(std::fabs(b.x - a.x) * 0.5f, std::fabs(b.y - a.y) * 0.5f, std::fabs(b.z - a.z) * 0.5f);
    out.push_back({{c.x, c.y, c.z, (float)mat}, {h.x, 0, 0, p0}, {0, h.y, 0, p1}, {0, 0, h.z, p2}, {col.x, col.y, col.z, seed}});
}
} // namespace

// =====================================================================================================
// Geometry helpers
// =====================================================================================================

void BrutalistUniverse::box(std::vector<Box>& out, const vec3& c, const vec3& h, const vec3& col, int mat, float p0, float p1, float p2,
                            float seed) {
    out.push_back({{c.x, c.y, c.z, (float)mat}, {h.x, 0, 0, p0}, {0, h.y, 0, p1}, {0, 0, h.z, p2}, {col.x, col.y, col.z, seed}});
}

void BrutalistUniverse::obox(std::vector<Box>& out, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat,
                             float p0, float p1, float p2, float seed) {
    out.push_back({{c.x, c.y, c.z, (float)mat}, {ax.x, ax.y, ax.z, p0}, {ay.x, ay.y, ay.z, p1}, {az.x, az.y, az.z, p2},
                   {col.x, col.y, col.z, seed}});
}

// A square beam from a to b, w thick.
void BrutalistUniverse::beam(std::vector<Box>& out, const vec3& a, const vec3& b, float w, const vec3& col, int mat) {
    const vec3 d = b - a;
    const float len = length(d);
    if (len < 1e-4f) return;
    const vec3 dir = d / len;
    const vec3 up = std::fabs(dir.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
    const vec3 s = normalize(cross(dir, up));
    const vec3 u = cross(s, dir);
    obox(out, (a + b) * 0.5f, dir * (len * 0.5f), u * (w * 0.5f), s * (w * 0.5f), col, mat);
}

void BrutalistUniverse::line(std::vector<Line>& out, const vec3& a, const vec3& b, float wpx, const vec3& col, float alpha) {
    if (alpha <= 0.002f) return;
    out.push_back({{a.x, a.y, a.z, wpx}, {b.x, b.y, b.z, alpha}, {col.x, col.y, col.z, 0}});
}

// A tuft of weeds: thin blades leaning out from a crack.
void BrutalistUniverse::tuft(std::vector<Box>& out, const vec3& base, float size, int blades, uint64_t seed, const vec3& col) {
    for (int i = 0; i < blades; i++) {
        const float a = hashf(seed * 31 + i) * TAU;
        const float lean = 0.25f + 0.7f * hashf(seed * 17 + i * 3);
        const float len = size * (0.45f + 0.55f * hashf(seed * 7 + i * 5));
        const vec3 dir = normalize(vec3(std::cos(a) * lean, 1.f, std::sin(a) * lean * 0.6f));
        const vec3 side = normalize(cross(dir, vec3(std::sin(a), 0.f, -std::cos(a)) + vec3(0.001f, 0, 0)));
        const vec3 nrm = cross(side, dir);
        const vec3 c = base + vec3(std::cos(a), 0, std::sin(a)) * size * 0.08f * hashf(seed + i) + dir * (len * 0.5f);
        const float w = size * (0.025f + 0.03f * hashf(seed * 3 + i * 11));
        const vec3 tint = col * (0.75f + 0.5f * hashf(seed * 5 + i * 13));
        obox(out, c, dir * (len * 0.5f), side * w, nrm * 0.008f, tint, M_PLANT, 0, 0, 0, hashf(seed + i * 7));
    }
}

// =====================================================================================================
// The static place
// =====================================================================================================

void BrutalistUniverse::buildWorld() {
    static_.clear();
    Rng r(seed_ ^ 0xB2B2ull);
    const Palette& P = palT_;
    const vec3 conc = P.conc, pave = P.conc * 0.92f;
    auto tone = [&](float k) { return conc * (1.f + k * (r.uniform() - 0.5f)); };

    // ---- The plaza, its steps up to the podium, the podium.
    span(static_, vec3(-160.f, Y_G - 1.f, 40.f), vec3(160.f, Y_G, Z_STEPS), pave, M_PAVE, 0, 0, 0, 0.3f);
    const int NSTEPS = 6;
    const float rise = (Y_P - Y_G) / NSTEPS, run = 0.9f;
    for (int i = 0; i < NSTEPS; i++) {
        const float z0 = Z_STEPS - run * i;
        span(static_, vec3(-72.f, Y_G - 0.5f, z0), vec3(72.f, Y_G + rise * (i + 1), z0 - run), pave * 1.02f, M_CONC, 0, 3.f, 0.2f, 0.1f * i);
    }
    const float zPod = Z_STEPS - run * NSTEPS;
    span(static_, vec3(-160.f, Y_G - 1.f, zPod), vec3(160.f, Y_P, -230.f), pave, M_PAVE, 0, 0, 0, 0.7f);
    // the retaining walls beyond the steps
    for (int s = -1; s <= 1; s += 2)
        span(static_, vec3(s * 72.f, Y_G - 0.5f, Z_STEPS + 0.01f), vec3(s * 160.f, Y_P + 0.9f, zPod), tone(0.1f), M_CONC, 0, 0.f, 0.7f, 0.4f);

    // ---- The monument: the sill the stack stands on, the piers, the lintel, the slot's dark lining, its mass.
    const float xs = X_PIER + 0.25f;
    span(static_, vec3(-xs, Y_G - 0.2f, Z_BACK - 2.f), vec3(xs, Y_SILL, 1.3f), conc, M_CONC, 0, 0.f, 0.5f, 0.11f);
    span(static_, vec3(-xs - 0.9f, Y_G - 0.2f, 1.3f), vec3(xs + 0.9f, Y_G + 0.6f, 2.5f), conc * 0.97f, M_CONC, 0, 3.f, 0.4f, 0.13f);
    for (int s = -1; s <= 1; s += 2) {
        const float x0 = s * X_SLOT, x1 = s * X_PIER;
        span(static_, vec3(x0, Y_SILL, Z_BACK - 2.f), vec3(x1, Y_LINTEL + 0.1f, Z_PIER), tone(0.06f), M_CONC, 0, 0.f, 0.75f, 0.2f + 0.1f * s);
    }
    span(static_, vec3(-xs, Y_LINTEL, Z_BACK - 2.f), vec3(xs, Y_MON, Z_PIER + 0.8f), conc, M_CONC, 0, 0.f, 0.9f, 0.31f);
    span(static_, vec3(-xs - 0.3f, Y_MON, Z_BACK - 2.2f), vec3(xs + 0.3f, Y_MON + 0.4f, Z_PIER + 1.0f), conc * 1.04f, M_CONC, 0, 3.f, 0.3f, 0.37f);
    span(static_, vec3(-X_SLOT, Y_SILL, Z_BACK - 0.4f), vec3(X_SLOT, Y_LINTEL, Z_BACK), P.lining, M_LINING, 0.8f, 0.f, 0.35f, 0.5f);
    // the mass behind (it casts its long shadow over the plaza)
    span(static_, vec3(-xs, Y_G - 0.2f, Z_BACK - 9.f), vec3(xs, Y_MON - 1.2f, Z_BACK - 2.f), tone(0.05f), M_CONC, 0, 2.f, 0.6f, 0.41f);
    // the low walls running out from it, a bench cast against the left one
    span(static_, vec3(-X_PIER, Y_G, -1.6f), vec3(-19.f, Y_G + 2.6f, -0.6f), tone(0.06f), M_CONC, 0, 0.f, 0.8f, 0.43f);
    span(static_, vec3(-X_PIER - 0.4f, Y_G, -0.6f), vec3(-16.f, Y_G + 0.9f, 0.6f), tone(0.06f), M_CONC, 0, 3.f, 0.4f, 0.47f);
    span(static_, vec3(X_PIER, Y_G, -2.4f), vec3(13.5f, Y_G + 3.4f, -1.4f), tone(0.06f), M_CONC, 0, 2.f, 0.8f, 0.53f);
    span(static_, vec3(13.5f, Y_G, -2.4f), vec3(14.6f, Y_G + 4.6f, 3.f), tone(0.06f), M_CONC, 0, 0.f, 0.8f, 0.59f);

    // ---- On the plaza: bollards along the steps' foot, a long cast bench, a planter with its shrub, lamp masts.
    for (float x = 17.f; x < 70.f; x += 4.2f)
        for (int s = -1; s <= 1; s += 2)
            span(static_, vec3(s * x - 0.3f, Y_G, Z_STEPS + 1.6f), vec3(s * x + 0.3f, Y_G + 0.95f, Z_STEPS + 2.2f), tone(0.06f), M_CONC, 0, 3.f, 0.5f,
                 0.02f * x);
    span(static_, vec3(19.f, Y_G, -8.4f), vec3(33.f, Y_G + 0.85f, -7.2f), tone(0.04f), M_CONC, 0, 3.f, 0.6f, 0.61f);
    span(static_, vec3(19.4f, Y_G + 0.85f, -8.5f), vec3(32.6f, Y_G + 0.95f, -7.1f), conc * 1.05f, M_CONC, 0, 3.f, 0.2f, 0.62f);
    {
        const vec3 pc(-27.f, Y_G, -6.f);
        span(static_, pc + vec3(-2.2f, 0.f, -2.2f), pc + vec3(2.2f, 1.4f, 2.2f), tone(0.05f), M_CONC, 0, 0.f, 0.7f, 0.63f);
        span(static_, pc + vec3(-1.9f, 1.4f, -1.9f), pc + vec3(1.9f, 1.45f, 1.9f), P.plant * 0.25f, M_PLANT);
        Rng sr(seed_ ^ 0x5B5Bull);
        for (int i = 0; i < 160; i++) {
            const float a = sr.uniform() * TAU, rad = std::sqrt(sr.uniform()) * 1.9f, h = 1.5f + sr.range(0.f, 1.6f) * (1.f - 0.5f * rad / 1.9f);
            const vec3 c = pc + vec3(std::cos(a) * rad, h, std::sin(a) * rad);
            const vec3 ax = normalize(vec3(sr.range(-1.f, 1.f), sr.range(-0.5f, 0.8f), sr.range(-1.f, 1.f)));
            const vec3 ay = normalize(cross(ax, vec3(0.2f, 1.f, 0.3f)));
            obox(static_, c, ax * 0.2f, ay * 0.13f, normalize(cross(ax, ay)) * 0.01f, P.plant * sr.range(0.7f, 1.2f), M_PLANT, 0, 0, 0, sr.uniform());
        }
        tuft(static_, pc + vec3(2.3f, 0.f, 1.5f), 0.8f, 7, seed_ * 5 + 3, P.plant);
    }
    for (float x : {-34.f, -19.5f, 21.f, 36.f}) {
        span(static_, vec3(x - 0.17f, Y_G, -3.17f), vec3(x + 0.17f, Y_G + 7.5f, -2.83f), tone(0.04f), M_CONC, 0, 3.f, 0.6f, 0.65f);
        span(static_, vec3(x - 0.17f, Y_G + 7.5f, -3.9f), vec3(x + 0.17f, Y_G + 7.8f, -2.83f), tone(0.04f), M_CONC, 0, 3.f, 0.3f, 0.66f);
        span(static_, vec3(x - 0.14f, Y_G + 7.38f, -3.85f), vec3(x + 0.14f, Y_G + 7.5f, -3.3f), P.glass, M_GLASS, 1.f, 1.f, 0.f, 0.67f);
    }

    // ---- The civic building on the left: pilotis, a deep glazed ground floor, a band of deep-set windows, the upper
    // floors cantilevered over the plaza behind their fins, a heavy attic; its stair tower and outside stair.
    {
        const float yCol = Y_P + 6.f;                // the pilotis' height
        const float zCol = -33.f, zGlass = -41.f;   // the columns, the glazed ground floor far back in its shade
        // the ground floor's glazing, in the shade of everything above
        span(static_, vec3(CIV_X0, Y_P, zGlass - 0.2f), vec3(CIV_X1 - 1.f, yCol, zGlass), P.glass, M_GLASS, 0.22f, 0.18f, 0.f, 0.17f);
        span(static_, vec3(CIV_X0, Y_P, CIV_Z0), vec3(CIV_X1 - 1.f, yCol, zGlass - 0.2f), conc, M_CONC, 0.4f, 0.f, 0.3f, 0.19f);
        for (float x = CIV_X1 - 2.2f; x > CIV_X0; x -= 4.8f)
            span(static_, vec3(x - 0.65f, Y_P, zCol - 0.65f), vec3(x + 0.65f, yCol, zCol + 0.65f), tone(0.05f), M_CONC, 0.8f, 1.f, 0.5f, x * 0.01f);
        // the first floor's slab, then the band of deep-set windows between their cast piers
        const float y1 = yCol + 1.1f, y2 = y1 + 4.4f;
        span(static_, vec3(CIV_X0, yCol, CIV_Z0), vec3(CIV_X1, y1, -31.f), conc, M_CONC, 0, 0.f, 0.6f, 0.21f);
        span(static_, vec3(CIV_X0, y1, CIV_Z0), vec3(CIV_X1, y2, -36.f), conc, M_CONC, 0.7f, 0.f, 0.4f, 0.23f);
        for (float x = CIV_X1 - 0.6f; x > CIV_X0; x -= 3.6f) {
            span(static_, vec3(x - 1.2f, y1, -36.f), vec3(x, y2, -31.f), tone(0.04f), M_CONC, 0, 2.f, 0.6f, 0.25f + x * 0.01f);
            span(static_, vec3(x - 3.6f, y1 + 0.2f, -35.2f), vec3(x - 1.2f, y2 - 0.3f, -35.f), P.glass, M_GLASS, 0.7f, 0.38f, 0.f,
                 hashf((uint64_t)(x * 10.f + 400.f)));
        }
        // the cantilevered upper floors: three storeys of spandrels and fins over the deep windows
        const float zF = -25.f, yTop = y2 + 12.f;
        const int storeys = 3;
        const float sh = (yTop - y2) / storeys;
        span(static_, vec3(CIV_X0 + 2.f, y2, CIV_Z0), vec3(CIV_X1 + 0.5f, yTop, -29.f), conc, M_CONC, 0.6f, 0.f, 0.4f, 0.27f);
        for (int k = 0; k < storeys; k++) {
            const float yb = y2 + sh * k;
            // the spandrel (a deep beam), its soffit seen from below on the first one
            span(static_, vec3(CIV_X0 + 2.f, yb, -29.f), vec3(CIV_X1 + 0.5f, yb + 1.3f, zF), tone(0.03f), M_CONC, 0, 0.f, 0.85f, 0.3f + 0.05f * k);
            // windows deep in their reveals, fins between them
            span(static_, vec3(CIV_X0 + 2.f, yb + 1.3f, -28.8f), vec3(CIV_X1 + 0.5f, yb + sh, -28.6f), P.glass, M_GLASS, 0.65f, 0.4f, 0.f,
                 0.31f + 0.07f * k);
            for (float x = CIV_X1 + 0.5f; x > CIV_X0 + 2.f; x -= 2.4f)
                span(static_, vec3(x - 0.45f, yb + 1.3f, -28.6f), vec3(x, yb + sh, zF - 0.02f), tone(0.03f), M_CONC, 0, 1.f, 0.5f, 0.6f + x * 0.01f);
        }
        // the attic: a heavy ribbed band, a little proud, with three deep slots
        span(static_, vec3(CIV_X0 + 1.5f, yTop, CIV_Z0), vec3(CIV_X1 + 1.f, yTop + 4.2f, zF + 0.6f), conc * 1.02f, M_CONC, 0, 2.f, 0.9f, 0.71f);
        for (int k = 0; k < 3; k++) {
            const float x = CIV_X1 - 8.f - 11.f * k;
            span(static_, vec3(x - 1.8f, yTop + 1.2f, zF + 0.55f), vec3(x + 1.8f, yTop + 2.6f, zF + 0.7f), P.lining * 0.5f, M_LINING, 0.3f, 0.f, 0.f, 0.73f);
        }
        span(static_, vec3(CIV_X0 + 1.2f, yTop + 4.2f, CIV_Z0), vec3(CIV_X1 + 1.3f, yTop + 4.7f, zF + 0.9f), conc * 1.05f, M_CONC, 0, 3.f, 0.4f, 0.77f);
        // the stair tower at its right end: blind, ribbed, a tall slot of a window
        const float tx0 = CIV_X1 + 1.5f, tx1 = CIV_X1 + 6.f;
        span(static_, vec3(tx0, Y_P, -38.f), vec3(tx1, yTop + 7.f, -31.f), tone(0.05f), M_CONC, 0, 1.f, 0.8f, 0.79f);
        span(static_, vec3(tx0 + 1.7f, y1, -30.95f), vec3(tx1 - 1.7f, yTop + 4.f, -30.9f), P.glass, M_GLASS, 0.4f, 0.5f, 0.6f, 0.81f);
        span(static_, vec3(tx0 - 0.2f, yTop + 7.f, -38.3f), vec3(tx1 + 0.2f, yTop + 7.5f, -30.7f), conc * 1.05f, M_CONC, 0, 3.f, 0.3f, 0.83f);
        // the outside stair up to the first floor, against the tower: steps, a solid sloping parapet
        const int n = 13;
        const float sx0 = tx1 + 0.2f, sx1 = tx1 + 3.4f;
        for (int i = 0; i < n; i++) {
            const float z = -22.f - 0.7f * i;
            span(static_, vec3(sx0, Y_P, z), vec3(sx1, Y_P + 0.46f * (i + 1), z - 0.7f), conc * 1.03f, M_CONC, 0, 3.f, 0.2f, 0.85f);
        }
        {
            const vec3 a(sx1 + 0.15f, Y_P + 1.f, -22.f), b(sx1 + 0.15f, Y_P + 0.46f * n + 1.f, -22.f - 0.7f * n);
            const vec3 d = b - a;
            const float len = length(d);
            const vec3 ax = d / len;
            obox(static_, (a + b) * 0.5f + vec3(0, -0.2f, 0), ax * (len * 0.5f), normalize(cross(vec3(1, 0, 0), ax)) * -1.1f, vec3(0.18f, 0, 0),
                 tone(0.04f), M_CONC, 0, 3.f, 0.5f, 0.87f);
        }
        span(static_, vec3(sx0, Y_P, -22.f - 0.7f * n), vec3(sx1 + 0.3f, yCol, -31.f), conc, M_CONC, 0, 0.f, 0.5f, 0.89f);
    }

    // ---- The growing building's base on the right: its podium deck on pilotis, the service core.
    {
        const float x1 = GROW_X0 + GROW_COLS * MOD_W + MOD_W * 0.5f;
        const float yDeck = GROW_Y0 - 0.9f;
        for (float x = GROW_X0 + 1.5f; x < x1; x += 5.2f)
            for (float z : {GROW_Z1 - 2.f, GROW_Z0 + 3.f})
                span(static_, vec3(x - 0.6f, Y_P, z - 0.6f), vec3(x + 0.6f, yDeck, z + 0.6f), tone(0.05f), M_CONC, 0.8f, 1.f, 0.5f, x * 0.013f);
        span(static_, vec3(GROW_X0, yDeck, GROW_Z0), vec3(x1, GROW_Y0 - 0.02f, GROW_Z1 + 0.5f), conc, M_CONC, 0, 0.f, 0.7f, 0.91f);
        span(static_, vec3(GROW_X0 + 4.f, Y_P, GROW_Z0 + 5.f), vec3(x1 - 4.f, yDeck, GROW_Z0 + 6.f), P.glass, M_GLASS, 0.6f, 0.4f, 0.3f, 0.93f);
        // the core: a blind ribbed tower, the lifts' crown
        const float cx0 = GROW_X0 - 5.f, cx1 = GROW_X0 - 0.2f;
        span(static_, vec3(cx0, Y_P, GROW_Z0 + 2.f), vec3(cx1, GROW_Y0 + MOD_H * GROW_LEVELS + 2.f, GROW_Z1 - 4.f), tone(0.04f), M_CONC, 0, 2.f,
             0.9f, 0.95f);
        span(static_, vec3(cx0 - 0.3f, GROW_Y0 + MOD_H * GROW_LEVELS + 2.f, GROW_Z0 + 1.7f), vec3(cx1 + 0.3f, GROW_Y0 + MOD_H * GROW_LEVELS + 2.6f, GROW_Z1 - 3.7f),
             conc * 1.05f, M_CONC, 0, 3.f, 0.3f, 0.97f);
    }

    // ---- The far city: slabs and towers in the haze.
    {
        Rng c(seed_ ^ 0xC17Full);
        for (int i = 0; i < 34; i++) {
            const float x = c.range(-280.f, 280.f), z = c.range(-330.f, -170.f);
            if (std::fabs(x) < 40.f && c.uniform() < 0.6f) continue;
            const float w = c.range(8.f, 26.f), d = c.range(8.f, 20.f), h = c.range(12.f, 60.f) * (c.uniform() < 0.25f ? 1.6f : 1.f);
            span(static_, vec3(x - w * 0.5f, Y_P, z - d * 0.5f), vec3(x + w * 0.5f, Y_P + h, z + d * 0.5f), P.far * c.range(0.85f, 1.1f), M_FAR, 0, 0, 0,
                 c.uniform());
        }
    }

    // ---- Weeds in the cracks: along the sill, at the walls' feet, in the steps' joints, on the coping.
    {
        const vec3 g = P.plant;
        uint64_t s = seed_ * 13 + 1;
        tuft(static_, vec3(-X_PIER + 0.4f, Y_SILL, 1.05f), 0.9f, 9, s++, g);
        tuft(static_, vec3(X_PIER - 0.6f, Y_SILL, 1.1f), 0.6f, 6, s++, g);
        tuft(static_, vec3(-X_PIER - 1.2f, Y_G, 1.9f), 1.2f, 11, s++, g);
        tuft(static_, vec3(X_PIER + 1.6f, Y_G, 2.2f), 0.8f, 8, s++, g);
        tuft(static_, vec3(-12.5f, Y_G, 0.8f), 1.1f, 10, s++, g);
        tuft(static_, vec3(-17.f, Y_G + 0.9f, -0.2f), 0.7f, 7, s++, g);
        tuft(static_, vec3(12.f, Y_G, -1.2f), 1.4f, 12, s++, g);
        tuft(static_, vec3(14.9f, Y_G, 2.6f), 0.9f, 8, s++, g);
        for (int i = 0; i < 9; i++) {
            const int st = r.irange(0, NSTEPS - 1);
            const float x = r.range(-60.f, 60.f);
            if (std::fabs(x) < 9.f) continue;
            tuft(static_, vec3(x, Y_G + rise * (st + 1), Z_STEPS - run * st - 0.1f), r.range(0.6f, 1.3f), 8, s++, g);
        }
        for (int i = 0; i < 4; i++) tuft(static_, vec3(r.range(-X_PIER, X_PIER), Y_MON + 0.4f, Z_PIER + 0.6f), r.range(0.5f, 0.9f), 6, s++, g);
        for (int i = 0; i < 10; i++) tuft(static_, vec3(r.range(-60.f, -16.f), Y_P, r.range(-24.f, -20.f)), r.range(0.6f, 1.1f), 7, s++, g * 0.9f);
        for (int i = 0; i < 6; i++) tuft(static_, vec3(r.range(16.f, 48.f), Y_P, r.range(-36.f, -24.f)), r.range(0.6f, 1.1f), 7, s++, g * 0.9f);
        // a sapling of birch seeded in a crack of the plaza
        const vec3 base(-24.f, Y_P, -21.f);
        beam(static_, base, base + vec3(0.3f, 5.5f, 0.f), 0.22f, vec3(0.75f, 0.74f, 0.7f), M_STEEL);
        Rng lr(seed_ ^ 0xB1C4ull);
        for (int i = 0; i < 130; i++) {
            const float a = lr.uniform() * TAU, h = lr.range(2.2f, 6.8f), rad = lr.range(0.2f, 1.f) * (1.f - 0.4f * std::fabs(h - 4.6f) / 2.4f) * 1.8f;
            const vec3 c = base + vec3(0.3f * h / 5.5f + std::cos(a) * rad, h, std::sin(a) * rad);
            const vec3 ax = normalize(vec3(lr.range(-1.f, 1.f), lr.range(-0.4f, 0.4f), lr.range(-1.f, 1.f)));
            const vec3 ay = normalize(cross(ax, vec3(0.3f, 1.f, 0.2f)));
            obox(static_, c, ax * 0.16f, ay * 0.11f, normalize(cross(ax, ay)) * 0.01f, g * lr.range(1.0f, 1.5f), M_PLANT, 0, 0, 0, lr.uniform());
        }
    }
    staticDirty_ = true;
}

// =====================================================================================================
// The growing building: its plan, its modules
// =====================================================================================================

// The slots of the pile, in the order they fill: level by level from the deck, each level in a shuffled order, every
// other level shifted half a module (its end cantilevered into the air); then a second, slimmer pile behind.
void BrutalistUniverse::planGrowth() {
    plan_.clear();
    Rng r(seed_ ^ 0x6A0Bull);
    auto pile = [&](float x0, float y0, float zc, int cols, int levels, float shift) {
        for (int l = 0; l < levels; l++) {
            int order[8];
            for (int c = 0; c < cols; c++) order[c] = c;
            for (int c = cols - 1; c > 0; c--) std::swap(order[c], order[r.irange(0, c)]);
            const float off = (l % 2) ? shift : 0.f;
            for (int k = 0; k < cols; k++) {
                const int c = order[k];
                Module m;
                m.col = c;
                m.level = l;
                m.kind = r.uniform() < 0.28f ? 1 : 0;
                const float zo = m.kind == 1 ? 3.2f : (r.uniform() < 0.3f ? -1.8f : 0.f);
                m.pos = vec3(x0 + MOD_W * (c + 0.5f) + off, y0 + MOD_H * (l + 0.5f), zc + zo);
                m.tone = vec3(1.f) * r.range(0.9f, 1.08f);
                m.seed = r.uniform();
                plan_.push_back(m);
            }
        }
    };
    pile(GROW_X0, GROW_Y0, (GROW_Z0 + GROW_Z1) * 0.5f, GROW_COLS, GROW_LEVELS, MOD_W * 0.5f);
    pile(GROW_X0 + 18.f, GROW_Y0 + 2.f, GROW_Z0 - 22.f, 3, 9, -MOD_W * 0.4f);
}

// A module: a cast shell, its face deep set behind a frame, a balcony's parapet; a cantilevered one has a deeper
// hood. `at` is where it hangs (its place once landed).
void BrutalistUniverse::buildModule(std::vector<Box>& out, const Module& m, const vec3& at, float alpha) const {
    (void)alpha;
    const vec3 conc = pal_.conc * m.tone;
    const float hw = MOD_W * 0.5f - 0.04f, hh = MOD_H * 0.5f - 0.03f, hd = MOD_D * 0.5f;
    const float zf = at.z + hd, rec = m.kind == 1 ? 2.2f : 1.5f;
    const float pat = m.level % 2 ? 2.f : 0.f;
    // the shell behind its face
    span(out, vec3(at.x - hw, at.y - hh, at.z - hd), vec3(at.x + hw, at.y + hh, zf - rec), conc, M_CONC, 0, pat, 0.6f, m.seed);
    // the glazing, deep in
    span(out, vec3(at.x - hw + 0.4f, at.y - hh + 0.4f, zf - rec), vec3(at.x + hw - 0.4f, at.y + hh - 0.5f, zf - rec + 0.06f), pal_.glass, M_GLASS,
         0.75f, 0.45f, 0.5f, m.seed);
    // the frame: a hood over it, the cheeks, the balcony's parapet
    span(out, vec3(at.x - hw, at.y + hh - 0.55f, zf - rec), vec3(at.x + hw, at.y + hh, zf), conc, M_CONC, 0, 0.f, 0.8f, m.seed + 0.1f);
    span(out, vec3(at.x - hw, at.y - hh, zf - rec), vec3(at.x - hw + 0.4f, at.y + hh - 0.55f, zf), conc, M_CONC, 0, 0.f, 0.7f, m.seed + 0.2f);
    span(out, vec3(at.x + hw - 0.4f, at.y - hh, zf - rec), vec3(at.x + hw, at.y + hh - 0.55f, zf), conc, M_CONC, 0, 0.f, 0.7f, m.seed + 0.3f);
    span(out, vec3(at.x - hw + 0.4f, at.y - hh, zf - 0.3f), vec3(at.x + hw - 0.4f, at.y - hh + 1.15f, zf), conc * 1.03f, M_CONC, 0, 3.f, 0.6f,
         m.seed + 0.4f);
    if (m.kind == 1) {
        // a cantilevered one: two fins down its face
        for (float fx : {-hw * 0.34f, hw * 0.34f})
            span(out, vec3(at.x + fx - 0.18f, at.y - hh + 1.15f, zf - rec), vec3(at.x + fx + 0.18f, at.y + hh - 0.55f, zf - 0.2f), conc, M_CONC, 0, 1.f,
                 0.5f, m.seed + 0.5f);
    }
}

// The tower crane beside the pile: a lattice mast, its slewing jib and counter-jib with their ballast, the cab.
void BrutalistUniverse::buildCrane(std::vector<Box>& out) {
    const vec3 col = pal_.steel;
    const float x = CRANE_X, z = CRANE_Z, m = 1.0f, w = 0.12f;
    // the mast: four chords and their zigzag bracing
    for (int i = 0; i < 4; i++) {
        const float dx = (i & 1) ? m : -m, dz = (i & 2) ? m : -m;
        beam(out, vec3(x + dx, Y_P, z + dz), vec3(x + dx, JIB_Y - 0.6f, z + dz), w * 1.4f, col, M_STEEL);
    }
    const float step = 2.f;
    int k = 0;
    for (float y = Y_P; y < JIB_Y - 2.f; y += step, k++) {
        const float s = (k % 2) ? 1.f : -1.f;
        beam(out, vec3(x - m * s, y, z + m), vec3(x + m * s, y + step, z + m), w, col, M_STEEL);
        beam(out, vec3(x - m, y, z - m * s), vec3(x - m, y + step, z + m * s), w, col, M_STEEL);
        beam(out, vec3(x + m, y, z - m * s), vec3(x + m, y + step, z + m * s), w, col, M_STEEL);
    }
    // the slewing jib along -x (a triangular lattice), the counter-jib with its concrete ballast, the cab, the top
    const float c = std::cos(crane_.jib), sn = std::sin(crane_.jib);
    auto J = [&](float u, float yy, float v) { return vec3(x - u * c + v * sn, yy, z - u * sn - v * c); };
    const float len = 44.f, back = 13.f;
    for (float v : {-0.7f, 0.7f}) beam(out, J(-back, JIB_Y, v), J(len, JIB_Y, v), w * 1.4f, col, M_STEEL);
    beam(out, J(-back, JIB_Y + 1.3f, 0.f), J(len, JIB_Y + 1.3f, 0.f), w * 1.4f, col, M_STEEL);
    k = 0;
    for (float u = -back; u < len; u += 2.f, k++) {
        const float s = (k % 2) ? 0.7f : -0.7f;
        beam(out, J(u, JIB_Y, s), J(u + 2.f, JIB_Y + 1.3f, 0.f), w, col, M_STEEL);
        beam(out, J(u, JIB_Y, -0.7f), J(u, JIB_Y, 0.7f), w, col, M_STEEL);
    }
    for (int i = 0; i < 3; i++) span(out, J(-back + 0.4f + 1.4f * i, JIB_Y - 1.6f, -0.9f), J(-back + 1.6f + 1.4f * i, JIB_Y + 0.6f, 0.9f), pal_.conc * 0.9f,
                                     M_CONC, 0, 3.f, 0.5f, 0.3f * i);
    beam(out, vec3(x, JIB_Y, z), vec3(x, CRANE_TOP, z), 0.5f, col, M_STEEL);
    span(out, J(-1.2f, JIB_Y - 2.2f, 1.1f), J(1.2f, JIB_Y - 0.2f, 3.0f), col * 0.9f, M_STEEL);
    // the trolley, the hook block
    const vec3 tr = J(crane_.trolley, JIB_Y - 0.15f, 0.f);
    span(out, tr + vec3(-0.6f, -0.35f, -0.6f), tr + vec3(0.6f, 0.05f, 0.6f), col * 0.7f, M_STEEL);
    const vec3 hk = vec3(tr.x, crane_.hook, tr.z);
    span(out, hk + vec3(-0.35f, -0.5f, -0.25f), hk + vec3(0.35f, 0.3f, 0.25f), pal_.steel * 0.5f + vec3(0.25f, 0.08f, 0.04f), M_STEEL);
}
