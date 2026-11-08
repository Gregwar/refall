// GROCERIES: a supermarket checkout in 3D. The blocks are groceries; cleared rows are scanned and packed into bags,
// and the shopping grows over the song into an absurd hypermarket.
//
// One world: we stand behind our checkout's counter, the well standing on it; to the right our belt brings the
// next shopper's products in toward the well (a divider between shoppers), to the left the bag stand and the
// basket, a cart behind the counter. Beyond, the other lanes, their number lights on poles; the produce on the left,
// pallets of products stacked in pyramids, shelves; at the back the aisles under their hanging signs, the fridges
// glowing along the back wall, the ceiling's rows of tubes mirrored in the glossy floor.
//
// The pieces are products, one per cell, each tetromino its own family (milk cartons, cereal boxes, punnets of
// berries, trays of apples, packs of tins, boxes of pasta, cartons of juice): cubes in clean packaging with
// invented, illegible labels. When a piece spawns, the front product of our belt is taken toward the well. A
// cleared row is scanned: the scanner on the well's right rail slides to the row, its red beam sweeps across it,
// each product flashing as it beeps; then the products are tossed one after the other into the bag on the stand.
// Ten products fill a bag; a full bag is lifted into the cart; a full cart rolls away to the queue of carts.
//
// The growth (the game's lines; ZEN_GROCERIES_LINES adds to them, ZEN_GROW_RATE adds lines over time): one lane
// and a basket; then bags pile up and carts line up; more lanes open, one after the other (their lights, their
// belts, their heaps of bags); the camera pulls back over the whole store as its roof lifts off; vans load at the
// store front, lorries at its side; the store grows wings into a hypermarket, mountains of bags rise around it,
// and a city of carts towers behind it, to the horizon.
//
// The song: calm is quiet late shopping (the light dimmer, the belts slow); mid wakes the store; a peak brightens
// every light and runs the belts fast, scanners beeping on the beat. A real drop (the HUD's section DROP, else the
// music's pace) is the millionth customer: confetti rains over the store, every scanner flashes on the beat, the
// light swells. A Tetris lights the lanes in a wave. Slow motion is closing time (the store dims to night but its
// checkouts); game over sweeps the stack off the counter, product after product.
//
// Layout contract: the board is drawn through the layout's reference camera raised to EYE_Y with a lens shift (as
// DOCKS): the board's plane keeps its pixels. The world has its own camera, the same one at the start, which pulls
// back with the growth while the well stays.
#include "universes/groceries/groceries.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/groceries/shaders.hpp"
#include "universes/groceries/world.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

using namespace groc;

namespace {

// Figtree, SIL Open Font License, see OFL-Figtree.txt.
const unsigned char FONT_BOLD[] = {
#include "universes/groceries/Figtree-SemiBold.ttf.inc"
};
const unsigned char FONT_MEDIUM[] = {
#include "universes/groceries/Figtree-Medium.ttf.inc"
};

namespace shaders = groceries_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hexs(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hexs(v)); }
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// ---- The variants (sRGB hex): the hour and the store's style.
struct VariantDef {
    const char* name;
    unsigned zen, hor, ground, sunGlow, fog;
    unsigned inTop, inBot, inKey, outTop, outBot, outKey;
    float inK, outK;
    unsigned floor, wall, wallOut, ceil, counter, steel, belt, gondola, shelfEdge, kraft, accent, accent2, tube, lamp, win, fridge,
        grass, asphalt, veil, cart;
    unsigned famA[7], famB[7], famC[7];
    unsigned goods[8], fabric[4], confetti[5];
    float sunAz, sunEl, night, gloss, stars, clouds, wood;
};
const VariantDef VARIANTS[] = {
    // a bright morning store: warm white light, cream tiles, a fresh green
    {"MORNING", 0x7fb0dc, 0xf4dcc2, 0xa6ae92, 0xffd2a0, 0xdfe2e2,
     0xfff4e6, 0xb4aa9c, 0xfff0da, 0xa8c4e4, 0x8a8a7c, 0xfff0d8, 1.0f, 1.0f,
     0xd9d2c4, 0xf2eee6, 0xd6d2ca, 0xf0eeea, 0xcbd3d8, 0xc8ccd0, 0x2a2c30, 0xe8eaea, 0xf6f6f2, 0xc49a6c, 0x2e9a6a, 0xfff2c0,
     0xfffaf0, 0xffd9a0, 0xffe0b0, 0xe8f4ff, 0x86a660, 0x4a4d52, 0x161c24, 0xc9ced4,
     {0x86c4e6, 0xf2c14e, 0x6a52a6, 0x74bc5c, 0xd84a3f, 0x3164b4, 0xf08a2a},
     {0xf6f2ea, 0xfff3d8, 0xf3ecdf, 0xc9a272, 0xf8f4ee, 0xf6f2ea, 0xfff4e2},
     {0x214f7a, 0xd8493c, 0x2c2452, 0x2f6b3a, 0xc9ccd1, 0xebd08e, 0x3f8f3a},
     {0xd9483b, 0xf2c14e, 0x3f7fbf, 0x6dbb5a, 0xf08a24, 0x7a5ea8, 0xeee6d2, 0x2a8a8a},
     {0x3f6f8f, 0x5f8f5a, 0xc25a3a, 0x2f3a48},
     {0xff6b5a, 0xffd23f, 0x3fc1c9, 0x8a6cf0, 0x7ed957},
     0.55f, 0.55f, 0.f, 0.38f, 0.f, 0.5f, 0.f},
    // late at night under the fluorescent tubes: cool light, a shiny floor, a red accent, the car park dark
    {"LATE NIGHT", 0x081020, 0x22304a, 0x0e1218, 0xff9a50, 0x182032,
     0xe4efec, 0x6a7a78, 0xe6f2ee, 0x1c2a44, 0x0a0c10, 0x7a8ab0, 0.9f, 0.35f,
     0xa9b0b4, 0xc8cfd0, 0x58606a, 0xd6dada, 0x30404a, 0xb8bec2, 0x1e2022, 0xd8dde0, 0xe8ecec, 0xb8946a, 0xe2453c, 0xf4f4ec,
     0xeafff4, 0xffb060, 0xffe2b8, 0xd8f0ff, 0x24342a, 0x2a2c30, 0x0b0f16, 0xb0b6bc,
     {0x86c4e6, 0xf2c14e, 0x6a52a6, 0x74bc5c, 0xd84a3f, 0x3164b4, 0xf08a2a},
     {0xf6f2ea, 0xfff3d8, 0xf3ecdf, 0xc9a272, 0xf8f4ee, 0xf6f2ea, 0xfff4e2},
     {0x214f7a, 0xd8493c, 0x2c2452, 0x2f6b3a, 0xc9ccd1, 0xebd08e, 0x3f8f3a},
     {0xd9483b, 0xf2c14e, 0x3f7fbf, 0x6dbb5a, 0xf08a24, 0x7a5ea8, 0xeee6d2, 0x2a8a8a},
     {0x3f6f8f, 0x5f8f5a, 0xc25a3a, 0x2f3a48},
     {0xff5a5a, 0xffd23f, 0x3fd1d9, 0xb08cff, 0x7ee957},
     -0.4f, 0.18f, 1.f, 0.6f, 0.7f, 0.15f, 0.f},
    // an organic co-op in the golden afternoon: wooden shelves, kraft and earthy packaging, terracotta tiles, sage
    {"CO-OP", 0x8ab4d0, 0xf6d6a6, 0xa8a070, 0xffbe70, 0xe8d8bc,
     0xfff0d8, 0xa08060, 0xffe6c0, 0xb8c8d8, 0x8a7a5a, 0xffd8a0, 1.0f, 1.1f,
     0xcdb39a, 0xf0e6d6, 0xc8b8a0, 0xe8dcc8, 0xb88a5a, 0xc0b8a8, 0x2c2a26, 0xb88c5c, 0xe8d8b8, 0xb88c5a, 0x5f8a4a, 0xf6ecd2,
     0xfff0d0, 0xffd090, 0xffd8a0, 0xfff0dc, 0x8aa060, 0x5a5650, 0x1a1612, 0xb8b0a0,
     {0x8fb4c9, 0xd9a93f, 0x6d4c7d, 0x8aab5b, 0xc4573a, 0x3f5f8a, 0xe08d45},
     {0xf1e8d6, 0xf6ead0, 0xefe3cc, 0xc4a070, 0xf2e8d8, 0xf0e8d8, 0xf6ead2},
     {0x2f4f68, 0xa8452e, 0x3a2440, 0x3f5f30, 0xc0bcb4, 0xe2c88a, 0x4f7f3a},
     {0xc4573a, 0xd9a93f, 0x5f7fa0, 0x8aab5b, 0xe08d45, 0x7a5a7a, 0xefe3cc, 0x4a7a6a},
     {0x6f8a5a, 0xb8643a, 0x3f5a6a, 0xa89060},
     {0xe8704a, 0xf2c14e, 0x6fb0a8, 0xb07ab0, 0x9ac060},
     0.7f, 0.3f, 0.f, 0.18f, 0.f, 0.45f, 1.f},
    // the blue hour: a deep evening sky, the store warm inside, an amber accent, the lamps coming on
    {"BLUE HOUR", 0x14204a, 0x6a74b0, 0x1a2030, 0xff9a6a, 0x2a3456,
     0xfff0dc, 0x8a7a6a, 0xffe8cc, 0x3a4a80, 0x141824, 0x9aaae0, 1.0f, 0.5f,
     0xc6bfb4, 0xece6dc, 0x4a5260, 0xe6e0d8, 0x384a6a, 0xc0c4c8, 0x222428, 0xe2e2de, 0xf0eee8, 0xc49a6c, 0xf08a3a, 0xfff4d6,
     0xfff4e4, 0xffb868, 0xffd8a0, 0xe8f0ff, 0x2c3a30, 0x303238, 0x0e1220, 0xc0c6cc,
     {0x86c4e6, 0xf2c14e, 0x6a52a6, 0x74bc5c, 0xd84a3f, 0x3164b4, 0xf08a2a},
     {0xf6f2ea, 0xfff3d8, 0xf3ecdf, 0xc9a272, 0xf8f4ee, 0xf6f2ea, 0xfff4e2},
     {0x214f7a, 0xd8493c, 0x2c2452, 0x2f6b3a, 0xc9ccd1, 0xebd08e, 0x3f8f3a},
     {0xd9483b, 0xf2c14e, 0x3f7fbf, 0x6dbb5a, 0xf08a24, 0x7a5ea8, 0xeee6d2, 0x2a8a8a},
     {0x3f6f8f, 0x5f8f5a, 0xc25a3a, 0x2f3a48},
     {0xff7a5a, 0xffd23f, 0x5ad1e0, 0xa08cff, 0x8ee070},
     -0.5f, 0.02f, 0.75f, 0.45f, 0.3f, 0.4f, 0.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

GroceriesUniverse::Palette makePalette(int v) {
    const VariantDef& V = VARIANTS[v];
    GroceriesUniverse::Palette P;
    P.zen = lin(V.zen); P.hor = lin(V.hor); P.ground = lin(V.ground); P.sunGlow = lin(V.sunGlow) * 0.6f; P.fog = lin(V.fog);
    P.inTop = lin(V.inTop) * 0.62f * V.inK; P.inBot = lin(V.inBot) * 0.42f * V.inK; P.inKey = lin(V.inKey) * 0.5f * V.inK;
    P.outTop = lin(V.outTop) * 0.7f * V.outK; P.outBot = lin(V.outBot) * 0.4f * V.outK; P.outKey = lin(V.outKey) * 0.9f * V.outK;
    P.floor = lin(V.floor); P.wall = lin(V.wall); P.wallOut = lin(V.wallOut); P.ceil = lin(V.ceil); P.counter = lin(V.counter);
    P.steel = lin(V.steel); P.belt = lin(V.belt); P.gondola = lin(V.gondola); P.shelfEdge = lin(V.shelfEdge); P.kraft = lin(V.kraft);
    P.accent = lin(V.accent); P.accent2 = lin(V.accent2); P.tube = lin(V.tube); P.lamp = lin(V.lamp); P.win = lin(V.win);
    P.fridge = lin(V.fridge); P.grass = lin(V.grass); P.asphalt = lin(V.asphalt); P.veil = lin(V.veil); P.cart = lin(V.cart);
    for (int i = 0; i < 7; i++) { P.famA[i] = lin(V.famA[i]); P.famB[i] = lin(V.famB[i]); P.famC[i] = lin(V.famC[i]); }
    for (int i = 0; i < 8; i++) P.goods[i] = lin(V.goods[i]);
    for (int i = 0; i < 4; i++) P.fabric[i] = lin(V.fabric[i]);
    for (int i = 0; i < 5; i++) P.confetti[i] = lin(V.confetti[i]);
    P.sunAz = V.sunAz; P.sunEl = V.sunEl; P.night = V.night; P.gloss = V.gloss; P.stars = V.stars; P.clouds = V.clouds; P.wood = V.wood;
    return P;
}

void lerpPal(GroceriesUniverse::Palette& a, const GroceriesUniverse::Palette& b, float k) {
    vec3* va = &a.zen;
    const vec3* vb = &b.zen;
    const int n = (int)((&a.confetti[4] - &a.zen) + 1);
    for (int i = 0; i < n; i++) va[i] = lerp(va[i], vb[i], k);
    float* fa = &a.sunAz;
    const float* fb = &b.sunAz;
    for (int i = 0; i < 7; i++) fa[i] = lerpf(fa[i], fb[i], k);
}

// ---- The growth: lines -> the camera's zoom (0 our lane .. 5.6 the cart city to the horizon).
struct Stage { float lines, zoom; };
const Stage STAGES[] = {{0, 0.f}, {6, 0.f}, {13, 0.45f}, {20, 1.f}, {32, 2.f}, {48, 3.f}, {78, 4.f}, {120, 5.f}, {190, 5.6f}};
constexpr int STAGE_COUNT = (int)(sizeof(STAGES) / sizeof(STAGES[0]));
float zoomOf(float L) {
    if (L <= STAGES[0].lines) return STAGES[0].zoom;
    for (int i = 0; i + 1 < STAGE_COUNT; i++)
        if (L < STAGES[i + 1].lines) {
            const float t = (L - STAGES[i].lines) / (STAGES[i + 1].lines - STAGES[i].lines);
            return lerpf(STAGES[i].zoom, STAGES[i + 1].zoom, t);
        }
    return STAGES[STAGE_COUNT - 1].zoom;
}

// Test hooks: ZEN_GROCERIES_LINES=N adds N lines to the growth, ZEN_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("ZEN_GROCERIES_LINES") ? std::atoi(std::getenv("ZEN_GROCERIES_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

// The camera's stages: eye and target (board units). The first is the board's camera (filled in at runtime).
struct CamKey { vec3 eye, target; };
CamKey CAM_KEYS[] = {
    {vec3(0.f, EYE_Y, 34.6f), vec3(0.f, EYE_Y, -65.4f)},
    {vec3(-12.f, 72.f, 64.f), vec3(-16.f, Y_FLOOR, -128.f)},
    {vec3(0.f, 330.f, 230.f), vec3(0.f, Y_FLOOR, -230.f)},
    {vec3(0.f, 760.f, 1100.f), vec3(0.f, Y_FLOOR, 60.f)},
    {vec3(0.f, 2000.f, 2600.f), vec3(0.f, Y_FLOOR, -100.f)},
    {vec3(0.f, 3600.f, 5600.f), vec3(0.f, 0.f, -1200.f)},
};
constexpr int CAM_KEY_COUNT = (int)(sizeof(CAM_KEYS) / sizeof(CAM_KEYS[0]));

mat4 inverse(const mat4& m) {
    const float* a = m.m;
    float inv[16];
    inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
    const float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    mat4 r{};
    if (std::fabs(det) < 1e-20f) return mat4::identity();
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] / det;
    return r;
}

std::string ascii(const std::string& s) {
    static const char* LATIN = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 128) { o += (char)c; continue; }
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB7) { o += '\x7f'; i++; continue; }
        if (c == 0xC3 && i + 1 < s.size()) {
            const unsigned char d = (unsigned char)s[i + 1];
            if (d >= 0x80 && d <= 0xBF) { o += LATIN[d - 0x80]; i++; continue; }
        }
        if ((c & 0xE0) == 0xC0) i += 1;
        else if ((c & 0xF0) == 0xE0) i += 2;
        else if ((c & 0xF8) == 0xF0) i += 3;
        o += '?';
    }
    return o;
}
int glyphIndex(unsigned char ch) {
    if (ch < 32 || ch >= 128) ch = '?';
    return ch - 32;
}
float easeOut(float t) { t = saturate(t); return 1.f - (1.f - t) * (1.f - t) * (1.f - t); }
vec3 bez(const vec3& a, const vec3& b, const vec3& c, float t) {
    const float u = 1.f - t;
    return a * (u * u) + b * (2.f * u * t) + c * (t * t);
}

constexpr float SCAN_LEAD = 0.06f, SCAN_SWEEP = 0.24f;   // the beam starts after the lead, sweeps the row in the sweep
const vec3 STAND_POS(-16.f, Y_CT, -0.8f);                 // the bag on the stand (its foot)
const vec3 CART_POS(-31.f, Y_FLOOR, -25.f);                // our lane's cart

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

void GroceriesUniverse::setupBoxVao(GLuint& vao, GLuint& vbo) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < 6; i++) {
        glEnableVertexAttribArray(2 + i);
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Box), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(2 + i, 1);
    }
    glBindVertexArray(0);
}

void GroceriesUniverse::setupBlobVao(GLuint& vao, GLuint& vbo) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Blob), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glBindVertexArray(0);
}

bool GroceriesUniverse::init(int width, int height) {
    using shaders::withCommon;
    progBox_ = ugl::program(shaders::BOX_VS, withCommon(shaders::BOX_FS).c_str(), "groceries solids");
    progBlob_ = ugl::program(shaders::BLOB_VS, shaders::BLOB_FS, "groceries shadows");
    progSky_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::SKY_FS).c_str(), "groceries sky");
    progSprite_ = ugl::program(withCommon(shaders::SPRITE_VS).c_str(), withCommon(shaders::SPRITE_FS).c_str(), "groceries glows");
    progLine_ = ugl::program(shaders::LINE_VS, shaders::LINE_FS, "groceries lines");
    progFlake_ = ugl::program(shaders::FLAKE_VS, shaders::FLAKE_FS, "groceries confetti");
    progVeil_ = ugl::program(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "groceries veil");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "groceries down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "groceries blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "groceries final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "groceries shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "groceries text");

    // a unit cube (-1..1): 36 vertices, position + normal
    {
        std::vector<float> v;
        const int faces[6][2] = {{0, 1}, {0, -1}, {1, 1}, {1, -1}, {2, 1}, {2, -1}};
        for (int f = 0; f < 6; f++) {
            const int ax = faces[f][0];
            const float sg = (float)faces[f][1];
            const int a1 = (ax + 1) % 3, a2 = (ax + 2) % 3;
            const float corners[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
            for (int k = 0; k < 6; k++) {
                float p[3], n[3] = {0, 0, 0};
                p[ax] = sg;
                p[a1] = corners[k][0];
                p[a2] = corners[k][1] * sg;
                n[ax] = sg;
                v.insert(v.end(), {p[0], p[1], p[2], n[0], n[1], n[2]});
            }
        }
        glGenBuffers(1, &cubeVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    }
    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    setupBoxVao(staticVao_, staticVbo_);
    setupBoxVao(roofVao_, roofVbo_);
    setupBoxVao(boxVao_, boxVbo_);
    setupBlobVao(blobVao_, blobVbo_);
    setupBlobVao(sblobVao_, sblobVbo_);
    // glows
    glGenVertexArrays(1, &spriteVao_);
    glGenBuffers(1, &spriteVbo_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Sprite), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    // confetti
    glGenVertexArrays(1, &flakeVao_);
    glGenBuffers(1, &flakeVbo_);
    glBindVertexArray(flakeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, flakeVbo_);
    for (int i = 0; i < 3; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Flake), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    // hairlines: a strip (along 0..1, across -1..1) + three vec4 per instance
    {
        const float strip[8] = {0, -1, 1, -1, 0, 1, 1, 1};
        glGenVertexArrays(1, &lineVao_);
        glGenBuffers(1, &lineVbo_);
        glGenBuffers(1, &stripVbo_);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, stripVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(strip), strip, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        for (int i = 0; i < 3; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Line), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    }
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    buildFont(bold_, FONT_BOLD);
    buildFont(medium_, FONT_MEDIUM);
    resize(width, height);
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void GroceriesUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] groceries font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = 64.f;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AW = 1024, AH = 1024, PAD = 8;
    std::vector<unsigned char> atlas(AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    for (int ch = 32; ch < 128; ch++) {
        const int cp = ch == 127 ? 0xB7 : ch;
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, cp, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
        if (!sdf) { g.w = g.h = 0; continue; }
        if (x + w + 1 > AW) { x = 1; y += rowH + 1; rowH = 0; }
        if (y + h + 1 > AH) { stbtt_FreeSDF(sdf, nullptr); break; }
        for (int r = 0; r < h; r++) std::memcpy(&atlas[(y + r) * AW + x], sdf + r * w, w);
        g.x0 = x / (float)AW; g.y0 = y / (float)AH; g.x1 = (x + w) / (float)AW; g.y1 = (y + h) / (float)AH;
        g.ox = (float)xo; g.oy = (float)yo; g.w = (float)w; g.h = (float)h;
        x += w + 1;
        rowH = std::max(rowH, h);
        stbtt_FreeSDF(sdf, nullptr);
    }
    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, AW, AH, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f.ok = true;
}

void GroceriesUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera ref = layout::camera(width_, height_);
    aspect_ = (float)width_ / height_;
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    focal_ = 1.f / std::tan(half);
    eyeB_ = vec3(ref.target.x, EYE_Y, ref.target.z + ref.dist);
    const mat4 view = mat4::lookAt(eyeB_, eyeB_ + vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));
    shift0_ = focal_ * (EYE_Y - ref.target.y) / ref.dist;
    mat4 shift = mat4::identity();
    shift.at(1, 3) = shift0_;
    vpB_ = shift * mat4::perspective(2.f * half, aspect_, 0.5f, 900.f) * view;
    CAM_KEYS[0].eye = eyeB_;
    CAM_KEYS[0].target = eyeB_ + vec3(0.f, 0.f, -100.f);
    // the board plane's mapping, for the veil
    {
        const vec2 a = layout::project(vpB_, vec3(-10.f, -10.f, 0.f), width_, height_);
        const vec2 b = layout::project(vpB_, vec3(10.f, 10.f, 0.f), width_, height_);
        const float sx = (b.x - a.x) / 20.f, sy = (a.y - b.y) / 20.f;  // pixels per unit
        pxPerUnit_ = sx;
        // pixel (0,0) top-left .. (w,h): board x at pixel px: -10 + (px - a.x) / sx
        const float xl = -10.f + (0.f - a.x) / sx, xr = -10.f + (width_ - a.x) / sx;
        const float yb = -10.f + (a.y - height_) / sy, yt = -10.f + (a.y - 0.f) / sy;
        viewC_ = {(xl + xr) * 0.5f, (yb + yt) * 0.5f};
        viewH_ = {(xr - xl) * 0.5f, (yt - yb) * 0.5f};
    }
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    if (msFbo_) {
        glDeleteFramebuffers(1, &msFbo_);
        glDeleteRenderbuffers(1, &msColor_);
        glDeleteRenderbuffers(1, &msDepth_);
    }
    glGenFramebuffers(1, &msFbo_);
    glGenRenderbuffers(1, &msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, width_, height_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    updateCamera();
}

GroceriesUniverse::~GroceriesUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progBox_, progBlob_, progSky_, progSprite_, progLine_, progFlake_, progVeil_, progDown_, progBlur_, progFinal_, progShape_,
                     progText_})
        glDeleteProgram(p);
    for (GLuint v : {staticVao_, roofVao_, boxVao_, blobVao_, sblobVao_, spriteVao_, lineVao_, flakeVao_, shapeVao_, textVao_})
        glDeleteVertexArrays(1, &v);
    for (GLuint b : {cubeVbo_, quadVbo_, stripVbo_, staticVbo_, roofVbo_, boxVbo_, blobVbo_, sblobVbo_, spriteVbo_, lineVbo_, flakeVbo_,
                     shapeVbo_, textVbo_})
        glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    for (Font* f : {&bold_, &medium_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The cameras
// =====================================================================================================

void GroceriesUniverse::updateCamera() {
    const float z = std::clamp(zoom_, 0.f, (float)(CAM_KEY_COUNT - 1) - 0.001f);
    const int i = (int)z;
    const float t = z - i;
    const float e = t * t * t * (t * (t * 6.f - 15.f) + 10.f);
    const CamKey& a = CAM_KEYS[i];
    const CamKey& b = CAM_KEYS[std::min(i + 1, CAM_KEY_COUNT - 1)];
    // the eye moves in a log-like way between far-apart stages (slow near, fast far)
    const float da = length(a.eye - a.target), db = length(b.eye - b.target);
    const float d = std::exp(lerpf(std::log(da), std::log(db), e));
    const vec3 fa = normalize(a.target - a.eye), fb = normalize(b.target - b.eye);
    const vec3 F = normalize(lerp(fa, fb, e));
    const vec3 T = lerp(a.target, b.target, e);
    eyeW_ = T - F * d;
    camF_ = F;
    camR_ = normalize(cross(F, vec3(0.f, 1.f, 0.f)));
    camU_ = cross(camR_, F);
    const float half = std::atan(1.f / focal_);
    const float nearP = std::max(0.5f, d * 0.004f), farP = std::max(2600.f, d * 7.f + 5000.f);
    mat4 shift = mat4::identity();
    shift.at(1, 3) = shift0_ * (1.f - smoothstepf(0.f, 1.f, zoom_));
    vpW_ = shift * mat4::perspective(2.f * half, aspect_, nearP, farP) * mat4::lookAt(eyeW_, eyeW_ + F, vec3(0.f, 1.f, 0.f));
    invVpW_ = inverse(vpW_);
}

vec2 GroceriesUniverse::project(const vec3& p) const { return layout::project(vpB_, p, width_, height_); }

// A world point -> the board point (z = 0 plane's depth adjusted) that lands on the same pixel, and how much smaller
// things there look than on the board.
vec3 GroceriesUniverse::sinkBoard(float* scale) const {
    const vec4 c = vpW_ * vec4(sinkW_, 1.f);
    const float w = std::max(c.w, 1e-3f);
    const float nx = c.x / w, ny = c.y / w;
    // the board camera: plane z = zb at distance (eyeB.z - zb); place the point on a plane a little in front
    const float zb = 1.5f;
    const float dist = eyeB_.z - zb;
    const float x = nx * dist / (focal_ / aspect_) + eyeB_.x;
    const float y = (ny - shift0_) * dist / focal_ + eyeB_.y;
    if (scale) {
        // a unit at the sink, seen by the world camera, vs a unit on the board's plane
        const float worldPx = focal_ / w;
        const float boardPx = focal_ / dist;
        *scale = std::clamp(worldPx / boardPx, 0.f, 1.5f);
    }
    return vec3(x, y, zb);
}

// Where the bags are being filled: the stand while it is in view, then a heap the camera can see on a side.
vec3 GroceriesUniverse::chooseSink() {
    if (zoom_ < 0.55f) return STAND_POS + vec3(0.f, BAG_SY * 2.f, 0.f);
    std::vector<vec3> cands;
    const float L = (float)lines_;
    for (int k = 1; k < LANES; k++)
        if (L >= 4.f + 4.f * k) cands.push_back(vec3(-112.f, Y_FLOOR + 6.f, -k * LANE_DZ - 12.f));
    cands.push_back(queueSlot(std::max(0, cartsArrived_ - 1)) + vec3(0.f, 9.f, 0.f));
    for (const Van& v : vans_)
        if (v.state == 1) {
            const float side = v.bay < 4 ? -1.f : 1.f;
            cands.push_back(vec3(side * (560.f + 34.f * (v.bay % 4)), Y_FLOOR + 10.f, Z_FRONT + 60.f));
        }
    for (int i = 0; i < MOUNTAINS; i++) {
        const float H = mountainH(i, gL_);
        if (H < 30.f) continue;
        cands.push_back(vec3(MOUNT_X[i], Y_FLOOR + H * 0.95f, MOUNT_Z[i]));
    }
    // the last candidate in view on a side wins (the newest features come last)
    vec3 best = cands.front();
    bool found = false;
    for (const vec3& c : cands) {
        const vec4 p = vpW_ * vec4(c, 1.f);
        if (p.w <= 0.f) continue;
        const float nx = p.x / p.w, ny = p.y / p.w;
        if (std::fabs(nx) > 0.56f && std::fabs(nx) < 0.97f && ny > -0.9f && ny < 0.8f) { best = c; found = true; }
    }
    if (!found) {
        // nothing on the sides: the nearest candidate to a side
        float bd = 1e9f;
        for (const vec3& c : cands) {
            const vec4 p = vpW_ * vec4(c, 1.f);
            if (p.w <= 0.f) continue;
            const float d = std::fabs(std::fabs(p.x / p.w) - 0.75f) + std::fabs(p.y / p.w) * 0.3f;
            if (d < bd) { bd = d; best = c; }
        }
    }
    return best;
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void GroceriesUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 1;                         // late night
        else if (t.hasSong && t.bassWeight < 0.45f && t.airWeight > 0.5f) v = 2;          // co-op
        else if (t.hasSong && !t.minor && t.brightness > 0.55f) v = 0;                    // morning
        else if (t.hasSong && t.minor) v = 3;                                             // blue hour
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    palT_ = makePalette(v);
    const bool instant = !palSet_ || seconds <= 0.1f;
    if (instant) { pal_ = palT_; palSet_ = true; }
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x6E0Cull);
    worldDirty_ = true;
    buildRoof();
}

void GroceriesUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    (void)impact;
    if (level < 2) dropMode_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.f, 6.f);
}

void GroceriesUniverse::drop(float strength) {
    dropMode_ = true;
    dropT_ = 0.f;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
}

void GroceriesUniverse::refresh(float strength) { refreshK_ = std::max(refreshK_, strength); }

TransitionHints GroceriesUniverse::hints() const {
    TransitionHints h;
    h.background = lerp(pal_.floor * pal_.inTop * 1.4f, pal_.veil, 0.4f);
    h.light = ugl::luma(h.background) > 0.3f;
    h.ink = h.light ? pal_.veil : vec3(0.92f);
    h.accent = pal_.accent;
    h.styles = TS_REBUILD | TS_TILES | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void GroceriesUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock:
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) seeds_[c.y][c.x] = rng_.uniform();
        break;
    case GameEvent::Spawn: {
        // the front product of our belt is taken toward the well
        int front = -1;
        for (size_t i = 0; i < riders_.size(); i++)
            if (riders_[i].kind == 0 && (front < 0 || riders_[i].x < riders_[front].x)) front = (int)i;
        if (front >= 0) {
            if (zoom_ < 0.3f && riders_[front].x < 13.f) {
                Hop h;
                h.p0 = vec3(riders_[front].x, Y_CT + 0.62f, -0.7f);
                h.fam = riders_[front].fam;
                h.seed = riders_[front].seed;
                hops_.push_back(h);
            }
            if (riders_[front].x < 13.f) riders_.erase(riders_.begin() + front);
        }
        break;
    }
    case GameEvent::Clear: {
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        if (ev.count >= 4) tetrisT_ = 0.f;
        pending_ += std::max(1, ev.count);
        // the scanner slides to the rows; its beam sweeps them right to left
        float y0 = 1e9f, y1 = -1e9f;
        for (int r : rows) {
            const float y = layout::cellPos(0.f, (float)r).y;
            y0 = std::min(y0, y);
            y1 = std::max(y1, y);
        }
        if (!rows.empty()) {
            scanFrom_ = scanY_;
            scanTo_ = (y0 + y1) * 0.5f;
            scanH_ = (y1 - y0) * 0.5f + 0.55f;
            scanT_ = 0.f;
        }
        int order = 0;
        for (auto& c : ev.cells) {
            if (c.x < 0 || c.x >= Game::W || c.y < Game::HIDDEN) continue;
            Flight fl;
            fl.p0 = layout::cellPos((float)c.x, (float)c.y);
            fl.cur = fl.p0;
            fl.type = c.type;
            fl.kind = 0;
            fl.seed = (c.y < Game::H && c.y >= 0) ? seeds_[c.y][c.x] : rng_.uniform();
            // scanned when the beam passes it (right to left), then tossed after a beat
            const float tx = SCAN_LEAD + (4.5f - fl.p0.x) / 9.f * SCAN_SWEEP;
            const int k = (int)(std::find(rows.begin(), rows.end(), c.y) - rows.begin());
            fl.delay = tx + 0.06f + 0.035f * (float)((9 - c.x) + 10 * k) * 0.55f;
            fl.dur = 0.8f + 0.12f * rng_.uniform();
            fl.spin = rng_.range(-1.f, 1.f);
            fl.scale = 1.f;
            flights_.push_back(fl);
            order++;
        }
        (void)order;
        // the seeds of the rows above step down
        {
            bool cleared[Game::H] = {};
            for (int y : rows) cleared[y] = true;
            float ns[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                for (int x = 0; x < Game::W; x++) ns[dst][x] = seeds_[y][x];
                dst--;
            }
            std::memcpy(seeds_, ns, sizeof(seeds_));
        }
        break;
    }
    case GameEvent::TopOut: {
        // the stack is swept off the counter, product after product, from the top
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            Flight fl;
            fl.p0 = fl.cur = layout::cellPos((float)c.x, (float)c.y);
            fl.type = c.type;
            fl.kind = 1;
            fl.seed = seeds_[c.y][c.x];
            fl.delay = 0.07f * (c.y - top) + 0.2f * hashf((uint64_t)(c.x * 17 + c.y * 3));
            fl.vel = vec3(rng_.range(-3.f, 3.f) + (c.x - 4.5f) * 0.5f, rng_.range(2.f, 6.f), rng_.range(3.f, 7.f));
            fl.spin = rng_.range(-4.f, 4.f);
            fl.dur = 2.4f;
            flights_.push_back(fl);
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The board: the products, the well's frame, the scanner, the previews
// =====================================================================================================

void GroceriesUniverse::product(std::vector<Box>& out, const vec3& c, float half, int type, float seed, float flash, float rot) {
    type = std::clamp(type, 0, 6);
    if (rot == 0.f) {
        box(out, c, vec3(half), vec3(1.f), M_PRODUCT, (float)type, flash, 0, seed);
        return;
    }
    const float ca = std::cos(rot), sa = std::sin(rot), cb = std::cos(rot * 0.6f), sb = std::sin(rot * 0.6f);
    // about z, then about x
    const vec3 ax(ca, sa, 0.f), ay0(-sa, ca, 0.f), az0(0.f, 0.f, 1.f);
    const vec3 ay = ay0 * cb + az0 * sb, az = az0 * cb - ay0 * sb;
    obox(out, c, ax * half, ay * half, az * half, vec3(1.f), M_PRODUCT, (float)type, flash, 0, seed);
}

void GroceriesUniverse::buildFrame() {
    const Palette& P = pal_;
    // the well's rails and its tray on the counter
    for (int s = -1; s <= 1; s += 2) box(board_, vec3(s * 5.2f, 0.f, -0.05f), vec3(0.13f, 10.2f, 0.55f), P.steel, M_STEEL);
    box(board_, vec3(0.f, -10.17f, -0.05f), vec3(5.33f, 0.13f, 0.62f), P.steel, M_STEEL);
    // the scanner on the right rail: a dark carriage, its red window
    const float y = scanY_;
    box(board_, vec3(5.62f, y, 0.05f), vec3(0.34f, 0.7f, 0.5f), vec3(0.1f, 0.105f, 0.11f), M_PAINT, 0.5f);
    const float on = scanT_ < SCAN_LEAD + SCAN_SWEEP + 0.05f ? 1.f : 0.f;
    box(board_, vec3(5.62f, y, 0.56f), vec3(0.2f, 0.42f, 0.03f), vec3(1.f, 0.15f, 0.1f), M_EMIT, 0.5f + 2.5f * on);
}

void GroceriesUniverse::buildBoard(const Game& g) {
    cells_.clear();
    const float half = CELL;
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            product(cells_, p, half, c.type, seeds_[y][x]);
        }
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
            if (cells[i][1] + dy < Game::HIDDEN - 0.5f) continue;
            product(cells_, w, half, p.type, 0.13f * i + 0.07f * p.type);
        }
        // the ghost: a hairline outline where it lands
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto has = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            const vec3 col = lerp(pal_.famA[std::clamp(p.type, 0, 6)], vec3(1.f), 0.45f);
            const float wpx = std::max(1.f, 1.5f * height_ / 1080.f);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                const float h = 0.47f, z = 0.47f;
                if (!has(c[0], c[1] - 1)) line(boardLines_, vec3(w.x - h, w.y + h, z), vec3(w.x + h, w.y + h, z), wpx, col, 0.7f);
                if (!has(c[0], c[1] + 1)) line(boardLines_, vec3(w.x - h, w.y - h, z), vec3(w.x + h, w.y - h, z), wpx, col, 0.7f);
                if (!has(c[0] - 1, c[1])) line(boardLines_, vec3(w.x - h, w.y - h, z), vec3(w.x - h, w.y + h, z), wpx, col, 0.7f);
                if (!has(c[0] + 1, c[1])) line(boardLines_, vec3(w.x + h, w.y - h, z), vec3(w.x + h, w.y + h, z), wpx, col, 0.7f);
            }
        }
    }
    // the previews: the held product in a basket, the next ones on little shelves
    auto preview = [&](int type, vec2 center, float sc, float dim) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        g.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (int i = 0; i < 4; i++) {
            const float ox = cells[i][0] - (minx + maxx) * 0.5f, oy = cells[i][1] - (miny + maxy) * 0.5f;
            box(cells_, vec3(center.x + ox * sc, center.y - oy * sc, 0.f), vec3(half * sc), vec3(dim), M_PRODUCT, (float)type, 0, 0, 0.21f * i);
        }
        (void)maxy;
    };
    if (!g.over()) {
        const float hs = layout::previewScale(0);
        const vec3 hc = layout::HOLD_CENTER;
        preview(g.holdType(), vec2{hc.x, hc.y}, hs, g.holdUsed() ? 0.5f : 1.f);
        // the basket around the held product
        box(board_, vec3(hc.x, hc.y - 0.35f, -0.1f), vec3(1.6f, 0.75f, 0.75f), pal_.cart * 0.8f, M_WIRE);
        box(board_, vec3(hc.x, hc.y + 0.38f, 0.64f), vec3(1.62f, 0.06f, 0.05f), pal_.accent, M_PAINT, 0.3f);
        for (int s = -1; s <= 1; s += 2)
            box(board_, vec3(hc.x + s * 1.05f, hc.y + 0.62f, -0.1f), vec3(0.1f, 0.16f, 0.55f), pal_.accent * 0.85f, M_PAINT, 0.3f);
        for (int i = 0; i < 3; i++) {
            const vec3 c = layout::nextCenter(i);
            const float sc = layout::previewScale(i);
            preview(g.next(i), vec2{c.x, c.y}, sc, 1.f);
            // a little shelf under it
            float lowest = 0.f;
            if (g.next(i) >= 0) {
                Piece p{g.next(i), 0, 0, 0};
                int cl[4][2];
                g.pieceCells(p, cl);
                float miny = 9, maxy = -9;
                for (auto& q : cl) { miny = std::min(miny, (float)q[1]); maxy = std::max(maxy, (float)q[1]); }
                lowest = (maxy - miny) * 0.5f * sc + half * sc;
            }
            box(board_, vec3(c.x, c.y - lowest - 0.08f, -0.2f), vec3(1.55f, 0.08f, 0.55f), pal_.steel, M_STEEL);
        }
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float GroceriesUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) w += f.g[glyphIndex((unsigned char)s[i])].adv * k + track;
    return w - track;
}

void GroceriesUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = size / f.px;
    const float baseline = y + f.capH * k;
    float pen = x;
    for (size_t i = 0; i < s.size(); i++) {
        const Glyph& g = f.g[glyphIndex((unsigned char)s[i])];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) out.insert(out.end(), q, q + 4);
        }
        pen += g.adv * k + track;
    }
}

void GroceriesUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", haloA);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void GroceriesUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void GroceriesUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    // clean white lettering, like a shop's digital price displays, on a soft dark halo; the accent the store's color
    const vec3 ink(0.95f, 0.95f, 0.93f), halo = ugl::toSrgb(pal_.veil) * 0.8f;
    const vec3 accent = lerp(ugl::toSrgb(pal_.accent), vec3(1.f), 0.2f);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
    // the bonus gauge: a fine ring, its charge in the accent, ticked
    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 120;
        const float th = std::max(1.2f, 1.6f * s), th2 = std::max(2.5f, 5.f * s);
        auto seg = [&](float a0, float a1, float rr, float w, const vec3& col, float al) {
            const float s0 = std::sin(a0), c0 = std::cos(a0), s1 = std::sin(a1), c1 = std::cos(a1);
            const float ri = rr - w * 0.5f, ro = rr + w * 0.5f;
            const float pts[4][2] = {{c.x + s0 * ri, c.y - c0 * ri}, {c.x + s0 * ro, c.y - c0 * ro}, {c.x + s1 * ro, c.y - c1 * ro}, {c.x + s1 * ri, c.y - c1 * ri}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        };
        const float dimk = vis * (1.f - 0.5f * dimT_);
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, r, th, ink, (g.bonusReady() ? 0.55f : 0.25f) * dimk);
        for (int i = 0; i < Game::BONUS_LINES; i++) {
            const float a = TAU * i / Game::BONUS_LINES;
            seg(a - 0.012f, a + 0.012f, r - th2, th2 * 0.9f, ink, 0.35f * dimk);
        }
        flushShapes();
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, r, th2, accent, 0.95f * dimk);
        }
        flushShapes();
    }
    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha) * vis;
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        auto upper = [](std::string t) {
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(bold_, upper(h.text), base * 0.72f, base * 0.2f, base * 0.06f);
            drawText(bold_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.75f * vis);
            continue;
        case HUD_SECTION:
            place(bold_, upper(h.text), base * 0.8f, base * 0.24f, -base * 0.04f);
            drawText(bold_, q, accent, halo, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(bold_, h.text, base * 1.1f, base * 0.0f, -base * 0.1f);
            drawText(bold_, q, ink, halo, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(medium_, upper(h.text), base * 0.64f, base * 0.16f);
            drawText(medium_, q, ink, halo, saturate(alpha * 1.2f) * 0.75f);
            continue;
        case HUD_LABEL:
            place(bold_, upper(h.text), base * 0.64f, base * 0.28f);
            drawText(bold_, q, ink, halo, saturate(alpha * 1.2f) * 0.62f);
            continue;
        case HUD_VALUE:
            place(bold_, h.text, base * 1.0f, base * 0.04f, -base * 0.04f);
            drawText(bold_, q, ink, halo, alpha * 0.96f);
            continue;
        case HUD_NOTICE:
            place(bold_, upper(h.text), base * 0.9f, base * 0.16f);
            drawText(bold_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(bold_, h.text, base * 0.84f, base * 0.08f);
            drawText(bold_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(bold_, upper(h.text), base * 1.0f, base * 0.26f, -base * 0.1f);
            drawText(bold_, q, ink, halo, alpha, 0.65f);
            continue;
        case HUD_TOAST_SUB:
            place(bold_, h.text, base * 0.95f, base * 0.08f);
            drawText(bold_, q, accent, halo, alpha * 0.9f, 0.65f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(bold_, upper(h.text), base * 0.9f, base * 0.22f);
                drawText(bold_, q, ink, halo, alpha, 0.7f);
            } else {
                place(medium_, h.text, base * 0.95f, base * 0.02f);
                drawText(medium_, q, h.accent ? accent : ink, halo, alpha, 0.7f);
            }
            continue;
        default:
            place(medium_, h.text, base * 0.78f, base * 0.04f);
            drawText(medium_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// The store's moving parts
// =====================================================================================================

void GroceriesUniverse::line(std::vector<Line>& out, const vec3& a, const vec3& b, float wpx, const vec3& col, float alpha) {
    Line l;
    l.a[0] = a.x; l.a[1] = a.y; l.a[2] = a.z; l.a[3] = wpx;
    l.b[0] = b.x; l.b[1] = b.y; l.b[2] = b.z; l.b[3] = alpha;
    l.c[0] = col.x; l.c[1] = col.y; l.c[2] = col.z; l.c[3] = 0.f;
    out.push_back(l);
}

void GroceriesUniverse::sprite(std::vector<Sprite>& out, const vec3& p, float r, const vec3& col, float k) {
    if (k <= 0.002f) return;
    Sprite s;
    s.p[0] = p.x; s.p[1] = p.y; s.p[2] = p.z; s.p[3] = r;
    s.c[0] = col.x; s.c[1] = col.y; s.c[2] = col.z; s.c[3] = k;
    out.push_back(s);
}

// Vans at the store front, lorries at its side: arrive, load, leave.
void GroceriesUniverse::updateVans(float dt, float wdt) {
    (void)dt;
    const float L = gL_;
    const int bays = L < 36.f ? 0 : std::min(8, 1 + (int)((L - 36.f) / 3.f));
    const int docks = L < 50.f ? 0 : std::min(5, 1 + (int)((L - 50.f) / 5.f));
    auto ensure = [&](int bay, bool lorry) {
        for (const Van& v : vans_)
            if (v.bay == bay && v.lorry == lorry) return;
        Van v;
        v.bay = bay;
        v.lorry = lorry;
        v.state = 0;
        v.t = 0.f;
        v.seed = rng_.uniform();
        vans_.push_back(v);
    };
    for (int b = 0; b < bays; b++) ensure(b % 2 ? 4 + b / 2 : b / 2, false);
    for (int d = 0; d < docks; d++) ensure(d, true);
    for (Van& v : vans_) {
        if (v.state == 0) { v.t += wdt / (v.lorry ? 7.f : 5.f); if (v.t >= 1.f) { v.state = 1; v.t = 0.f; v.fill = 0.f; } }
        else if (v.state == 1) { v.fill += wdt / ((v.lorry ? 20.f : 12.f) + 4.f * v.seed); if (v.fill >= 1.f) { v.state = 2; v.t = 0.f; } }
        else if (v.state == 2) { v.t += wdt / (v.lorry ? 7.f : 5.f); if (v.t >= 1.f) { v.state = 3; v.t = 0.f; } }
        else { v.t += wdt; if (v.t > 2.5f) { v.state = 0; v.t = 0.f; v.seed = rng_.uniform(); } }
    }
}

void GroceriesUniverse::buildVans() {
    const Palette& P = pal_;
    const float L = gL_;
    int wings = 0;
    if (L >= 66.f) wings = std::min(3, 1 + (int)((L - 62.f) / 9.f));
    const vec3 bodyCols[4] = {vec3(0.86f), P.accent, lin(0xe8e4dc), P.fabric[3]};
    for (const Van& v : vans_) {
        float travel = 0.f;
        if (v.state == 0) travel = 1.f - easeOut(v.t);
        else if (v.state == 2) travel = v.t * v.t;
        else if (v.state == 3) continue;
        const vec3 body = bodyCols[(int)(v.seed * 4.f) % 4];
        if (!v.lorry) {
            // nose in toward the store, its rear doors open toward the car park
            const float side = v.bay < 4 ? -1.f : 1.f;
            const float x = side * (560.f + 34.f * (v.bay % 4));
            const float z = Z_FRONT + 52.f + travel * 520.f;
            const float y = Y_FLOOR;
            box(boxes_, vec3(x, y + 9.f, z - 23.f), vec3(9.f, 7.f, 6.f), body, M_VAN, 1.f, 0, 0, v.seed);
            // the cargo box, open at the back: two walls, a roof, a floor
            for (int s = -1; s <= 1; s += 2) box(boxes_, vec3(x + s * 9.6f, y + 12.f, z + 3.f), vec3(0.4f, 10.f, 20.f), body, M_VAN, 0.f, 0, 0, v.seed);
            box(boxes_, vec3(x, y + 21.6f, z + 3.f), vec3(10.f, 0.4f, 20.f), body, M_PAINT, 0.3f);
            box(boxes_, vec3(x, y + 2.4f, z + 3.f), vec3(10.f, 0.4f, 20.f), vec3(0.2f), M_PAINT);
            box(boxes_, vec3(x, y + 12.f, z - 16.6f), vec3(10.f, 10.f, 0.4f), body, M_PAINT, 0.3f);
            // the rear doors, swung open
            for (int s = -1; s <= 1; s += 2) box(boxes_, vec3(x + s * 14.5f, y + 12.f, z + 23.f), vec3(4.8f, 9.5f, 0.3f), body, M_PAINT, 0.3f);
            for (int s = -1; s <= 1; s += 2)
                for (int f = -1; f <= 1; f += 2) box(boxes_, vec3(x + s * 8.f, y + 1.6f, z + f * 14.f), vec3(1.2f, 1.6f, 2.f), vec3(0.04f), M_PAINT);
            // its load: rows of bags from the front
            const int n = (int)(v.fill * 18.f + (v.state == 2 ? 18.f : 0.f));
            for (int i = 0; i < std::min(n, 18); i++) {
                const float h = hashf((uint64_t)(v.seed * 1e4f) + i);
                vec3 fab;
                const int stl = fabricStyle(h, fab);
                buildBag(boxes_, vec3(x - 5.f + (i % 3) * 5.f, y + 2.8f + ((i / 3) % 2) * 5.f, z - 13.f + (i / 6) * 4.f), h * 0.4f, 1.1f, stl, fab, 0.9f,
                         h * 10.f, 0, 0, false);
            }
            // bags waiting on the ground behind it
            if (v.state == 1)
                for (int i = 0; i < (int)((1.f - v.fill) * 8.f); i++) {
                    const float h = hashf((uint64_t)(v.seed * 2e4f) + i);
                    vec3 fab;
                    const int stl = fabricStyle(h, fab);
                    buildBag(boxes_, vec3(x - 6.f + (i % 4) * 4.f, y, z + 30.f + (i / 4) * 3.f), h, 1.f, stl, fab, 0.9f, h * 10.f, 0, 0, false);
                }
            blob(blobs_, vec3(x, Y_FLOOR + 0.08f, z - 2.f), 12.f, 30.f, 0.5f, 0.5f);
        } else {
            // a lorry backed up to the side wall
            const float xw = -(X_WALL + MODULE_W * wings) - 4.f;
            const float z = -110.f - 75.f * v.bay;
            const float x = xw - 70.f - travel * 900.f;
            const float y = Y_FLOOR;
            box(boxes_, vec3(x, y + 18.f, z), vec3(66.f, 13.f, 12.f), lin(0xf0eee8), M_VAN, 0.f, 0, 0, v.seed);
            box(boxes_, vec3(x - 80.f, y + 13.f, z), vec3(11.f, 11.f, 11.5f), body, M_VAN, 1.f, 0, 0, v.seed);
            for (int i = 0; i < 4; i++)
                for (int s = -1; s <= 1; s += 2)
                    box(boxes_, vec3(x + (i < 2 ? 40.f + i * 9.f : -78.f + (i - 2) * 14.f), y + 2.4f, z + s * 10.f), vec3(2.4f, 2.4f, 1.6f), vec3(0.04f),
                        M_PAINT);
            // pallets of bags waiting at the dock
            if (v.state == 1) {
                const int n = (int)((1.f - v.fill) * 3.f) + 1;
                for (int i = 0; i < n; i++) {
                    const vec3 pc(xw + 12.f, Y_FLOOR, z - 8.f + i * 8.f);
                    box(boxes_, pc + vec3(0.f, 0.6f, 0.f), vec3(4.f, 0.6f, 3.6f), lin(0xc8a878), M_WOOD);
                    for (int b = 0; b < 4; b++) {
                        const float h = hashf((uint64_t)(v.seed * 3e4f) + i * 7 + b);
                        vec3 fab;
                        const int stl = fabricStyle(h, fab);
                        buildBag(boxes_, pc + vec3((b % 2) * 3.6f - 1.8f, 1.2f, (b / 2) * 2.5f - 1.25f), h, 0.95f, stl, fab, 0.9f, h * 10.f, 0, 0, false);
                    }
                }
            }
            blob(blobs_, vec3(x - 10.f, Y_FLOOR + 0.08f, z), 85.f, 15.f, 0.5f, 0.4f);
        }
    }
    // a convoy of lorries on the road (the absurd stages)
    if (L >= 108.f) {
        const int n = std::min(18, 4 + (int)((L - 108.f) / 4.f));
        for (int i = 0; i < n; i++) {
            const float dir = i % 2 ? 1.f : -1.f;
            const float x = std::fmod(worldTime_ * 70.f * dir + i * 600.f + 50000.f, 9000.f) - 4500.f;
            const float z = CAR_Z1 + 32.f + dir * 10.f;
            box(boxes_, vec3(x, Y_FLOOR + 18.f, z), vec3(66.f, 13.f, 9.f), lin(0xf0eee8), M_VAN, 0.f, 0, 0, i * 0.37f);
            box(boxes_, vec3(x + dir * 80.f, Y_FLOOR + 13.f, z), vec3(11.f, 11.f, 9.f), bodyCols[i % 4], M_VAN, 1.f, 0, 0, i * 0.2f);
        }
    }
}

// =====================================================================================================
// Drawing
// =====================================================================================================

void GroceriesUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    // ZEN_GPUTIME: the GPU time of a frame, printed every 240 frames
    static const bool gpuTime = std::getenv("ZEN_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    lerpPal(pal_, palT_, 1.f - std::exp(-dt * 1.2f));
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += f.paused ? 0.0 : (double)dt;
    worldTime_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && overT_ < 0.f && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    dropT_ += dt;
    tetrisT_ += dt;
    if (isDrop && !wasDrop_) {
        // the millionth customer: a burst of confetti
        for (int i = 0; i < 320; i++) {
            FlakeP p;
            p.p = {rng_.range(-22.f, 22.f), rng_.range(12.5f, 26.f)};
            p.v = {rng_.range(-1.5f, 1.5f), rng_.range(-3.f, 0.f)};
            p.z = rng_.range(-6.f, -1.2f);
            p.size = rng_.range(0.13f, 0.24f);
            p.ang = rng_.range(0.f, TAU);
            p.angV = rng_.range(-3.f, 3.f);
            p.flip = rng_.range(0.f, TAU);
            p.flipV = rng_.range(4.f, 10.f);
            p.age = 0.f;
            p.life = rng_.range(5.f, 8.f);
            p.col = pal_.confetti[rng_.irange(0, 4)];
            flakes_.push_back(p);
        }
        swell_ = 1.f;
    }
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 2.f : 0.5f, dt);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    refreshK_ = approach(refreshK_, 0.f, 0.6f, dt);
    const float kick = kickIn_;
    kickIn_ = 0.f;
    kickGlow_ = std::max(approach(kickGlow_, 0.f, 6.f, dt), kick);
    // the beat
    bool newBeat = false;
    if (music.hasSong) {
        const double b = std::floor(music.beat);
        if (b != lastBeat_) { newBeat = lastBeat_ >= 0.0; lastBeat_ = b; }
    } else {
        static float acc = 0.f;
        acc += wdt * 2.f;
        if (acc > 1.f) { acc -= 1.f; newBeat = true; }
    }
    beatFlash_ = approach(beatFlash_, 0.f, 7.f, dt);
    if (newBeat && dropAmt_ > 0.3f) beatFlash_ = 1.f;
    // the belts' pace
    const float beltT = (0.35f + 0.65f * wm + 0.7f * wp + 0.6f * dropAmt_) * (1.f - 0.7f * slow_);
    beltK_ = approach(beltK_, beltT, 1.5f, dt);
    beltPhase_ += wdt * beltK_;

    // ---- The lines -> the growth.
    const int extra = extraLines(time_);
    const int lines = game.lines() + extra;
    if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
        // a new game: the store opens again
        gL_ = (float)lines;
        gLVel_ = 0.f;
        pending_ = 0;
        cartBags_ = 0;
        cartsArrived_ = 0;
        rolls_.clear();
        lifts_.clear();
        standFill_ = 0;
        vans_.clear();
        worldDirty_ = true;
    }
    lines_ = lines;
    if (linesShown_ < 0) {
        gL_ = (float)lines;
        cartBags_ = lines % 6;
        cartsArrived_ = lines / 6;
        lastExtra_ = extra;
    }
    linesShown_ = lines;
    {
        // extra lines (the test hooks) bring their bags straight to the cart
        if (lastExtra_ < 0) lastExtra_ = extra;
        if (extra > lastExtra_) cartBags_ += extra - lastExtra_;
        lastExtra_ = extra;
        const float w = 2.6f;
        gLVel_ += (w * w * ((float)lines - gL_) - 2.f * w * gLVel_) * wdt;
        gL_ += gLVel_ * wdt;
        if (gL_ > lines) { gL_ = (float)lines; gLVel_ = 0.f; }
        const float zt = zoomOf(gL_);
        const float wz = 1.1f;
        zoomVel_ += (wz * wz * (zt - zoom_) - 2.f * wz * zoomVel_) * wdt;
        zoom_ += zoomVel_ * wdt;
        if (linesShown_ >= 0 && std::fabs(zoom_ - zt) > 3.f) { zoom_ = zt; zoomVel_ = 0.f; }
    }
    updateCamera();
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Our lane: the bag on the stand, the bags lifted into the cart, full carts rolling to the queue.
    standPop_ = approach(standPop_, 1.f, 6.f, dt);
    while (standFill_ >= 10) {
        standFill_ -= 10;
        Lift l;
        l.style = standStyle_;
        l.seed = standSeed_;
        fabricStyle(standSeed_, l.fabric);
        l.from = STAND_POS;
        l.to = CART_POS + vec3(-2.6f + (cartBags_ % 3) * 2.9f, 5.5f, (cartBags_ / 3 % 2 ? 1.f : -1.f) * 1.25f);
        lifts_.push_back(l);
        standSeed_ = rng_.uniform();
        standStyle_ = fabricStyle(standSeed_, l.fabric);
        standPop_ = 0.f;
    }
    for (Lift& l : lifts_) {
        l.t += dt / 0.95f;
        if (l.t >= 1.f) {
            cartBags_++;
            pending_ = std::max(0, pending_ - 1);
        }
    }
    lifts_.erase(std::remove_if(lifts_.begin(), lifts_.end(), [](const Lift& l) { return l.t >= 1.f; }), lifts_.end());
    while (cartBags_ >= 6) {
        cartBags_ -= 6;
        if (rolls_.size() < 3) {
            Roll r;
            r.slot = cartsArrived_ + (int)rolls_.size();
            r.seed = rng_.uniform();
            rolls_.push_back(r);
            cartIn_ = 0.f;
        } else {
            cartsArrived_++;
        }
    }
    for (Roll& r : rolls_) {
        r.t += wdt / 3.6f;
        if (r.t >= 1.f) cartsArrived_++;
    }
    rolls_.erase(std::remove_if(rolls_.begin(), rolls_.end(), [](const Roll& r) { return r.t >= 1.f; }), rolls_.end());
    cartIn_ = approach(cartIn_, 1.f, 1.6f, wdt);
    if (worldDirty_ || builtL_ != lines_ || builtCarts_ != cartsArrived_ || builtVariant_ != variant_) {
        buildWorld();
        glBindBuffer(GL_ARRAY_BUFFER, staticVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(static_.size() * sizeof(Box)), static_.data(), GL_STATIC_DRAW);
        staticCount_ = static_.size();
        glBindBuffer(GL_ARRAY_BUFFER, sblobVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(staticBlobs_.size() * sizeof(Blob)), staticBlobs_.data(), GL_STATIC_DRAW);
        staticBlobCount_ = staticBlobs_.size();
        glBindBuffer(GL_ARRAY_BUFFER, roofVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(roof_.size() * sizeof(Box)), roof_.data(), GL_STATIC_DRAW);
        roofCount_ = roof_.size();
    }
    sinkW_ = chooseSink();

    // ---- Lanes: open ones lit, their scanners beeping on the beat.
    for (int k = 0; k < LANES; k++) {
        const float want = (k == 0 || gL_ >= 4.f + 4.f * k) ? 1.f : 0.f;
        laneOn_[k] = approach(laneOn_[k], want, want > laneOn_[k] ? 1.5f : 3.f, dt);
        laneBeep_[k] = approach(laneBeep_[k], 0.f, 9.f, dt);
        if (newBeat && laneOn_[k] > 0.5f && rng_.chance(0.12f + 0.25f * wm + 0.3f * wp)) laneBeep_[k] = 1.f;
    }
    updateVans(dt, wdt);

    // ---- Our belt: products riding in, a divider between shoppers.
    {
        const float speed = 5.f * beltK_;
        if (riders_.empty()) {
            // the belt starts out loaded
            for (float x = 11.f; x < 33.f; x += rng_.range(1.25f, 2.6f)) {
                Rider r;
                r.x = x;
                r.kind = rng_.chance(0.1f) ? 1 : 0;
                r.fam = rng_.irange(0, 6);
                r.seed = rng_.uniform();
                riders_.push_back(r);
            }
        }
        std::sort(riders_.begin(), riders_.end(), [](const Rider& a, const Rider& b) { return a.x < b.x; });
        float limit = 10.4f;
        for (Rider& r : riders_) {
            const float w = r.kind == 1 ? 0.45f : 0.55f;
            const float stop = limit + w;
            r.x = std::max(stop, r.x - speed * wdt);
            limit = r.x + w + 0.12f;
        }
        const float last = riders_.empty() ? 0.f : riders_.back().x;
        riderGap_ -= speed * wdt;
        if (riders_.size() < 26 && last < 33.f && riderGap_ <= 0.f) {
            Rider r;
            r.x = 35.5f;
            if (rng_.chance(0.1f)) r.kind = 1;
            r.fam = rng_.irange(0, 6);
            r.seed = rng_.uniform();
            riders_.push_back(r);
            riderGap_ = rng_.range(0.05f, 1.2f);
        }
    }
    for (Hop& h : hops_) h.t += dt / 0.42f;
    hops_.erase(std::remove_if(hops_.begin(), hops_.end(), [](const Hop& h) { return h.t >= 1.f; }), hops_.end());

    // ---- The cleared products: scanned, tossed into the bags.
    scanT_ += dt;
    {
        const float target = scanT_ < 1.1f ? scanTo_ : -9.35f;
        if (scanT_ < SCAN_LEAD) scanY_ = lerpf(scanFrom_, scanTo_, easeOut(scanT_ / SCAN_LEAD));
        else scanY_ = approach(scanY_, target, scanT_ < 1.1f ? 30.f : 3.f, dt);
        beamX_ = 5.f - 10.f * saturate((scanT_ - SCAN_LEAD) / SCAN_SWEEP);
    }
    float sinkScale = 1.f;
    const vec3 sinkB = sinkBoard(&sinkScale);
    for (Flight& fl : flights_) {
        fl.t += dt;
        if (fl.kind == 1) {
            const float tt = fl.t - fl.delay;
            if (tt > 0.f) {
                fl.vel.y -= 26.f * dt;
                fl.cur = fl.cur + fl.vel * dt;
            }
            continue;
        }
        if (fl.kind == 2) continue;
        const float tt = fl.t - fl.delay;
        if (tt <= 0.f) { fl.cur = fl.p0; continue; }
        const float s = saturate(tt / fl.dur);
        if (!fl.landed && s >= 1.f) {
            fl.landed = true;
            standFill_++;
        }
        const vec3 end = sinkB + vec3((fl.seed - 0.5f) * 1.2f * sinkScale, 0.f, 0.f);
        const vec3 a = fl.p0 + vec3(0.f, 0.f, 0.6f);
        const vec3 ctl = vec3(lerpf(a.x, end.x, 0.35f), std::max(a.y, end.y) + 4.5f + 2.f * fl.seed, lerpf(a.z, end.z, 0.5f) + 2.f);
        const float e = s < 0.5f ? 2.f * s * s : 1.f - 2.f * (1.f - s) * (1.f - s);
        fl.cur = bez(a, ctl, end, 0.15f * s + 0.85f * e);
        fl.scale = lerpf(1.f, std::max(0.05f, 0.82f * sinkScale), smoothstepf(0.1f, 0.9f, s)) * (1.f - smoothstepf(0.9f, 1.f, s));
    }
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(),
                                  [](const Flight& fl) { return (fl.kind == 0 && fl.landed) || (fl.kind == 1 && fl.t > fl.delay + fl.dur); }),
                   flights_.end());

    // ---- Confetti: the drop's burst, then a light shower for the drop's length.
    if (isDrop && overT_ < 0.f) {
        static float acc = 0.f;
        acc += wdt * 26.f;
        while (acc > 1.f) {
            acc -= 1.f;
            FlakeP p;
            p.p = {rng_.range(-22.f, 22.f), 13.f};
            p.v = {rng_.range(-1.f, 1.f), rng_.range(-2.f, -0.5f)};
            p.z = rng_.range(-6.f, -1.2f);
            p.size = rng_.range(0.12f, 0.22f);
            p.ang = rng_.range(0.f, TAU);
            p.angV = rng_.range(-3.f, 3.f);
            p.flip = rng_.range(0.f, TAU);
            p.flipV = rng_.range(4.f, 10.f);
            p.age = 0.f;
            p.life = rng_.range(5.f, 8.f);
            p.col = pal_.confetti[rng_.irange(0, 4)];
            flakes_.push_back(p);
        }
    }
    flakeInst_.clear();
    for (FlakeP& p : flakes_) {
        p.age += wdt;
        p.v.y = approach(p.v.y, -2.2f, 1.6f, wdt);
        p.v.x = approach(p.v.x, std::sin(worldTime_ * 1.3f + p.flip) * 0.8f, 1.f, wdt);
        p.p.x += p.v.x * wdt;
        p.p.y += p.v.y * wdt;
        p.ang += p.angV * wdt;
        p.flip += p.flipV * wdt;
        const float a = smoothstepf(0.f, 0.2f, p.age) * (1.f - smoothstepf(p.life - 1.f, p.life, p.age));
        Flake fk;
        fk.p[0] = p.p.x; fk.p[1] = p.p.y; fk.p[2] = p.z; fk.p[3] = p.size;
        fk.c[0] = p.col.x; fk.c[1] = p.col.y; fk.c[2] = p.col.z; fk.c[3] = a;
        fk.r[0] = p.ang; fk.r[1] = p.flip; fk.r[2] = 0.55f; fk.r[3] = 0.f;
        flakeInst_.push_back(fk);
    }
    flakes_.erase(std::remove_if(flakes_.begin(), flakes_.end(), [](const FlakeP& p) { return p.age > p.life || p.p.y < -14.f; }), flakes_.end());

    // ---- The dynamic world.
    boxes_.clear();
    blobs_.clear();
    sprites_.clear();
    worldLines_.clear();
    const Palette& P = pal_;
    const float zoomNear = 1.f - smoothstepf(1.5f, 2.6f, zoom_);
    if (zoomNear > 0.f) {
        // our belt's riders
        for (const Rider& r : riders_) {
            if (r.x > 34.f) continue;
            if (r.kind == 1) {
                box(boxes_, vec3(r.x, Y_CT + 0.62f, -0.7f), vec3(0.42f, 0.4f, 2.1f), vec3(0.12f), M_PAINT, 0.4f);
                box(boxes_, vec3(r.x, Y_CT + 1.03f, -0.7f), vec3(0.3f, 0.03f, 2.f), P.accent, M_PAINT, 0.2f);
            } else {
                product(boxes_, vec3(r.x, Y_CT + 0.68f, -0.7f + (r.seed - 0.5f) * 1.6f), 0.48f, r.fam, r.seed);
            }
        }
        for (const Hop& h : hops_) {
            const vec3 to(0.5f, 10.5f, 0.f);
            const vec3 ctl(h.p0.x * 0.6f, 8.f, 2.f);
            const float e = easeOut(h.t);
            product(boxes_, bez(h.p0, ctl, to, e), 0.48f * (1.f - smoothstepf(0.6f, 1.f, h.t)), h.fam, h.seed, 0.f, h.t * 3.f);
        }
        // the bag on the stand, filling up
        {
            vec3 fab;
            fabricStyle(standSeed_, fab);
            const float pop = 0.3f + 0.7f * easeOut(standPop_);
            buildBag(boxes_, STAND_POS, 0.f, pop, standStyle_, fab, std::min(1.f, standFill_ / 9.f), standSeed_ * 10.f);
            // the products sitting in it (their tops showing)
            for (int i = 0; i < standFill_; i++) {
                const float h = hashf((uint64_t)(standSeed_ * 1e5f) + i);
                const vec3 pp = STAND_POS + vec3(-1.f + (i % 3) * 1.f, BAG_SY * 2.f * pop - 1.2f + 0.25f * (i / 3) + 0.4f * h, (i % 2 ? 0.4f : -0.4f));
                product(boxes_, pp, 0.42f, (int)(h * 7.f) % 7, h, 0.f, (h - 0.5f) * 0.5f);
            }
        }
        for (const Lift& l : lifts_) {
            const float e = l.t < 0.5f ? 2.f * l.t * l.t : 1.f - 2.f * (1.f - l.t) * (1.f - l.t);
            const vec3 ctl((l.from.x + l.to.x) * 0.5f, std::max(l.from.y, l.to.y) + 9.f, (l.from.z + l.to.z) * 0.5f);
            buildBag(boxes_, bez(l.from, ctl, l.to, e), 0.f, lerpf(1.f, 0.82f, e), l.style, l.fabric, 1.f, l.seed * 10.f);
        }
        // our cart, and the full ones rolling to the queue
        {
            const vec3 cp = CART_POS + vec3(-40.f * (1.f - easeOut(cartIn_)), 0.f, 0.f);
            buildCart(boxes_, &blobs_, cp, 0.f, cartBags_, 0.5f);
            for (const Roll& r : rolls_) {
                const vec3 to = queueSlot(r.slot);
                const float e = r.t * r.t * (3.f - 2.f * r.t);
                const vec3 mid(CART_POS.x - 6.f, Y_FLOOR, (CART_POS.z + to.z) * 0.5f);
                const vec3 p = bez(CART_POS, mid, to, e);
                const vec3 d = bez(CART_POS, mid, to, std::min(1.f, e + 0.02f)) - p;
                const float yaw = length(d) > 1e-4f ? std::atan2(-d.z, d.x) + PI : PI;
                buildCart(boxes_, &blobs_, p, yaw, 6, r.seed * 10.f + 3.f);
            }
        }
        // the open lanes' belts carry products to their scanners
        for (int k = 1; k < LANES; k++) {
            if (laneOn_[k] < 0.05f) continue;
            const float zc = -k * LANE_DZ;
            for (int i = 0; i < 9; i++) {
                const float u = std::fmod(beltPhase_ * 0.12f + i / 9.f + k * 0.13f, 1.f);
                const float x = 32.f - u * 22.f;
                const int fam = (int)(hashf((uint64_t)(k * 100 + i + (int)(beltPhase_ * 0.12f + i / 9.f + k * 0.13f) * 9)) * 7.f);
                product(boxes_, vec3(x, Y_CT + 0.68f, zc - 0.7f), 0.5f * smoothstepf(0.f, 0.06f, u) * laneOn_[k], fam, 0.3f + i);
            }
        }
    }
    buildVans();

    // ---- Lights: groups, glows, point lights.
    float groupK[16];
    const float lightLv = (0.82f + 0.08f * wm + 0.14f * wp + 0.12f * dropAmt_ + 0.15f * swell_ + 0.1f * beatFlash_) * (1.f - 0.55f * slow_);
    for (int k = 0; k < LANES; k++) {
        const float tet = tetrisT_ < 3.f ? std::exp(-std::pow((tetrisT_ - k * 0.12f) * 6.f, 2.f)) : 0.f;
        groupK[k] = (0.05f + 0.95f * laneOn_[k]) * (0.85f + 0.25f * wp + 0.6f * beatFlash_ * dropAmt_) + 1.5f * tet;
    }
    groupK[G_SCAN] = 0.6f + 0.3f * wp + 3.5f * beatFlash_ * dropAmt_ + 0.6f * kickGlow_ * wp;
    groupK[G_TUBES] = lightLv * (1.f - 0.6f * slow_);
    groupK[G_LAMPS] = P.night;
    groupK[G_FRIDGE] = 0.85f + 0.15f * wm;
    groupK[G_FLOOR] = 1.f;
    groupK[G_TILL] = 0.8f + 0.2f * wm;
    groupK[G_SIGN] = 0.8f;
    groupK[G_WIN] = 1.f;
    for (int k = 0; k < LANES; k++) {
        const float zc = -k * LANE_DZ;
        const float beep = std::max(laneBeep_[k], beatFlash_ * dropAmt_);
        sprite(sprites_, vec3(7.4f, Y_CT + 1.2f, zc - 0.7f), 2.2f + 2.f * beep, vec3(1.f, 0.12f, 0.08f), (0.25f + 1.8f * beep) * laneOn_[k]);
        sprite(sprites_, vec3(19.f, 6.8f, zc - 4.f), 3.6f, P.accent, 0.12f * groupK[k]);
    }
    if (P.night > 0.05f)
        for (const Lamp& l : lamps_) sprite(sprites_, l.p, 14.f, l.col, 0.7f * P.night);
    pointP_.clear();
    pointC_.clear();
    {
        // our scanner's red light, the other lanes' as they beep
        const float scanOn = scanT_ < SCAN_LEAD + SCAN_SWEEP + 0.1f ? 1.f : 0.f;
        pointP_.push_back(vec4(vec3(6.5f, scanY_, 2.f), 9.f));
        pointC_.push_back(vec4(vec3(1.f, 0.15f, 0.1f), 0.4f + 2.5f * scanOn));
        pointP_.push_back(vec4(vec3(7.4f, Y_CT + 2.f, -1.f), 7.f));
        pointC_.push_back(vec4(vec3(1.f, 0.15f, 0.1f), 0.5f + 2.f * std::max(laneBeep_[0], beatFlash_ * dropAmt_)));
        for (int k = 1; k < LANES && pointP_.size() < 16; k++) {
            if (laneOn_[k] < 0.1f) continue;
            pointP_.push_back(vec4(vec3(7.4f, Y_CT + 2.f, -k * LANE_DZ - 1.f), 8.f));
            pointC_.push_back(vec4(vec3(1.f, 0.15f, 0.1f), (0.4f + 2.f * std::max(laneBeep_[k], beatFlash_ * dropAmt_)) * laneOn_[k]));
        }
    }

    // ---- The board.
    board_.clear();
    boardLines_.clear();
    boardSprites_.clear();
    buildFrame();
    buildBoard(game);
    // the scanned products: in the row (flashing under the beam), then flying to the bags
    for (const Flight& fl : flights_) {
        if (fl.kind == 1) {
            if (fl.t < fl.delay) { product(cells_, fl.p0, CELL, fl.type, fl.seed); continue; }
            product(cells_, fl.cur, CELL, fl.type, fl.seed, 0.f, (fl.t - fl.delay) * fl.spin);
            continue;
        }
        const float tx = SCAN_LEAD + (4.5f - fl.p0.x) / 9.f * SCAN_SWEEP;
        const float flash = std::exp(-std::pow((fl.t - tx) * 14.f, 2.f)) + 0.35f * smoothstepf(tx, tx + 0.05f, fl.t) * (1.f - smoothstepf(tx + 0.1f, tx + 0.4f, fl.t));
        const float tt = fl.t - fl.delay;
        const float rot = tt > 0.f ? fl.spin * tt * 4.f : 0.f;
        product(cells_, fl.cur, CELL * fl.scale, fl.type, fl.seed, flash, rot);
        if (std::fabs(fl.t - tx) < 0.06f) sprite(boardSprites_, fl.p0 + vec3(0.f, 0.f, 0.6f), 1.1f, vec3(1.f, 0.25f, 0.2f), 1.f);
    }
    // the beam: a red line sweeping the scanned rows, the fan from the scanner's window
    if (scanT_ > SCAN_LEAD && scanT_ < SCAN_LEAD + SCAN_SWEEP + 0.05f) {
        const float wpx = std::max(1.5f, 3.f * height_ / 1080.f);
        const vec3 red(1.f, 0.2f, 0.12f);
        const float y0 = scanTo_ - scanH_, y1 = scanTo_ + scanH_;
        line(boardLines_, vec3(beamX_, y0, 0.6f), vec3(beamX_, y1, 0.6f), wpx, red * 1.6f, 0.95f);
        line(boardLines_, vec3(beamX_, y0, 0.6f), vec3(beamX_, y1, 0.6f), wpx * 5.f, red, 0.25f);
        line(boardLines_, vec3(5.4f, scanY_, 0.6f), vec3(beamX_, y1, 0.6f), wpx * 0.6f, red, 0.35f);
        line(boardLines_, vec3(5.4f, scanY_, 0.6f), vec3(beamX_, y0, 0.6f), wpx * 0.6f, red, 0.35f);
        for (float y = y0 + 0.5f; y < y1; y += 1.f) sprite(boardSprites_, vec3(beamX_, y, 0.7f), 0.9f, red, 0.9f);
    }
    // a landing: a little sparkle at the sink (the flights' end)
    // (the landed products are gone; the bag's fill shows them)

    // ---- Uniforms.
    const vec3 sunDir = normalize(vec3(std::sin(P.sunAz) * std::cos(P.sunEl), std::sin(P.sunEl) + 0.05f, -std::cos(P.sunAz) * std::cos(P.sunEl)));
    // the store's light: dimmer when calm, brighter at a peak, swelling on the drop; closing time in slow motion
    const float inK = (0.86f + 0.08f * wm + 0.12f * wp + 0.1f * dropAmt_ + 0.12f * swell_ + 0.05f * beatFlash_) * (1.f - 0.5f * slow_);
    const vec3 calmTint = lerp(vec3(0.92f, 0.97f, 1.04f), vec3(1.f), wm);
    const float lift = smoothstepf(0.03f, 0.75f, zoom_);
    const float roofY = lift * lift * 1400.f;
    int wings = 0;
    if (gL_ >= 66.f) wings = std::min(3, 1 + (int)((gL_ - 62.f) / 9.f));
    const float storeHX = X_WALL + MODULE_W * wings;
    const float camDist = length(eyeW_ - (eyeW_ + camF_ * 100.f)) * 0.f + length(eyeW_);
    const float fogStart = zoom_ < 0.5f ? 300.f : camDist * 0.9f;
    const float fogD = zoom_ < 0.5f ? 0.0006f : 0.12f / std::max(camDist, 1.f);
    auto common = [&](GLuint p, const vec3& eye, bool fog) {
        set3f(p, "uEye", eye);
        set3f(p, "uZen", P.zen);
        set3f(p, "uHor", P.hor);
        set3f(p, "uGround", P.ground);
        set3f(p, "uSunDir", sunDir);
        set3f(p, "uSunGlow", P.sunGlow);
        set3f(p, "uFogCol", P.fog);
        set1f(p, "uFogD", fog ? fogD : 0.f);
        set1f(p, "uFogStart", fogStart);
        set1f(p, "uTime", worldTime_);
        set1f(p, "uSat", 1.f + 0.06f * wp + 0.08f * dropAmt_);
        set1f(p, "uNight", P.night);
    };
    auto lighting = [&](GLuint p) {
        set3f(p, "uAmbTop", P.inTop * inK * calmTint);
        set3f(p, "uAmbBot", P.inBot * inK);
        set3f(p, "uKeyDir", normalize(vec3(-0.3f, 0.85f, 0.45f)));
        set3f(p, "uKeyCol", P.inKey * inK * calmTint);
        set3f(p, "uOutTop", P.outTop);
        set3f(p, "uOutBot", P.outBot);
        set3f(p, "uOutKey", P.outKey);
        set3f(p, "uOutKeyDir", P.night > 0.5f ? normalize(vec3(0.3f, 0.8f, 0.3f)) : sunDir);
        set3f(p, "uLampCol", P.lamp);
        set1f(p, "uLampK", P.night * 1.2f);
        set1f(p, "uParkHalfW", 760.f + MODULE_W * wings);
        set4f(p, "uStoreBox", storeHX, Z_BACK, Z_FRONT + 1.f, Y_CEIL + 8.f);
        const int n = (int)pointP_.size();
        set1i(p, "uNL", n);
        if (n) {
            glUniform4fv(ugl::U(p, "uLP"), n, &pointP_[0].x);
            glUniform4fv(ugl::U(p, "uLC"), n, &pointC_[0].x);
        }
        glUniform1fv(ugl::U(p, "uGroupK"), 16, groupK);
        glUniform3fv(ugl::U(p, "uFA"), 7, &P.famA[0].x);
        glUniform3fv(ugl::U(p, "uFB"), 7, &P.famB[0].x);
        glUniform3fv(ugl::U(p, "uFC"), 7, &P.famC[0].x);
        glUniform3fv(ugl::U(p, "uPal"), 8, &P.goods[0].x);
        set3f(p, "uKraft", P.kraft);
        set3f(p, "uAccent", P.accent);
        set3f(p, "uShelfEdge", P.shelfEdge);
        set3f(p, "uWinCol", P.win);
        set3f(p, "uFridgeCol", P.fridge);
        set1f(p, "uFloorY", Y_FLOOR);
        set1f(p, "uCeilY", Y_CEIL);
        set1f(p, "uTubeK", groupK[G_TUBES] * (1.f - lift));
        set1f(p, "uTubeZ0", Z_BACK + 18.f);
        set1f(p, "uTubeDZ", 30.f);
        set1f(p, "uStoreX", X_WALL);
        set1f(p, "uStoreZ0", Z_BACK);
        set1f(p, "uStoreZ1", Z_FRONT);
        set1f(p, "uGloss", P.gloss);
        set1f(p, "uBeltK", 1.f);
        set1f(p, "uWinK", 0.25f + 0.9f * P.night);
        set2f(p, "uGrow", gL_, gL_);
        set3f(p, "uOffset", vec3(0.f));
        set1f(p, "uStackA", 0.f);
    };
    auto drawBoxes = [&](GLuint vao, GLuint vbo, const std::vector<Box>* data, size_t count) {
        if (!count) return;
        glBindVertexArray(vao);
        if (data) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(Box)), data->data(), GL_STREAM_DRAW);
        }
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)count);
    };
    auto drawBlobs = [&](GLuint vao, GLuint vbo, const std::vector<Blob>* data, size_t count, const mat4& vp) {
        if (!count) return;
        glUseProgram(progBlob_);
        setMat(progBlob_, "uVP", vp);
        set2f(progBlob_, "uGrow", gL_, gL_);
        glBindVertexArray(vao);
        if (data) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(Blob)), data->data(), GL_STREAM_DRAW);
        }
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
    };
    auto drawSprites = [&](const std::vector<Sprite>& S, const mat4& vp, const vec3& eye, const vec3& r, const vec3& u, bool fog) {
        if (S.empty()) return;
        glUseProgram(progSprite_);
        common(progSprite_, eye, fog);
        setMat(progSprite_, "uVP", vp);
        set3f(progSprite_, "uCamR", r);
        set3f(progSprite_, "uCamU", u);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(S.size() * sizeof(Sprite)), S.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)S.size());
    };
    auto drawLines = [&](const std::vector<Line>& L, const mat4& vp, bool additive) {
        if (L.empty()) return;
        glUseProgram(progLine_);
        setMat(progLine_, "uVP", vp);
        set2f(progLine_, "uRes", (float)width_, (float)height_);
        set1f(progLine_, "uAdd", additive ? 1.f : 0.f);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(L.size() * sizeof(Line)), L.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)L.size());
    };

    // ---- The world (multisampled).
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(progBox_);
    common(progBox_, eyeW_, true);
    lighting(progBox_);
    setMat(progBox_, "uVP", vpW_);
    set1f(progBox_, "uFogOn", 1.f);
    drawBoxes(staticVao_, staticVbo_, nullptr, staticCount_);
    drawBoxes(boxVao_, boxVbo_, &boxes_, boxes_.size());
    if (roofY < 1300.f) {
        set3f(progBox_, "uOffset", vec3(0.f, roofY, 0.f));
        drawBoxes(roofVao_, roofVbo_, nullptr, roofCount_);
        set3f(progBox_, "uOffset", vec3(0.f));
    }
    // the sky where nothing stands
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(1.0, 1.0);
    glUseProgram(progSky_);
    common(progSky_, eyeW_, true);
    setMat(progSky_, "uInvVP", invVpW_);
    set1f(progSky_, "uStars", P.stars);
    set1f(progSky_, "uClouds", P.clouds);
    ugl::drawFullscreen();
    glDepthRange(0.0, 1.0);
    glDepthFunc(GL_LESS);
    // contact shadows on the floor
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.f, -4.f);
    drawBlobs(sblobVao_, sblobVbo_, nullptr, staticBlobCount_, vpW_);
    drawBlobs(blobVao_, blobVbo_, &blobs_, blobs_.size(), vpW_);
    glDisable(GL_POLYGON_OFFSET_FILL);
    // glows
    glBlendFunc(GL_ONE, GL_ONE);
    drawSprites(sprites_, vpW_, eyeW_, camR_, camU_, true);
    // confetti (board space, behind the well's veil)
    glDisable(GL_DEPTH_TEST);
    if (!flakeInst_.empty()) {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(progFlake_);
        setMat(progFlake_, "uVP", vpB_);
        glBindVertexArray(flakeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, flakeVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(flakeInst_.size() * sizeof(Flake)), flakeInst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)flakeInst_.size());
    }
    // the veil behind the well and the HUD's columns
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progVeil_);
    set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progVeil_, "uVeil", P.veil);
    set3f(progVeil_, "uRail", lerp(P.accent, vec3(1.f), 0.5f) * 0.4f);
    set1f(progVeil_, "uWellA", 0.62f + 0.06f * P.night);
    set1f(progVeil_, "uColA", 0.42f + 0.06f * smoothstepf(0.5f, 2.f, zoom_));
    set1f(progVeil_, "uRailA", 0.f);
    set1f(progVeil_, "uPx", pxPerUnit_);
    set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.6f, layout::LEFT_COLUMN[3]);
    set4f(progVeil_, "uColR", 5.6f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    ugl::drawFullscreen();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // ---- The board (its own camera, over the world).
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(progBox_);
    common(progBox_, eyeB_, false);
    lighting(progBox_);
    setMat(progBox_, "uVP", vpB_);
    set1f(progBox_, "uFogOn", 0.f);
    set1f(progBox_, "uTubeK", 0.f);
    drawBoxes(boxVao_, boxVbo_, &board_, board_.size());
    set1f(progBox_, "uStackA", 1.f);
    drawBoxes(boxVao_, boxVbo_, &cells_, cells_.size());
    set1f(progBox_, "uStackA", 0.f);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glBlendFunc(GL_ONE, GL_ONE);
    drawSprites(boardSprites_, vpB_, eyeB_, vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), false);
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawLines(boardLines_, vpB_, false);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // ---- Resolve, the glow, the frame.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uThr", 0.55f);
    ugl::drawFullscreen();
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    for (int i = 0; i < 2; i++) {
        const float st = 1.f + i;
        bloomB_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        set2f(progBlur_, "uDir", st / bloomA_.w, 0.f);
        ugl::drawFullscreen();
        bloomA_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        set2f(progBlur_, "uDir", 0.f, st / bloomA_.h);
        ugl::drawFullscreen();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set1i(progFinal_, "uBloom", 1);
    glActiveTexture(GL_TEXTURE0);
    set1f(progFinal_, "uBloomK", (0.5f + 0.1f * wp + 0.25f * dropAmt_ + 0.2f * swell_) * (1.f + 0.3f * slow_));
    set1f(progFinal_, "uExposure", 1.f);
    set1f(progFinal_, "uHalo", 0.06f);
    set1f(progFinal_, "uSatF", 1.f + 0.04f * wp + 0.06f * dropAmt_);
    set1f(progFinal_, "uSlow", slow_ * 0.5f);
    set3f(progFinal_, "uSlowTint", vec3(0.78f, 0.88f, 1.08f));
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
    (void)camDist;
    if (gpuQ) {
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
        static double acc = 0.0; static int n = 0;
        acc += ns * 1e-6; n++;
        if (n == 240) { std::fprintf(stderr, "[groceries] gpu %.2f ms/frame (lines %d, zoom %.2f)\n", acc / n, lines_, zoom_); acc = 0.0; n = 0; }
    }
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_groceries() {
    static const UniverseEntry e{
        "GROCERIES", "a supermarket checkout in 3D: the pieces are groceries, cleared rows are scanned and bagged, the store grows into an absurd hypermarket",
        [] { return std::unique_ptr<Universe>(new GroceriesUniverse()); },
        // A busy, cheerful world: suits rhythmic, driving songs a little better; fits most.
        [](const SongLook& l) { return 0.8f + 0.2f * l.density + 0.1f * l.bassWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
