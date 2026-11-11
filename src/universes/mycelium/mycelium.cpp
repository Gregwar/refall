// MYCELIUM: a glowing fungal network grows over the forest floor at night with the cleared lines, from a single spore
// among the moss until it wraps the whole Earth in filaments of light.
//
// It starts as close as a macro lens: one spore glowing on the forest floor at dusk, its first germ tubes reaching
// out between the fallen leaves, moss cushions and their capsules standing around like a little forest, the real
// forest a far tree line under the moon. Every cleared line spurts the network outward (it is grown once per game in
// log space: network.cpp, the same density of threads at every scale it reaches) and the camera rises and pulls back
// with it: the moss, the clearing, the forest, a valley with a village and hot-air balloons, the clouds, a region
// where the threads glow under the land like veins of light, a continent, and at last the Earth seen from space on
// its night side, wrapped in a living web of light, the moon beyond. Mushrooms burst up where the threads run, each
// bigger than the last (a pin, then a cap opening with a little overshoot): caps among the moss, parasols taller than
// the trees, towers standing above the clouds, glowing pins all over the globe. The growth is G = ln(the network's
// reach in meters): 2 mm at the start, 1 m at ~8 lines, 500 m (the village) at ~20, 75 km at ~30, the planet a ball
// at ~43, wrapped at ~50 (one song).
//
// The look: a moonlit night in clean colors, toon light (crisp bands, cool tinted shades), the threads and caps the
// only bright things: the threads glow over the ground (thin, crisp ribbons that never get thinner than a pixel, a
// soft bloom around them, a pool of their light under the land), the caps are translucent and lit from within, they
// light the ground around them. The sky, the planet (the forest floor at every scale, forests and fields, the
// village's windows, continents, oceans, cities, the day side's crescent from space), the moon and space are one
// ray-traced pass. The music: calm is a still network breathing; mid sends pulses of light out along some threads
// every two beats; a peak along all of them, the caps brighter, spores drifting up. **The drop** (a real one: the
// HUD's section, else the music's pace) sends one wave of light from the spore through the whole network; reaching
// each mushroom it climbs the stem, the cap flares and a cloud of glowing spores rises into the night.
//
// The pieces are mycelium felt: deep translucent shapes woven of glowing threads. Cleared rows fray into light that
// flies to the network's heart, which pulses it out along the threads.
//
// Cost: the world at a reduced resolution (at most 720 lines) with depth: one ray-traced sky pass, ~80k thread quads
// (most culled in the vertex shader: not yet born, or far under a pixel), the visible mushrooms turned on a lathe
// (~1.3k triangles each), a few thousand puffs and billboards, clouds; a quarter-resolution bloom; the stars, the veil,
// the pieces and the lettering at full resolution. Nothing is uploaded while the network grows (one upload a game).
#include "universes/mycelium/mycelium.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/mycelium/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Josefin Sans (light labels, regular numbers) and Yrsa Italic (the song, the notices), SIL Open Font License, see
// OFL-JosefinSans.txt and OFL-Yrsa.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/mycelium/JosefinSans-Light.ttf.inc"
};
const unsigned char FONT_CAPSB[] = {
#include "universes/mycelium/JosefinSans-Regular.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/mycelium/Yrsa-Italic.ttf.inc"
};

namespace shaders = mycelium_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, shaders::COMMON_GLSL);
    std::string v(vs);
    v.insert(v.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(v.c_str(), f.c_str(), name);
}
// A world shader: the light in both stages.
GLuint compileWorld(const char* vs, const char* fs, const char* name) {
    const std::string pre = std::string(shaders::COMMON_GLSL) + shaders::LIGHT_GLSL;
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
float sq(float x) { return x * x; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): the night (sky top, sky low, the dusk's glow on the horizon, mist, space, the nebula, the moon's
// light, the shade's light); the ground (moss, soil, fallen leaves, fields, forests, far land, ocean, town lights);
// the plants (foliage, moss), bark, stones; the threads (body, tips, pulses), three cap colors, the stems, the clouds
// (lit, shade); the HUD's ink and accent, the veil; the seven pieces; the glow, the pool of light under the land, the
// stars, the bloom.
struct VariantDef {
    const char* name;
    unsigned skyTop, skyLow, dusk, mist, spaceC, nebula, moonC, ambient;
    unsigned moss, soil, litter, field, forest, land, ocean, city;
    unsigned flora, floraLit, bark, rock;
    unsigned thread, threadTip, pulse, cap[3], stem, cloudLit, cloudShade;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float glow, pool, stars, bloom;
};
const VariantDef VARIANTS[] = {
    // Ice-blue threads, turquoise, blue and mint caps under a deep blue night.
    {"GLACIER", 0x061234, 0x1c4684, 0x3c7cae, 0x0e2244, 0x02040e, 0x3a2a8a, 0xbcd4ff, 0x1c2c5a,
     0x2a6a54, 0x2e2a3c, 0x6a5a4a, 0x2e5a56, 0x123a3c, 0x2a4a44, 0x0a1e4a, 0xffc070,
     0x1e6a5e, 0x3a7a5c, 0x2a2a3e, 0x56607a,
     0x2ad4ff, 0xb0fff8, 0xeaffff, {0x34e4ff, 0x6a86ff, 0x6affc4}, 0xb8ccd6, 0x8aa4d4, 0x24386a,
     0xe6f4ff, 0x7ae6ff, 0x030812,
     {0x4ee6ff, 0xffd84a, 0xb88cff, 0x6ef0a0, 0xff6e86, 0x5c8cff, 0xffa04a}, 1.f, 1.f, 1.f, 0.7f},
    // Magenta threads, pink, violet and coral caps in a plum night.
    {"ORCHID", 0x12082c, 0x44206e, 0x9a4686, 0x1c1230, 0x05020c, 0x8a2a8a, 0xffe0f4, 0x28243e,
     0x345050, 0x2e1e2e, 0x7a4a46, 0x44445a, 0x22223c, 0x3a3044, 0x160c36, 0xffb070,
     0x3e3466, 0x3a5a56, 0x2e2236, 0x6a5a72,
     0xff52d4, 0xffc0f0, 0xfff0fc, {0xff48b4, 0xa862ff, 0xff8466}, 0xd8c2d2, 0xb490c8, 0x3e2458,
     0xfff0fa, 0xff98de, 0x0c0512,
     {0x5ee0ff, 0xffe066, 0xd884ff, 0x7aef9a, 0xff6a8a, 0x6a8aff, 0xffa860}, 1.f, 1.f, 1.f, 0.7f},
    // Amber threads, honey, gold and red caps in a teal night.
    {"EMBER", 0x04182a, 0x124a5a, 0x2e7c7a, 0x0a2a34, 0x020608, 0x1a6a6a, 0xfff0d6, 0x183a40,
     0x2a5236, 0x2e2420, 0x7a5232, 0x30503a, 0x10302a, 0x2a3a2a, 0x082430, 0xffd080,
     0x1e5644, 0x3a5a34, 0x2a241e, 0x5a5a54,
     0xffa02a, 0xffe090, 0xfff4da, {0xff9426, 0xffcc44, 0xff5836}, 0xe2ceb2, 0x92b8b8, 0x1e4048,
     0xfff4e4, 0xffc66e, 0x030e10,
     {0x4ee8e0, 0xffe04a, 0xc890ff, 0x8af06a, 0xff6a5a, 0x6a9cff, 0xffa040}, 1.f, 1.f, 1.f, 0.7f},
    // Green foxfire: lime threads, emerald and chartreuse caps.
    {"JADE", 0x041020, 0x103a46, 0x2c6a5a, 0x0a2226, 0x02060a, 0x2a6a4a, 0xe8ffee, 0x16342e,
     0x2a5c32, 0x28241c, 0x6a5632, 0x2e5436, 0x0e3020, 0x2a402e, 0x061c2c, 0xffd080,
     0x1e5838, 0x3a6a34, 0x24261e, 0x4e5a52,
     0x74ff54, 0xd8ffaa, 0xf2fff0, {0x94ff36, 0x36ffac, 0xe4ff56}, 0xccdcc4, 0x94b8ae, 0x1c3c3a,
     0xf0fff0, 0xb4ff86, 0x020a06,
     {0x4ee0ff, 0xffd84a, 0xc08cff, 0x6af0b0, 0xff6e8a, 0x6a8cff, 0xffa24a}, 1.f, 1.f, 1.f, 0.7f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = MyceliumUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.skyTop = L(d.skyTop); p.skyLow = L(d.skyLow); p.dusk = L(d.dusk); p.mist = L(d.mist); p.spaceC = L(d.spaceC);
    p.nebula = L(d.nebula); p.moonC = L(d.moonC); p.ambient = L(d.ambient);
    p.moss = L(d.moss); p.soil = L(d.soil); p.litter = L(d.litter); p.field = L(d.field); p.forest = L(d.forest);
    p.land = L(d.land); p.ocean = L(d.ocean); p.city = L(d.city);
    p.flora = L(d.flora); p.floraLit = L(d.floraLit); p.bark = L(d.bark); p.rock = L(d.rock);
    p.thread = L(d.thread); p.threadTip = L(d.threadTip); p.pulse = L(d.pulse);
    for (int i = 0; i < 3; i++) p.cap[i] = L(d.cap[i]);
    p.stem = L(d.stem); p.cloudLit = L(d.cloudLit); p.cloudShade = L(d.cloudShade);
    p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.glow = d.glow; p.pool = d.pool; p.stars = d.stars; p.bloom = d.bloom;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

constexpr float CLEAR_T = 0.36f;     // a cleared row frays into light in this long (real time)
constexpr float FLOW_T = 1.2f;       // its light flies to the network's heart in this long
constexpr int MAX_MOTES = 1600;
constexpr int MAX_SPORES = 2400;
constexpr float CLOUD_Y = 2000.f;    // the cloud layer (meters)
constexpr float CAM_AZ = 0.35f;      // the camera's side of the spore
constexpr int LATHE_U = 22, LATHE_V = 40;
const float R_E = (float)myc::R_EARTH;

// Test hooks: ZEN_MYCELIUM_LINES=N adds N lines to the growth (to look at a late stage in a short shot); ZEN_GROW_RATE=R
// adds R more lines a second (a time-lapse).
int extraLines(double t) {
    static const int n = std::getenv("ZEN_MYCELIUM_LINES") ? std::atoi(std::getenv("ZEN_MYCELIUM_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return std::max(0, n) + (int)(rate * t);
}

// The camera for a growth: where it looks from and along, and how far it frames (its distance to the anchor).
// Close to the moss at first, the spore at the right of the well, low over the floor with the sky at the top; then
// rising and looking down more steeply as the network spreads (the clearing, the forest, the valley, the region),
// then out in space looking at the planet's night side, the moon beyond.
struct CamPose { vec3 eye, fwd; float D, tanH; };
CamPose cameraFor(float G, float aspect, float wobble, float spin = 0.f) {
    const float Rr = std::exp(G);
    const float kP = smoothstepf(std::log(4.e5f), std::log(5.e6f), G);    // the planet in the frame
    const float kO = smoothstepf(std::log(5.e6f), std::log(1.55e7f), G);  // pulling out to the moon
    const float kM = smoothstepf(std::log(0.03f), std::log(60.f), G);     // the floor -> the forest
    const float kR = smoothstepf(std::log(2000.f), std::log(1.e5f), G);   // the valley -> the region, more steeply down
    const float tanH = std::tan(lerpf(25.f, 27.f, kM) * PI / 180.f);
    float pitch = lerpf(-7.f, -20.f, kM);
    pitch = lerpf(pitch, -29.f, kR);
    pitch = lerpf(pitch, -60.f, kP);
    pitch *= PI / 180.f;
    const float F = lerpf(lerpf(1.05f, 0.82f, kM) * Rr, 0.95f * R_E, kP) * lerpf(1.f, 1.75f, kO);
    const vec3 anchor(0.f, lerpf(0.12f * Rr * (1.f - kR), -R_E, kP), 0.f);
    const float sx = lerpf(lerpf(0.64f, 0.56f, kM) - 0.06f * kR, 0.52f, kP) - 0.04f * kO;
    const float sy = lerpf(lerpf(-0.36f, -0.3f, kM) - 0.05f * kR, -0.42f, kP) + 0.3f * kO;
    const float az = CAM_AZ + wobble + spin * kO;   // once out in space, slowly around the planet
    const float yaw = std::atan(sx * tanH * aspect / std::cos(pitch));
    const float phi = az + yaw;
    vec3 fwd(-std::sin(phi) * std::cos(pitch), std::sin(pitch), -std::cos(phi) * std::cos(pitch));
    const vec3 right = normalize(cross(fwd, vec3(0, 1, 0)));
    const vec3 upv = cross(right, fwd);
    const vec3 dirA = normalize(fwd + right * (sx * tanH * aspect) + upv * (sy * tanH));
    const float D = F / tanH;
    CamPose c;
    c.eye = anchor - dirA * D;
    c.fwd = fwd;
    c.D = D;
    c.tanH = tanH;
    return c;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool MyceliumUniverse::init(int width, int height) {
    progSky_ = compileWorld(ugl::FULLSCREEN_VS, shaders::SKY_FS, "mycelium sky");
    progThread_ = compileWorld(shaders::THREAD_VS, shaders::THREAD_FS, "mycelium thread");
    progShroom_ = compileWorld(shaders::SHROOM_VS, shaders::SHROOM_FS, "mycelium shroom");
    progPuff_ = compileWorld(shaders::PUFF_VS, shaders::PUFF_FS, "mycelium puff");
    progGlow_ = compileWorld(shaders::GLOW_VS, shaders::GLOW_FS, "mycelium glow");
    progCloud_ = compileWorld(shaders::CLOUD_VS, shaders::CLOUD_FS, "mycelium cloud");
    progSea_ = compileWorld(ugl::FULLSCREEN_VS, shaders::SEA_FS, "mycelium sea");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "mycelium comp");
    progBright_ = compile(ugl::FULLSCREEN_VS, shaders::BRIGHT_FS, "mycelium bright");
    progBlur_ = compile(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "mycelium blur");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "mycelium sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "mycelium veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "mycelium cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "mycelium shape");
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "mycelium text");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "mycelium post");

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
    instanced(threadVao_, threadVbo_, 4, sizeof(float) * 16);
    instanced(puffVao_, puffVbo_, 4, sizeof(float) * 16);
    instanced(glowVao_, glowVbo_, 3, sizeof(float) * 12);
    instanced(cloudVao_, cloudVbo_, 2, sizeof(float) * 8);
    instanced(spriteVao_, spriteVbo_, 3, sizeof(SpriteInst));
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));
    // the mushrooms' lathe: (u around, v along the profile), instanced
    {
        std::vector<float> uv;
        std::vector<uint32_t> idx;
        for (int j = 0; j <= LATHE_V; j++)
            for (int i = 0; i <= LATHE_U; i++) {
                uv.push_back((float)i / LATHE_U);
                uv.push_back((float)j / LATHE_V);
            }
        for (int j = 0; j < LATHE_V; j++)
            for (int i = 0; i < LATHE_U; i++) {
                const uint32_t a = j * (LATHE_U + 1) + i, b = a + 1, c = a + LATHE_U + 1, d = c + 1;
                idx.insert(idx.end(), {a, b, d, a, d, c});
            }
        shroomIdx_ = (GLsizei)idx.size();
        glGenVertexArrays(1, &shroomVao_);
        glGenBuffers(1, &shroomVbo_);
        glGenBuffers(1, &shroomIbo_);
        glGenBuffers(1, &shroomInst_);
        glBindVertexArray(shroomVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shroomVbo_);
        glBufferData(GL_ARRAY_BUFFER, uv.size() * sizeof(float), uv.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, shroomInst_);
        for (int i = 0; i < 5; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 20, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shroomIbo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);
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

    buildFont(caps_, FONT_CAPS);
    buildFont(capsB_, FONT_CAPSB);
    buildFont(ital_, FONT_ITAL);
    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    buildFlora();
    buildClouds();
    return glGetError() == GL_NO_ERROR;
}

void MyceliumUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] mycelium font init failed\n");
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

void MyceliumUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    aspect_ = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    const int wh = std::max(180, std::min(720, (int)(height_ * 0.66f)));
    const int ww = std::max(320, (int)std::lround((float)wh * width_ / height_));
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
    bloomA_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

MyceliumUniverse::~MyceliumUniverse() {
    for (ugl::Target* t : {&worldT_, &bloomA_, &bloomB_, &sceneT_}) t->destroy();
    for (GLuint p : {progSky_, progThread_, progShroom_, progPuff_, progGlow_, progCloud_, progSea_, progComp_, progBright_,
                     progBlur_, progSprite_, progVeil_, progCell_, progShape_, progText_, progPost_})
        glDeleteProgram(p);
    for (GLuint v : {threadVao_, shroomVao_, puffVao_, glowVao_, cloudVao_, spriteVao_, cellVao_, shapeVao_, textVao_})
        glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, threadVbo_, shroomVbo_, shroomIbo_, shroomInst_, puffVbo_, glowVbo_, cloudVbo_, spriteVbo_,
                     cellVbo_, shapeVbo_, textVbo_})
        glDeleteBuffers(1, &b);
    for (Font* f : {&caps_, &capsB_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The world: the network, the mushrooms, the ground cover, the village, the clouds
// =====================================================================================================

void MyceliumUniverse::uploadNet() {
    net_ = buildMycNet(netSeed_);
    glBindBuffer(GL_ARRAY_BUFFER, threadVbo_);
    glBufferData(GL_ARRAY_BUFFER, net_.seg.size() * sizeof(float), net_.seg.data(), GL_STATIC_DRAW);
    threadCount_ = (GLsizei)(net_.seg.size() / 16);
    planMushrooms();
    netDirty_ = false;
}

// The mushrooms, once per game: one born every third of an e-fold of growth, at a site the threads reached a little
// before, at about half the network's reach, where the camera will see it in the open beside the well; each a third of
// the network's reach tall when it bursts up (capped at some 60 km: then glowing pins all over the globe), with two or
// three smaller ones at its foot.
void MyceliumUniverse::planMushrooms() {
    shrooms_.clear();
    Rng r(netSeed_ ^ 0x5A00ull);
    if (net_.sites.empty()) return;
    int n = 0;
    for (float Gi = myc::G_MIN + 0.9f; Gi < myc::G_MAX - 0.1f; Gi += r.range(0.26f, 0.38f), n++) {
        const float reach = std::exp(Gi);
        // the camera a moment after the birth: where it will look
        const CamPose cp = cameraFor(Gi + 0.5f, aspect_, 0.f);
        const vec3 right = normalize(cross(cp.fwd, vec3(0, 1, 0)));
        const vec3 upv = cross(right, cp.fwd);
        int best = -1;
        float bestS = -1e9f;
        for (int k = 0; k < 60; k++) {
            const int si = r.irange(0, (int)net_.sites.size() - 1);
            const MycNet::Site& s = net_.sites[si];
            if (s.g > Gi - 0.15f) continue;
            const float rho = std::sqrt(s.x * s.x + s.z * s.z);
            const float lr = std::log(std::max(rho, 1e-6f) / (reach * 0.7f));
            if (std::fabs(lr) > 1.2f) continue;
            const vec3 w = myc::toWorld(s.x, s.z) + myc::normalAt(s.x, s.z) * (reach * 0.12f);
            const vec3 d = w - cp.eye;
            const float z = dot(d, cp.fwd);
            if (z <= 0.f) continue;
            const float sx = dot(d, right) / (z * cp.tanH * aspect_), sy = dot(d, upv) / (z * cp.tanH);
            // in the open beside the well, on the screen, a little to the right preferred
            float score = -std::fabs(lr) * 0.6f;
            const float ax = std::fabs(sx);
            score -= ax < 0.36f ? 3.f : 0.f;
            score -= ax > 0.92f ? 2.f : 0.f;
            score -= sy > 0.75f || sy < -0.92f ? 2.f : 0.f;
            score += sx > 0.f ? 0.4f : 0.f;
            score += r.range(0.f, 0.5f);
            if (score > bestS) { bestS = score; best = si; }
        }
        if (best < 0) continue;
        const MycNet::Site& s = net_.sites[best];
        Mushroom m;
        m.x = s.x; m.z = s.z;
        m.g = Gi;
        m.h0 = std::min(reach * r.range(0.42f, 0.58f), 8.e4f);
        m.type = r.chance(0.45f) ? 0 : r.chance(0.55f) ? 1 : 2;
        m.capR = m.type == 0 ? r.range(0.36f, 0.46f) : m.type == 1 ? r.range(0.26f, 0.32f) : r.range(0.17f, 0.22f);
        m.color = r.irange(0, 2);
        m.seed = r.uniform();
        m.lean = r.range(0.02f, 0.12f);
        m.leanA = r.range(0.f, TAU);
        shrooms_.push_back(m);
        // a few smaller ones at its foot
        const int c = r.irange(1, 3);
        for (int i = 0; i < c; i++) {
            Mushroom q = m;
            const float a = r.range(0.f, TAU), d = m.h0 * r.range(0.2f, 0.6f);
            q.x += std::cos(a) * d;
            q.z += std::sin(a) * d;
            q.g = Gi + r.range(0.05f, 0.3f);
            q.h0 = m.h0 * r.range(0.22f, 0.5f);
            q.type = r.chance(0.6f) ? m.type : r.irange(0, 2);
            q.capR = q.type == 0 ? r.range(0.36f, 0.46f) : q.type == 1 ? r.range(0.26f, 0.32f) : r.range(0.17f, 0.22f);
            q.color = r.chance(0.6f) ? m.color : r.irange(0, 2);
            q.seed = r.uniform();
            q.lean = r.range(0.05f, 0.25f);
            q.leanA = std::atan2(q.z - m.z, q.x - m.x) + r.range(-0.4f, 0.4f);
            shrooms_.push_back(q);
        }
    }
}

// The height: it bursts up when born, then keeps growing (some 2.6 times taller over the next scales).
float MyceliumUniverse::shroomHeight(const Mushroom& m, float G) const {
    const float e = (G - m.g) / 0.45f;
    if (e < -0.3f) return 0.f;
    const float rise = smoothstepf(-0.3f, 0.35f, e);
    return m.h0 * rise * (1.f + 2.2f * smoothstepf(0.f, 3.5f, G - m.g));
}
float MyceliumUniverse::shroomOpen(const Mushroom& m, float G) const {
    const float o = smoothstepf(0.05f, 1.f, (G - m.g) / 0.45f);
    return o * (1.f + 0.18f * std::sin(o * PI));
}

// Ground cover at every scale, around the spore: moss sprigs with their capsules, moss cushions and pebbles, grass
// tufts, bushes, trees; each scale is shown while it is a sensible size on screen.
void MyceliumUniverse::buildFlora() {
    plants_.clear();
    Rng r(0xF10Aull);
    for (int k = 0; k <= 10; k++) {
        const float s = 0.0006f * std::pow(3.f, (float)k);
        const int n = k >= 9 ? 130 : 150;
        for (int i = 0; i < n; i++) {
            const float a = r.range(0.f, TAU);
            const float d = s * std::exp(r.range(std::log(k >= 9 ? 3.f : 1.8f), std::log(k >= 9 ? 30.f : 45.f)));
            Plant p;
            p.x = std::cos(a) * d;
            p.z = std::sin(a) * d;
            p.s = s * r.range(0.7f, 1.3f);
            p.seed = r.uniform();
            p.level = k;
            if (k <= 2) p.kind = 0;                                   // moss sprigs
            else if (k <= 4) p.kind = r.chance(0.8f) ? 1 : 2;         // moss cushions, pebbles
            else if (k <= 6) p.kind = r.chance(0.6f) ? 3 : r.chance(0.5f) ? 1 : 2;   // grass, moss, stones
            else if (k <= 8) p.kind = r.chance(0.55f) ? 4 : 3;        // bushes, tall grass and ferns
            else p.kind = 5;                                          // trees
            plants_.push_back(p);
        }
    }
    // the village: houses gathered in a clearing ahead of the spore, on the left of the view
    houses_.clear();
    const vec3 fwdH(-std::sin(CAM_AZ), 0.f, -std::cos(CAM_AZ)), rightH(std::cos(CAM_AZ), 0.f, -std::sin(CAM_AZ));
    const vec3 vc = fwdH * 350.f - rightH * 1400.f;
    villageW_ = vc;
    for (int i = 0; i < 40; i++) {
        const float a = r.range(0.f, TAU), d = 200.f * std::sqrt(r.uniform());
        House h;
        h.x = vc.x + std::cos(a) * d;
        h.z = vc.z + std::sin(a) * d;
        h.w = r.range(10.f, 16.f);
        h.h = h.w * r.range(0.55f, 0.75f);
        h.seed = r.uniform();
        h.kind = 0;
        houses_.push_back(h);
    }
}

void MyceliumUniverse::buildClouds() {
    Rng r(0xC10Dull);
    clouds_.clear();
    for (int c = 0; c < 170; c++) {
        const int band = c < 40 ? 0 : c < 120 ? 1 : 2;
        const float a = r.range(0, TAU);
        const float d = band == 0 ? r.range(600.f, 4000.f) : band == 1 ? r.range(4000.f, 18000.f) : r.range(18000.f, 50000.f);
        const float big = (band == 0 ? r.range(110.f, 240.f) : band == 1 ? r.range(200.f, 450.f) : r.range(450.f, 950.f));
        const vec3 ctr(std::cos(a) * d, CLOUD_Y + r.range(-250.f, 350.f), std::sin(a) * d);
        const int n = r.irange(5, 8);
        const float ca = r.range(0, TAU);
        const vec3 ax(std::cos(ca), 0.f, std::sin(ca)), az(-std::sin(ca), 0.f, std::cos(ca));
        for (int i = 0; i < n; i++) {
            const float u = (i - (n - 1) * 0.5f) / std::max(1.f, (n - 1) * 0.5f);
            Puff p;
            p.r = big * r.range(0.55f, 0.85f) * (1.f - 0.45f * std::fabs(u));
            p.p = ctr + ax * (u * big * 1.5f) + az * (r.range(-0.4f, 0.4f) * big) +
                  vec3(0.f, r.range(0.f, 0.5f) * big * (1.f - std::fabs(u)) + p.r * 0.3f, 0.f);
            p.seed = r.uniform();
            clouds_.push_back(p);
        }
    }
}

// =====================================================================================================
// Camera
// =====================================================================================================

void MyceliumUniverse::updateCamera(float dt) {
    if (gc_ < -50.f) { gc_ = g_; gcVel_ = 0.f; }
    {
        // a critically damped spring, slower than the growth's: the network spurts out, the camera follows
        const float w = 3.6f;
        gcVel_ += (w * w * (g_ - gc_) + 2.f * w * (0.85f * gVel_ - gcVel_)) * dt;
        gc_ += gcVel_ * dt;
    }
    const CamPose c = cameraFor(gc_, aspect_, 0.035f * std::sin(time_ * 0.021f), time_ * 0.03f);
    tanH_ = c.tanH;
    eye_ = c.eye;
    look_ = c.fwd;
    camD_ = c.D;
    camR_ = normalize(cross(look_, vec3(0, 1, 0)));
    camU_ = cross(camR_, look_);
    {
        const vec3 oc(eye_.x, eye_.y + R_E, eye_.z);
        alt_ = std::max(0.f, length(oc) - R_E);
        if (length(oc) < 1e7f) alt_ = std::max(0.f, eye_.y + (eye_.x * eye_.x + eye_.z * eye_.z) / (2.f * R_E));
    }
    // the moon: low over the forest from the ground, high beside the planet from space
    const float kP = smoothstepf(std::log(4.e5f), std::log(5.e6f), gc_);
    const vec3 fwdH(-std::sin(CAM_AZ), 0.f, -std::cos(CAM_AZ)), rightH(std::cos(CAM_AZ), 0.f, -std::sin(CAM_AZ));
    const vec3 mG = normalize(fwdH * 0.9f - rightH * 0.42f + vec3(0.f, 0.2f, 0.f));
    const vec3 mP = normalize(look_ + camU_ * 0.3f - camR_ * 0.66f);
    moonDir_ = normalize(lerp(mG, mP, kP));
    // the sun: deep under the spore's horizon, behind the planet seen from space (its day side a crescent)
    {
        const float kO = smoothstepf(std::log(5.e6f), std::log(1.55e7f), gc_);
        const CamPose P = cameraFor(myc::G_MAX, aspect_, 0.f, time_ * 0.03f * kO);
        const vec3 pr = normalize(cross(P.fwd, vec3(0, 1, 0))), pu = cross(pr, P.fwd);
        const vec3 toC = vec3(0.f, -R_E, 0.f) - P.eye;
        const vec3 dirC = normalize(toC);
        const float ang = std::asin(std::min(1.f, R_E / length(toC))) * 0.9f;
        sunDir_ = normalize(dirC * std::cos(ang) + normalize(pr * -0.75f + pu * 0.66f) * std::sin(ang));
    }
    const float zn = std::max(1e-6f, camD_ * 0.01f), zf = std::max({1.6e5f, camD_ * 1e4f});
    const mat4 view3 = mat4::lookAt(vec3(0.f), look_, vec3(0, 1, 0));
    proj3_ = mat4::perspective(2.f * std::atan(tanH_), aspect_, zn, zf);
    vp3_ = proj3_ * view3;
}

vec2 MyceliumUniverse::toBoard(const vec3& w) const {
    const vec4 c = vp3_ * vec4(w - eye_, 1.f);
    if (c.w <= 1e-12f) return V(viewC_.x + viewH_.x, viewC_.y);
    return V(viewC_.x + c.x / c.w * viewH_.x, viewC_.y + c.y / c.w * viewH_.y);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void MyceliumUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 1;            // the plum night for dark, minor songs
        else if (t.hasSong && !t.minor && t.brightness > 0.58f) v = 2;      // the amber for bright, major ones
        else if (t.hasSong && t.airWeight > 0.6f) v = 0;                    // ice blue for airy ones
        else {
            static const int BY_HUE[] = {3, 0, 2, 1, 3, 0};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x3C3Cull);
    if (netSeed_ == 0) { netSeed_ = seed_; netDirty_ = true; }
    if (seconds <= 0.f) palSet_ = false;
}

void MyceliumUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

void MyceliumUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.4f + 0.4f * strength);
}

void MyceliumUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.25f * strength); }

vec2 MyceliumUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints MyceliumUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.skyTop, p.mist, 0.5f);
    h.light = false;
    h.ink = p.ink;
    h.accent = p.thread;
    h.styles = TS_INK | TS_LUMA | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void MyceliumUniverse::syncIds(const Game& g) {
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

void MyceliumUniverse::addMote(const Mote& m) {
    if ((int)motes_.size() >= MAX_MOTES) return;
    motes_.push_back(m);
}

void MyceliumUniverse::onEvent(const GameEvent& ev, const Game& game) {
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
                m.size = rng_.range(0.07f, 0.11f);
                m.col = lerp(pal_.piece[std::clamp(c.type, 0, 6)], pal_.threadTip, 0.4f);
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
        // the cells fray into light that flies to the network's heart
        vec2 to = toBoard(vec3(0.f));
        to.x = std::clamp(to.x, viewC_.x - viewH_.x + 1.f, viewC_.x + viewH_.x - 1.f);
        to.y = std::clamp(to.y, viewC_.y - viewH_.y + 1.f, viewC_.y + viewH_.y - 1.f);
        const float side = to.x >= 0.f ? 1.f : -1.f;
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
                mt.to = add(to, V(rng_.range(-0.5f, 0.5f), rng_.range(-0.4f, 0.4f)));
                mt.ctl = V(side * rng_.range(5.5f, 7.5f), std::max(mt.from.y, mt.to.y) + rng_.range(1.f, 4.f));
                mt.p = mt.from;
                mt.life = FLOW_T * rng_.range(0.8f, 1.1f);
                mt.age = -rng_.range(0.f, 0.25f) - (float)(side > 0 ? Game::W - 1 - cl.x : cl.x) * 0.015f;
                mt.size = rng_.range(0.1f, 0.16f);
                mt.col = lerp(pal_.piece[std::clamp(cl.type, 0, 6)], pal_.threadTip, 0.5f);
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
        // a spurt, the heart pulsing its light out through every thread (as the motes arrive)
        spurt_ = std::max(spurt_, 0.4f + 0.15f * lines);
        if ((int)pulses_.size() < 6) {
            const float q0 = std::log(camD_ * 0.004f);
            const float dur = lines >= 4 ? 1.8f : 1.5f;
            pulses_.push_back({q0 - (g_ + 0.6f - q0) / dur * 0.9f, (g_ + 0.6f - q0) / dur, lines >= 4 ? 1.3f : 0.7f, 1.f, 0.45f});
        }
        if (lines >= 4) { tetrisGlow_ = 1.f; burst_ = 1.f; }
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

void MyceliumUniverse::cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, 0.f, 0.f}});
}

void MyceliumUniverse::buildBoard(const Game& g) {
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
    // game over: the stack's light drains from the top down
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
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void MyceliumUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param, float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

// =====================================================================================================
// Text
// =====================================================================================================

float MyceliumUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void MyceliumUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void MyceliumUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float halo) {
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
    set1f(progText_, "uGlowA", 0.4f * (1.f - b));
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD: Josefin Sans capitals, light and widely spaced, the numbers in its regular weight, the song and the
// notices in Yrsa italics.
void MyceliumUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
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
    const float halo = 0.6f;
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
            const float sz = base * 0.95f, tr = sz * 0.3f;
            const float wName = textWidth(caps_, name, sz, tr);
            std::string vt = var;
            for (size_t i = 1; i < vt.size(); i++) vt[i] = (char)std::tolower((unsigned char)vt[i]);
            const float vsz = base * 1.3f;
            const float wVar = vt.empty() ? 0.f : textWidth(ital_, vt, vsz, 0.f);
            const float gap = base * 0.7f;
            const float x = ugl::hudLeft(h, wName + (vt.empty() ? 0.f : gap + wVar), width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(caps_, name, x, h.y, sz, tr, q);
            drawText(caps_, q, ink, a, halo);
            if (!vt.empty()) {
                q.clear();
                textQuads(ital_, vt, x + wName + gap, h.y - sz * 0.2f, vsz, 0.f, q);
                drawText(ital_, q, accent, a * 0.9f, halo);
            }
            continue;
        }
        case HUD_SECTION:
            place(caps_, h.text, base * 0.85f, base * 0.55f);
            drawText(caps_, q, ink, saturate(0.2f + h.alpha * 1.9f), halo);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.55f, base * 0.02f, -base * 0.3f);
            drawText(ital_, q, ink, saturate(h.alpha * 1.3f), halo);
            continue;
        case HUD_INFO:
            place(caps_, h.text, base * 0.68f, base * 0.25f);
            drawText(caps_, q, ink, saturate(alpha * 1.3f) * 0.85f, halo);
            continue;
        case HUD_LABEL:
            place(caps_, h.text, base * 0.72f, base * 0.45f);
            drawText(caps_, q, lerp(ink, accent, 0.5f), saturate(alpha * 1.2f) * 0.92f, halo);
            continue;
        case HUD_VALUE:
            place(capsB_, h.text, base * 1.08f, base * 0.1f);
            drawText(capsB_, q, ink, alpha, halo);
            continue;
        case HUD_NOTICE:
            place(ital_, h.text, base * 1.45f, base * 0.02f);
            drawText(ital_, q, accent, alpha, halo);
            continue;
        case HUD_KEY:
            place(capsB_, h.text, base * 0.78f, base * 0.12f);
            drawText(capsB_, q, ink, alpha, halo);
            continue;
        case HUD_TOAST:
            place(capsB_, h.text, base * 1.2f, base * 0.5f, -base * 0.15f);
            drawText(capsB_, q, ink, alpha, 0.85f);
            continue;
        case HUD_TOAST_SUB:
            place(ital_, h.text, base * 1.3f, base * 0.02f);
            drawText(ital_, q, accent, alpha * 0.9f, 0.85f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(capsB_, h.text, base * 1.1f, base * 0.22f, -base * 0.1f);
                drawText(capsB_, q, ink, alpha, 0.85f);
            } else {
                place(caps_, h.text, base * 0.8f, base * 0.14f);
                drawText(caps_, q, h.accent ? accent : ink, alpha, 0.85f);
            }
            continue;
        default:
            place(caps_, h.text, base * 0.8f, base * 0.12f);
            drawText(caps_, q, h.accent ? accent : ink, alpha, halo);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MyceliumUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    if (!f.bare) syncIds(game); // (a break's blank board keeps the stack's ids)
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) { // (a break's blank board is never over)
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

    // ---- The game's lines -> the growth (a new game grows a new network from a new spore).
    {
        const int lines = f.bare ? lines_ : linesHere(game) + extraLines(time_); // (a break: the growth as it was)
        if (netSeed_ == 0) { netSeed_ = seed_ ? seed_ : 1; netDirty_ = true; }
        if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
            netSeed_ = splitmix64(netSeed_ + 0x9E37ull);
            netDirty_ = true;
            gBonus_ = 0.f;
            g_ = myc::growthOf(lines);
            gVel_ = 0.f;
            newT_ = 0.f;
            spores_.clear();
            pulses_.clear();
        }
        lines_ = lines;
        if (netDirty_) uploadNet();
        if (isDrop && !wasDrop_) gBonus_ += 0.25f; // the drop spreads it a little farther
        gTarget_ = std::min(myc::G_MAX, myc::growthOf(lines) + gBonus_);
        if (linesShown_ < 0) { g_ = gTarget_; gVel_ = 0.f; }
        linesShown_ = lines;
        // a spurt: a critically damped spring toward the target, over a second or so
        const float w = 4.f;
        gVel_ += (w * w * (gTarget_ - g_) - 2.f * w * gVel_) * wdt;
        // capped (a line is a big step now the journey fits in one song): an early Tetris spreads a breath longer
        // instead of jolting the view
        gVel_ = std::min(gVel_, 3.f);
        g_ += gVel_ * wdt;
        if (g_ > gTarget_ + 0.001f && gVel_ > 0.f) { g_ = gTarget_; gVel_ = 0.f; }
        newT_ += dt;
    }
    spurt_ = approach(spurt_, 0.f, 1.2f, dt);
    burst_ = approach(burst_, 0.f, 0.8f, dt);
    if (isDrop && !wasDrop_) {
        // one wave of light from the spore out through the whole network
        waveQ_ = std::log(camD_ * 0.003f);
        waveSpeed_ = (g_ + 0.8f - waveQ_) / 3.4f;
        waveAmp_ = 0.75f;
    }
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.5f : 0.4f, dt);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    if (waveAmp_ > 0.f) {
        waveQ_ += waveSpeed_ * wdt;
        if (waveQ_ > g_ + 1.f) waveAmp_ = approach(waveAmp_, 0.f, 0.8f, dt);
    }
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        if (slow_ > 0.01f) {
            // slow motion: a hush, the night cooler and stiller, the moon brighter
            const float k = slow_;
            pal_.moonC = lerp(pal_.moonC, pal_.moonC * vec3(0.85f, 0.95f, 1.25f), k);
            pal_.mist = lerp(pal_.mist, pal_.mist * vec3(0.8f, 0.9f, 1.25f), k);
            pal_.dusk = lerp(pal_.dusk, pal_.dusk * vec3(0.6f, 0.8f, 1.2f), k);
        }
    }
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    tetrisGlow_ = approach(tetrisGlow_, 0.f, 0.4f, dt);
    heart_ = approach(heart_, 0.f, 1.5f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    kickGlow_ = std::max(approach(kickGlow_, 0.f, 4.f, dt), kick * wp);
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    windPhase_ += wdt * (0.6f + 0.3f * wm + 0.4f * wp);

    // ---- The beat: pulses along the threads (mid: some of them every two beats; a peak: all of them, every beat).
    {
        bool beat = false;
        if (music.hasSong) {
            const double b = std::floor(music.beat);
            if (lastBeat_ < 0.0) lastBeat_ = b;
            if (b > lastBeat_) { lastBeat_ = b; beat = true; beatCount_++; }
        } else if (!f.paused) {
            beatClock_ += dt;
            if (beatClock_ > 0.5f) { beatClock_ -= 0.5f; beat = true; beatCount_++; }
        }
        if (beat && !f.paused && overT_ < 0.f && (int)pulses_.size() < 6) {
            const bool every = wp > 0.5f;
            if (wm > 0.4f && (every || beatCount_ % 2 == 0)) {
                const float q0 = std::log(camD_ * 0.01f);
                const float dur = every ? 1.6f : 2.2f;
                pulses_.push_back({q0, (g_ + 0.4f - q0) / dur, 0.35f + 0.25f * wp + 0.3f * kickGlow_, every ? 0.75f : 0.35f, 0.3f});
            }
        }
        for (Pulse& p : pulses_) p.q += p.speed * wdt;
        pulses_.erase(std::remove_if(pulses_.begin(), pulses_.end(), [&](const Pulse& p) { return p.q > g_ + 1.f; }), pulses_.end());
    }

    updateCamera(dt);
    const float G = g_;
    const float reach = std::exp(G);
    const float pxK = 2.f * tanH_ / worldT_.h;   // meters per pixel per meter of distance
    const vec3 fwdH(-std::sin(CAM_AZ), 0.f, -std::cos(CAM_AZ)), rightH(std::cos(CAM_AZ), 0.f, -std::sin(CAM_AZ));
    auto pxOf = [&](const vec3& p, float size) {
        const vec3 d = p - eye_;
        const float z = dot(d, look_);
        return z <= 0.f ? -1.f : size / (pxK * length(d));
    };
    auto inView = [&](const vec3& p, float rad) {
        const vec3 d = p - eye_;
        const float z = dot(d, look_);
        if (z < -rad) return false;
        const float zz = std::max(z, 1e-9f);
        const float sx = std::fabs(dot(d, camR_)) - rad, sy = std::fabs(dot(d, camU_)) - rad;
        return sx < zz * tanH_ * aspect_ * 1.1f && sy < zz * tanH_ * 1.1f;
    };

    // ---- The mushrooms: their state, the drop's wave reaching them, spores rising from their caps.
    const float capGlow = (0.62f + 0.12f * wm + 0.35f * wp + 0.25f * life + 0.3f * swell_ + 0.4f * tetrisGlow_ + 0.25f * kickGlow_) * pal_.glow;
    minst_.clear();
    struct Light { vec3 p; float r; vec3 c; float score; };
    std::vector<Light> lights;
    std::vector<vec4> capSpots;   // where spores rise from (cap center, size)
    for (Mushroom& m : shrooms_) {
        const float h = shroomHeight(m, G);
        if (h <= 0.f) { m.burst = false; continue; }
        const vec3 base = myc::toWorld(m.x, m.z, -0.03 * h);
        const vec3 up = myc::normalAt(m.x, m.z);
        // the drop's wave climbing it, the cap flaring, a cloud of spores
        const float mq = std::log(std::max(std::sqrt(m.x * m.x + m.z * m.z), 1e-6f));
        if (waveAmp_ > 0.4f && m.flareT > 3.f && waveQ_ > mq && waveQ_ < mq + 0.6f) m.flareT = 0.f;
        m.flareT += wdt;
        const float climb = m.flareT < 1.4f ? m.flareT / 0.9f : -1.f;
        m.flare = approach(m.flare, m.flareT > 0.7f && m.flareT < 2.f ? 1.f : 0.f, m.flareT < 2.f ? 4.f : 0.7f, dt);
        const vec3 capC = base + up * (h * 0.8f);
        const float px = pxOf(capC, h);
        if (!m.burst && G > m.g && G < m.g + 0.6f && px > 4.f && overT_ < 0.f) {
            // born: a burst of sparks as the cap opens
            m.burst = true;
            for (int i = 0; i < 14 && (int)spores_.size() < MAX_SPORES; i++) {
                Spore s;
                const vec3 dir = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(0.2f, 1.f), rng_.range(-1.f, 1.f)));
                s.p = capC;
                s.v = dir * (h * rng_.range(0.4f, 1.f));
                s.age = -rng_.range(0.f, 0.3f);
                s.life = rng_.range(0.8f, 1.6f);
                s.size = h * rng_.range(0.02f, 0.04f);
                s.seed = rng_.uniform();
                s.col = lerp(pal_.cap[m.color], vec3(1.f), 0.4f) * 1.4f;
                s.kind = 2;
                spores_.push_back(s);
            }
        }
        if (m.flareT >= 0.8f && m.flareT - wdt < 0.8f) {
            for (int i = 0; i < 40 && (int)spores_.size() < MAX_SPORES; i++) {
                Spore s;
                s.p = capC + vec3(rng_.range(-1.f, 1.f), rng_.range(0.f, 0.3f), rng_.range(-1.f, 1.f)) * (h * m.capR);
                s.v = (up * rng_.range(0.15f, 0.45f) + vec3(rng_.range(-0.1f, 0.1f), 0.f, rng_.range(-0.1f, 0.1f))) * h;
                s.age = -rng_.range(0.f, 0.5f);
                s.life = rng_.range(3.f, 6.f);
                s.size = h * rng_.range(0.012f, 0.025f);
                s.seed = rng_.uniform();
                s.col = lerp(pal_.cap[m.color], pal_.pulse, 0.3f);
                s.kind = 0;
                spores_.push_back(s);
            }
        }
        if (px < 1.2f || !inView(capC, h)) continue;
        if (px > 0.f) capSpots.push_back(vec4(capC.x, capC.y, capC.z, h));
        const float open = shroomOpen(m, G);
        const float wilt = smoothstepf(0.f, 1.f, over_);
        const float glow = capGlow * (1.f + 0.7f * m.flare) * (0.85f + 0.3f * m.seed);
        const vec3 lean(std::cos(m.leanA) * m.lean, 0.f, std::sin(m.leanA) * m.lean);
        const float v[20] = {base.x, base.y, base.z, h, up.x, up.y, up.z, m.capR,
                             pal_.cap[m.color].x, pal_.cap[m.color].y, pal_.cap[m.color].z, (float)m.type,
                             open, glow, wilt, m.seed, lean.x, lean.y, lean.z, climb};
        minst_.insert(minst_.end(), v, v + 20);
        if (open > 0.3f) {
            const vec3 lc = pal_.cap[m.color] * (glow * 0.55f * open * (1.f - 0.8f * wilt));
            lights.push_back({capC, h * m.capR * 1.3f, lc, px});
        }
    }
    std::sort(lights.begin(), lights.end(), [](const Light& a, const Light& b) { return a.score > b.score; });
    if (lights.size() > 6) lights.resize(6);

    // ---- Particles in the world: spores drifting up from the caps (from space: spore clouds drifting off).
    {
        const float rate = (2.f + 4.f * wm + 14.f * wp + 30.f * life + 50.f * burst_) * (overT_ >= 0.f ? 0.2f : 1.f);
        if (!f.paused) sporeAcc_ += rate * wdt;
        sporeAcc_ = std::min(sporeAcc_, 60.f);
        while (sporeAcc_ >= 1.f && !capSpots.empty() && (int)spores_.size() < MAX_SPORES) {
            sporeAcc_ -= 1.f;
            const vec4& c = capSpots[(size_t)rng_.irange(0, (int)capSpots.size() - 1)];
            const float h = c.w;
            const vec3 cp(c.x, c.y, c.z);
            const vec3 up = normalize(cp + vec3(0.f, R_E, 0.f));
            Spore s;
            s.p = cp + vec3(rng_.range(-0.3f, 0.3f), rng_.range(-0.05f, 0.1f), rng_.range(-0.3f, 0.3f)) * h;
            s.v = (up * rng_.range(0.05f, 0.15f) + vec3(rng_.range(-0.04f, 0.04f), 0.f, rng_.range(-0.04f, 0.04f))) * h;
            s.age = 0.f;
            s.life = rng_.range(4.f, 8.f);
            s.size = h * rng_.range(0.008f, 0.018f);
            s.seed = rng_.uniform();
            s.col = lerp(pal_.threadTip, pal_.cap[rng_.irange(0, 2)], 0.5f);
            s.kind = 0;
            spores_.push_back(s);
        }
        // from space: clouds of spores drifting off the glowing planet
        if (alt_ > 1.e5f && overT_ < 0.f && !f.paused) {
            const float k = smoothstepf(1.e5f, 1.e6f, alt_);
            spaceAcc_ += (28.f + 30.f * wp + 50.f * life) * k * wdt;
            while (spaceAcc_ >= 1.f && (int)spores_.size() < MAX_SPORES) {
                spaceAcc_ -= 1.f;
                const float a = rng_.range(0.f, TAU), d = std::min(reach, 1.9e7f) * std::sqrt(rng_.uniform());
                const float x = std::cos(a) * d, z = std::sin(a) * d;
                Spore s;
                s.p = myc::toWorld(x, z, camD_ * 0.005f);
                const vec3 up = myc::normalAt(x, z);
                s.v = up * (camD_ * rng_.range(0.006f, 0.02f)) + vec3(rng_.range(-1.f, 1.f), 0.f, rng_.range(-1.f, 1.f)) * (camD_ * 0.003f);
                s.age = 0.f;
                s.life = rng_.range(5.f, 9.f);
                s.size = camD_ * rng_.range(0.0012f, 0.003f);
                s.seed = rng_.uniform();
                s.col = lerp(pal_.threadTip, pal_.cap[rng_.irange(0, 2)], rng_.range(0.2f, 0.7f));
                s.kind = 0;
                spores_.push_back(s);
            }
        }
        // a Tetris: a burst of spores from the network's heart
        if (burst_ > 0.95f) {
            for (int i = 0; i < 60 && (int)spores_.size() < MAX_SPORES; i++) {
                Spore s;
                const float a = rng_.range(0.f, TAU), d = reach * std::sqrt(rng_.uniform()) * 0.7f;
                s.p = myc::toWorld(std::cos(a) * d, std::sin(a) * d, reach * 0.02f);
                const vec3 up = myc::normalAt(std::cos(a) * d, std::sin(a) * d);
                s.v = up * (reach * rng_.range(0.04f, 0.12f));
                s.age = -rng_.range(0.f, 0.6f);
                s.life = rng_.range(3.f, 5.f);
                s.size = reach * rng_.range(0.004f, 0.009f);
                s.seed = rng_.uniform();
                s.col = lerp(pal_.threadTip, pal_.pulse, rng_.uniform());
                s.kind = 0;
                spores_.push_back(s);
            }
            burst_ = 0.94f;
        }
        for (Spore& s : spores_) {
            s.age += wdt;
            const float fl = std::sin(time_ * 1.3f + s.seed * 40.f);
            s.p += (s.v + vec3(fl, 0.f, std::cos(time_ * 1.1f + s.seed * 20.f)) * (s.size * 0.6f)) * wdt;
            if (s.kind == 2) s.v = s.v * std::exp(-2.f * wdt);
        }
        spores_.erase(std::remove_if(spores_.begin(), spores_.end(), [](const Spore& s) { return s.age >= s.life; }), spores_.end());
    }

    // ---- 2D motes over the board.
    for (Mote& m : motes_) {
        m.age += dt;
        if (m.kind == 1) {
            const float t = smoothstepf(0.f, 1.f, saturate(m.age / m.life));
            const vec2 a = lerp2(m.from, m.ctl, t), b = lerp2(m.ctl, m.to, t);
            m.p = lerp2(a, b, t);
            if (m.age >= m.life && m.age - dt < m.life) heart_ = std::min(1.5f, heart_ + 0.08f);
        } else {
            m.p = add(m.p, mul(m.v, dt));
            if (m.kind == 3) m.v = mul(m.v, std::exp(-2.5f * dt));
        }
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.age >= m.life; }), motes_.end());
    for (auto& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    sprites_.clear();
    for (const Mote& m : motes_) {
        if (m.age < 0.f || f.bare) continue; // (the motes over the board: none in a break)
        const float t = m.age / m.life;
        float a = smoothstepf(0.f, 0.12f, t) * (1.f - smoothstepf(0.6f, 1.f, t)) * m.bright;
        if (m.kind == 1) a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.85f, 1.f, t)) * m.bright;
        sprite(m.p, m.size * (m.kind == 1 ? 2.f : 1.6f), m.col, a * world, 0);
    }
    if (!f.bare) {
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        sprite(V(gc.x, gc.y), layout::GAUGE_R * 1.25f, on ? pal_.threadTip : pal_.accent, 1.f, 1, fill, on ? 1.f : 0.f);
    }
    cells_.clear();
    if (!f.bare) buildBoard(game); // (a break: the world alone)

    // ---- Ground cover, houses, things in the air: built for the view.
    finst_.clear();
    ginst_.clear();
    auto glow = [&](const vec3& p, float size, const vec3& c, float a, int mode, float seed, float spin, float param = 0.f) {
        a *= world;
        if (a <= 0.004f) return;
        const float q[12] = {p.x, p.y, p.z, size, c.x, c.y, c.z, a, (float)mode, seed, spin, param};
        ginst_.insert(ginst_.end(), q, q + 12);
    };
    auto puff = [&](const vec3& c, float r, const vec3& col, float seed, const vec3& clump, float a, int kind, float under) {
        a *= world;
        if (a <= 0.004f) return;
        const float q[16] = {c.x, c.y, c.z, r, col.x, col.y, col.z, seed, clump.x, clump.y, clump.z, a, (float)kind, under, 0.f, 0.f};
        finst_.insert(finst_.end(), q, q + 16);
    };
    {
        const float H = (float)worldT_.h;
        for (const Plant& pl : plants_) {
            const float s = pl.s;
            const vec3 base = myc::toWorld(pl.x, pl.z);
            const float d = length(base - eye_);
            const float px = s / (pxK * std::max(d, 1e-9f));
            const float a = smoothstepf(2.5f, 6.f, px) * (1.f - smoothstepf(H * 0.3f, H * 0.6f, px)) * smoothstepf(0.6f * s, 1.6f * s, d) *
                            smoothstepf(0.55f * camD_, 0.95f * camD_, d) *
                            (pl.kind == 5 ? 1.f - smoothstepf(H * 0.1f, H * 0.2f, px) : pl.kind == 4 ? 1.f - smoothstepf(H * 0.14f, H * 0.26f, px) : 1.f);
            if (a <= 0.01f || !inView(base + vec3(0.f, s, 0.f), s * 2.f)) continue;
            const float rho = std::sqrt(pl.x * pl.x + pl.z * pl.z);
            const float under = std::exp(-sq(rho / (reach * 0.8f))) * 0.9f * (1.f - over_) * pal_.glow;
            Rng pr((uint64_t)(pl.seed * 1e9) + 7);
            const float sw = std::sin(windPhase_ * 1.3f + pl.seed * 30.f) * (0.06f + 0.1f * wp);
            switch (pl.kind) {
            case 0: {   // a moss sprig: a little clump, its capsules on their setae
                const vec3 cl = base + vec3(0.f, s * 0.15f, 0.f);
                for (int i = 0; i < 5; i++) {
                    const vec3 o(pr.range(-0.35f, 0.35f) * s, pr.range(0.05f, 0.3f) * s, pr.range(-0.35f, 0.35f) * s);
                    puff(base + o, s * pr.range(0.16f, 0.26f), pal_.floraLit * pr.range(0.8f, 1.2f), pr.uniform(), cl, a, 1, under);
                }
                if (pr.chance(0.35f)) {
                    const vec3 o(pr.range(-0.3f, 0.3f) * s, 0.f, pr.range(-0.3f, 0.3f) * s);
                    glow(base + o, s * pr.range(0.5f, 0.8f), lerp(pal_.litter, pal_.floraLit, 0.5f) * 0.6f, a * 0.7f, 10, pr.uniform(), 0.04f, 1.f);
                }
                break;
            }
            case 1: {   // a moss cushion
                const vec3 cl = base - vec3(0.f, s * 0.2f, 0.f);
                const int n = pr.irange(5, 9);
                for (int i = 0; i < n; i++) {
                    const float aa = pr.range(0.f, TAU), dd = pr.range(0.f, 0.45f) * s;
                    const vec3 o(std::cos(aa) * dd, pr.range(0.05f, 0.25f) * s, std::sin(aa) * dd);
                    puff(base + o, s * pr.range(0.22f, 0.36f), pal_.floraLit * pr.range(0.85f, 1.2f), pr.uniform(), cl, a, 1, under);
                }
                break;
            }
            case 2:     // a pebble
                puff(base + vec3(0.f, s * 0.12f, 0.f), s * pr.range(0.25f, 0.4f), pal_.rock * pr.range(0.8f, 1.2f), pr.uniform(),
                     base - vec3(0.f, s, 0.f), a, 2, under);
                break;
            case 3:     // a grass tuft
                glow(base, s * 0.6f, lerp(pal_.flora, pal_.floraLit, 0.5f) * 0.9f, a * (1.f - smoothstepf(H * 0.12f, H * 0.25f, px)), 3, pl.seed, sw);
                break;
            case 4: {   // a bush
                const vec3 cl = base;
                const int n = pr.irange(5, 8);
                for (int i = 0; i < n; i++) {
                    const float aa = pr.range(0.f, TAU), dd = pr.range(0.f, 0.4f) * s;
                    const vec3 o(std::cos(aa) * dd, pr.range(0.2f, 0.6f) * s, std::sin(aa) * dd);
                    puff(base + o, s * pr.range(0.2f, 0.32f), pal_.flora * pr.range(0.8f, 1.15f), pr.uniform(), cl, a, 0, under);
                }
                break;
            }
            default: {  // a tree: a trunk, a round crown of puffs
                const float th = s * pr.range(1.1f, 1.6f);
                glow(base, th * 0.5f, pal_.bark, a, 10, pl.seed, 0.09f, 0.f);
                const vec3 cc = base + vec3(0.f, th * 0.95f, 0.f);
                const int n = pr.irange(6, 10);
                for (int i = 0; i < n; i++) {
                    const vec3 o(pr.range(-0.35f, 0.35f) * s, pr.range(-0.25f, 0.3f) * s, pr.range(-0.35f, 0.35f) * s);
                    puff(cc + o, s * pr.range(0.2f, 0.3f), pal_.flora * pr.range(0.75f, 1.1f), pr.uniform(), cc - vec3(0.f, s * 0.2f, 0.f), a, 0, under * 0.6f);
                }
                break;
            }
            }
        }
        // the village
        for (const House& h : houses_) {
            const vec3 p = myc::toWorld(h.x, h.z);
            const float px = h.w / (pxK * std::max(length(p - eye_), 1e-9f));
            const float a = smoothstepf(1.5f, 4.f, px) * (1.f - smoothstepf(120.f, 300.f, px));
            if (a > 0.01f && inView(p, h.w)) glow(p, h.h, lerp(pal_.bark, pal_.rock, 0.5f) * 0.8f, a * world, 11, h.seed, 0.f);
        }
        // hot-air balloons over the valley, glowing from their burners
        if (alt_ < 4.e4f) {
            static const unsigned BAL[6] = {0xff3b4a, 0xffc61a, 0x2a8cff, 0xa64cff, 0xff7a1a, 0x18c8a8};
            Rng br(0xBA11ull);
            for (int i = 0; i < 9; i++) {
                const float f = br.range(600.f, 4500.f), rr = br.range(-3500.f, 800.f);
                const float hh = br.range(250.f, 1400.f);
                const vec3 q = fwdH * f + rightH * rr;
                const vec3 p = myc::toWorld(q.x + time_ * 1.2f, q.z, hh + std::sin(time_ * 0.3f + i) * 5.f);
                glow(p, br.range(11.f, 17.f), ugl::toLinear(hex(BAL[i % 6])), world, 7, br.uniform(), time_ + i * 1.7f);
            }
        }
        // airliners at cruising height, their lights blinking
        if (alt_ > 300.f && alt_ < 3.e5f) {
            Rng ar(0xA1B2ull);
            for (int i = 0; i < 4; i++) {
                const float a = ar.range(0.f, TAU), d = ar.range(3000.f, 25000.f), dir = ar.range(0.f, TAU);
                const float hh = ar.range(9000.f, 11500.f);
                const float u = std::fmod(time_ * 240.f + ar.range(0.f, 6e4f), 6e4f) - 3e4f;
                const vec3 p = myc::toWorld(std::cos(a) * d + std::cos(dir) * u, std::sin(a) * d + std::sin(dir) * u, hh);
                glow(p, 30.f, pal_.cloudShade, world, 12, ar.uniform(), time_ + i);
            }
        }
        // satellites in orbit
        if (alt_ > 5.e4f) {
            Rng sr(0x5A7Eull);
            for (int i = 0; i < 7; i++) {
                const float h = sr.range(3.5e5f, 1.4e6f);
                const float tilt = sr.range(-0.5f, 0.5f), az = sr.range(0.f, TAU);
                const vec3 u = normalize(vec3(std::sin(tilt) * std::cos(az), std::cos(tilt), std::sin(tilt) * std::sin(az)));
                const vec3 v = normalize(cross(u, vec3(std::cos(az + 1.f), 0.f, std::sin(az + 1.f))));
                const float th = sr.range(-0.4f, 0.4f) + time_ * sr.range(0.004f, 0.012f) * (sr.chance(0.5f) ? 1.f : -1.f);
                const float rr = R_E + h;
                const vec3 p = vec3(0.f, -R_E, 0.f) + (u * std::cos(th) + v * std::sin(th)) * rr;
                glow(p, 2200.f * std::sqrt(h / 4.e5f), vec3(1.f), world * smoothstepf(5.e4f, 2.e5f, alt_), 6, sr.uniform(), time_ + i);
            }
        }
        // the spore: the network's heart, glowing
        {
            const float hs = std::max(0.00025f, camD_ * 0.004f);
            const float hb = (0.55f + 0.5f * heart_ + 0.3f * spurt_) * (1.f - 0.8f * over_) * std::exp(-std::max(0.f, G - 6.f) * 0.25f);
            glow(vec3(0.f, 0.00012f, 0.f), hs, lerp(pal_.threadTip, vec3(1.f), 0.3f), hb * world, 0, 0.f, 0.f);
            if (heart_ > 0.05f) glow(vec3(0.f, 0.00012f, 0.f), hs * 2.5f, pal_.threadTip, heart_ * 0.5f * world, 9, 0.f, 0.f);
        }
        for (const Spore& s : spores_) {
            if (s.age < 0.f) continue;
            const float t = s.age / s.life;
            const float a = smoothstepf(0.f, 0.1f, t) * (1.f - smoothstepf(0.6f, 1.f, t)) * (0.75f + 0.25f * std::sin(time_ * 3.f + s.seed * 40.f));
            glow(s.p, s.size, s.col, a * world, s.kind == 2 ? 2 : 0, s.seed, 0.f);
        }
    }

    // =========================================================== the world (reduced resolution)
    const float mistK = (1.f / (lerpf(14.f, 40.f, smoothstepf(1.f, 300.f, camD_)) * camD_)) * (1.f - smoothstepf(1000.f, 20000.f, camD_));
    auto setLight = [&](GLuint p) {
        set3f(p, "uEye", eye_);
        set3f(p, "uMoonDir", moonDir_);
        set3f(p, "uMoonC", pal_.moonC * (0.55f + 0.05f * wp));
        set3f(p, "uAmbC", pal_.ambient);
        set3f(p, "uMistC", pal_.mist);
        set3f(p, "uNetC", pal_.thread);
        set1f(p, "uFogK", 2.e-5f);
        set1f(p, "uMistK", mistK);
        set1f(p, "uAlt", alt_);
        set1f(p, "uTime", time_);
        set1f(p, "uBridge", bridge);
        set3f(p, "uBridgeCol", bridge_.color);
    };
    worldT_.bind();
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    // the sky, the planet, space (it writes the planet's depth)
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
    set1f(progSky_, "uPix", pxK);
    set1f(progSky_, "uStars", pal_.stars);
    set1f(progSky_, "uHills", 1.f - smoothstepf(30.f, 1500.f, alt_));
    set1f(progSky_, "uGlow", 0.3f * wp + 0.5f * life);
    set1f(progSky_, "uReach", reach);
    set1f(progSky_, "uPoolK", pal_.pool * (0.22f + 0.1f * wp + 0.15f * life + 0.25f * waveAmp_ * 0.5f + 0.15f * heart_) * (1.f - 0.85f * over_));
    set1f(progSky_, "uOver", over_);
    set3f(progSky_, "uSunDir", sunDir_);
    set3f(progSky_, "uMoonLit", normalize(look_ * -1.f + camR_ * 0.7f + camU_ * 0.35f));
    set3f(progSky_, "uSkyTop", pal_.skyTop);
    set3f(progSky_, "uSkyLow", pal_.skyLow);
    set3f(progSky_, "uDusk", pal_.dusk);
    set3f(progSky_, "uSpaceC", pal_.spaceC);
    set3f(progSky_, "uNebulaC", pal_.nebula);
    set3f(progSky_, "uMoss", pal_.moss);
    set3f(progSky_, "uSoil", pal_.soil);
    set3f(progSky_, "uLitter", pal_.litter);
    set3f(progSky_, "uField", pal_.field);
    set3f(progSky_, "uForest", pal_.forest);
    set3f(progSky_, "uLand", pal_.land);
    set3f(progSky_, "uOcean", pal_.ocean);
    set3f(progSky_, "uCity", pal_.city);
    set3f(progSky_, "uPoolC", pal_.thread);
    {
        const vec3 mc = moonDir_ * 3.5e8f;
        set4f(progSky_, "uMoon", mc.x, mc.y, mc.z, 2.3e7f);
        set4f(progSky_, "uVillage", villageW_.x, villageW_.y, villageW_.z, 230.f);
        float lp[24] = {}, lc[24] = {};
        for (size_t i = 0; i < lights.size(); i++) {
            lp[i * 4 + 0] = lights[i].p.x; lp[i * 4 + 1] = lights[i].p.y; lp[i * 4 + 2] = lights[i].p.z; lp[i * 4 + 3] = lights[i].r;
            lc[i * 4 + 0] = lights[i].c.x * world; lc[i * 4 + 1] = lights[i].c.y * world; lc[i * 4 + 2] = lights[i].c.z * world;
        }
        glUniform4fv(glGetUniformLocation(progSky_, "uLightP"), 6, lp);
        glUniform4fv(glGetUniformLocation(progSky_, "uLightC"), 6, lc);
    }
    ugl::drawFullscreen();

    glDepthFunc(GL_LESS);
    // the mushrooms
    if (!minst_.empty()) {
        glUseProgram(progShroom_);
        setLight(progShroom_);
        setMat(progShroom_, "uVP", vp3_);
        set3f(progShroom_, "uStemC", pal_.stem);
        set1f(progShroom_, "uCapGlow", world);
        glBindVertexArray(shroomVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shroomInst_);
        glBufferData(GL_ARRAY_BUFFER, minst_.size() * sizeof(float), minst_.data(), GL_STREAM_DRAW);
        glDrawElementsInstanced(GL_TRIANGLES, shroomIdx_, GL_UNSIGNED_INT, nullptr, (GLsizei)(minst_.size() / 20));
    }
    // the ground cover's puffs
    if (!finst_.empty()) {
        glUseProgram(progPuff_);
        setLight(progPuff_);
        setMat(progPuff_, "uVP", vp3_);
        set3f(progPuff_, "uCamR", camR_);
        set3f(progPuff_, "uCamU", camU_);
        set1f(progPuff_, "uPx", pxK);
        set3f(progPuff_, "uRimC", pal_.thread * 0.6f);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(puffVao_);
        glBindBuffer(GL_ARRAY_BUFFER, puffVbo_);
        glBufferData(GL_ARRAY_BUFFER, finst_.size() * sizeof(float), finst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(finst_.size() / 16));
        glDisable(GL_BLEND);
    }

    // ---- Transparent: the clouds (far to near), the sea of clouds, the threads, then the lights in the air.
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    if (alt_ < 2.5e5f) {
        std::vector<std::pair<float, int>> order;
        order.reserve(clouds_.size());
        const float drift = time_ * (5.f + 6.f * wm + 6.f * wp);
        for (size_t i = 0; i < clouds_.size(); i++) {
            const vec3 p = clouds_[i].p + vec3(drift, 0.f, 0.f);
            const vec3 d = p - eye_;
            const float d2 = dot(d, d);
            if (clouds_[i].r < 0.6f * pxK * std::sqrt(d2)) continue;
            order.push_back({d2, (int)i});
        }
        std::sort(order.begin(), order.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
        cinst_.clear();
        for (auto& o : order) {
            const Puff& pf = clouds_[o.second];
            const vec3 p = pf.p + vec3(drift, 0.f, 0.f);
            const float dEye = std::sqrt(o.first);
            const float a = smoothstepf(pf.r * 0.8f, pf.r * 2.5f, dEye) * 0.9f;
            const float rho = std::sqrt(p.x * p.x + p.z * p.z);
            const float under = std::exp(-sq(rho / (reach * 0.8f))) * (1.f - over_);
            const float q[8] = {p.x, p.y, p.z, pf.r, pf.seed, a, under, 0.f};
            cinst_.insert(cinst_.end(), q, q + 8);
        }
        if (!cinst_.empty()) {
            glUseProgram(progCloud_);
            setLight(progCloud_);
            setMat(progCloud_, "uVP", vp3_);
            set3f(progCloud_, "uCamR", camR_);
            set3f(progCloud_, "uCamU", camU_);
            set3f(progCloud_, "uCloudLit", pal_.cloudLit);
            set3f(progCloud_, "uCloudShade", pal_.cloudShade);
            glBindVertexArray(cloudVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cloudVbo_);
            glBufferData(GL_ARRAY_BUFFER, cinst_.size() * sizeof(float), cinst_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(cinst_.size() / 8));
        }
    }
    if (eye_.y > CLOUD_Y + 200.f) {
        glUseProgram(progSea_);
        setLight(progSea_);
        setMat(progSea_, "uVP", vp3_);
        set3f(progSea_, "uLook", look_);
        set3f(progSea_, "uCamR", camR_);
        set3f(progSea_, "uCamU", camU_);
        set3f(progSea_, "uSunDir", sunDir_);
        set2f(progSea_, "uTan", tanH_ * aspect_, tanH_);
        set1f(progSea_, "uPix", pxK);
        set1f(progSea_, "uCloudY", CLOUD_Y);
        set1f(progSea_, "uDrift", time_ * (8.f + 10.f * wm + 10.f * wp));
        set1f(progSea_, "uFade", 0.6f * smoothstepf(CLOUD_Y + 200.f, CLOUD_Y + 1800.f, eye_.y));
        set3f(progSea_, "uCloudLit", pal_.cloudLit);
        set3f(progSea_, "uCloudShade", pal_.cloudShade);
        ugl::drawFullscreen();
    }
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    // the threads (additive)
    if (threadCount_ > 0) {
        float pu[24] = {};
        for (size_t i = 0; i < pulses_.size() && i < 6; i++) {
            pu[i * 4 + 0] = pulses_[i].q; pu[i * 4 + 1] = pulses_[i].amp; pu[i * 4 + 2] = pulses_[i].width; pu[i * 4 + 3] = pulses_[i].frac;
        }
        glUseProgram(progThread_);
        setLight(progThread_);
        setMat(progThread_, "uVP", vp3_);
        set3f(progThread_, "uCamR", camR_);
        set3f(progThread_, "uCamU", camU_);
        set1f(progThread_, "uG", G);
        set1f(progThread_, "uPx", pxK);
        set1f(progThread_, "uNear", camD_);
        set3f(progThread_, "uThreadC", pal_.thread);
        set3f(progThread_, "uTipC", pal_.threadTip);
        set3f(progThread_, "uPulseC", pal_.pulse);
        set1f(progThread_, "uBright", lerpf(1.f, 0.8f, smoothstepf(2.e4f, 2.e6f, alt_)) * (0.6f + 0.1f * wm + 0.25f * wp + 0.2f * life + 0.25f * swell_ + 0.3f * tetrisGlow_ + 0.2f * heart_) * pal_.glow * world);
        set1f(progThread_, "uOver", smoothstepf(0.f, 1.f, over_));
        glUniform4fv(glGetUniformLocation(progThread_, "uPulse"), 6, pu);
        set4f(progThread_, "uWave", waveQ_, waveAmp_, 0.45f, 0.f);
        glBlendFunc(GL_ONE, GL_ONE);
        glBindVertexArray(threadVao_);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, threadCount_);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (!ginst_.empty()) {
        glUseProgram(progGlow_);
        setLight(progGlow_);
        setMat(progGlow_, "uVP", vp3_);
        set3f(progGlow_, "uCamR", camR_);
        set3f(progGlow_, "uCamU", camU_);
        set1f(progGlow_, "uPx", pxK);
        glBindVertexArray(glowVao_);
        glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
        glBufferData(GL_ARRAY_BUFFER, ginst_.size() * sizeof(float), ginst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(ginst_.size() / 12));
    }
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // ---- The bloom (quarter resolution).
    bloomA_.bind();
    glUseProgram(progBright_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progBright_, "uSrc", 0);
    set2f(progBright_, "uTexel", 1.f / worldT_.w, 1.f / worldT_.h);
    set1f(progBright_, "uThreshold", 0.3f);
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
    set1f(progComp_, "uBloomK", pal_.bloom * (1.f + 0.25f * wp + 0.3f * life + 0.4f * tetrisGlow_) * world);
    set1f(progComp_, "uTwinkle", 0.25f + 0.15f * wm + 0.2f * wp);
    set1f(progComp_, "uTime", time_);
    set1f(progComp_, "uSat", 1.08f);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    // ---- The veil behind the well and the HUD's columns (none in a break).
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(pal_.thread * 0.45f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", 0.8f);
        set1f(progVeil_, "uColA", 0.5f * world);
        set1f(progVeil_, "uRailA", 0.5f);
        set1f(progVeil_, "uPx", pxPerUnit_);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.f, layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }

    // ---- Pieces, previews, the motes over them, the HUD.
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uTime", time_);
        set3f(progCell_, "uLightC", lerp(pal_.threadTip, vec3(1.f), 0.4f));
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
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_mycelium() {
    static const UniverseEntry e{
        "MYCELIUM", "a glowing fungal network growing over the forest floor at night, from a single spore among the moss until it wraps the Earth in light",
        [] { return std::unique_ptr<Universe>(new MyceliumUniverse()); },
        // Fits most songs; darker, denser ones a little better.
        [](const SongLook& l) { return 0.85f + 0.2f * (1.f - l.brightness) + 0.1f * l.density; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
