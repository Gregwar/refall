// DOCKS: building the port (the static terminal across the basin, the cranes, the ships and their cargo).
#include <algorithm>
#include <cmath>

#include "universes/docks/docks.hpp"
#include "universes/docks/world.hpp"

using namespace docks_world;

namespace {
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }
} // namespace

// =====================================================================================================
// Geometry helpers
// =====================================================================================================

void DocksUniverse::box(std::vector<Box>& out, const vec3& c, const vec3& h, const vec3& col, int mat, float p0, float p1, float p2,
                        float seed) {
    out.push_back({{c.x, c.y, c.z, (float)mat}, {h.x, 0, 0, p0}, {0, h.y, 0, p1}, {0, 0, h.z, p2}, {col.x, col.y, col.z, seed}});
}

void DocksUniverse::obox(std::vector<Box>& out, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat,
                         float p0, float p1, float p2, float seed) {
    out.push_back({{c.x, c.y, c.z, (float)mat}, {ax.x, ax.y, ax.z, p0}, {ay.x, ay.y, ay.z, p1}, {az.x, az.y, az.z, p2},
                   {col.x, col.y, col.z, seed}});
}

// A square beam from a to b, w thick.
void DocksUniverse::beam(std::vector<Box>& out, const vec3& a, const vec3& b, float w, const vec3& col, int mat) {
    const vec3 d = b - a;
    const float len = length(d);
    if (len < 1e-4f) return;
    const vec3 dir = d / len;
    const vec3 up = std::fabs(dir.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
    const vec3 s = normalize(cross(dir, up));
    const vec3 u = cross(s, dir);
    obox(out, (a + b) * 0.5f, dir * (len * 0.5f), u * (w * 0.5f), s * (w * 0.5f), col, mat);
}

void DocksUniverse::line(std::vector<Line>& out, const vec3& a, const vec3& b, float wpx, const vec3& col, float alpha) {
    if (alpha <= 0.002f) return;
    out.push_back({{a.x, a.y, a.z, wpx}, {b.x, b.y, b.z, alpha}, {col.x, col.y, col.z, 0}});
}

void DocksUniverse::sprite(const vec3& p, float r, const vec3& col, float k) {
    if (k <= 0.002f) return;
    sprites_.push_back({{p.x, p.y, p.z, r}, {col.x, col.y, col.z, k}});
}

// =====================================================================================================
// The static port: the near quay and its gantry, the far quay, the yard, the masts, the sheds and offices
// =====================================================================================================

void DocksUniverse::buildPort() {
    static_.clear();
    lamps_.clear();
    Rng r(seed_ ^ 0x9047ull);
    const Palette& P = palT_;
    const vec3 concrete = P.quay;
    const vec3 dark(0.035f, 0.036f, 0.04f);

    // ---- The near quay, the stack's ground.
    box(static_, vec3(0.f, Y_Q - 2.f, (Z_QE + 80.f) * 0.5f), vec3(400.f, 2.f, (80.f - Z_QE) * 0.5f), concrete, M_CONCRETE, 1.f);
    // its quay wall seen at the edge (a coping, fenders hanging into the water) and bollards
    box(static_, vec3(0.f, Y_Q - 1.6f, Z_QE - 0.05f), vec3(400.f, 1.7f, 0.05f), concrete * 0.6f, M_CONCRETE);
    for (int i = -30; i <= 30; i++) {
        const float x = i * 8.f + 2.f;
        box(static_, vec3(x, Y_Q + 0.22f, Z_QE + 0.55f), vec3(0.22f, 0.22f, 0.22f), vec3(0.06f, 0.06f, 0.065f), M_STEEL);
        box(static_, vec3(x, Y_Q + 0.47f, Z_QE + 0.55f), vec3(0.3f, 0.05f, 0.3f), vec3(0.06f, 0.06f, 0.065f), M_STEEL);
    }
    // the gantry over the well: its back legs at the sides of the screen on their bogies, the rest out of frame
    {
        const vec3 paint = lerp(P.crane, vec3(0.3f, 0.31f, 0.33f), 0.45f) * 0.8f, paint2 = lerp(P.crane2, vec3(0.3f), 0.3f) * 0.8f;
        for (int s = -1; s <= 1; s += 2) {
            const float x = s * NEAR_LEG_X, z = NEAR_LEG_Z1;
            box(static_, vec3(x, Y_Q + 2.f + 16.f, z), vec3(0.5f, 16.f, 0.5f), paint, M_STEEL);
            // the bogie on its rail, the sill stub
            box(static_, vec3(x, Y_Q + 1.45f, z + 0.6f), vec3(0.62f, 0.45f, 1.9f), paint2, M_STEEL);
            box(static_, vec3(x, Y_Q + 0.5f, z + 0.6f), vec3(0.45f, 0.5f, 1.7f), dark, M_DARK);
            for (int w = -1; w <= 1; w += 2) box(static_, vec3(x, Y_Q + 0.33f, z + 0.6f + w * 1.1f), vec3(0.52f, 0.33f, 0.33f), vec3(0.02f), M_DARK);
            // an electrical house and a ladder on the leg's inner side
            box(static_, vec3(x - s * 1.15f, Y_Q + 3.4f, z), vec3(0.62f, 0.85f, 0.7f), paint2 * 0.9f, M_STEEL);
            box(static_, vec3(x - s * 0.62f, Y_Q + 9.f, z + 0.45f), vec3(0.05f, 7.f, 0.05f), paint * 0.7f, M_STEEL);
        }
    }
    // a few containers waiting on the near quay, out at the sides
    for (int s = -1; s <= 1; s += 2) {
        for (int row = 0; row < 3; row++)
            for (int bay = 0; bay < 3; bay++) {
                const int h = 1 + (int)(hashf(seed_ + row * 7 + bay * 13 + (s > 0 ? 99 : 0)) * 3.f);
                for (int t = 0; t < h; t++) {
                    const float x = s * (20.5f + bay * 2.65f), z = -12.f + row * 1.0f;
                    const int ci = (int)(hashf(seed_ * 3 + row * 31 + bay * 5 + t * 17 + (s > 0 ? 7 : 0)) * 10.f);
                    box(static_, vec3(x, Y_Q + 0.49f + t * 0.98f, z), vec3(1.27f, 0.47f, 0.46f), cargoColor(ci), M_BOX, 1.f, 0, 0,
                        hashf(row * 3 + bay + t * 11));
                }
            }
    }

    // ---- The far quay: its wall and fenders, the apron, the yard, the masts, the sheds and offices.
    const float farD = 260.f;
    box(static_, vec3(0.f, (Y_F + Y_W - 3.f) * 0.5f, Z_FE - farD * 0.5f), vec3(500.f, (Y_F - Y_W + 3.f) * 0.5f, farD * 0.5f), concrete,
        M_CONCRETE, 2.f);
    for (int i = -60; i <= 60; i++) {
        const float x = i * 5.f;
        box(static_, vec3(x, (Y_F + Y_W) * 0.5f - 0.3f, Z_FE + 0.35f), vec3(0.6f, 1.05f, 0.35f), vec3(0.02f), M_DARK);
    }
    // the yard: blocks of stacked containers, long side along the quay
    {
        const float blockL = 26.f, gap = 7.f;
        for (int zb = 0; zb < 2; zb++) {
            const float z0 = Z_YARD - zb * 14.f;
            for (int b = -8; b <= 7; b++) {
                const float x0 = b * (blockL + gap) + gap * 0.5f + (zb ? 9.f : 0.f);
                for (int bay = 0; bay < 10; bay++)
                    for (int row = 0; row < 6; row++) {
                        const uint64_t h = splitmix64(seed_ ^ (uint64_t)((zb * 100 + b + 50) * 1000 + bay * 10 + row));
                        float hn = hashf(h);
                        // the stacks fall away at the ends of a block
                        int tiers = 1 + (int)(hn * 3.6f);
                        if (bay == 0 || bay == 9) tiers = std::min(tiers, 2);
                        if (hashf(h ^ 5) < 0.12f) tiers = 0;
                        for (int t = 0; t < tiers; t++) {
                            const int ci = (int)(hashf(h ^ (uint64_t)(t * 77 + 3)) * 10.f);
                            box(static_, vec3(x0 + bay * 2.6f + 1.3f, Y_F + 0.49f + t * 0.98f, z0 - row * 1.0f), vec3(1.25f, 0.47f, 0.46f),
                                lerp(cargoColor(ci), vec3(0.28f, 0.29f, 0.3f), 0.4f) * 0.8f, M_BOX, 1.f, 0, 0, hashf(h ^ (uint64_t)t));
                        }
                    }
            }
        }
    }
    // the masts: tall poles along the apron and through the yard, each a row of lamps at its head
    for (int row = 0; row < 3; row++) {
        const float z = row == 0 ? Z_YARD + 5.f : row == 1 ? Z_YARD - 28.5f : Z_YARD - 8.f;
        const float h = row == 2 ? 20.f : 26.f;
        for (int i = -7; i <= 7; i++) {
            const float x = i * 34.f + (row == 1 ? 17.f : 0.f) + (row == 2 ? 8.5f : 0.f);
            if (row == 2 && (i % 2)) continue;
            if (std::fabs(x) < 9.f) continue; // never right behind the well
            box(static_, vec3(x, Y_F + h * 0.5f, z), vec3(0.22f, h * 0.5f, 0.22f), vec3(0.32f, 0.33f, 0.34f), M_STEEL);
            box(static_, vec3(x, Y_F + h + 0.1f, z), vec3(1.4f, 0.12f, 0.5f), vec3(0.2f), M_STEEL);
            for (int k = -1; k <= 1; k++) {
                Lamp l;
                l.p = vec3(x + k * 0.9f, Y_F + h - 0.15f, z + 0.3f);
                l.col = (row + i) % 3 == 0 ? P.lampB : P.lampA;
                l.r = 1.3f;
                l.k = 1.f;
                l.range = k == 0 ? 42.f : 0.f;
                l.group = std::abs(i) % 3 == 0 ? 0 : (std::abs(i) % 3 == 1 ? 1 : 2);
                l.order = std::fabs(x) * 0.012f + row * 0.15f;
                lamps_.push_back(l);
            }
        }
    }
    // sheds and offices behind the yard, the town's port buildings beyond
    {
        for (int i = -6; i <= 6; i++) {
            const float x = i * 40.f + r.range(-8.f, 8.f);
            const float w = r.range(9.f, 15.f), h = r.range(4.f, 7.f), d = r.range(6.f, 10.f);
            box(static_, vec3(x, Y_F + h * 0.5f, Z_YARD - 46.f - d * 0.5f), vec3(w, h * 0.5f, d * 0.5f), lerp(P.quay, vec3(0.55f, 0.58f, 0.62f), 0.4f) * 0.8f,
                M_BUILDING, 0.12f, 1.f, 0.f, r.uniform());
            box(static_, vec3(x, Y_F + h + 0.25f, Z_YARD - 46.f - d * 0.5f), vec3(w + 0.2f, 0.25f, d * 0.5f + 0.2f), vec3(0.18f), M_STEEL);
        }
        for (int i = 0; i < 9; i++) {
            const float x = r.range(-170.f, 170.f);
            if (std::fabs(x) < 12.f) continue;
            const float w = r.range(3.f, 6.f), h = r.range(6.f, 15.f), d = r.range(3.f, 6.f);
            const float z = Z_YARD - 64.f - r.range(0.f, 30.f);
            box(static_, vec3(x, Y_F + h * 0.5f, z), vec3(w, h * 0.5f, d), lerp(vec3(0.3f, 0.32f, 0.36f), P.quay, 0.3f), M_BUILDING,
                r.range(0.25f, 0.55f), 1.2f, 0.f, r.uniform());
            Lamp l;
            l.p = vec3(x, Y_F + h + 0.6f, z);
            l.col = vec3(1.f, 0.08f, 0.04f);
            l.r = 0.9f;
            l.k = 0.8f;
            l.group = 0;
            l.order = std::fabs(x) * 0.012f + 1.f;
            lamps_.push_back(l);
        }
    }
    staticDirty_ = true;
}

// =====================================================================================================
// A ship-to-shore crane on the far quay
// =====================================================================================================

void DocksUniverse::buildCrane(std::vector<Box>& out, const Crane& c, bool) const {
    const Palette& P = pal_;
    const vec3 paint = P.crane, paint2 = P.crane2;
    const float x = c.x;
    const float top = CRANE_TOP;
    const float zm = (CRANE_SEA + CRANE_LAND) * 0.5f;
    const float hw = 3.7f;          // half the legs' spacing along the quay
    const float gx = 1.7f;          // the boom's girders
    // the boom's outreach pivots up around its hinge over the seaside legs (raised on an idle crane)
    const float zh = CRANE_SEA + 1.f;
    const float ang = c.raise * 1.32f;
    const vec3 odir(0.f, std::sin(ang), std::cos(ang));
    const float olen = BOOM_TIP - zh;
    auto outAt = [&](float t, float gxs) { return vec3(x + gxs, BOOM_Y, zh) + odir * (olen * t); };
    for (int s = -1; s <= 1; s += 2) {
        const float lx = x + s * hw;
        for (float z : {CRANE_SEA, CRANE_LAND}) {
            box(out, vec3(lx, (Y_F + top) * 0.5f + 0.5f, z), vec3(0.42f, (top - Y_F) * 0.5f, 0.42f), paint, M_STEEL);
            box(out, vec3(lx, Y_F + 0.45f, z), vec3(0.45f, 0.45f, 1.4f), vec3(0.03f), M_DARK);
        }
        box(out, vec3(lx, Y_F + 1.5f, zm), vec3(0.38f, 0.45f, 8.4f), paint, M_STEEL);
        box(out, vec3(lx, Y_F + 8.f, zm), vec3(0.25f, 0.25f, 8.f), paint, M_STEEL);
        beam(out, vec3(lx, Y_F + 1.9f, CRANE_SEA - 0.5f), vec3(lx, Y_F + 7.8f, zm), 0.26f, paint, M_STEEL);
        beam(out, vec3(lx, Y_F + 1.9f, CRANE_LAND + 0.5f), vec3(lx, Y_F + 7.8f, zm), 0.26f, paint, M_STEEL);
        // the A-frame and its stays
        const vec3 apex(x + s * 2.2f, CRANE_APEX, zm + 1.f);
        beam(out, vec3(lx, top + 1.f, CRANE_SEA), apex, 0.38f, paint, M_STEEL);
        beam(out, vec3(lx, top + 1.f, CRANE_LAND), apex, 0.38f, paint, M_STEEL);
        beam(out, apex, outAt(1.f, s * gx) + vec3(0, 0.5f, 0), 0.16f, paint, M_STEEL);
        beam(out, apex, outAt(0.5f, s * gx) + vec3(0, 0.5f, 0), 0.16f, paint, M_STEEL);
        beam(out, apex, vec3(x + s * gx, BOOM_Y + 0.5f, BOOM_BACK + 2.f), 0.16f, paint, M_STEEL);
        // the boom's two girders: the backreach, and the outreach over the ship
        box(out, vec3(x + s * gx, BOOM_Y, (zh + BOOM_BACK) * 0.5f), vec3(0.24f, 0.5f, (zh - BOOM_BACK) * 0.5f), paint, M_STEEL);
        obox(out, outAt(0.5f, s * gx), odir * (olen * 0.5f), vec3(0.f, std::cos(ang), -std::sin(ang)) * 0.5f, vec3(0.24f, 0, 0), paint, M_STEEL);
    }
    // the portal's beams across the quay
    for (float z : {CRANE_SEA, CRANE_LAND}) box(out, vec3(x, top + 0.55f, z), vec3(hw + 0.4f, 0.55f, 0.45f), paint, M_STEEL);
    box(out, vec3(x, CRANE_APEX, zm + 1.f), vec3(2.6f, 0.22f, 0.22f), paint, M_STEEL);
    for (int k = 0; k < 4; k++) {
        const float z = lerpf(BOOM_BACK + 2.f, zh - 1.f, k / 3.f);
        box(out, vec3(x, BOOM_Y + 0.4f, z), vec3(gx, 0.12f, 0.12f), paint, M_STEEL);
    }
    for (int k = 1; k < 4; k++) {
        const vec3 p = outAt(k / 3.f, 0.f) + vec3(0, 0.4f, 0);
        box(out, p, vec3(gx, 0.12f, 0.12f), paint, M_STEEL);
    }
    // the machinery house on the back of the boom
    box(out, vec3(x, BOOM_Y + 1.6f, BOOM_BACK + 7.f), vec3(2.2f, 1.f, 4.f), paint2, M_STEEL);
    if (c.raise > 0.01f) return;
    // the trolley, its cab, the hoist and the spreader
    const float tz = lerpf(BOOM_BACK + 12.f, BOOM_TIP + 2.f, c.trolley);
    box(out, vec3(x, BOOM_Y - 0.5f, tz), vec3(gx + 0.2f, 0.38f, 1.3f), paint2 * 0.85f, M_STEEL);
    box(out, vec3(x - 0.9f, BOOM_Y - 1.55f, tz + 0.6f), vec3(0.6f, 0.6f, 0.6f), paint2, M_CAB);
    const float sy = craneHook(c);
    box(out, vec3(x, sy, tz), vec3(1.35f, 0.12f, 0.5f), lerp(paint2, vec3(0.9f, 0.7f, 0.15f), 0.6f), M_STEEL);
    if (c.loaded) box(out, vec3(x, sy - 0.6f, tz), vec3(1.27f, 0.47f, 0.46f), cargoColor(c.colorIdx), M_BOX, 1.f, 0, 0, 0.3f);
}

// The spreader's height under a crane's boom: low over the quay or the ship's cargo, up under the boom.
float DocksUniverse::craneHook(const Crane& c) const {
    const float overShip = smoothstepf(0.4f, 0.6f, c.trolley);
    const float low = lerpf(Y_F + 1.2f, DECK_Y + 4.9f, overShip);
    return lerpf(BOOM_Y - 2.3f, low, c.hoist);
}

// =====================================================================================================
// Ships
// =====================================================================================================

vec3 DocksUniverse::slotPos(int i, int slot) const {
    const Ship& sh = ships_[i];
    const float s = i == 0 ? 1.f : -1.f;
    const float xStern = sh.x - s * SHIP_LEN * 0.5f;
    const int perBay = ROWS * TIERS;
    const int bay = slot / perBay, rem = slot % perBay, tier = rem / ROWS, row = rem % ROWS;
    const float u = 11.4f + (bay + 0.5f) * 2.75f;
    return vec3(xStern + s * u, DECK_Y + 0.3f + 0.49f + tier * 0.98f, SHIP_Z + (row - (ROWS - 1) * 0.5f) * 0.98f);
}

void DocksUniverse::newShip(int i, bool docked) {
    Ship& sh = ships_[i];
    Rng r(seed_ ^ (uint64_t)(0x5419 + i * 7919 + sh.seed * 31));
    sh.seed = sh.seed + 1 + (int)r.irange(0, 50);
    const int n = BAYS * ROWS * TIERS;
    sh.slot.assign(n, -1);
    sh.landT.assign(n, 99.f);
    sh.order.clear();
    sh.reserved = 0;
    sh.hull = palT_.hull[r.irange(0, 2)];
    const vec3 bands[4] = {vec3(0.75f, 0.08f, 0.05f), vec3(0.05f, 0.25f, 0.6f), vec3(0.85f, 0.55f, 0.05f), vec3(0.9f, 0.9f, 0.88f)};
    sh.band = bands[r.irange(0, 3)];
    // the ship comes in with some cargo already on board, toward its bow
    const int preBays = r.irange(3, 5);
    for (int b = BAYS - 1; b >= BAYS - preBays; b--)
        for (int row = 0; row < ROWS; row++) {
            const int h = (b == BAYS - 1) ? r.irange(1, 2) : r.irange(2, TIERS);
            for (int t = 0; t < h; t++) sh.slot[b * ROWS * TIERS + t * ROWS + row] = 0;
        }
    for (int k = 0; k < n; k++)
        if (sh.slot[k] >= 0) sh.slot[k] = (signed char)r.irange(0, 9);
    // the free slots fill from the deck house toward the bow, a tier at a time across each bay
    for (int b = 0; b < BAYS; b++)
        for (int t = 0; t < TIERS; t++)
            for (int k = 0; k < ROWS; k++) {
                const int row = (k % 2) ? (ROWS / 2 + (k + 1) / 2) : (ROWS / 2 - k / 2);
                const int idx = b * ROWS * TIERS + t * ROWS + std::clamp(row, 0, ROWS - 1);
                if (sh.slot[idx] < 0 && std::find(sh.order.begin(), sh.order.end(), idx) == sh.order.end()) sh.order.push_back(idx);
            }
    const float berth = shipBerthX(i);
    const float s = i == 0 ? -1.f : 1.f; // the outward side
    if (docked) {
        sh.state = 0;
        sh.x = berth;
    } else {
        sh.state = 2;
        sh.t = 0.f;
        sh.x = berth + s * 150.f;
    }
}

// The next free slot of a ship (-1 if none): reserved for a container on its way.
int DocksUniverse::reserveSlot(int i) {
    Ship& sh = ships_[i];
    if (sh.state != 0 || sh.reserved >= (int)sh.order.size()) return -1;
    return sh.order[sh.reserved++];
}

void DocksUniverse::buildShip(std::vector<Box>& out, int i) {
    const Ship& sh = ships_[i];
    if (sh.state == 3) return;
    const float s = i == 0 ? 1.f : -1.f;
    const float xStern = sh.x - s * SHIP_LEN * 0.5f;
    auto U = [&](float u) { return xStern + s * u; };
    auto span = [&](float u0, float u1, float y0, float y1, float hz, const vec3& col, int mat, float p0 = 0, float seed = 0) {
        box(out, vec3(U((u0 + u1) * 0.5f), (y0 + y1) * 0.5f, SHIP_Z), vec3((u1 - u0) * 0.5f, (y1 - y0) * 0.5f, hz), col, mat, p0, 0, 0, seed);
    };
    const float bottom = Y_W - 3.f;
    const vec3 hull = sh.hull;
    span(0.f, BOW_U, bottom, DECK_Y, SHIP_HALF_B, hull, M_HULL, 0.7f);
    // the sheer line: a pale stripe along the deck's edge
    box(out, vec3(U(BOW_U * 0.5f), DECK_Y - 0.25f, SHIP_Z + SHIP_HALF_B + 0.02f), vec3(BOW_U * 0.5f, 0.09f, 0.02f), lerp(sh.band, vec3(0.9f), 0.5f), M_STEEL);
    // the bow: a raised forecastle narrowing to the stem, its sides two slanted plates
    const float fc = DECK_Y + 1.1f;
    span(BOW_U, BOW_U + 5.f, bottom, fc, 5.5f, hull, M_HULL, 0.7f);
    span(BOW_U + 5.f, BOW_U + 8.5f, bottom, fc, 4.2f, hull, M_HULL, 0.7f);
    span(BOW_U + 8.5f, BOW_U + 10.5f, bottom, fc, 2.6f, hull, M_HULL, 0.7f);
    span(BOW_U + 10.5f, SHIP_LEN - 0.4f, bottom, fc, 1.1f, hull, M_HULL, 0.7f);
    for (int side = -1; side <= 1; side += 2) {
        const vec3 a(U(BOW_U - 1.f), 0.f, SHIP_Z + side * SHIP_HALF_B), b(U(SHIP_LEN), 0.f, SHIP_Z + side * 0.2f);
        const vec3 d = b - a;
        const float len = length(d);
        const vec3 dir = d / len;
        const vec3 n = normalize(cross(dir, vec3(0, 1, 0))) * (float)side * s;
        const vec3 c = (a + b) * 0.5f + n * -0.12f;
        obox(out, vec3(c.x, (bottom + fc) * 0.5f, c.z), dir * (len * 0.5f), vec3(0, (fc - bottom) * 0.5f, 0), n * 0.12f, hull, M_HULL, 0.7f);
    }
    // the hatch covers
    span(10.6f, BOW_U + 1.f, DECK_Y, DECK_Y + 0.3f, SHIP_HALF_B - 0.4f, vec3(0.22f, 0.26f, 0.24f), M_DECK);
    // the deck house, its bridge wings, the funnel with the line's band, a mast
    const vec3 white(0.82f, 0.83f, 0.82f);
    span(2.5f, 9.5f, DECK_Y, DECK_Y + 8.8f, 5.2f, white, M_HOUSE, 0.f, (float)sh.seed);
    span(7.6f, 9.9f, DECK_Y + 8.2f, DECK_Y + 9.2f, SHIP_HALF_B + 0.6f, white, M_HOUSE, 0.f, (float)sh.seed + 3.f);
    span(2.4f, 9.6f, DECK_Y + 9.2f, DECK_Y + 9.4f, 5.3f, vec3(0.3f), M_STEEL);
    span(0.6f, 3.0f, DECK_Y + 3.f, DECK_Y + 10.6f, 1.4f, hull * 0.9f + vec3(0.02f), M_STEEL);
    span(0.6f, 3.0f, DECK_Y + 9.3f, DECK_Y + 10.4f, 1.45f, sh.band, M_STEEL);
    span(0.6f, 3.0f, DECK_Y + 10.6f, DECK_Y + 11.3f, 1.45f, vec3(0.03f), M_DARK);
    box(out, vec3(U(6.f), DECK_Y + 10.9f, SHIP_Z), vec3(0.08f, 1.5f, 0.08f), vec3(0.4f), M_STEEL);
    // a lashing bridge between every two bays
    for (int b = 0; b <= BAYS; b += 2) {
        const float u = 11.4f + b * 2.75f;
        span(u - 0.1f, u + 0.1f, DECK_Y + 0.3f, DECK_Y + 2.3f, SHIP_HALF_B - 0.5f, vec3(0.3f, 0.32f, 0.3f), M_STEEL);
    }
    // the cargo
    for (size_t k = 0; k < sh.slot.size(); k++) {
        if (sh.slot[k] < 0) continue;
        vec3 p = slotPos(i, (int)k);
        // a container just set down settles
        const float lt = sh.landT[k];
        if (lt < 0.5f) p.y += 0.12f * std::exp(-lt * 8.f) * std::cos(lt * 22.f);
        box(out, p, vec3(1.27f, 0.47f, 0.46f), cargoColor(sh.slot[k]), M_BOX, 1.f, 0, 0, hashf((uint64_t)k * 31 + sh.seed));
    }
}
