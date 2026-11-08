// GROCERIES: the checkout counter seen from above, as instanced boxes (see groceries.cpp): the belt and its rails,
// the steel lane down to the scanner, the scanner, the bag rack, the till, the receipt printer and its receipt, the
// card terminal, a rack of sweets, and the customer's things.
#include <algorithm>
#include <cmath>

#include "universes/groceries/groceries.hpp"
#include "universes/groceries/world.hpp"

using namespace groc;

namespace {
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }
vec3 hex(unsigned v) { return ugl::toLinear(vec3(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f)); }
} // namespace

// =====================================================================================================
// Helpers
// =====================================================================================================

void GroceriesUniverse::box(std::vector<Box>& out, const vec3& c, const vec3& h, const vec3& col, int mat, float p0, float p1,
                            float p2, float seed, int group) {
    obox(out, c, vec3(h.x, 0.f, 0.f), vec3(0.f, h.y, 0.f), vec3(0.f, 0.f, h.z), col, mat, p0, p1, p2, seed, group);
}

void GroceriesUniverse::obox(std::vector<Box>& out, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col,
                             int mat, float p0, float p1, float p2, float seed, int group) {
    Box b;
    b.c[0] = c.x; b.c[1] = c.y; b.c[2] = c.z; b.c[3] = (float)mat;
    b.x[0] = ax.x; b.x[1] = ax.y; b.x[2] = ax.z; b.x[3] = p0;
    b.y[0] = ay.x; b.y[1] = ay.y; b.y[2] = ay.z; b.y[3] = p1;
    b.z[0] = az.x; b.z[1] = az.y; b.z[2] = az.z; b.z[3] = p2;
    b.col[0] = col.x; b.col[1] = col.y; b.col[2] = col.z; b.col[3] = seed;
    b.e[0] = 0.f; b.e[1] = 0.f; b.e[2] = 0.f; b.e[3] = (float)group;
    out.push_back(b);
}

void GroceriesUniverse::blob(std::vector<Blob>& out, const vec3& c, float hx, float hy, float a, float soft) {
    Blob b;
    b.p[0] = c.x; b.p[1] = c.y; b.p[2] = c.z; b.p[3] = a;
    b.r[0] = hx; b.r[1] = hy; b.r[2] = soft; b.r[3] = 0.f;
    out.push_back(b);
}

int GroceriesUniverse::fabricStyle(float h, vec3& fabric) const {
    // mostly kraft paper, some reusable fabric bags, a few white paper ones
    fabric = palT_.fabric[(int)(h * 997.f) % 4];
    if (h < 0.55f) return 0;
    if (h < 0.85f) return 1;
    return 2;
}

// A product's place in the bag being packed (relative to the bag's center at the counter's top): six on the bottom
// in two rows of three, four on top.
vec3 GroceriesUniverse::slotPos(int i) const {
    const float s = 0.76f, h = CELL * PACK;
    const int layer = i < 6 ? 0 : 1;
    const int j = layer ? i - 6 : i;
    const float x = layer ? (j % 2 - 0.5f) * s : (j % 3 - 1.f) * s;
    const float y = (j / (layer ? 2 : 3) - 0.5f) * s;
    return vec3(x, y, 0.1f + h + layer * (2.f * h + 0.03f));
}

// A full bag's place at the counter's right edge: two columns, from the bottom up.
vec3 GroceriesUniverse::shelfPos(int i) const {
    // past the edge's eight places, a second layer on top of the first
    const int layer = i / 8, j = i % 8;
    const int col = j % 2, row = j / 2;
    return vec3(16.6f + col * 2.95f + layer * 0.5f, -3.9f + row * 2.45f + layer * 0.4f, Z_TOP + layer * BAG_H * 0.55f);
}

// A bag seen from above, open: four walls of paper or fabric, its floor in shade, two handles, and `filled`
// products packed neatly inside.
void GroceriesUniverse::buildBag(std::vector<Box>& out, const vec3& c, float s, int style, const vec3& fabric, int filled, float seed,
                                 float yaw) {
    const vec3 col = style == 1 ? fabric : palT_.kraft;
    const float hx = BAG_HX * s, hy = BAG_HY * s, H = BAG_H * s, t = 0.1f * s;
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const vec3 X(cy, sy, 0.f), Y(-sy, cy, 0.f), Z(0.f, 0.f, 1.f);
    auto at = [&](float x, float y, float z) { return c + X * x + Y * y + Z * z; };
    for (int k = -1; k <= 1; k += 2) {
        obox(out, at(0.f, k * (hy - t), H * 0.5f), X * hx, Y * t, Z * (H * 0.5f), col, M_BAG, (float)style, 0.f, 0.f, seed);
        obox(out, at(k * (hx - t), 0.f, H * 0.5f), X * t, Y * (hy - 2.f * t), Z * (H * 0.5f), col * 0.96f, M_BAG, (float)style, 0.f, 0.f, seed);
    }
    obox(out, at(0.f, 0.f, 0.05f), X * (hx - 2.f * t), Y * (hy - 2.f * t), Z * 0.05f, col, M_BAG, (float)style, 1.f, 0.f, seed);
    // the handles, standing up over the long sides
    const vec3 hc = style == 1 ? fabric * 0.8f : palT_.kraft * 0.82f;
    for (int k = -1; k <= 1; k += 2) {
        const float hh = 0.9f * s;
        for (int j = -1; j <= 1; j += 2)
            obox(out, at(j * hx * 0.42f, k * (hy - t), H + hh * 0.5f), X * (0.07f * s), Y * (0.07f * s), Z * (hh * 0.5f), hc, M_PAINT, 0.f, 0.f,
                 0.f, seed);
        obox(out, at(0.f, k * (hy - t), H + hh), X * (hx * 0.42f + 0.07f * s), Y * (0.07f * s), Z * (0.07f * s), hc, M_PAINT, 0.f, 0.f, 0.f, seed);
    }
    // the goods inside
    for (int i = 0; i < filled && i < 10; i++) {
        const vec3 sp = slotPos(i) * s;
        const int type = bagTypesFor_ && i < (int)bagTypesFor_->size() ? (*bagTypesFor_)[i] : (int)(hashf((uint64_t)(seed * 1e5f) + i) * 7.f) % 7;
        const float rot = (hashf((uint64_t)(seed * 3e5f) + i) - 0.5f) * 0.12f;
        const vec3 p = c + X * sp.x + Y * sp.y + Z * sp.z;
        const float h = CELL * PACK * s;
        const float cr = std::cos(yaw + rot), sr = std::sin(yaw + rot);
        obox(out, p, vec3(cr, sr, 0.f) * h, vec3(-sr, cr, 0.f) * h, Z * h, vec3(1.f), M_PRODUCT, (float)type, 0.f, 0.f,
             hashf((uint64_t)(seed * 7e5f) + i));
    }
    blob(blobs_, vec3(c.x + 0.3f, c.y - 0.3f, Z_TOP + 0.01f), hx + 0.5f, hy + 0.5f, 0.35f, 0.6f);
}

// =====================================================================================================
// The static counter
// =====================================================================================================

void GroceriesUniverse::buildCounter() {
    static_.clear();
    staticBlobs_.clear();
    const Palette& P = palT_;
    Rng r(seed_ ^ 0xC0C0ull);
    const vec3 dark = hex(0x24262a);
    // the counter's top, all across
    box(static_, vec3(0.f, 0.f, Z_TOP - 0.5f), vec3(40.f, 26.f, 0.5f), P.counter, M_COUNTER, P.wood);
    // the belt: rubber between two steel rails (the well's walls), from the top of the frame down to its end roller
    box(static_, vec3(0.f, 1.2f, Z_TOP + 0.0f), vec3(5.f, 11.65f, 0.03f), P.belt, M_BELT);
    for (int s = -1; s <= 1; s += 2) {
        box(static_, vec3(s * 5.18f, 1.f, Z_TOP + 0.2f), vec3(0.18f, 12.f, 0.2f), P.steel, M_STEEL);
        box(static_, vec3(s * 5.18f, 1.f, Z_TOP + 0.41f), vec3(0.05f, 12.f, 0.01f), P.accent * 0.6f + vec3(0.4f), M_EMIT, 1.f, 0, 0, 0, G_RAIL);
    }
    box(static_, vec3(0.f, -10.62f, Z_TOP + 0.12f), vec3(5.36f, 0.22f, 0.14f), P.steel, M_STEEL);
    // the steel lane along the right rail, down to the scanner's bed
    box(static_, vec3(CHUTE_X, 1.9f, Z_TOP + 0.02f), vec3(0.62f, 8.6f, 0.03f), P.steel * 0.95f, M_STEEL);
    box(static_, vec3(CHUTE_X + 0.68f, 1.9f, Z_TOP + 0.15f), vec3(0.06f, 8.6f, 0.15f), P.steel, M_STEEL);
    // the scanner's bed (steel), its glass, the upright scanner behind it
    box(static_, vec3(8.5f, SCAN_Y, Z_TOP + 0.02f), vec3(3.0f, 1.65f, 0.03f), P.steel, M_COUNTER, 2.f);
    box(static_, vec3(SCAN_X, SCAN_Y, Z_TOP + 0.04f), vec3(1.45f, 1.15f, 0.03f), vec3(0.05f), M_SCANNER, 1.f, 0, 0, 0, G_SCAN);
    box(static_, vec3(SCAN_X, SCAN_Y - 2.05f, Z_TOP + 1.2f), vec3(1.7f, 0.42f, 1.2f), dark, M_PAINT, 0.4f);
    blob(staticBlobs_, vec3(SCAN_X + 0.4f, SCAN_Y - 2.3f, Z_TOP + 0.01f), 2.4f, 0.9f, 0.4f, 0.6f);
    // the bag rack: a steel frame holding the bag's mouth open
    for (int s = -1; s <= 1; s += 2) {
        box(static_, vec3(BAG_X + s * 1.75f, BAG_Y, Z_TOP + BAG_H * 0.5f), vec3(0.07f, 1.35f, 0.07f), P.steel, M_STEEL);
        box(static_, vec3(BAG_X + s * 1.75f, BAG_Y, Z_TOP + 0.04f), vec3(0.1f, 1.45f, 0.04f), P.steel, M_STEEL);
        for (int k = -1; k <= 1; k += 2)
            box(static_, vec3(BAG_X + s * 1.75f, BAG_Y + k * 1.3f, Z_TOP + BAG_H * 0.25f), vec3(0.06f, 0.06f, BAG_H * 0.25f), P.steel, M_STEEL);
    }
    // a stack of bags, folded flat, beside the rack
    for (int i = 0; i < 6; i++)
        box(static_, vec3(BAG_X + 0.1f * (i % 2), -11.15f + 0.03f * i, Z_TOP + 0.03f + 0.05f * i), vec3(1.5f, 0.55f, 0.025f),
            P.kraft * (0.92f + 0.05f * (i % 2)), M_BAG, 0.f, 0.f, 0.f, 0.3f * i);
    // the till: its drawer and keys, the screen tilted toward the cashier
    {
        const vec3 c(PRINT_X, 4.9f, Z_TOP);
        box(static_, c + vec3(0.f, 0.f, 0.3f), vec3(2.6f, 1.8f, 0.3f), dark, M_PAINT, 0.3f);
        box(static_, c + vec3(0.f, -0.9f, 0.65f), vec3(2.f, 0.75f, 0.06f), dark * 1.4f, M_TILL, 1.f, 0, 0, 0.3f, G_TILL);
        const float a = 0.6f;
        obox(static_, c + vec3(0.f, 0.75f, 1.3f), vec3(2.3f, 0.f, 0.f), vec3(0.f, std::cos(a), std::sin(a)) * 1.5f,
             vec3(0.f, -std::sin(a), std::cos(a)) * 0.12f, dark, M_TILL, 0.f, 0, 0, 0.7f, G_TILL);
        box(static_, c + vec3(0.f, 1.3f, 0.8f), vec3(0.25f, 0.25f, 0.3f), dark * 0.9f, M_PAINT, 0.3f);
        blob(staticBlobs_, c + vec3(0.4f, -0.4f, 0.01f), 3.4f, 2.5f, 0.4f, 0.5f);
    }
    // the receipt printer, its paper slot and its light
    {
        const vec3 c(PRINT_X, PRINT_Y, Z_TOP);
        box(static_, c + vec3(0.f, 0.f, 0.45f), vec3(1.3f, 1.05f, 0.45f), hex(0xd8d8d4) * 0.6f + dark * 0.4f, M_PAINT, 0.35f);
        box(static_, c + vec3(0.f, -0.92f, 0.91f), vec3(0.95f, 0.05f, 0.01f), vec3(0.03f), M_PAINT);
        box(static_, c + vec3(0.85f, 0.6f, 0.91f), vec3(0.09f, 0.09f, 0.01f), hex(0x5ad16a), M_EMIT, 1.f, 0, 0, 0, G_LED);
        blob(staticBlobs_, c + vec3(0.3f, -0.3f, 0.01f), 1.8f, 1.5f, 0.4f, 0.5f);
    }
    // the card terminal, turned a little toward the customer
    {
        const vec3 c(-13.0f, -5.0f, Z_TOP);
        const float a = 0.3f;
        const vec3 X(std::cos(a), std::sin(a), 0.f), Y(-std::sin(a), std::cos(a), 0.f), Z(0.f, 0.f, 1.f);
        obox(static_, c + Z * 0.3f, X * 0.95f, Y * 1.5f, Z * 0.3f, dark, M_PAINT, 0.4f);
        obox(static_, c + Y * 0.55f + Z * 0.62f, X * 0.7f, Y * 0.5f, Z * 0.02f, dark, M_TILL, 0.f, 0, 0, 0.9f, G_TILL);
        obox(static_, c - Y * 0.6f + Z * 0.62f, X * 0.65f, Y * 0.62f, Z * 0.02f, dark * 1.3f, M_TILL, 1.f, 0, 0, 0.1f);
        blob(staticBlobs_, c + vec3(0.3f, -0.3f, 0.01f), 1.4f, 1.9f, 0.4f, 0.5f);
    }
    // a rack of sweets at the top right: rows of little packs
    {
        const vec3 c(17.2f, 9.3f, Z_TOP);
        box(static_, c + vec3(0.f, 0.f, 0.08f), vec3(3.9f, 2.6f, 0.08f), P.steel * 0.9f, M_STEEL);
        for (int row = 0; row < 4; row++) {
            box(static_, c + vec3(0.f, -2.1f + row * 1.4f - 0.62f, 0.4f), vec3(3.8f, 0.04f, 0.25f), P.steel, M_STEEL);
            const int fam = r.irange(0, 6);
            for (int k = 0; k < 8; k++) {
                const int f = (k % 3 == 0) ? r.irange(0, 6) : fam;
                box(static_, c + vec3(-3.3f + k * 0.94f, -2.1f + row * 1.4f, 0.3f + 0.03f * (k % 2)), vec3(0.42f, 0.5f, 0.14f), vec3(1.f),
                    M_PRODUCT, (float)f, 0, 0, r.uniform());
            }
        }
        blob(staticBlobs_, c + vec3(0.3f, -0.3f, 0.01f), 4.3f, 3.0f, 0.35f, 0.4f);
    }
    staticCount_ = static_.size();
}

// =====================================================================================================
// The receipt: printed line by line, it drapes from the printer onto the counter and curls at its end
// =====================================================================================================

void GroceriesUniverse::buildReceipt(float len, float t) {
    (void)t;
    const Palette& P = pal_;
    const float w = 0.72f, seg = 0.3f;
    const vec3 start(PRINT_X, PRINT_Y - 0.95f, Z_TOP + 0.85f);
    const float curl = std::min(1.8f, len * 0.35f);
    const float flat = len - curl;
    auto at = [&](float s) {
        // out of the slot, down onto the counter, then lying loosely along it
        const float drop = smoothstepf(0.f, 1.3f, s);
        const float wave = 0.06f * (0.5f + 0.5f * std::sin(s * 1.3f)) * smoothstepf(1.5f, 4.f, s);
        return vec3(start.x + 0.4f * std::sin(s * 0.42f) * smoothstepf(1.f, 4.f, s), start.y - s,
                    lerpf(start.z, Z_TOP + 0.03f, drop) + wave);
    };
    vec3 prev = at(0.f);
    float s = 0.f;
    while (s < flat - 1e-3f) {
        const float s1 = std::min(flat, s + seg);
        const vec3 p1 = at(s1);
        const vec3 d = p1 - prev;
        const float L = length(d);
        const vec3 ay = d * (0.5f / std::max(L, 1e-4f)) * L;
        const vec3 ax(w, 0.f, 0.f);
        const vec3 nz = normalize(cross(ax, ay)) * 0.012f;
        obox(boxes_, (prev + p1) * 0.5f, ax, ay * 1.04f, nz, P.paper, M_PAPER, 0.f, (s + s1) * 0.5f);
        prev = p1;
        s = s1;
    }
    // the curl at its end: the paper rolls up on itself
    const vec3 base = at(flat);
    const float R = 0.3f;
    const int n = 12;
    vec3 cp = base;
    for (int i = 1; i <= n; i++) {
        const float u = (float)i / n;
        const float th = u * curl / R * 0.9f;
        const vec3 p(base.x, base.y - R * std::sin(th), base.z + R * (1.f - std::cos(th)));
        const vec3 d = p - cp;
        const float L = length(d);
        if (L > 1e-4f) {
            const vec3 ax(w, 0.f, 0.f);
            const vec3 ay = d * 0.5f;
            const vec3 nz = normalize(cross(ax, ay)) * 0.012f;
            obox(boxes_, (cp + p) * 0.5f, ax, ay * 1.08f, nz, P.paper * 0.97f, M_PAPER, 0.f, flat + curl * (u - 0.5f / n));
        }
        cp = p;
    }
    blob(blobs_, vec3(start.x + 0.2f, start.y - flat * 0.5f - 0.6f, Z_TOP + 0.01f), w + 0.3f, flat * 0.5f + 0.6f, 0.18f, 0.5f);
}

// =====================================================================================================
// The customer's things, set down on the counter one after the other over the song
// =====================================================================================================

void GroceriesUniverse::buildThings(float L) {
    const Palette& P = pal_;
    auto pop = [&](float at) { return smoothstepf(at, at + 0.6f, L); };
    auto turn = [](float a) { return std::make_pair(vec3(std::cos(a), std::sin(a), 0.f), vec3(-std::sin(a), std::cos(a), 0.f)); };
    const vec3 Z(0.f, 0.f, 1.f);
    // the shopping list
    if (float k = pop(1.f); k > 0.f) {
        const auto [X, Y] = turn(0.18f);
        const vec3 c(-19.7f, -3.6f + (1.f - k) * 1.5f, Z_TOP + 0.02f);
        obox(boxes_, c, X * 1.15f, Y * 1.6f, Z * 0.012f, P.paper * 1.02f, M_PAPER, 1.f, 0.f, 0.f, 0.37f);
        blob(blobs_, c + vec3(0.15f, -0.15f, -0.01f), 1.3f, 1.75f, 0.15f, 0.5f);
    }
    // the keys: a ring, two keys, a fob
    if (float k = pop(4.f); k > 0.f) {
        const vec3 c(-13.2f, -9.2f + (1.f - k) * 1.f, Z_TOP + 0.05f);
        for (int i = 0; i < 8; i++) {
            const float a = i * TAU / 8.f;
            const auto [X, Y] = turn(a + PI * 0.5f);
            obox(boxes_, c + vec3(std::cos(a), std::sin(a), 0.f) * 0.42f, X * 0.18f, Y * 0.035f, Z * 0.035f, P.steel, M_STEEL);
        }
        {
            const auto [X, Y] = turn(-0.5f);
            obox(boxes_, c + X * 0.0f + Y * -1.1f + vec3(0.35f, 0.f, 0.02f), X * 0.14f, Y * 0.75f, Z * 0.035f, hex(0xc8a85a), M_STEEL);
            obox(boxes_, c + Y * -0.7f + vec3(0.42f, 0.f, 0.02f), X * 0.28f, Y * 0.2f, Z * 0.05f, hex(0xc8a85a), M_STEEL);
        }
        {
            const auto [X, Y] = turn(0.9f);
            obox(boxes_, c + vec3(-0.75f, -0.85f, 0.03f), X * 0.13f, Y * 0.7f, Z * 0.035f, P.steel * 1.05f, M_STEEL);
        }
        box(boxes_, c + vec3(0.45f, 0.75f, 0.1f), vec3(0.35f, 0.48f, 0.12f), P.accent, M_PAINT, 0.4f);
        blob(blobs_, c + vec3(0.1f, -0.3f, -0.04f), 1.2f, 1.5f, 0.25f, 0.6f);
    }
    // the wallet, a card peeking out
    if (float k = pop(9.f); k > 0.f) {
        const auto [X, Y] = turn(-0.12f);
        const vec3 c(-19.5f, -7.7f + (1.f - k) * 1.f, Z_TOP);
        obox(boxes_, c + Z * 0.22f, X * 1.5f, Y * 1.05f, Z * 0.22f, hex(0x6a4630), M_PAINT, 0.15f, 0, 0, 0.5f);
        obox(boxes_, c + Z * 0.445f + X * 0.95f + Y * 0.15f, X * 0.85f, Y * 0.55f, Z * 0.01f, P.accent2, M_PAPER, 2.f);
        obox(boxes_, c + Z * 0.445f - X * 0.0f, X * 1.45f, Y * 0.02f, Z * 0.008f, hex(0x4a3020), M_PAINT);
        blob(blobs_, c + vec3(0.25f, -0.25f, 0.01f), 1.8f, 1.35f, 0.35f, 0.5f);
    }
    // a takeaway coffee
    if (float k = pop(15.f); k > 0.f) {
        const vec3 c(-13.4f, 8.8f, Z_TOP);
        const float h = 2.3f * k;
        box(boxes_, c + Z * (h * 0.45f), vec3(0.62f, 0.62f, h * 0.45f), P.kraft * 0.9f, M_DISC, 0.f);
        box(boxes_, c + Z * (h + 0.02f), vec3(0.75f, 0.75f, 0.03f), vec3(0.95f, 0.94f, 0.92f), M_DISC, 0.f);
        blob(blobs_, c + vec3(0.6f, -0.5f, 0.01f), 1.3f, 1.1f, 0.4f, 0.8f);
    }
    // sunglasses
    if (float k = pop(24.f); k > 0.f) {
        const vec3 c(-19.7f, 8.6f + (1.f - k), Z_TOP + 0.2f);
        for (int s = -1; s <= 1; s += 2) box(boxes_, c + vec3(s * 0.62f, 0.f, 0.f), vec3(0.55f, 0.48f, 0.04f), vec3(0.f), M_DISC, 1.f);
        box(boxes_, c + vec3(0.f, 0.15f, 0.02f), vec3(0.16f, 0.04f, 0.03f), vec3(0.05f), M_PAINT, 0.5f);
        for (int s = -1; s <= 1; s += 2) box(boxes_, c + vec3(s * 1.1f, 0.85f, -0.08f), vec3(0.04f, 0.9f, 0.03f), vec3(0.05f), M_PAINT, 0.5f);
        blob(blobs_, c + vec3(0.2f, -0.1f, -0.19f), 1.5f, 1.f, 0.3f, 0.6f);
    }
    // the loyalty card, by the terminal
    if (float k = pop(35.f); k > 0.f) {
        const auto [X, Y] = turn(-0.35f);
        obox(boxes_, vec3(-13.4f, -1.6f + (1.f - k), Z_TOP + 0.02f), X * 0.85f, Y * 0.55f, Z * 0.01f, P.accent, M_PAPER, 2.f);
    }
    // a folded tote, for next time
    if (float k = pop(50.f); k > 0.f) {
        const auto [X, Y] = turn(0.1f);
        const vec3 c(-13.6f, 1.9f + (1.f - k), Z_TOP + 0.12f);
        obox(boxes_, c, X * 1.3f, Y * 0.85f, Z * 0.12f, P.fabric[1], M_BAG, 1.f, 0.f, 0.f, 0.9f);
        blob(blobs_, c + vec3(0.2f, -0.2f, -0.11f), 1.9f, 1.2f, 0.3f, 0.5f);
    }
}
