// GIANT TREE: a world tree grows behind the well with every cleared line, absurdly: from a little round sapling in
// a bright meadow it shoots up past the hot-air balloons, through the clouds, into the stratosphere where the sky
// turns deep blue and the horizon curves, out into orbit among the satellites, then on until the planet is a ball
// under its roots, the moon floats by its crown and, far out, ringed planets and a galaxy.
//
// The growth is G = ln(the trunk's height in meters): 1.3 m at the first line, the clouds at ~10 lines, the
// stratosphere at ~16, orbit at ~26, the planet a ball at ~33, the moon at ~44, the ringed planet after it from ~47,
// the end of the journey at 50 (one song), and it keeps going (more slowly) to some two million kilometers. The tree
// is parametric (tree.cpp, TREE_GLSL): the trunk's rings are placed against
// G (thick at the foot, slender under the crown: it grows ever taller for its width), the crown is built in its own
// space and scaled on G (the sapling's lollipop head spreads into a broad crown over the first lines as its limbs
// are born, then it rides the trunk up), vine leaves spiral up the trunk sprouting as the tip passes. Each clear is
// a spurt: the growth springs to its new target, the crown squashes and stretches, sparkles burst out of it,
// streaks rush down past it, and the camera follows a moment later (the tree shoots up out of the frame first). A
// quad is a huge spurt. The camera rides with the crown: low in the grass by the sapling, then up with the crown
// looking down the soaring trunk at the land falling away, the clouds sinking under it, the planet curving, then
// a ball far below. Game over: every leaf falls; the next game sprouts a new seed (the camera dives back down).
//
// The look: a toon world in saturated colors: crisp bands of light with cool tinted shades, warm rims, round
// scalloped puffs of foliage with painterly leaf strokes, toon cumulus, a meadow in crisp two-tone patches with
// flowers and round trees, the aerial perspective in color (an exponential atmosphere: none of it in space). The
// sky, the planet (continents, oceans, cities on the night side, the limb's glow), the moon (a sphere floating
// behind the tree), the sun and space (nebula, a ringed planet, a galaxy) are one ray-traced pass. The music: calm
// is a light breeze; mid stirs the wind and drifts petals; a peak sways the tree, shimmers the leaves and fills the
// air with sparkles. **The drop** (a real one only, not a chorus) shoots the tree up an extra burst while a rainbow
// of light runs up the trunk from the roots, out through the crown and on up into space as a beam, and the crown
// blooms under a shower of rainbow petals.
//
// The pieces are seed glass: translucent glass shapes with a warm seed of light inside, fine leaf veins. Cleared rows
// dissolve into light that flows out of the well to the crown. Before a song the camera eases in to the tree from
// further back, turning a little around it; after it, it drifts back away from it.
//
// Cost: the world at a reduced resolution (at most 720 lines) with depth: one ray-traced sky pass (planet, moon,
// space), ~25k triangles of bark, ~1.5k puffs (most of them too small to draw at any time), ~1300 cloud puffs, a
// ray-traced cloud shell once above it; a quarter resolution bloom; the stars, the veil, the pieces and the
// lettering at full resolution. Nothing is uploaded while the tree grows (one upload per game).
//
// Layout contract: the board is drawn flat (orthographic at the reference camera's scale); only the world behind
// it has its own 3D camera.
#include "universes/gianttree/gianttree.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/gianttree/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Marcellus (the labels and numbers, carved capitals) and Cormorant Garamond Italic (the song, the universe's
// name, the notices), SIL Open Font License, see OFL-Marcellus.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/gianttree/Marcellus-Regular.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/gianttree/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = gianttree_shaders;
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
// A world shader: the light in both stages, the tree's shape in the vertex shader.
GLuint compileWorld(const char* vs, const char* fs, const char* name, bool tree) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, std::string(shaders::COMMON_GLSL) + shaders::LIGHT_GLSL);
    std::string v(vs);
    v.insert(v.find('\n') + 1, std::string(shaders::COMMON_GLSL) + shaders::LIGHT_GLSL + (tree ? shaders::TREE_GLSL : ""));
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 lerp2(vec2 a, vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): the sky (zenith, horizon), space and its nebula's two colors, the sun and its glow, the toon
// light (lit, shade), the haze, the rim; the meadow (light, dark), the ocean, the land, the deserts, the flowers;
// the bark (lit, dark), the leaves (mid, sunlit, shade), the blossoms, the fruits, the magic light, the clouds
// (lit, shade), the petals; the HUD's ink and accent, the veil; the seven pieces; the sun's place (azimuth from
// straight behind the tree, elevation), its size, night, stars, the leaves' own glow, how many blossoms, bloom.
struct VariantDef {
    const char* name;
    unsigned zenith, horizon, space, nebula, nebula2, sun, sunGlow, lit, shadow, haze, rim;
    unsigned groundA, groundB, ocean, land, desert, flower;
    unsigned bark, barkDark, leaf, leaf2, leafShade, blossom, fruit, glow, cloudLit, cloudShade, petal;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float sunAz, sunEl, sunSize, night, stars, leafGlow, blossomK, bloom;
};
const VariantDef VARIANTS[] = {
    // A bright day: a deep blue sky, a warm high sun, a vivid green crown dotted with pink blossoms, red apples.
    {"MEADOW", 0x1f6fe0, 0x9ce6ff, 0x03061a, 0x5a3cff, 0x20c0ff, 0xfff6dc, 0xffe49a, 0xfff2d6, 0x6f86d8, 0xa6e0ff, 0xffe8a0,
     0x86de4c, 0x4fb43a, 0x1aa6e0, 0x56c046, 0xe8cf72, 0xff7ab8,
     0xb0703e, 0x5c3456, 0x3fc23c, 0xc4f454, 0x1c7a64, 0xff92c6, 0xff3b30, 0xfff27a, 0xffffff, 0x9fb8f4, 0xffb4dc,
     0xffffff, 0xffe066, 0x0b2350,
     {0x5ee0ff, 0xffd84a, 0xc28cff, 0x6ef08a, 0xff6e8a, 0x5c8cff, 0xffa04a}, 2.45f, 0.72f, 0.03f, 0.f, 0.f, 0.f, 0.55f, 0.35f},
    // A golden sunset: an orange and rose sky, an autumn crown of gold and red, golden fruits, the sun low ahead.
    {"SUNSET", 0x3446b8, 0xffa860, 0x080418, 0xff5aa0, 0xffa040, 0xfff0c0, 0xff8a3c, 0xffd8a0, 0xa874c4, 0xffb88a, 0xffc070,
     0xb6cc4a, 0x6c9a3a, 0x2a7ec8, 0x7aaa48, 0xf0b860, 0xff6a5a,
     0xb85c38, 0x5a2a4c, 0xf59a30, 0xffdc5a, 0xc4544a, 0xff6a5a, 0xffe14a, 0xffd36a, 0xffe2c4, 0xb27ac4, 0xffa850,
     0xfff4e4, 0xffc070, 0x2a1030,
     {0x6cd8e0, 0xffd04a, 0xd890ff, 0xa8e05a, 0xff6a5a, 0x7c8cff, 0xff9a3a}, -1.45f, 0.14f, 0.034f, 0.f, 0.f, 0.f, 0.3f, 0.5f},
    // A vivid night: an ultramarine sky, the tree glowing turquoise with magenta blossoms, golden fruits, the stars.
    {"STARLIGHT", 0x0a1452, 0x2c50c0, 0x02030e, 0x8a3cff, 0x18b0ff, 0xd8e6ff, 0x6a8cff, 0xa8c0ff, 0x2c2c78, 0x2c4cb0, 0x9ae0ff,
     0x2a9a72, 0x145060, 0x0c3a90, 0x1e7060, 0x50609a, 0xff7ae8,
     0x7a64b8, 0x2a1c58, 0x22c8c8, 0x7cfff0, 0x1a4aa4, 0xff7ae8, 0xffe86a, 0x8affff, 0xa8bcff, 0x3c3c96, 0xd0a8ff,
     0xeef4ff, 0x8affff, 0x050a28,
     {0x5ef0ff, 0xffe46a, 0xd080ff, 0x5effb0, 0xff6ab8, 0x6a8cff, 0xffa060}, 2.2f, 0.5f, 0.03f, 1.f, 1.f, 0.55f, 0.5f, 0.6f},
    // A fantasy in pastels: a lilac sky, a pink cherry crown with white blossoms on a lavender trunk, mint meadows.
    {"CANDY", 0x7a6cf0, 0xffc4ea, 0x0a0620, 0xff6ad8, 0x7ae0ff, 0xfffaf0, 0xffcaf0, 0xfff0f6, 0x8a78d8, 0xffd0f0, 0xfff0a0,
     0x9cf0b8, 0x58c8a4, 0x6ad8f4, 0x9ce89a, 0xffe0a0, 0xffffff,
     0xc884b8, 0x6a4a9e, 0xff86c4, 0xffd0ea, 0xb054b4, 0xffffff, 0xffd84a, 0xfff0a0, 0xffffff, 0xc2acf2, 0xffc0e0,
     0xffffff, 0xfff0a0, 0x281850,
     {0x7ae8ff, 0xffe070, 0xd8a0ff, 0x8af0b0, 0xff8ab8, 0x8aa0ff, 0xffb070}, 2.7f, 0.62f, 0.03f, 0.f, 0.f, 0.15f, 0.65f, 0.4f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = GiantTreeUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.zenith = L(d.zenith); p.horizon = L(d.horizon); p.space = L(d.space); p.nebula = L(d.nebula); p.nebula2 = L(d.nebula2);
    p.sun = L(d.sun); p.sunGlow = L(d.sunGlow); p.lit = L(d.lit); p.shadow = L(d.shadow); p.haze = L(d.haze); p.rim = L(d.rim);
    p.groundA = L(d.groundA); p.groundB = L(d.groundB); p.ocean = L(d.ocean); p.land = L(d.land); p.desert = L(d.desert);
    p.flower = L(d.flower);
    p.bark = L(d.bark); p.barkDark = L(d.barkDark); p.leaf = L(d.leaf); p.leaf2 = L(d.leaf2); p.leafShade = L(d.leafShade);
    p.blossom = L(d.blossom); p.fruit = L(d.fruit); p.glow = L(d.glow); p.cloudLit = L(d.cloudLit);
    p.cloudShade = L(d.cloudShade); p.petal = L(d.petal); p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.sunAz = d.sunAz; p.sunEl = d.sunEl; p.sunSize = d.sunSize; p.night = d.night; p.stars = d.stars;
    p.leafGlow = d.leafGlow; p.blossomAmt = d.blossomK; p.bloom = d.bloom;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

constexpr float CLEAR_T = 0.38f;     // a cleared row dissolves into light in this long (real time)
constexpr float FLOW_T = 1.25f;      // its light flows to the tree in this long
constexpr int MAX_MOTES = 1600;
constexpr int MAX_PETALS = 1600;
constexpr float CLOUD_Y = 1900.f;    // the cloud layer (meters)
constexpr float CAM_AZ = 0.3f;       // the camera's side of the tree

// The journey: lines -> the trunk's height (meters), interpolated in log space; beyond the table, slower and slower.
// The whole journey fits in some 50 lines of this scale (scene completion: paceLines() rescales the real lines
// to the song's length, 50 for a 3:20 song): something new every five lines or so, the meadow's first
// stages a little quicker (some 2x taller a line), then about 1.4x a line from the clouds on.
struct Stage { float lines, height; };
const Stage STAGES[] = {
    {0, 1.3f}, {1.2f, 3.f}, {3.2f, 12.f}, {5.4f, 60.f}, {7.4f, 320.f}, {10, 2400.f}, {12.5f, 6.e3f}, {16, 1.5e4f},
    {19.5f, 3.5e4f}, {22.5f, 9.e4f}, {26, 3.5e5f}, {30, 1.5e6f}, {33.5f, 6.e6f}, {37, 2.5e7f}, {41.5f, 1.e8f},
    {45, 3.6e8f}, {50, 1.5e9f},
};
constexpr int STAGE_COUNT = (int)(sizeof(STAGES) / sizeof(STAGES[0]));

// Test hooks: REFALL_GIANTTREE_LINES=N adds N lines to the growth, REFALL_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("REFALL_GIANTTREE_LINES") ? std::atoi(std::getenv("REFALL_GIANTTREE_LINES")) : 0;
    static const double rate = std::getenv("REFALL_GROW_RATE") ? std::atof(std::getenv("REFALL_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool GiantTreeUniverse::init(int width, int height) {
    progSky_ = compileWorld(ugl::FULLSCREEN_VS, shaders::SKY_FS, "gianttree sky", false);
    progBark_ = compileWorld(shaders::BARK_VS, shaders::BARK_FS, "gianttree bark", true);
    progLeaf_ = compileWorld(shaders::LEAF_VS, shaders::LEAF_FS, "gianttree leaf", true);
    progCloud_ = compileWorld(shaders::CLOUD_VS, shaders::CLOUD_FS, "gianttree cloud", false);
    progSea_ = compileWorld(ugl::FULLSCREEN_VS, shaders::SEA_FS, "gianttree sea", false);
    progGlow_ = compile(shaders::GLOW_VS, shaders::GLOW_FS, "gianttree glow");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "gianttree comp");
    progBright_ = compile(ugl::FULLSCREEN_VS, shaders::BRIGHT_FS, "gianttree bright");
    progBlur_ = compile(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "gianttree blur");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "gianttree sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "gianttree veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "gianttree cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "gianttree shape");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "gianttree post");
    progUp_ = compile(ugl::FULLSCREEN_VS, ugl::UPSAMPLE_DEPTH_FS, "gianttree upsample");

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
    instanced(leafVao_, leafVbo_, 4, sizeof(float) * 16);
    instanced(glowVao_, glowVbo_, 3, sizeof(float) * 12);
    instanced(cloudVao_, cloudVbo_, 2, sizeof(float) * 8);
    instanced(spriteVao_, spriteVbo_, 2, sizeof(SpriteInst));
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));
    // the bark: five vec4 a vertex, indexed
    glGenVertexArrays(1, &barkVao_);
    glGenBuffers(1, &barkVbo_);
    glGenBuffers(1, &barkIbo_);
    glBindVertexArray(barkVao_);
    glBindBuffer(GL_ARRAY_BUFFER, barkVbo_);
    for (int i = 0; i < 5; i++) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 20, (void*)(sizeof(float) * 4 * i));
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, barkIbo_);
    // shapes and text (pixels)
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    buildMeadow();
    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

// Grass tufts and wildflowers around the sapling's foot (meters).
void GiantTreeUniverse::buildMeadow() {
    Rng tr(0x6A55ull);
    tufts_.clear();
    for (int i = 0; i < 1100; i++) {
        const float a = tr.range(0, TAU), d = 0.12f + 4.5f * std::pow(tr.uniform(), 0.8f);
        Tuft t;
        t.p = vec3(std::cos(a) * d, 0.f, std::sin(a) * d);
        t.h = tr.range(0.05f, 0.12f) * (0.7f + 0.3f * std::min(1.f, d / 2.f));
        t.seed = tr.uniform();
        t.flower = tr.chance(0.07f) ? 1.f : 0.f;
        if (t.flower > 0.5f) t.h *= 1.3f;
        tufts_.push_back(t);
    }
}

void GiantTreeUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    aspect_ = aspect;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the sky at a reduced resolution (at most 720 lines), with its depth
    const int sh = std::max(180, std::min(720, (int)(height_ * 0.66f)));
    const int sw = std::max(320, (int)std::lround((float)sh * width_ / height_));
    skyT_.create(sw, sh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    skyT_.attachDepthTexture();
    // the meshes: multisampled (4x) at the full resolution, up to 1440 lines
    const int wh = std::max(180, std::min(1440, height_));
    const int ww = std::max(320, (int)std::lround((float)wh * width_ / height_));
    msT_.create(ww, wh, 4);
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    // the bloom from the sky's resolution / 4, as before
    bloomA_.create(std::max(16, sw / 4), std::max(16, sh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(std::max(16, sw / 4), std::max(16, sh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GiantTreeUniverse::~GiantTreeUniverse() {
    for (ugl::Target* t : {&skyT_, &worldT_, &bloomA_, &bloomB_, &sceneT_}) t->destroy();
    msT_.destroy();
    for (GLuint p : {progSky_, progSea_, progBark_, progLeaf_, progGlow_, progCloud_, progComp_, progBright_, progBlur_,
                     progSprite_, progVeil_, progCell_, progShape_, progPost_, progUp_})
        glDeleteProgram(p);
    for (GLuint v : {barkVao_, leafVao_, glowVao_, cloudVao_, spriteVao_, cellVao_, shapeVao_})
        glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, barkVbo_, barkIbo_, leafVbo_, glowVbo_, cloudVbo_, spriteVbo_,
                     cellVbo_, shapeVbo_})
        glDeleteBuffers(1, &b);
}

// =====================================================================================================
// The tree
// =====================================================================================================

// Lines -> G: the journey's table, then a slow climb on (some 30% taller every ten lines), up to G_MAX.
float GiantTreeUniverse::growthOf(float lines) const {
    const float l = std::max(0.f, lines);
    float h;
    if (l >= STAGES[STAGE_COUNT - 1].lines) {
        h = std::log(STAGES[STAGE_COUNT - 1].height) + (l - STAGES[STAGE_COUNT - 1].lines) * 0.026f;
    } else {
        int i = 0;
        while (i + 2 < STAGE_COUNT && STAGES[i + 1].lines <= l) i++;
        const float t = (l - STAGES[i].lines) / (STAGES[i + 1].lines - STAGES[i].lines);
        h = lerpf(std::log(STAGES[i].height), std::log(STAGES[i + 1].height), t);
    }
    return std::min(h, gtree::G_MAX);
}

vec3 GiantTreeUniverse::crownCenter(float G) const { return gtree::tip(G, tree_.wob) + tree_.crownC * gtree::crownW(G); }

// The crown's horizontal radius (meters): only the dome on the sapling, its limbs spreading over the first lines.
float GiantTreeUniverse::crownRadius(float G) const {
    return gtree::crownW(G) * lerpf(0.36f, tree_.crownR, smoothstepf(0.3f, 3.3f, G));
}

void GiantTreeUniverse::uploadTree() {
    tree_ = buildGiantTree(treeSeed_);
    glBindVertexArray(barkVao_);
    glBindBuffer(GL_ARRAY_BUFFER, barkVbo_);
    glBufferData(GL_ARRAY_BUFFER, tree_.bark.size() * sizeof(float), tree_.bark.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, barkIbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, tree_.index.size() * sizeof(uint32_t), tree_.index.data(), GL_STATIC_DRAW);
    barkCount_ = (GLsizei)tree_.index.size();
    glBindBuffer(GL_ARRAY_BUFFER, leafVbo_);
    glBufferData(GL_ARRAY_BUFFER, tree_.puffs.size() * sizeof(float), tree_.puffs.data(), GL_STATIC_DRAW);
    leafCount_ = (GLsizei)(tree_.puffs.size() / 16);
    glBindVertexArray(0);
    // the clouds: toon cumulus scattered over the country (fewer right around the trunk on the camera's side)
    Rng r(treeSeed_ ^ 0xC10Dull);
    puffs_.clear();
    for (int c = 0; c < 190; c++) {
        const int band = c < 40 ? 0 : c < 140 ? 1 : 2;
        float a = r.range(0, TAU);
        const float d = band == 0 ? r.range(300.f, 3000.f) : band == 1 ? r.range(3000.f, 16000.f) : r.range(16000.f, 45000.f);
        if (band == 0 && std::fabs(std::remainder(a - CAM_AZ, TAU)) < 0.9f) a += PI;
        const float big = (band == 0 ? r.range(90.f, 220.f) : band == 1 ? r.range(180.f, 420.f) : r.range(400.f, 900.f));
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
            puffs_.push_back(p);
        }
    }
    treeDirty_ = false;
}

// =====================================================================================================
// Camera
// =====================================================================================================

// Rides with the crown, a moment behind it: low in the grass by the sapling at the right of the scene (the whole
// little tree in view), then rising with the crown and looking down along the soaring trunk at the land falling
// away, the clouds sinking under it, the horizon curving, the planet a ball far below. Only this camera moves: the
// board is drawn on its own. The world is drawn relative to the eye (vp3_ has the eye at its origin).
void GiantTreeUniverse::updateCamera(float dt) {
    if (gc_ < -50.f) { gc_ = g_; gcVel_ = 0.f; }
    {
        // a critically damped spring, slower than the growth's (partly fed its speed): the crown shoots up, we follow
        const float w = 4.f;
        gcVel_ += (w * w * (g_ - gc_) + 2.f * w * (0.9f * gVel_ - gcVel_)) * dt;
        gc_ += gcVel_ * dt;
    }
    const float G = gc_;
    const float T = std::exp(G);
    const vec3 cc = crownCenter(G);
    const float Rc = crownRadius(G);
    const float W = gtree::crownW(G);
    const float kF = smoothstepf(1.6f, 5.5f, G);      // the ground -> riding with the crown
    const float kH = smoothstepf(8.5f, 14.f, G);      // high up: looking down along the trunk
    // the planet a ball under the roots: look straight down the trunk at it; then up again at the moon, the stars
    const float kP = smoothstepf(13.f, 16.f, G) * (1.f - smoothstepf(17.5f, 19.5f, G));
    tanH_ = std::tan(lerpf(26.f, 29.f, kF) * PI / 180.f);
    // The camera's direction first (its azimuth on the tree's side, its pitch with the stage), then its place: the
    // anchor (the whole tree by the ground, then the crown) sits at a fixed point of the screen, right of the well,
    // at the distance that frames it.
    const float top = T + 0.55f * W;
    const vec3 aGround(0.f, 0.47f * top, 0.f);
    const float FG = 0.68f * top + 0.15f;
    const float FF = lerpf(3.0f, 3.3f, kH) * Rc * lerpf(1.f, 1.15f, kP);
    const vec3 anchor = lerp(aGround, cc, kF);
    const float F = lerpf(FG, FF, kF);
    const float sx = lerpf(0.62f, 0.64f, kF), sy = lerpf(0.f, 0.36f, kF) + 0.1f * kP;
    const float kD = smoothstepf(18.f, 20.f, G);      // deep space: looking out at the moon, the planets
    const float pitch = (lerpf(4.f, -15.f, kF) - 12.f * kH * (1.f - kD) - 40.f * kP) * PI / 180.f;
    const float az = CAM_AZ + 0.04f * std::sin(time_ * 0.023f) + camSwing_;
    // looking toward the tree (-z, turned by the azimuth), then turned left so the anchor lands right of the well
    const float yaw = std::atan(sx * tanH_ * aspect_ / std::cos(pitch));
    const float phi = az + yaw;
    vec3 fwd(-std::sin(phi) * std::cos(pitch), std::sin(pitch), -std::cos(phi) * std::cos(pitch));
    vec3 right = normalize(cross(fwd, vec3(0, 1, 0)));
    vec3 upv = cross(right, fwd);
    const vec3 dirA = normalize(fwd + right * (sx * tanH_ * aspect_) + upv * (sy * tanH_));
    const float D = F / tanH_ * (1.f + camBack_);
    vec3 eye = anchor - dirA * D;
    eye.y = std::max(eye.y, 0.25f + 0.2f * std::min(T, 4.f));
    eye_ = eye;
    look_ = fwd;
    camR_ = normalize(cross(fwd, vec3(0, 1, 0)));
    camU_ = cross(camR_, fwd);
    {
        const vec3 oc(eye.x, eye.y + gtree::R_EARTH, eye.z);
        alt_ = std::max(0.f, length(oc) - gtree::R_EARTH);
        if (length(oc) < 1e7f) alt_ = std::max(0.f, eye.y + (eye.x * eye.x + eye.z * eye.z) / (2.f * gtree::R_EARTH));
    }
    const float zn = std::max(0.02f, D * 0.02f), zf = std::max({1.6e5f, D * 80.f, T * 3.f});
    const mat4 view3 = mat4::lookAt(vec3(0.f), fwd, vec3(0, 1, 0));
    proj3_ = mat4::perspective(2.f * std::atan(tanH_), aspect_, zn, zf);
    vp3_ = proj3_ * view3;
}

vec2 GiantTreeUniverse::toBoard(const vec3& w) const {
    const vec4 c = vp3_ * vec4(w - eye_, 1.f);
    if (c.w <= 1e-4f) return V(viewC_.x + viewH_.x, viewC_.y);
    return V(viewC_.x + c.x / c.w * viewH_.x, viewC_.y + c.y / c.w * viewH_.y);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void GiantTreeUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && !t.minor && t.brightness > 0.58f) v = 0;          // the bright day for bright, major songs
        else if (t.hasSong && t.minor && t.brightness < 0.42f) v = 2;      // the glowing night for dark ones
        else if (t.hasSong && t.airWeight > 0.62f && t.density < 0.45f) v = 3; // the pastel fantasy for airy, sparse ones
        else {
            static const int BY_HUE[] = {1, 0, 2, 1, 3, 0};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x77EEull);
    if (treeSeed_ == 0) { treeSeed_ = seed_; treeDirty_ = true; }
    if (seconds <= 0.f) palSet_ = false;
}

void GiantTreeUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

// The drop: the light flows up the tree (only on a real drop: see render).
void GiantTreeUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void GiantTreeUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 GiantTreeUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints GiantTreeUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.zenith, p.horizon, 0.45f) * (p.night > 0.5f ? 1.f : 0.75f);
    h.light = false;
    h.ink = p.ink;
    h.accent = p.glow;
    h.styles = TS_INK | TS_LUMA | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void GiantTreeUniverse::syncIds(const Game& g) {
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

void GiantTreeUniverse::addMote(const Mote& m) {
    if ((int)motes_.size() >= MAX_MOTES) return;
    motes_.push_back(m);
}

void GiantTreeUniverse::onEvent(const GameEvent& ev, const Game& game) {
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
        // the cells dissolve into light that flows out of the well to the tree's crown
        vec2 to = toBoard(crownCenter(g_));
        to.x = std::clamp(to.x, -viewH_.x + 1.f, viewH_.x - 1.f);
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
                mt.to = add(to, V(rng_.range(-1.5f, 1.5f), rng_.range(-1.2f, 1.2f)));
                mt.ctl = V(side * rng_.range(6.f, 8.f), std::max(mt.from.y, mt.to.y) + rng_.range(1.f, 4.f));
                mt.p = mt.from;
                mt.life = FLOW_T * rng_.range(0.8f, 1.1f);
                mt.age = -rng_.range(0.f, 0.25f) - (float)(side > 0 ? Game::W - 1 - cl.x : cl.x) * 0.015f;
                mt.size = rng_.range(0.1f, 0.17f);
                mt.col = lerp(pal_.piece[std::clamp(cl.type, 0, 6)], pal_.glow, 0.5f);
                mt.bright = 1.2f;
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
        // a spurt: the crown squashes and stretches, sparkles and leaves burst out of it
        spurt_ = std::max(spurt_, 0.4f + 0.15f * lines);
        jellyV_ += 1.6f + 0.9f * lines;
        {
            const float W = gtree::crownW(gTarget_), Rc = crownRadius(gTarget_);
            const vec3 cc = crownCenter(g_);
            const int n = lines >= 4 ? 46 : 7 * lines;
            for (int i = 0; i < n && (int)petals_.size() < MAX_PETALS; i++) {
                Petal q;
                const vec3 dir = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-0.3f, 1.f), rng_.range(-1.f, 1.f)));
                q.p = cc + dir * Rc * rng_.range(0.4f, 0.9f);
                q.v = dir * Rc * rng_.range(0.6f, 1.6f) + vec3(0.f, Rc * 0.5f, 0.f);
                q.age = -rng_.range(0.f, 0.3f);
                q.life = rng_.range(0.7f, 1.3f);
                q.size = W * rng_.range(0.035f, 0.07f) * (lines >= 4 ? 1.4f : 1.f);
                q.seed = rng_.uniform();
                q.spin = 0.f;
                q.kind = 4;
                q.col = i % 3 == 0 ? pal_.glow * 1.6f : i % 3 == 1 ? lerp(pal_.blossom, vec3(1.f), 0.3f) * 1.4f : lerp(pal_.leaf2, vec3(1.f), 0.4f) * 1.3f;
                petals_.push_back(q);
            }
            petalAcc_ += 8.f * lines;
        }
        if (lines >= 4) {
            quadGlow_ = 1.f;
            bloomBurst_ = 1.f;
            if (flowT_ > 3.f) { flowT_ = 0.f; flowDur_ = 1.6f; flowK_ = 0.8f; }
            petalAcc_ += 110.f;
        } else if (flowT_ > 2.5f) {
            flowT_ = 0.f;
            flowDur_ = 1.3f;
            flowK_ = 0.15f + 0.08f * lines;
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
        petalAcc_ += 160.f;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void GiantTreeUniverse::cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase,
                             float shine) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, shine, 0.f}});
}

void GiantTreeUniverse::buildBoard(const Game& g) {
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
            cell(p.x, p.y, 0.5f, maskOf(x, y, same), tintOf(c.type), 1.f, 0, 0.f, 0.f);
        }
    // game over: the stack's light drains from the top down
    if (overT_ >= 0.f)
        for (const Fading& o : overCells_) {
            const float rowFrac = (o.t - Game::HIDDEN) / (float)(Game::H - Game::HIDDEN);
            const float k = smoothstepf(rowFrac * 0.6f, rowFrac * 0.6f + 0.3f, over_);
            cell(o.x, o.y, 0.5f, o.mask, tintOf(o.type), 1.f - 0.5f * smoothstepf(0.7f, 1.f, over_), 0, 0.f, 0.f);
            cells_.back().c[3] = k;
        }
    for (const Fading& f : fading_) cell(f.x, f.y, 0.5f, f.mask, tintOf(f.type), 1.f, 0, saturate(f.t / CLEAR_T), 0.f);
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
                cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), tintOf(p.type), 0.5f, 1, 0.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), tintOf(p.type), 1.f, 0, 0.f, 0.f);
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
            cell(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), tintOf(type), alpha, 0, 0.f, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void GiantTreeUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}});
}

// =====================================================================================================
// The HUD
// =====================================================================================================

void GiantTreeUniverse::drawHud(const std::vector<HudRect>& panels) {
    if (panels.empty()) return;
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

// The lettering, by the host (text.hpp): Marcellus capitals, widely spaced, the song, the variant and the notices
// in Cormorant italics; each line on a soft halo of the sky's veil (fainter at night), in a glow of its ink. The
// inks are the scene's (linear), toned as the post tones the scene: dimmed, their highlights rolled off, to sRGB.
void GiantTreeUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* caps = text::font(FONT_CAPS);
    text::Font* ital = text::font(FONT_ITAL);
    const float s = height_ / 1080.f;
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount), dim = 1.f - dimT_ * 0.5f;
    auto tone = [&](const vec3& c0) {
        vec3 c = c0 * dim;
        for (float* v : {&c.x, &c.y, &c.z}) *v /= 1.f + std::max(*v - 0.75f, 0.f) * 0.7f;
        return ugl::toSrgb(c);
    };
    auto inkOf = [&](const vec3& c) { return tone(lerp(c, bridge_.ink, b)); };
    const vec3 ink = inkOf(pal_.ink), accent = inkOf(pal_.accent);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    bool full = false; // the fuller halo
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        // the name in spaced capitals, the variant in italics after it, a little higher
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        size_t n = 1;
        while (n < var.size() && ((unsigned char)var[n] & 0xC0) == 0x80) n++;
        if (!var.empty()) var = var.substr(0, n) + text::lower(var.substr(n));
        const float sz = base * 0.95f, tr = sz * 0.28f, vsz = base * 0.95f * 1.3f, gap = base * 0.7f;
        const float a = saturate(0.35f + h.alpha * 1.6f);
        put(caps, name, sz, tr, ink, a);
        if (!var.empty()) {
            // (the name's spacing stays after its last letter: the gap takes it back; each run was placed by its own
            // cap line)
            text::Span& v = out.add(var, ital, vsz, accent, 0.f, a * 0.9f);
            v.gap = (gap - tr) / vsz;
            v.dy = -sz * 0.15f + text::capHeight(ital) * vsz - text::capHeight(caps) * sz;
        }
        break;
    }
    case HUD_SECTION: put(caps, h.text, base * 0.85f, base * 0.5f, ink, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(ital, h.text, base * 1.5f, base * 0.02f, ink, h.alpha * 1.3f, -base * 0.25f); break;
    case HUD_INFO: put(caps, h.text, base * 0.68f, base * 0.22f, ink, saturate(alpha * 1.3f) * 0.85f); break;
    case HUD_LABEL: put(caps, h.text, base * 0.7f, base * 0.42f, inkOf(lerp(pal_.ink, pal_.accent, 0.45f)), saturate(alpha * 1.2f) * 0.92f); break;
    case HUD_VALUE: put(caps, h.text, base * 1.08f, base * 0.08f, ink, alpha); break;
    case HUD_NOTICE: put(ital, h.text, base * 1.4f, base * 0.02f, accent, alpha); break;
    case HUD_KEY: put(caps, h.text, base * 0.78f, base * 0.1f, ink, alpha); break;
    case HUD_TOAST: put(caps, h.text, base * 1.2f, base * 0.5f, ink, alpha, -base * 0.15f); full = true; break;
    case HUD_TOAST_SUB: put(ital, h.text, base * 1.25f, base * 0.02f, accent, alpha * 0.9f); full = true; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(caps, h.text, base * 1.1f, base * 0.2f, ink, alpha, -base * 0.1f);
        else put(caps, h.text, base * 0.78f, base * 0.12f, h.accent ? accent : ink, alpha);
        full = true;
        break;
    default: put(caps, h.text, base * 0.78f, base * 0.1f, h.accent ? accent : ink, alpha); break;
    }
    // the letters as the scene drew them, on a halo of the veil, in a glow (gone with the bridge)
    text::Lettering& l = lettering_[full ? 1 : 0];
    l.glsl = shaders::LETTER_GLSL;
    l.p0 = vec4(tone(lerp(pal_.veil * 0.6f, bridge_.color, b)), full ? 0.85f : pal_.night > 0.5f ? 0.6f : 0.8f);
    l.p1 = vec4(0.45f * (1.f - b), ugl::luma(lerp(pal_.veil, bridge_.color, b)) * dim, 0.f, 0.f);
    out.lettering = &l;
    out.look.weight = 0.2f / std::max(1.f, out.spans[0].size); // (small letters: as bold as linear blending made them)
    out.alpha = saturate(fade_);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void GiantTreeUniverse::render(const Frame& f, GLuint fbo) {
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

    // ---- The game's lines -> the growth (a new game sprouts a new seed).
    {
        // (on the 50-line scale: the song's pace, paceLines; a break: the growth as it was)
        const float lines = f.bare ? lines_ : paceLines(game, (float)extraLines(time_));
        if (treeSeed_ == 0) { treeSeed_ = seed_ ? seed_ : 1; treeDirty_ = true; }
        if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
            treeSeed_ = splitmix64(treeSeed_ + 0x9E37ull);
            treeDirty_ = true;
            gBonus_ = 0.f;
            g_ = growthOf(lines);
            gVel_ = 0.f;
            shed_ = 0.f;
            seedT_ = 0.f;
            petals_.clear();
        }
        lines_ = lines;
        if (treeDirty_) uploadTree();
        if (isDrop && !wasDrop_) {
            // the drop shoots the tree up an extra burst
            gBonus_ += 0.45f;
            jellyV_ += 5.f;
            petalAcc_ += 160.f;
        }
        gTarget_ = std::min(gtree::G_MAX, growthOf(lines) + gBonus_);
        if (linesShown_ < 0) { g_ = gTarget_; gVel_ = 0.f; }
        linesShown_ = lines;
        // a spurt: a critically damped spring toward the target, over a second or so
        const float w = 4.2f;
        gVel_ += (w * w * (gTarget_ - g_) - 2.f * w * gVel_) * wdt;
        // capped (a line is a big step now the journey fits in one song): a quad in the meadow takes a breath
        // longer instead of jolting the view
        gVel_ = std::min(gVel_, 3.f);
        g_ += gVel_ * wdt;
        if (g_ > gTarget_ + 0.001f && gVel_ > 0.f) { g_ = gTarget_; gVel_ = 0.f; }
        seedT_ += dt;
        // the crown's squash and stretch (a springy wobble)
        jellyV_ += (-90.f * jelly_ - 7.f * jellyV_) * wdt;
        jelly_ += jellyV_ * wdt;
        jelly_ = std::clamp(jelly_, -0.25f, 0.3f);
    }
    shed_ = approach(shed_, overT_ >= 0.f ? 1.f : 0.f, overT_ >= 0.f ? 0.6f : 3.f, dt);
    spurt_ = approach(spurt_, 0.f, 1.2f, dt);
    bloomBurst_ = approach(bloomBurst_, 0.f, 0.7f, dt);

    if (isDrop && !wasDrop_) {
        flowT_ = 0.f;
        flowDur_ = 2.4f;
        flowK_ = 1.f;
        gust_ = 1.f;
    }
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.5f : 0.4f, dt);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    flowT_ += dt;
    const float flow = flowT_ / flowDur_ * 1.8f;
    flowK_ = approach(flowK_, 0.f, flowT_ > flowDur_ * 1.4f ? 0.6f : 0.03f, dt);
    beam_ = approach(beam_, isDrop && flow > 1.3f ? 1.f : 0.f, isDrop ? 2.f : 0.5f, dt);
    gust_ = approach(gust_, 0.f, 0.45f, dt);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        if (slow_ > 0.01f) {
            // slow motion: a hush, the light softer and bluer
            const float k = slow_;
            pal_.sunGlow = lerp(pal_.sunGlow, pal_.sunGlow * vec3(0.8f, 0.9f, 1.2f), k);
            pal_.haze = lerp(pal_.haze, pal_.haze * vec3(0.85f, 0.9f, 1.1f), k);
            pal_.lit = lerp(pal_.lit, pal_.lit * vec3(0.9f, 0.95f, 1.1f), k);
        }
    }
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    quadGlow_ = approach(quadGlow_, 0.f, 0.4f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    kickGlow_ = std::max(approach(kickGlow_, 0.f, 4.f, dt), kick * wp);
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    float beatPulse = music.hasSong ? music.beatPulse : 0.f;
    if (!f.paused && !music.hasSong) {
        beatClock_ += dt;
        if (beatClock_ > 0.5f) beatClock_ -= 0.5f;
        beatPulse = std::max(0.f, 1.f - beatClock_ * 4.f);
    }
    shimmer_ = (0.15f * wm + 0.5f * wp + 0.4f * life) * (0.7f + 0.3f * beatPulse);

    // ---- The wind: a breeze, stirring at mid, swaying at a peak.
    const float wind = 0.35f + 0.45f * wm + 0.7f * wp + 0.3f * life;
    windPhase_ += wdt * (0.7f + 0.3f * wm + 0.35f * wp);

    // the setup: the camera eases in to the tree, turning a little around it; the teardown: it drifts back, away
    {
        const float in = 1.f - smoothstepf(0.f, 0.9f, f.setup), out = smoothstepf(0.f, 1.f, f.teardown);
        camBack_ = 0.7f * in * in + 0.9f * out * out;
        camSwing_ = -0.3f * in * in + 0.2f * out;
    }
    updateCamera(dt);
    const float W = gtree::crownW(g_);
    const float Rc = crownRadius(g_);
    const vec3 cc = crownCenter(g_);
    const vec3 tipW = gtree::tip(g_, tree_.wob);
    const float spaceK = smoothstepf(1.2e4f, 9.e4f, alt_);

    // ---- Particles in the world (meters): petals, sparkles, streaks, rainbow petals.
    {
        auto crownPoint = [&](vec3* p) {
            const int n = (int)(tree_.puffs.size() / 16);
            for (int tries = 0; tries < 8 && n > 0; tries++) {
                const float* l = &tree_.puffs[(size_t)rng_.irange(0, n - 1) * 16];
                if (l[4] != 0.f || l[5] > g_) continue;
                *p = tipW + vec3(l[0], l[1], l[2]) * W + vec3(rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f)) * (l[3] * W);
                return true;
            }
            return false;
        };
        const float ps = W * 0.022f;
        const float petalRate = 1.f + 3.f * wm + 7.f * wp + 50.f * life + 30.f * bloomBurst_ + (overT_ >= 0.f && overT_ < 4.f ? 70.f : 0.f);
        const float sporeRate = (1.f + 3.f * wm + 10.f * wp + 16.f * life) + 10.f * pal_.night;
        if (!f.paused) {
            petalAcc_ += petalRate * wdt;
            sporeAcc_ += sporeRate * wdt;
            streakAcc_ += std::min(std::max(0.f, gVel_ - 0.05f) * 160.f, 140.f) * wdt;
        }
        petalAcc_ = std::min(petalAcc_, 320.f);
        while (petalAcc_ >= 1.f && (int)petals_.size() < MAX_PETALS) {
            petalAcc_ -= 1.f;
            vec3 p;
            if (!crownPoint(&p)) break;
            Petal q;
            q.p = p;
            q.v = vec3(rng_.range(0.3f, 1.2f), -rng_.range(0.5f, 1.3f), rng_.range(-0.4f, 0.4f)) * (W * 0.12f);
            q.age = 0.f;
            q.life = rng_.range(5.f, 9.f);
            q.size = ps * rng_.range(0.7f, 1.2f);
            q.seed = rng_.uniform();
            q.spin = rng_.range(-3.f, 3.f);
            q.kind = (life > 0.3f && rng_.chance(0.7f)) || bloomBurst_ > 0.3f ? 3 : 0;
            q.col = q.kind == 3 ? lerp(ugl::toLinear(hex(0xff4a6a)), vec3(1.f), 0.f) : pal_.petal;
            if (q.kind == 3) {
                // a rainbow
                const float h = rng_.uniform() * 6.f;
                const float r = saturate(std::fabs(h - 3.f) - 1.f), g = saturate(2.f - std::fabs(h - 2.f)), b = saturate(2.f - std::fabs(h - 4.f));
                q.col = lerp(vec3(r, g, b), vec3(1.f), 0.25f) * 1.4f;
            }
            petals_.push_back(q);
        }
        sporeAcc_ = std::min(sporeAcc_, 60.f);
        while (sporeAcc_ >= 1.f && (int)petals_.size() < MAX_PETALS) {
            sporeAcc_ -= 1.f;
            vec3 p;
            if (!crownPoint(&p)) break;
            Petal q;
            q.p = p + vec3(rng_.range(-0.5f, 0.5f), rng_.range(-0.4f, 0.1f), rng_.range(-0.5f, 0.5f)) * W;
            q.v = vec3(rng_.range(-0.2f, 0.4f), rng_.range(0.3f, 0.8f), rng_.range(-0.2f, 0.2f)) * (W * 0.06f);
            q.age = 0.f;
            q.life = rng_.range(4.f, 8.f);
            q.size = ps * rng_.range(0.4f, 0.7f);
            q.seed = rng_.uniform();
            q.spin = 0.f;
            q.kind = 1;
            q.col = lerp(pal_.glow, pal_.fruit, q.seed * 0.4f);
            petals_.push_back(q);
        }
        // the drop's beam: sparks of every color rushing up out of the crown
        for (int i = 0; i < (int)(beam_ * 3.f + rng_.uniform()) && (int)petals_.size() < MAX_PETALS && !f.paused; i++) {
            Petal q;
            const float a = rng_.range(0.f, TAU), d = Rc * 0.3f * std::sqrt(rng_.uniform());
            q.p = cc + vec3(std::cos(a) * d, Rc * rng_.range(0.f, 0.5f), std::sin(a) * d);
            q.v = vec3(0.f, Rc * rng_.range(2.f, 5.f), 0.f);
            q.age = 0.f;
            q.life = rng_.range(1.2f, 2.f);
            q.size = Rc * rng_.range(0.04f, 0.08f);
            q.seed = rng_.uniform();
            q.spin = 0.f;
            q.kind = 4;
            const float h = rng_.uniform() * 6.f;
            q.col = lerp(vec3(saturate(std::fabs(h - 3.f) - 1.f), saturate(2.f - std::fabs(h - 2.f)), saturate(2.f - std::fabs(h - 4.f))), vec3(1.f), 0.3f) * 1.6f;
            petals_.push_back(q);
        }
        // streaks rushing down past a spurt
        streakAcc_ = std::min(streakAcc_, 40.f);
        while (streakAcc_ >= 1.f && (int)petals_.size() < MAX_PETALS) {
            streakAcc_ -= 1.f;
            Petal q;
            const float a = rng_.range(0.f, TAU), d = Rc * rng_.range(0.6f, 2.6f);
            q.p = tipW + vec3(std::cos(a) * d, rng_.range(-2.5f, 0.3f) * Rc, std::sin(a) * d);
            q.v = vec3(0.f, -W * 0.8f, 0.f);
            q.age = 0.f;
            q.life = rng_.range(0.35f, 0.7f);
            q.size = Rc * rng_.range(0.03f, 0.05f);
            q.seed = rng_.uniform();
            q.spin = 0.f;
            q.kind = 5;
            q.col = lerp(vec3(1.f), pal_.glow, 0.3f);
            petals_.push_back(q);
        }
        for (Petal& q : petals_) {
            q.age += wdt;
            const float fl = std::sin(time_ * 1.7f + q.seed * 40.f);
            q.p += (q.v + vec3(fl * 0.08f + gust_ * 0.4f, 0.f, std::cos(time_ * 1.3f + q.seed * 20.f) * 0.05f) * (q.kind == 5 ? 0.f : W)) * wdt;
            if (q.kind == 4) q.v = q.v * std::exp(-1.2f * wdt);
            if ((q.kind == 0 || q.kind == 3) && q.p.y < q.size) q.life = std::min(q.life, q.age + 0.5f);
        }
        petals_.erase(std::remove_if(petals_.begin(), petals_.end(), [](const Petal& q) { return q.age >= q.life; }), petals_.end());
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

    sprites_.clear();
    for (const Mote& m : motes_) {
        if (m.age < 0.f || f.bare) continue; // (the clears' motes: none in a break)
        const float t = m.age / m.life;
        float a = smoothstepf(0.f, 0.12f, t) * (1.f - smoothstepf(0.6f, 1.f, t)) * m.bright;
        if (m.kind == 1) a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.85f, 1.f, t)) * m.bright;
        sprite(m.p, m.size * (m.kind == 1 ? 2.1f : 1.6f), m.col, a * world, 0);
    }
    cells_.clear();
    if (!f.bare) buildBoard(game); // (a break: the world alone)

    // =========================================================== the world (the sky at a reduced resolution, the meshes
    // multisampled at the full resolution)
    const vec3 sunDir = normalize(vec3(std::sin(pal_.sunAz) * std::cos(pal_.sunEl), std::sin(pal_.sunEl),
                                       -std::cos(pal_.sunAz) * std::cos(pal_.sunEl)));
    const float bloom = life * smoothstepf(1.f, 1.6f, flow) + 0.6f * bloomBurst_;
    const vec3 fwdH(-std::sin(CAM_AZ), 0.f, -std::cos(CAM_AZ)), rightH(std::cos(CAM_AZ), 0.f, -std::sin(CAM_AZ));
    auto setLight = [&](GLuint p) {
        set3f(p, "uEye", eye_);
        set3f(p, "uSunDir", sunDir);
        set3f(p, "uSunC", pal_.lit * (1.f + 0.12f * wp + 0.2f * life));
        set3f(p, "uShadowC", pal_.shadow * lerpf(1.f, 0.6f, spaceK));
        set3f(p, "uSunGlow", pal_.sunGlow * (1.f + 0.2f * wp + 0.3f * life));
        set3f(p, "uHazeC", pal_.haze);
        set3f(p, "uRimC", pal_.rim);
        set1f(p, "uFogK", 2.2e-5f);
        set1f(p, "uAlt", alt_);
        set1f(p, "uNight", pal_.night);
        set1f(p, "uTime", time_);
        set1f(p, "uBridge", bridge);
        set3f(p, "uBridgeCol", bridge_.color);
    };
    auto setTree = [&](GLuint p) {
        set4f(p, "uWob", tree_.wob[0], tree_.wob[1], tree_.wob[2], tree_.wob[3]);
        set1f(p, "uG", g_);
        set1f(p, "uWind", wind);
        set1f(p, "uWindPhase", windPhase_);
        set1f(p, "uGust", gust_);
        set1f(p, "uJelly", jelly_);
    };
    auto setFlow = [&](GLuint p) {
        set3f(p, "uGlowC", pal_.glow);
        set1f(p, "uLeafGlow", pal_.leafGlow * (0.8f + 0.2f * wm + 0.3f * wp));
        set1f(p, "uFlow", flow);
        set1f(p, "uFlowK", flowK_);
        set1f(p, "uBloom", bloom);
    };
    // (the sizes under which things are culled or held: in the reduced resolution's pixels, as they were)
    const float minPx = 2.f * tanH_ / skyT_.h * 0.8f;

    skyT_.bind();
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
    set1f(progSky_, "uPix", 2.f * tanH_ / skyT_.h);
    set1f(progSky_, "uSunSize", pal_.sunSize);
    set1f(progSky_, "uStars", pal_.stars);
    set1f(progSky_, "uHills", 1.f - smoothstepf(150.f, 2500.f, alt_));
    set1f(progSky_, "uDeep", smoothstepf(18.f, 20.5f, gc_));
    // the ringed planet only once the moon has gone by under the crown
    set1f(progSky_, "uRinged", smoothstepf(19.9f, 20.9f, gc_));
    set1f(progSky_, "uGlow", 0.3f * wp + 0.6f * life + 0.4f * quadGlow_ + 0.2f * swell_);
    set1f(progSky_, "uSpaceK", spaceK);
    set3f(progSky_, "uZenithC", pal_.zenith);
    set3f(progSky_, "uHorizonC", pal_.horizon);
    set3f(progSky_, "uSpaceC", pal_.space);
    set3f(progSky_, "uNebulaC", pal_.nebula);
    set3f(progSky_, "uNebula2C", pal_.nebula2);
    set3f(progSky_, "uGroundA", pal_.groundA);
    set3f(progSky_, "uGroundB", pal_.groundB);
    set3f(progSky_, "uOceanC", pal_.ocean);
    set3f(progSky_, "uLandC", pal_.land);
    set3f(progSky_, "uDesertC", pal_.desert);
    set3f(progSky_, "uFlowerC", pal_.flower);
    {
        const vec3 moon = (fwdH * 0.25f - rightH * 0.97f) * 2.2e8f + vec3(0.f, 2.2e8f, 0.f);
        set4f(progSky_, "uMoon", moon.x, moon.y, moon.z, 2.2e7f);
    }
    set4f(progSky_, "uCrownW", cc.x, cc.y, cc.z, Rc);
    set3f(progSky_, "uPlanetDir", normalize(fwdH * 0.35f - rightH * 0.94f + vec3(0.f, -0.02f, 0.f)));
    set3f(progSky_, "uGalaxyDir", normalize(fwdH * 0.94f - rightH * 0.3f + vec3(0.f, 0.12f, 0.f)));
    set1f(progSky_, "uBeam", beam_ * world);
    set3f(progSky_, "uBeamPos", cc);
    ugl::drawFullscreen();

    // ---- the meshes, multisampled, over the sky upsampled with its depth
    msT_.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(progUp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyT_.tex);
    set1i(progUp_, "uColor", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, skyT_.depthTex);
    set1i(progUp_, "uDepth", 1);
    glActiveTexture(GL_TEXTURE0);
    ugl::drawFullscreen();

    glDepthFunc(GL_LESS);
    // the bark
    if (barkCount_ > 0) {
        glUseProgram(progBark_);
        setLight(progBark_);
        setTree(progBark_);
        setFlow(progBark_);
        setMat(progBark_, "uVP", vp3_);
        set3f(progBark_, "uBarkC", pal_.bark);
        set3f(progBark_, "uBarkDark", pal_.barkDark);
        glBindVertexArray(barkVao_);
        glDrawElements(GL_TRIANGLES, barkCount_, GL_UNSIGNED_INT, nullptr);
    }
    // the puffs of foliage
    if (leafCount_ > 0) {
        glUseProgram(progLeaf_);
        setLight(progLeaf_);
        setTree(progLeaf_);
        setFlow(progLeaf_);
        setMat(progLeaf_, "uVP", vp3_);
        set1f(progLeaf_, "uShed", shed_);
        set1f(progLeaf_, "uBurst", bloomBurst_ + 0.5f * spurt_);
        set1f(progLeaf_, "uMinPx", minPx);
        set3f(progLeaf_, "uCamR", camR_);
        set3f(progLeaf_, "uCamU", camU_);
        set3f(progLeaf_, "uLeafC", pal_.leaf);
        set3f(progLeaf_, "uLeaf2C", pal_.leaf2);
        set3f(progLeaf_, "uLeafShadeC", pal_.leafShade);
        set3f(progLeaf_, "uBlossomC", pal_.blossom);
        set3f(progLeaf_, "uFruitC", pal_.fruit);
        set1f(progLeaf_, "uBlossom", saturate(pal_.blossomAmt * (0.45f + 0.55f * smoothstepf(1.5f, 4.f, g_)) + 0.25f * bloomBurst_ + 0.2f * life +
                                             0.15f * smoothstepf(10.f, 18.f, g_)));
        set1f(progLeaf_, "uShimmer", shimmer_);
        set1f(progLeaf_, "uSpace", spaceK);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(leafVao_);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, leafCount_);
        glDisable(GL_BLEND);
    }

    // ---- Transparent: the clouds (far to near), the sea of clouds, then the lights in the air.
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    if (alt_ < 2.5e5f) {
        std::vector<std::pair<float, int>> order;
        order.reserve(puffs_.size());
        const float drift = time_ * (6.f + 8.f * wm + 8.f * wp);
        const float pxK = 1.f / (minPx / 0.8f);
        for (size_t i = 0; i < puffs_.size(); i++) {
            const vec3 p = puffs_[i].p + vec3(drift, 0.f, 0.f);
            const vec3 d = p - eye_;
            const float d2 = dot(d, d);
            if (puffs_[i].r * pxK < 0.6f * std::sqrt(d2)) continue;   // under a pixel
            order.push_back({d2, (int)i});
        }
        std::sort(order.begin(), order.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
        cinst_.clear();
        for (auto& o : order) {
            const Puff& pf = puffs_[o.second];
            const vec3 p = pf.p + vec3(drift, 0.f, 0.f);
            // fade the puffs the camera is about to enter
            const float dEye = std::sqrt(o.first);
            const float a = smoothstepf(pf.r * 0.8f, pf.r * 2.5f, dEye) * (1.f - 0.3f * pal_.night);
            const float q[8] = {p.x, p.y, p.z, pf.r, pf.seed, a, 0.f, 0.f};
            cinst_.insert(cinst_.end(), q, q + 8);
        }
        if (!cinst_.empty()) {
            glUseProgram(progCloud_);
            setLight(progCloud_);
            setMat(progCloud_, "uVP", vp3_);
            set3f(progCloud_, "uCamR", camR_);
            set3f(progCloud_, "uCamU", camU_);
            set3f(progCloud_, "uCloudLit", pal_.cloudLit * (1.f + 0.15f * life));
            set3f(progCloud_, "uCloudShade", pal_.cloudShade);
            glBindVertexArray(cloudVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cloudVbo_);
            glBufferData(GL_ARRAY_BUFFER, cinst_.size() * sizeof(float), cinst_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(cinst_.size() / 8));
        }
    }
    if (eye_.y > CLOUD_Y + 200.f) {
        // the sea of clouds under the camera, then the planet's weather from orbit
        glUseProgram(progSea_);
        setLight(progSea_);
        setMat(progSea_, "uVP", vp3_);
        set3f(progSea_, "uLook", look_);
        set3f(progSea_, "uCamR", camR_);
        set3f(progSea_, "uCamU", camU_);
        set2f(progSea_, "uTan", tanH_ * aspect_, tanH_);
        set1f(progSea_, "uPix", 2.f * tanH_ / worldT_.h);
        set1f(progSea_, "uCloudY", CLOUD_Y);
        set1f(progSea_, "uDrift", time_ * (8.f + 10.f * wm + 10.f * wp));
        set1f(progSea_, "uFade", 0.92f * smoothstepf(CLOUD_Y + 200.f, CLOUD_Y + 1800.f, eye_.y));
        set3f(progSea_, "uCloudLit", pal_.cloudLit * (1.f + 0.15f * life));
        set3f(progSea_, "uCloudShade", pal_.cloudShade);
        ugl::drawFullscreen();
    }
    glDepthMask(GL_FALSE);
    {
        pinst_.clear();
        auto glow = [&](const vec3& p, float size, const vec3& c, float a, int mode, float seed, float spin) {
            if (a <= 0.004f) return;
            const float q[12] = {p.x, p.y, p.z, size, c.x, c.y, c.z, a, (float)mode, seed, spin, 0.f};
            pinst_.insert(pinst_.end(), q, q + 12);
        };
        // the grass by the sapling (gone once the camera is far)
        if (gc_ < 4.5f) {
            const vec3 gcol = lerp(pal_.groundA, pal_.leaf2, 0.25f) * (pal_.night > 0.5f ? 0.9f : 1.05f);
            const vec3 flowers[3] = {pal_.flower, ugl::toLinear(hex(0xffe14a)), vec3(1.f)};
            for (const Tuft& t : tufts_) {
                const vec3 c = t.flower > 0.5f ? flowers[(int)(t.seed * 3.f) % 3] : gcol * (0.8f + 0.4f * t.seed);
                glow(t.p, t.h, c, world, t.flower > 0.5f ? 4 : 3, t.seed, std::sin(time_ * 1.3f + t.p.x * 2.f + t.seed * 6.f) * (0.15f + 0.25f * wind));
            }
        }
        // birds circling the crown while it is still near the ground
        if (g_ > 2.f && g_ < 8.5f && overT_ < 0.f) {
            const float R = Rc * 1.4f;
            for (int fl = 0; fl < 2; fl++) {
                const float dir = fl == 0 ? 1.f : -1.f;
                const float ang = dir * time_ * (0.12f + 0.04f * fl) + fl * 2.6f;
                const vec3 fc = cc + vec3(std::cos(ang) * R, (fl == 0 ? 0.3f : -0.2f) * Rc, std::sin(ang) * R);
                const vec3 fwd = vec3(-std::sin(ang), 0.f, std::cos(ang)) * dir;
                const vec3 side = vec3(std::cos(ang), 0.f, std::sin(ang));
                const int n = fl == 0 ? 7 : 5;
                for (int i = 0; i < n; i++) {
                    const float k = (float)((i + 1) / 2) * (i % 2 ? 1.f : -1.f);
                    const vec3 p = fc - fwd * (std::fabs(k) * 1.6f) + side * (k * 1.5f) + vec3(0.f, std::sin(time_ * 0.7f + i) * 0.4f, 0.f);
                    const vec3 col = pal_.night > 0.5f ? lerp(pal_.glow, vec3(1.f), 0.3f) : vec3(1.f, 0.98f, 0.95f);
                    glow(p, 0.6f, col, world * 0.95f, 5, (float)i * 0.37f + fl, time_ * 9.f + i * 1.3f);
                }
            }
        }
        // hot-air balloons drifting around the clouds' height
        if (alt_ < 4.e4f) {
            static const unsigned BAL[6] = {0xff3b4a, 0xffc61a, 0x2a8cff, 0xa64cff, 0xff7a1a, 0x18c8a8};
            Rng br(0xBA11ull);
            for (int i = 0; i < 9; i++) {
                float a = br.range(0.f, TAU);
                if (std::fabs(std::remainder(a - CAM_AZ, TAU)) < 0.7f) a += 1.6f;
                const float d = br.range(250.f, 3500.f);
                const float h = br.range(350.f, 2900.f);
                const float sz = br.range(12.f, 20.f);
                const vec3 p(std::cos(a) * d + time_ * 1.5f, h + std::sin(time_ * 0.3f + i) * 6.f, std::sin(a) * d);
                glow(p, sz, ugl::toLinear(hex(BAL[i % 6])), world, 7, br.uniform(), 0.f);
            }
        }
        // satellites in orbit
        if (alt_ > 3.e4f) {
            Rng sr(0x5A7Eull);
            for (int i = 0; i < 7; i++) {
                const float h = sr.range(3.5e5f, 1.4e6f);
                const float tilt = sr.range(-0.5f, 0.5f), az = sr.range(0.f, TAU);
                const vec3 u = normalize(vec3(std::sin(tilt) * std::cos(az), std::cos(tilt), std::sin(tilt) * std::sin(az)));
                const vec3 v = normalize(cross(u, vec3(std::cos(az + 1.f), 0.f, std::sin(az + 1.f))));
                const float th = sr.range(-0.4f, 0.4f) + time_ * sr.range(0.004f, 0.012f) * (sr.chance(0.5f) ? 1.f : -1.f);
                const float rr = gtree::R_EARTH + h;
                const vec3 p = vec3(0.f, -gtree::R_EARTH, 0.f) + (u * std::cos(th) + v * std::sin(th)) * rr;
                glow(p, 2200.f * std::sqrt(h / 4.e5f), vec3(1.f), world * smoothstepf(3.e4f, 1.5e5f, alt_), 6, sr.uniform(), time_ + i);
            }
        }
        for (const Petal& q : petals_) {
            const float t = q.age / q.life;
            const float a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.7f, 1.f, t));
            switch (q.kind) {
            case 1: glow(q.p, q.size * 1.6f, q.col, a * 0.85f * world, 2, q.seed, 0.f); break;
            case 4: glow(q.p, q.size, q.col, a * world, 9, q.seed, 0.f); break;
            case 5: glow(q.p, q.size, q.col, a * 0.55f * world, 8, q.seed, 0.f); break;
            case 3: glow(q.p, q.size * 1.2f, q.col, a * world, 1, q.seed, q.seed * 6.f + time_ * q.spin); break;
            default: glow(q.p, q.size, q.col * (pal_.night > 0.5f ? 1.5f : 1.f), a * 0.95f * world, 1, q.seed, q.seed * 6.f + time_ * q.spin); break;
            }
        }
        if (!pinst_.empty()) {
            glUseProgram(progGlow_);
            setMat(progGlow_, "uVP", vp3_);
            set3f(progGlow_, "uEye", eye_);
            set3f(progGlow_, "uCamR", camR_);
            set3f(progGlow_, "uCamU", camU_);
            set1f(progGlow_, "uMinPx", minPx);
            glBindVertexArray(glowVao_);
            glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
            glBufferData(GL_ARRAY_BUFFER, pinst_.size() * sizeof(float), pinst_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(pinst_.size() / 12));
        }
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    msT_.resolve(worldT_);

    // ---- The bloom (the sky's resolution / 4, its taps as far apart as before).
    bloomA_.bind();
    glUseProgram(progBright_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progBright_, "uSrc", 0);
    set2f(progBright_, "uTexel", 1.f / skyT_.w, 1.f / skyT_.h);
    set1f(progBright_, "uThreshold", pal_.night > 0.5f ? 0.45f : 0.85f);
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
    set1f(progComp_, "uBloomK", pal_.bloom * (1.f + 0.3f * wp + 0.35f * life + 0.4f * quadGlow_) * world);
    set1f(progComp_, "uTwinkle", 0.25f + 0.2f * wm + 0.25f * wp);
    set1f(progComp_, "uTime", time_);
    set1f(progComp_, "uSat", 1.1f);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    // ---- The veil behind the well and the HUD's columns (none in a break).
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(pal_.glow * 0.5f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", pal_.night > 0.5f ? 0.8f : 0.72f);
        set1f(progVeil_, "uColA", (pal_.night > 0.5f ? 0.4f : 0.66f + 0.08f * wp) * world);
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
        set3f(progCell_, "uLightC", lerp(pal_.glow, vec3(1.f), 0.4f));
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
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(f.panels);
    fade_ = f.fade;
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

const UniverseEntry& universe_gianttree() {
    static const UniverseEntry e{
        "GIANT TREE", "a toon world tree shooting up with every cleared line, from a round sapling in a meadow through the clouds into space",
        [] { return std::unique_ptr<Universe>(new GiantTreeUniverse()); },
        // Fits most songs; the calm, airy ones a little better.
        [](const SongLook& l) { return 0.85f + 0.2f * (1.f - l.density) + 0.15f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
