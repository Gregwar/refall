// MYCELIUM: a glowing fungal network spreads through a cross-section of the forest floor at night. The soil is cut
// along the front of the scene (its horizons, stones and roots lit only by the threads' light) and the forest stands
// behind the cut: the floor of moss receding into the mist, plants, trees and a far tree line under the night sky.
//
// It starts as a single spore just under the surface, a millimeter across, seen as close as a macro lens would:
// the soil's grains are pebbles, blurred blades of grass tower behind. Every cleared line pushes the threads deeper
// and wider (the network is grown in log space: network.cpp), and the camera pulls back with it, so the scale keeps
// changing: grass roots, then a fern's, then a tree's root plates go by, until the network is a kilometer of glowing
// cords. Mushrooms burst up through the surface where the threads reach it, each bigger than the last: a pin first,
// then caps among the moss, the ferns, the bushes, until parasol towers stand taller than the trees, their caps
// lighting the canopy around them. The growth is G = ln(the network's reach in meters), from a millimeter at the
// first line to some 50 m at 48 lines (the mushrooms as tall as the trees) and 1 km at 84 lines; a cabin, pylons, a church, wind turbines, skyscrapers and airliners stand among the mushrooms for scale.
//
// The music: calm is a still network breathing faintly; mid sends pulses of light out along some threads every two
// beats; a peak along all of them, caps glowing brighter and spores drifting up. **The drop** (a real one: the HUD's
// section, else the music's pace) sends one wave of light from the spore out through the whole network, up the
// stems of every mushroom as it reaches them, the caps flaring, and a cloud of spores floating up into the night.
// Each clear is a spurt: the threads shoot ahead with bright tips, the camera following a moment later.
//
// The pieces are mycelium felt: deep translucent shapes woven of glowing threads. Cleared rows fray into light that
// flies down into the soil to the network.
//
// Cost: the world at a reduced resolution (at most 720 lines): one full-screen pass (sky, tree line, floor, soil with
// stones at every scale), ~36k thread quads, ~30k root quads (most culled in the vertex shader as sub-pixel), a few
// hundred plant and mushroom quads, a quarter-resolution light pass and bloom.
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

GLuint compile(const char* vs, const char* fs, const char* name, bool lights = false) {
    const std::string pre = std::string(shaders::COMMON_GLSL) + (lights ? shaders::LIGHTS_GLSL : "");
    std::string f(fs);
    f.insert(f.find('\n') + 1, pre);
    std::string v(vs);
    v.insert(v.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 lerp2(vec2 a, vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): the night (sky top, sky low, mist, moon), the floor (moss, its light), the soil (litter, humus,
// topsoil, subsoil), rock, roots, the ambient light; the threads (body, tips, pulses), the caps and gills (three
// mushroom colors), the stems, foliage, bark, the floor's specks; the HUD's ink and accent, the veil; the seven
// pieces; the glow, the threads' light on the soil, the mist's share of the caps' light, the stars.
struct VariantDef {
    const char* name;
    unsigned skyTop, skyLow, mist, moon, floorC, floorLit, soilA, soilB, soilC, soilD, rock, rootC, ambient;
    unsigned thread, threadTip, pulse, cap[3], gill[3], stem, foliage, bark, speck, ink, accent, veil;
    unsigned piece[7];
    float glow, soilLight, mistK, stars;
};
const VariantDef VARIANTS[] = {
    // Ice-blue threads and turquoise caps under a deep blue night.
    {"GLACIER", 0x040a1c, 0x112848, 0x14304a, 0xdce8ff, 0x1a2a2c, 0xffffff, 0x2a2026, 0x3e2e26, 0x55402e, 0x6e5640,
     0x6c7684, 0x7a5a44, 0x3e4a64,
     0x2ad4ff, 0xa0fff6, 0xe4ffff, {0x34e4ff, 0x6a86ff, 0x6affc4}, {0xb4fff4, 0xc4d0ff, 0xd0ffe6}, 0xb8ccd6, 0x14302e,
     0x181a22, 0x5aeaff, 0xe6f4ff, 0x7ae6ff, 0x030812,
     {0x4ee6ff, 0xffd84a, 0xb88cff, 0x6ef0a0, 0xff6e86, 0x5c8cff, 0xffa04a}, 1.f, 1.f, 0.07f, 1.f},
    // Magenta threads, pink and violet caps in a plum night.
    {"ORCHID", 0x0b0418, 0x26143c, 0x321a44, 0xffe6f2, 0x20172a, 0xffffff, 0x2e1e28, 0x44302e, 0x5a3e34, 0x745446,
     0x7a7084, 0x7c5446, 0x4a3e62,
     0xff52d4, 0xffb4ee, 0xfff0fc, {0xff48b4, 0xa862ff, 0xff8466}, {0xffc4ea, 0xdcbcff, 0xffd4c4}, 0xd8c2d2, 0x281a32,
     0x1c1422, 0xff86e4, 0xfff0fa, 0xff98de, 0x0c0512,
     {0x5ee0ff, 0xffe066, 0xd884ff, 0x7aef9a, 0xff6a8a, 0x6a8aff, 0xffa860}, 1.f, 1.f, 0.07f, 1.f},
    // Amber threads and honey caps in a teal night.
    {"EMBER", 0x020e18, 0x0c2a36, 0x123238, 0xfff0d6, 0x1a241e, 0xffffff, 0x2a2018, 0x40301f, 0x5a4229, 0x765c3c,
     0x767670, 0x7e5a38, 0x3a5258,
     0xffa02a, 0xffdc84, 0xfff4da, {0xff9426, 0xffcc44, 0xff5836}, {0xffdc9c, 0xffeeac, 0xffac8c}, 0xe2ceb2, 0x122e28,
     0x181816, 0xffc45c, 0xfff4e4, 0xffc66e, 0x030e10,
     {0x4ee8e0, 0xffe04a, 0xc890ff, 0x8af06a, 0xff6a5a, 0x6a9cff, 0xffa040}, 1.f, 1.f, 0.07f, 1.f},
    // Green foxfire: lime threads, emerald and chartreuse caps.
    {"JADE", 0x020b0f, 0x0b2828, 0x10362e, 0xecffee, 0x17241b, 0xffffff, 0x262218, 0x3a2e20, 0x4e3c28, 0x66543e,
     0x6c7870, 0x6c5638, 0x3a5246,
     0x74ff54, 0xd4ffa4, 0xf2fff0, {0x94ff36, 0x36ffac, 0xe4ff56}, {0xdcffbc, 0xbcffec, 0xf6ffc4}, 0xccdcc4, 0x13281b,
     0x161b17, 0xa4ff66, 0xf0fff0, 0xb4ff86, 0x020a06,
     {0x4ee0ff, 0xffd84a, 0xc08cff, 0x6af0b0, 0xff6e8a, 0x6a8cff, 0xffa24a}, 1.f, 1.f, 0.07f, 1.f},
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
    p.skyTop = L(d.skyTop); p.skyLow = L(d.skyLow); p.mist = L(d.mist); p.moon = L(d.moon); p.floorC = L(d.floorC);
    p.floorLit = L(d.floorLit); p.soilA = L(d.soilA); p.soilB = L(d.soilB); p.soilC = L(d.soilC); p.soilD = L(d.soilD);
    p.rock = L(d.rock); p.rootC = L(d.rootC); p.ambient = L(d.ambient);
    p.thread = L(d.thread); p.threadTip = L(d.threadTip); p.pulse = L(d.pulse);
    for (int i = 0; i < 3; i++) { p.cap[i] = L(d.cap[i]); p.gill[i] = L(d.gill[i]); }
    p.stem = L(d.stem); p.foliage = L(d.foliage); p.bark = L(d.bark); p.speck = L(d.speck);
    p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.glow = d.glow; p.soilLight = d.soilLight; p.mistK = d.mistK; p.stars = d.stars;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

constexpr float CLEAR_T = 0.36f;     // a cleared row frays into light in this long (real time)
constexpr float FLOW_T = 1.2f;       // its light flies down to the network in this long
constexpr int MAX_MOTES = 1600;
constexpr int MAX_SPORES = 2600;
enum BoardType { B_MUSHROOM = 0, B_TREE, B_GRASS, B_FERN, B_ROCK, B_BUSH, B_PINS, B_MOSS };

// Test hooks: ZEN_MYCELIUM_LINES=N adds N lines to the growth (to look at a late stage in a short shot); ZEN_GROW_RATE=R adds R more lines a second (a time-lapse).
int extraLines(double t) {
    static const int n = std::getenv("ZEN_MYCELIUM_LINES") ? std::atoi(std::getenv("ZEN_MYCELIUM_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return std::max(0, n) + (int)(rate * t);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool MyceliumUniverse::init(int width, int height) {
    progBg_ = compile(ugl::FULLSCREEN_VS, shaders::BG_FS, "mycelium bg", true);
    progThread_ = compile(shaders::THREAD_VS, shaders::THREAD_FS, "mycelium thread");
    progNode_ = compile(shaders::NODE_VS, shaders::NODE_FS, "mycelium node");
    progRoot_ = compile(shaders::ROOT_VS, shaders::ROOT_FS, "mycelium root");
    progBoard_ = compile(shaders::BOARD_VS, shaders::BOARD_FS, "mycelium board", true);
    progGlow_ = compile(shaders::GLOW_VS, shaders::GLOW_FS, "mycelium glow");
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
    instanced(threadVao_, threadVbo_, 3, sizeof(float) * 12);
    instanced(nodeVao_, nodeVbo_, 2, sizeof(float) * 8);
    instanced(rootVao_, rootVbo_, 2, sizeof(float) * 8);
    instanced(boardVao_, boardVbo_, 5, sizeof(Board));
    instanced(glowVao_, glowVbo_, 3, sizeof(float) * 12);
    instanced(spriteVao_, spriteVbo_, 3, sizeof(SpriteInst));
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));
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
    lightA_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    lightB_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomA_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

MyceliumUniverse::~MyceliumUniverse() {
    for (ugl::Target* t : {&worldT_, &lightA_, &lightB_, &bloomA_, &bloomB_, &sceneT_}) t->destroy();
    for (GLuint p : {progBg_, progThread_, progNode_, progRoot_, progBoard_, progGlow_, progComp_, progBright_, progBlur_,
                     progSprite_, progVeil_, progCell_, progShape_, progText_, progPost_})
        glDeleteProgram(p);
    for (GLuint v : {threadVao_, nodeVao_, rootVao_, boardVao_, glowVao_, spriteVao_, cellVao_, shapeVao_, textVao_})
        glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, threadVbo_, nodeVbo_, rootVbo_, boardVbo_, glowVbo_, spriteVbo_, cellVbo_, shapeVbo_, textVbo_})
        glDeleteBuffers(1, &b);
    for (Font* f : {&caps_, &capsB_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The network and its mushrooms
// =====================================================================================================

void MyceliumUniverse::uploadNet() {
    net_ = buildMycNet(netSeed_);
    glBindBuffer(GL_ARRAY_BUFFER, threadVbo_);
    glBufferData(GL_ARRAY_BUFFER, net_.seg.size() * sizeof(float), net_.seg.data(), GL_STATIC_DRAW);
    threadCount_ = (GLsizei)(net_.seg.size() / 12);
    glBindBuffer(GL_ARRAY_BUFFER, nodeVbo_);
    glBufferData(GL_ARRAY_BUFFER, net_.nodes.size() * sizeof(float), net_.nodes.data(), GL_STATIC_DRAW);
    nodeCount_ = (GLsizei)(net_.nodes.size() / 8);
    glBindBuffer(GL_ARRAY_BUFFER, rootVbo_);
    glBufferData(GL_ARRAY_BUFFER, net_.roots.size() * sizeof(float), net_.roots.data(), GL_STATIC_DRAW);
    rootCount_ = (GLsizei)(net_.roots.size() / 8);
    planMushrooms();
    planLandmarks();
    netDirty_ = false;
}

// The mushrooms: one at each of these lines, each bigger than the last (half the network's reach), with a few
// smaller ones at its foot. Each comes up where the threads reach the surface, placed so that it stands in the
// open at the sides of the well when it is born.
void MyceliumUniverse::planMushrooms() {
    shrooms_.clear();
    Rng r(netSeed_ ^ 0x5A0Full);
    static const int AT[] = {2,  4,  6,  8,  10, 12, 14, 17, 20, 23, 26, 29, 32, 35, 38, 41, 44, 48, 52, 56, 60,
                             65, 70, 75, 80, 86, 92, 98, 105, 112, 120, 130, 140, 150, 160, 175, 190, 205, 220};
    float side = 1.f;
    int lastColor = -1;
    for (int li : AT) {
        const float gl = myc::growthOf(li);
        const float gm = gl - 0.03f;
        const float R = std::exp(gm);
        const float H = 0.58f * R * r.range(0.85f, 1.1f);
        const myc::View vw = myc::frame(gl, 16.f / 9.f, 11.9f);
        auto xOf = [&](float bx) { return vw.cx + bx * vw.vh / 11.9f; };
        // a place in the open (the margins beside the HUD), over threads already there, away from the last ones
        float bestX = 0.f, bestScore = -1e9f, usedSide = side;
        auto tryRange = [&](float lo, float hi, float sd, float bonus) {
            for (int k = 0; k < 24; k++) {
                const float bx = sd * r.range(lo, hi);
                const float x = xOf(bx);
                if (std::fabs(x) > 0.95f * R) continue;
                float score = bonus + r.range(0.f, 0.5f);
                for (const Mushroom& o : shrooms_) {
                    if (o.companion || o.h0 < 0.25f * H) continue;
                    const float d = std::fabs(o.x - x) / H;
                    if (d < 0.5f) score -= (0.5f - d) * 6.f;
                }
                if (score > bestScore) { bestScore = score; bestX = x; usedSide = sd; }
            }
        };
        tryRange(side > 0.f ? 15.f : 15.5f, 19.5f, side, 3.f);
        tryRange(side > 0.f ? 15.5f : 15.f, 19.5f, -side, 2.f);
        if (bestScore < 0.5f) { tryRange(11.5f, 15.f, 1.f, 1.f); tryRange(12.f, 15.5f, -1.f, 1.f); }
        if (bestScore < -1e8f) bestX = xOf(side * 17.f);
        Mushroom m;
        m.x = bestX;
        // the pulse coordinate of the threads under it (the nearest site), else from its distance
        m.q = std::log(std::max(std::fabs(m.x), 1e-4f));
        {
            float bd = 0.2f * R;
            for (const MycNet::Site& st : net_.sites)
                if (st.g < gm && std::fabs(st.x - m.x) < bd) { bd = std::fabs(st.x - m.x); m.q = st.q; }
        }
        const bool flip = usedSide == side;
        m.g = gm;
        m.h0 = H;
        m.z = H * r.range(0.04f, 0.18f);
        m.seed = r.uniform();
        m.lean = r.range(-0.05f, 0.05f);
        m.type = shrooms_.size() < 2 ? 2 : (r.chance(0.6f) ? 0 : 2);
        if (r.chance(0.15f)) m.type = 1;
        int c = r.irange(0, 2);
        if (c == lastColor) c = (c + 1) % 3;
        lastColor = c;
        m.color = c;
        shrooms_.push_back(m);
        // companions at its foot
        const int n = r.irange(2, 4);
        for (int k = 0; k < n; k++) {
            Mushroom q = m;
            q.companion = true;
            const float sd = r.chance(0.5f) ? 1.f : -1.f;
            q.h0 = H * r.range(0.12f, 0.38f);
            q.x = m.x + sd * H * r.range(0.07f, 0.4f);
            q.g = gm + r.range(0.03f, 0.25f);
            q.z = std::max(0.01f * H, m.z + H * r.range(-0.03f, 0.12f));
            q.seed = r.uniform();
            q.lean = sd * r.range(0.0f, 0.12f);
            q.type = r.chance(0.5f) ? 1 : m.type;
            q.color = r.chance(0.6f) ? m.color : r.irange(0, 2);
            shrooms_.push_back(q);
        }
        if (flip) side = -side;
    }
}

float MyceliumUniverse::shroomHeight(const Mushroom& m) const {
    // it keeps growing after it bursts up (some four times taller over the next scales)
    const float grow = std::exp(0.55f * std::min(std::max(0.f, g_ - m.g), 2.6f));
    const float a = m.age;
    const float e = std::max(0.f, 1.f - std::exp(-a * 3.2f) * std::cos(a * 5.f));
    return m.h0 * grow * e;
}

// =====================================================================================================
// Camera
// =====================================================================================================

void MyceliumUniverse::updateCamera(float dt) {
    if (gc_ < -50.f) { gc_ = g_; gcVel_ = 0.f; }
    const float w = 3.4f;
    gcVel_ += (w * w * (g_ - gc_) + 2.f * w * (0.85f * gVel_ - gcVel_)) * dt;
    gc_ += gcVel_ * dt;
    view_ = myc::frame(gc_, aspect_, viewH_.y);
    pxW_ = 2.f * view_.vh / (float)std::max(1, worldT_.h);
}

vec2 MyceliumUniverse::worldToBoard(float x, float y, float z) const {
    const float s = view_.D / (view_.D + z);
    const float wx = view_.ex + (x - view_.ex) * s, wy = view_.ey + (y - view_.ey) * s;
    return V(viewC_.x + (wx - view_.cx) / view_.vh * viewH_.y, viewC_.y + (wy - view_.cy) / view_.vh * viewH_.y);
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
    h.background = lerp(p.skyLow, p.soilB, 0.5f);
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
        // the cells fray into light that flies down into the soil, to the network's heart
        vec2 to = worldToBoard(0.f, myc::SPORE_Y);
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
                mt.to = add(to, V(rng_.range(-1.2f, 1.2f), rng_.range(-0.8f, 0.8f)));
                mt.ctl = V(side * rng_.range(5.5f, 7.5f), std::min(mt.from.y, mt.to.y) + rng_.range(2.f, 5.f));
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
        // a spurt, and a pulse out from the spore through every thread
        spurt_ = std::max(spurt_, 0.4f + 0.15f * lines);
        if ((int)pulses_.size() < 6) {
            const float q0 = std::log(view_.vh * 0.04f);
            pulses_.push_back({q0, (g_ + 0.4f - q0) / 1.6f, lines >= 4 ? 1.1f : 0.5f, 1.f, rng_.uniform()});
        }
        if (lines >= 4) tetrisGlow_ = 1.f;
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
// Plants, trees, rocks: built for the view every frame
// =====================================================================================================

// Things at every scale stand behind the cut: for each size class, a few rows at depths proportional to it, laid on
// a grid of cells hashed into plants, rocks and trees; only those that are a sensible size on screen are kept (so a
// moss sprig in the macro view, a fern later, trees in the wide view). The plants on the cut's edge come from the
// roots' clusters (network.cpp).
void MyceliumUniverse::buildFlora() {
    const myc::View& v = view_;
    const float hw = v.vh * aspect_;
    const float screenH = (float)worldT_.h;
    const float R = std::exp(g_);
    const float world = 1.f - smoothstepf(0.f, 0.7f, smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    auto push = [&](float x, float y, float z, float h, float w, int type, float seed, float alpha, const float c[4],
                    const vec3& col, float glow, const vec3& col2, float e) {
        Board b;
        b.a[0] = x; b.a[1] = y; b.a[2] = z; b.a[3] = h;
        b.b[0] = w; b.b[1] = (float)type; b.b[2] = seed; b.b[3] = alpha;
        for (int i = 0; i < 4; i++) b.c[i] = c[i];
        b.d[0] = col.x; b.d[1] = col.y; b.d[2] = col.z; b.d[3] = glow;
        b.e[0] = col2.x; b.e[1] = col2.y; b.e[2] = col2.z; b.e[3] = e;
        boards_.push_back(b);
    };
    auto sizeAlpha = [&](float h, float z) {
        const float hpx = h * v.D / (v.D + z) / pxW_;
        return smoothstepf(3.f, 9.f, hpx) * (1.f - smoothstepf(1.f * screenH, 1.7f * screenH, hpx));
    };
    auto hashf = [](int a, int b, int c) {
        uint64_t h = splitmix64((uint64_t)(uint32_t)a * 0x9E3779B97F4A7C15ull ^ ((uint64_t)(uint32_t)b << 20) ^ ((uint64_t)c << 40));
        return (h >> 40) * (1.0f / 16777216.0f);
    };
    const float pinsK = smoothstepf(myc::growthOf(2), myc::growthOf(4), g_);
    // clearings: no tree in front of a human-made thing that is showing (its window span and depth)
    struct Span { float l, r, z, k; };
    std::vector<Span> clear;
    for (const Landmark& m : marks_) {
        if (m.type >= L_PLANE || m.type == L_WIRES || m.shroom < 0 || !shrooms_[m.shroom].born) continue;
        const float sk = smoothstepf(0.f, 1.5f, shrooms_[m.shroom].age);
        if (sk <= 0.f) continue;
        const float k = v.D / (v.D + m.z);
        const float w = m.w * (m.type == L_PYLON ? 2.f : 1.3f);
        clear.push_back({v.ex + (m.x - w - v.ex) * k, v.ex + (m.x + w - v.ex) * k, m.z, sk});
    }
    auto hides = [&](float x, float w, float z) {
        const float k = v.D / (v.D + z);
        const float l = v.ex + (x - w - v.ex) * k, r = v.ex + (x + w - v.ex) * k;
        float h = 0.f;
        for (const Span& c : clear)
            if (z < c.z && r > c.l && l < c.r) h = std::max(h, c.k);
        return h;
    };
    // the floor's clutter at every scale
    for (int k = 0; k <= 10; k++) {
        const float s = 0.0015f * std::pow(3.f, (float)k);
        for (int j = 0; j < 3; j++) {
            const float z0 = s * (j == 0 ? 1.1f : j == 1 ? 2.8f : 6.5f);
            const bool trees = k >= 8;
            const float hTyp = trees ? 22.f : s;
            if (sizeAlpha(hTyp, z0) <= 0.f && sizeAlpha(hTyp * 0.5f, z0) <= 0.f) continue;
            const float sc = (v.D + z0) / v.D;
            const float left = v.ex + (v.cx - hw - v.ex) * sc, right = v.ex + (v.cx + hw - v.ex) * sc;
            const float cs = s * (trees ? 0.9f : 1.1f);
            const int i0 = (int)std::floor((left - 2.f * s) / cs), i1 = (int)std::ceil((right + 2.f * s) / cs);
            const int n = i1 - i0;
            const int stride = std::max(1, n / 220);
            for (int i = i0; i <= i1; i += stride) {
                const float h0 = hashf(i, k, j * 7 + 1);
                if (h0 > (trees ? 0.5f : 0.62f)) continue;
                const float x = (i + 0.15f + 0.7f * hashf(i, k, j * 7 + 2)) * cs;
                const float z = z0 * (0.8f + 0.4f * hashf(i, k, j * 7 + 3));
                const float seed = hashf(i, k, j * 7 + 4);
                const float tsel = hashf(i, k, j * 7 + 5);
                const bool under = std::fabs(x) < R * 0.85f && z < R * 0.4f;
                int type;
                float h, wq;
                float c[4] = {0, 0, 0, 0};
                vec3 col = pal_.cap[(int)(seed * 3.f) % 3];
                float glow = 0.f;
                if (trees) {
                    type = B_TREE;
                    h = 17.f + 13.f * seed;
                    c[0] = 0.018f + 0.014f * hashf(i, k, 9);
                    c[1] = 0.32f + 0.15f * hashf(i, k, 10);
                    c[2] = 0.2f + 0.12f * hashf(i, k, 11);
                    wq = h * 0.36f;
                } else if (k <= 1) {
                    type = (under && pinsK > 0.f && tsel < 0.18f) ? B_PINS : tsel < 0.7f ? B_MOSS : B_ROCK;
                    h = s * (0.6f + 0.6f * seed);
                    wq = h * (type == B_ROCK ? 0.7f : 0.9f);
                } else if (k <= 3) {
                    type = (under && pinsK > 0.f && tsel < 0.22f) ? B_PINS : tsel < 0.45f ? B_MOSS : tsel < 0.85f ? B_GRASS : B_ROCK;
                    h = s * (0.7f + 0.7f * seed);
                    wq = h * (type == B_ROCK ? 0.7f : type == B_GRASS ? 0.5f : 0.9f);
                } else if (k <= 5) {
                    type = (under && pinsK > 0.f && tsel < 0.18f) ? B_PINS : tsel < 0.45f ? B_GRASS : tsel < 0.85f ? B_FERN : B_ROCK;
                    h = s * (0.8f + 0.7f * seed);
                    wq = h * (type == B_ROCK ? 0.7f : type == B_FERN ? 0.75f : 0.5f);
                } else {
                    type = tsel < 0.4f ? B_FERN : tsel < 0.82f ? B_BUSH : B_ROCK;
                    h = s * (0.7f + 0.8f * seed) * (type == B_BUSH ? 1.1f : 1.f);
                    wq = h * (type == B_ROCK ? 0.7f : type == B_FERN ? 0.75f : 0.7f);
                }
                if (type == B_PINS) {
                    c[0] = seed;
                    glow = pal_.glow * (0.7f + 0.3f * hashf(i, k, 12)) * pinsK * (1.f - 0.7f * over_);
                    h *= 0.8f;
                    wq = h * 0.6f;
                }
                if (type == B_MOSS) glow = pinsK * (under ? 1.f : 0.f) * (1.f - 0.7f * over_);
                const float a = sizeAlpha(h, z) * world;
                if (a <= 0.003f) continue;
                const float hid = (type == B_TREE || type == B_BUSH) ? hides(x, wq * 0.8f, z) : 0.f;
                if (hid >= 0.999f) continue;
                push(x, myc::surf(x), z, h, wq, type, seed, a * (1.f - hid), c, col, glow, vec3(0.f), 0.f);
            }
        }
    }
    // the plants on the cut's edge (over the roots)
    for (const MycNet::Plant& p : net_.plants) {
        const float x = p.x;
        if (x < v.cx - hw * 1.2f - p.s * 2.f || x > v.cx + hw * 1.2f + p.s * 2.f) continue;
        int type;
        float h, wq, z;
        float c[4] = {0, 0, 0, 0};
        if (p.cls <= 2) { type = p.seed < 0.5f ? B_MOSS : B_GRASS; h = p.s * (type == B_MOSS ? 0.5f : 1.3f); wq = h * 0.8f; z = p.s * 0.06f; }
        else if (p.cls <= 4) { type = p.seed < 0.55f ? B_GRASS : B_FERN; h = p.s * 1.4f; wq = h * 0.7f; z = p.s * 0.08f; }
        else if (p.cls <= 6) { type = p.seed < 0.5f ? B_FERN : B_BUSH; h = p.s * 1.1f; wq = h * 0.75f; z = p.s * 0.1f; }
        else {
            type = B_TREE;
            h = std::clamp(p.s * 2.f, 15.f, 30.f);
            c[0] = 0.02f + 0.012f * p.seed;
            c[1] = 0.3f + 0.15f * p.seed;
            c[2] = 0.2f + 0.1f * p.seed;
            wq = h * 0.36f;
            z = 0.6f + p.s * 0.08f;
        }
        const float a = sizeAlpha(h, z) * world;
        if (a <= 0.003f) continue;
        const float hid = type == B_TREE ? hides(x, wq * 0.8f, z) : 0.f;
        if (hid >= 0.999f) continue;
        push(x, myc::surf(x), z, h, wq, type, p.seed, a * (1.f - hid), c, vec3(0.f), 0.f, vec3(0.f), 0.f);
    }
}

void MyceliumUniverse::buildShroomBoards() {
    const float world = 1.f - smoothstepf(0.f, 0.7f, smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    const float wilt = smoothstepf(0.5f, 3.5f, overT_ >= 0.f ? overT_ : 0.f);
    for (const Mushroom& m : shrooms_) {
        if (!m.born) continue;
        const float H = shroomHeight(m) * (1.f - wilt);
        if (H <= 0.f) continue;
        const float hpx = H * view_.D / (view_.D + m.z) / pxW_;
        if (hpx < 2.f) continue;
        const float open = smoothstepf(0.25f, 1.8f, m.age) * (1.f - 0.4f * wilt);
        const float glow = pal_.glow * (0.6f + 0.1f * wm + 0.2f * wp + 0.1f * life + 0.12f * swell_) * (1.f - 0.75f * over_) +
                           m.flare * 0.8f + m.beat * 0.4f;
        const float wave = m.flareT < 1.6f ? m.flareT / 0.75f : 0.f;
        Board b;
        b.a[0] = m.x; b.a[1] = myc::surf(m.x); b.a[2] = m.z; b.a[3] = H;
        b.b[0] = H * 0.55f; b.b[1] = (float)B_MUSHROOM; b.b[2] = m.seed; b.b[3] = world * smoothstepf(2.f, 6.f, hpx);
        b.c[0] = open; b.c[1] = (float)m.type; b.c[2] = wave; b.c[3] = m.lean + (m.companion ? 0.f : 0.f) + wilt * 0.15f;
        const vec3 cc = pal_.cap[m.color], gc = pal_.gill[m.color];
        b.d[0] = cc.x; b.d[1] = cc.y; b.d[2] = cc.z; b.d[3] = glow;
        b.e[0] = gc.x; b.e[1] = gc.y; b.e[2] = gc.z; b.e[3] = 0.f;
        boards_.push_back(b);
    }
}

// =====================================================================================================
// Human-made things, for scale
// =====================================================================================================

// Things people built, at their real sizes, standing among the mushrooms: each one comes with the mushroom that is
// about its height when it bursts up (at its foot, beside its stem, at its depth), so the two are seen side by side;
// then the mushrooms keep growing and newer ones tower higher, and the same house, church or skyscraper ends up at an
// ankle. A cabin and its car, a farmhouse, a line of pylons, a church, a helicopter, wind turbines, a hot-air balloon,
// skyscrapers, a radio mast, airliners crossing between the caps (at fixed altitudes: low beside the last caps), and
// clouds wrapping the stems.
void MyceliumUniverse::planLandmarks() {
    marks_.clear();
    Rng r(netSeed_ ^ 0x1A4D3A2Cull);
    // the main mushroom whose height is nearest a target
    auto pick = [&](float target) {
        int best = -1;
        float bd = 1e9f;
        for (int i = 0; i < (int)shrooms_.size(); i++) {
            const Mushroom& m = shrooms_[i];
            if (m.companion) continue;
            const float d = std::fabs(std::log(m.h0 / target));
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    };
    // its side of the screen when it is born (to keep what stands by it in the open)
    auto sideOf = [&](const Mushroom& m) {
        const myc::View v = myc::frame(m.g, 16.f / 9.f, 11.9f);
        return (m.x - v.cx) >= 0.f ? 1.f : -1.f;
    };
    auto add = [&](int type, int sh, float dx, float y, float dz, float h, float w, float p0 = 0.f) {
        if (sh < 0) return;
        const Mushroom& m = shrooms_[sh];
        Landmark l;
        l.type = type; l.shroom = sh; l.x = m.x + dx; l.y = y; l.z = std::max(0.5f, m.z + dz); l.h = h; l.w = w;
        l.p0 = p0; l.seed = r.uniform(); l.u = 0.f; l.init = false;
        if (l.y < -1e8f) l.y = myc::surf(l.x);
        marks_.push_back(l);
    };
    const float GROUND = -1e9f;
    // beside a mushroom: toward the inside of the screen when it stands at the edge, at its foot, a little in front
    auto beside = [&](int sh, float gap) {
        if (sh < 0) return 0.f;
        const Mushroom& m = shrooms_[sh];
        return -sideOf(m) * (0.07f * m.h0 + gap);
    };
    {   // a cabin, its car parked by it
        const int s = pick(6.5f);
        const float dx = beside(s, 5.f);
        add(L_CABIN, s, dx, GROUND, -0.3f * (s >= 0 ? shrooms_[s].z : 0.f), 6.f, 4.4f, 0.f);
        add(L_CAR, s, dx + (dx > 0.f ? 8.f : -8.f), GROUND, -0.35f * (s >= 0 ? shrooms_[s].z : 0.f), 1.5f, 2.3f, dx > 0.f ? 1.f : -1.f);
    }
    {   const int s = pick(10.f); add(L_CABIN, s, beside(s, 9.f), GROUND, -0.3f * (s >= 0 ? shrooms_[s].z : 0.f), 9.f, 8.5f, 1.f); }
    {   // a line of pylons striding past a mushroom, their wires
        const int s = pick(40.f);
        if (s >= 0) {
            const float dir = -sideOf(shrooms_[s]);
            const float x0 = 0.05f * shrooms_[s].h0 * dir;
            for (int i = 0; i < 4; i++) {
                const float dx = x0 + dir * i * 60.f;
                add(L_PYLON, s, dx, GROUND, 10.f + i * 4.f, 38.f, 13.f);
                if (i < 3) add(L_WIRES, s, dx + dir * 30.f, GROUND, 12.f + i * 4.f, 38.f, 30.f);
            }
        }
    }
    {   const int s = pick(55.f); add(L_SPIRE, s, beside(s, 30.f), GROUND, -0.3f * (s >= 0 ? shrooms_[s].z : 0.f), 55.f, 25.f); }
    {   const int s = pick(100.f); add(L_HELI, s, 0.f, 130.f, -0.4f * (s >= 0 ? shrooms_[s].z : 0.f), 6.f, 4.4f, 1.f); }
    {   // wind turbines around a mushroom's foot
        const int s = pick(160.f);
        if (s >= 0) {
            const float sd = -sideOf(shrooms_[s]);
            for (int i = 0; i < 3; i++)
                add(L_TURBINE, s, sd * (0.12f * shrooms_[s].h0 + 70.f + i * 160.f), GROUND, (i - 1) * 40.f, 150.f, 64.f);
        }
    }
    {   const int s = pick(230.f); add(L_BALLOON, s, 0.f, 230.f, -0.5f * (s >= 0 ? shrooms_[s].z : 0.f), 26.f, 9.f, -1.f); }
    {   // skyscrapers growing among the stems
        const int s = pick(400.f);
        add(L_SKYLINE, s, beside(s, 60.f), GROUND, -0.25f * (s >= 0 ? shrooms_[s].z : 0.f), 400.f, 300.f);
        const int s2 = pick(650.f);
        if (s2 >= 0 && s2 != s) add(L_SKYLINE, s2, beside(s2, 70.f), GROUND, -0.25f * shrooms_[s2].z, 460.f, 340.f);
        add(L_MAST, s2 >= 0 ? s2 : s, -beside(s2 >= 0 ? s2 : s, 200.f), GROUND, -0.2f * (s >= 0 ? shrooms_[s].z : 0.f), 600.f, 230.f);
    }
    {   // airliners, low over the land first, then far under the caps
        add(L_PLANE, pick(170.f), 0.f, 210.f, -20.f, 60.f, 34.f, 1.f);
        add(L_PLANE, pick(420.f), 0.f, 480.f, -40.f, 70.f, 40.f, -1.f);
        add(L_PLANE, pick(850.f), 0.f, 880.f, -60.f, 70.f, 40.f, 1.f);
    }
    {   // clouds wrapping the stems of the tallest
        for (int k = 0; k < 2; k++) {
            const int s = pick(k == 0 ? 1100.f : 1800.f);
            if (s < 0) continue;
            const float H = shrooms_[s].h0;
            for (int i = 0; i < 6; i++)
                add(L_CLOUD, s, r.range(-0.6f, 0.6f) * H, r.range(0.35f, 0.6f) * H * (k == 0 ? 1.f : 0.8f), (i % 2 ? -0.06f : 0.08f) * H,
                    0.12f * H, r.range(0.25f, 0.5f) * H);
        }
    }
}

void MyceliumUniverse::buildLandmarks() {
    markGlow_.clear();
    const myc::View& v = view_;
    const float hw = v.vh * aspect_;
    const float screenH = (float)worldT_.h;
    const float world = 1.f - smoothstepf(0.f, 0.7f, smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    auto sizeA = [&](float h, float z) {
        const float hpx = h * v.D / (v.D + z) / pxW_;
        return smoothstepf(1.f, 3.f, hpx) * (1.f - smoothstepf(1.4f * screenH, 2.2f * screenH, hpx));
    };
    // a light: never under ~3 px, so a small lamp stays a point of light
    auto light = [&](const vec3& p, float size, const vec3& c, float a) {
        if (a <= 0.004f) return;
        const float minS = 3.f * pxW_ * (v.D + p.z) / v.D;
        const float q[12] = {p.x, p.y, p.z, std::max(size, minS), c.x, c.y, c.z, a * world, 3.f, 0.f, 0.f, 0.f};
        markGlow_.insert(markGlow_.end(), q, q + 12);
    };
    auto blink = [&](float f, float ph) { const float s = 0.5f + 0.5f * std::sin(time_ * TAU * f + ph); return 0.25f + 0.75f * s * s * s; };
    const vec3 warm = ugl::toLinear(vec3(1.f, 0.78f, 0.5f)), red = ugl::toLinear(vec3(1.f, 0.16f, 0.1f)),
               green = ugl::toLinear(vec3(0.3f, 1.f, 0.45f)), white = vec3(1.f, 0.97f, 0.92f);
    const float t = time_;
    const float dtw = std::max(0.f, time_ - markTime_);
    markTime_ = time_;
    for (Landmark& m : marks_) {
        if (m.shroom < 0 || m.shroom >= (int)shrooms_.size()) continue;
        const Mushroom& sh = shrooms_[m.shroom];
        if (!sh.born) { m.init = false; continue; }
        const float ga = smoothstepf(0.f, 1.5f, sh.age);
        float x = m.x, y = m.y;
        const bool flies = m.type == L_PLANE || m.type == L_HELI || m.type == L_BALLOON;
        if (flies) {
            // moving in screen space (u: the offset from the eye in window heights at its depth), so a plane always
            // moves forward on screen however the camera pulls back
            const float span = aspect_ * 1.05f + (m.type == L_PLANE ? 8.f * m.h / v.vh : 0.25f);
            const float c0 = (v.cx - v.ex) / v.vh;
            if (!m.init) {
                const float k = v.D / (v.D + m.z);
                m.u = (sh.x - v.ex) * k / v.vh - m.p0 * 0.6f;
                m.init = true;
            }
            const float speed = m.type == L_PLANE ? 0.16f : m.type == L_HELI ? 0.07f : 0.012f;
            m.u += m.p0 * speed * dtw;
            if (m.u > c0 + span) m.u -= 2.f * span;
            if (m.u < c0 - span) m.u += 2.f * span;
            x = v.ex + m.u * v.vh * (v.D + m.z) / v.D;
            if (m.type == L_BALLOON) y += 6.f * std::sin(t * 0.2f + m.seed * 9.f);
        }
        const float a = ga * sizeA(m.h, m.z) * world;
        Board b;
        b.a[0] = x; b.a[1] = y; b.a[2] = m.z; b.a[3] = m.h;
        b.b[0] = m.w; b.b[1] = (float)(8 + m.type); b.b[2] = m.seed; b.b[3] = a;
        b.c[0] = m.p0; b.c[1] = b.c[2] = b.c[3] = 0.f;
        for (int i = 0; i < 4; i++) { b.d[i] = 0.f; b.e[i] = 0.f; }
        if (a > 0.003f) boards_.push_back(b);
        const float la = ga * world * smoothstepf(0.3f, 1.5f, m.h * v.D / (v.D + m.z) / pxW_ + 1.f);
        switch (m.type) {
        case L_CABIN: light(vec3(x, y + m.h * 0.3f, m.z - 0.5f), m.h * 0.9f, warm, 0.1f * la); break;
        case L_CAR:
            light(vec3(x + m.w * 0.95f * m.p0, y + m.h * 0.42f, m.z - 0.3f), m.h * 0.35f, white, 0.8f * la);
            light(vec3(x + m.w * 0.95f * m.p0 + m.p0 * m.h * 2.f, y + m.h * 0.3f, m.z - 0.3f), m.h * 1.6f, white, 0.12f * la);
            break;
        case L_PYLON: light(vec3(x, y + m.h, m.z - 0.5f), 1.2f, red, 0.5f * la * blink(0.4f, 0.f)); break;
        case L_SPIRE: light(vec3(x - m.h * 0.22f, y + m.h * 0.3f, m.z - 1.f), m.h * 0.35f, warm, 0.12f * la); break;
        case L_MAST:
            for (int i = 1; i <= 3; i++) light(vec3(x, y + m.h * (i == 3 ? 1.f : i * 0.33f), m.z - 1.f), 3.f, red, 0.85f * la * blink(0.5f, 0.f));
            break;
        case L_TURBINE: light(vec3(x, y + m.h * 0.68f, m.z - 1.f), 2.5f, red, 0.75f * la * blink(0.5f, 1.3f)); break;
        case L_SKYLINE:
            light(vec3(x + 0.05f * m.w, y + m.h * 1.06f, m.z - 1.f), 4.f, red, 0.8f * la * blink(0.45f, 2.f));
            light(vec3(x, y + m.h * 0.3f, m.z - 5.f), m.w * 1.2f, warm, 0.05f * la);
            break;
        case L_PLANE: {
            const float dir = m.p0;
            Board tr = b;
            const float len = 7.f * m.h;
            tr.a[0] = x - dir * (len + m.h * 0.5f); tr.a[1] = y + m.h * 0.5f - 0.4f * m.h; tr.a[3] = 0.8f * m.h;
            tr.b[0] = len; tr.b[1] = 17.f; tr.b[3] = ga * world * 0.8f;
            tr.c[0] = dir;
            boards_.push_back(tr);
            light(vec3(x + dir * m.h * 0.5f, y + m.h * 0.47f, m.z - 1.f), 2.f, white, 0.9f * la);
            light(vec3(x + dir * m.h * 1.6f, y + m.h * 0.4f, m.z - 1.f), m.h * 1.2f, white, 0.07f * la);
            light(vec3(x, y + m.h * 0.48f, m.z - 1.f), 1.5f, red, 0.9f * la);
            light(vec3(x, y + m.h * 0.48f, m.z + 1.f), 1.5f, green, 0.6f * la);
            light(vec3(x - dir * m.h * 0.45f, y + m.h * 0.68f, m.z - 1.f), 1.5f, white, 0.9f * la * blink(0.7f, m.seed * 6.f));
            light(vec3(x, y + m.h * 0.56f, m.z - 1.f), 1.5f, red, 0.7f * la * blink(0.55f, 1.f));
            break;
        }
        case L_HELI:
            light(vec3(x, y + m.h * 0.7f, m.z - 1.f), 1.2f, red, 0.85f * la * blink(0.6f, 0.5f));
            light(vec3(x + m.p0 * m.h * 0.3f, y + m.h * 0.3f, m.z - 1.f), 1.2f, white, 0.6f * la);
            break;
        case L_BALLOON:
            light(vec3(x, y + m.h * 0.3f, m.z - 1.f), m.h * 0.5f, warm, 0.18f * la * (0.7f + 0.3f * std::sin(t * 0.7f + m.seed * 9.f)));
            break;
        default: break;
        }
    }
    (void)hw;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MyceliumUniverse::render(const Frame& f, GLuint fbo) {
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
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 3.f) : 0.f, overT_ >= 0.f ? 10.f : 0.8f, dt);
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

    // ---- The game's lines -> the growth (a new game grows a new network).
    {
        const int lines = game.lines() + extraLines(time_);
        if (netSeed_ == 0) { netSeed_ = seed_ ? seed_ : 1; netDirty_ = true; }
        if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
            netSeed_ = splitmix64(netSeed_ + 0x9E37ull);
            netDirty_ = true;
            gBonus_ = 0.f;
            g_ = myc::growthOf(lines);
            gVel_ = 0.f;
            spores_.clear();
            pulses_.clear();
        }
        lines_ = lines;
        if (netDirty_) uploadNet();
        if (isDrop && !wasDrop_) gBonus_ += 0.12f;
        gTarget_ = std::min(myc::G_MAX, myc::growthOf(lines) + gBonus_);
        if (linesShown_ < 0) { g_ = gTarget_; gVel_ = 0.f; }
        linesShown_ = lines;
        // a spurt: a critically damped spring toward the target, over a second or so
        const float w = 3.6f;
        gVel_ += (w * w * (gTarget_ - g_) - 2.f * w * gVel_) * wdt;
        g_ += gVel_ * wdt;
        if (g_ > gTarget_ + 0.001f && gVel_ > 0.f) { g_ = gTarget_; gVel_ = 0.f; }
    }
    spurt_ = approach(spurt_, 0.f, 1.2f, dt);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        if (slow_ > 0.01f) {
            // slow motion: a hush, the night colder and the light paler
            const float k = slow_;
            pal_.mist = lerp(pal_.mist, pal_.mist * vec3(0.8f, 0.95f, 1.25f), k);
            pal_.skyLow = lerp(pal_.skyLow, pal_.skyLow * vec3(0.8f, 0.95f, 1.2f), k);
            pal_.thread = lerp(pal_.thread, lerp(pal_.thread, pal_.pulse, 0.4f), k);
        }
    }
    const bool dropStart = isDrop && !wasDrop_;
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.5f : 0.4f, dt);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    swell_ = approach(swell_, 0.f, 0.35f, dt);
    tetrisGlow_ = approach(tetrisGlow_, 0.f, 0.4f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    kickGlow_ = std::max(approach(kickGlow_, 0.f, 4.f, dt), kick * wp);
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    gust_ = approach(gust_, 0.f, 0.4f, dt);
    const float wind = 0.25f + 0.25f * wm + 0.35f * wp + 0.4f * gust_;
    windPhase_ += wdt * (0.6f + 0.25f * wm + 0.3f * wp);

    updateCamera(dt);
    const float qStart = std::log(view_.vh * 0.04f);

    // ---- The beat: a pulse out along the threads every two beats (some threads at mid, all at a peak).
    {
        bool fire = false;
        float beatSec = 60.f / std::max(60.f, music.bpm);
        if (music.hasSong) {
            const double b = std::floor(music.beat);
            if (b != lastBeat_) {
                if (lastBeat_ >= 0.0 && ((long long)b % 2 == 0)) fire = true;
                lastBeat_ = b;
            }
        } else if (!f.paused) {
            beatClock_ += dt;
            beatSec = 0.5f;
            if (beatClock_ > 1.f) { beatClock_ -= 1.f; fire = true; }
        }
        if (fire && !f.paused && overT_ < 0.f) {
            beatCount_++;
            const float k = wm * (1.f - wp) + wp;
            if (k > 0.25f && (int)pulses_.size() < 6) {
                const float amp = (0.25f + 0.35f * wp + 0.25f * life) * (0.7f + 0.3f * std::min(1.f, kick + 0.4f));
                pulses_.push_back({qStart, (g_ + 0.4f - qStart) / (beatSec * 5.f), amp, 0.3f + 0.7f * wp, rng_.uniform()});
            }
            // a mushroom or two glow softly on the beat
            if (k > 0.25f && !shrooms_.empty()) {
                for (int t = 0; t < 1 + (wp > 0.5f ? 1 : 0); t++) {
                    const int i = rng_.irange(0, (int)shrooms_.size() - 1);
                    if (shrooms_[i].born && shrooms_[i].companion) shrooms_[i].beatK = std::max(shrooms_[i].beatK, 0.4f + 0.4f * wp);
                }
            }
        }
        for (Pulse& p : pulses_) p.q += p.speed * wdt;
        pulses_.erase(std::remove_if(pulses_.begin(), pulses_.end(), [&](const Pulse& p) { return p.q > g_ + 0.8f; }), pulses_.end());
    }
    // ---- The drop: one wave of light from the spore through the whole network, up into every mushroom.
    if (dropStart) {
        waveQ_ = qStart - 0.3f;
        waveSpeed_ = (g_ + 0.5f - waveQ_) / 2.6f;
        waveAmp_ = 0.01f;
        waveT_ = 0.f;
        gust_ = 1.f;
    }
    if (waveAmp_ > 0.f) {
        waveQ_ += waveSpeed_ * wdt;
        waveT_ += wdt;
        if (waveQ_ > g_ + 0.4f) waveAmp_ = approach(waveAmp_, 0.f, 0.6f, dt);
        else waveAmp_ = 1.25f * smoothstepf(0.f, 0.5f, waveT_);
    }

    // ---- Mushrooms: born as the growth passes them, flaring as the drop's wave reaches them.
    {
        for (Mushroom& m : shrooms_) {
            if (!m.born && g_ >= m.g) { m.born = true; m.age = 0.f; }
            if (m.born && g_ < m.g - 0.1f) { m.born = false; m.age = 0.f; }
            if (!m.born) continue;
            m.age += wdt;
            m.beatK = approach(m.beatK, 0.f, 1.2f, dt);
            m.beat = approach(m.beat, m.beatK, 6.f, dt);
            if (waveAmp_ > 0.5f && m.flareT > 3.f && waveQ_ >= m.q && waveQ_ < m.q + 1.f && m.age > 0.5f) m.flareT = 0.f;
            if (m.flareT < 50.f) {
                const float prev = m.flareT;
                m.flareT += wdt;
                if (prev < 0.75f && m.flareT >= 0.75f) {
                    m.flareK = 1.f;
                    // a cloud of spores floats up from the cap
                    const float H = shroomHeight(m);
                    const float hpx = H * view_.D / (view_.D + m.z) / pxW_;
                    const int n = (int)std::clamp(hpx * (m.companion ? 0.25f : 0.6f), 0.f, m.companion ? 30.f : 160.f);
                    const float hs = m.type == 0 ? 0.8f : m.type == 1 ? 0.72f : 0.64f;
                    const float rc = H * (m.type == 0 ? 0.42f : m.type == 1 ? 0.16f : 0.34f);
                    for (int i = 0; i < n && (int)spores_.size() < MAX_SPORES; i++) {
                        Spore s;
                        const float u = rng_.range(-1.f, 1.f);
                        s.p = vec3(m.x + u * rc, myc::surf(m.x) + H * (hs + 0.05f + 0.1f * (1.f - u * u)), m.z + rng_.range(-0.05f, 0.05f) * H);
                        s.v = vec3(u * rng_.range(0.08f, 0.35f), rng_.range(0.1f, 0.45f), rng_.range(-0.05f, 0.05f)) * H;
                        s.age = -rng_.range(0.f, 1.2f);
                        s.life = rng_.range(6.f, 11.f);
                        s.size = H * rng_.range(0.008f, 0.02f);
                        s.seed = rng_.uniform();
                        s.col = lerp(pal_.gill[m.color], pal_.pulse, rng_.range(0.f, 0.5f));
                        s.kind = 0;
                        spores_.push_back(s);
                    }
                }
            }
            m.flareK = approach(m.flareK, 0.f, 0.35f, dt);
            m.flare = approach(m.flare, m.flareK, 2.5f, dt);
        }
    }
    // the lights: the biggest caps light the floor, the plants and the canopy
    struct Light { vec3 p; float r; vec3 c; float h; };
    std::vector<Light> lights;
    {
        const float wilt = smoothstepf(0.5f, 3.5f, overT_ >= 0.f ? overT_ : 0.f);
        for (const Mushroom& m : shrooms_) {
            if (!m.born) continue;
            const float H = shroomHeight(m) * (1.f - wilt);
            if (H <= 0.f) continue;
            const float hs = m.type == 0 ? 0.8f : m.type == 1 ? 0.72f : 0.64f;
            const float glow = pal_.glow * (0.6f + 0.1f * wm + 0.2f * wp + 0.1f * life + 0.12f * swell_) * (1.f - 0.75f * over_) + m.flare * 0.8f + m.beat * 0.3f;
            const vec3 c = pal_.cap[m.color] * glow * 0.9f * smoothstepf(0.f, 0.6f, m.age);
            lights.push_back({vec3(m.x, myc::surf(m.x) + H * hs, m.z), H * 0.8f, c, H});
        }
        std::sort(lights.begin(), lights.end(), [](const Light& a, const Light& b) { return a.h > b.h; });
        if (lights.size() > 8) lights.resize(8);
    }

    // ---- Spores in the air: a few always drifting, more at a peak, rising from the caps.
    {
        const float rate = (0.8f + 2.f * wm + 6.f * wp + 10.f * life) * world;
        if (!f.paused) sporeAcc_ += rate * wdt;
        sporeAcc_ = std::min(sporeAcc_, 20.f);
        while (sporeAcc_ >= 1.f && (int)spores_.size() < MAX_SPORES) {
            sporeAcc_ -= 1.f;
            Spore s;
            // from a born mushroom's cap, else over the floor in view
            int pick = -1;
            for (int t = 0; t < 6 && !shrooms_.empty(); t++) {
                const int i = rng_.irange(0, (int)shrooms_.size() - 1);
                if (shrooms_[i].born && shroomHeight(shrooms_[i]) > view_.vh * 0.05f) { pick = i; break; }
            }
            if (pick >= 0) {
                const Mushroom& m = shrooms_[pick];
                const float H = shroomHeight(m);
                s.p = vec3(m.x + rng_.range(-0.35f, 0.35f) * H, myc::surf(m.x) + H * rng_.range(0.6f, 0.85f), m.z);
                s.v = vec3(rng_.range(-0.04f, 0.04f), rng_.range(0.03f, 0.1f), 0.f) * H;
                s.size = H * rng_.range(0.005f, 0.01f);
                s.col = lerp(pal_.gill[m.color], pal_.pulse, 0.3f);
            } else {
                const float x = view_.cx + rng_.range(-1.f, 1.f) * view_.vh * aspect_;
                s.p = vec3(x, myc::surf(x) + rng_.range(0.f, 0.3f) * view_.vh, rng_.range(0.05f, 1.f) * view_.vh);
                s.v = vec3(rng_.range(-0.02f, 0.02f), rng_.range(0.01f, 0.04f), 0.f) * view_.vh;
                s.size = view_.vh * rng_.range(0.003f, 0.006f);
                s.col = lerp(pal_.threadTip, pal_.pulse, 0.3f);
            }
            s.age = 0.f;
            s.life = rng_.range(6.f, 10.f);
            s.seed = rng_.uniform();
            s.kind = 0;
            spores_.push_back(s);
        }
        for (Spore& s : spores_) {
            s.age += wdt;
            const float sc = s.size * 30.f;
            const vec3 turb(std::sin(time_ * 0.7f + s.seed * 40.f) * 0.5f + gust_ * 0.8f, std::sin(time_ * 0.5f + s.seed * 17.f) * 0.2f, 0.f);
            s.p = s.p + (s.v + turb * sc * 0.25f) * wdt;
            s.v = s.v * std::exp(-0.25f * wdt) + vec3(0.f, sc * 0.02f, 0.f) * wdt;
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
        if (m.kind == 1) a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.8f, 1.f, t)) * m.bright;
        sprite(m.p, m.size * (m.kind == 1 ? 2.f : 1.6f), m.col, a * world, 0);
    }
    {
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        sprite(V(gc.x, gc.y), layout::GAUGE_R * 1.25f, on ? pal_.threadTip : pal_.accent, 1.f, 1, fill, on ? 1.f : 0.f);
    }
    cells_.clear();
    buildBoard(game);

    // ---- Billboards, far to near.
    boards_.clear();
    buildFlora();
    buildShroomBoards();
    buildLandmarks();
    order_.clear();
    for (int i = 0; i < (int)boards_.size(); i++) order_.push_back({boards_[i].a[2], i});
    std::sort(order_.begin(), order_.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
    sorted_.clear();
    for (auto& o : order_) sorted_.push_back(boards_[o.second]);

    // =========================================================== uniforms shared by the world's passes
    const float bright = (0.42f + 0.08f * wm + 0.14f * wp + 0.08f * life + 0.12f * swell_ + 0.2f * tetrisGlow_) * pal_.glow *
                         (1.f - 0.8f * over_) * world;
    const float vh = view_.vh;
    const float dof = 0.035f * vh * (1.f - smoothstepf(std::log(0.4f), std::log(30.f), std::log(vh)));
    const float fogD = 70.f + 2.5f * vh;
    const vec3 mistC = pal_.mist * 1.5f * (0.9f + 0.1f * wm + 0.15f * wp + 0.08f * life);
    auto setCam = [&](GLuint p, float pxw) {
        set4f(p, "uCam", view_.cx, view_.cy, view_.vh, aspect_);
        set3f(p, "uEyeP", vec3(view_.ex, view_.ey, view_.D));
        set1f(p, "uPxW", pxw);
    };
    auto setLights = [&](GLuint p) {
        float lp[32] = {}, lc[24] = {};
        for (size_t i = 0; i < lights.size(); i++) {
            lp[i * 4 + 0] = lights[i].p.x; lp[i * 4 + 1] = lights[i].p.y; lp[i * 4 + 2] = lights[i].p.z; lp[i * 4 + 3] = lights[i].r;
            lc[i * 3 + 0] = lights[i].c.x; lc[i * 3 + 1] = lights[i].c.y; lc[i * 3 + 2] = lights[i].c.z;
        }
        glUniform4fv(ugl::U(p, "uLP"), 8, lp);
        glUniform3fv(ugl::U(p, "uLC"), 8, lc);
        set1i(p, "uNL", (int)lights.size());
        set3f(p, "uAmbient", pal_.ambient);
        set3f(p, "uMistC", mistC);
        set3f(p, "uMoonC", pal_.moon * (1.f - 0.3f * slow_));
        set1f(p, "uFogD", fogD);
        set1f(p, "uMistK", pal_.mistK);
        set1f(p, "uBridge", bridge);
        set3f(p, "uBridgeCol", bridge_.color);
    };
    auto setPulses = [&](GLuint p) {
        float pu[24] = {};
        for (size_t i = 0; i < pulses_.size() && i < 6; i++) {
            pu[i * 4 + 0] = pulses_[i].q; pu[i * 4 + 1] = pulses_[i].amp * world; pu[i * 4 + 2] = pulses_[i].frac; pu[i * 4 + 3] = pulses_[i].seed;
        }
        glUniform4fv(ugl::U(p, "uPulse"), 6, pu);
        set3f(p, "uWave", vec3(waveQ_, waveAmp_ * world, 0.32f));
        set3f(p, "uThreadC", pal_.thread);
        set3f(p, "uTipC", pal_.threadTip);
        set3f(p, "uPulseC", pal_.pulse);
        set1f(p, "uBright", bright);
        set1f(p, "uG", g_);
    };

    // =========================================================== the threads' light (quarter resolution)
    glDisable(GL_DEPTH_TEST);
    lightA_.bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    if (threadCount_ > 0) {
        glUseProgram(progThread_);
        setCam(progThread_, 2.f * vh / (float)lightA_.h);
        setPulses(progThread_);
        set1f(progThread_, "uMode", 1.f);
        set1f(progThread_, "uSurfPx", 2.f * vh / (float)lightA_.h * 2.f);
        glBindVertexArray(threadVao_);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, threadCount_);
    }
    glDisable(GL_BLEND);
    for (int pass = 0; pass < 2; pass++) {
        lightB_.bind();
        glUseProgram(progBlur_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, lightA_.tex);
        set1i(progBlur_, "uSrc", 0);
        set2f(progBlur_, "uDir", 1.f / lightA_.w, 0.f);
        ugl::drawFullscreen();
        lightA_.bind();
        glBindTexture(GL_TEXTURE_2D, lightB_.tex);
        set2f(progBlur_, "uDir", 0.f, 1.f / lightA_.h);
        ugl::drawFullscreen();
    }

    // =========================================================== the world (reduced resolution, with depth)
    worldT_.bind();
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    const float soilLight = 1.9f * pal_.soilLight * (0.85f + 0.1f * wm + 0.15f * wp + 0.1f * life) * (1.f - 0.6f * over_);
    {
        glUseProgram(progBg_);
        setCam(progBg_, pxW_);
        setLights(progBg_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, lightA_.tex);
        set1i(progBg_, "uLight", 0);
        set1f(progBg_, "uPx", pxW_);
        set1f(progBg_, "uTime", time_);
        set1f(progBg_, "uDof", dof);
        set1f(progBg_, "uSoilLight", soilLight);
        set1f(progBg_, "uNetR", std::exp(g_));
        set1f(progBg_, "uGlowAll", (0.7f + 0.2f * wm + 0.3f * wp + 0.3f * life) * (1.f - 0.8f * over_));
        set3f(progBg_, "uSkyTop", pal_.skyTop);
        set3f(progBg_, "uSkyLow", pal_.skyLow);
        set3f(progBg_, "uFloorC", pal_.floorC);
        set3f(progBg_, "uFloorLit", pal_.floorLit);
        set3f(progBg_, "uSoilA", pal_.soilA);
        set3f(progBg_, "uSoilB", pal_.soilB);
        set3f(progBg_, "uSoilC", pal_.soilC);
        set3f(progBg_, "uSoilD", pal_.soilD);
        set3f(progBg_, "uRock", pal_.rock);
        set3f(progBg_, "uSpeck", pal_.speck);
        set3f(progBg_, "uThread", pal_.thread);
        set2f(progBg_, "uMoonPos", 0.76f, 0.64f);
        ugl::drawFullscreen();
    }
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    // the roots in the cut
    if (rootCount_ > 0) {
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
        glUseProgram(progRoot_);
        setCam(progRoot_, pxW_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, lightA_.tex);
        set1i(progRoot_, "uLight", 0);
        set2f(progRoot_, "uRes", (float)worldT_.w, (float)worldT_.h);
        set1f(progRoot_, "uSoilLight", soilLight);
        set3f(progRoot_, "uRootC", pal_.rootC);
        set3f(progRoot_, "uAmbient", pal_.ambient);
        glBindVertexArray(rootVao_);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, rootCount_);
    }
    // the threads and their knots
    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
    if (threadCount_ > 0) {
        glUseProgram(progThread_);
        setCam(progThread_, pxW_);
        setPulses(progThread_);
        set1f(progThread_, "uMode", 0.f);
        set1f(progThread_, "uSurfPx", pxW_ * 1.5f);
        glBindVertexArray(threadVao_);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, threadCount_);
    }
    if (nodeCount_ > 0) {
        glUseProgram(progNode_);
        setCam(progNode_, pxW_);
        setPulses(progNode_);
        set1f(progNode_, "uTime", time_);
        glBindVertexArray(nodeVao_);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, nodeCount_);
    }
    // plants, trees and mushrooms behind the cut (depth tested: the cut and the floor in front hide them)
    glEnable(GL_DEPTH_TEST);
    if (!sorted_.empty()) {
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(progBoard_);
        setCam(progBoard_, pxW_);
        setLights(progBoard_);
        set1f(progBoard_, "uDof", dof);
        set1f(progBoard_, "uTime", time_);
        set1f(progBoard_, "uWind", wind);
        set1f(progBoard_, "uWindPhase", windPhase_);
        set3f(progBoard_, "uStemC", pal_.stem);
        set3f(progBoard_, "uFoliageC", pal_.foliage);
        set3f(progBoard_, "uBarkC", pal_.bark);
        set3f(progBoard_, "uRockC", pal_.rock);
        set3f(progBoard_, "uThreadC", pal_.thread);
        glBindVertexArray(boardVao_);
        glBindBuffer(GL_ARRAY_BUFFER, boardVbo_);
        glBufferData(GL_ARRAY_BUFFER, sorted_.size() * sizeof(Board), sorted_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sorted_.size());
    }
    // spores, the caps' halos in the mist, the network's heart
    {
        ginst_.clear();
        auto glow = [&](const vec3& p, float size, const vec3& c, float a, int kind, float seed) {
            if (a <= 0.004f || size <= 0.f) return;
            const float q[12] = {p.x, p.y, p.z, size, c.x, c.y, c.z, a, (float)kind, seed, 0.f, 0.f};
            ginst_.insert(ginst_.end(), q, q + 12);
        };
        const float wilt = smoothstepf(0.5f, 3.5f, overT_ >= 0.f ? overT_ : 0.f);
        for (const Mushroom& m : shrooms_) {
            if (!m.born) continue;
            const float H = shroomHeight(m) * (1.f - wilt);
            if (H * view_.D / (view_.D + m.z) / pxW_ < 4.f) continue;
            const float hs = m.type == 0 ? 0.8f : m.type == 1 ? 0.72f : 0.64f;
            const float rc = m.type == 0 ? 0.46f : m.type == 1 ? 0.17f : 0.36f;
            const float g = (0.08f + 0.03f * wp + 0.03f * life) * (1.f - 0.75f * over_) + m.flare * 0.14f + m.beat * 0.06f;
            glow(vec3(m.x, myc::surf(m.x) + H * (hs + 0.05f), m.z + 0.001f * H), H * rc * 2.2f, pal_.cap[m.color], g * world * smoothstepf(0.f, 1.f, m.age), 1, m.seed);
        }
        // the heart: where the threads are too fine to see, a knot of light
        {
            const float rr = 30.f * pxW_;
            glow(vec3(0.f, myc::SPORE_Y, 0.f), rr * 1.6f, lerp(pal_.thread, pal_.threadTip, 0.5f), 0.3f * bright, 2, 0.f);
        }
        ginst_.insert(ginst_.end(), markGlow_.begin(), markGlow_.end());
        for (const Spore& s : spores_) {
            if (s.age < 0.f) continue;
            const float t = s.age / s.life;
            const float a = smoothstepf(0.f, 0.1f, t) * (1.f - smoothstepf(0.6f, 1.f, t)) * (0.7f + 0.3f * std::sin(time_ * 2.f + s.seed * 30.f));
            glow(s.p, s.size, s.col, a * world, 0, s.seed);
        }
        if (!ginst_.empty()) {
            glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
            glUseProgram(progGlow_);
            setCam(progGlow_, pxW_);
            glBindVertexArray(glowVao_);
            glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
            glBufferData(GL_ARRAY_BUFFER, ginst_.size() * sizeof(float), ginst_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(ginst_.size() / 12));
        }
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
    set1f(progBright_, "uThreshold", 0.5f);
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
    set2f(progComp_, "uRes", (float)width_, (float)height_);
    set1f(progComp_, "uBloomK", (0.7f + 0.1f * wp + 0.08f * life) * world);
    set1f(progComp_, "uTwinkle", 0.25f + 0.15f * wm + 0.2f * wp);
    set1f(progComp_, "uTime", time_);
    set1f(progComp_, "uStars", pal_.stars * world);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    // ---- The veil behind the well and the HUD's columns.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progVeil_);
    set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
    set3f(progVeil_, "uRail", lerp(pal_.thread * 0.45f, bridge_.ink * 0.4f, bridge));
    set1f(progVeil_, "uWellA", 0.8f);
    set1f(progVeil_, "uColA", 0.56f * world);
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
        "MYCELIUM", "a glowing fungal network spreading through a cross-section of the forest floor at night, from a single spore to mushroom towers taller than the trees",
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
