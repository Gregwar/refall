// GIANT TREE: a world tree grows behind the well with every cleared line, from a tiny sapling at the side of the
// scene into a celestial tree whose crown rises through the clouds into the stars.
//
// The tree is real 3D geometry (tree.cpp): a trunk with buttressed roots, great limbs and two finer orders of
// branches as tapered, furrowed cylinders, the crown as clumps of leaf clusters (and blossoms). It is generated
// once per game from a seed, every part carrying the growth at which it is born, and the shaders reveal it against
// the growth: each cleared line raises the growth (a spurt over a second or so: branches reach out and thicken,
// buds burst into leaves), a Tetris more, with a burst of blossoms and light. As it grows the whole tree scales up
// (a sapling of a meter at first, some 600 m at the end) and the camera pulls back and tilts up so it keeps
// fitting, from a low view in the grass by the sapling to the colossal trunk rising out of a sea of clouds,
// its crown against a sky deepening into stars (the haze, in meters, shows the scale). Full size at 120 lines (the
// growth eases out: the first lines show the most); beyond, it keeps blooming. Game over: every leaf falls; the
// next game sprouts a new seed.
//
// The light: a low sun (or the moon) behind and to the side, a soft key light with a sky ambient, the leaves glowing
// through when the eye looks toward the sun, a rim of light on the bark, the crown's shade inside it, the haze. The
// lanterns (glowing fruits) appear once the tree is grand. The music: calm is a still, soft world (a light breeze);
// mid stirs the wind and lets the petals drift; a peak sways the branches more, shimmers the leaves, fills the air
// with spores and petals and the lanterns breathe with the beat. **The drop** (a real one only, not a chorus): light
// flows up from the roots through the trunk and out along every branch to the leaves, then the whole crown blooms
// and glows while a shower of luminous petals falls and a gust leans the tree.
//
// The pieces are seed glass: translucent glass shapes with a warm seed of light glowing deep inside each shape, fine
// leaf veins. Cleared rows dissolve into light that flows out of the well to the tree's crown.
//
// Cost: the world at a reduced resolution (at most 720 lines) with depth: ~50k triangles of bark, ~9k leaf
// billboards (soft edged), a ground grid of ~36k triangles, ~1700 cloud puffs; a quarter resolution bloom; the
// stars, the veil, the pieces and the lettering at full resolution. Nothing is uploaded while the tree grows
// (one upload per game).
//
// Layout contract: the board is drawn flat (orthographic at the reference camera's scale); only the world behind
// it has its own 3D camera.
#include "universes/gianttree/gianttree.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/gianttree/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

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
// A world shader: the light in the fragment shader, the wind in the vertex shader.
GLuint compileWorld(const char* vs, const char* fs, const char* name, bool sway) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, std::string(shaders::COMMON_GLSL) + shaders::LIGHT_GLSL);
    std::string v(vs);
    v.insert(v.find('\n') + 1, std::string(shaders::COMMON_GLSL) + (sway ? shaders::SWAY_GLSL : ""));
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 lerp2(vec2 a, vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// CPU value noise (the ground's heights).
float hashv(float x, float y) {
    float h = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
    return h - std::floor(h);
}
float vnoise(float x, float y) {
    const float ix = std::floor(x), iy = std::floor(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    const float a = hashv(ix, iy), b = hashv(ix + 1, iy), c = hashv(ix, iy + 1), d = hashv(ix + 1, iy + 1);
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
float fbm(float x, float y) {
    float s = 0, a = 0.5f;
    for (int i = 0; i < 4; i++) {
        s += a * vnoise(x, y);
        const float nx = 0.8f * x - 0.6f * y, ny = 0.6f * x + 0.8f * y;
        x = nx * 2.03f + 1.7f;
        y = ny * 2.03f + 1.7f;
        a *= 0.5f;
    }
    return s / 0.9375f;
}

// A variant (sRGB): the sky (zenith, sky, horizon, haze), the sun and its glow, the ambient, the ground (near, far)
// and the mist, the bark (lit, dark), the leaves (two greens), the blossoms, the glow of the tree's light, the
// lanterns, the clouds (lit, shade), the petals, the HUD's ink and accent, the veil; the seven pieces; the sun's
// place (azimuth from straight behind the tree, elevation), its size, night, stars, nebula, the leaves' own glow,
// how many blossoms, bloom, haze density.
struct VariantDef {
    const char* name;
    unsigned zenith, sky, horizon, haze, sun, sunGlow, ambient, ground, groundFar, mist;
    unsigned bark, barkDark, leaf, leaf2, blossom, glow, lantern, cloudLit, cloudShade, petal;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float sunAz, sunEl, sunSize, night, stars, nebula, leafGlow, blossomK, bloom, fog;
};
const VariantDef VARIANTS[] = {
    // Spring at dawn: a peach horizon under a lilac sky, fresh green leaves and pink blossoms.
    {"BLOSSOM", 0x2c3a72, 0x93a6d6, 0xf8c8b0, 0xe6c6cc, 0xfff2dc, 0xffb48c, 0x9fb0d8, 0x7a9a66, 0x8a9ab8, 0xf0dae2,
     0x76584c, 0x2c201e, 0x6fae4c, 0xb4d470, 0xffbcd2, 0xfff0c4, 0xffd890, 0xfff2ee, 0x9c90b8, 0xffd2e2,
     0xfff6f2, 0xffbccc, 0x1c1a34,
     {0x7cd6ea, 0xf6d688, 0xc6a4f2, 0x98e0a0, 0xf890aa, 0x8aa6f2, 0xf8ac80}, -0.75f, 0.13f, 0.026f, 0.f, 0.f, 0.f, 0.f, 0.42f, 0.45f, 1.f / 3800.f},
    // Autumn at sunset: a golden crown against an amber sky, red leaves among the gold.
    {"AUTUMN", 0x2a2250, 0xb88878, 0xffbe74, 0xf0b48c, 0xfff0c0, 0xff9448, 0xc09080, 0x6e6a34, 0xa08a90, 0xf8cea4,
     0x5e4236, 0x22160f, 0xe0902e, 0xf6c454, 0xe85a2c, 0xffd890, 0xffb860, 0xffe0b0, 0x9a6c86, 0xf6a03c,
     0xfff2e0, 0xffc070, 0x1e1420,
     {0x7ccad0, 0xf6c860, 0xc89ae0, 0xa8d070, 0xf07c6c, 0x8a9ee8, 0xf89c50}, 0.8f, 0.07f, 0.03f, 0.f, 0.f, 0.f, 0.f, 0.3f, 0.5f, 1.f / 3200.f},
    // A silver moon tree at night: pale glowing leaves, the moon high, stars.
    {"MOON", 0x02040e, 0x0a1832, 0x2a3e6a, 0x34486e, 0xf4f6ff, 0x7c8ccc, 0x40507a, 0x24364e, 0x2a3858, 0x50648e,
     0x4c566c, 0x10141e, 0x8cd0e4, 0xdaf2ff, 0xffffff, 0x9cf0ff, 0xb0e8ff, 0xa8b8e0, 0x34446c, 0xd8f4ff,
     0xe8f0ff, 0x9ce8ff, 0x040814,
     {0x7ee0f8, 0xe8e0a0, 0xb0a0f8, 0x88f0c8, 0xf0a0c8, 0x90a8ff, 0xf0b890}, -1.0f, 0.2f, 0.05f, 1.f, 1.f, 0.f, 0.7f, 0.18f, 0.6f, 1.f / 4200.f},
    // A cosmic tree: starlit indigo foliage under a nebula, golden star blossoms.
    {"COSMOS", 0x05010f, 0x1a0c38, 0x5a2c6c, 0x5e3678, 0xffe6ff, 0xc078ff, 0x50386c, 0x281a44, 0x3a2458, 0x6a4490,
     0x3c2e54, 0x0c0818, 0x5e54cc, 0xe8b8f0, 0xfff0b0, 0xffa8f0, 0xffe0a0, 0xc8a0e8, 0x2a1848, 0xffe8ff,
     0xf6eaff, 0xffb0f0, 0x08041a,
     {0x70d0ff, 0xffd870, 0xd890ff, 0x80f0b0, 0xff80b0, 0x8890ff, 0xffa070}, 0.65f, 0.3f, 0.02f, 1.f, 1.f, 1.f, 1.f, 0.3f, 0.7f, 1.f / 4000.f},
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
    p.zenith = L(d.zenith); p.sky = L(d.sky); p.horizon = L(d.horizon); p.haze = L(d.haze); p.sun = L(d.sun);
    p.sunGlow = L(d.sunGlow); p.ambient = L(d.ambient); p.ground = L(d.ground); p.groundFar = L(d.groundFar);
    p.mist = L(d.mist); p.bark = L(d.bark); p.barkDark = L(d.barkDark); p.leaf = L(d.leaf); p.leaf2 = L(d.leaf2);
    p.blossom = L(d.blossom); p.glow = L(d.glow); p.lantern = L(d.lantern); p.cloudLit = L(d.cloudLit);
    p.cloudShade = L(d.cloudShade); p.petal = L(d.petal); p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.sunAz = d.sunAz; p.sunEl = d.sunEl; p.sunSize = d.sunSize; p.night = d.night; p.stars = d.stars;
    p.nebula = d.nebula; p.leafGlow = d.leafGlow; p.blossomAmt = d.blossomK; p.bloom = d.bloom; p.fog = d.fog;
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
constexpr int MAX_PETALS = 1400;
constexpr int FULL_LINES = 120;      // the celestial tree
constexpr float CLOUD_Y = 205.f;     // the cloud layer (meters)

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool GiantTreeUniverse::init(int width, int height) {
    progSky_ = compileWorld(ugl::FULLSCREEN_VS, shaders::SKY_FS, "gianttree sky", false);
    progGround_ = compileWorld(shaders::GROUND_VS, shaders::GROUND_FS, "gianttree ground", false);
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
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "gianttree text");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "gianttree post");

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
    instanced(spriteVao_, spriteVbo_, 3, sizeof(SpriteInst));
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
    // the ground
    glGenVertexArrays(1, &groundVao_);
    glGenBuffers(1, &groundVbo_);
    glGenBuffers(1, &groundIbo_);
    glBindVertexArray(groundVao_);
    glBindBuffer(GL_ARRAY_BUFFER, groundVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundIbo_);
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
    buildFont(ital_, FONT_ITAL);
    buildGround();
    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void GiantTreeUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] gianttree font init failed\n");
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

// The ground's height (meters) around the tree's foot: flat by the sapling, a gentle hill under the grown tree,
// rolling country beyond, higher hills far away.
float GiantTreeUniverse::groundAt(float x, float z) const {
    const float r = std::sqrt(x * x + z * z);
    float h = 40.f * (std::exp(-(r / 420.f) * (r / 420.f)) - 1.f);
    h += 0.25f * (vnoise(x / 5.f, z / 5.f) - 0.5f) * smoothstepf(2.f, 12.f, r);
    h += 4.f * (fbm(x / 90.f, z / 90.f) - 0.5f) * smoothstepf(20.f, 120.f, r);
    h += 60.f * (fbm(x / 1700.f + 3.f, z / 1700.f) - 0.45f) * smoothstepf(300.f, 1600.f, r);
    h += 260.f * std::max(0.f, fbm(x / 6000.f + 9.f, z / 6000.f) - 0.35f) * smoothstepf(3000.f, 14000.f, r);
    return h;
}

void GiantTreeUniverse::buildGround() {
    std::vector<float> rad;
    rad.push_back(0.f);
    float r = 0.2f;
    while (r < 42000.f) { rad.push_back(r); r = r * 1.085f + 0.04f; }
    const int segs = 128;
    std::vector<float> v;
    std::vector<uint32_t> idx;
    auto push = [&](float x, float z) {
        const float e = std::max(0.05f, std::sqrt(x * x + z * z) * 0.01f);
        const float h = groundAt(x, z);
        const float hx = groundAt(x + e, z) - groundAt(x - e, z), hz = groundAt(x, z + e) - groundAt(x, z - e);
        const vec3 n = normalize(vec3(-hx, 2.f * e, -hz));
        const float q[6] = {x, h, z, n.x, n.y, n.z};
        v.insert(v.end(), q, q + 6);
    };
    push(0.f, 0.f);
    for (size_t i = 1; i < rad.size(); i++)
        for (int j = 0; j < segs; j++) {
            const float a = (j + 0.5f * (i % 2)) / segs * TAU;
            push(rad[i] * std::cos(a), rad[i] * std::sin(a));
        }
    for (int j = 0; j < segs; j++) {
        const uint32_t q[3] = {0, (uint32_t)(1 + j), (uint32_t)(1 + (j + 1) % segs)};
        idx.insert(idx.end(), q, q + 3);
    }
    for (size_t i = 1; i + 1 < rad.size(); i++)
        for (int j = 0; j < segs; j++) {
            const uint32_t a = (uint32_t)(1 + (i - 1) * segs + j), b = (uint32_t)(1 + (i - 1) * segs + (j + 1) % segs);
            const uint32_t c = a + segs, d = b + segs;
            const uint32_t q[6] = {a, c, b, b, c, d};
            idx.insert(idx.end(), q, q + 6);
        }
    // grass tufts around the sapling, a few wildflowers among them
    Rng tr(0x6A55ull);
    tufts_.clear();
    for (int i = 0; i < 900; i++) {
        const float a = tr.range(0, TAU), d = 0.1f + 3.2f * std::pow(tr.uniform(), 0.7f);
        Tuft t;
        t.p = vec3(std::cos(a) * d, 0.f, std::sin(a) * d);
        t.p.y = groundAt(t.p.x, t.p.z);
        t.h = tr.range(0.04f, 0.1f) * (0.7f + 0.3f * std::min(1.f, d / 2.f));
        t.seed = tr.uniform();
        t.flower = tr.chance(0.05f) ? 1.f : 0.f;
        tufts_.push_back(t);
    }
    glBindVertexArray(groundVao_);
    glBindBuffer(GL_ARRAY_BUFFER, groundVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundIbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
    groundCount_ = (GLsizei)idx.size();
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
    // the world at a reduced resolution
    const int wh = std::max(180, std::min(720, (int)(height_ * 0.66f)));
    const int ww = std::max(320, (int)std::lround((float)wh * width_ / height_));
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
    bloomA_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(std::max(16, ww / 4), std::max(16, wh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GiantTreeUniverse::~GiantTreeUniverse() {
    for (ugl::Target* t : {&worldT_, &bloomA_, &bloomB_, &sceneT_}) t->destroy();
    for (GLuint p : {progSky_, progSea_, progGround_, progBark_, progLeaf_, progGlow_, progCloud_, progComp_, progBright_, progBlur_,
                     progSprite_, progVeil_, progCell_, progShape_, progText_, progPost_})
        glDeleteProgram(p);
    for (GLuint v : {barkVao_, leafVao_, glowVao_, cloudVao_, groundVao_, spriteVao_, cellVao_, shapeVao_, textVao_})
        glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, barkVbo_, barkIbo_, leafVbo_, glowVbo_, cloudVbo_, groundVbo_, groundIbo_, spriteVbo_,
                     cellVbo_, shapeVbo_, textVbo_})
        glDeleteBuffers(1, &b);
    for (Font* f : {&caps_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The tree
// =====================================================================================================

// Lines -> growth: the sapling at 0.045, the full tree at FULL_LINES; easing out, so the first lines show the most.
float GiantTreeUniverse::growthOf(int lines) const {
    const float t = std::min(1.f, lines / (float)FULL_LINES);
    return 0.045f + 0.955f * std::pow(t, 0.72f);
}

// Growth -> meters per tree unit (the trunk is 100 units): a sapling of a meter, a tree of some 600 m.
float GiantTreeUniverse::scaleOf(float g) const {
    return 0.032f * std::pow(5.6f / 0.032f, std::pow(saturate(g), 0.82f));
}

void GiantTreeUniverse::uploadTree() {
    tree_ = buildGiantTree(treeSeed_, 0);
    glBindVertexArray(barkVao_);
    glBindBuffer(GL_ARRAY_BUFFER, barkVbo_);
    glBufferData(GL_ARRAY_BUFFER, tree_.bark.size() * sizeof(float), tree_.bark.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, barkIbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, tree_.index.size() * sizeof(uint32_t), tree_.index.data(), GL_STATIC_DRAW);
    barkCount_ = (GLsizei)tree_.index.size();
    glBindBuffer(GL_ARRAY_BUFFER, leafVbo_);
    glBufferData(GL_ARRAY_BUFFER, tree_.leaves.size() * sizeof(float), tree_.leaves.data(), GL_STATIC_DRAW);
    leafCount_ = (GLsizei)(tree_.leaves.size() / 16);
    lanternCount_ = (GLsizei)(tree_.lanterns.size() / 8);
    glBindVertexArray(0);
    // the clouds: a layer of soft clouds kilometers wide, a few drifting by the tree
    Rng r(treeSeed_ ^ 0xC10Dull);
    puffs_.clear();
    // cumulus around the trunk (the grown tree rises through them), across the country, far away
    for (int c = 0; c < 260; c++) {
        const int band = c < 34 ? 0 : c < 180 ? 1 : 2;
        float a = r.range(0, TAU);
        // none around the trunk on the camera's side (the camera looks at the tree from azimuth ~0.3)
        if (band == 0) a = 0.3f + PI + r.range(-2.2f, 2.2f);
        const float d = band == 0 ? r.range(45.f, 450.f) : band == 1 ? r.range(450.f, 3000.f) : r.range(3000.f, 11000.f);
        const float big = band == 0 ? r.range(22.f, 50.f) : band == 1 ? r.range(45.f, 110.f) : r.range(110.f, 240.f);
        const vec3 ctr(std::cos(a) * d, CLOUD_Y + r.range(-8.f, 12.f) + big * 0.2f, std::sin(a) * d);
        const int n = r.irange(5, 9);
        const float ca = r.range(0, TAU);
        const vec3 ax(std::cos(ca), 0.f, std::sin(ca)), az(-std::sin(ca), 0.f, std::cos(ca));
        for (int i = 0; i < n; i++) {
            const float u = (i - (n - 1) * 0.5f) / std::max(1.f, (n - 1) * 0.5f);
            Puff p;
            p.r = big * r.range(0.6f, 1.f) * (1.f - 0.4f * std::fabs(u));
            p.p = ctr + ax * (u * big * 1.7f + r.range(-0.3f, 0.3f) * big) + az * (r.range(-0.7f, 0.7f) * big) +
                  vec3(0.f, r.range(0.f, 0.45f) * big * (1.f - std::fabs(u)), 0.f);
            p.seed = r.uniform();
            puffs_.push_back(p);
        }
    }
    treeDirty_ = false;
}

// =====================================================================================================
// Camera
// =====================================================================================================

// Frames the tree as it grows: a low view in the grass by the sapling at the right of the scene, then pulling back
// and tilting up, the trunk moving toward the middle, until the celestial tree rises from the bottom of the screen
// into the sky. Only this camera moves: the board is drawn on its own.
void GiantTreeUniverse::updateCamera(float dt) {
    const float s = scaleOf(g_);
    const float hv = tree_.sample(tree_.height, g_);
    const float cr = tree_.sample(tree_.crownR, g_);
    const float H = s * (hv + 1.5f), W = s * cr * 2.f;
    const float F0 = std::max(H, W * 0.8f);
    if (camH_ < 0.f) camH_ = F0;
    else camH_ = std::exp(approach(std::log(camH_), std::log(F0), 1.1f, dt));
    const float F = camH_;
    const float k = smoothstepf(0.f, 1.f, saturate(std::log(F / 1.1f) / std::log(700.f / 1.1f)));
    tanH_ = std::tan(lerpf(21.f, 26.f, k) * PI / 180.f);
    const float D = F * lerpf(6.0f, 1.3f, k);
    const float hc = F * lerpf(0.2f, 0.03f, k);
    const float xs = lerpf(0.66f, 0.5f, k);
    const float az = 0.3f + 0.05f * std::sin(time_ * 0.021f);
    vec3 eye(std::sin(az) * D, 0.f, std::cos(az) * D);
    const float low = std::max(groundAt(eye.x, eye.z), groundAt(eye.x * 0.97f, eye.z * 0.97f)) + hc;
    // the celestial tree: the camera rises above the sea of clouds the trunk grows out of
    const float rise = smoothstepf(0.82f, 0.98f, k);
    eye.y = lerpf(low, CLOUD_Y + 60.f, rise);
    const float ty = F * lerpf(0.38f, 0.58f, k);
    const vec3 target(0.f, ty, 0.f);
    vec3 fwd = normalize(target - eye);
    const vec3 right = normalize(cross(fwd, vec3(0, 1, 0)));
    const float yaw = std::atan(xs * tanH_ * aspect_);
    fwd = normalize(fwd * std::cos(yaw) - right * std::sin(yaw));
    eye_ = eye;
    look_ = fwd;
    camR_ = normalize(cross(fwd, vec3(0, 1, 0)));
    camU_ = cross(camR_, fwd);
    const float zn = std::max(0.02f, D * 0.015f), zf = std::max(70000.f, D * 40.f);
    view3_ = mat4::lookAt(eye_, eye_ + fwd, vec3(0, 1, 0));
    proj3_ = mat4::perspective(2.f * std::atan(tanH_), aspect_, zn, zf);
    vp3_ = proj3_ * view3_;
}

vec2 GiantTreeUniverse::toBoard(const vec3& w) const {
    const vec4 c = vp3_ * vec4(w, 1.f);
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
        if (t.hasSong && !t.minor && t.brightness > 0.58f) v = 0;          // spring for bright, major songs
        else if (t.hasSong && t.minor && t.brightness < 0.42f) v = 2;      // the moon tree for dark ones
        else if (t.hasSong && t.airWeight > 0.62f && t.density < 0.45f) v = 3; // the cosmos for airy, sparse ones
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
    h.background = lerp(p.sky, p.horizon, 0.4f) * (p.night > 0.5f ? 1.f : 0.7f);
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
        const float s = scaleOf(g_);
        const vec3 crownW = vec3(tree_.sample(tree_.crownX, g_), tree_.sample(tree_.crownY, g_), tree_.sample(tree_.crownZ, g_)) * s;
        vec2 to = toBoard(crownW);
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
        spurt_ = std::max(spurt_, 0.4f + 0.15f * lines);
        if (lines >= 4) {
            tetrisGlow_ = 1.f;
            bloomBurst_ = 1.f;
            if (flowT_ > 4.f) { flowT_ = 0.f; flowSpeed_ = tree_.maxPath / 1.8f; flowK_ = 0.7f; }
            petalAcc_ += 110.f;
        } else if (flowT_ > 2.5f) {
            flowT_ = 0.f;
            flowSpeed_ = tree_.maxPath / 1.5f;
            flowK_ = 0.12f + 0.08f * lines;
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
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void GiantTreeUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param, float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

// =====================================================================================================
// Text
// =====================================================================================================

float GiantTreeUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void GiantTreeUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void GiantTreeUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float halo) {
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
    set1f(progText_, "uGlowA", 0.45f * (1.f - b));
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD: Marcellus capitals, widely spaced, the song and the notices in Cormorant italics.
void GiantTreeUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
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
    const float halo = pal_.night > 0.5f ? 0.6f : 0.8f;
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
            const float sz = base * 0.95f, vsz = base * 0.95f, tr = sz * 0.28f;
            const float wName = textWidth(caps_, name, sz, tr);
            std::string vt = var;
            for (size_t i = 1; i < vt.size(); i++) vt[i] = (char)std::tolower((unsigned char)vt[i]);
            const float wVar = vt.empty() ? 0.f : textWidth(ital_, vt, vsz * 1.3f, 0.f);
            const float gap = base * 0.7f;
            const float x = ugl::hudLeft(h, wName + (vt.empty() ? 0.f : gap + wVar), width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(caps_, name, x, h.y, sz, tr, q);
            drawText(caps_, q, ink, a, halo);
            if (!vt.empty()) {
                q.clear();
                textQuads(ital_, vt, x + wName + gap, h.y - sz * 0.15f, vsz * 1.3f, 0.f, q);
                drawText(ital_, q, accent, a * 0.9f, halo);
            }
            continue;
        }
        case HUD_SECTION:
            place(caps_, h.text, base * 0.85f, base * 0.5f);
            drawText(caps_, q, ink, saturate(0.2f + h.alpha * 1.9f), halo);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.5f, base * 0.02f, -base * 0.25f);
            drawText(ital_, q, ink, saturate(h.alpha * 1.3f), halo);
            continue;
        case HUD_INFO:
            place(caps_, h.text, base * 0.68f, base * 0.22f);
            drawText(caps_, q, ink, saturate(alpha * 1.3f) * 0.85f, halo);
            continue;
        case HUD_LABEL:
            place(caps_, h.text, base * 0.7f, base * 0.42f);
            drawText(caps_, q, lerp(ink, accent, 0.45f), saturate(alpha * 1.2f) * 0.92f, halo);
            continue;
        case HUD_VALUE:
            place(caps_, h.text, base * 1.08f, base * 0.08f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_NOTICE:
            place(ital_, h.text, base * 1.4f, base * 0.02f);
            drawText(ital_, q, accent, alpha, halo);
            continue;
        case HUD_KEY:
            place(caps_, h.text, base * 0.78f, base * 0.1f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_TOAST:
            place(caps_, h.text, base * 1.2f, base * 0.5f, -base * 0.15f);
            drawText(caps_, q, ink, alpha, 0.85f);
            continue;
        case HUD_TOAST_SUB:
            place(ital_, h.text, base * 1.25f, base * 0.02f);
            drawText(ital_, q, accent, alpha * 0.9f, 0.85f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(caps_, h.text, base * 1.1f, base * 0.2f, -base * 0.1f);
                drawText(caps_, q, ink, alpha, 0.85f);
            } else {
                place(caps_, h.text, base * 0.78f, base * 0.12f);
                drawText(caps_, q, h.accent ? accent : ink, alpha, 0.85f);
            }
            continue;
        default:
            place(caps_, h.text, base * 0.78f, base * 0.1f);
            drawText(caps_, q, h.accent ? accent : ink, alpha, halo);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void GiantTreeUniverse::render(const Frame& f, GLuint fbo) {
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

    // ---- The game's lines -> the growth (a new game sprouts a new seed).
    {
        const int lines = game.lines();
        if (treeSeed_ == 0) { treeSeed_ = seed_ ? seed_ : 1; treeDirty_ = true; }
        if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
            treeSeed_ = splitmix64(treeSeed_ + 0x9E37ull);
            treeDirty_ = true;
            g_ = 0.f;
            gVel_ = 0.f;
            shed_ = 0.f;
            seedT_ = 0.f;
            petals_.clear();
        }
        lines_ = lines;
        if (treeDirty_) uploadTree();
        gTarget_ = growthOf(lines);
        if (linesShown_ < 0) { g_ = gTarget_; gVel_ = 0.f; }
        linesShown_ = lines;
        // a spurt: a critically damped spring toward the target, over a second or two
        const float w = 3.2f;
        gVel_ += (w * w * (gTarget_ - g_) - 2.f * w * gVel_) * wdt;
        g_ += gVel_ * wdt;
        if (g_ > gTarget_ + 0.002f) { g_ = gTarget_; gVel_ = 0.f; }
        seedT_ += dt;
    }
    shed_ = approach(shed_, overT_ >= 0.f ? 1.f : 0.f, overT_ >= 0.f ? 0.6f : 3.f, dt);
    spurt_ = approach(spurt_, 0.f, 1.2f, dt);
    bloomBurst_ = approach(bloomBurst_, 0.f, 0.7f, dt);

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
    if (isDrop && !wasDrop_) {
        flowT_ = 0.f;
        flowSpeed_ = tree_.maxPath / 3.f;
        flowK_ = 1.f;
        gust_ = 1.f;
    }
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.5f : 0.4f, dt);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    flowT_ += dt;
    flowK_ = approach(flowK_, 0.f, flowT_ > 3.5f ? 0.5f : 0.05f, dt);
    gust_ = approach(gust_, 0.f, 0.45f, dt);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        if (slow_ > 0.01f) {
            // slow motion: a hush, the light softer and bluer, the lanterns brighter
            const float k = slow_;
            pal_.sunGlow = lerp(pal_.sunGlow, pal_.sunGlow * vec3(0.8f, 0.9f, 1.2f), k);
            pal_.haze = lerp(pal_.haze, pal_.haze * vec3(0.8f, 0.88f, 1.1f), k);
            pal_.lantern = pal_.lantern * (1.f + 0.6f * k);
        }
    }
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    tetrisGlow_ = approach(tetrisGlow_, 0.f, 0.4f, dt);
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

    // ---- The wind: a breeze, stirring at mid, swaying at a peak; big things move slowly.
    const float s = scaleOf(g_);
    const float bigK = saturate(std::log(std::max(s, 0.033f) / 0.033f) / std::log(5.6f / 0.033f));
    const float wind = 0.35f + 0.45f * wm + 0.7f * wp + 0.3f * life;
    windPhase_ += wdt * (0.7f + 0.3f * wm + 0.35f * wp) * lerpf(1.5f, 0.55f, bigK);

    updateCamera(dt);

    // ---- Petals and spores in the air (meters), from the living leaves.
    {
        auto randomLeaf = [&](vec3* p) {
            const int n = (int)(tree_.leaves.size() / 16);
            for (int tries = 0; tries < 6 && n > 0; tries++) {
                const float* l = &tree_.leaves[(size_t)rng_.irange(0, n - 1) * 16];
                if (l[7] <= g_ && l[8] > g_) { *p = vec3(l[0], l[1], l[2]) * s; return true; }
            }
            return false;
        };
        const float petalRate = (0.6f + 2.f * wm + 6.f * wp + 55.f * life + 25.f * bloomBurst_) * (0.4f + 0.6f * smoothstepf(0.05f, 0.4f, g_)) +
                                (overT_ >= 0.f && overT_ < 4.f ? 60.f : 0.f);
        const float sporeRate = (1.f + 2.f * wm + 9.f * wp + 14.f * life) * smoothstepf(0.1f, 0.5f, g_) + 8.f * pal_.night * smoothstepf(0.1f, 0.5f, g_);
        if (!f.paused) {
            petalAcc_ += petalRate * wdt;
            sporeAcc_ += sporeRate * wdt;
        }
        petalAcc_ = std::min(petalAcc_, 300.f);
        while (petalAcc_ >= 1.f && (int)petals_.size() < MAX_PETALS) {
            petalAcc_ -= 1.f;
            vec3 p;
            if (!randomLeaf(&p)) break;
            Petal q;
            q.p = p;
            q.v = vec3(rng_.range(0.6f, 2.f), -rng_.range(1.2f, 2.6f), rng_.range(-0.6f, 0.6f)) * s;
            q.age = 0.f;
            q.life = rng_.range(7.f, 12.f);
            q.size = s * rng_.range(0.7f, 1.2f);
            q.seed = rng_.uniform();
            q.spin = rng_.range(-3.f, 3.f);
            q.kind = (life > 0.3f && rng_.chance(0.5f)) || bloomBurst_ > 0.3f ? 2 : 0;
            petals_.push_back(q);
        }
        sporeAcc_ = std::min(sporeAcc_, 60.f);
        while (sporeAcc_ >= 1.f && (int)petals_.size() < MAX_PETALS) {
            sporeAcc_ -= 1.f;
            vec3 p;
            if (!randomLeaf(&p)) break;
            Petal q;
            q.p = p + vec3(rng_.range(-6.f, 6.f), rng_.range(-6.f, 2.f), rng_.range(-6.f, 6.f)) * s;
            q.v = vec3(rng_.range(-0.4f, 0.8f), rng_.range(0.6f, 1.6f), rng_.range(-0.4f, 0.4f)) * s;
            q.age = 0.f;
            q.life = rng_.range(5.f, 9.f);
            q.size = s * rng_.range(0.35f, 0.6f);
            q.seed = rng_.uniform();
            q.spin = 0.f;
            q.kind = 1;
            petals_.push_back(q);
        }
        for (Petal& q : petals_) {
            q.age += wdt;
            const float fl = std::sin(time_ * 1.7f + q.seed * 40.f);
            q.p += (q.v + vec3(fl * 1.1f + gust_ * 6.f, 0.f, std::cos(time_ * 1.3f + q.seed * 20.f) * 0.6f) * s) * wdt;
            if (q.kind != 1 && q.p.y < groundAt(q.p.x, q.p.z) + q.size) q.life = std::min(q.life, q.age + 0.6f);
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
        // the bonus gauge: a faint ring, the charge a bright arc
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        sprite(V(gc.x, gc.y), layout::GAUGE_R * 1.25f, on ? pal_.glow : pal_.accent, 1.f, 1, fill, on ? 1.f : 0.f);
    }
    cells_.clear();
    buildBoard(game);

    // =========================================================== the world (reduced resolution)
    const vec3 sunDir = normalize(vec3(std::sin(pal_.sunAz) * std::cos(pal_.sunEl), std::sin(pal_.sunEl),
                                       -std::cos(pal_.sunAz) * std::cos(pal_.sunEl)));
    const float flowFront = flowT_ * flowSpeed_;
    const float bloom = life * smoothstepf(2.2f, 3.6f, flowT_ < 50.f ? flowT_ : 99.f) + 0.6f * bloomBurst_ * smoothstepf(1.f, 2.f, flowT_);
    const vec4 crownT(tree_.sample(tree_.crownX, g_), tree_.sample(tree_.crownY, g_), tree_.sample(tree_.crownZ, g_),
                      tree_.sample(tree_.crownR, g_));
    const float high = smoothstepf(0.45f, 0.95f, g_);
    auto setLight = [&](GLuint p) {
        set3f(p, "uEye", eye_);
        set3f(p, "uSunDir", sunDir);
        set3f(p, "uSunC", pal_.sun * (pal_.night > 0.5f ? 0.55f : 1.f) * (1.f + 0.15f * wp + 0.2f * life));
        set3f(p, "uSunGlow", pal_.sunGlow * (1.f + 0.2f * wp + 0.3f * life));
        set3f(p, "uSkyC", pal_.sky);
        set3f(p, "uZenithC", pal_.zenith);
        set3f(p, "uHorizonC", pal_.horizon);
        set3f(p, "uHazeC", pal_.haze);
        set3f(p, "uGroundC", pal_.ground);
        set3f(p, "uMistC", pal_.mist);
        set1f(p, "uFogK", pal_.fog);
        set1f(p, "uNight", pal_.night);
        set1f(p, "uTime", time_);
        set1f(p, "uBridge", bridge);
        set3f(p, "uBridgeCol", bridge_.color);
    };
    auto setSway = [&](GLuint p) {
        set1f(p, "uG", g_);
        set1f(p, "uWind", wind);
        set1f(p, "uWindPhase", windPhase_);
        set1f(p, "uGust", gust_);
    };
    auto setFlow = [&](GLuint p) {
        set3f(p, "uGlowC", pal_.glow);
        set4f(p, "uCrown", crownT.x, crownT.y, crownT.z, crownT.w);
        set1f(p, "uLeafGlow", pal_.leafGlow * (0.8f + 0.2f * wm + 0.3f * wp));
        set1f(p, "uFlow", flowFront);
        set1f(p, "uFlowK", flowK_);
        set1f(p, "uFlowW", tree_.maxPath * 0.045f);
        set1f(p, "uBloom", bloom);
    };
    const float minPx = 2.f * tanH_ / worldT_.h * 0.8f;

    worldT_.bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(progSky_);
    setLight(progSky_);
    set3f(progSky_, "uLook", look_);
    set3f(progSky_, "uCamR", camR_);
    set3f(progSky_, "uCamU", camU_);
    set2f(progSky_, "uTan", tanH_ * aspect_, tanH_);
    set1f(progSky_, "uSunSize", pal_.sunSize);
    set1f(progSky_, "uStars", pal_.stars);
    set1f(progSky_, "uHigh", high);
    set1f(progSky_, "uNebula", pal_.nebula * (0.8f + 0.3f * wp + 0.3f * life));
    set1f(progSky_, "uGlow", 0.3f * wp + 0.6f * life + 0.4f * tetrisGlow_ + 0.2f * swell_);
    ugl::drawFullscreen();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    // the ground
    glUseProgram(progGround_);
    setLight(progGround_);
    setMat(progGround_, "uVP", vp3_);
    set4f(progGround_, "uCrownW", crownT.x * s, crownT.y * s, crownT.z * s, crownT.w * s);
    set1f(progGround_, "uScale", s);
    set3f(progGround_, "uBlossomC", pal_.blossom);
    set3f(progGround_, "uLeafC", pal_.leaf);
    set3f(progGround_, "uAmbientC", pal_.ambient);
    glBindVertexArray(groundVao_);
    glDrawElements(GL_TRIANGLES, groundCount_, GL_UNSIGNED_INT, nullptr);
    // the bark
    if (barkCount_ > 0) {
        glUseProgram(progBark_);
        setLight(progBark_);
        setSway(progBark_);
        setFlow(progBark_);
        setMat(progBark_, "uVP", vp3_);
        set1f(progBark_, "uScale", s);
        set3f(progBark_, "uBarkC", pal_.bark);
        set3f(progBark_, "uBarkDark", pal_.barkDark);
        set3f(progBark_, "uYoungC", lerp(pal_.leaf, pal_.bark, 0.55f));
        glBindVertexArray(barkVao_);
        glDrawElements(GL_TRIANGLES, barkCount_, GL_UNSIGNED_INT, nullptr);
    }
    // the leaves
    if (leafCount_ > 0) {
        glUseProgram(progLeaf_);
        setLight(progLeaf_);
        setSway(progLeaf_);
        setFlow(progLeaf_);
        setMat(progLeaf_, "uVP", vp3_);
        set1f(progLeaf_, "uScale", s);
        set1f(progLeaf_, "uShed", shed_);
        set1f(progLeaf_, "uBurst", bloomBurst_ + 0.4f * spurt_);
        set1f(progLeaf_, "uMinPx", minPx);
        set3f(progLeaf_, "uCamR", camR_);
        set3f(progLeaf_, "uCamU", camU_);
        set3f(progLeaf_, "uLeafC", pal_.leaf);
        set3f(progLeaf_, "uLeaf2C", pal_.leaf2);
        set3f(progLeaf_, "uBlossomC", pal_.blossom);
        set1f(progLeaf_, "uBlossom", saturate(pal_.blossomAmt + 0.25f * bloomBurst_ + 0.15f * life +
                                             0.2f * saturate((lines_ - FULL_LINES) / 60.f)));
        set1f(progLeaf_, "uShimmer", shimmer_);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(leafVao_);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, leafCount_);
        glDisable(GL_BLEND);
    }

    // ---- Transparent: the clouds (far to near), then the lights in the air.
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    {
        // far cumulus on the sea (they write their depth where dense: the sea passes behind them)
        std::vector<std::pair<float, int>> order;
        order.reserve(puffs_.size());
        const float camD = std::sqrt(eye_.x * eye_.x + eye_.z * eye_.z);
        const float drift = time_ * (4.f + 6.f * wm + 6.f * wp);
        for (size_t i = 0; i < puffs_.size(); i++) {
            const vec3 p = puffs_[i].p + vec3(drift, 0.f, 0.f);
            const vec3 d = p - eye_;
            order.push_back({dot(d, d), (int)i});
        }
        std::sort(order.begin(), order.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
        cinst_.clear();
        for (auto& o : order) {
            const Puff& pf = puffs_[o.second];
            const vec3 p = pf.p + vec3(drift, 0.f, 0.f);
            // fade the puffs the camera is about to enter
            const float dEye = std::sqrt(o.first);
            const float a = smoothstepf(pf.r * 0.6f, pf.r * 2.f, dEye) * smoothstepf(camD * 0.3f, camD * 0.55f, dEye) *
                            (1.f - 0.4f * pal_.night);
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
            set1f(progCloud_, "uDrift", time_ * 0.01f);
            glBindVertexArray(cloudVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cloudVbo_);
            glBufferData(GL_ARRAY_BUFFER, cinst_.size() * sizeof(float), cinst_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(cinst_.size() / 8));
        }
    }
    glDepthMask(GL_FALSE);
    {
        // the sea of clouds
        glUseProgram(progSea_);
        setLight(progSea_);
        setMat(progSea_, "uVP", vp3_);
        set3f(progSea_, "uLook", look_);
        set3f(progSea_, "uCamR", camR_);
        set3f(progSea_, "uCamU", camU_);
        set2f(progSea_, "uTan", tanH_ * aspect_, tanH_);
        set1f(progSea_, "uCloudY", CLOUD_Y);
        set1f(progSea_, "uDrift", time_ * (5.f + 8.f * wm + 8.f * wp));
        set1f(progSea_, "uFade", 0.7f * smoothstepf(CLOUD_Y + 15.f, CLOUD_Y + 50.f, eye_.y));
        set1f(progSea_, "uCover", eye_.y > CLOUD_Y ? 0.8f : 0.3f);
        set3f(progSea_, "uCloudLit", pal_.cloudLit * (1.f + 0.15f * life));
        set3f(progSea_, "uCloudShade", pal_.cloudShade);
        ugl::drawFullscreen();
    }
    {
        pinst_.clear();
        auto glow = [&](const vec3& p, float size, const vec3& c, float a, int mode, float seed, float spin) {
            if (a <= 0.004f) return;
            const float q[12] = {p.x, p.y, p.z, size, c.x, c.y, c.z, a, (float)mode, seed, spin, 0.f};
            pinst_.insert(pinst_.end(), q, q + 12);
        };
        // the grass by the sapling (gone once the camera is far)
        if (s < 0.5f) {
            const vec3 gcol = lerp(pal_.ground, pal_.leaf, 0.35f) * (pal_.night > 0.5f ? 0.9f : 1.25f);
            for (const Tuft& t : tufts_) {
                const vec3 c = t.flower > 0.5f ? lerp(pal_.blossom, vec3(1.f), 0.3f * t.seed) : gcol * (0.8f + 0.4f * t.seed);
                glow(t.p, t.h, c, world, t.flower > 0.5f ? 4 : 3, t.seed, std::sin(time_ * 1.3f + t.p.x * 2.f + t.seed * 6.f) * (0.15f + 0.25f * wind));
            }
        }
        // birds: two small flocks circling the crown (they fade once they are too small to see)
        if (s > 0.25f && overT_ < 0.f) {
            const vec3 cc = vec3(crownT.x, crownT.y, crownT.z) * s;
            const float R = crownT.w * s * 1.25f;
            for (int fl = 0; fl < 2; fl++) {
                const float dir = fl == 0 ? 1.f : -1.f;
                const float ang = dir * time_ * (0.05f + 0.02f * fl) + fl * 2.6f;
                const vec3 fc = cc + vec3(std::cos(ang) * R, (fl == 0 ? 0.25f : -0.1f) * crownT.w * s + std::sin(time_ * 0.13f + fl) * 4.f,
                                          std::sin(ang) * R);
                const vec3 fwd = vec3(-std::sin(ang), 0.f, std::cos(ang)) * dir;
                const vec3 side = vec3(std::cos(ang), 0.f, std::sin(ang));
                const int n = fl == 0 ? 7 : 5;
                for (int i = 0; i < n; i++) {
                    const float k = (float)((i + 1) / 2) * (i % 2 ? 1.f : -1.f);
                    const vec3 p = fc - fwd * (std::fabs(k) * 2.4f) + side * (k * 2.2f) +
                                   vec3(0.f, std::sin(time_ * 0.7f + i) * 0.6f, 0.f);
                    const vec3 col = pal_.night > 0.5f ? lerp(pal_.glow, vec3(1.f), 0.3f) * 0.9f : pal_.barkDark * 0.7f;
                    glow(p, 0.9f, col, world * (pal_.night > 0.5f ? 0.8f : 0.85f), 5, (float)i * 0.37f + fl, time_ * 7.f + i * 1.3f);
                }
            }
        }
        // the lanterns, once the tree is grand; breathing slowly, with the beat at a peak
        for (int i = 0; i < lanternCount_; i++) {
            const float* l = &tree_.lanterns[(size_t)i * 8];
            const float on = smoothstepf(l[4], l[4] + 0.05f, g_) * (1.f - shed_);
            if (on <= 0.f) continue;
            const float br = (0.7f + 0.3f * std::sin(time_ * (0.5f + l[5]) + l[5] * 30.f)) * (1.f + 0.25f * wp * beatPulse + 0.6f * bloom);
            const vec3 c = lerp(pal_.lantern, pal_.glow, l[5] * 0.5f) * (pal_.night > 0.5f ? 1.3f : 1.0f);
            const vec3 p = vec3(l[0], l[1], l[2]) * s;
            glow(p, l[3] * s * 1.6f, c, on * br * world, 0, l[5], 0.f);
        }
        for (const Petal& q : petals_) {
            const float t = q.age / q.life;
            const float a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.75f, 1.f, t));
            if (q.kind == 1) glow(q.p, q.size * 1.6f, lerp(pal_.glow, pal_.lantern, q.seed * 0.5f), a * 0.8f * world, 2, q.seed, 0.f);
            else if (q.kind == 2) glow(q.p, q.size * 1.4f, lerp(pal_.glow, pal_.petal, 0.4f) * 1.4f, a * world, 2, q.seed, 0.f);
            else glow(q.p, q.size, pal_.petal * (pal_.night > 0.5f ? 1.6f : 1.f), a * 0.95f * world, 1, q.seed, q.seed * 6.f + time_ * q.spin);
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

    // ---- The bloom (quarter resolution).
    bloomA_.bind();
    glUseProgram(progBright_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progBright_, "uSrc", 0);
    set2f(progBright_, "uTexel", 1.f / worldT_.w, 1.f / worldT_.h);
    set1f(progBright_, "uThreshold", pal_.night > 0.5f ? 0.35f : 0.75f);
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
    set1f(progComp_, "uBloomK", pal_.bloom * (1.f + 0.3f * wp + 0.35f * life + 0.4f * tetrisGlow_) * world);
    set1f(progComp_, "uTwinkle", 0.25f + 0.2f * wm + 0.25f * wp);
    set1f(progComp_, "uTime", time_);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    // ---- The veil behind the well and the HUD's columns.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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

const UniverseEntry& universe_gianttree() {
    static const UniverseEntry e{
        "GIANT TREE", "a world tree growing with every cleared line, from a sapling into a celestial tree rising through the clouds",
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
