// GROCERIES: the store and the world around it, as instanced boxes (see groceries.cpp). Built once per song and
// again whenever the lines change: everything that grows carries its birth (in lines), so it pops into the world
// as the growth shown passes it, without the buffers being rebuilt every frame.
#include <algorithm>
#include <cmath>

#include "universes/groceries/groceries.hpp"
#include "universes/groceries/world.hpp"

using namespace groc;

namespace {
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }
vec3 hex(unsigned v) { return ugl::toLinear(vec3(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f)); }
// When a lane opens (lines).
float laneOpenL(int k) { return k == 0 ? -1.f : 4.f + 4.f * k; }
} // namespace

// =====================================================================================================
// Helpers
// =====================================================================================================

void GroceriesUniverse::box(std::vector<Box>& out, const vec3& c, const vec3& h, const vec3& col, int mat, float p0, float p1,
                            float p2, float seed, float birth, int group, float span, int channel) {
    Box b;
    b.c[0] = c.x; b.c[1] = c.y; b.c[2] = c.z; b.c[3] = (float)mat;
    b.x[0] = h.x; b.x[1] = 0; b.x[2] = 0; b.x[3] = p0;
    b.y[0] = 0; b.y[1] = h.y; b.y[2] = 0; b.y[3] = p1;
    b.z[0] = 0; b.z[1] = 0; b.z[2] = h.z; b.z[3] = p2;
    b.col[0] = col.x; b.col[1] = col.y; b.col[2] = col.z; b.col[3] = seed;
    b.e[0] = birth; b.e[1] = (float)channel; b.e[2] = span; b.e[3] = (float)group;
    out.push_back(b);
}

void GroceriesUniverse::obox(std::vector<Box>& out, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col,
                             int mat, float p0, float p1, float p2, float seed, float birth, int group, float span, int channel) {
    Box b;
    b.c[0] = c.x; b.c[1] = c.y; b.c[2] = c.z; b.c[3] = (float)mat;
    b.x[0] = ax.x; b.x[1] = ax.y; b.x[2] = ax.z; b.x[3] = p0;
    b.y[0] = ay.x; b.y[1] = ay.y; b.y[2] = ay.z; b.y[3] = p1;
    b.z[0] = az.x; b.z[1] = az.y; b.z[2] = az.z; b.z[3] = p2;
    b.col[0] = col.x; b.col[1] = col.y; b.col[2] = col.z; b.col[3] = seed;
    b.e[0] = birth; b.e[1] = (float)channel; b.e[2] = span; b.e[3] = (float)group;
    out.push_back(b);
}

void GroceriesUniverse::blob(std::vector<Blob>& out, const vec3& c, float hx, float hz, float a, float soft, float birth) {
    Blob b;
    b.p[0] = c.x; b.p[1] = c.y; b.p[2] = c.z; b.p[3] = a;
    b.r[0] = hx; b.r[1] = hz; b.r[2] = soft; b.r[3] = birth;
    out.push_back(b);
}

int GroceriesUniverse::fabricStyle(float h, vec3& fabric) const {
    // mostly kraft paper, some reusable fabric bags, a few white paper ones
    fabric = palT_.fabric[(int)(h * 997.f) % 4];
    if (h < 0.55f) return 0;
    if (h < 0.88f) return 1;
    return 2;
}

// A bag standing on `base` (its foot), turned by yaw (and tipped by tilt): its handles, the goods showing at the top.
void GroceriesUniverse::buildBag(std::vector<Box>& out, const vec3& base, float yaw, float s, int style, const vec3& fabric,
                                 float fill, float seed, float birth, int channel, bool handles, float tilt) {
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float ct = std::cos(tilt), st = std::sin(tilt);
    vec3 ax(cy * BAG_SX * s, 0.f, -sy * BAG_SX * s), ay(0.f, BAG_SY * s, 0.f), az(sy * BAG_SZ * s, 0.f, cy * BAG_SZ * s);
    if (tilt != 0.f) {
        // tip it over its x axis
        const vec3 ny = ay * ct + az * (st * BAG_SY / BAG_SZ), nz = az * ct - ay * (st * BAG_SZ / BAG_SY);
        ay = ny;
        az = nz;
    }
    const vec3 c = base + ay;
    const vec3 col = style == 1 ? fabric : palT_.kraft;
    obox(out, c, ax, ay, az, col, M_BAG, (float)style, fill, 0.f, seed, birth, -1, 0.8f, channel);
    if (!handles) return;
    // two handles: a thin strap up and over, on the long sides
    const vec3 hc = style == 1 ? fabric * 0.8f : palT_.kraft * 0.85f;
    const vec3 up = normalize(ay), side = normalize(ax), fwd = normalize(az);
    for (int k = -1; k <= 1; k += 2) {
        const vec3 hb = c + up * (BAG_SY * s) + fwd * (k * BAG_SZ * s * 0.55f);
        const float hh = 0.9f * s;
        for (int j = -1; j <= 1; j += 2)
            obox(out, hb + side * (j * BAG_SX * s * 0.45f) + up * hh * 0.5f, side * 0.07f * s, up * hh * 0.5f, fwd * 0.07f * s, hc, M_PAINT,
                 0, 0, 0, seed, birth, -1, 0.8f, channel);
        obox(out, hb + up * hh, side * (BAG_SX * s * 0.45f + 0.07f * s), up * 0.07f * s, fwd * 0.07f * s, hc, M_PAINT, 0, 0, 0, seed, birth,
             -1, 0.8f, channel);
    }
}

// A shopping cart on the floor at p, heading along yaw, with `bags` bags in its basket.
void GroceriesUniverse::buildCart(std::vector<Box>& out, std::vector<Blob>* blobs, const vec3& p, float yaw, int bags, float seed,
                                  float birth, int channel, bool lod) {
    const Palette& P = palT_;
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const vec3 fx(cy, 0.f, -sy), fz(sy, 0.f, cy), up(0.f, 1.f, 0.f);
    auto at = [&](float x, float y, float z) { return p + fx * x + up * y + fz * z; };
    const vec3 wire = P.cart;
    const vec3 handle = P.accent;
    // the basket (a wire box, open on top)
    obox(out, at(0.6f, 7.6f, 0.f), fx * 4.4f, up * 2.3f, fz * 2.8f, wire, M_WIRE, 0, 0, 0, seed, birth, -1, 0.8f, channel);
    // the handle bar at the back
    obox(out, at(-4.5f, 10.4f, 0.f), fx * 0.25f, up * 0.25f, fz * 2.9f, handle, M_PAINT, 0.3f, 0, 0, seed, birth, -1, 0.8f, channel);
    if (!lod) {
        // the frame, the lower tray, the wheels
        for (int s = -1; s <= 1; s += 2) {
            obox(out, at(-3.9f, 9.0f, s * 2.6f), fx * 0.12f, up * 1.4f, fz * 0.12f, wire, M_STEEL, 0, 0, 0, seed, birth, -1, 0.8f, channel);
            obox(out, at(0.f, 3.2f, s * 2.2f), fx * 4.2f, up * 0.12f, fz * 0.12f, wire, M_STEEL, 0, 0, 0, seed, birth, -1, 0.8f, channel);
            obox(out, at(-3.4f, 3.1f, s * 2.4f), fx * 0.12f, up * 2.5f, fz * 0.12f, wire, M_STEEL, 0, 0, 0, seed, birth, -1, 0.8f, channel);
            for (int f = -1; f <= 1; f += 2)
                obox(out, at(f * 3.6f, 0.55f, s * 2.1f), fx * 0.55f, up * 0.55f, fz * 0.25f, vec3(0.05f), M_PAINT, 0, 0, 0, seed, birth, -1,
                     0.8f, channel);
        }
        obox(out, at(0.f, 2.4f, 0.f), fx * 3.6f, up * 0.12f, fz * 2.0f, wire, M_WIRE, 0, 0, 0, seed, birth, -1, 0.8f, channel);
    } else {
        obox(out, at(0.f, 2.4f, 0.f), fx * 4.f, up * 2.4f, fz * 2.2f, wire * 0.6f, M_STEEL, 0, 0, 0, seed, birth, -1, 0.8f, channel);
    }
    // the bags inside, standing in two rows
    for (int i = 0; i < bags && i < 6; i++) {
        const int col = i % 3, row = i / 3;
        const float h = hashf((uint64_t)(seed * 1000.f) * 7 + i);
        vec3 fab;
        const int stl = fabricStyle(h, fab);
        buildBag(out, at(-2.6f + col * 2.9f, 5.5f, (row ? 1.f : -1.f) * 1.25f), yaw + (h - 0.5f) * 0.3f, 0.82f, stl, fab, 0.6f + 0.4f * h,
                 h * 10.f, birth, channel, !lod, 0.f);
    }
    if (blobs) blob(*blobs, vec3(p.x, Y_FLOOR + 0.06f, p.z), 5.2f, 3.6f, 0.45f, 0.6f, channel == 0 ? birth : 0.f);
}

// A heap of bags around c (on the floor): layers of loosely set bags, each popping in at its birth.
void GroceriesUniverse::buildPile(const vec3& c, int n, float seed, float birth0, float perLine, int channel) {
    int i = 0;
    const int layers[5][2] = {{4, 3}, {3, 3}, {3, 2}, {2, 1}, {1, 1}};
    for (int L = 0; L < 5 && i < n; L++) {
        const int nx = layers[L][0], nz = layers[L][1];
        for (int zi = 0; zi < nz && i < n; zi++)
            for (int xi = 0; xi < nx && i < n; xi++, i++) {
                const float h = hashf((uint64_t)(seed * 7777.f) + i * 31);
                const vec3 p = c + vec3((xi - (nx - 1) * 0.5f) * 3.6f + (h - 0.5f) * 1.2f, L * 3.9f, (zi - (nz - 1) * 0.5f) * 2.6f + (hashf(i * 13 + 5) - 0.5f) * 1.f);
                vec3 fab;
                const int stl = fabricStyle(h, fab);
                const float b = birth0 >= 0.f ? birth0 + i / perLine : 0.f;
                buildBag(static_, p, (h - 0.5f) * 0.9f, 1.f, stl, fab, 0.5f + 0.5f * h, h * 10.f, b, channel, L == 0 || h > 0.5f,
                         L > 0 ? (hashf(i * 7 + 3) - 0.5f) * 0.35f : 0.f);
            }
    }
    // beyond the heap: a second one beside it
    if (i < n) buildPile(c + vec3(-16.f, 0.f, 3.f), n - i, seed + 1.3f, birth0 >= 0.f ? birth0 + i / perLine : -1.f, perLine, channel);
    blob(staticBlobs_, vec3(c.x, Y_FLOOR + 0.06f, c.z), 9.f, 6.f, 0.4f, 0.6f, birth0 >= 0.f ? birth0 : 0.f);
}

// A mountain of bags: rings of bags on a cone (two rows a ring, tipped outward as on a heap), its height from the
// growth (the rings widen with it), a dark core inside so that no gap shows through.
void GroceriesUniverse::buildMountain(const vec3& c, float H, float seed, float birth0, float perLine) {
    if (H < 8.f) return;
    const float s = 1.6f * std::max(1.f, H / 70.f);
    const float dh = 2.5f * s;
    const float slope = 1.0f;
    const float topBirth = birth0 + H / perLine;
    int ring = 0;
    for (float h = 0.f; h < H - dh * 0.5f; h += dh, ring++) {
        const float R = (H - h) * slope;
        const float b = birth0 + h / perLine;
        const float b2 = std::max(birth0, std::min(b, topBirth - 0.01f));
        for (int row = 0; row < 2; row++) {
            const float rr0 = R - row * 2.2f * s;
            if (rr0 < s) continue;
            const int n = std::max(3, (int)(TAU * rr0 / (BAG_SX * 2.f * s * 0.8f)));
            for (int i = 0; i < n; i++) {
                const float hh = hashf((uint64_t)(seed * 1000.f) * 131 + ring * 977 + row * 7919 + i);
                const float a = (i + hh * 0.5f + row * 0.5f) / n * TAU + ring * 0.37f;
                const float rr = rr0 + (hh - 0.5f) * s * 1.2f;
                const vec3 p = c + vec3(std::cos(a) * rr, h + row * 0.8f * s, std::sin(a) * rr);
                vec3 fab;
                const int stl = fabricStyle(hh, fab);
                buildBag(static_, p, -a + PI * 0.5f + (hh - 0.5f) * 0.9f, s, stl, fab, 0.7f + 0.3f * hh, hh * 10.f, b2, 0, false,
                         0.3f + 0.3f * hh);
            }
        }
        // the core: two crossed slabs (an octagon) in the bags' shade, under the ring
        if (R > 4.f * s) {
            const float cr = R - 3.2f * s;
            for (int k = 0; k < 2; k++) {
                const float a = k * PI * 0.25f;
                obox(static_, c + vec3(0.f, h + dh * 0.6f, 0.f), vec3(std::cos(a), 0.f, std::sin(a)) * (cr * 0.93f), vec3(0.f, dh * 0.6f, 0.f),
                     vec3(-std::sin(a), 0.f, std::cos(a)) * (cr * 0.93f), palT_.kraft * 0.3f, M_PAINT, 0, 0, 0, seed + ring, b2, -1, 0.8f);
            }
        }
    }
    vec3 fab;
    buildBag(static_, c + vec3(0.f, H - dh * 0.6f, 0.f), seed, s * 1.1f, 0, fab, 1.f, seed, topBirth - 0.01f, 0, false, 0.f);
    blob(staticBlobs_, vec3(c.x, Y_FLOOR + 0.1f, c.z), H * slope * 1.2f, H * slope * 1.2f, 0.45f, 0.5f, birth0);
}

// The cart city behind the store: blocks of towers of stacked carts (each tower drawn as stacked baskets by its
// material), rising segment by segment with the lines.
void GroceriesUniverse::buildCartCity(float L) {
    if (L < 92.f) return;
    const float L0 = 95.f;
    Rng r(seed_ ^ 0xCA27ull);
    for (int bz = 0; bz < 8; bz++)
        for (int bx = -7; bx <= 7; bx++) {
            const float x = bx * 330.f + (bz % 2) * 80.f, z = -1150.f - bz * 330.f;
            // the blocks nearest the store first
            const float rank = std::fabs((float)bx) * 0.7f + bz * 1.2f;
            const float birth = L0 + rank * 4.5f;
            if (birth > L + 6.f) continue;
            box(static_, vec3(x, Y_FLOOR + 0.3f, z), vec3(135.f, 0.3f, 135.f), lerp(palT_.asphalt, vec3(0.8f), 0.5f), M_PAINT, 0, 0, 0, 0, birth, -1, 1.f);
            for (int t = 0; t < 4; t++) {
                const float hh = r.uniform();
                const vec3 tp(x + (t % 2 ? 62.f : -62.f) + r.range(-8.f, 8.f), Y_FLOOR, z + (t / 2 ? 62.f : -62.f) + r.range(-8.f, 8.f));
                const float layer = 15.f;
                const int layers = 6 + (int)(hh * hh * 44.f) + (rank < 4.f ? 6 : 0);
                const vec3 col = lerp(palT_.cart, palT_.confetti[(t + bx * 3 + bz) % 5], 0.65f);
                const float w = r.range(38.f, 52.f), d = r.range(26.f, 36.f);
                // in four segments, each popping up in turn
                const int seg = 4;
                for (int k = 0; k < seg; k++) {
                    const int l0 = layers * k / seg, l1 = layers * (k + 1) / seg;
                    if (l1 <= l0) continue;
                    const float y0 = Y_FLOOR + l0 * layer, y1 = Y_FLOOR + l1 * layer;
                    box(static_, vec3(tp.x, (y0 + y1) * 0.5f, tp.z), vec3(w * 0.5f, (y1 - y0) * 0.5f, d * 0.5f), col, M_CARTS, layer, 0, 0, hh,
                        birth + 0.6f + k * 1.4f + hh * 2.f, -1, 1.2f);
                }
                blob(staticBlobs_, vec3(tp.x, Y_FLOOR + 0.7f, tp.z), w * 0.8f, d * 0.8f, 0.45f, 0.6f, birth);
            }
        }
}

// A hypermarket wing: a copy of the store, simplified (no ceiling: the camera is high above by then).
void GroceriesUniverse::buildModule(float x0, float birth, bool simple) {
    (void)simple;
    const Palette& P = palT_;
    const float zc = (Z_BACK + Z_FRONT) * 0.5f, hz = (Z_FRONT - Z_BACK) * 0.5f;
    box(static_, vec3(x0, Y_FLOOR - 0.5f, zc), vec3(X_WALL, 0.5f, hz), P.floor, M_FLOOR, 0.3f, 0, 0, 0, birth, G_FLOOR, 1.5f);
    // outer walls
    const float side = x0 > 0 ? 1.f : -1.f;
    box(static_, vec3(x0 + side * (X_WALL + 1.f), (Y_FLOOR + Y_CEIL + 6.f) * 0.5f, zc), vec3(1.f, (Y_CEIL + 6.f - Y_FLOOR) * 0.5f, hz),
        P.wallOut, M_WALL, 0.f, 0, 0, 0, birth, -1, 1.5f);
    box(static_, vec3(x0, (Y_FLOOR + Y_CEIL + 6.f) * 0.5f, Z_BACK - 1.f), vec3(X_WALL, (Y_CEIL + 6.f - Y_FLOOR) * 0.5f, 1.f), P.wallOut,
        M_WALL, 0.f, 0, 0, 0, birth, -1, 1.5f);
    for (int i = 0; i <= 10; i++)
        box(static_, vec3(x0 - X_WALL + i * MODULE_W / 10.f, (Y_FLOOR + Y_CEIL) * 0.5f, Z_FRONT), vec3(0.5f, (Y_CEIL - Y_FLOOR) * 0.5f, 0.5f),
            P.steel, M_STEEL, 0, 0, 0, 0, birth, -1, 1.5f);
    // lanes
    for (int k = 0; k < 8; k++) {
        const float z = -k * LANE_DZ;
        const float b = birth + 0.05f * k;
        box(static_, vec3(x0, (Y_FLOOR + Y_CT) * 0.5f, z - 0.5f), vec3(33.f, (Y_CT - Y_FLOOR) * 0.5f, 3.6f), P.counter, M_PAINT, 0.15f, 0, 0, 0,
            b, -1, 1.f);
        box(static_, vec3(x0 + 21.f, Y_CT + 0.1f, z - 0.7f), vec3(12.f, 0.15f, 2.3f), P.belt, M_BELT, -2.f, 0, 0, 0, b, -1, 1.f);
        box(static_, vec3(x0 + 19.f, 6.8f, z - 4.6f), vec3(1.5f, 1.8f, 0.45f), P.accent, M_SIGN, 1.f, 0, (float)(k + 9), 0, b, G_SIGN, 1.f);
        box(static_, vec3(x0 + 19.f, (Y_CT + 5.f) * 0.5f, z - 4.6f), vec3(0.25f, (5.f - Y_CT) * 0.5f, 0.25f), P.steel, M_STEEL, 0, 0, 0, 0, b,
            -1, 1.f);
        // a heap of bags at every lane's end
        for (int j = 0; j < 6; j++) {
            const float h = hashf((uint64_t)(x0 * 3.f) + k * 17 + j);
            vec3 fab;
            const int stl = fabricStyle(h, fab);
            buildBag(static_, vec3(x0 - 44.f - (j % 3) * 3.7f, Y_FLOOR + (j / 3) * 3.9f, z - 8.f + (j % 2) * 2.f), h, 1.f, stl, fab, 0.8f, h * 10.f,
                     b + 0.2f + j * 0.1f, 0, false);
        }
    }
    // aisles and the fridges at the back
    for (int i = 0; i < 13; i++) {
        const float x = x0 - 204.f + 34.f * i;
        box(static_, vec3(x, Y_FLOOR + 11.f, (Z_AISLE0 + Z_AISLE1) * 0.5f), vec3(5.f, 11.f, (Z_AISLE0 - Z_AISLE1) * 0.5f), P.gondola, M_SHELF,
            P.wood, (float)i, 0, (float)i * 0.1f, birth + 0.3f + 0.04f * i, -1, 1.f);
    }
    box(static_, vec3(x0, Y_FLOOR + 11.f, Z_BACK + 5.f), vec3(X_WALL - 2.f, 11.f, 4.f), vec3(0.8f), M_SHELF, 2.f, 3.f, 0, 0, birth + 0.6f,
        G_FRIDGE, 1.f);
    blob(staticBlobs_, vec3(x0, Y_FLOOR + 0.07f, zc), X_WALL + 6.f, hz + 6.f, 0.25f, 0.2f, birth);
}

// The car park in front of the store: bays, parked cars, lamp posts, hedges, the cart shelters.
void GroceriesUniverse::buildCarPark(float halfW) {
    const Palette& P = palT_;
    box(static_, vec3(0.f, Y_FLOOR - 0.6f, (Z_FRONT + CAR_Z1) * 0.5f), vec3(halfW, 0.6f, (CAR_Z1 - Z_FRONT) * 0.5f), P.asphalt, M_ASPHALT);
    box(static_, vec3(0.f, Y_FLOOR + 0.25f, Z_FRONT + 11.f), vec3(halfW, 0.25f, 11.f), lerp(P.asphalt, vec3(0.7f), 0.6f), M_PAINT);
    // the roads: one along the front, one each side
    box(static_, vec3(0.f, Y_FLOOR - 0.4f, CAR_Z1 + 32.f), vec3(9000.f, 0.4f, 22.f), P.asphalt * 0.8f, M_ASPHALT, 1.f);
    for (int s = -1; s <= 1; s += 2)
        box(static_, vec3(s * (halfW + 40.f), Y_FLOOR - 0.45f, -1500.f), vec3(20.f, 0.4f, 2200.f), P.asphalt * 0.8f, M_ASPHALT, 1.f);
    Rng r(seed_ ^ 0xCA2Full);
    const vec3 carCols[8] = {hex(0xe8e8e4), hex(0xb8bcc2), hex(0x5a6068), hex(0x1e2226), hex(0x2a3a5a), hex(0x7a2a2a), hex(0x6a7a62), hex(0xd8c8a8)};
    // the bays: rows across the car park; some taken
    for (int row = 0; row < 8; row++) {
        const float base = 100.f + 140.f * (row / 2);
        const float zr = (row % 2 == 0) ? base + 24.f : base + 116.f;
        if (zr > CAR_Z1 - 20.f) break;
        for (float x = -halfW + 13.f; x < halfW - 13.f; x += 26.f) {
            if (std::fabs(x) < 70.f) continue;            // the walkway to the entrance
            if (!r.chance(0.42f)) continue;
            const vec3 col = carCols[r.irange(0, 7)];
            const float y0 = Y_FLOOR;
            box(static_, vec3(x, y0 + 3.4f, zr), vec3(8.6f, 2.6f, 20.f), col, M_PAINT, 0.5f, 0, 0, r.uniform());
            box(static_, vec3(x, y0 + 8.f, zr + (row % 2 ? 2.f : -2.f)), vec3(7.8f, 2.2f, 10.f), col, M_VAN, 1.f, 0, 1.f, r.uniform());
            blob(staticBlobs_, vec3(x, Y_FLOOR + 0.05f, zr), 10.f, 22.f, 0.5f, 0.5f);
        }
        // a hedge at each row's ends
        if (row % 2 == 1)
            for (int s = -1; s <= 1; s += 2)
                box(static_, vec3(s * (halfW - 6.f), Y_FLOOR + 2.5f, base + 70.f), vec3(5.f, 2.5f, 60.f), P.grass * 0.7f, M_GRASS);
    }
    // lamp posts
    for (float x = -halfW + 65.f; x < halfW; x += 130.f)
        for (float z = Z_FRONT + 63.f; z < CAR_Z1; z += 140.f) {
            box(static_, vec3(x, Y_FLOOR + 27.f, z), vec3(0.6f, 27.f, 0.6f), P.steel * 0.8f, M_STEEL);
            box(static_, vec3(x, Y_FLOOR + 54.f, z), vec3(3.2f, 0.6f, 1.4f), P.lamp, M_EMIT, 1.2f, 0, 0, 0, 0, G_LAMPS);
            lamps_.push_back({vec3(x, Y_FLOOR + 53.f, z), P.lamp, 6.f, 1.f, G_LAMPS});
        }
    // the cart shelters
    for (int i = 0; i < 6; i++) {
        const float x = (i % 2 ? 1.f : -1.f) * (120.f + 160.f * (i / 2)), z = Z_FRONT + 190.f + 140.f * (i / 2) * 0.f;
        if (std::fabs(x) > halfW - 40.f) continue;
        box(static_, vec3(x, Y_FLOOR + 13.f, z), vec3(20.f, 0.4f, 7.f), P.accent, M_PAINT, 0.3f);
        for (int s = -1; s <= 1; s += 2)
            box(static_, vec3(x + s * 19.f, Y_FLOOR + 6.5f, z), vec3(0.4f, 6.5f, 0.4f), P.steel, M_STEEL);
        for (int c = 0; c < 6; c++)
            buildCart(static_, &staticBlobs_, vec3(x - 12.f + c * 4.8f, Y_FLOOR, z), PI * 0.5f, 0, 0.3f + c, 0.f, 0, true);
    }
}

// The land around: a lawn by the store, then a patchwork of fields (drawn by the ground's material) and their
// hedgerows, a few farms.
void GroceriesUniverse::buildTown() {
    const Palette& P = palT_;
    box(static_, vec3(0.f, Y_FLOOR - 1.6f, 0.f), vec3(16000.f, 0.8f, 16000.f), P.grass, M_GRASS, 1.f);
    Rng r(seed_ ^ 0x70A7ull);
    const vec3 walls[4] = {hex(0xe8e0d0), hex(0xd8d2c8), hex(0xc8b8a0), hex(0xe0d8cc)};
    const vec3 roofs[3] = {hex(0xa8483a), hex(0x5a5e66), hex(0x8a5a3a)};
    for (int i = 0; i < 26; i++) {
        const float a = r.range(0.f, TAU), d = r.range(2600.f, 7000.f);
        const float x = std::cos(a) * d * 1.3f, z = std::sin(a) * d;
        if (z < -900.f && std::fabs(x) < 2800.f) continue;
        // a farm: a house and a barn
        for (int b = 0; b < 2; b++) {
            const float w = b ? 70.f : 40.f, dz = b ? 40.f : 30.f, h = b ? 30.f : 22.f;
            const vec3 c(x + b * 70.f, Y_FLOOR + h * 0.5f, z + b * 20.f);
            box(static_, c, vec3(w * 0.5f, h * 0.5f, dz * 0.5f), walls[r.irange(0, 3)], M_PAINT);
            box(static_, c + vec3(0.f, h * 0.5f + 4.f, 0.f), vec3(w * 0.52f, 4.f, dz * 0.4f), roofs[r.irange(0, 2)], M_PAINT, 0.1f);
        }
    }
}

// The store's position in the queue of full carts (ours): back along the left, then rows behind.
vec3 GroceriesUniverse::queueSlot(int i) const {
    if (i < 12) return vec3(-40.f - 1.8f * i, Y_FLOOR, -26.f - 12.f * i);
    const int j = i - 12;
    return vec3(-58.f - 10.5f * (j % 4), Y_FLOOR, -30.f - 12.f * (j / 4));
}

// =====================================================================================================
// A checkout lane
// =====================================================================================================

void GroceriesUniverse::buildLane(int k, bool open, float openL) {
    const Palette& P = palT_;
    const float zc = -k * LANE_DZ;
    const float L = (float)lines_;
    const float xm = (LANE_X0 + LANE_X1) * 0.5f, hx = (LANE_X1 - LANE_X0) * 0.5f;
    // the counter: a laminate body, a steel top, a dark kick plate
    box(static_, vec3(xm, (Y_FLOOR + Y_CT - 0.3f) * 0.5f, zc - 0.5f), vec3(hx, (Y_CT - 0.3f - Y_FLOOR) * 0.5f, 3.5f), P.counter, M_PAINT, 0.12f);
    box(static_, vec3(xm, Y_CT - 0.15f, zc - 0.5f), vec3(hx + 0.3f, 0.15f, 3.8f), P.steel, M_STEEL);
    box(static_, vec3(xm, Y_FLOOR + 0.5f, zc + 3.05f), vec3(hx - 0.5f, 0.5f, 0.1f), P.counter * 0.3f, M_PAINT);
    box(static_, vec3(xm, Y_CT - 1.3f, zc + 3.06f), vec3(hx - 0.2f, 0.75f, 0.1f), P.accent, M_PAINT, 0.25f);
    // the belt (it runs while the lane is open), its rails
    const float bx0 = 9.5f, bx1 = LANE_X1 - 1.f;
    box(static_, vec3((bx0 + bx1) * 0.5f, Y_CT + 0.08f, zc - 0.7f), vec3((bx1 - bx0) * 0.5f, 0.12f, 2.3f), P.belt, M_BELT, open ? -2.2f : 0.f);
    for (int s = -1; s <= 1; s += 2)
        box(static_, vec3((bx0 + bx1) * 0.5f, Y_CT + 0.3f, zc - 0.7f + s * 2.45f), vec3((bx1 - bx0) * 0.5f + 0.3f, 0.3f, 0.13f), P.steel, M_STEEL);
    // the scanner: a window in the counter and an upright one, glowing red
    box(static_, vec3(7.4f, Y_CT + 0.02f, zc - 0.7f), vec3(1.3f, 0.06f, 1.6f), vec3(0.05f), M_SCANNER, 1.f, 0, 0, 0, 0, G_SCAN);
    box(static_, vec3(7.4f, Y_CT + 2.2f, zc - 3.25f), vec3(1.5f, 2.2f, 0.55f), vec3(0.11f, 0.115f, 0.12f), M_PAINT, 0.4f);
    box(static_, vec3(7.4f, Y_CT + 2.5f, zc - 2.66f), vec3(1.05f, 1.3f, 0.05f), vec3(0.05f), M_SCANNER, 0.8f, 0, 0, 0, 0, G_SCAN);
    // the till on its pole, the card terminal
    box(static_, vec3(14.5f, Y_CT + 2.3f, zc - 3.4f), vec3(0.2f, 2.3f, 0.2f), P.steel, M_STEEL);
    box(static_, vec3(14.5f, Y_CT + 5.8f, zc - 3.3f), vec3(2.f, 1.4f, 0.28f), vec3(0.09f, 0.095f, 0.1f), M_TILL, 0, 0, 0, 0.3f + k, 0, G_TILL);
    box(static_, vec3(4.6f, Y_CT + 1.3f, zc + 2.4f), vec3(0.08f, 1.3f, 0.08f), P.steel, M_STEEL);
    box(static_, vec3(4.6f, Y_CT + 2.8f, zc + 2.4f), vec3(0.75f, 0.3f, 1.0f), vec3(0.1f), M_TILL, 0, 0, 0, 0.7f + k, 0, G_TILL);
    // the lane's number on its pole
    box(static_, vec3(19.f, (Y_CT + 5.f) * 0.5f, zc - 4.6f), vec3(0.25f, (5.f - Y_CT) * 0.5f, 0.25f), P.steel, M_STEEL);
    box(static_, vec3(19.f, 6.8f, zc - 4.6f), vec3(1.6f, 1.9f, 0.45f), P.accent, M_SIGN, 1.f, 0, (float)(k + 1), 0, 0, k);
    // the bag stand at the bagging end
    for (float x : {-23.5f, -8.5f}) box(static_, vec3(x, Y_CT + 2.6f, zc - 2.4f), vec3(0.14f, 2.6f, 0.14f), P.steel, M_STEEL);
    box(static_, vec3(-16.f, Y_CT + 5.2f, zc - 2.4f), vec3(7.6f, 0.14f, 0.14f), P.steel, M_STEEL);
    if (k > 0 && open) {
        const float h = hashf((uint64_t)k * 77);
        vec3 fab;
        const int stl = fabricStyle(h, fab);
        buildBag(static_, vec3(-16.f, Y_CT, zc - 0.8f), 0.f, 1.f, stl, fab, 0.4f + 0.5f * h, h * 10.f, openL + 0.3f);
    }
    blob(staticBlobs_, vec3(xm, Y_FLOOR + 0.05f, zc - 0.3f), hx + 2.f, 5.5f, 0.5f, 0.4f);
    if (k == 0) {
        // our basket, at the end of the counter
        box(static_, vec3(-28.5f, Y_CT + 1.5f, zc - 0.5f), vec3(2.6f, 1.5f, 1.8f), P.accent, M_WIRE);
        for (int s = -1; s <= 1; s += 2)
            box(static_, vec3(-28.5f + s * 1.6f, Y_CT + 3.2f, zc - 0.5f), vec3(0.15f, 0.25f, 1.6f), P.accent * 0.8f, M_PAINT, 0.3f);
        return;
    }
    if (!open) return;
    // a heap of bags on the floor at the lane's end, carts lining up
    const float rate = 1.5f;
    const int n = std::min(40, (int)((L + 3.f - openL) * rate));
    if (n > 0) buildPile(vec3(-112.f, Y_FLOOR, zc - 12.f), n, (float)k * 3.1f, openL + 0.5f, rate);
    const int nc = std::min(5, (int)((L + 3.f - openL) / 3.5f));
    for (int i = 0; i < nc; i++)
        buildCart(static_, &staticBlobs_, vec3(-140.f - 11.f * i, Y_FLOOR, zc - 14.f), 0.1f * (hashf(k * 9 + i) - 0.5f), 4 + i % 3,
                  k * 1.7f + i, openL + 3.5f * (i + 1));
}

// =====================================================================================================
// The ceiling (it lifts off as the camera rises): tiles, rows of tubes, the hanging signs
// =====================================================================================================

void GroceriesUniverse::buildRoof() {
    roof_.clear();
    const Palette& P = palT_;
    box(roof_, vec3(0.f, Y_CEIL + 0.6f, (Z_BACK + Z_FRONT) * 0.5f), vec3(X_WALL, 0.6f, (Z_FRONT - Z_BACK) * 0.5f), P.ceil, M_CEIL);
    for (float z = Z_BACK + 18.f; z < Z_FRONT; z += 30.f)
        for (float n = std::floor(-X_WALL / 16.f); n * 16.f < X_WALL; n += 1.f) {
            const float cx = 16.f * (n + 0.35f);
            if (std::fabs(cx) > X_WALL - 5.f) continue;
            box(roof_, vec3(cx, Y_CEIL - 0.25f, z), vec3(4.3f, 0.25f, 0.6f), P.tube, M_EMIT, 1.6f, 0, 0, 0, 0, G_TUBES);
            box(roof_, vec3(cx, Y_CEIL - 0.05f, z), vec3(4.6f, 0.08f, 0.95f), P.steel * 0.9f, M_STEEL);
        }
    // the aisle signs over the aisles, the department panels over the produce and the shelves on the right
    for (int j = 0; j < 12; j++) {
        const float x = -187.f + 34.f * j;
        const vec3 col = P.goods[j % 8] * 0.85f;
        box(roof_, vec3(x, 15.f, Z_AISLE0 + 6.f), vec3(9.f, 2.4f, 0.25f), col, M_SIGN, (float)(j % 6), 0, 0, 0.1f * j);
        box(roof_, vec3(x, 20.6f, Z_AISLE0 + 6.f), vec3(2.4f, 2.4f, 0.25f), P.accent, M_SIGN, 0.6f, 0, (float)(j + 1), 0, 0, G_SIGN);
        for (int s = -1; s <= 1; s += 2)
            box(roof_, vec3(x + s * 7.f, (17.4f + Y_CEIL) * 0.5f, Z_AISLE0 + 6.f), vec3(0.05f, (Y_CEIL - 17.4f) * 0.5f, 0.05f), vec3(0.3f), M_STEEL);
    }
    // price boards hanging over the produce, the pallets and the shelves on the right
    Rng r(seed_ ^ 0x9819ull);
    for (int i = 0; i < 14; i++) {
        const float side = i % 2 ? 1.f : -1.f;
        const float x = side * r.range(58.f, 200.f), z = r.range(-330.f, -30.f);
        box(roof_, vec3(x, 12.f, z), vec3(4.2f, 2.6f, 0.2f), P.accent2, M_SIGN, -1.f, 0, (float)r.irange(10, 99), r.uniform());
        box(roof_, vec3(x, (14.6f + Y_CEIL) * 0.5f, z), vec3(0.05f, (Y_CEIL - 14.6f) * 0.5f, 0.05f), vec3(0.3f), M_STEEL);
    }
    roofCount_ = roof_.size();
}

// =====================================================================================================
// The whole world
// =====================================================================================================

void GroceriesUniverse::buildWorld() {
    static_.clear();
    staticBlobs_.clear();
    lamps_.clear();
    const Palette& P = palT_;
    const float L = (float)lines_;
    Rng r(seed_ ^ 0x57A1ull);
    const float zc = (Z_BACK + Z_FRONT) * 0.5f, hz = (Z_FRONT - Z_BACK) * 0.5f;
    // ---- the floor, the walls (inside, outside), the glazed front
    box(static_, vec3(0.f, Y_FLOOR - 0.5f, zc), vec3(X_WALL, 0.5f, hz), P.floor, M_FLOOR, 1.f, 0, 0, 0, 0, G_FLOOR);
    for (int s = -1; s <= 1; s += 2) {
        box(static_, vec3(s * (X_WALL + 0.5f), (Y_FLOOR + Y_CEIL) * 0.5f, zc), vec3(0.5f, (Y_CEIL - Y_FLOOR) * 0.5f, hz), P.wall, M_WALL, 1.f);
        box(static_, vec3(s * (X_WALL + 1.5f), (Y_FLOOR + Y_CEIL + 6.f) * 0.5f, zc), vec3(0.5f, (Y_CEIL + 6.f - Y_FLOOR) * 0.5f, hz + 2.f),
            P.wallOut, M_WALL, 0.f);
    }
    box(static_, vec3(0.f, (Y_FLOOR + Y_CEIL) * 0.5f, Z_BACK - 0.5f), vec3(X_WALL, (Y_CEIL - Y_FLOOR) * 0.5f, 0.5f), P.wall, M_WALL, 1.f);
    box(static_, vec3(0.f, (Y_FLOOR + Y_CEIL + 6.f) * 0.5f, Z_BACK - 1.5f), vec3(X_WALL + 2.f, (Y_CEIL + 6.f - Y_FLOOR) * 0.5f, 0.5f),
        P.wallOut, M_WALL, 0.f);
    for (float x = -X_WALL; x <= X_WALL + 0.1f; x += MODULE_W / 16.f)
        box(static_, vec3(x, (Y_FLOOR + Y_CEIL) * 0.5f, Z_FRONT), vec3(0.45f, (Y_CEIL - Y_FLOOR) * 0.5f, 0.45f), P.steel, M_STEEL);
    box(static_, vec3(0.f, Y_CEIL + 1.5f, Z_FRONT), vec3(X_WALL + 2.f, 3.f, 0.8f), P.wallOut, M_WALL, 0.f);
    // ---- the checkout lanes
    for (int k = 0; k < LANES; k++) {
        const float oL = laneOpenL(k);
        buildLane(k, L >= oL, oL);
    }
    // ---- our queue of full carts
    for (int i = 0; i < cartsArrived_ && i < 64; i++) buildCart(static_, &staticBlobs_, queueSlot(i), 0.04f * std::sin(i * 1.7f), 6, 11.f + i);
    // ---- product pyramids on pallets between the lanes, on the right
    for (int g = 0; g < LANES; g++) {
        const float z = -(g + 0.5f) * LANE_DZ, x = 66.f + (g % 2) * 24.f;
        box(static_, vec3(x, Y_FLOOR + 0.7f, z), vec3(7.f, 0.7f, 6.f), P.counter * 0.0f + hex(0xc8a878), M_WOOD);
        const int fam = (g * 3 + 1) % 7;
        const float hs = 1.15f;
        int lay = 0;
        for (int nx = 5, nz = 4; nx > 0 && nz > 0; nx--, nz--, lay++)
            for (int a = 0; a < nx; a++)
                for (int b = 0; b < nz; b++)
                    box(static_, vec3(x + (a - (nx - 1) * 0.5f) * hs * 2.05f, Y_FLOOR + 1.4f + hs + lay * hs * 2.02f, z + (b - (nz - 1) * 0.5f) * hs * 2.05f),
                        vec3(hs), vec3(1.f), M_PRODUCT, (float)fam, 0, 0, r.uniform());
        box(static_, vec3(x + 9.f, Y_FLOOR + 6.f, z + 4.f), vec3(0.1f, 6.f, 0.1f), P.steel, M_STEEL);
        box(static_, vec3(x + 9.f, Y_FLOOR + 13.f, z + 4.f), vec3(2.6f, 1.6f, 0.12f), P.accent2, M_SIGN, -1.f, 0, (float)r.irange(10, 99), r.uniform());
        blob(staticBlobs_, vec3(x, Y_FLOOR + 0.05f, z), 8.f, 7.f, 0.5f, 0.5f);
    }
    // ---- the shelves on the right, facing us
    for (int j = 0; j < 8; j++) {
        const float z = -48.f - 36.f * j;
        box(static_, vec3(176.f, Y_FLOOR + 9.f, z), vec3(46.f, 9.f, 4.2f), P.gondola, M_SHELF, P.wood, 0, (float)j * 0.37f, 0.5f + j);
        blob(staticBlobs_, vec3(176.f, Y_FLOOR + 0.05f, z), 48.f, 7.f, 0.4f, 0.5f);
    }
    // ---- the produce on the left: tables of crates
    const vec3 fruit[8] = {hex(0xc8352e), hex(0x8cc04a), hex(0xf09a28), hex(0xf2d040), hex(0xd8482a), hex(0x5a2a5a), hex(0x3a8a3a), hex(0xe8c49a)};
    for (int j = 0; j < 9; j++) {
        const float z = -40.f - 32.f * j, x = -192.f;
        box(static_, vec3(x, Y_FLOOR + 5.f, z), vec3(26.f, 0.5f, 9.f), hex(0xb08860), M_WOOD);
        for (int s = -1; s <= 1; s += 2)
            box(static_, vec3(x + s * 24.f, Y_FLOOR + 2.3f, z), vec3(0.6f, 2.3f, 8.f), hex(0x8a6844), M_WOOD);
        for (int a = 0; a < 6; a++)
            for (int b = 0; b < 2; b++)
                box(static_, vec3(x - 21.f + a * 8.4f, Y_FLOOR + 6.8f, z + (b ? 4.f : -4.f)), vec3(3.9f, 1.3f, 3.7f), fruit[(a * 3 + b + j * 5) % 8], M_CRATE,
                    0, 0, 0, r.uniform());
        blob(staticBlobs_, vec3(x, Y_FLOOR + 0.05f, z), 27.f, 10.f, 0.4f, 0.5f);
    }
    // ---- the aisles: gondolas full of goods, promotional stacks at their ends
    for (int i = 0; i < 13; i++) {
        const float x = -204.f + 34.f * i;
        for (int run = 0; run < 2; run++) {
            const float z0 = run ? -516.f : Z_AISLE0, z1 = run ? Z_AISLE1 : -500.f;
            box(static_, vec3(x, Y_FLOOR + 11.f, (z0 + z1) * 0.5f), vec3(5.f, 11.f, (z0 - z1) * 0.5f), P.gondola, M_SHELF, P.wood, 0,
                (float)(i * 2 + run) * 0.21f, 0.3f + i + run * 13.f);
            blob(staticBlobs_, vec3(x, Y_FLOOR + 0.05f, (z0 + z1) * 0.5f), 7.f, (z0 - z1) * 0.5f + 2.f, 0.45f, 0.3f);
        }
        if (i % 2 == 0) {
            const int fam = i % 7;
            const float hs = 1.1f;
            for (int lay = 0; lay < 3; lay++)
                for (int a = 0; a < 3 - lay; a++)
                    box(static_, vec3(x + (a - (2 - lay) * 0.5f) * hs * 2.05f, Y_FLOOR + hs + lay * hs * 2.02f, Z_AISLE0 + 3.f), vec3(hs), vec3(1.f),
                        M_PRODUCT, (float)fam, 0, 0, r.uniform());
        }
    }
    // ---- the fridges along the back wall, the department panels above them
    box(static_, vec3(0.f, Y_FLOOR + 11.f, Z_BACK + 5.f), vec3(X_WALL - 2.f, 11.f, 4.f), vec3(0.82f), M_SHELF, 2.f, 0, 3.f, 0.77f, 0, G_FRIDGE);
    for (int i = 0; i < 6; i++)
        box(static_, vec3(-195.f + 78.f * i, 12.5f, Z_BACK + 1.2f), vec3(26.f, 4.f, 0.3f), P.goods[(i * 3) % 8] * 0.8f, M_SIGN, (float)(i % 6), 0, 0,
            0.4f * i);
    // ---- outside: the car park (wider with the wings), the town, the land
    int wings = 0;
    if (L >= 62.f) wings = std::min(3, 1 + (int)((L - 62.f) / 9.f));
    for (int w = 1; w <= wings; w++)
        for (int s = -1; s <= 1; s += 2) buildModule(s * MODULE_W * w, 66.f + (w - 1) * 9.f + (s > 0 ? 2.f : 0.f), true);
    buildCarPark(760.f + MODULE_W * wings);
    buildTown();
    // ---- the absurd: mountains of bags, the city of carts
    for (int i = 0; i < MOUNTAINS; i++) {
        const float H = mountainH(i, L + 1.f);
        if (H <= 0.f) continue;
        buildMountain(vec3(MOUNT_X[i], Y_FLOOR, MOUNT_Z[i]), H, 3.7f + i * 1.3f, mountainL0(i), 9.f + 0.8f * i);
    }
    buildCartCity(L);
    builtL_ = lines_;
    builtCarts_ = cartsArrived_;
    builtVariant_ = variant_;
    worldDirty_ = false;
}
