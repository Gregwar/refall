// GLOWING FOREST: a night forest as a relief print with fluorescent inks.
//
// One world: a hand-carved print of a night forest, complete from the first frame. The whole frame is a key block of
// black ink on paper, its detail cut out with a gouge, and every carved stroke is shading: the sky is solid night cut
// in long horizontal strokes that thicken toward a moon cut out of the block (a ring of solid ink around it, two
// strokes of cloud left across it) and a few stars nicked where it is dark; a far glade glows low on the left between
// the trunks; three depths of trunks stand against it, cut in vertical strokes, the farther the more carved away (the
// forest recedes by carving, not by grey); the canopy hangs over the top in black masses whose edges are cut into fans
// of leaves, vines at the sides; the near trunks frame the view, solid ink cut in long strokes along the bark (a broad
// lit edge on the moon's side, the life's glow low on the side facing the well), buttress roots, glowing veins cut up
// them. The floor is solid ink nicked with U-gouge dabs, grass tufts standing black against the light; stones with
// contour cuts, a large rock in the bottom-left corner (the HUD's corner text on its solid face) and a pool on the
// right, its surface cut in long strokes that mirror the print above it. The well and the HUD's text sit on solid,
// uncut ink.
//
// Everything that glows is printed in two fluorescent spot inks rolled under the key and showing where it is carved:
// low in the forest and in the glade the cuts open onto the inks (the life's light in the air); the glowing life is
// placed among the trunks (mushrooms of three cap forms with gills, slender mycena, ferns, bell flowers, reeds with
// seed heads, cushions of moss, bracket fungi up the trunks, glowworm threads hanging from the canopy), each plant cut
// bare and inked, shaded by cuts across its body, a margin of uncut key around it, its light widening the cuts around
// it (the sky's, the trunks', the floor's) in its ink. The print is pulled with its artefacts: the glow plates a little
// out of register, the key's carved edges pressed into the paper, its solids starved by a light roller here and there,
// the paper's tooth and fibres, and a faint fluorescent glow the inks give off into the dark.
//
// The blocks are neon tubes: a bright rim in the type's color, a dark tinted inside, the type's pattern in thin neon
// lines. A cleared row is gouged away in strokes where it
// stands; a hard drop knocks a few chips out from under the piece. The ghost is a cut outline.
//
// The song: calm is a still print, the plants' light short and the fluorescence restrained, a few fireflies (sparks
// cut in ink); mid lights the plants on the beat grid, a quarter of them every two beats (the cuts around them
// widening, local and eased), more fireflies; a peak cuts the light wider, the pulses swell with the bass, a kick
// lights a single plant and spores rise. The drop turns the forest to neon: on its downbeat a wave runs out from the
// well and, element by element, the print gives way to a clean night with the forest in neon line art (the trunks'
// edges and branches, the plants' contours, the moon's rim, the floor's specks), colored by a gradient on each side
// from the well out, neon tubes up the well's sides; the variant's third ink takes the second glow ink's place, the
// glow plates are knocked out of register and settle back; it stays for the whole drop and goes out after. A quad makes the whole forest flare and breathe out spores. Slow motion pulls the glow inks into one cold
// ink; game over drains the stack's ink to the paper from the top, the roller then inking it over in black. Before a
// song the print is pulled: the key's cuts, then the glow inks, the plants cut in and lit one by one; after it the
// light goes out plant by plant and the sheet sinks into the dark. A new song in the same forest only changes its inks.
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

// A print (sRGB): its paper, the key block's ink, the three drums (a yellow, a pink, a blue), the drums inking the
// world's two glow inks (bits 1, 2, 4: two bits print an overprint; the drum left over is the drop's ink, which takes
// the second glow ink's place for a drop), the one cold ink of slow motion, the seven blocks' inks (I O T S Z J L;
// J white) and the drop's neons (a gradient on each side from the well out, and the caps' warm neon).
struct VariantDef {
    const char* name;
    unsigned paper, key;
    unsigned drums[3];
    int glowA, glowB;
    unsigned slow;
    unsigned pieces[7];
    unsigned neon[7];
};
const VariantDef VARIANTS[] = {
    // foxfire: chartreuse and cyan on black, the drop in pink
    {"FOXFIRE", 0xf1efe9, 0x0a0b0d, {0xd2ff3a, 0xff4fa6, 0x3cc8f4}, 1, 4, 0xa8bccc,
     {0x3cc8f4, 0xd8ff3a, 0xa274f4, 0x58d86c, 0xff5a5e, 0xf1efe9, 0xff9a3c}, {0x2ad8ff, 0x2cff9a, 0xd8ff2a, 0xffb01e, 0xff3f8c, 0xb84cff, 0xff6a3a}},
    // moonshade: cyan and pink on an indigo key, the drop in yellow
    {"MOONSHADE", 0xeef0f2, 0x0b1029, {0xfff04a, 0xff62c0, 0x4aa8ff}, 4, 2, 0xb4bed8,
     {0x4aa8ff, 0xfff04a, 0xb07cff, 0x64e0a4, 0xff5c7c, 0xeef0f2, 0xffa244}, {0x5a6bff, 0x2ad8ff, 0x3cffd2, 0xb44cff, 0xff3fd2, 0xff6a8a, 0xffb02a}},
    // honeycap: yellow and orange (yellow over pink) on a warm black, the drop in blue
    {"HONEYCAP", 0xf3f0ea, 0x110d0b, {0xffe838, 0xff5a9a, 0x40b8f0}, 1, 3, 0xbcc0cc,
     {0x40b8f0, 0xffe838, 0xb07ae8, 0x8cd850, 0xff4f5a, 0xf3f0ea, 0xff962a}, {0x3a6bff, 0x9a4cff, 0xff2fb4, 0x1ee8ff, 0x2cff7a, 0xd8ff2a, 0xff7a3a}},
    // sporebloom: pink and lemon on a plum black, the drop in cyan
    {"SPOREBLOOM", 0xf2eff0, 0x140a19, {0xf4ff50, 0xff3d8c, 0x52d2f8}, 2, 1, 0xc4b8d4,
     {0x52d2f8, 0xf4ff50, 0xb46cff, 0x7ce870, 0xff3d6c, 0xf2eff0, 0xffa050}, {0xff3fd2, 0xff6a4a, 0xffd22a, 0xb44cff, 0x5a6bff, 0x2ad8ff, 0xff8a3a}},
    // verdigris: green (yellow over teal) and teal on a verdigris black, the drop in pink
    {"VERDIGRIS", 0xeef0ec, 0x0b1e1b, {0xe6ff4a, 0xff4fb0, 0x30b4d0}, 5, 4, 0xb0c4c0,
     {0x30c4cc, 0xe6ff4a, 0x9c7cf4, 0x6ce070, 0xff5c6a, 0xeef0ec, 0xffa848}, {0x1ef0e0, 0x2cff7a, 0xc8ff2a, 0x3a8bff, 0x9a4cff, 0xff3fb0, 0xff7a4a}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float CLEAR_T = 0.24f;    // a cleared row is gone in this long (real time)
constexpr float OUT_T = 1.5f;       // game over: a block's ink drains, then it is inked over
constexpr float WAVE_SPEED = 2.6f;  // the drop's wave of light, board units a second

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
const float KIND_H[9] = {1.55f, 2.0f, 4.f, 2.3f, 2.0f, 0.4f, 0.9f, 1.9f, 2.6f};

vec2 operator+(const vec2& a, const vec2& b) { return {a.x + b.x, a.y + b.y}; }
vec2 operator*(const vec2& a, float k) { return {a.x * k, a.y * k}; }
vec2 lerp2(const vec2& a, const vec2& b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// An element's tag (mirrors tagOf in the shaders): how far from the well it stands.
float tagOf(float cx) { return (1.f + std::clamp(std::floor((std::fabs(cx) - 5.f) / 1.2f), 0.f, 15.f)) / 255.f; }

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
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::BG_FS).c_str(), "forest key block");
    progHalo_ = ugl::program(shaders::HALO_VS, shaders::HALO_FS, "forest light");
    progPlant_ = ugl::program(shaders::PLANT_VS, withCommon(shaders::PLANT_FS).c_str(), "forest life");
    progMote_ = ugl::program(shaders::HALO_VS, withCommon(shaders::MOTE_FS).c_str(), "forest motes");
    progPiece_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::PIECE_FS).c_str(), "forest blocks");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::DOWN_FS).c_str(), "forest fluorescence");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "forest blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::FINAL_FS).c_str(), "forest print");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "forest shape");

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
    glBindVertexArray(0);

    resize(width, height);
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
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
    plateT_.create(width_, height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    {
        if (tagTex_) glDeleteTextures(1, &tagTex_);
        glGenTextures(1, &tagTex_);
        glBindTexture(GL_TEXTURE_2D, tagTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, plateT_.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tagTex_, 0);
        const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, bufs);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    lightT_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    if (rebuild || trunks_.empty()) {
        // a new frame size: the same forest laid out again
        buildTrunks();
        if (forestSeed_) buildForest();
    }
}

ForestUniverse::~ForestUniverse() {
    for (ugl::Target* t : {&plateT_, &lightT_, &bloomA_, &bloomB_}) t->destroy();
    glDeleteTextures(1, &tagTex_);
    for (GLuint p : {progBg_, progHalo_, progPlant_, progMote_, progPiece_, progDown_, progBlur_, progFinal_, progShape_})
        glDeleteProgram(p);
    for (GLuint v : {plantVao_, haloVao_, pieceVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, plantVbo_, haloVbo_, pieceVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// The forest
// =====================================================================================================

// The near trunks: a few on each side, outside the HUD's columns (they stay for the universe's life); the moon high on
// the right in the widest gap between them, the glade low on the left, the pool on the right floor.
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
    // the moon, in the widest gap between the near trunks on the right
    const float s0 = layout::RIGHT_COLUMN[2] + 0.6f, s1 = viewH_.x - 0.3f;
    float best = 0.f, bx0 = s0, bx1 = s1;
    {
        std::vector<const Trunk*> right;
        for (const Trunk& t : trunks_)
            if (t.x > 0.f) right.push_back(&t);
        std::sort(right.begin(), right.end(), [](const Trunk* a, const Trunk* b) { return a->x < b->x; });
        float from = s0;
        for (size_t i = 0; i <= right.size() && from < s1; i++) {
            const float to = i < right.size() ? std::min(right[i]->x - right[i]->hw * 1.3f, s1) : s1;
            if (to - from > best) { best = to - from; bx0 = from; bx1 = to; }
            if (i < right.size()) from = right[i]->x + right[i]->hw * 1.3f;
        }
    }
    if (best < 2.8f) {
        // a narrow view: the moon in the middle of the strip, the near trunks in its way stepping out past it
        bx0 = s0;
        bx1 = s1;
        best = s1 - s0;
    }
    moon_[2] = std::clamp(best * 0.36f, 1.1f, 2.2f);
    moon_[0] = std::min((bx0 + bx1) * 0.5f, s1 - moon_[2]);
    moon_[1] = viewC_.y + viewH_.y - 3.8f - moon_[2] * 0.6f;
    for (Trunk& t : trunks_)
        if (t.x > 0.f && std::fabs(t.x - moon_[0]) < moon_[2] + t.hw * 1.3f + 0.2f) t.x = moon_[0] + moon_[2] + t.hw * 1.3f + 0.2f;
    // the glade: far between the trunks on the left, under the title and beside the left column
    {
        const float g0 = -viewH_.x, g1 = layout::LEFT_COLUMN[0] - 0.4f;
        glade_[0] = (g0 + g1) * 0.5f;
        glade_[1] = -2.6f;
        glade_[2] = std::max(2.5f, (g1 - g0) * 0.62f);
        glade_[3] = 4.2f;
    }
    // the still pool on the right floor, between the first two trunks when there is room
    pool_[0] = pool_[1] = 0.f;
    {
        std::vector<const Trunk*> right;
        for (const Trunk& t : trunks_)
            if (t.x > 0.f) right.push_back(&t);
        std::sort(right.begin(), right.end(), [](const Trunk* a, const Trunk* b) { return a->x < b->x; });
        if (right.size() >= 2) {
            const float x0 = right[0]->x + right[0]->hw + 1.1f, x1 = std::min(right[1]->x - right[1]->hw - 0.7f, viewH_.x - 0.6f);
            if (x1 - x0 > 1.6f) { pool_[0] = x0; pool_[1] = x1; }
        }
    }
}

// The life, placed among the trunks: a row on the floor beside the well and under the HUD's columns (low there),
// large plants out at the sides, a smaller row behind them, life on the rock's top and the pool's banks, glowworm
// threads hanging from the canopy (never across the moon or the title), bracket fungi up the near trunks; nothing
// behind the HUD's text or in the bottom-left corner's.
void ForestUniverse::buildForest() {
    plants_.clear();
    Rng r(forestSeed_ ^ 0xF0E57ull);
    const float edge = viewH_.x - 0.4f;
    const float cornerX = -cornerRight_ - 0.3f;   // the bottom-left corner's text
    const Rock rock{rock_[0], rock_[1], rock_[2]};
    auto add = [&](float x, float y, int kind, float scale, bool flip) {
        Plant p;
        p.x = x;
        p.y = y;
        p.kind = kind;
        p.scale = scale;
        p.flip = flip;
        p.seed = r.uniform() * 10.f;
        p.colMix = r.chance(0.35f) ? 1.f : r.range(0.f, 0.3f);
        p.height = KIND_H[kind];
        p.group = r.irange(0, 3);
        plants_.push_back(p);
    };
    // the floor: a row behind (smaller, a little further back, drawn first), then the front row
    for (int row = 1; row >= 0; row--)
    for (int side = -1; side <= 1; side += 2) {
        float ax = 5.9f + r.range(0.f, 0.3f) + (row ? 0.7f : 0.f);
        while (ax < edge) {
            const float x = side * ax;
            const float y = groundY(x, groundSeed_, rock, pool_) + 0.02f + (row ? 0.25f : 0.f);
            // how high it may grow here
            float room;
            if (ax < 6.9f) room = 0.9f;
            else if (ax < layout::RIGHT_COLUMN[2] + 0.4f) room = (side < 0 ? layout::LEFT_COLUMN[1] : layout::RIGHT_COLUMN[1]) - 0.5f - y;
            else room = side < 0 ? 0.45f : 7.f;
            int kind;
            const float k = r.uniform();
            if (room < 1.f) kind = k < 0.6f ? 5 : 1;
            else if (row) kind = k < 0.35f ? 7 : k < 0.65f ? 3 : k < 0.8f ? 1 : 4;
            else if (room < 4.f) kind = k < 0.35f ? 0 : k < 0.5f ? 4 : k < 0.68f ? 3 : k < 0.8f ? 1 : k < 0.9f ? 8 : 7;
            else kind = k < 0.26f ? 3 : k < 0.5f ? 0 : k < 0.66f ? 4 : k < 0.84f ? 8 : 7;
            float scale = room > 4.f ? r.range(1.4f, 1.9f) : r.range(1.15f, 1.6f);
            if (kind == 3 && room > 4.f) scale = r.range(1.6f, 2.f);
            scale = std::min(scale, room / KIND_H[kind]);
            if (kind == 1 && room < 1.f) scale = std::min(scale, 0.45f);
            if (row) scale *= 0.7f;
            // the bottom-left corner holds the section and the universe's names: nothing grows there; nor in the pool
            const bool corner = (side < 0 && ax > cornerX) || poolK(x, pool_) > 0.0f || poolK(x - 0.6f, pool_) > 0.0f ||
                                poolK(x + 0.6f, pool_) > 0.0f;
            if (scale > 0.3f && !corner) add(x, y, kind, scale, r.chance(0.5f));
            const float wide = kind == 3 ? 1.5f : kind == 0 || kind == 4 ? 1.2f : kind == 5 ? 1.4f : 0.9f;
            ax += r.range(0.85f, 1.25f) * std::max(scale, 0.6f) * wide * (row ? 1.5f : 1.f);
        }
    }
    // the pool's banks: reeds standing in the shallows and life leaning over the water, mirrored in it
    if (pool_[1] > pool_[0]) {
        add(pool_[0] - 0.15f, pool_[2] + 0.03f, 7, r.range(1.1f, 1.3f), false);
        add(pool_[1] + 0.2f, pool_[2] + 0.03f, 7, r.range(0.95f, 1.15f), true);
        add(pool_[0] - 0.9f, groundY(pool_[0] - 0.9f, groundSeed_, rock, pool_) + 0.02f, 0, r.range(1.1f, 1.3f), false);
        add(pool_[1] + 1.f, groundY(pool_[1] + 1.f, groundSeed_, rock, pool_) + 0.02f, 4, r.range(1.1f, 1.3f), true);
    }
    // the canopy: glowworm threads hanging on the free right side (clear of the moon), and on the left short of the
    // song's title
    {
        const float top = viewC_.y + viewH_.y + 0.15f;
        float x = 11.9f + r.range(0.f, 0.6f);
        while (x < viewH_.x - 0.4f) {
            if (std::fabs(x - moon_[0]) > moon_[2] + 0.6f) add(x, top, 2, r.range(0.95f, 1.2f), r.chance(0.5f));
            x += r.range(1.4f, 2.4f);
        }
        x = 12.2f + r.range(0.f, 0.6f);
        while (x < viewH_.x - 7.f) {
            add(-x, top, 2, r.range(0.8f, 1.05f), r.chance(0.5f));
            x += r.range(1.4f, 2.4f);
        }
    }
    // the rock in the bottom-left corner: low life along its top, above the corner's text
    {
        float x = -viewH_.x + 0.9f + r.range(0.f, 0.4f);
        while (x < cornerRight_ - 0.2f) {
            const float y = groundY(x, groundSeed_, rock, pool_) + 0.02f;
            const float k = r.uniform();
            const int kind = k < 0.3f ? 5 : k < 0.5f ? 1 : k < 0.75f ? 0 : 3;
            const float scale = kind == 3 ? r.range(0.7f, 0.9f) : r.range(0.7f, 1.0f);
            add(x, y, kind, scale, r.chance(0.5f));
            x += r.range(1.1f, 1.7f) * (kind == 3 ? 1.4f : 1.f);
        }
    }
    // the bark: bracket fungi stacked up the near trunks, high on the left (under the title), clear of the HUD
    for (size_t ti = 0; ti < trunks_.size(); ti++) {
        const Trunk& t = trunks_[ti];
        const bool big = t.hw > 0.8f;
        const int n = big ? 4 : 3;
        const float g = groundY(t.x, groundSeed_, rock, pool_);
        for (int i = 0; i < n; i++) {
            const float y = g + 3.f + (i + r.range(0.15f, 0.85f)) * (15.5f / n);
            const float side = r.chance(0.5f) ? 1.f : -1.f;
            const float yy = y - g;
            const float cx = t.x + t.lean * yy + 0.16f * std::sin(yy * 0.16f + ti * 2.3f) - 0.16f * std::sin(ti * 2.3f);
            const float hw = t.hw * (1.f - 0.007f * yy);
            const float x = cx + side * hw * 0.92f;
            const float sc = big ? r.range(1.05f, 1.35f) : r.range(0.75f, 0.95f);
            if (std::fabs(x) > viewH_.x - 0.4f || inHud(x, y) || inHud(x + side * sc, y) || y > 7.5f) continue;
            add(x, y, 6, sc, side < 0.f);
        }
    }
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
        if (t.hasSong && t.bassWeight > 0.62f && t.dynamics > 0.5f) v = 2;                         // honeycap
        else if (t.hasSong && t.airWeight > 0.6f && t.brightness > 0.55f && !t.minor) v = 3;       // sporebloom
        else if (t.hasSong && t.minor && t.brightness < 0.5f) v = 1;                               // moonshade
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    // a glow ink: the drums in `mask` printed one over the other
    auto glowInk = [&](int mask) {
        vec3 c(1.f);
        for (int i = 0; i < 3; i++)
            if (mask & (1 << i)) c = c * lin(V.drums[i]);
        return c;
    };
    paperT_ = lin(V.paper);
    keyT_ = lin(V.key);
    glowT_[0] = glowInk(V.glowA);
    glowT_[1] = glowInk(V.glowB);
    dropInkT_ = glowInk(7 & ~(V.glowA | V.glowB));
    slowInkT_ = lin(V.slow);
    for (int i = 0; i < 7; i++) pieceT_[i] = lin(V.pieces[i]);
    for (int i = 0; i < 7; i++) neonT_[i] = lin(V.neon[i]);
    const bool instant = !colSet_ || seconds <= 0.1f;
    if (instant) {
        paper_ = paperT_; key_ = keyT_; dropInk_ = dropInkT_; slowInk_ = slowInkT_;
        glow_[0] = glowT_[0]; glow_[1] = glowT_[1];
        for (int i = 0; i < 7; i++) piece_[i] = pieceT_[i];
        for (int i = 0; i < 7; i++) neon_[i] = neonT_[i];
        colSet_ = true;
    }
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0xF02E57ull);
    // the forest is laid out with the universe's first song and stays: a new song only changes its inks
    if (instant || !forestSeed_) {
        forestSeed_ = seed_;
        seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
        groundSeed_ = (float)((seed_ >> 12) % 628) * 0.01f;
        buildForest();
    }
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

// The drop: the blacklight comes on and the fluorescent inks blaze, the third ink rolls out from the well, the glow
// plates are knocked out of register on the downbeat, a wave of light runs through the forest.
void ForestUniverse::drop(float strength) {
    dropMode_ = true;
    dropT_ = 0.f;
    waveT_ = 0.f;
    waveK_ = 0.75f + 0.25f * strength;
    jolt_ = 0.6f + 0.4f * strength;
}

vec2 ForestUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints ForestUniverse::hints() const {
    TransitionHints h;
    h.background = key_;
    h.light = false;
    h.ink = lerp(glow_[0], paper_, 0.3f);
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
        // a few chips of the block knocked out from under the piece, at the contact
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
        for (int s = -1; s <= 1; s += 2)
            for (int k = 0; k < 3; k++) {
                const vec3 p = layout::cellPos(s < 0 ? (float)minX - 0.25f : (float)maxX + 0.25f, (float)low + 0.4f);
                Mote m;
                m.kind = 2;
                m.p = m.p0 = vec2{p.x, p.y};
                m.target = vec2{p.x + s * rng_.range(0.4f, 0.9f), p.y + rng_.range(0.05f, 0.45f)};
                m.life = rng_.range(0.4f, 0.6f);
                m.r = rng_.range(0.08f, 0.13f);
                m.a = 0.9f;
                m.seed = rng_.range(0.f, TAU);
                m.inkB = (float)(k % 2);
                motes_.push_back(m);
            }
        break;
    }
    case GameEvent::Clear: {
        // each row is gouged away where it stands; a quad makes the whole forest flare and breathe out spores
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= Game::HIDDEN && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            const CellLook l = id ? look_[c.y][c.x] : lookOf(nextId_ + 1000 + c.x, (float)c.x, 0.f);
            clears_.push_back({p.x, p.y, c.type, l, 0.f, 0.f});
        }
        if (ev.count >= 4) {
            quadGlow_ = 1.f;
            int n = 0;
            for (const Plant& p : plants_) {
                if (p.kind == 2 || n > 40) continue;
                n++;
                Mote m;
                m.kind = 1;
                m.p = m.p0 = plantTop(p);
                m.v = vec2{rng_.range(-0.15f, 0.15f), rng_.range(0.35f, 0.7f)};
                m.delay = rng_.range(0.f, 0.8f);
                m.life = rng_.range(2.5f, 4.f);
                m.r = rng_.range(0.12f, 0.2f);
                m.a = 0.8f;
                m.seed = rng_.uniform() * 10.f;
                m.inkB = p.colMix;
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
        // the stack's ink drains from the top, then the roller inks it over
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

void ForestUniverse::addCell(float x, float y, float half, int type, const CellLook& l, float alpha, int kind, float prog) {
    if (alpha <= 0.003f) return;
    const vec3 c = pieceColor(type);
    cells_.push_back({{x, y, half, (float)kind}, {l.nx, l.ny, l.seed, (float)std::clamp(type, 0, 6)}, {c.x, c.y, c.z, alpha},
                      {prog, 0.f, 0.f, 0.f}});
}

void ForestUniverse::buildBoard(const Game& g) {
    auto pieceLook = [&](int cells[4][2], int id, int cx, int cy) {
        int minX = 99, minY = 99;
        for (int i = 0; i < 4; i++) { minX = std::min(minX, cells[i][0]); minY = std::min(minY, cells[i][1]); }
        return lookOf(id, (float)(cx - minX), (float)(cy - minY));
    };
    // the ghost: the blocks' cut outlines where it will land
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
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy); // (whole above the well too: it spawns there)
            addCell(w.x, w.y, 0.5f, p.type, pieceLook(cells, nextId_, c[0], c[1]), 1.f, 0, 0.f);
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

void ForestUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const vec3 halo = ugl::toSrgb(key_), accent = lerp(ugl::toSrgb(glow_[0]), vec3(1.f), 0.2f);
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
}


void ForestUniverse::readBoxes(const Frame& f) {
    boxN_ = 0;
    if (!f.hudBoxes) return;
    const float sx = 2.f * viewH_.x / width_, sy = 2.f * viewH_.y / height_;
    for (size_t i = 0; i < f.hudBoxes->size() && boxN_ < MAX_BOXES; i++) {
        const vec4& b = (*f.hudBoxes)[i];
        if (b.z <= b.x || (i < f.hud.size() && f.hud[i].alpha < 0.03f)) continue;
        float* o = boxes_ + boxN_++ * 4;
        o[0] = viewC_.x - viewH_.x + b.x * sx;
        o[1] = viewC_.y + viewH_.y - b.w * sy;
        o[2] = viewC_.x - viewH_.x + b.z * sx;
        o[3] = viewC_.y + viewH_.y - b.y * sy;
    }
}

void ForestUniverse::setNeon(GLuint prog) const {
    float nv[21];
    for (int i = 0; i < 7; i++) { nv[i * 3] = neon_[i].x; nv[i * 3 + 1] = neon_[i].y; nv[i * 3 + 2] = neon_[i].z; }
    glUniform3fv(ugl::U(prog, "uNeonL"), 3, nv);
    glUniform3fv(ugl::U(prog, "uNeonR"), 3, nv + 9);
    glUniform3fv(ugl::U(prog, "uNeonWarm"), 1, nv + 18);
}

void ForestUniverse::setBoxes(GLuint prog) const {
    glUniform4fv(ugl::U(prog, "uBox"), MAX_BOXES, boxes_);
    set1i(prog, "uBoxes", boxN_);
}

// The lettering, by the host (text.hpp): Cormorant Garamond Medium in spaced capitals for the names, the numbers and
// the celebrations, EB Garamond Italic for the song and the notices; the paper's white cut out of the key, the
// accents in the first glow ink, on a soft halo of the key's ink.
void ForestUniverse::hudLine(const HudText& h, text::Line& out) {
    const float s = height_ / 1080.f;
    const vec3 ink = ugl::toSrgb(paper_) * 0.88f, halo = ugl::toSrgb(key_);
    const vec3 accent = lerp(ugl::toSrgb(glow_[0]), vec3(1.f), 0.2f);
    text::Font* caps = text::font(FONT_CAPS);
    text::Font* it = text::font(FONT_IT);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    float haloA = 0.45f;
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(caps, text::upper(h.text), base * 0.74f, base * 0.3f, ink, saturate(0.3f + h.alpha * 1.5f) * 0.75f, base * 0.06f); break;
    case HUD_SECTION: put(caps, text::upper(h.text), base * 0.82f, base * 0.32f, accent, 0.3f + h.alpha * 1.7f, -base * 0.04f); break;
    case HUD_TITLE: put(it, h.text, base * 1.2f, base * 0.01f, ink, h.alpha * 1.3f, -base * 0.14f); break;
    case HUD_INFO: put(caps, text::upper(h.text), base * 0.66f, base * 0.24f, ink, saturate(alpha * 1.2f) * 0.72f); break;
    case HUD_LABEL: put(caps, text::upper(h.text), base * 0.7f, base * 0.38f, ink, saturate(alpha * 1.2f) * 0.58f); break;
    case HUD_VALUE: put(caps, h.text, base * 1.05f, base * 0.1f, ink, alpha * 0.95f, -base * 0.06f); break;
    case HUD_NOTICE: put(it, h.text, base * 1.15f, base * 0.02f, accent, alpha); break;
    case HUD_KEY: put(caps, h.text, base * 0.86f, base * 0.1f, ink, alpha); break;
    case HUD_TOAST: put(caps, text::upper(h.text), base * 1.05f, base * 0.36f, ink, alpha, -base * 0.12f); haloA = 0.6f; break;
    case HUD_TOAST_SUB: put(it, h.text, base * 1.05f, base * 0.02f, accent, alpha * 0.9f); haloA = 0.6f; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(caps, text::upper(h.text), base * 0.92f, base * 0.28f, ink, alpha);
        else put(it, h.text, base * 1.0f, base * 0.02f, h.accent ? accent : ink, alpha);
        haloA = 0.7f;
        break;
    default: put(caps, h.text, base * 0.78f, base * 0.08f, h.accent ? accent : ink, alpha); break;
    }
    // the letters a touch full (their edge a little out), the halo a smooth falloff just around them
    out.look.weight = 0.0125f;
    out.look.halo = halo;
    out.look.haloA = haloA;
    out.look.haloW = 0.075f;
    out.look.haloPow = 1.5f;
    out.alpha = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
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
    readBoxes(f);
    // a new variant eases in
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        for (int i = 0; i < 2; i++) glow_[i] = lerp(glow_[i], glowT_[i], k);
        paper_ = lerp(paper_, paperT_, k);
        key_ = lerp(key_, keyT_, k);
        dropInk_ = lerp(dropInk_, dropInkT_, k);
        slowInk_ = lerp(slowInk_, slowInkT_, k);
        for (int i = 0; i < 7; i++) piece_[i] = lerp(piece_[i], pieceT_[i], k);
        for (int i = 0; i < 7; i++) neon_[i] = lerp(neon_[i], neonT_[i], k);
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
    // the drop's third ink: in as it rolls out, lifted over a couple of seconds after the drop; the blacklight on
    // the downbeat (a swell over the first bars, then a steady level for the whole drop), off over a few seconds after
    dropInkK_ = approach(dropInkK_, dropMode_ ? 1.f : 0.f, dropMode_ ? 2.f : 0.6f, dt);
    {
        const float want = dropMode_ ? 0.8f + 0.2f * (1.f - smoothstepf(4.f * spb, 16.f * spb, dropT_)) : 0.f;
        blaze_ = approach(blaze_, want, dropMode_ ? 2.5f : 0.4f, dt);
    }
    jolt_ *= std::exp(-dt * 1.1f);
    quadGlow_ = approach(quadGlow_, 0.f, 0.45f, dt);
    float bass = 0.f;
    for (int i = 0; i < 3; i++) bass += music.bandsFast[i];
    bass = saturate(bass / 3.f * 1.6f);
    // the setup pulls the print (the key, then the glow inks, the plants cut in and lit one by one); the teardown puts
    // the light out the same way
    const float wake = smoothstepf(0.05f, 0.85f, f.setup) * (1.f - smoothstepf(0.f, 0.85f, f.teardown));
    auto awake = [&](float seed, float w) { // a plant's own turn (by its seed) to come in or go out
        const float h = seed * 0.1f;
        return smoothstepf(0.65f * h, 0.65f * h + 0.35f, w);
    };
    const float cutIn = smoothstepf(0.25f, 0.9f, f.setup);
    // the life's overall level: a steady step per section, never pulsing with the beat
    const float life = (0.55f + 0.2f * wm + 0.3f * wp + 0.25f * fest_ + 0.4f * blaze_) * (1.f - 0.3f * slow_);
    // the beat grid: a quarter of the plants glow on each two beats
    const float half = beat_ * 0.5f;
    const int halfI = (int)std::floor(half);
    const float ph = half - std::floor(half);
    const int groupNow = ((halfI % 4) + 4) % 4;
    const float env = smoothstepf(0.f, 0.16f, ph) * (1.f - smoothstepf(0.3f, 1.f, ph));
    const float pulseK = (0.2f * wm + 0.35f * wp) * (0.65f + 0.7f * bass * wp) * (1.f - 0.7f * slow_);
    // a kick at a peak lights a single plant
    if (kickIn_ > 0.3f && wp > 0.4f && !plants_.empty()) {
        for (int tries = 0; tries < 6; tries++) {
            Plant& p = plants_[rng_.irange(0, (int)plants_.size() - 1)];
            if (p.kind != 2) { p.kickGlow = std::max(p.kickGlow, kickIn_); break; }
        }
    }
    kickIn_ = 0.f;
    if (waveT_ > 14.f) waveK_ = 0.f;
    // the breeze: still at calm, livelier with the song, a gust after the drop that settles
    wind_ = approach(wind_, (0.35f + 0.35f * wm + 0.35f * wp + 0.5f * fest_) * (1.f - 0.6f * slow_), 0.3f, dt);
    const float waveR = waveT_ * WAVE_SPEED;

    // ---- The plants.
    for (Plant& p : plants_) p.kickGlow *= std::exp(-dt * 2.5f);
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
            m.r = rng_.range(0.26f, 0.38f);
            m.a = rng_.range(0.7f, 1.f);
            m.seed = rng_.uniform() * 10.f;
            m.inkB = rng_.chance(0.6f) ? 1.f : 0.f;
            motes_.push_back(m);
        }
    }
    for (Mote& m : motes_) {
        const float mdt = m.kind == 2 ? dt : wdt;
        m.t += mdt;
        const float tt = m.t - m.delay;
        if (tt <= 0.f) continue;
        if (m.kind == 0) {
            const float w = std::sin(m.t * 0.7f + m.seed) * 0.6f;
            m.p = m.p + vec2{m.v.x + w * m.v.y, m.v.y - w * m.v.x + 0.12f * std::sin(m.t * 1.3f + m.seed * 2.f)} * mdt;
            m.trailAcc += mdt;
            if (m.trailAcc > 0.16f) {
                m.trailAcc = 0.f;
                for (int k = 7; k > 0; k--) m.trail[k] = m.trail[k - 1];
                m.trail[0] = m.p;
                m.trailN = std::min(8, m.trailN + 1);
            }
        } else if (m.kind == 1) {
            m.p = m.p + vec2{m.v.x + 0.12f * std::sin(m.t * 1.1f + m.seed), m.v.y} * mdt;
        } else {
            // a chip: knocked out, falling back
            const float s = saturate(tt / m.life);
            m.p = vec2{lerpf(m.p0.x, m.target.x, 1.f - (1.f - s) * (1.f - s)), m.p0.y + (m.target.y - m.p0.y) * 4.f * s * (1.f - s)};
        }
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.t > m.delay + m.life; }), motes_.end());
    // spores rising lazily from the plants at a peak
    if (wp > 0.3f && slow_ < 0.5f) {
        const float rate = (0.6f * wp + 1.2f * fest_) * wdt;
        if (rng_.uniform() < rate && !plants_.empty()) {
            const Plant& p = plants_[rng_.irange(0, (int)plants_.size() - 1)];
            if (p.kind != 2) {
                Mote m;
                m.kind = 1;
                m.p = m.p0 = plantTop(p);
                m.v = vec2{rng_.range(-0.1f, 0.1f), rng_.range(0.25f, 0.5f)};
                m.life = rng_.range(3.f, 5.f);
                m.r = rng_.range(0.1f, 0.17f);
                m.a = 0.7f;
                m.seed = rng_.uniform() * 10.f;
                m.inkB = p.colMix;
                motes_.push_back(m);
            }
        }
    }
    for (Fade& c : clears_) c.t += dt;
    clears_.erase(std::remove_if(clears_.begin(), clears_.end(), [](const Fade& c) { return c.t > CLEAR_T; }), clears_.end());

    // ---- Instances.
    plantInst_.clear();
    halos_.clear();
    cells_.clear();
    moteInst_.clear();
    // the light each plant gives around it (a glowworm thread glows on its own, its light too thin to cut the air)
    for (const Plant& p : plants_) {
        const float grow = awake(p.seed, cutIn);
        if (grow <= 0.f) continue;
        const float dist = std::max(0.f, std::fabs(p.x) - 5.5f) + 0.25f * std::max(0.f, p.y + 10.f);
        const float arrive = dist / WAVE_SPEED;
        const float wave = waveK_ * smoothstepf(arrive - 0.4f, arrive + 0.2f, waveT_) * std::exp(-std::max(0.f, waveT_ - arrive - 0.2f) / 1.8f);
        const float pulse = p.group == groupNow ? env * pulseK : 0.f;
        const float glow = life * (1.f + pulse + 0.6f * p.kickGlow + 0.5f * quadGlow_) * awake(p.seed, wake);
        const float sc = p.scale * (1.f + 0.05f * wave);
        plantInst_.push_back({{p.x, p.y, sc, (float)p.kind}, {p.seed, grow, glow, p.flip ? 1.f : 0.f},
                              {p.colMix, tagOf(p.x), p.kind == 0 || p.kind == 4 || p.kind == 6 ? 0.85f : 0.f, wave},
                              {0.f, 0.f, 0.f, smoothstepf(0.f, 0.15f, grow)}});
        if (p.kind == 2) continue;
        const vec2 c = plantTop(p);
        const float hr = (1.3f + 0.5f * p.height) * p.scale * (1.f + 0.25f * wave + 0.15f * pulse);
        const float kindK = p.kind == 5 ? 0.6f : p.kind == 6 ? 0.7f : 1.f;
        const float hl = 0.36f * glow * grow * (1.f + 0.8f * wave) * kindK;
        halos_.push_back({{c.x, c.y, hr, hl}, {p.colMix, 0.f, 0.f, 1.f}});
    }
    if (!bare_) { // the world alone: no blocks, nothing cleared or inked over
        for (const Fade& c : clears_) addCell(c.x, c.y, 0.5f, c.type, c.l, 1.f, 2, saturate(c.t / CLEAR_T));
        if (overT_ >= 0.f)
            for (const Fade& o : outs_) {
                const float t = saturate((overT_ - o.delay) / OUT_T);
                addCell(o.x, o.y, 0.5f, o.type, o.l, 1.f, 3, t);
            }
        buildBoard(game);
    }
    for (const Mote& m : motes_) {
        const float tt = m.t - m.delay;
        if (tt <= 0.f) continue;
        if (bare_ && m.kind == 2) continue; // (a hard drop's chips)
        const float s = saturate(tt / m.life);
        float a = m.a, r = m.r, shape = 1.f, ang = 0.f;
        if (m.kind == 0) {
            // a firefly: a spark that fades in and out over its life, brightening and dimming slowly (never flashing)
            const float blink = 0.5f + 0.5f * std::sin(m.t * (0.9f + 0.4f * std::fmod(m.seed, 1.f)) + m.seed * 3.f);
            a *= smoothstepf(0.f, 0.15f, s) * (1.f - smoothstepf(0.8f, 1.f, s));
            r *= 0.55f + 0.45f * blink;
            a *= smoothstepf(0.5f, 0.8f, wake); // (the fireflies come out last, and leave first)
            // never over the well or the HUD's text
            a *= smoothstepf(5.9f, 6.8f, std::fabs(m.p.x));
            if (inHud(m.p.x, m.p.y)) a = 0.f;
            shape = 0.f;
            ang = m.seed + 0.3f * std::sin(m.t * 0.5f);
            // its trail: a dotted line of nicks along where it flew
            for (int k = 1; k < m.trailN; k += 2) {
                const vec2 q = m.trail[k];
                float ta = a * (1.f - (k + 1) / 9.f);
                ta *= smoothstepf(5.9f, 6.8f, std::fabs(q.x));
                if (inHud(q.x, q.y)) ta = 0.f;
                if (ta > 0.05f) moteInst_.push_back({{q.x, q.y, m.r * (0.32f - 0.02f * k), ta}, {m.inkB, 1.f, 0.f, 0.f}});
            }
        } else if (m.kind == 1) {
            a *= smoothstepf(0.f, 0.15f, s) * (1.f - smoothstepf(0.5f, 1.f, s));
            a *= smoothstepf(5.9f, 6.8f, std::fabs(m.p.x));
            if (inHud(m.p.x, m.p.y)) a = 0.f;
        } else {
            a *= 1.f - smoothstepf(0.6f, 1.f, s);
            shape = 2.f;
            ang = m.seed + tt * 12.f;
        }
        if (a <= 0.05f) continue;
        moteInst_.push_back({{m.p.x, m.p.y, r, a}, {m.inkB, shape, ang, 0.f}});
    }

    // ---- The plants' light.
    const float aaW = 2.f * viewH_.y / (float)height_;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    lightT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    drawInst(progHalo_, haloVao_, haloVbo_, halos_.data(), halos_.size() * sizeof(Inst2), halos_.size());

    // ---- The plates.
    plateT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progBg_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lightT_.tex);
    set1i(progBg_, "uLight", 0);
    set4f(progBg_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progBg_, "uMoon", vec3(moon_[0], moon_[1], moon_[2]));
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
    set4f(progBg_, "uGlade", glade_[0], glade_[1], glade_[2], glade_[3]);
    set1f(progBg_, "uWind", wind_);
    set1f(progBg_, "uTime", worldTime_);
    set1f(progBg_, "uSeed", seedF_);
    set1f(progBg_, "uMistK", (0.3f + 0.1f * wm + 0.14f * wp + 0.1f * fest_ + 0.16f * blaze_) * (1.f - 0.3f * slow_) * wake);
    set1f(progBg_, "uMoonK", 0.85f + 0.15f * slow_);
    set1f(progBg_, "uLife", life * wake);
    set1f(progBg_, "uWaveR", waveR);
    set1f(progBg_, "uWaveK", waveK_);
    set1f(progBg_, "uAA", aaW);
    setBoxes(progBg_);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    // the life, cut over it
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!plantInst_.empty()) {
        glUseProgram(progPlant_);
        set1f(progPlant_, "uTime", worldTime_);
        set1f(progPlant_, "uAAp", aaW * 1.2f);
        set1f(progPlant_, "uWind", wind_);
        // a margin of uncut key around each plant first (the inks under it kept), then the plants
        set1f(progPlant_, "uOut", 0.07f);
        glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
        drawInst(progPlant_, plantVao_, plantVbo_, plantInst_.data(), plantInst_.size() * sizeof(Inst), plantInst_.size());
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        set1f(progPlant_, "uOut", 0.f);
        glDisablei(GL_BLEND, 1); // (a plant's tag replaces what is behind it)
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)plantInst_.size());
        glEnablei(GL_BLEND, 1);
    }
    // fireflies, spores and chips: the deepest cut, the densest ink and the brightest neon win
    glBlendEquation(GL_MAX);
    glUseProgram(progMote_);
    set1f(progMote_, "uAA", aaW);
    drawInst(progMote_, haloVao_, haloVbo_, moteInst_.data(), moteInst_.size() * sizeof(Inst2), moteInst_.size());
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_BLEND);

    // ---- The fluorescence.
    const float inkK = smoothstepf(0.3f, 0.8f, f.setup);
    const float rollR = waveT_ * WAVE_SPEED * 1.3f + 1.f;
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, plateT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / plateT_.w, 1.5f / plateT_.h);
    set3f(progDown_, "uInkA", glow_[0]);
    set3f(progDown_, "uInkB", glow_[1]);
    set3f(progDown_, "uInkC", dropInk_);
    set1f(progDown_, "uC", dropInkK_ * (1.f - slow_));
    set1f(progDown_, "uRollR", rollR);
    set1f(progDown_, "uInkK", inkK);
    set4f(progDown_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progDown_, "uBlaze", blaze_ * (1.f - slow_));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tagTex_);
    glActiveTexture(GL_TEXTURE0);
    set1i(progDown_, "uTags", 1);
    setNeon(progDown_);
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

    // ---- The print.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, plateT_.tex);
    set1i(progFinal_, "uPlates", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set1i(progFinal_, "uBloom", 1);
    glActiveTexture(GL_TEXTURE0);
    set2f(progFinal_, "uTexel", 1.f / width_, 1.f / height_);
    {
        // the glow plates a little out of register (their own way each print), knocked further on the drop's
        // downbeat and settling back; in register in slow motion
        const float px = height_ / 1080.f * (1.f - 0.8f * slow_);
        const float a0 = seedF_ * 2.1f, a1 = a0 + 2.4f;
        const float ra = (1.4f + 5.f * jolt_) * px, rb = (1.1f + 4.f * jolt_) * px;
        set2f(progFinal_, "uOffA", std::cos(a0) * ra / width_, std::sin(a0) * ra / height_);
        set2f(progFinal_, "uOffB", std::cos(a1) * rb / width_, std::sin(a1) * rb / height_);
    }
    set3f(progFinal_, "uPaper", paper_);
    set3f(progFinal_, "uKey", key_);
    set3f(progFinal_, "uInkA", glow_[0]);
    set3f(progFinal_, "uInkB", glow_[1]);
    set3f(progFinal_, "uInkC", dropInk_);
    set3f(progFinal_, "uSlowInk", slowInk_);
    set1f(progFinal_, "uC", dropInkK_ * (1.f - slow_));
    set1f(progFinal_, "uRollR", rollR);
    set1f(progFinal_, "uInkK", inkK);
    set1f(progFinal_, "uKeyK", 0.9f + 0.1f * smoothstepf(0.f, 0.4f, f.setup));
    set1f(progFinal_, "uBloomK", ((0.08f + 0.04f * wp) * (1.f - blaze_) + 0.4f * blaze_) * (1.f - 0.5f * slow_));
    set1f(progFinal_, "uBlaze", blaze_ * (1.f - slow_));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tagTex_);
    glActiveTexture(GL_TEXTURE0);
    set1i(progFinal_, "uTags", 2);
    setNeon(progFinal_);
    set4f(progFinal_, "uGlade", glade_[0], glade_[1], glade_[2], glade_[3]);
    set1f(progFinal_, "uSlow", slow_);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade) * smoothstepf(0.f, 0.35f, f.setup) * (1.f - 0.75f * smoothstepf(0.4f, 1.f, f.teardown)));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set4f(progFinal_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progFinal_, "uTime", worldTime_);
    set1f(progFinal_, "uSeed", seedF_);
    set1f(progFinal_, "uAA", aaW);
    set1f(progFinal_, "uGS", groundSeed_);
    set3f(progFinal_, "uRock", vec3(rock_[0], rock_[1], rock_[2]));
    set4f(progFinal_, "uPool", pool_[0], pool_[1], pool_[2], 0.f);
    setBoxes(progFinal_);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // the blocks, printed in register over it
    glUseProgram(progPiece_);
    set3f(progPiece_, "uKey", key_);
    set3f(progPiece_, "uBridgeInk", bridge_.ink);
    set1f(progPiece_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set1f(progPiece_, "uFade", saturate(f.fade));
    set1f(progPiece_, "uDim", dimT_);
    drawInst(progPiece_, pieceVao_, pieceVbo_, cells_.data(), cells_.size() * sizeof(Inst), cells_.size());
    drawHud(f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_forest() {
    static const UniverseEntry e{
        "GLOWING FOREST", "a night forest as a relief print in black ink and fluorescent inks, neon-tube blocks; on the drop the forest turns to neon line art",
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
