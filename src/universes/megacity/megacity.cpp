// MEGACITY: a single hut on a riverbank grows into a city covering the whole planet, with the cleared lines. At the
// first line it is a hut at dusk, its window glowing with the fire, smoke rising from its roof, trees around it and
// the river behind it mirroring the afterglow. The city sprawls organically, along the river, the highways and the
// coast of a bay, in lobes and fingers around parks, satellite towns growing on their own and merging: a village with
// its church, a walled medieval town, a stone city of perimeter blocks; then a long skyscraper stage (downtown
// clusters of towers taller and more numerous line after line, then megatalls), arcologies, the space elevator's
// tether rising into orbit, the continents, the oceans, until the planet is one glowing ball of lights under an
// orbital ring being built around it.
//
// The lines drive a frontier F (meters, a table interpolated in log space: the first towers at ~28 lines, megatalls
// from ~70, arcologies from ~88, the continent from ~110, the planet covered at 152, then the ring); each place rises
// when F passes its organic distance D (mcity::field, the same on the CPU and in the ground's shader). The plan is
// generated once per game (city.cpp): lots sized by D on district-turned grids, each built in its era's style, the
// downtowns' towers planted on their own; the buildings' shader raises each out of the ground as the frontier passes
// it (and sinks the old town where the towers rise), the ground's shader draws the streets, the highways' traffic and
// the lots too small to draw as buildings from the same lots. Dusk turns to night as the town grows.
//
// The camera pulls back as the city grows: low by the hut at the right of the well, then over the village and the
// town, then low over the skyline, slowly circling the main downtown and framing its tallest towers, then up over
// the continent, then the planet as a ball. **The drop** (a real one only) raises a whole new district (a wave of
// towers in the skyscraper stage) out of the ground (the frontier jumps out by half), and its lights come on patch by
// patch on the beat; lanterns rise over a town, searchlights sweep the sky over a modern city. A clear's cells
// dissolve into light that flows out of the well to the city; a Tetris adds fireworks. Slow motion slows the traffic
// and cools the light; game over turns the lights off from the edge inward, and the next game starts again from a
// hut (the camera dives back down).
//
// Layout contract: the board is drawn flat (orthographic at the reference camera's scale); only the world behind it
// has its own 3D camera.
#include "universes/megacity/megacity.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/megacity/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Jura (Light for the song and the notices, Medium for the labels and numbers), SIL Open Font License, see
// OFL-Jura.txt.
const unsigned char FONT_LIGHT[] = {
#include "universes/megacity/Jura-Light.ttf.inc"
};
const unsigned char FONT_MEDIUM[] = {
#include "universes/megacity/Jura-Medium.ttf.inc"
};

namespace shaders = megacity_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name, bool world = false) {
    const std::string pre = std::string(shaders::COMMON_GLSL) + (world ? shaders::LIGHT_GLSL : "");
    std::string f(fs);
    f.insert(f.find('\n') + 1, pre);
    std::string v(vs);
    v.insert(v.find('\n') + 1, pre);
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 lerp2(vec2 a, vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): the sky at dusk (zenith, horizon) and at night, space and its nebula, the sun and its glow, the
// moon; the light (lit, shade), the haze at dusk and at night, the city's glow on it; the land (grass light and
// dark, fields, far land, deserts, ocean, the river, woods); the buildings (walls, roofs, stone, glass, arcologies);
// the lights (warm windows, cool ones, the accent, street lamps, tail lights, beacons); the HUD's ink, accent and
// veil; the seven pieces; the sun's azimuth (from the camera's first view, rightward), snow, an aurora, neon.
struct VariantDef {
    const char* name;
    unsigned duskZenith, duskHorizon, nightZenith, nightHorizon, space, nebula, sun, sunGlow, moon;
    unsigned lit, shadow, haze, nightHaze, cityGlow;
    unsigned grass, grass2, field, land, desert, ocean, river, forest;
    unsigned wallA, wallB, roofA, roofB, stone, glass, arco;
    unsigned winWarm, winCool, winAccent, street, traffic, beacon;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float sunAz, snow, aurora, neon, nightBias;
};
const VariantDef VARIANTS[] = {
    // Dusk to a deep blue night: an orange afterglow, amber lamps in the old town, white towers, teal arcologies.
    {"TWILIGHT", 0x2c3c94, 0xff9858, 0x070c28, 0x1a2a60, 0x02030c, 0x4a3cff, 0xffe2b0, 0xff7038, 0xe8ecff,
     0xffc898, 0x24306a, 0xc896a8, 0x161c44, 0xff9448,
     0x62a844, 0x337838, 0xdcb85a, 0x4a8a3c, 0xd8b878, 0x1a5aa0, 0x16346e, 0x1e5232,
     0xf2dcc0, 0xc89e78, 0xcc5038, 0x6a3a2c, 0xbaa890, 0x1c2c50, 0xdadfea,
     0xffae4c, 0xd2e8ff, 0x3ee6d6, 0xffa040, 0xff3434, 0xff2a2a,
     0xffffff, 0xffc070, 0x0a1030,
     {0x5ee0ff, 0xffd84a, 0xc28cff, 0x6ef08a, 0xff6e8a, 0x5c8cff, 0xffa04a}, 0.22f, 0.f, 0.f, 0.f, 0.f},
    // A magenta dusk into a violet night: pink and cyan lights, a neon skyline mirrored in a dark river.
    {"NEON", 0x3c1a80, 0xff5aa8, 0x0a0422, 0x2c0c50, 0x04020c, 0xff3cc8, 0xffd0e0, 0xff4a9a, 0xf0e0ff,
     0xffb0d4, 0x2a1a5c, 0xb070c4, 0x1e0a36, 0xff4ab4,
     0x3a8a6c, 0x1c4a4c, 0x9a8a5c, 0x2a6a5a, 0xb08a8a, 0x1a3a8a, 0x1c0a44, 0x123838,
     0xe2d2ea, 0x9a7aa4, 0x8c3a6c, 0x3a2448, 0x8c829c, 0x1c1638, 0xdcd2f2,
     0xffa070, 0x60e8ff, 0xff40c4, 0xff70c4, 0x40f0ff, 0xff3070,
     0xfff0ff, 0xff78d8, 0x100828,
     {0x40f0ff, 0xfff060, 0xd070ff, 0x60ffa0, 0xff5aa0, 0x6a7aff, 0xffa050}, -0.3f, 0.f, 0.f, 1.f, 0.15f},
    // A snowy north: a peach dusk over the snow, then a night of aurora, red wooden houses, warm gold windows.
    {"AURORA", 0x12306e, 0xf0a088, 0x030c1e, 0x0c2a3c, 0x01040a, 0x20c0a0, 0xfff0d0, 0xff9868, 0xe0f0ff,
     0xffd8b8, 0x30487c, 0xb8c0da, 0x0c1c32, 0xffc070,
     0xc4d4ec, 0x8ea4c8, 0xb4c4dc, 0xb0c4d8, 0xc8d0dc, 0x10406a, 0x0c2a4a, 0x1c3a40,
     0xece0d0, 0xa8462e, 0x4a5a6c, 0x2a3038, 0xa2aab2, 0x14283c, 0xe2eef8,
     0xffc060, 0xc0f0ff, 0x50ffb0, 0xffb860, 0xff4040, 0xff3030,
     0xf0fbff, 0x80ffd0, 0x041420,
     {0x60e8ff, 0xffe070, 0xb090ff, 0x70ffb0, 0xff7090, 0x6a90ff, 0xffa860}, 0.55f, 1.f, 1.f, 0.f, 0.38f},
    // A river in the desert: green irrigated fields in the sand, adobe and gold, a burnt orange dusk, golden lights.
    {"DUNE", 0x2c3074, 0xff8a40, 0x0a0a24, 0x2c1c3c, 0x04030a, 0xff8a3c, 0xffe0a0, 0xff6820, 0xfff0d8,
     0xffb070, 0x3a2a5c, 0xd8a080, 0x241a32, 0xffa040,
     0xccaa84, 0xae8c6c, 0x6aa03a, 0xd0a060, 0xe8c080, 0x1a6a9c, 0x103c5c, 0x3a6a2a,
     0xf2d8a8, 0xd2a070, 0xc89060, 0x8a5a3a, 0xd8b880, 0x2a2432, 0xf2dcb8,
     0xffb040, 0xffeac4, 0xffd040, 0xffa030, 0xff3020, 0xff2a2a,
     0xfff6e8, 0xffc060, 0x1a0c18,
     {0x50d8ff, 0xffd040, 0xd080ff, 0x80e070, 0xff6070, 0x6a80ff, 0xff9a40}, -0.12f, 0.f, 0.f, 0.f, 0.12f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = MegaCityUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.duskZenith = L(d.duskZenith); p.duskHorizon = L(d.duskHorizon); p.nightZenith = L(d.nightZenith);
    p.nightHorizon = L(d.nightHorizon); p.space = L(d.space); p.nebula = L(d.nebula); p.sun = L(d.sun);
    p.sunGlow = L(d.sunGlow); p.moonC = L(d.moon);
    p.lit = L(d.lit); p.shadow = L(d.shadow); p.haze = L(d.haze); p.nightHaze = L(d.nightHaze); p.cityGlow = L(d.cityGlow);
    p.grass = L(d.grass); p.grass2 = L(d.grass2); p.field = L(d.field); p.land = L(d.land); p.desert = L(d.desert);
    p.ocean = L(d.ocean); p.river = L(d.river); p.forest = L(d.forest);
    p.wallA = L(d.wallA); p.wallB = L(d.wallB); p.roofA = L(d.roofA); p.roofB = L(d.roofB); p.stone = L(d.stone);
    p.glass = L(d.glass); p.arco = L(d.arco);
    p.winWarm = L(d.winWarm); p.winCool = L(d.winCool); p.winAccent = L(d.winAccent); p.street = L(d.street);
    p.traffic = L(d.traffic); p.beacon = L(d.beacon);
    p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.sunAz = d.sunAz; p.snow = d.snow; p.aurora = d.aurora; p.neon = d.neon; p.nightBias = d.nightBias;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

constexpr float CLEAR_T = 0.38f;     // a cleared row dissolves into light in this long (real time)
constexpr float FLOW_T = 1.25f;      // its light flows to the city in this long
constexpr int MAX_MOTES = 1600;
constexpr int MAX_SPARKS = 1400;
constexpr float CAM_AZ = -1.5708f;   // the camera looks east (+x), over the hut at the river
constexpr float RING_ALT = 2.6e6f;   // the orbital ring's height

// The journey: lines -> the city's radius (meters), interpolated in log space.
struct Stage { float lines, radius; };
// The skyscraper stage is long: the first towers at ~28 lines, megatalls from ~70, arcologies from ~88, the
// continent from ~110, the planet covered at 152.
const Stage STAGES[] = {
    {0, 4.f}, {2, 14.f}, {4, 35.f}, {7, 90.f}, {10, 220.f}, {14, 550.f}, {18, 1300.f}, {23, 2600.f}, {28, 4500.f},
    {34, 6500.f}, {40, 9000.f}, {46, 1.3e4f}, {52, 1.8e4f}, {58, 2.5e4f}, {64, 3.4e4f}, {70, 4.6e4f}, {76, 6.5e4f},
    {82, 9.5e4f}, {88, 1.4e5f}, {94, 2.2e5f}, {100, 3.8e5f}, {108, 7.e5f}, {116, 1.3e6f}, {124, 2.5e6f}, {132, 5.e6f},
    {142, 1.e7f}, {152, 2.4e7f},
};
constexpr int STAGE_COUNT = (int)(sizeof(STAGES) / sizeof(STAGES[0]));
const float G_MAX = std::log(2.4e7f);

// Test hooks: ZEN_MEGACITY_LINES=N adds N lines to the city's growth (to look at a stage without playing to it); ZEN_GROW_RATE=R adds R more lines a second (a time-lapse).
int extraLines(double t) {
    static const int n = std::getenv("ZEN_MEGACITY_LINES") ? std::atoi(std::getenv("ZEN_MEGACITY_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool MegaCityUniverse::init(int width, int height) {
    progSky_ = compile(ugl::FULLSCREEN_VS, shaders::SKY_FS, "megacity sky", true);
    progBld_ = compile(shaders::BLD_VS, shaders::BLD_FS, "megacity buildings", true);
    progGlow_ = compile(shaders::GLOW_VS, shaders::GLOW_FS, "megacity glow");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "megacity comp");
    progBright_ = compile(ugl::FULLSCREEN_VS, shaders::BRIGHT_FS, "megacity bright");
    progBlur_ = compile(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "megacity blur");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "megacity sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "megacity veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "megacity cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "megacity shape");
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "megacity text");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "megacity post");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, int attribs, int stride) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < attribs; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(glowVao_, glowVbo_, 4, sizeof(float) * 16);
    instanced(spriteVao_, spriteVbo_, 3, sizeof(SpriteInst));
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));

    // the building's mesh: four walls (y 0..1) and a gable roof (its ridge along x at y 2), 6 floats a vertex
    {
        std::vector<float> m;
        auto vtx = [&](float x, float y, float z, float nx, float ny, float nz) {
            const float v[6] = {x, y, z, nx, ny, nz};
            m.insert(m.end(), v, v + 6);
        };
        auto quadF = [&](const float* a, const float* b, const float* c, const float* d, float nx, float ny, float nz) {
            vtx(a[0], a[1], a[2], nx, ny, nz); vtx(b[0], b[1], b[2], nx, ny, nz); vtx(c[0], c[1], c[2], nx, ny, nz);
            vtx(a[0], a[1], a[2], nx, ny, nz); vtx(c[0], c[1], c[2], nx, ny, nz); vtx(d[0], d[1], d[2], nx, ny, nz);
        };
        const float p000[3] = {-1, 0, -1}, p100[3] = {1, 0, -1}, p001[3] = {-1, 0, 1}, p101[3] = {1, 0, 1};
        const float p010[3] = {-1, 1, -1}, p110[3] = {1, 1, -1}, p011[3] = {-1, 1, 1}, p111[3] = {1, 1, 1};
        const float rA[3] = {-1, 2, 0}, rB[3] = {1, 2, 0};
        quadF(p100, p101, p111, p110, 1, 0, 0);
        quadF(p001, p000, p010, p011, -1, 0, 0);
        quadF(p101, p001, p011, p111, 0, 0, 1);
        quadF(p000, p100, p110, p010, 0, 0, -1);
        quadF(p011, p111, rB, rA, 0, 1, 0.5f);
        quadF(p110, p010, rA, rB, 0, 1, -0.5f);
        vtx(p110[0], p110[1], p110[2], 1, 0, 0); vtx(p111[0], p111[1], p111[2], 1, 0, 0); vtx(rB[0], rB[1], rB[2], 1, 0, 0);
        vtx(p011[0], p011[1], p011[2], -1, 0, 0); vtx(p010[0], p010[1], p010[2], -1, 0, 0); vtx(rA[0], rA[1], rA[2], -1, 0, 0);
        meshCount_ = (GLsizei)(m.size() / 6);
        glGenVertexArrays(1, &bldVao_);
        glGenBuffers(1, &meshVbo_);
        glGenBuffers(1, &bldVbo_);
        glBindVertexArray(bldVao_);
        glBindBuffer(GL_ARRAY_BUFFER, meshVbo_);
        glBufferData(GL_ARRAY_BUFFER, m.size() * sizeof(float), m.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
        glBindBuffer(GL_ARRAY_BUFFER, bldVbo_);
        for (int i = 0; i < 5; i++) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 20, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(2 + i, 1);
        }
    }
    // shapes and text (pixels)
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

    buildFont(light_, FONT_LIGHT);
    buildFont(medium_, FONT_MEDIUM);
    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void MegaCityUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] megacity font init failed\n");
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
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, ch, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, ch, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
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

void MegaCityUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    aspect_ = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the world at a reduced resolution
    const int wh = std::max(180, std::min(720, (int)(height_ * 0.7f)));
    const int ww = std::max(320, (int)std::lround((float)wh * width_ / height_));
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
    bloomA_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

MegaCityUniverse::~MegaCityUniverse() {
    for (ugl::Target* t : {&worldT_, &bloomA_, &bloomB_, &sceneT_}) t->destroy();
    for (GLuint p : {progSky_, progBld_, progGlow_, progComp_, progBright_, progBlur_, progSprite_, progVeil_, progCell_,
                     progShape_, progText_, progPost_})
        glDeleteProgram(p);
    for (GLuint v : {bldVao_, glowVao_, spriteVao_, cellVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, meshVbo_, bldVbo_, glowVbo_, spriteVbo_, cellVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&light_, &medium_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The city
// =====================================================================================================

// Lines -> G = ln(the city's radius), up to the planet covered.
float MegaCityUniverse::growthOf(int lines) const {
    const float l = (float)std::max(0, lines);
    if (l >= STAGES[STAGE_COUNT - 1].lines) return G_MAX;
    int i = 0;
    while (i + 2 < STAGE_COUNT && STAGES[i + 1].lines <= l) i++;
    const float t = (l - STAGES[i].lines) / (STAGES[i + 1].lines - STAGES[i].lines);
    return lerpf(std::log(STAGES[i].radius), std::log(STAGES[i + 1].radius), t);
}

void MegaCityUniverse::uploadCity() {
    plan_ = buildMegaCity(citySeed_);
    glBindBuffer(GL_ARRAY_BUFFER, bldVbo_);
    glBufferData(GL_ARRAY_BUFFER, plan_.inst.size() * sizeof(float), plan_.inst.data(), GL_STATIC_DRAW);
    bldCount_ = (GLsizei)(plan_.inst.size() / 20);
    cityDirty_ = false;
}

// =====================================================================================================
// Camera
// =====================================================================================================

// Pulls back with the city, a moment behind its growth: low by the hut at the right of the well, then over the
// village, then higher and farther, the skyline against the sky, then the planet's curve, then the planet as a ball.
void MegaCityUniverse::updateCamera(float dt) {
    const float gGoal = std::min(g_, G_MAX) + 0.9f * orbit_;
    if (gc_ < -50.f) { gc_ = gGoal; gcVel_ = 0.f; }
    {
        const float w = 1.7f;
        gcVel_ += (w * w * (gGoal - gc_) + 2.f * w * (0.3f * gVel_ - gcVel_)) * dt;
        gc_ += gcVel_ * dt;
    }
    const float G = gc_;
    const float cityR = std::exp(std::min(G, G_MAX));
    const float R = mcity::R_EARTH;
    const float kV = smoothstepf(std::log(6.f), std::log(200.f), G);           // the hut -> the village
    const float kT = smoothstepf(std::log(1300.f), std::log(3800.f), G);       // the stone city: lower, toward downtown
    const float kD = smoothstepf(std::log(3800.f), std::log(7000.f), G);       // the skyline
    const float kL = smoothstepf(std::log(1.4e5f), std::log(6.e5f), G);        // arcologies -> the continent from above
    const float kP = smoothstepf(std::log(2.e6f), std::log(1.6e7f), G);        // the planet as a ball
    const float kO = smoothstepf(G_MAX, G_MAX + 0.9f, G);                       // the ring's view
    tanH_ = std::tan(26.f * PI / 180.f);
    const vec3 dc(plan_.downtown[0], 0.f, plan_.downtown[1]);
    // the town: over the hut, then lower and toward the downtown-to-be as it becomes a stone city
    const float Fcity = std::max(lerpf(6.f, 4.2f, kV), lerpf(0.62f, 0.4f, kT) * cityR);
    const float pCity = -6.f - 14.f * kV + 9.f * kT;
    const vec3 aCity = lerp(vec3(0.f, lerpf(1.3f, 0.f, kV), 0.f), dc * 0.6f, kT);
    const float sxCity = lerpf(0.68f, 0.f, smoothstepf(std::log(12.f), std::log(400.f), G)) + 0.2f * kT;
    const float syCity = lerpf(-0.08f, -0.42f, smoothstepf(std::log(30.f), std::log(3000.f), G)) + 0.12f * kT;
    // the skyline: low, a few degrees under the horizon, framing the tallest towers, circling the downtown slowly
    const float Htop = mcity::towerH(std::min(cityR, 7.e4f)) * 1.35f;
    const float tD = saturate(std::log(std::max(cityR, mcity::SKY_F0) / mcity::SKY_F0) / std::log(14.f));
    const float rDown = 350.f + 2250.f * std::pow(tD, 0.7f);   // the downtown's extent (see city.cpp)
    const float Fsky = std::max(1.6f * Htop + 60.f, tanH_ * 1.25f * rDown);
    const vec3 aSky = dc + vec3(0.f, 0.3f * Htop, 0.f);
    const float pSky = -3.5f;
    // later: the continent from above, the city's whole extent
    const float Flate = 0.62f * cityR;
    const float pLate = -20.f - 10.f * smoothstepf(std::log(1.e6f), std::log(4.e6f), G);
    const vec3 aLate(0.f, 0.f, 0.f);
    const float wSky = kD * (1.f - kL), wLate = kD * kL, wCity = 1.f - kD;
    const float Fc = std::exp(std::log(Fcity) * wCity + std::log(Fsky) * wSky + std::log(Flate) * wLate);
    const vec3 anchorC = aCity * wCity + aSky * wSky + aLate * wLate;
    const float pitchDeg = pCity * wCity + pSky * wSky + pLate * wLate;
    const float Fp = R * lerpf(1.0f, 1.25f, kO);
    const float F = std::exp(lerpf(std::log(Fc), std::log(Fp), kP));
    const float pitch = lerpf(pitchDeg, -32.f + 8.f * kO, kP) * PI / 180.f;
    const vec3 anchor = lerp(anchorC, vec3(0.f, -R, 0.f), kP);
    const float sx = (sxCity * wCity + 0.4f * wSky) * (1.f - kP) + 0.52f * kP;
    const float sy = (syCity * wCity - 0.08f * wSky - 0.3f * wLate) * (1.f - kP) + lerpf(-0.1f, -0.2f, kO) * kP;
    const float az = CAM_AZ + 0.05f * std::sin(time_ * 0.021f) + 0.32f * wSky * std::sin(time_ * 0.011f) - 0.004f * time_ * kP;
    const float yaw = std::atan(sx * tanH_ * aspect_ / std::cos(pitch));
    const float phi = az + yaw;
    vec3 fwd(-std::sin(phi) * std::cos(pitch), std::sin(pitch), -std::cos(phi) * std::cos(pitch));
    vec3 right = normalize(cross(fwd, vec3(0, 1, 0)));
    vec3 upv = cross(right, fwd);
    const vec3 dirA = normalize(fwd + right * (sx * tanH_ * aspect_) + upv * (sy * tanH_));
    const float D = F / tanH_;
    vec3 eye = anchor - dirA * D;
    eye.y = std::max(eye.y, 0.9f + 0.05f * Htop * wSky);
    eye_ = eye;
    look_ = fwd;
    camR_ = right;
    camU_ = upv;
    camD_ = D;
    {
        const vec3 oc(eye.x, eye.y + R, eye.z);
        alt_ = std::max(0.f, length(oc) - R);
        if (length(oc) < 1e7f) alt_ = std::max(0.f, eye.y + (eye.x * eye.x + eye.z * eye.z) / (2.f * R));
    }
    const float zn = std::max(0.05f, D * 0.004f), zf = std::max({2.e4f, D * 60.f});
    const mat4 view3 = mat4::lookAt(vec3(0.f), fwd, vec3(0, 1, 0));
    const mat4 proj3 = mat4::perspective(2.f * std::atan(tanH_), aspect_, zn, zf);
    vp3_ = proj3 * view3;
}

vec2 MegaCityUniverse::toBoard(const vec3& w) const {
    const vec4 c = vp3_ * vec4(w - eye_, 1.f);
    if (c.w <= 1e-4f) return V(viewC_.x + viewH_.x, viewC_.y);
    return V(viewC_.x + c.x / c.w * viewH_.x, viewC_.y + c.y / c.w * viewH_.y);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void MegaCityUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.bpm > 118.f && t.bassWeight > 0.55f) v = 1;             // the neon night for driving songs
        else if (t.hasSong && t.minor && t.brightness < 0.45f) v = 2;              // the aurora for dark, minor ones
        else if (t.hasSong && !t.minor && t.brightness > 0.6f) v = 3;              // the desert's gold for bright ones
        else {
            static const int BY_HUE[] = {0, 3, 0, 2, 1, 0};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x3E6Aull);
    if (citySeed_ == 0) { citySeed_ = seed_; cityDirty_ = true; }
    if (seconds <= 0.f) palSet_ = false;
}

void MegaCityUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropMode_ = false;
    (void)impact;
    const bool instant = !hasLevel_ || seconds < 0.1f;
    if (instant) {
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

// The drop: a new district rises (only on a real drop: see render).
void MegaCityUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void MegaCityUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 MegaCityUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints MegaCityUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.nightZenith, p.nightHorizon, 0.5f) * 1.2f;
    h.light = false;
    h.ink = p.ink;
    h.accent = p.winWarm;
    h.styles = TS_INK | TS_LUMA | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void MegaCityUniverse::syncIds(const Game& g) {
    bool ok = true;
    for (int y = 0; y < Game::H && ok; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) { ok = false; break; }
    if (ok) return;
    std::memset(ids_, 0, sizeof(ids_));
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0 || ids_[y][x]) continue;
            const int id = nextId_++;
            int stack[8][2], sp = 0, count = 0;
            stack[sp][0] = x; stack[sp][1] = y; sp++;
            ids_[y][x] = id;
            while (sp > 0 && count < 4) {
                sp--;
                const int cx = stack[sp][0], cy = stack[sp][1];
                count++;
                const int dd[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& e : dd) {
                    const int nx = cx + e[0], ny = cy + e[1];
                    if (nx < 0 || ny < 0 || nx >= Game::W || ny >= Game::H || ids_[ny][nx]) continue;
                    if (g.cell(nx, ny).type != g.cell(x, y).type || sp >= 8 || count + sp >= 4) continue;
                    ids_[ny][nx] = id;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

void MegaCityUniverse::addMote(const Mote& m) {
    if ((int)motes_.size() >= MAX_MOTES) return;
    motes_.push_back(m);
}

void MegaCityUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::HardDrop: {
        float yLo = 99.f;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f) continue;
            for (int i = 0; i < 2; i++) {
                Mote m;
                m.p = V(p.x + rng_.range(-0.4f, 0.4f), p.y - 0.5f);
                m.v = V(rng_.range(-0.6f, 0.6f), rng_.range(0.1f, 0.5f));
                m.life = rng_.range(0.6f, 1.1f);
                m.size = rng_.range(0.07f, 0.12f);
                m.col = pal_.piece[std::clamp(c.type, 0, 6)];
                m.kind = 3;
                m.seed = rng_.uniform();
                addMote(m);
            }
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        const int lines = std::max(1, ev.count);
        // the cells dissolve into light that flows out of the well to the city's edge (on the screen's right side)
        const float cityR = std::exp(std::min(g_, G_MAX));
        vec2 to;
        {
            const float r = std::min(cityR * 0.9f, 6.e6f), ph = 0.5f;
            const float th = r / mcity::R_EARTH, s2 = std::sin(th * 0.5f);
            vec3 p(mcity::R_EARTH * std::sin(th) * std::cos(ph), -2.f * mcity::R_EARTH * s2 * s2, mcity::R_EARTH * std::sin(th) * std::sin(ph));
            if (cityR > 3800.f && cityR < 2.e5f) p = vec3(plan_.downtown[0], mcity::towerH(cityR) * 0.6f, plan_.downtown[1]);
            to = toBoard(p);
        }
        to.x = std::clamp(to.x, 6.5f, viewH_.x - 1.5f);
        to.y = std::clamp(to.y, viewC_.y - viewH_.y + 1.5f, viewC_.y + viewH_.y * 0.3f);
        for (auto& cl : ev.cells) {
            if (cl.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)cl.x, (float)cl.y);
            const int id = ids_[cl.y][cl.x];
            int m = 0;
            if (id) {
                const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
                for (int i = 0; i < 8; i++) {
                    const int nx = cl.x + dx[i], ny = cl.y + dy[i];
                    if (nx < 0 || nx >= Game::W || ny < 0 || ny >= Game::H || !rows[ny]) continue;
                    if (ids_[ny][nx] == id) m |= 1 << i;
                }
            }
            fading_.push_back({p.x, p.y, std::clamp(cl.type, 0, 6), m, 0.f});
            for (int i = 0; i < 2; i++) {
                Mote mt;
                mt.kind = 1;
                mt.from = V(p.x + rng_.range(-0.3f, 0.3f), p.y + rng_.range(-0.3f, 0.3f));
                mt.to = add(to, V(rng_.range(-1.8f, 1.8f), rng_.range(-0.8f, 0.8f)));
                mt.ctl = V(rng_.range(6.f, 8.f), std::max(mt.from.y, mt.to.y) + rng_.range(1.f, 4.f));
                mt.p = mt.from;
                mt.life = FLOW_T * rng_.range(0.8f, 1.1f);
                mt.age = -rng_.range(0.f, 0.25f) - (float)(Game::W - 1 - cl.x) * 0.015f;
                mt.size = rng_.range(0.1f, 0.16f);
                mt.col = lerp(pal_.piece[std::clamp(cl.type, 0, 6)], pal_.winWarm, 0.5f);
                mt.bright = 1.1f;
                mt.seed = rng_.uniform();
                addMote(mt);
            }
        }
        {
            int nb[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
        }
        spurt_ = std::max(spurt_, 0.4f + 0.15f * lines);
        if (lines >= 4) {
            tetrisGlow_ = 1.f;
            // fireworks over the city's edge
            const float R = std::min(cityR, 4.e6f);
            for (int b = 0; b < 5; b++) {
                const float ph = 0.2f + rng_.range(-0.5f, 0.5f), r = R * rng_.range(0.5f, 0.95f);
                const float th = r / mcity::R_EARTH, s2 = std::sin(th * 0.5f);
                const vec3 base(mcity::R_EARTH * std::sin(th) * std::cos(ph), -2.f * mcity::R_EARTH * s2 * s2, mcity::R_EARTH * std::sin(th) * std::sin(ph));
                const vec3 up = normalize(base + vec3(0.f, mcity::R_EARTH, 0.f));
                const vec3 c = base + up * (R * rng_.range(0.15f, 0.3f) + 8.f);
                const float hue = rng_.uniform() * 6.f;
                const vec3 col = lerp(vec3(saturate(std::fabs(hue - 3.f) - 1.f), saturate(2.f - std::fabs(hue - 2.f)), saturate(2.f - std::fabs(hue - 4.f))), vec3(1.f), 0.35f) * 1.5f;
                const float delay = b * 0.22f;
                for (int i = 0; i < 40 && (int)sparks_.size() < MAX_SPARKS; i++) {
                    Spark s;
                    const vec3 dir = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f)));
                    s.p = c;
                    s.v = dir * (R * rng_.range(0.05f, 0.08f) + 3.f);
                    s.age = -delay;
                    s.life = rng_.range(1.4f, 2.0f);
                    s.size = R * 0.006f + 0.25f;
                    s.seed = rng_.uniform();
                    s.kind = 2;
                    s.col = col;
                    sparks_.push_back(s);
                }
            }
        }
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
        overCells_.clear();
        bool full[Game::H][Game::W] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) full[c.y][c.x] = true;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.x < 0 || c.x >= Game::W || c.y >= Game::H) continue;
            const int id = ids_[c.y][c.x];
            int m = 0;
            const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
            for (int i = 0; i < 8; i++) {
                const int nx = c.x + dx[i], ny = c.y + dy[i];
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H || !full[ny][nx]) continue;
                if (id && ids_[ny][nx] == id) m |= 1 << i;
            }
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            overCells_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, (float)c.y});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void MegaCityUniverse::cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, 0.f, 0.f}});
}

void MegaCityUniverse::buildBoard(const Game& g) {
    auto tintOf = [&](int t) { return pal_.piece[std::clamp(t, 0, 6)]; };
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            auto same = [&](int nx, int ny) {
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H) return false;
                if (std::fabs(g.rowOffset(ny) - off) > 0.02f) return false;
                if (ids_[y][x] && ids_[ny][nx]) return ids_[ny][nx] == ids_[y][x];
                return g.cell(nx, ny).type == c.type;
            };
            const vec3 p = layout::cellPos((float)x, y - off);
            cell(p.x, p.y, 0.5f, maskOf(x, y, same), tintOf(c.type), 1.f, 0, 0.f);
        }
    // game over: the stack's light goes out from the top down
    if (overT_ >= 0.f)
        for (const Fading& o : overCells_) {
            const float rowFrac = (o.t - Game::HIDDEN) / (float)(Game::H - Game::HIDDEN);
            const float k = smoothstepf(rowFrac * 0.6f, rowFrac * 0.6f + 0.3f, over_);
            cell(o.x, o.y, 0.5f, o.mask, tintOf(o.type), 1.f - 0.5f * smoothstepf(0.7f, 1.f, over_), 0, 0.f);
            cells_.back().c[3] = k;
        }
    for (const Fading& f : fading_) cell(f.x, f.y, 0.5f, f.mask, tintOf(f.type), 1.f, 0, saturate(f.t / CLEAR_T));
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto inGhost = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), tintOf(p.type), 0.5f, 1, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < 0) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), tintOf(p.type), 1.f, 0, 0.f);
        }
    }
    auto preview = [&](int type, vec2 center, float sc, float alpha) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        g.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        for (auto& c : cells) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            cell(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), tintOf(type), alpha, 0, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void MegaCityUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param, float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

// =====================================================================================================
// Text
// =====================================================================================================

float MegaCityUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            w += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
    return w - track;
}

void MegaCityUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                                 std::vector<float>& out) const {
    if (!f.ok) return;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    const float baseline = f.capH * k;
    float pen = 0.f;
    auto place = [&](float u, float v, float tu, float tv) {
        out.push_back(x + u); out.push_back(y + v); out.push_back(tu); out.push_back(tv);
    };
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            place(x0, y0, g.x0, g.y0); place(x1, y0, g.x1, g.y0); place(x1, y1, g.x1, g.y1);
            place(x0, y0, g.x0, g.y0); place(x1, y1, g.x1, g.y1); place(x0, y1, g.x0, g.y1);
        }
        pen += g.adv * k + track;
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            pen += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
}

void MegaCityUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float halo) {
    if (q.empty() || !f.ok || alpha <= 0.004f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    set3f(progText_, "uInk", lerp(ink, bridge_.ink, b));
    set3f(progText_, "uHalo", lerp(pal_.veil * 0.6f, bridge_.color, b));
    set1f(progText_, "uHaloA", halo);
    set1f(progText_, "uGlowA", 0.5f * (1.f - b));
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD: Jura, the labels in widely spaced medium capitals, the song and the notices light.
void MegaCityUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        std::vector<float> v;
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal_.accent : pal_.veil;
            const float a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{r.x, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y + r.h, c.x, c.y, c.z, a},
                                    {r.x, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y + r.h, c.x, c.y, c.z, a}, {r.x, r.y + r.h, c.x, c.y, c.z, a}};
            for (auto& p : qd) v.insert(v.end(), p, p + 6);
        }
        glUseProgram(progShape_);
        set2f(progShape_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(shapeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6));
    }
    const vec3 ink = pal_.ink, accent = pal_.accent;
    std::vector<float> q;
    const float halo = 0.65f;
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string name = h.text, var;
            const size_t dot = name.find("\xC2\xB7");
            if (dot != std::string::npos) { var = name.substr(dot + 2); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            const float sz = base * 0.9f, tr = sz * 0.32f;
            const float wName = textWidth(medium_, name, sz, tr);
            const float wVar = var.empty() ? 0.f : textWidth(light_, var, sz, tr * 0.8f);
            const float gap = base * 0.8f;
            const float x = ugl::hudLeft(h, wName + (var.empty() ? 0.f : gap + wVar), width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(medium_, name, x, h.y, sz, tr, q);
            drawText(medium_, q, ink, a, halo);
            if (!var.empty()) {
                q.clear();
                textQuads(light_, var, x + wName + gap, h.y, sz, tr * 0.8f, q);
                drawText(light_, q, accent, a * 0.95f, halo);
            }
            continue;
        }
        case HUD_SECTION:
            place(medium_, h.text, base * 0.85f, base * 0.55f);
            drawText(medium_, q, ink, saturate(0.2f + h.alpha * 1.9f), halo);
            continue;
        case HUD_TITLE:
            place(light_, h.text, base * 1.4f, base * 0.06f, -base * 0.2f);
            drawText(light_, q, ink, saturate(h.alpha * 1.3f), halo);
            continue;
        case HUD_INFO:
            place(medium_, h.text, base * 0.68f, base * 0.25f);
            drawText(medium_, q, ink, saturate(alpha * 1.3f) * 0.85f, halo);
            continue;
        case HUD_LABEL:
            place(medium_, h.text, base * 0.7f, base * 0.45f);
            drawText(medium_, q, lerp(ink, accent, 0.5f), saturate(alpha * 1.2f) * 0.92f, halo);
            continue;
        case HUD_VALUE:
            place(medium_, h.text, base * 1.1f, base * 0.1f);
            drawText(medium_, q, ink, alpha, halo);
            continue;
        case HUD_NOTICE:
            place(light_, h.text, base * 1.3f, base * 0.08f);
            drawText(light_, q, accent, alpha, halo);
            continue;
        case HUD_KEY:
            place(medium_, h.text, base * 0.78f, base * 0.12f);
            drawText(medium_, q, ink, alpha, halo);
            continue;
        case HUD_TOAST:
            place(medium_, h.text, base * 1.15f, base * 0.55f, -base * 0.15f);
            drawText(medium_, q, ink, alpha, 0.85f);
            continue;
        case HUD_TOAST_SUB:
            place(light_, h.text, base * 1.2f, base * 0.1f);
            drawText(light_, q, accent, alpha * 0.9f, 0.85f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(medium_, h.text, base * 1.05f, base * 0.25f, -base * 0.1f);
                drawText(medium_, q, ink, alpha, 0.85f);
            } else {
                place(medium_, h.text, base * 0.78f, base * 0.14f);
                drawText(medium_, q, h.accent ? accent : ink, alpha, 0.85f);
            }
            continue;
        default:
            place(medium_, h.text, base * 0.78f, base * 0.12f);
            drawText(medium_, q, h.accent ? accent : ink, alpha, halo);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MegaCityUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 3.5f) : 0.f, overT_ >= 0.f ? 10.f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    // a real drop, not a chorus (both rise into a peak): from the HUD's section, else from the music's pace
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && overT_ < 0.f && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    // the beat (a steady 120 BPM without a song)
    if (!f.paused && !music.hasSong) fakeBeat_ += dt * 2.0;
    const double beat = music.hasSong ? music.beat : fakeBeat_;

    // ---- The game's lines -> the growth (a new game starts again from a hut).
    {
        const int lines = game.lines() + extraLines(time_);
        if (citySeed_ == 0) { citySeed_ = seed_ ? seed_ : 1; cityDirty_ = true; }
        if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
            citySeed_ = splitmix64(citySeed_ + 0x9E37ull);
            cityDirty_ = true;
            gBonus_ = 0.f;
            g_ = growthOf(lines);
            gVel_ = 0.f;
            dropR0_ = dropR1_ = 0.f;
            sparks_.clear();
            resetT_ = 0.f;
        }
        lines_ = lines;
        if (cityDirty_) uploadCity();
        if (isDrop && !wasDrop_ && g_ < G_MAX - 0.05f) {
            // the drop raises a whole new district: the city's radius jumps out by half, its lights to come on
            const float r0 = std::exp(g_);
            gBonus_ += 0.42f;
            const float r1 = std::exp(std::min(G_MAX, growthOf(lines) + gBonus_));
            dropR0_ = r0 * 1.01f;
            dropR1_ = r1 * 1.02f;
            dropBeat0_ = std::floor(beat) + 1.0;
            dropLit_ = false;
        }
        gTarget_ = std::min(G_MAX, growthOf(lines) + gBonus_);
        const float over = (float)(lines - (int)STAGES[STAGE_COUNT - 1].lines) + gBonus_ * 8.f;
        orbit_ = approach(orbit_, saturate(over / 40.f) * (gTarget_ >= G_MAX - 1e-3f ? 1.f : 0.f), 0.6f, wdt);
        if (linesShown_ < 0) { g_ = gTarget_; gVel_ = 0.f; }
        linesShown_ = lines;
        // a spurt: a spring toward the target; a drop's district rises more slowly (a wave rolling outward)
        const float w = dropR1_ > 0.f && std::exp(g_) < dropR1_ * 0.98f ? 1.2f : 3.6f;
        gVel_ += (w * w * (gTarget_ - g_) - 2.f * w * gVel_) * wdt;
        g_ += gVel_ * wdt;
        if (g_ > gTarget_ + 0.001f && gVel_ > 0.f) { g_ = gTarget_; gVel_ = 0.f; }
        resetT_ += dt;
    }
    spurt_ = approach(spurt_, 0.f, 1.0f, dt);
    // the drop's district: its blocks' lights come on one beat after the other (a share of them each beat)
    if (dropR1_ > 0.f) {
        const float b = (float)(beat - dropBeat0_);
        dropBeats_ = b < 0.f ? 0.f : std::floor(b) + smoothstepf(0.f, 0.35f, b - std::floor(b));
        if (dropBeats_ > 20.f) dropLit_ = true;
    }
    const float dropShare = dropR1_ > 0.f ? saturate(dropBeats_ / 16.f) * 1.05f : 1.1f;

    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.5f : 0.4f, dt);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    beams_ = approach(beams_, isDrop ? 1.f : 0.f, isDrop ? 0.8f : 0.5f, dt);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        if (slow_ > 0.01f) {
            // slow motion: a hush, the light cooler
            const float k = slow_;
            pal_.winWarm = lerp(pal_.winWarm, pal_.winWarm * vec3(0.8f, 0.9f, 1.2f), k);
            pal_.street = lerp(pal_.street, pal_.street * vec3(0.8f, 0.9f, 1.2f), k);
            pal_.nightHaze = lerp(pal_.nightHaze, pal_.nightHaze * vec3(0.85f, 0.9f, 1.25f), k);
        }
    }
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    tetrisGlow_ = approach(tetrisGlow_, 0.f, 0.4f, dt);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    traffic_ = approach(traffic_, 0.35f + 0.3f * wm + 0.35f * wp + 0.3f * life, 0.8f, dt);
    trafficPhase_ += wdt * (0.25f + 0.25f * wm + 0.35f * wp + 0.2f * life);

    updateCamera(dt);
    const float G = std::min(g_, G_MAX);
    const float cityR = std::exp(G);
    const float gcv = std::min(gc_, G_MAX);
    const float nightK0 = smoothstepf(std::log(7.f), std::log(5000.f), gcv);
    const float nightK = nightK0 + pal_.nightBias * (1.f - nightK0);
    const float glowK = smoothstepf(std::log(300.f), std::log(2.e4f), gcv) * (0.85f + 0.15f * wp + 0.2f * life) * (1.f - 0.7f * over_);
    const float spaceK = smoothstepf(1.2e4f, 9.e4f, alt_);
    const float R = mcity::R_EARTH;
    const vec3 fwdH(-std::sin(CAM_AZ), 0.f, -std::cos(CAM_AZ)), rightH(std::cos(CAM_AZ), 0.f, -std::sin(CAM_AZ));
    // the sun: just setting at the first line, sinking with the night, behind the planet at the end
    const float sunEl = lerpf(1.2f, -15.f, nightK) - 40.f * smoothstepf(std::log(2.e6f), std::log(1.6e7f), gcv);
    const float sunElR = sunEl * PI / 180.f;
    const vec3 sunDir = normalize((fwdH * std::cos(pal_.sunAz) + rightH * std::sin(pal_.sunAz)) * std::cos(sunElR) + vec3(0.f, std::sin(sunElR), 0.f));
    const float sunUp = smoothstepf(-0.06f, 0.03f, std::sin(sunElR));
    const float lit = saturate((0.42f + 0.14f * wm + 0.16f * wp + 0.08f * life + 0.1f * swell_) * (0.75f + 0.25f * nightK));
    const float overR = over_ > 0.001f ? cityR * 1.02f * (1.f - over_) : 1e30f;
    // the pixel's size at the anchor; lots smaller than a few of them are left to the ground's shader
    const float pxM = camD_ * 2.f * tanH_ / (float)worldT_.h;
    const float drawR0 = 2.2f * pxM / 0.07f;

    // ---- Lights in the air (meters).
    {
        const float s = std::max(cityR, 4.f);
        auto groundAt = [&](float r, float ph) {
            const float th = r / R, s2 = std::sin(th * 0.5f);
            return vec3(R * std::sin(th) * std::cos(ph), -2.f * R * s2 * s2, R * std::sin(th) * std::sin(ph));
        };
        if (!f.paused) {
            // flying traffic over the city once it is big (more at a peak, many on the drop)
            if (cityR > 3000.f && cityR < 8.e6f) flyAcc_ += (6.f + 10.f * wm + 16.f * wp + 30.f * life) * wdt;
            // fireflies by the hut at dusk
            if (cityR < 150.f) fireflyAcc_ += 3.f * wdt;
        }
        flyAcc_ = std::min(flyAcc_, 30.f);
        while (flyAcc_ >= 1.f && (int)sparks_.size() < MAX_SPARKS) {
            flyAcc_ -= 1.f;
            Spark q;
            const float r = s * std::sqrt(rng_.range(0.02f, 0.95f)), ph = rng_.range(0.f, TAU);
            const vec3 base = groundAt(r, ph);
            const vec3 up = normalize(base + vec3(0.f, R, 0.f));
            q.p = base + up * (s * rng_.range(0.004f, 0.02f) + 30.f);
            const vec3 tang = normalize(cross(up, normalize(base + vec3(1e-3f, 0.f, 0.f))));
            q.v = tang * (s * rng_.range(0.012f, 0.03f) * (rng_.chance(0.5f) ? 1.f : -1.f));
            q.age = 0.f;
            q.life = rng_.range(5.f, 9.f);
            q.size = s * 0.0016f;
            q.seed = rng_.uniform();
            q.kind = 0;
            q.col = rng_.chance(0.5f) ? lerp(pal_.winCool, vec3(1.f), 0.4f) * 1.3f : (rng_.chance(0.6f) ? pal_.traffic * 1.4f : pal_.winAccent * 1.4f);
            sparks_.push_back(q);
        }
        // the drop over a town: sky lanterns rising from the new district (searchlights once it is a modern city)
        const bool town = dropR1_ > 0.f && cityR < 6000.f;
        if (!f.paused && beams_ > 0.01f && town) lanternAcc_ += 6.f * beams_ * wdt;
        if (isDrop && !lanternBurst_ && town) lanternAcc_ += 35.f;
        lanternBurst_ = isDrop;
        lanternAcc_ = std::min(lanternAcc_, 40.f);
        while (lanternAcc_ >= 1.f && (int)sparks_.size() < MAX_SPARKS) {
            lanternAcc_ -= 1.f;
            Spark q;
            const float r = rng_.range(std::max(dropR0_ * 0.6f, 5.f), std::min(dropR1_, cityR)), ph = rng_.range(-1.4f, 1.4f);
            const vec3 base = groundAt(r, ph);
            q.p = base + vec3(0.f, rng_.range(2.f, 6.f) + cityR * 0.01f, 0.f);
            q.v = vec3(rng_.range(-0.1f, 0.3f), 1.f, rng_.range(-0.15f, 0.15f)) * (1.f + cityR * 0.012f);
            q.age = -rng_.range(0.f, 0.6f);
            q.life = rng_.range(7.f, 11.f);
            q.size = 0.3f + cityR * 0.005f;
            q.seed = rng_.uniform();
            q.kind = 3;
            q.col = lerp(pal_.winWarm, vec3(1.f, 0.6f, 0.25f), rng_.uniform() * 0.5f) * 1.8f;
            sparks_.push_back(q);
        }
        fireflyAcc_ = std::min(fireflyAcc_, 10.f);
        while (fireflyAcc_ >= 1.f && (int)sparks_.size() < MAX_SPARKS) {
            fireflyAcc_ -= 1.f;
            Spark q;
            q.p = vec3(rng_.range(-6.f, 22.f), rng_.range(0.3f, 2.2f), rng_.range(-14.f, 14.f));
            q.v = vec3(rng_.range(-0.3f, 0.3f), rng_.range(-0.05f, 0.1f), rng_.range(-0.3f, 0.3f));
            q.age = 0.f;
            q.life = rng_.range(3.f, 6.f);
            q.size = 0.05f;
            q.seed = rng_.uniform();
            q.kind = 1;
            q.col = vec3(0.9f, 1.f, 0.4f) * 1.6f;
            sparks_.push_back(q);
        }
        for (Spark& q : sparks_) {
            q.age += wdt;
            if (q.age < 0.f) continue;
            if (q.kind == 2) {
                q.v = q.v * std::exp(-1.6f * wdt);
                const vec3 up = normalize(q.p + vec3(0.f, R, 0.f));
                q.p += (q.v - up * (s * 0.006f + 1.5f) * q.age * 0.6f) * wdt;
            } else if (q.kind == 3) {
                q.p += (q.v + vec3(std::sin(time_ * 0.5f + q.seed * 20.f) * 0.15f, 0.f, 0.f) * (1.f + cityR * 0.012f)) * wdt;
            } else if (q.kind == 1) {
                q.p += (q.v + vec3(std::sin(time_ * 1.3f + q.seed * 30.f) * 0.2f, std::sin(time_ * 0.9f + q.seed * 17.f) * 0.08f, 0.f)) * wdt;
            } else {
                q.p += q.v * wdt * lerpf(1.f, 0.4f, slow_);
            }
        }
        sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& q) { return q.age >= q.life; }), sparks_.end());

        ginst_.clear();
        auto glow = [&](const vec3& p, float size, const vec3& c, float a, int mode, float seed, float param,
                        const vec3& dir = vec3(0.f), float len = 0.f) {
            if (a <= 0.004f) return;
            const float q[16] = {p.x, p.y, p.z, size, c.x, c.y, c.z, a, (float)mode, seed, param, 0.f, dir.x, dir.y, dir.z, len};
            ginst_.insert(ginst_.end(), q, q + 16);
        };
        // the trees around the hut, until the town reaches them
        if (cityR < 900.f) {
            const vec3 tint = lerp(vec3(1.f), vec3(0.55f, 0.6f, 0.85f), nightK * 0.8f);
            for (size_t i = 0; i + 3 < plan_.trees.size(); i += 4) {
                const float x = plan_.trees[i], z = plan_.trees[i + 1], sz = plan_.trees[i + 2], sd = plan_.trees[i + 3];
                const float r = std::sqrt(x * x + z * z);
                const float dT = mcity::field(x, z, plan_.sats);
                const float keep = 1.f - smoothstepf(std::max(dT * 1.1f, 230.f), std::max(dT * 1.4f, 300.f), cityR);
                (void)r;
                if (keep <= 0.f) continue;
                glow(vec3(x, -0.05f, z), sz * keep, tint, world, 2, sd, std::sin(pal_.sunAz) > 0.f ? 0.6f : -0.6f);
            }
        }
        // the hut's chimney smoke
        if (cityR < 400.f) {
            const float sm = 1.f - smoothstepf(60.f, 400.f, cityR);
            for (int i = 0; i < 9; i++) {
                const float t = std::fmod(time_ * 0.25f + i / 9.f, 1.f);
                const vec3 p(0.3f + t * 1.6f + 0.25f * std::sin(time_ * 0.7f + i), 3.9f + t * 5.f, -0.6f + t * 0.8f);
                glow(p, 0.35f + t * 1.4f, lerp(vec3(0.55f, 0.55f, 0.62f), pal_.nightHaze * 3.f, nightK) * 0.6f, 0.38f * (1.f - t) * smoothstepf(0.f, 0.1f, t) * sm * world, 4, (float)i, 0.f);
            }
            // the hearth's light spilling on the ground at the door
            glow(vec3(1.9f, 0.15f, -0.3f), 1.6f, pal_.winWarm, 0.35f * world * sm, 0, 0.3f, 0.f);
        }
        // red beacons on the tall buildings (blinking slowly; on the beat grid at a peak, each its own phase)
        if (cityR > 2000.f) {
            const size_t nb = plan_.beacons.size() / 4;
            const float rLo = std::max(drawR0 * 0.5f, 0.f);
            size_t i0 = 0, i1 = nb;
            {
                size_t lo = 0, hi = nb;
                while (lo < hi) { const size_t m = (lo + hi) / 2; if (plan_.beacons[m * 4 + 3] < rLo) lo = m + 1; else hi = m; }
                i0 = lo;
                lo = i0; hi = nb;
                while (lo < hi) { const size_t m = (lo + hi) / 2; if (plan_.beacons[m * 4 + 3] < cityR) lo = m + 1; else hi = m; }
                i1 = lo;
            }
            if (i1 > i0 + 2500) i0 = i1 - 2500;
            const float bsz = std::max(1.2f, cityR * 0.00025f);
            for (size_t i = i0; i < i1; i++) {
                const float* b = &plan_.beacons[i * 4];
                const float h = mcity::hashLot((float)i, 3.f);
                const float slowB = 0.5f + 0.5f * std::sin(time_ * 1.4f + h * 30.f);
                const float ph = (float)std::fmod(beat * 0.5 + h, 1.0);
                const float gridB = 1.f - smoothstepf(0.0f, 0.3f, ph);
                const float on = lerpf(smoothstepf(0.3f, 0.9f, slowB), gridB, wp) * (b[3] < overR ? 1.f : 0.f);
                glow(vec3(b[0], b[1], b[2]), bsz, pal_.beacon * 1.1f, (0.15f + 0.6f * on) * world * nightK * (h < 0.4f ? 1.f : 0.f), 1, h, 0.f);
            }
        }
        // the space elevator: its tether rising into orbit, climbers on it, the station at its top
        const float elevK = smoothstepf(std::log(5.5e5f), std::log(3.e6f), G);
        if (elevK > 0.f && alt_ > 100.f) {
            const vec3 top = plan_.elevator + plan_.elevatorUp * 46000.f;
            const float L = elevK * RING_ALT;
            glow(top, 1500.f, lerp(pal_.winCool, vec3(1.f), 0.3f) * 1.2f, 0.85f * world, 3, 0.f, 0.f, plan_.elevatorUp, L);
            glow(top + plan_.elevatorUp * L, 30000.f, lerp(pal_.winAccent, vec3(1.f), 0.4f) * 2.f, world * elevK, 1, 0.5f, 0.f);
            for (int c = 0; c < 6; c++) {
                const float t = std::fmod(time_ * 0.05f + c / 6.f, 1.f);
                glow(top + plan_.elevatorUp * (L * t), 8000.f, pal_.winWarm * 2.f, world * elevK * smoothstepf(0.f, 0.05f, t) * (1.f - smoothstepf(0.9f, 1.f, t)), 1, (float)c, 0.f);
            }
        }
        // the orbital ring: built around the planet from the elevator's top, more tethers as it passes over them
        if (orbit_ > 0.002f) {
            const vec3 e1 = plan_.elevatorUp;
            const vec3 e2 = normalize(rightH * 0.85f - fwdH * 0.5f);
            const float Rr = R + RING_ALT;
            const vec3 c0(0.f, -R, 0.f);
            const int N = 720;
            const float span = orbit_ * PI;
            const float seg = TAU * Rr / N;
            for (int i = 0; i < N; i++) {
                float a = (i + 0.5f) / N * TAU;
                if (a > PI) a -= TAU;
                const float k = 1.f - smoothstepf(span - 0.08f, span, std::fabs(a));
                if (k <= 0.f) continue;
                const vec3 p = c0 + (e1 * std::cos(a) + e2 * std::sin(a)) * Rr;
                const float twinkle = 0.85f + 0.15f * std::sin(time_ * 0.8f + i * 0.7f);
                glow(p, seg * 0.9f, lerp(pal_.winCool, pal_.winAccent, 0.35f) * 1.3f * twinkle, k * world, 0, (float)i, 0.f);
            }
            for (int t = 1; t < 6; t++) {
                float a = t * TAU / 6.f;
                if (a > PI) a -= TAU;
                const float k = smoothstepf(std::fabs(a) + 0.05f, std::fabs(a) + 0.3f, span);
                if (k <= 0.f) continue;
                const vec3 dir = e1 * std::cos(a) + e2 * std::sin(a);
                glow(c0 + dir * R, 1500.f, lerp(pal_.winCool, vec3(1.f), 0.3f), 0.8f * k * world, 3, 0.f, 0.f, dir, RING_ALT * k);
            }
        }
        // the drop's searchlights: slow beams sweeping the sky from the new district
        if (beams_ > 0.01f && dropR1_ > 0.f && cityR > 4000.f && cityR < 3.e6f) {
            for (int i = 0; i < 4; i++) {
                // on the far side of the city, so they rise behind it, sweeping slowly
                const float ph = -0.75f + i * 0.5f, r = lerpf(dropR0_, std::min(dropR1_, cityR), 0.35f + 0.15f * (i % 2));
                const vec3 base = groundAt(r, ph);
                const vec3 up = normalize(base + vec3(0.f, R, 0.f));
                const vec3 t1 = normalize(cross(up, vec3(0.f, 0.f, 1.f))), t2 = cross(up, t1);
                const float a = time_ * 0.22f + i * 1.9f;
                const vec3 dir = normalize(up * 2.6f + t1 * std::cos(a) + t2 * std::sin(a));
                const vec3 col = i % 2 ? lerp(pal_.winCool, vec3(1.f), 0.3f) : lerp(pal_.winWarm, vec3(1.f), 0.3f);
                const float q[16] = {base.x, base.y, base.z, cityR * 0.004f, col.x, col.y, col.z, 0.16f * beams_ * world, 3.f, 0.f, 1.f, 12.f,
                                     dir.x, dir.y, dir.z, cityR * 0.7f};
                ginst_.insert(ginst_.end(), q, q + 16);
                (void)glow;
            }
        }
        for (const Spark& q : sparks_) {
            if (q.age < 0.f) continue;
            const float t = q.age / q.life;
            float a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.6f, 1.f, t));
            if (q.kind == 1) a *= 0.5f + 0.5f * std::sin(time_ * 3.f + q.seed * 40.f);
            if (q.kind == 3) a *= 0.85f + 0.15f * std::sin(time_ * 2.3f + q.seed * 30.f);
            glow(q.p, q.size, q.col, a * world * (q.kind == 0 ? nightK : 1.f), q.kind == 2 ? 0 : 1, q.seed, 0.f);
        }
    }

    // ---- 2D motes over the board.
    for (Mote& m : motes_) {
        m.age += dt;
        if (m.kind == 1) {
            const float t = smoothstepf(0.f, 1.f, saturate(m.age / m.life));
            const vec2 a = lerp2(m.from, m.ctl, t), b = lerp2(m.ctl, m.to, t);
            m.p = lerp2(a, b, t);
        } else {
            m.p = add(m.p, mul(m.v, dt));
            if (m.kind == 3) m.v = mul(m.v, std::exp(-2.5f * dt));
        }
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.age >= m.life; }), motes_.end());
    for (auto& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    sprites_.clear();
    for (const Mote& m : motes_) {
        if (m.age < 0.f) continue;
        const float t = m.age / m.life;
        float a = smoothstepf(0.f, 0.12f, t) * (1.f - smoothstepf(0.6f, 1.f, t)) * m.bright;
        if (m.kind == 1) a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.85f, 1.f, t)) * m.bright;
        sprite(m.p, m.size * (m.kind == 1 ? 2.1f : 1.6f), m.col, a * world, 0);
    }
    {
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        sprite(V(gc.x, gc.y), layout::GAUGE_R * 1.25f, on ? pal_.winWarm : pal_.accent, 1.f, 1, fill, on ? 1.f : 0.f);
    }
    cells_.clear();
    buildBoard(game);

    // =========================================================== the world (reduced resolution)
    const vec3 skyAmb = lerp(lerp(pal_.duskZenith, pal_.duskHorizon, 0.45f) * 1.1f, pal_.nightZenith * 1.8f + vec3(0.006f, 0.007f, 0.012f), nightK);
    auto setLight = [&](GLuint p) {
        set3f(p, "uEye", eye_);
        set3f(p, "uSunDir", sunDir);
        set3f(p, "uSunC", pal_.lit * (0.25f + 0.75f * sunUp));
        set3f(p, "uShadowC", pal_.shadow);
        set3f(p, "uSunGlow", pal_.sunGlow);
        set3f(p, "uHazeC", lerp(pal_.haze, pal_.nightHaze, nightK) + pal_.cityGlow * glowK * 0.04f);
        set3f(p, "uSkyAmb", skyAmb);
        set1f(p, "uFogK", 2.4e-5f);
        set1f(p, "uAlt", alt_);
        set1f(p, "uNightK", nightK);
        set1f(p, "uTime", time_);
        set1f(p, "uBridge", bridge);
        set3f(p, "uBridgeCol", bridge_.color);
        set3f(p, "uWinWarm", pal_.winWarm);
        set3f(p, "uWinCool", pal_.winCool);
        set3f(p, "uWinAccent", pal_.winAccent);
        set1f(p, "uCityR", cityR);
        set1f(p, "uLit", lit);
        set1f(p, "uGlowK", glowK);
        set1f(p, "uOverR", overR);
        set1f(p, "uNeon", pal_.neon);
        set4f(p, "uDrop", dropR0_, dropR1_, dropShare, 0.f);
        {
            float sv[mcity::SATS * 3];
            for (int j = 0; j < mcity::SATS; j++) { sv[j * 3] = plan_.sats[j].x; sv[j * 3 + 1] = plan_.sats[j].z; sv[j * 3 + 2] = plan_.sats[j].a; }
            glUniform3fv(ugl::U(p, "uSats"), mcity::SATS, sv);
        }
    };
    const float minPx = 2.f * tanH_ / worldT_.h;

    worldT_.bind();
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(progSky_);
    setLight(progSky_);
    setMat(progSky_, "uVP", vp3_);
    set3f(progSky_, "uLook", look_);
    set3f(progSky_, "uCamR", camR_);
    set3f(progSky_, "uCamU", camU_);
    set2f(progSky_, "uTan", tanH_ * aspect_, tanH_);
    set1f(progSky_, "uPix", minPx);
    set1f(progSky_, "uStars", 0.7f + 0.3f * pal_.snow);
    set1f(progSky_, "uHills", 1.f - smoothstepf(300.f, 4000.f, alt_));
    set1f(progSky_, "uSpaceK", spaceK);
    set1f(progSky_, "uAurora", pal_.aurora * (0.6f + 0.25f * wm + 0.35f * wp));
    set1f(progSky_, "uSnow", pal_.snow);
    set3f(progSky_, "uDuskZ", pal_.duskZenith);
    set3f(progSky_, "uDuskH", pal_.duskHorizon);
    set3f(progSky_, "uNightZ", pal_.nightZenith);
    set3f(progSky_, "uNightH", pal_.nightHorizon);
    set3f(progSky_, "uSpaceC", pal_.space);
    set3f(progSky_, "uNebulaC", pal_.nebula);
    set3f(progSky_, "uMoonC", pal_.moonC);
    set3f(progSky_, "uCityGlowC", pal_.cityGlow);
    set3f(progSky_, "uGrass", pal_.grass);
    set3f(progSky_, "uGrass2", pal_.grass2);
    set3f(progSky_, "uFieldC", pal_.field);
    set3f(progSky_, "uLandC", pal_.land);
    set3f(progSky_, "uDesertC", pal_.desert);
    set3f(progSky_, "uOceanC", pal_.ocean);
    set3f(progSky_, "uRiverC", pal_.river);
    set3f(progSky_, "uForestC", pal_.forest);
    set3f(progSky_, "uStreetC", pal_.street);
    set3f(progSky_, "uTrafficC", pal_.traffic);
    {
        // the moon: high on the left of the first view, far
        const vec3 moon = (fwdH * 0.8f - rightH * 0.45f) * 3.0e8f + vec3(0.f, 1.6e8f, 0.f);
        set4f(progSky_, "uMoon", moon.x, moon.y, moon.z, 1.9e6f * 1.6f);
    }
    set1f(progSky_, "uDrawR0", drawR0);
    set1f(progSky_, "uTraffic", traffic_ * (1.f - over_));
    set1f(progSky_, "uTrafficPhase", trafficPhase_);
    set1f(progSky_, "uBuild", (0.2f + 0.8f * spurt_) * smoothstepf(std::log(400.f), std::log(4000.f), G) * (1.f - over_));
    ugl::drawFullscreen();

    glDepthFunc(GL_LESS);
    // the buildings: those big enough to see, born by now (and the elevator's tower)
    if (bldCount_ > 0) {
        const float rLo = drawR0 > 400.f ? drawR0 * 0.6f : 0.f;
        const auto& B = plan_.births;
        const int fixedFirst = std::min(plan_.fixedFirst, (int)B.size());
        const int first = (int)(std::lower_bound(B.begin(), B.begin() + fixedFirst, rLo) - B.begin());
        const int last = (int)(std::upper_bound(B.begin(), B.begin() + fixedFirst, cityR) - B.begin());
        glUseProgram(progBld_);
        setLight(progBld_);
        setMat(progBld_, "uVP", vp3_);
        set1f(progBld_, "uMinPx", minPx);
        set3f(progBld_, "uWallA", pal_.wallA);
        set3f(progBld_, "uWallB", pal_.wallB);
        set3f(progBld_, "uRoofA", pal_.roofA);
        set3f(progBld_, "uRoofB", pal_.roofB);
        set3f(progBld_, "uStone", pal_.stone);
        set3f(progBld_, "uGlassC", pal_.glass);
        set3f(progBld_, "uArcoC", pal_.arco);
        set3f(progBld_, "uCityGlowC", pal_.cityGlow);
        set3f(progBld_, "uMoonDir", normalize(fwdH * 0.8f - rightH * 0.45f + vec3(0.f, 0.53f, 0.f)));
        set3f(progBld_, "uMoonL", pal_.moonC * 0.045f + pal_.nightHorizon * 0.3f);
        glBindVertexArray(bldVao_);
        auto drawRange = [&](int a, int b) {
            if (b <= a) return;
            glBindBuffer(GL_ARRAY_BUFFER, bldVbo_);
            for (int i = 0; i < 5; i++)
                glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 20, (void*)(sizeof(float) * (20 * (size_t)a + 4 * i)));
            glDrawArraysInstanced(GL_TRIANGLES, 0, meshCount_, b - a);
        };
        drawRange(first, last);
        if (cityR > 4.e5f) drawRange(fixedFirst, (int)B.size());
    }

    // ---- The lights in the air (depth tested, not written).
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    if (!ginst_.empty()) {
        glUseProgram(progGlow_);
        setMat(progGlow_, "uVP", vp3_);
        set3f(progGlow_, "uEye", eye_);
        set3f(progGlow_, "uCamR", camR_);
        set3f(progGlow_, "uCamU", camU_);
        set1f(progGlow_, "uMinPx", minPx);
        set3f(progGlow_, "uTreeC", lerp(pal_.forest * 2.4f, pal_.grass, 0.25f) * (1.f - 0.3f * pal_.snow));
        set3f(progGlow_, "uTreeShade", pal_.forest * 0.85f + pal_.shadow * 0.15f);
        set3f(progGlow_, "uTrunkC", pal_.roofB * 0.6f);
        glBindVertexArray(glowVao_);
        glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
        glBufferData(GL_ARRAY_BUFFER, ginst_.size() * sizeof(float), ginst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(ginst_.size() / 16));
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // ---- The bloom (quarter resolution).
    bloomA_.bind();
    glUseProgram(progBright_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progBright_, "uSrc", 0);
    set2f(progBright_, "uTexel", 1.f / worldT_.w, 1.f / worldT_.h);
    set1f(progBright_, "uThreshold", lerpf(0.75f, 0.35f, nightK));
    ugl::drawFullscreen();
    for (int pass = 0; pass < 2; pass++) {
        bloomB_.bind();
        glUseProgram(progBlur_);
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        set1i(progBlur_, "uSrc", 0);
        set2f(progBlur_, "uDir", 1.f / bloomA_.w, 0.f);
        ugl::drawFullscreen();
        bloomA_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        set2f(progBlur_, "uDir", 0.f, 1.f / bloomA_.h);
        ugl::drawFullscreen();
    }

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progComp_, "uWorld", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set1i(progComp_, "uBloomT", 1);
    glActiveTexture(GL_TEXTURE0);
    set3f(progComp_, "uLook", look_);
    set3f(progComp_, "uCamR", camR_);
    set3f(progComp_, "uCamU", camU_);
    set2f(progComp_, "uTan", tanH_ * aspect_, tanH_);
    set2f(progComp_, "uRes", (float)width_, (float)height_);
    set1f(progComp_, "uBloomK", (0.45f + 0.35f * nightK) * (1.f + 0.2f * wp + 0.3f * life + 0.3f * tetrisGlow_) * world);
    set1f(progComp_, "uTwinkle", 0.25f + 0.2f * wm + 0.2f * wp);
    set1f(progComp_, "uTime", time_);
    set1f(progComp_, "uSat", 1.12f);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    // ---- The veil behind the well and the HUD's columns.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progVeil_);
    set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
    set3f(progVeil_, "uRail", lerp(pal_.winWarm * 0.45f, bridge_.ink * 0.4f, bridge));
    set1f(progVeil_, "uWellA", 0.78f);
    set1f(progVeil_, "uColA", (0.42f + 0.1f * (1.f - nightK)) * world);
    set1f(progVeil_, "uRailA", 0.5f);
    set1f(progVeil_, "uPx", pxPerUnit_);
    set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.f, layout::LEFT_COLUMN[3]);
    set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    ugl::drawFullscreen();

    // ---- Pieces, previews, the motes over them, the HUD.
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uTime", time_);
        set3f(progCell_, "uLightC", lerp(pal_.winWarm, vec3(1.f), 0.45f));
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    if (!sprites_.empty()) {
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        set1f(progSprite_, "uPxPerUnit", pxPerUnit_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(f.hud, f.panels);
    glDisable(GL_BLEND);

    // ---- Post.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progPost_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progPost_, "uScene", 0);
    set1f(progPost_, "uFade", saturate(f.fade));
    set1f(progPost_, "uDim", dimT_);
    ugl::drawFullscreen();
    (void)spaceK;
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_megacity() {
    static const UniverseEntry e{
        "MEGACITY", "a hut on a riverbank growing with every cleared line into a city that covers the whole planet",
        [] { return std::unique_ptr<Universe>(new MegaCityUniverse()); },
        // Fits most songs; the driving, dense ones a little better.
        [](const SongLook& l) { return 0.85f + 0.15f * l.density + 0.1f * l.bassWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
