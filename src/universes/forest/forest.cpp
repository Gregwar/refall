// GLOWING FOREST: a night forest lit only by bioluminescence.
//
// One world: a dark, painted night forest. Four depths of trunks fade into a luminous mist (the far ones thin and pale,
// the nearer ones darker, with limbs), banks of mist drift between them, pale shafts of light fall through gaps in the
// canopy with motes of dust in them, and clumps of leaves and vines hang over the top corners. At the sides stand the
// near trunks: ridged bark, buttress roots, moss, and veins and dots of glowing fungus that climb them as the forest
// grows. A large mossy rock fills the bottom-left corner (the HUD's corner text sits on its dark face), a still pool lies
// on the right floor between the trunks (it mirrors the glow on its banks, glowing pads afloat on it), stones and a
// carpet of glowing moss spread over the floor with the song. There is no moon on screen and no lamp: all the light is
// the life's own. Glowing mushrooms (domed, conical or flat caps with gills), slender mycena, ferns unrolling from
// fiddleheads, bell flowers, glowing reeds, cushions of moss, bracket fungi up the trunks and glowworm threads hanging
// from the canopy grow beside and outside the well and below the HUD's columns, never behind their text; they sway in a
// slow breeze, and each lights the mist around it with a soft halo.
//
// The forest grows with the lines: a new song starts with a dark forest and a few faint plants; each cleared row
// breaks into spores that drift out of the well to an empty spot, where a new plant comes up and stays for the song
// (a quad sows a cluster of four at once and the whole forest flares up). The spots fill in stages (the front row, then
// the bark and the left's threads, then a back row), so the forest keeps growing to the end of a long song; once
// every spot is taken, new lines make the old plants grow fuller.
//
// The pieces are solid little cubes of luminous fungus flesh, one per cell with a thin gap: lit from within, darker
// toward their sides, threaded with bright veins and a few glowing spots, a pale line of moonlight along their top edge;
// each of the seven types in a clean color of the variant. A cleared row flares and breaks into spores in a quarter of a
// second; a hard drop puffs a little glowing dust out from under the piece. The ghost is a thin outline.
//
// The song: calm is a still forest, its plants dim and steady, a few fireflies; mid makes the plants glow on the beat
// grid, a quarter of them every two beats (local, eased, never the whole scene), the fireflies more numerous; a peak
// brightens the life steadily, the pulses swell with the bass, the kicks light single plants and spores rise. The drop
// sends one slow wave of light through the forest: from the well outward, plant after plant and the moss between them
// flare up as it passes and settle back over the following seconds. Slow motion is the forest under a cold moon, its
// glow paled to silver; game over puts the stack's glow out from the top, the blocks crumbling away.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/forest/forest.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/forest/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Cormorant Garamond Medium (capitals, numbers) and EB Garamond Italic (the song, the celebrations), SIL Open Font License, see
// OFL-CormorantGaramond.txt and OFL-EBGaramond.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/forest/CormorantGaramond-Medium.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/forest/EBGaramond12-Italic.ttf.inc"
};

namespace shaders = forest_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// A forest (sRGB): the night (top, low), its mist and moss, the life's two glows, the moon of slow motion, and the
// seven piece colors (I O T S Z J L).
struct VariantDef {
    const char* name;
    unsigned skyTop, skyLow, mist, moss;
    unsigned glowA, glowB, moon;
    unsigned pieces[7];
};
const VariantDef VARIANTS[] = {
    // the reference: mint and cyan foxfire in a blue-green night
    {"FOXFIRE", 0x020406, 0x07110f, 0x2a4a48, 0x3a6a4a, 0x52f0c0, 0x7ad8ff, 0xa8c8e8,
     {0x52d8f0, 0xf0d86a, 0xa888f8, 0x78ec8a, 0xf88a78, 0x6890ff, 0xf8aa5a}},
    // blue mushrooms and violet bells in a deep blue wood
    {"MOONSHADE", 0x020309, 0x070a1a, 0x2a3060, 0x34406a, 0x6c94ff, 0xb68cff, 0xb8c4f0,
     {0x8ee6ff, 0xf2dc8a, 0xc49cff, 0x7cf0c4, 0xff8cbc, 0x7890ff, 0xffae78}},
    // honey fungus: amber caps in a warm brown dark
    {"HONEYCAP", 0x020306, 0x080a12, 0x2c3248, 0x3a4436, 0xffb050, 0xffd878, 0xc8c8d8,
     {0x6ad4c8, 0xffe07a, 0xb89cf0, 0xa8dc70, 0xff7a5c, 0xf0a0b8, 0xffaa48}},
    // glowworm pinpoints: pale cyan and white in a cold dark
    {"GLOWWORM", 0x010205, 0x03080f, 0x1c3046, 0x24404c, 0x9ef2ff, 0xd8fff0, 0xa8c0e0,
     {0x60e4f0, 0xf0d49c, 0x9c88ff, 0xbaf070, 0xff98b8, 0x6aa8ff, 0xc8fff0}},
    // spore bloom: pink and violet flowers in a plum night
    {"SPOREBLOOM", 0x040306, 0x110816, 0x4a2856, 0x4a3460, 0xff70c8, 0xc480ff, 0xc8b8e8,
     {0x6ad8f0, 0xf8d878, 0xb07cff, 0x88f0c0, 0xff78b8, 0x8ea0ff, 0xffa888}},
    // verdigris: chartreuse and jade in a grey-green wood
    {"VERDIGRIS", 0x030504, 0x0a120c, 0x34483a, 0x4a6a3a, 0xbcff74, 0x74ffb4, 0xc0d0c8,
     {0x58ccd8, 0xf0e8b0, 0xa898f0, 0xbcf060, 0xf0909c, 0x7aa8f8, 0xf0b85c}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float CLEAR_T = 0.24f;    // a cleared row is gone in this long (real time)
constexpr float OUT_T = 1.5f;       // game over: a block's glow goes out and it crumbles
constexpr float WAVE_SPEED = 2.6f;  // the drop's wave of light, board units a second
constexpr float GROW_T = 2.8f;      // a new plant comes up in this long

// The floor's height (mirrors baseY, rockTop and groundY in the shaders): a gently rolling floor, and the mossy rock
// rising in the bottom-left corner (centered on rockX).
float baseY(float x, float gs) {
    const float ax = std::fabs(x);
    const float k = smoothstepf(4.5f, 8.f, ax);
    return -10.55f + k * (0.22f * std::sin(0.37f * x + gs) + 0.12f * std::sin(1.13f * x + 2.f * gs)) + 0.35f * smoothstepf(14.f, 21.f, x);
}
struct Rock { float x, w, h; };   // the mossy rock: its center, half width and height over -11.3
float rockTop(float x, float gs, const Rock& r) {
    const float bx = (x - r.x) / r.w;
    const float k = std::max(1.f - bx * bx, 0.f);
    return -11.3f + (r.h + 0.18f * std::sin(x * 0.9f + gs)) * std::pow(k, 0.55f);
}
float poolK(float x, const float* pool) {
    return pool[1] > pool[0] ? smoothstepf(pool[0] - 0.7f, pool[0] + 0.5f, x) * smoothstepf(pool[1] + 0.7f, pool[1] - 0.5f, x) : 0.f;
}
float groundY(float x, float gs, const Rock& rock, const float* pool) {
    const float a = baseY(x, gs), b = rockTop(x, gs, rock);
    const float h = std::max(0.3f - std::fabs(a - b), 0.f) / 0.3f;
    return lerpf(std::max(a, b) + h * h * 0.3f * 0.25f, -11.9f, poolK(x, pool));
}

// A plant's height (plant units) by kind: mushrooms, mycena, glowworm threads (hanging down), fern, bells, moss,
// bracket fungi (on the bark), glowing reeds.
const float KIND_H[8] = {1.55f, 2.0f, 4.f, 2.3f, 2.0f, 0.4f, 0.9f, 1.9f};

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

vec2 operator+(const vec2& a, const vec2& b) { return {a.x + b.x, a.y + b.y}; }
vec2 operator*(const vec2& a, float k) { return {a.x * k, a.y * k}; }
vec2 lerp2(const vec2& a, const vec2& b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// A VAO over the unit quad with `n` per-instance vec4 attributes (locations 1..n) of `stride` bytes.
void instVao(GLuint quad, GLuint& vao, GLuint& vbo, int n, size_t stride) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < n; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
}

// The HUD's columns and corners, in board units: nothing glowing grows there.
bool inHud(float x, float y) {
    if (x > layout::LEFT_COLUMN[0] - 0.3f && x < layout::LEFT_COLUMN[2] + 0.2f && y > layout::LEFT_COLUMN[1] - 0.3f) return true;
    if (x > layout::RIGHT_COLUMN[0] - 0.2f && x < layout::RIGHT_COLUMN[2] + 0.3f && y > layout::RIGHT_COLUMN[1] - 0.3f) return true;
    return false;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool ForestUniverse::init(int width, int height) {
    using shaders::withCommon;
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::BG_FS).c_str(), "forest night");
    progHalo_ = ugl::program(shaders::HALO_VS, shaders::HALO_FS, "forest halo");
    progPlant_ = ugl::program(shaders::PLANT_VS, withCommon(shaders::PLANT_FS).c_str(), "forest life");
    progVeil_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::VEIL_FS).c_str(), "forest well");
    progMote_ = ugl::program(shaders::HALO_VS, shaders::MOTE_FS, "forest motes");
    progPiece_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::PIECE_FS).c_str(), "forest blocks");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "forest down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "forest blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::FINAL_FS).c_str(), "forest final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "forest shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "forest text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    instVao(quadVbo_, plantVao_, plantVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, pieceVao_, pieceVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, haloVao_, haloVbo_, 2, sizeof(Inst2));
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

    buildFont(fontCap_, FONT_CAPS);
    buildFont(fontIt_, FONT_IT);
    resize(width, height);
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void ForestUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] forest font init failed\n");
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
        const int cp = ch == 127 ? 0xB7 : ch; // the middle dot in DEL's slot
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

void ForestUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    bool rebuild = std::fabs(hw - viewH_.x) > 0.05f || std::fabs(hh - viewH_.y) > 0.05f;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    // the mossy rock carries the bottom-left corner's text (the section and the universe's name) on its dark face:
    // its top stays above the text, and the floor's plants keep to the right of it
    const float oldRock = rock_[0] + rock_[1] + rock_[2];
    {
        const float s = std::max(1.f, height_ / 900.f), unit = 2.f * viewH_.y / height_;
        const float textTop = viewC_.y - viewH_.y + 84.f * s * unit;
        cornerRight_ = std::min(-viewH_.x + 318.f * s * unit, -8.f);
        const float right = std::min(cornerRight_ + 2.6f, -6.4f);
        rock_[0] = -viewH_.x + (right + viewH_.x) * 0.36f;
        rock_[1] = right - rock_[0];
        const float bx = (cornerRight_ - rock_[0]) / rock_[1];
        rock_[2] = std::clamp((textTop + 0.4f + 11.3f) / std::pow(std::max(1.f - bx * bx, 0.05f), 0.55f), 2.4f, 4.2f);
    }
    if (std::fabs(rock_[0] + rock_[1] + rock_[2] - oldRock) > 0.05f) rebuild = true;
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    if (rebuild || trunks_.empty()) {
        buildTrunks();
        if (!plants_.empty()) {
            // a new frame size: the same forest laid out again, as many plants alive
            int k = 0;
            for (const Plant& p : plants_)
                if (p.live && p.fade < 0.f) k++;
            buildForest(true);
            for (Plant& p : plants_) p.live = false;
            for (int i = 0; i < k; i++) {
                const int pi = sprout(0.f, 0.f, false);
                if (pi >= 0) plants_[pi].grow = 1.f;
            }
        }
    }
}

ForestUniverse::~ForestUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progBg_, progHalo_, progPlant_, progVeil_, progMote_, progPiece_, progDown_, progBlur_, progFinal_, progShape_, progText_})
        glDeleteProgram(p);
    for (GLuint v : {plantVao_, haloVao_, pieceVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, plantVbo_, haloVbo_, pieceVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontCap_, &fontIt_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The forest
// =====================================================================================================

// The near trunks: a few on each side, outside the HUD's columns (they stay for the universe's life).
void ForestUniverse::buildTrunks() {
    trunks_.clear();
    Rng r(trunkSeed_);
    const float edge = viewH_.x + 0.6f;
    for (int side = -1; side <= 1; side += 2) {
        float ax = 13.6f + r.range(0.f, 0.7f);
        bool first = true;
        while (ax < edge && (int)trunks_.size() < 8) {
            Trunk t;
            t.x = side * ax;
            t.hw = first ? r.range(0.9f, 1.1f) : r.range(0.45f, 0.65f);
            t.lean = r.range(-0.015f, 0.015f);
            t.flare = r.range(0.7f, 1.0f);
            trunks_.push_back(t);
            ax += side > 0 ? r.range(5.6f, 6.4f) : r.range(4.2f, 5.2f);
            first = false;
        }
    }
    // the still pool on the right floor, between the first two trunks when there is room
    pool_[0] = pool_[1] = 0.f;
    for (size_t i = 0; i + 1 < trunks_.size(); i++) {
        const Trunk& a = trunks_[i];
        const Trunk& b = trunks_[i + 1];
        if (a.x < 0.f || b.x < a.x) continue;
        const float x0 = a.x + a.hw + 1.1f, x1 = std::min(b.x - b.hw - 0.7f, viewH_.x - 0.6f);
        if (x1 - x0 > 1.6f) { pool_[0] = x0; pool_[1] = x1; }
        break;
    }
}

// The spots where life can grow (a new song: the old forest withers, a fresh one waits to grow), in the order
// they fill. On the floor beside the well, under the HUD's columns (low), out at the sides (tall); on the near trunks.
void ForestUniverse::buildForest(bool instant) {
    if (instant) plants_.clear();
    else
        for (Plant& p : plants_)
            if (p.fade < 0.f) p.fade = 0.f;
    Rng r(seed_ ^ 0xF0E57ull);
    std::vector<Plant> fresh;
    std::vector<float> late;    // added to a spot's place in the order: the back row, the bark and the left come later
    float lateNow = 0.f;
    const float edge = viewH_.x - 0.4f;
    const float cornerX = -cornerRight_ - 0.3f;   // the bottom-left corner's text
    auto add = [&](float x, float y, int kind, float scale, bool flip) {
        Plant p;
        p.x = x;
        p.y = y;
        p.kind = kind;
        p.scale = scale;
        p.flip = flip;
        p.seed = r.uniform() * 10.f;
        p.colMix = r.chance(0.3f) ? 1.f : r.range(0.f, 0.35f);
        p.height = KIND_H[kind];
        fresh.push_back(p);
        late.push_back(lateNow);
    };
    // the floor: a back row (smaller, a little further, drawn behind; it fills once the front row has grown), then
    // the front row
    for (int row = 1; row >= 0; row--)
    for (int side = -1; side <= 1; side += 2) {
        lateNow = row ? 0.7f : 0.f;
        float ax = 5.9f + r.range(0.f, 0.4f) + (row ? 0.45f : 0.f);
        while (ax < edge) {
            const float x = side * ax;
            const float y = groundY(x, groundSeed_, Rock{rock_[0], rock_[1], rock_[2]}, pool_) + 0.02f + (row ? 0.22f : 0.f);
            // how high it may grow here
            float room;
            if (ax < 6.9f) room = 0.9f;
            else if (ax < layout::RIGHT_COLUMN[2] + 0.4f) room = (side < 0 ? layout::LEFT_COLUMN[1] : layout::RIGHT_COLUMN[1]) - 0.5f - y;
            else room = side < 0 ? 0.45f : 7.f;
            int kind;
            const float k = r.uniform();
            if (room < 1.f) kind = k < 0.55f ? 5 : 1;
            else if (row) kind = k < 0.35f ? 7 : k < 0.6f ? 3 : k < 0.75f ? 1 : k < 0.88f ? 4 : 0;
            else if (room < 4.f) kind = k < 0.32f ? 0 : k < 0.46f ? 5 : k < 0.6f ? 1 : k < 0.72f ? 4 : k < 0.86f ? 3 : 7;
            else kind = k < 0.28f ? 3 : k < 0.52f ? 0 : k < 0.7f ? 4 : k < 0.82f ? 7 : k < 0.92f ? 1 : 5;
            float scale = r.range(0.65f, 1.25f);
            if (kind == 3 && room > 4.f) scale = r.range(1.0f, 1.5f);
            scale = std::min(scale, room / KIND_H[kind]);
            if (kind == 1 && room < 1.f) scale = std::min(scale, 0.42f);
            if (row) scale *= 0.72f;
            // the bottom-left corner holds the section and the universe's names: nothing grows there; nor in the pool
            const bool corner = (side < 0 && ax > cornerX) || poolK(x, pool_) > 0.0f || poolK(x - 0.6f, pool_) > 0.0f ||
                                poolK(x + 0.6f, pool_) > 0.0f;
            if (scale > 0.25f && !corner) add(x, y, kind, scale, r.chance(0.5f));
            ax += r.range(0.7f, 1.35f) * (kind == 3 ? 1.5f : 1.f);
        }
    }
    // the pool's banks: reeds standing in the shallows and life leaning over the water, mirrored in it
    if (pool_[1] > pool_[0]) {
        lateNow = 0.1f;
        add(pool_[0] - 0.15f, pool_[2] + 0.03f, 7, r.range(0.95f, 1.15f), false);
        add(pool_[1] + 0.2f, pool_[2] + 0.03f, 7, r.range(0.8f, 1.0f), true);
        add(pool_[0] - 0.75f, groundY(pool_[0] - 0.75f, groundSeed_, Rock{rock_[0], rock_[1], rock_[2]}, pool_) + 0.02f, 0,
            r.range(0.8f, 1.0f), false);
        add(pool_[1] + 0.8f, groundY(pool_[1] + 0.8f, groundSeed_, Rock{rock_[0], rock_[1], rock_[2]}, pool_) + 0.02f, 4,
            r.range(0.85f, 1.05f), true);
    }
    // the canopy: glowworm threads hanging on the free right side, and on the left short of the song's title
    {
        const float top = viewC_.y + viewH_.y + 0.15f;
        lateNow = 0.f;
        float x = 11.9f + r.range(0.f, 0.6f);
        while (x < viewH_.x - 0.4f) {
            add(x, top, 2, r.range(0.85f, 1.15f), r.chance(0.5f));
            x += r.range(1.6f, 2.8f);
        }
        lateNow = 0.45f;
        x = 12.2f + r.range(0.f, 0.6f);
        while (x < viewH_.x - 7.f) {
            add(-x, top, 2, r.range(0.7f, 1.0f), r.chance(0.5f));
            x += r.range(1.6f, 2.8f);
        }
    }
    // the mossy rock in the bottom-left corner: low life along its top, above the corner's text
    lateNow = 0.05f;
    {
        float x = -viewH_.x + 0.9f + r.range(0.f, 0.4f);
        while (x < cornerRight_ - 0.2f) {
            const float y = groundY(x, groundSeed_, Rock{rock_[0], rock_[1], rock_[2]}, pool_) + 0.02f;
            const float k = r.uniform();
            const int kind = k < 0.3f ? 5 : k < 0.5f ? 1 : k < 0.7f ? 0 : k < 0.85f ? 7 : 3;
            float scale = r.range(0.55f, 0.85f);
            if (kind == 3) scale = r.range(0.6f, 0.8f);
            add(x, y, kind, scale, r.chance(0.5f));
            x += r.range(0.9f, 1.5f) * (kind == 3 ? 1.4f : 1.f);
        }
    }
    // the bark: bracket fungi up the near trunks, on both sides, clear of the HUD and the title
    lateNow = 0.4f;
    for (size_t ti = 0; ti < trunks_.size(); ti++) {
        const Trunk& t = trunks_[ti];
        const int n = t.hw > 0.8f ? 3 : 2;
        const float g = groundY(t.x, groundSeed_, Rock{rock_[0], rock_[1], rock_[2]}, pool_);
        for (int i = 0; i < n; i++) {
            const float y = g + 2.8f + (i + r.range(0.1f, 0.9f)) * (11.f / n);
            const float side = r.chance(0.5f) ? 1.f : -1.f;
            const float yy = y - g;
            const float cx = t.x + t.lean * yy + 0.16f * std::sin(yy * 0.16f + ti * 2.3f) - 0.16f * std::sin(ti * 2.3f);
            const float hw = t.hw * (1.f - 0.007f * yy);
            const float x = cx + side * hw * 0.92f;
            if (std::fabs(x) > viewH_.x - 0.4f || inHud(x, y) || inHud(x + side * 0.8f, y) || y > 5.5f) continue;
            add(x, y, 6, t.hw > 0.8f ? r.range(0.85f, 1.1f) : r.range(0.6f, 0.8f), side < 0.f);
        }
    }
    // the order: mostly shuffled, the floor near the well and the free sides first, the far edge and the bark later
    std::vector<std::pair<float, int>> pr;
    const int off = (int)plants_.size();
    for (int i = 0; i < (int)fresh.size(); i++) {
        const Plant& p = fresh[i];
        const float ax = std::fabs(p.x);
        float key = r.uniform() + late[i];
        if (p.kind == 2) key += 0.2f;
        if (ax > 18.f) key += 0.25f;
        if (p.kind == 5 && ax > 7.f) key += 0.15f;
        pr.push_back({key, off + i});
    }
    std::sort(pr.begin(), pr.end());
    for (size_t i = 0; i < fresh.size(); i++) fresh[i].group = (int)(i % 4);
    for (Plant& p : fresh) plants_.push_back(p);
    order_.clear();
    for (auto& q : pr) order_.push_back(q.second);
    // a few faint plants from the start
    for (int i = 0; i < 4; i++) {
        const int pi = sprout(instant ? 0.f : 2.f + i * 0.7f, 0.f, false);
        if (pi >= 0 && instant) plants_[pi].grow = 1.f;
    }
}

// Brings the next spot to life after `delay` seconds (on the side of nearX when it can; 0 either). Once every spot is
// taken, an old plant grows fuller instead. Returns the plant.
int ForestUniverse::sprout(float delay, float nearX, bool fast) {
    int pi = -1;
    for (size_t k = 0; k < order_.size(); k++) {
        const int i = order_[k];
        if (nearX == 0.f || (plants_[i].x < 0.f) == (nearX < 0.f) || k >= 3 || k + 1 == order_.size()) {
            pi = i;
            order_.erase(order_.begin() + (long)k);
            break;
        }
    }
    if (pi < 0) {
        const float cornerX = -cornerRight_ - 0.3f;
        std::vector<int> live;
        for (int i = 0; i < (int)plants_.size(); i++)
            if (plants_[i].live && plants_[i].fade < 0.f && plants_[i].scale < 1.6f && plants_[i].kind != 5) live.push_back(i);
        if (live.empty()) return -1;
        pi = live[rng_.irange(0, (int)live.size() - 1)];
        Plant& p = plants_[pi];
        // fuller, if there is room (under the columns and beside the well it stays as it is)
        const float ax = std::fabs(p.x);
        if (ax > 11.8f && p.kind != 2 && !(p.x < 0.f && ax > cornerX - 0.5f)) p.scale *= 1.1f;
        p.kickGlow = 1.f;
        return pi;
    }
    Plant& p = plants_[pi];
    p.live = true;
    p.grow = 0.f;
    p.delay = delay;
    p.growDur = fast ? GROW_T * 0.7f : GROW_T;
    return pi;
}

vec2 ForestUniverse::plantTop(const Plant& p) const {
    if (p.kind == 2) return vec2{p.x, p.y - 3.f * p.scale};
    if (p.kind == 6) return vec2{p.x + (p.flip ? -0.4f : 0.4f) * p.scale, p.y};
    return vec2{p.x, p.y + p.height * 0.55f * p.scale};
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void ForestUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.4f) v = 3;                                   // glowworm
        else if (t.hasSong && t.bassWeight > 0.62f && t.dynamics > 0.5f) v = 2;                    // honeycap
        else if (t.hasSong && t.airWeight > 0.6f && t.brightness > 0.55f && !t.minor) v = 4;       // sporebloom
        else if (t.hasSong && t.minor && t.brightness < 0.55f) v = 1;                              // moonshade
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    skyT_[0] = lin(V.skyTop);
    skyT_[1] = lin(V.skyLow);
    mistT_ = lin(V.mist);
    mossT_ = lin(V.moss);
    glowT_[0] = lin(V.glowA);
    glowT_[1] = lin(V.glowB);
    moonT_ = lin(V.moon);
    for (int i = 0; i < 7; i++) pieceT_[i] = lin(V.pieces[i]);
    const bool instant = !colSet_ || seconds <= 0.1f;
    if (instant) {
        sky_[0] = skyT_[0]; sky_[1] = skyT_[1];
        mist_ = mistT_; moss_ = mossT_; moon_ = moonT_;
        glow_[0] = glowT_[0]; glow_[1] = glowT_[1];
        for (int i = 0; i < 7; i++) piece_[i] = pieceT_[i];
        colSet_ = true;
    }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    if (instant) groundSeed_ = (float)((seed_ >> 12) % 628) * 0.01f;
    rng_ = Rng(seed_ ^ 0xF02E57ull);
    buildForest(instant);
}

void ForestUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

// The drop: one slow wave of light through the forest, from the well outward.
void ForestUniverse::drop(float strength) {
    dropMode_ = true;
    dropT_ = 0.f;
    waveT_ = 0.f;
    waveK_ = 0.75f + 0.25f * strength;
}

vec2 ForestUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints ForestUniverse::hints() const {
    TransitionHints h;
    h.background = lerp(sky_[0], sky_[1], 0.6f);
    h.light = false;
    h.ink = lerp(glow_[0], vec3(1.f), 0.2f) * 0.8f;
    h.accent = glow_[1];
    h.styles = TS_INK | TS_LUMA | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

ForestUniverse::CellLook ForestUniverse::lookOf(int id, float relX, float relY) const {
    CellLook l;
    const uint64_t h = splitmix64((uint64_t)id * 0x9E3779B97F4A7C15ull ^ seed_);
    l.nx = hashf(h) * 180.f + relX * 1.13f;
    l.ny = hashf(h ^ 0x51) * 180.f - relY * 1.13f;
    l.seed = hashf(h ^ 0xA7 ^ (uint64_t)(relX * 7 + relY * 3 + 20));
    return l;
}

// Keeps the ids in step with the board (a new board, a stack inherited from another universe).
void ForestUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0) { ids_[y][x] = 0; continue; }
            if (ids_[y][x]) continue;
            ids_[y][x] = nextId_++;
            look_[y][x] = lookOf(ids_[y][x], (float)x, (float)y);
        }
}

void ForestUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        // the piece keeps the flesh it fell with
        const int id = nextId_++;
        int minX = 99, minY = 99;
        for (auto& c : ev.cells) { minX = std::min(minX, c.x); minY = std::min(minY, c.y); }
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            look_[c.y][c.x] = lookOf(id, (float)(c.x - minX), (float)(c.y - minY));
        }
        break;
    }
    case GameEvent::HardDrop: {
        // a small breath of glowing dust pushed out from under the piece, at the contact
        if (!game.hasPiece()) break;
        Piece gp = game.piece();
        gp.y = game.ghostY();
        int cells[4][2];
        game.pieceCells(gp, cells);
        int low = -1, minX = 99, maxX = -1;
        for (auto& c : cells) low = std::max(low, c[1]);
        for (auto& c : cells)
            if (c[1] == low) { minX = std::min(minX, c[0]); maxX = std::max(maxX, c[0]); }
        if (low < Game::HIDDEN) break;
        const vec3 col = pieceColor(gp.type);
        for (int s = -1; s <= 1; s += 2) {
            const vec3 p = layout::cellPos(s < 0 ? (float)minX - 0.3f : (float)maxX + 0.3f, (float)low + 0.42f);
            Mote m;
            m.kind = 3;
            m.p = m.p0 = vec2{p.x, p.y};
            m.target = vec2{p.x + s * 0.8f, p.y + 0.3f};
            m.life = 0.65f;
            m.r = 0.5f;
            m.a = 0.22f;
            m.col = col;
            motes_.push_back(m);
        }
        break;
    }
    case GameEvent::Clear: {
        // each row breaks into spores that drift out to an empty spot of the forest, where a plant comes up
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= Game::HIDDEN && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        std::sort(rows.begin(), rows.end());
        const bool quadClear = ev.count >= 4;
        std::vector<int> targets;
        if (quadClear) {
            // a cluster of four, close together
            const int first = sprout(1.0f, (sideFlip_++ % 2) ? 1.f : -1.f, true);
            if (first >= 0) {
                targets.push_back(first);
                const float x0 = plants_[first].x;
                for (int n = 0; n < 3; n++) {
                    int best = -1;
                    float bd = 1e9f;
                    for (size_t k = 0; k < order_.size(); k++) {
                        const Plant& p = plants_[order_[k]];
                        const float d = std::fabs(p.x - x0) + std::fabs(p.y - plants_[first].y) * 0.5f;
                        if ((p.x < 0.f) == (x0 < 0.f) && d < bd) { bd = d; best = (int)k; }
                    }
                    if (best < 0) break;
                    const int pi = order_[best];
                    order_.erase(order_.begin() + best);
                    order_.insert(order_.begin(), pi);
                    const int got = sprout(1.1f + 0.15f * n, 0.f, true);
                    if (got >= 0) targets.push_back(got);
                }
            }
            quadGlow_ = 1.f;
            // the whole forest breathes out spores
            int n = 0;
            for (const Plant& p : plants_) {
                if (!p.live || p.fade >= 0.f || p.grow < 0.5f || n > 40) continue;
                n++;
                Mote m;
                m.kind = 2;
                m.p = m.p0 = plantTop(p);
                m.v = vec2{rng_.range(-0.15f, 0.15f), rng_.range(0.35f, 0.7f)};
                m.delay = rng_.range(0.f, 0.8f);
                m.life = rng_.range(2.5f, 4.f);
                m.r = rng_.range(0.12f, 0.2f);
                m.a = 0.8f;
                m.seed = rng_.uniform() * 10.f;
                m.col = lerp(glow_[0], glow_[1], p.colMix);
                motes_.push_back(m);
            }
        } else {
            for (size_t k = 0; k < rows.size(); k++) {
                const int pi = sprout(1.0f + 0.15f * k, (sideFlip_++ % 2) ? 1.f : -1.f, false);
                if (pi >= 0) targets.push_back(pi);
            }
        }
        for (size_t k = 0; k < rows.size(); k++) {
            const int y = rows[k];
            for (auto& c : ev.cells) {
                if (c.y != y || c.x < 0 || c.x >= Game::W) continue;
                const vec3 p = layout::cellPos((float)c.x, (float)c.y);
                const int id = ids_[c.y][c.x];
                const CellLook l = id ? look_[c.y][c.x] : lookOf(nextId_ + 1000 + c.x, (float)c.x, 0.f);
                clears_.push_back({p.x, p.y, c.type, l, 0.f, 0.f});
                // a spore per cell, the row's spores gathering on its plant
                Mote m;
                m.kind = 1;
                m.p = m.p0 = vec2{p.x, p.y};
                const int ti = targets.empty() ? -1 : targets[quadClear ? (size_t)c.x * targets.size() / Game::W : k % targets.size()];
                if (ti >= 0) {
                    const vec2 top = plantTop(plants_[ti]);
                    m.target = vec2{top.x + rng_.range(-0.3f, 0.3f) * plants_[ti].scale, top.y + rng_.range(-0.3f, 0.3f)};
                } else {
                    const float s = c.x < Game::W / 2 ? -1.f : 1.f;
                    m.target = vec2{s * rng_.range(12.f, 19.f), rng_.range(-9.f, -3.f)};
                }
                m.delay = 0.015f * (float)(c.x % 5);
                m.life = rng_.range(0.95f, 1.25f);
                m.r = rng_.range(0.16f, 0.24f);
                m.a = 0.9f;
                m.seed = rng_.uniform() * 10.f;
                m.col = lerp(c.type >= 0 ? pieceColor(c.type) : glow_[0], ti >= 0 ? lerp(glow_[0], glow_[1], plants_[ti].colMix) : glow_[0], 0.4f);
                motes_.push_back(m);
            }
        }
        // the ids and looks of the rows above step down
        {
            bool cleared[Game::H] = {};
            for (int y : rows) cleared[y] = true;
            for (auto& c : ev.cells)
                if (c.y >= 0 && c.y < Game::H) cleared[c.y] = true;
            int nb[Game::H][Game::W] = {};
            CellLook nl[Game::H][Game::W];
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                for (int x = 0; x < Game::W; x++) { nb[dst][x] = ids_[y][x]; nl[dst][x] = look_[y][x]; }
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) look_[y][x] = nl[y][x];
        }
        break;
    }
    case GameEvent::TopOut: {
        // the stack's glow goes out from the top, the blocks crumbling away
        overT_ = 0.f;
        outs_.clear();
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            const CellLook l = id ? look_[c.y][c.x] : lookOf(5000 + c.y * 10 + c.x, 0.f, 0.f);
            outs_.push_back({p.x, p.y, c.type, l, 0.f, 0.25f + 0.07f * (c.y - top) + 0.1f * hashf((uint64_t)(c.x * 31 + c.y))});
        }
        std::memset(ids_, 0, sizeof(ids_));
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

vec3 ForestUniverse::pieceColor(int type) const { return piece_[std::clamp(type, 0, 6)]; }

void ForestUniverse::addCell(float x, float y, float half, int type, const CellLook& l, float alpha, int kind, float prog, float glow) {
    if (alpha <= 0.003f) return;
    const vec3 c = pieceColor(type);
    cells_.push_back({{x, y, half, (float)kind}, {l.nx, l.ny, l.seed, glow}, {c.x, c.y, c.z, alpha}, {prog, 0.f, 0.f, 0.f}});
}

void ForestUniverse::buildBoard(const Game& g) {
    auto pieceLook = [&](int cells[4][2], int id, int cx, int cy) {
        int minX = 99, minY = 99;
        for (int i = 0; i < 4; i++) { minX = std::min(minX, cells[i][0]); minY = std::min(minY, cells[i][1]); }
        return lookOf(id, (float)(cx - minX), (float)(cy - minY));
    };
    // the ghost: the cubes' outlines where it will land
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                addCell(w.x, w.y, 0.5f, p.type, pieceLook(gc, nextId_, c[0], c[1]), 1.f, 1, 0.f);
            }
        }
    }
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            addCell(p.x, p.y, 0.5f, c.type, look_[y][x], 1.f, 0, 0.f);
        }
    // the falling piece: exactly a locked one at its place
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            const float a = saturate((10.6f - w.y) / 0.6f);
            addCell(w.x, w.y, 0.5f, p.type, pieceLook(cells, nextId_, c[0], c[1]), a, 0, 0.f);
        }
    }
    // the previews
    auto preview = [&](int type, vec2 center, float sc, float alpha, int id) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        g.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (auto& c : cells) {
            const float ox = c[0] - (minx + maxx) * 0.5f, oy = c[1] - (miny + maxy) * 0.5f;
            addCell(center.x + ox * sc, center.y - oy * sc, 0.5f * sc, type, pieceLook(cells, id, c[0], c[1]), alpha, 0, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.45f : 1.f,
            90000 + g.holdType());
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f, nextId_ + 1 + i);
    }
}

void ForestUniverse::drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count) {
    if (!count) return;
    glUseProgram(prog);
    setMat(prog, "uVP", vp_);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float ForestUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) w += f.g[glyphIndex((unsigned char)s[i])].adv * k + track;
    return w - track;
}

void ForestUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                               std::vector<float>& out) const {
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

void ForestUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
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

void ForestUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void ForestUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    // pale lettering, faintly tinted by the forest's glow, on a soft halo of the night
    const vec3 ink = lerp(vec3(0.80f, 0.82f, 0.80f), ugl::toSrgb(glow_[0]), 0.08f), halo = ugl::toSrgb(sky_[1]) * 0.5f;
    const vec3 accent = lerp(ugl::toSrgb(glow_[0]), vec3(1.f), 0.2f);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
    // the bonus gauge: a fine pale ring, its charge in the forest's glow
    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 120;
        const float th = std::max(1.2f, 1.5f * s), th2 = std::max(2.5f, 4.5f * s);
        auto seg = [&](float a0, float a1, float w, const vec3& col, float al) {
            const float s0 = std::sin(a0), c0 = std::cos(a0), s1 = std::sin(a1), c1 = std::cos(a1);
            const float ri = r - w * 0.5f, ro = r + w * 0.5f;
            const float pts[4][2] = {{c.x + s0 * ri, c.y - c0 * ri}, {c.x + s0 * ro, c.y - c0 * ro},
                                     {c.x + s1 * ro, c.y - c1 * ro}, {c.x + s1 * ri, c.y - c1 * ri}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        };
        const float dimk = vis * (1.f - 0.5f * dimT_);
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, th, ink, (g.bonusReady() ? 0.5f : 0.2f) * dimk);
        flushShapes();
        const vec3 ga = ugl::toSrgb(glow_[0]), gb = ugl::toSrgb(glow_[1]);
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, th2, lerp(ga, gb, (i + 0.5f) / N), 0.9f * dimk);
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
            place(fontCap_, upper(h.text), base * 0.74f, base * 0.3f, base * 0.06f);
            drawText(fontCap_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.75f * vis);
            continue;
        case HUD_SECTION:
            place(fontCap_, upper(h.text), base * 0.82f, base * 0.32f, -base * 0.04f);
            drawText(fontCap_, q, accent, halo, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(fontIt_, h.text, base * 1.2f, base * 0.01f, -base * 0.14f);
            drawText(fontIt_, q, ink, halo, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(fontCap_, upper(h.text), base * 0.66f, base * 0.24f);
            drawText(fontCap_, q, ink, halo, saturate(alpha * 1.2f) * 0.72f);
            continue;
        case HUD_LABEL:
            place(fontCap_, upper(h.text), base * 0.7f, base * 0.38f);
            drawText(fontCap_, q, ink, halo, saturate(alpha * 1.2f) * 0.58f);
            continue;
        case HUD_VALUE:
            place(fontCap_, h.text, base * 1.05f, base * 0.1f, -base * 0.06f);
            drawText(fontCap_, q, ink, halo, alpha * 0.95f);
            continue;
        case HUD_NOTICE:
            place(fontIt_, h.text, base * 1.15f, base * 0.02f);
            drawText(fontIt_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontCap_, h.text, base * 0.86f, base * 0.1f);
            drawText(fontCap_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontCap_, upper(h.text), base * 1.05f, base * 0.36f, -base * 0.12f);
            drawText(fontCap_, q, ink, halo, alpha, 0.6f);
            continue;
        case HUD_TOAST_SUB:
            place(fontIt_, h.text, base * 1.05f, base * 0.02f);
            drawText(fontIt_, q, accent, halo, alpha * 0.9f, 0.6f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontCap_, upper(h.text), base * 0.92f, base * 0.28f);
                drawText(fontCap_, q, ink, halo, alpha, 0.7f);
            } else {
                place(fontIt_, h.text, base * 1.0f, base * 0.02f);
                drawText(fontIt_, q, h.accent ? accent : ink, halo, alpha, 0.7f);
            }
            continue;
        default:
            place(fontCap_, h.text, base * 0.78f, base * 0.08f);
            drawText(fontCap_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void ForestUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    if (!bare_) syncIds(game); // the world alone: the blocks' looks kept for the whole frame
    // a new variant eases in
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        for (int i = 0; i < 2; i++) { sky_[i] = lerp(sky_[i], skyT_[i], k); glow_[i] = lerp(glow_[i], glowT_[i], k); }
        mist_ = lerp(mist_, mistT_, k);
        moss_ = lerp(moss_, mossT_, k);
        moon_ = lerp(moon_, moonT_, k);
        for (int i = 0; i < 7; i++) piece_[i] = lerp(piece_[i], pieceT_[i], k);
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.0f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) { overT_ = -1.f; outs_.clear(); }
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    time_ += dt;
    worldTime_ += wdt;
    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    if (music.hasSong) beat_ = (float)music.beat;
    else beat_ += wdt * 2.f;
    const float spb = 60.f / std::max(60.f, music.bpm);
    dropT_ += dt;
    waveT_ += dt;
    float festT = 0.f;
    if (dropMode_) festT = smoothstepf(0.f, 2.f * spb, dropT_) * (1.f - 0.6f * smoothstepf(16.f * spb, 48.f * spb, dropT_));
    fest_ = approach(fest_, festT, festT > fest_ ? 1.2f : 0.3f, dt);
    quadGlow_ = approach(quadGlow_, 0.f, 0.45f, dt);
    float bass = 0.f;
    for (int i = 0; i < 3; i++) bass += music.bandsFast[i];
    bass = saturate(bass / 3.f * 1.6f);
    // the life's overall level: a steady step per section, never pulsing with the beat
    const float life = (0.55f + 0.2f * wm + 0.3f * wp + 0.25f * fest_) * (1.f - 0.35f * slow_);
    // the beat grid: a quarter of the plants glow on each two beats
    const float half = beat_ * 0.5f;
    const int halfI = (int)std::floor(half);
    const float ph = half - std::floor(half);
    const int groupNow = ((halfI % 4) + 4) % 4;
    const float env = smoothstepf(0.f, 0.16f, ph) * (1.f - smoothstepf(0.3f, 1.f, ph));
    const float pulseK = (0.18f * wm + 0.32f * wp) * (0.65f + 0.7f * bass * wp) * (1.f - 0.7f * slow_);
    // a kick at a peak lights a single plant
    if (kickIn_ > 0.3f && wp > 0.4f && !plants_.empty()) {
        for (int tries = 0; tries < 6; tries++) {
            Plant& p = plants_[rng_.irange(0, (int)plants_.size() - 1)];
            if (p.live && p.fade < 0.f && p.grow > 0.8f) { p.kickGlow = std::max(p.kickGlow, kickIn_); break; }
        }
    }
    kickIn_ = 0.f;
    if (waveT_ > 14.f) waveK_ = 0.f;
    // how much of the forest has grown: the bark's veins, the moss carpet and the mist's tint follow it
    {
        float total = 0.f, done = 0.f;
        for (const Plant& p : plants_) {
            if (p.fade >= 0.f) continue;
            total += 1.f;
            if (p.live) done += p.delay > 0.f ? 0.f : p.grow;
        }
        const float want = total > 0.f ? done / total : 0.f;
        grow_ = approach(grow_, want, 0.08f, dt);
    }
    // the breeze: still at calm, livelier with the song, a gust after the drop that settles
    wind_ = approach(wind_, (0.35f + 0.35f * wm + 0.35f * wp + 0.5f * fest_) * (1.f - 0.6f * slow_), 0.3f, dt);
    const float waveR = waveT_ * WAVE_SPEED;

    // ---- The plants.
    for (Plant& p : plants_) {
        if (p.fade >= 0.f) { p.fade += dt / 3.f; continue; }
        if (!p.live) continue;
        if (p.delay > 0.f) { p.delay -= dt; continue; }
        p.grow = std::min(1.f, p.grow + dt / p.growDur * (1.f + 0.5f * fest_));
        p.kickGlow *= std::exp(-dt * 2.5f);
    }
    // the withered plants of an old forest leave; the order of what is left to fill follows
    if (std::any_of(plants_.begin(), plants_.end(), [](const Plant& p) { return p.fade >= 1.f; })) {
        std::vector<int> remap(plants_.size(), -1);
        std::vector<Plant> kept;
        for (size_t i = 0; i < plants_.size(); i++)
            if (plants_[i].fade < 1.f) { remap[i] = (int)kept.size(); kept.push_back(plants_[i]); }
        plants_.swap(kept);
        std::vector<int> o;
        for (int i : order_)
            if (i >= 0 && i < (int)remap.size() && remap[i] >= 0) o.push_back(remap[i]);
        order_.swap(o);
    }
    // ---- Fireflies drifting in the free parts of the forest, more as the song lifts.
    {
        int fireflies = 0;
        for (const Mote& m : motes_)
            if (m.kind == 0) fireflies++;
        const float want = (3.f + 7.f * wm + 8.f * wp + 4.f * fest_) * (1.f - 0.3f * slow_);
        fireflyAcc_ += wdt * 0.8f;
        if (fireflies < want && fireflyAcc_ > 0.f) {
            fireflyAcc_ = -rng_.range(0.2f, 0.8f);
            Mote m;
            m.kind = 0;
            for (int tries = 0; tries < 8; tries++) {
                const float side = rng_.chance(0.5f) ? -1.f : 1.f;
                const float x = side * rng_.range(6.8f, viewH_.x - 0.5f);
                const float y = rng_.range(-10.f, 6.f);
                if (!inHud(x, y) && !(x < -9.f && y < -7.f)) { m.p = m.p0 = vec2{x, y}; break; }
                m.p = m.p0 = vec2{side * 16.f, 0.f};
            }
            const float ang = rng_.range(0.f, TAU);
            m.v = vec2{std::cos(ang), std::sin(ang) * 0.5f} * rng_.range(0.2f, 0.45f);
            m.life = rng_.range(7.f, 13.f);
            m.r = rng_.range(0.28f, 0.42f);
            m.a = rng_.range(0.5f, 0.9f);
            m.seed = rng_.uniform() * 10.f;
            m.col = lerp(glow_[1], vec3(0.9f, 1.f, 0.55f), 0.35f);
            motes_.push_back(m);
        }
    }
    for (Mote& m : motes_) {
        const float mdt = m.kind == 0 || m.kind == 2 ? wdt : dt;
        m.t += mdt;
        const float tt = m.t - m.delay;
        if (tt <= 0.f) continue;
        if (m.kind == 0) {
            const float w = std::sin(m.t * 0.7f + m.seed) * 0.6f;
            m.p = m.p + vec2{m.v.x + w * m.v.y, m.v.y - w * m.v.x + 0.12f * std::sin(m.t * 1.3f + m.seed * 2.f)} * mdt;
            m.trailAcc += mdt;
            if (m.trailAcc > 0.11f) {
                m.trailAcc = 0.f;
                for (int k = 7; k > 0; k--) m.trail[k] = m.trail[k - 1];
                m.trail[0] = m.p;
                m.trailN = std::min(8, m.trailN + 1);
            }
        } else if (m.kind == 1) {
            // an arc up and out of the well, then down onto its plant
            const float s = saturate(tt / m.life);
            const float e = 1.f - std::pow(1.f - s, 2.f);
            const vec2 c1{m.p0.x + (m.target.x - m.p0.x) * 0.2f, m.p0.y + 2.f};
            const vec2 c2{m.target.x, m.target.y + 2.2f};
            const float a = 1.f - e;
            const float k0 = a * a * a, k1 = 3.f * a * a * e, k2 = 3.f * a * e * e, k3 = e * e * e;
            m.p = vec2{m.p0.x * k0 + c1.x * k1 + c2.x * k2 + m.target.x * k3, m.p0.y * k0 + c1.y * k1 + c2.y * k2 + m.target.y * k3};
        } else if (m.kind == 2) {
            m.p = m.p + vec2{m.v.x + 0.12f * std::sin(m.t * 1.1f + m.seed), m.v.y} * mdt;
        } else {
            const float s = saturate(tt / m.life);
            const float e = 1.f - (1.f - s) * (1.f - s);
            m.p = lerp2(m.p0, m.target, e);
        }
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.t > m.delay + m.life; }), motes_.end());
    // spores rising lazily from the plants at a peak
    if (wp > 0.3f && slow_ < 0.5f) {
        const float rate = (0.6f * wp + 1.2f * fest_) * wdt;
        if (rng_.uniform() < rate && !plants_.empty()) {
            const Plant& p = plants_[rng_.irange(0, (int)plants_.size() - 1)];
            if (p.live && p.fade < 0.f && p.grow > 0.8f) {
                Mote m;
                m.kind = 2;
                m.p = m.p0 = plantTop(p);
                m.v = vec2{rng_.range(-0.1f, 0.1f), rng_.range(0.25f, 0.5f)};
                m.life = rng_.range(3.f, 5.f);
                m.r = rng_.range(0.1f, 0.17f);
                m.a = 0.7f;
                m.seed = rng_.uniform() * 10.f;
                m.col = lerp(glow_[0], glow_[1], p.colMix);
                motes_.push_back(m);
            }
        }
    }
    for (Fade& c : clears_) c.t += dt;
    clears_.erase(std::remove_if(clears_.begin(), clears_.end(), [](const Fade& c) { return c.t > CLEAR_T; }), clears_.end());
    if (!bare_) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Instances.
    plantInst_.clear();
    halos_.clear();
    cells_.clear();
    moteInst_.clear();
    // the halos add up: a crowded forest lights its mist a little less per plant (light in the dark, never a haze)
    int liveN = 0;
    for (const Plant& p : plants_)
        if (p.live && p.delay <= 0.f) liveN++;
    const float haloK = 1.f / std::sqrt(1.f + liveN / 14.f);
    for (const Plant& p : plants_) {
        if (!p.live || p.delay > 0.f) continue;
        const float fa = p.fade >= 0.f ? 1.f - smoothstepf(0.f, 1.f, p.fade) : 1.f;
        if (fa <= 0.f || p.grow <= 0.f) continue;
        const float dist = std::max(0.f, std::fabs(p.x) - 5.5f) + 0.25f * std::max(0.f, p.y + 10.f);
        const float arrive = dist / WAVE_SPEED;
        const float wave = waveK_ * smoothstepf(arrive - 0.4f, arrive + 0.2f, waveT_) * std::exp(-std::max(0.f, waveT_ - arrive - 0.2f) / 1.8f);
        const float pulse = p.group == groupNow ? env * pulseK : 0.f;
        float glow = life * (1.f + pulse + 0.6f * p.kickGlow + 0.5f * quadGlow_);
        // a young plant lights up as it grows
        glow *= 0.35f + 0.65f * smoothstepf(0.f, 0.8f, p.grow);
        const vec3 ca = lerp(glow_[0], glow_[1], p.colMix), cb = lerp(glow_[1], glow_[0], 0.3f + 0.4f * p.colMix);
        const float sc = p.scale * (1.f + 0.05f * wave);
        plantInst_.push_back({{p.x, p.y, sc, (float)p.kind}, {p.seed, p.grow, glow, p.flip ? 1.f : 0.f},
                              {ca.x, ca.y, ca.z, wave}, {cb.x, cb.y, cb.z, fa * smoothstepf(0.f, 0.15f, p.grow)}});
        // the light it casts on the mist
        const vec2 c = plantTop(p);
        const float hr = (1.4f + 1.2f * p.height * 0.5f) * p.scale * (1.f + 0.2f * wave);
        const float ha = 0.05f * glow * fa * smoothstepf(0.f, 1.f, p.grow) * (1.f + 1.4f * wave) * (p.kind == 5 ? 0.6f : p.kind == 2 ? 0.5f : p.kind == 6 ? 0.6f : 1.f) * haloK;
        halos_.push_back({{c.x, c.y, hr, ha}, {ca.x, ca.y, ca.z, 0.f}});
    }
    if (!bare_) { // the world alone: no blocks, nothing flaring or going out
        for (const Fade& c : clears_) addCell(c.x, c.y, 0.5f, c.type, c.l, 1.f, 2, saturate(c.t / CLEAR_T));
        if (overT_ >= 0.f)
            for (const Fade& o : outs_) {
                const float t = saturate((overT_ - o.delay) / OUT_T);
                if (t < 1.f) addCell(o.x, o.y, 0.5f, o.type, o.l, 1.f, 3, t);
            }
        buildBoard(game);
    }
    for (const Mote& m : motes_) {
        const float tt = m.t - m.delay;
        if (tt <= 0.f) continue;
        if (bare_ && (m.kind == 1 || m.kind == 3)) continue; // (the spores out of a cleared row, a hard drop's dust)
        const float s = saturate(tt / m.life);
        float a = m.a, r = m.r, core = 0.f;
        if (m.kind == 0) {
            // a firefly: fades in and out over its life, glows and dims slowly (never flashing)
            const float blink = 0.5f + 0.5f * std::sin(m.t * (0.9f + 0.4f * std::fmod(m.seed, 1.f)) + m.seed * 3.f);
            a *= smoothstepf(0.f, 0.15f, s) * (1.f - smoothstepf(0.8f, 1.f, s)) * (0.15f + 0.85f * blink * blink);
            core = 1.f;
            // never over the well or the HUD's text
            a *= smoothstepf(5.9f, 6.8f, std::fabs(m.p.x));
            if (inHud(m.p.x, m.p.y)) a *= 0.2f;
            // its trail: a fading streak of light along where it flew
            for (int k = 0; k < m.trailN; k++) {
                const vec2 q = m.trail[k];
                float ta = a * 0.3f * (1.f - (k + 1) / 9.f);
                ta *= smoothstepf(5.9f, 6.8f, std::fabs(q.x));
                if (inHud(q.x, q.y)) ta *= 0.2f;
                if (ta > 0.003f) moteInst_.push_back({{q.x, q.y, r * (0.42f - 0.025f * k), ta}, {m.col.x, m.col.y, m.col.z, 0.25f}});
            }
        } else if (m.kind == 1) {
            a *= smoothstepf(0.f, 0.1f, s) * (1.f - smoothstepf(0.75f, 1.f, s));
            r *= 1.f - 0.4f * s;
            core = 0.7f;
        } else if (m.kind == 2) {
            a *= smoothstepf(0.f, 0.15f, s) * (1.f - smoothstepf(0.5f, 1.f, s));
            core = 0.5f;
            a *= smoothstepf(5.9f, 6.8f, std::fabs(m.p.x));
            if (inHud(m.p.x, m.p.y)) a *= 0.3f;
        } else {
            a *= smoothstepf(0.f, 0.15f, s) * (1.f - s);
            r *= 0.6f + 0.6f * s;
        }
        if (a <= 0.003f) continue;
        moteInst_.push_back({{m.p.x, m.p.y, r, a}, {m.col.x, m.col.y, m.col.z, core}});
    }

    // ---- Draw.
    const float aaW = 2.f * viewH_.y / (float)height_;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progBg_);
    set4f(progBg_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progBg_, "uSkyTop", sky_[0]);
    set3f(progBg_, "uSkyLow", sky_[1]);
    set3f(progBg_, "uMist", mist_);
    set3f(progBg_, "uMoss", moss_);
    set3f(progBg_, "uGlowA", glow_[0]);
    set3f(progBg_, "uGlowB", glow_[1]);
    {
        float tv[32] = {};
        const int n = std::min(8, (int)trunks_.size());
        for (int i = 0; i < n; i++) {
            tv[i * 4] = trunks_[i].x; tv[i * 4 + 1] = trunks_[i].hw; tv[i * 4 + 2] = trunks_[i].lean; tv[i * 4 + 3] = trunks_[i].flare;
        }
        glUniform4fv(ugl::U(progBg_, "uTrunk"), 8, tv);
        set1i(progBg_, "uTrunks", n);
    }
    set1f(progBg_, "uGS", groundSeed_);
    set3f(progBg_, "uRock", vec3(rock_[0], rock_[1], rock_[2]));
    set4f(progBg_, "uPool", pool_[0], pool_[1], pool_[2], 0.f);
    set1f(progBg_, "uGrow", grow_);
    set1f(progBg_, "uWind", wind_);
    set1f(progBg_, "uTime", worldTime_);
    set1f(progBg_, "uSeed", seedF_);
    set1f(progBg_, "uMistK", (0.5f + 0.15f * wm + 0.25f * wp + 0.25f * fest_) * (1.f - 0.25f * slow_));
    set1f(progBg_, "uLife", life);
    set1f(progBg_, "uWaveR", waveR);
    set1f(progBg_, "uWaveK", waveK_);
    set1f(progBg_, "uAA", aaW);
    set1f(progBg_, "uSlow", slow_);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // the scene's alpha is the stack's (for the bridge)
    // the light of the life on the mist
    glBlendFunc(GL_ONE, GL_ONE);
    drawInst(progHalo_, haloVao_, haloVbo_, halos_.data(), halos_.size() * sizeof(Inst2), halos_.size());
    // the life
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!plantInst_.empty()) {
        glUseProgram(progPlant_);
        set1f(progPlant_, "uSlow", slow_);
        set1f(progPlant_, "uTime", worldTime_);
        set1f(progPlant_, "uAAp", aaW * 1.2f);
        set3f(progPlant_, "uMoon", moon_);
        set1f(progPlant_, "uGS", groundSeed_);
        set1f(progPlant_, "uWind", wind_);
        drawInst(progPlant_, plantVao_, plantVbo_, plantInst_.data(), plantInst_.size() * sizeof(Inst), plantInst_.size());
    }
    // the well
    if (!bare_) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uSkyLow", sky_[1]);
        set3f(progVeil_, "uLine", lerp(vec3(0.45f, 0.5f, 0.5f), glow_[0], 0.25f));
        set1f(progVeil_, "uAA", aaW);
        set1f(progVeil_, "uLineA", 0.15f);
        ugl::drawFullscreen();
    }
    // fireflies and spores
    glBlendFunc(GL_ONE, GL_ONE);
    drawInst(progMote_, haloVao_, haloVbo_, moteInst_.data(), moteInst_.size() * sizeof(Inst2), moteInst_.size());
    // the blocks: the stack writes the scene's alpha
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(progPiece_);
    set1f(progPiece_, "uGlow", (1.f + 0.06f * wp + 0.05f * fest_) * (1.f - 0.2f * slow_));
    set1f(progPiece_, "uSlow", slow_);
    set3f(progPiece_, "uMoon", moon_);
    drawInst(progPiece_, pieceVao_, pieceVbo_, cells_.data(), cells_.size() * sizeof(Inst), cells_.size());

    // ---- The glow.
    glDisable(GL_BLEND);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uBlockCut", 0.8f);
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

    // ---- The frame.
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
    set1f(progFinal_, "uBloomK", (0.5f + 0.12f * wp + 0.15f * fest_) * (1.f - 0.3f * slow_));
    set1f(progFinal_, "uSlow", slow_ * 0.8f);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    set3f(progFinal_, "uMoon", moon_);
    set4f(progFinal_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progFinal_, "uTime", worldTime_);
    set1f(progFinal_, "uGS", groundSeed_);
    set3f(progFinal_, "uRock", vec3(rock_[0], rock_[1], rock_[2]));
    set4f(progFinal_, "uPool", pool_[0], pool_[1], pool_[2], 0.f);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_forest() {
    static const UniverseEntry e{
        "GLOWING FOREST", "a night forest lit by bioluminescent life; each cleared row sows a glowing plant, the drop sends a wave of light through it",
        [] { return std::unique_ptr<Universe>(new ForestUniverse()); },
        // A night world: fits most songs, a little more the slower, airy ones.
        [](const SongLook& l) { return 0.75f + 0.2f * l.airWeight + 0.15f * (1.f - l.density); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
