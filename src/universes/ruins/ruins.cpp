// RUINS: ancient ruins in a jungle, seen in 3D behind the well. Nothing grows and nothing gets built: the ruins are
// simply there, and the camera drifts slowly among them over the song (a slow orbit and dolly around the temple at
// the end of an avenue of columns, revealing it from changing angles). The pieces are carved stone column drums,
// one per cell, each piece type its own stone. Leaves, lianas and light shafts sway gently with the music, the mist
// drifts, birds take off from the trees on the strong beats of a peak; a cleared row's drums crack and crumble, their
// chips and dust falling away behind the well. The drop turns the sun to golden hour for the whole drop section (a
// low warm sun raking the stones, long shadows, golden shafts and glowing mist), easing back when it ends.
//
// The world is one ray-marched pass at a reduced resolution (shaders.hpp): the temple (a Maya pyramid, a Khmer
// temple mountain with a strangler fig, a Doric temple), the avenue, the trunks and the ground as signed distance
// fields; the foliage as layered planes with leafy masks; the canopy a two-layer leaf mask that also dapples the
// sun on everything below it and carves the light shafts in the mist.
#include "universes/ruins/ruins.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/ruins/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Marcellus (labels, numbers) and Cormorant Garamond Italic (the song, the variant, the notices), SIL Open Font
// License, see OFL-Marcellus.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/ruins/Marcellus-Regular.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/ruins/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = ruins_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;
using Palette = RuinsUniverse::Palette;

GLuint compile(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, shaders::COMMON_GLSL);
    std::string v(vs);
    v.insert(v.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): its temple (0 Maya, 1 Khmer, 2 Greek), its stone, its jungle, its light, its seven drums
// (sandstone, ochre, jade, slate blue, terracotta, white marble, rose granite, each tuned to the place).
struct VariantDef {
    const char* name;
    int kind;
    unsigned stone, stone2, moss, moss2, leaf, leafLit, bark, ground, skyZ, skyH, fog, sun, amb;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float fogK, gap;
};
const VariantDef VARIANTS[] = {
    // MAYA: a grey limestone pyramid in a lush green rainforest
    {"MAYA", 0, 0xb4ab96, 0x625d50, 0x6f8c32, 0x2f4a1a, 0x1f3f1c, 0xb6d84a, 0x5c5248, 0x4a3f2a, 0x6c9cc8, 0xd6e6dc,
     0xc4ccc2, 0xfff0d2, 0x7a92a8,
     0xf6eedc, 0x9ad8a6, 0x0a0f0a,
     {0xe6c890, 0xdc9c40, 0x58b088, 0x6e92c8, 0xcc6a4c, 0xeee8dc, 0xdc8e9a}, 0.006f, 0.1f},
    // KHMER: a sandstone temple mountain, a strangler fig pouring its roots over it, humid mist
    {"KHMER", 1, 0xa89a80, 0x4e4a40, 0x7a8a3a, 0x3a4a22, 0x22381c, 0xc0d050, 0xb2aa9c, 0x544730, 0x86a2b4, 0xe2e4d2,
     0xcccbb8, 0xfff0d4, 0x8494a0,
     0xf8f0dc, 0xf2bc66, 0x0d0e09,
     {0xdec28e, 0xd8963a, 0x50ac92, 0x7088bc, 0xc46248, 0xeae4d6, 0xd48ca0}, 0.008f, 0.12f},
    // GREEK: a white marble temple under umbrella pines and cypresses, a Mediterranean blue sky
    {"GREEK", 2, 0xdcd4c4, 0x8a8274, 0x7e8a3c, 0x4a5326, 0x2c4424, 0xb0c060, 0x5e4c3c, 0x8a7656, 0x4c84c6, 0xdce6ee,
     0xd0dae4, 0xfff4e0, 0x8aa2be,
     0xfaf6ee, 0xe8d08c, 0x0b0d10,
     {0xeccf9a, 0xe4a448, 0x62ba96, 0x789ad4, 0xd4705a, 0xf6f2ea, 0xe49aa8}, 0.004f, 0.2f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned x) { return ugl::toLinear(hex(x)); };
    Palette p;
    p.stone = L(d.stone); p.stone2 = L(d.stone2); p.moss = L(d.moss); p.moss2 = L(d.moss2); p.leaf = L(d.leaf);
    p.leafLit = L(d.leafLit); p.bark = L(d.bark); p.ground = L(d.ground); p.skyZ = L(d.skyZ); p.skyH = L(d.skyH);
    p.fog = L(d.fog); p.sun = L(d.sun); p.amb = L(d.amb); p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    vec3* pa = &a.stone;
    const vec3* pb = &b.stone;
    for (int i = 0; i < (int)(sizeof(Palette) / sizeof(vec3)); i++) pa[i] = lerp(pa[i], pb[i], k);
}

constexpr float CLEAR_T = 0.5f; // a cleared drum crumbles away in this long (real time)

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool RuinsUniverse::init(int width, int height) {
    progWorld_ = compile(ugl::FULLSCREEN_VS, shaders::WORLD_FS, "ruins world");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "ruins comp");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "ruins sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "ruins veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "ruins cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "ruins shape");
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "ruins text");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "ruins post");

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
    buildFont(ital_, FONT_ITAL);
    pal_ = paletteOf(0);

    // the butterflies: a few, each with its home beside the well
    Rng r(0xB077);
    for (int i = 0; i < 4; i++) {
        Fly f;
        const float side = (i % 2) ? 1.f : -1.f;
        f.home = V(side * r.range(12.f, 16.5f), r.range(-8.5f, -1.f));
        f.p = f.home;
        f.phase = r.range(0.f, TAU);
        f.seed = r.uniform();
        f.hue = (float)i;
        flies_.push_back(f);
    }
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void RuinsUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] ruins font init failed\n");
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
        for (int rr = 0; rr < h; rr++) std::memcpy(&atlas[(y + rr) * AW + x], sdf + rr * w, w);
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

void RuinsUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * (float)width_ / height_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the world at a reduced resolution (at most 600 lines)
    const int wh = std::max(180, std::min(600, (int)(height_ * 0.56f)));
    const int ww = std::max(320, (int)std::lround((float)wh * width_ / height_));
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    aspect_ = (float)width_ / height_;
    updateCamera();
}

// The drift: a slow orbit around the temple's court (about 0.3 rad either way over ~4 minutes), a slow dolly in and
// out, the eye's height breathing a little; it looks a little up, at the temple, a little ahead of its swing.
void RuinsUniverse::updateCamera() {
    const float t = camPhase_;
    const float ang = side_ * 0.12f + 0.24f * std::sin(t * 0.026f) + 0.06f * std::sin(t * 0.017f + 1.3f);
    const float R = 64.f + 5.f * std::sin(t * 0.019f + 0.7f);
    const vec3 C(0.f, 0.f, -50.f);
    eye_ = C + vec3(std::sin(ang) * R, 3.6f + 1.0f * std::sin(t * 0.023f + 2.f), std::cos(ang) * R);
    // the temple beside the well (on this song's side), not hidden behind it
    const vec3 toC = normalize(vec3(C.x - eye_.x, 0.f, C.z - eye_.z));
    const vec3 r0 = normalize(cross(toC, vec3(0.f, 1.f, 0.f)));
    const vec3 target = C + vec3(0.f, 10.5f, 0.f) - r0 * (side_ * 21.f);
    fwd_ = normalize(target - eye_);
    right_ = normalize(cross(fwd_, vec3(0.f, 1.f, 0.f)));
    up_ = cross(right_, fwd_);
    tanH_ = 0.43f;
}

RuinsUniverse::~RuinsUniverse() {
    for (ugl::Target* t : {&worldT_, &sceneT_}) t->destroy();
    for (GLuint p : {progWorld_, progComp_, progSprite_, progVeil_, progCell_, progShape_, progText_, progPost_}) glDeleteProgram(p);
    for (GLuint v : {spriteVao_, cellVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, cellVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&caps_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void RuinsUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.brightness > 0.6f && t.airWeight > 0.5f) v = 2;           // bright, airy songs: the marble
        else if (t.hasSong && t.bassWeight > 0.6f && (t.minor || t.dynamics > 0.5f)) v = 1; // deep ones: the fig's temple
        else {
            static const int BY_HUE[] = {0, 1, 0, 2, 0, 1};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x2B1E5ull);
    // each song starts the drift at its own place along the orbit
    if (seconds <= 0.f) {
        palSet_ = false;
        camPhase_ = (float)((seed_ >> 20) % 1000) * 0.24f;
        side_ = ((seed_ >> 12) & 1) ? 1.f : -1.f;
    }
}

void RuinsUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

void RuinsUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
    releaseBirds(9, 0);
}

void RuinsUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 RuinsUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints RuinsUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.fog, p.leaf, 0.55f) * 0.55f;
    h.light = false;
    h.ink = p.ink;
    h.accent = p.accent;
    h.styles = TS_INK | TS_LUMA | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Events
// =====================================================================================================

void RuinsUniverse::releaseBirds(int n, int side) {
    // from the trees beside the well (one side, or both), up and away
    for (int i = 0; i < n && birds_.size() < 40; i++) {
        const float s = side != 0 ? (float)side : (i % 2 ? 1.f : -1.f);
        Bird b;
        b.p = V(s * rng_.range(11.5f, 17.f), rng_.range(1.f, 7.f));
        b.v = V(s * rng_.range(1.0f, 3.2f), rng_.range(2.2f, 4.0f));
        b.age = -rng_.range(0.f, 0.5f);
        b.phase = rng_.range(0.f, TAU);
        b.size = rng_.range(0.22f, 0.36f);
        b.glide = rng_.uniform();
        birds_.push_back(b);
    }
}

void RuinsUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::HardDrop: {
        // a little dust pushed out from under the piece
        float yLo = 99.f, xs = 0.f;
        int n = 0;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f) continue;
            xs += p.x;
            n++;
        }
        if (!n) break;
        xs /= n;
        for (int i = 0; i < 6; i++) {
            Bit b;
            b.p = V(xs + rng_.range(-0.5f, 0.5f) * n * 0.5f, yLo - 0.5f);
            b.v = V(rng_.range(-1.f, 1.f), rng_.range(0.05f, 0.3f));
            b.age = 0.f;
            b.life = rng_.range(0.5f, 0.9f);
            b.size = rng_.range(0.16f, 0.26f);
            b.spin = 0.f;
            b.seed = rng_.uniform();
            b.col = lerp(pal_.stone, pal_.ink, 0.3f);
            b.mode = 0;
            b.alpha = 0.25f;
            puffs_.push_back(b);
        }
        break;
    }
    case GameEvent::Clear: {
        int nCells = 0;
        for (auto& cl : ev.cells) {
            if (cl.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)cl.x, (float)cl.y);
            const int type = std::clamp(cl.type, 0, 6);
            crumbling_.push_back({p.x, p.y, type, 0.f, rng_.uniform()});
            nCells++;
            // chips falling away behind the well, smaller as they recede
            for (int k = 0; k < 3; k++) {
                Bit b;
                b.p = V(p.x + rng_.range(-0.35f, 0.35f), p.y + rng_.range(-0.3f, 0.3f));
                b.v = V(rng_.range(-1.6f, 1.6f), rng_.range(0.2f, 2.4f));
                b.age = -rng_.range(0.05f, 0.3f);
                b.life = rng_.range(0.9f, 1.4f);
                b.size = rng_.range(0.07f, 0.16f);
                b.spin = rng_.range(-6.f, 6.f);
                b.seed = rng_.uniform();
                b.col = pal_.piece[type] * rng_.range(0.6f, 0.9f);
                b.mode = 2;
                b.alpha = 1.f;
                chips_.push_back(b);
            }
            if (nCells % 2 == 0) {
                Bit b;
                b.p = V(p.x, p.y - 0.2f);
                b.v = V(rng_.range(-0.4f, 0.4f), rng_.range(-0.4f, 0.1f));
                b.age = 0.f;
                b.life = rng_.range(1.0f, 1.5f);
                b.size = rng_.range(0.7f, 1.0f);
                b.spin = 0.f;
                b.seed = rng_.uniform();
                b.col = lerp(pal_.stone, pal_.piece[type], 0.4f);
                b.mode = 0;
                b.alpha = 0.3f;
                dust_.push_back(b);
            }
        }
        // the noise startles a few birds; a Tetris a whole flock
        if (ev.count >= 4) releaseBirds(12, 0);
        else if (rng_.chance(0.4f)) releaseBirds(3 + rng_.irange(0, 3 - 1), rng_.chance(0.5f) ? 1 : -1);
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
        overCells_.clear();
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.x < 0 || c.x >= Game::W || c.y >= Game::H) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            overCells_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), (float)c.y});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void RuinsUniverse::cell(float x, float y, float half, const vec3& tint, float alpha, int mode, float erase, float seed) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, seed}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, 0.f, 0.f}});
}

void RuinsUniverse::buildBoard(const Game& g) {
    auto tintOf = [&](int t) { return pal_.piece[std::clamp(t, 0, 6)]; };
    auto seedOf = [](int x, int y) { return (float)((x * 7 + y * 13) % 17) / 17.f; };
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            const vec3 p = layout::cellPos((float)x, y - off);
            cell(p.x, p.y, 0.5f, tintOf(c.type), 1.f, 0, 0.f, seedOf(x, y));
        }
    // game over: the drums grey and darken from the top down, like stones long fallen
    if (overT_ >= 0.f)
        for (const Over& o : overCells_) {
            const float rowFrac = (o.row - Game::HIDDEN) / (float)(Game::H - Game::HIDDEN);
            const float k = smoothstepf(rowFrac * 0.6f, rowFrac * 0.6f + 0.3f, over_);
            cell(o.x, o.y, 0.5f, tintOf(o.type), 1.f - 0.4f * smoothstepf(0.7f, 1.f, over_), 0, 0.f, 0.f);
            cells_.back().c[2] = k;
        }
    for (const Crumble& c : crumbling_) cell(c.x, c.y, 0.5f, tintOf(c.type), 1.f, 0, std::max(0.001f, saturate(c.t / CLEAR_T)), c.seed);
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cell(w.x, w.y, 0.5f, tintOf(p.type), 0.6f, 1, 0.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, tintOf(p.type), 1.f, 0, 0.f, seedOf(c[0], c[1]));
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
        for (auto& c : cells) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            cell(px, py, 0.5f * sc, tintOf(type), alpha, 0, 0.f, seedOf(c[0], c[1]));
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void RuinsUniverse::sprite(std::vector<SpriteInst>& to, vec2 p, float size, const vec3& col, float alpha, int mode, float param,
                           float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    to.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

void RuinsUniverse::drawSprites(const std::vector<SpriteInst>& list) {
    if (list.empty()) return;
    glUseProgram(progSprite_);
    setMat(progSprite_, "uVP", vp_);
    set1f(progSprite_, "uPxPerUnit", pxPerUnit_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, list.size() * sizeof(SpriteInst), list.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)list.size());
}

// =====================================================================================================
// Text
// =====================================================================================================

float RuinsUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void RuinsUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void RuinsUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float halo) {
    if (q.empty() || !f.ok || alpha <= 0.004f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    set3f(progText_, "uInk", lerp(ink, bridge_.ink, b));
    set3f(progText_, "uHalo", lerp(pal_.veil * 0.7f, bridge_.color, b));
    set1f(progText_, "uHaloA", halo);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD: Marcellus' carved capitals, widely spaced (like an inscription); the song, the variant and the notices
// in Cormorant Garamond's italics. The universe's line ends with the hour of the light.
void RuinsUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
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
    const float halo = 0.9f;
    const char* hour = slow_ > 0.5f ? "blue hour" : (gold_ > 0.5f ? "golden hour" : "late morning");
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
            const float vsz = base * 1.35f;
            const float wVar = vt.empty() ? 0.f : textWidth(ital_, vt, vsz, 0.f);
            const float gap = base * 0.7f;
            const float wW = textWidth(ital_, hour, vsz, 0.f);
            const float total = wName + (vt.empty() ? 0.f : gap + wVar) + gap * 1.4f + wW;
            const float x = ugl::hudLeft(h, total, width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(caps_, name, x, h.y, sz, tr, q);
            drawText(caps_, q, ink, a, halo);
            float cx = x + wName;
            if (!vt.empty()) {
                q.clear();
                textQuads(ital_, vt, cx + gap, h.y - sz * 0.3f, vsz, 0.f, q);
                drawText(ital_, q, accent, a * 0.92f, halo);
                cx += gap + wVar;
            }
            cx += gap * 1.4f;
            q.clear();
            textQuads(ital_, hour, cx, h.y - sz * 0.3f, vsz, 0.f, q);
            drawText(ital_, q, ink, a * 0.7f, halo);
            continue;
        }
        case HUD_SECTION:
            place(caps_, h.text, base * 0.9f, base * 0.5f);
            drawText(caps_, q, ink, saturate(0.2f + h.alpha * 1.9f), halo);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.8f, base * 0.01f, -base * 0.4f);
            drawText(ital_, q, ink, saturate(h.alpha * 1.3f), halo);
            continue;
        case HUD_INFO:
            place(caps_, h.text, base * 0.66f, base * 0.16f);
            drawText(caps_, q, ink, saturate(alpha * 1.3f) * 0.8f, halo);
            continue;
        case HUD_LABEL:
            place(caps_, h.text, base * 0.82f, base * 0.45f);
            drawText(caps_, q, lerp(ink, accent, 0.55f), saturate(alpha * 1.2f) * 0.95f, halo);
            continue;
        case HUD_VALUE:
            place(caps_, h.text, base * 1.08f, base * 0.08f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_NOTICE:
            place(ital_, h.text, base * 1.7f, base * 0.01f);
            drawText(ital_, q, accent, alpha, halo);
            continue;
        case HUD_KEY:
            place(caps_, h.text, base * 0.75f, base * 0.1f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_TOAST:
            place(caps_, h.text, base * 1.3f, base * 0.5f, -base * 0.2f);
            drawText(caps_, q, ink, alpha, 0.85f);
            continue;
        case HUD_TOAST_SUB:
            place(ital_, h.text, base * 1.5f, base * 0.01f);
            drawText(ital_, q, accent, alpha * 0.9f, 0.85f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(caps_, h.text, base * 1.15f, base * 0.3f, -base * 0.1f);
                drawText(caps_, q, ink, alpha, 0.85f);
            } else {
                place(caps_, h.text, base * 0.85f, base * 0.16f);
                drawText(caps_, q, h.accent ? accent : ink, alpha, 0.85f);
            }
            continue;
        default:
            place(caps_, h.text, base * 0.85f, base * 0.14f);
            drawText(caps_, q, h.accent ? accent : ink, alpha, halo);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void RuinsUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    // ZEN_GPUTIME: the GPU time of a frame, printed every 60 frames
    static const bool gpuTime = std::getenv("ZEN_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) {
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
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = overT_ < 0.f && (sectionKnown ? sectionDrop : dropMode_ && (!music.hasSong || music.speed > 1.26f));
    // the golden hour: in over a few seconds when the drop lands, held for the whole drop, out slowly after it
    gold_ = approach(gold_, isDrop ? 1.f : 0.f, isDrop ? 0.9f : 0.3f, dt);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { pal_ = target; palSet_ = true; }
        else lerpPalette(pal_, target, 1.f - std::exp(-dt * 1.0f));
    }
    double beat;
    if (music.hasSong) beat = music.beat;
    else {
        if (!f.paused) ownBeat_ += dt * 2.0;
        beat = ownBeat_;
    }
    {
        const float k = std::max(music.kick, kickIn_) * wp;
        kickS_ = approach(kickS_, k, k > kickS_ ? 5.f : 1.2f, dt);
    }
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    // the leaves sway more as the music rises (slow envelopes; the phase runs a little faster too)
    swayAmp_ = approach(swayAmp_, 0.35f + 0.4f * wm + 0.35f * wp + 0.2f * gold_, 0.5f, dt);
    sway_ += wdt * (0.6f + 0.5f * wm + 0.4f * wp);
    // the drift
    if (!f.paused) camPhase_ += wdt * (1.f + 0.25f * wm + 0.2f * wp);
    updateCamera();

    // ---- Birds: on strong beats of a peak (every two beats at most, a cooldown between flocks), now and then
    // in calmer parts.
    birdCool_ -= wdt;
    {
        const double half = std::floor(beat / 2.0);
        if (half != lastHalf_) {
            lastHalf_ = half;
            if (wp > 0.5f && birdCool_ <= 0.f && std::max(music.kick, kickIn_) > 0.5f) {
                releaseBirds(4 + rng_.irange(0, 4 - 1), rng_.chance(0.5f) ? 1 : -1);
                birdCool_ = 6.f;
            }
        }
        calmBirds_ -= wdt;
        if (calmBirds_ <= 0.f) {
            releaseBirds(2 + rng_.irange(0, 2 - 1), rng_.chance(0.5f) ? 1 : -1);
            calmBirds_ = rng_.range(14.f, 26.f);
        }
    }
    for (Bird& b : birds_) {
        b.age += wdt;
        if (b.age < 0.f) continue;
        b.p = V(b.p.x + b.v.x * wdt, b.p.y + b.v.y * wdt);
        b.v = V(b.v.x * (1.f + 0.15f * wdt), b.v.y * std::exp(-0.25f * wdt));
        const float glide = b.age > 1.2f && std::sin(b.age * 1.3f + b.glide * 6.f) > 0.6f ? 0.15f : 1.f;
        b.phase += wdt * 15.f * glide;
    }
    birds_.erase(std::remove_if(birds_.begin(), birds_.end(),
                                [&](const Bird& b) { return b.age > 7.f || std::fabs(b.p.x) > viewH_.x + 1.5f || b.p.y > viewH_.y + 1.5f; }),
                 birds_.end());
    for (Fly& fl : flies_) {
        const float t = time_ * (0.5f + 0.25f * wm + 0.25f * wp);
        fl.p = V(fl.home.x + 1.6f * std::sin(t * 0.37f + fl.phase) + 0.6f * std::sin(t * 1.1f + fl.seed * 9.f),
                 fl.home.y + 1.1f * std::sin(t * 0.29f + fl.phase * 1.7f) + 0.35f * std::sin(t * 1.7f + fl.seed * 4.f));
        fl.phase += 0.f;
    }

    // ---- Dust, chips, crumbling drums (at the real speed).
    auto stepBits = [&](std::vector<Bit>& v, float grav, float drag) {
        for (Bit& b : v) {
            b.age += dt;
            if (b.age < 0.f) continue;
            b.v = V(b.v.x * std::exp(-drag * dt), (b.v.y - grav * dt) * std::exp(-drag * dt));
            b.p = V(b.p.x + b.v.x * dt, b.p.y + b.v.y * dt);
            b.spin += 0.f;
        }
        v.erase(std::remove_if(v.begin(), v.end(), [](const Bit& b) { return b.age >= b.life; }), v.end());
    };
    stepBits(chips_, 13.f, 0.3f);
    stepBits(dust_, -0.2f, 1.2f);
    stepBits(puffs_, 0.f, 2.5f);
    for (Crumble& c : crumbling_) c.t += dt;
    crumbling_.erase(std::remove_if(crumbling_.begin(), crumbling_.end(), [](const Crumble& c) { return c.t > CLEAR_T; }), crumbling_.end());
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);

    // ---- The light: late morning, high sun behind the temple (the shafts toward us); the drop's golden hour, a
    // low warm sun on the left raking across the stones; slow motion a cool blue hush.
    const int kind = VARIANTS[wrapV(variant_)].kind;
    const float g = smoothstepf(0.f, 1.f, gold_);
    vec3 sunDir = normalize(lerp(normalize(vec3(-0.5f, 0.76f, 0.3f)), normalize(vec3(-0.82f, 0.2f, 0.36f)), g));
    vec3 sunC = lerp(pal_.sun * 2.5f, vec3(3.8f, 1.85f, 0.6f), g);
    vec3 skyZ = lerp(pal_.skyZ, vec3(0.42f, 0.42f, 0.52f), g);
    vec3 skyH = lerp(pal_.skyH, vec3(1.25f, 0.78f, 0.34f), g);
    vec3 amb = lerp(pal_.amb * 0.9f, pal_.amb * vec3(0.95f, 0.66f, 0.5f) * 0.7f, g);
    vec3 fog = lerp(pal_.fog, vec3(1.0f, 0.7f, 0.36f), g);
    float fogK = VARIANTS[wrapV(variant_)].fogK * (1.f + 0.15f * g) * (1.f + 0.25f * (1.f - wm));
    float mist = lerpf(lerpf(1.4f, 0.9f, wm), 0.6f, wp) * (1.f + 0.1f * g);
    float shaft = (1.f + 0.25f * wp) * (1.f + 1.1f * g);
    sunC = sunC * 1.2f;
    float gap = VARIANTS[wrapV(variant_)].gap + 0.16f * g;
    {
        const vec3 hush(0.55f, 0.7f, 1.15f);
        sunC = lerp(sunC, sunC * hush * 0.45f, slow_);
        amb = lerp(amb, amb * hush * 0.8f, slow_);
        skyH = lerp(skyH, skyH * hush * 0.7f, slow_);
        skyZ = lerp(skyZ, skyZ * hush * 0.7f, slow_);
        fog = lerp(fog, fog * hush * 0.7f, slow_);
        // the end of a game: the light fades a little
        sunC = sunC * (1.f - 0.4f * over_);
    }

    // ---- Sprites.
    back_.clear();
    front_.clear();
    const vec3 birdC = lerp(pal_.leaf * 0.25f, vec3(0.05f, 0.03f, 0.02f), g);
    if (!f.bare) {
        for (const Bit& b : chips_) {
            if (b.age < 0.f) continue;
            const float t = b.age / b.life;
            // receding as it falls: smaller, darker, fading into the haze
            const float sz = b.size * (1.f - 0.55f * t);
            sprite(back_, b.p, sz, b.col * (1.f - 0.5f * t), 1.f - smoothstepf(0.6f, 1.f, t), 2, b.spin * b.age + b.seed * 6.f, b.seed);
        }
        for (const Bit& b : dust_) {
            const float t = b.age / b.life;
            sprite(back_, b.p, b.size * (1.f + 0.8f * t), b.col, b.alpha * smoothstepf(0.f, 0.12f, t) * (1.f - smoothstepf(0.3f, 1.f, t)), 0, b.seed);
        }
        for (const Bit& b : puffs_) {
            const float t = b.age / b.life;
            sprite(front_, b.p, b.size * (1.f + t), b.col, b.alpha * smoothstepf(0.f, 0.1f, t) * (1.f - smoothstepf(0.5f, 1.f, t)), 0, b.seed);
        }
    }
    for (const Bird& b : birds_) {
        if (b.age < 0.f) continue;
        const float a = smoothstepf(0.f, 0.3f, b.age) * (1.f - smoothstepf(5.5f, 7.f, b.age)) * 0.9f * world;
        sprite(back_, b.p, b.size * (1.f - 0.25f * saturate(b.age / 6.f)), birdC, a, 3, b.phase);
    }
    {
        static const unsigned FLY_C[4] = {0x5aa8ff, 0xffd060, 0xff8a40, 0xf4f0e0};
        for (const Fly& fl : flies_) {
            const vec3 c = ugl::toLinear(hex(FLY_C[(int)fl.hue & 3])) * lerpf(0.8f, 1.1f, g);
            sprite(back_, fl.p, 0.2f, c, 0.85f * world * (1.f - over_), 4, time_ * 9.f + fl.phase);
        }
    }
    if (!f.bare) {
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        sprite(front_, V(gc.x, gc.y), layout::GAUGE_R * 1.25f, on ? pal_.ink : pal_.accent, 1.f, 1, fill, on ? 1.f : 0.f);
    }
    cells_.clear();
    if (!f.bare) buildBoard(game);

    // =========================================================== the world (reduced resolution)
    {
        worldT_.bind();
        glDisable(GL_BLEND);
        const GLuint P = progWorld_;
        glUseProgram(P);
        set3f(P, "uEye", eye_);
        set3f(P, "uFwd", fwd_);
        set3f(P, "uRight", right_);
        set3f(P, "uUp", up_);
        set1f(P, "uTanH", tanH_);
        set1f(P, "uAspect", aspect_);
        set1f(P, "uTime", std::fmod(sway_, 2000.f));
        set1f(P, "uPx", 2.f * tanH_ / worldT_.h);
        set1i(P, "uKind", kind);
        set3f(P, "uSunDir", sunDir);
        set3f(P, "uSunCol", sunC);
        set3f(P, "uSkyZ", skyZ);
        set3f(P, "uSkyH", skyH);
        set3f(P, "uAmb", amb);
        set3f(P, "uFog", fog);
        set3f(P, "uStone", pal_.stone);
        set3f(P, "uStone2", pal_.stone2);
        set3f(P, "uMoss", pal_.moss);
        set3f(P, "uMoss2", pal_.moss2);
        set3f(P, "uLeaf", pal_.leaf);
        set3f(P, "uLeafLit", pal_.leafLit);
        set3f(P, "uBark", pal_.bark);
        set3f(P, "uGround", pal_.ground);
        set1f(P, "uFogK", fogK);
        set1f(P, "uMist", mist);
        set1f(P, "uShaft", shaft);
        set1f(P, "uGap", gap);
        set1f(P, "uSway", swayAmp_);
        set1f(P, "uGold", g);
        set1f(P, "uWorldA", world);
        set3f(P, "uBridgeCol", bridge_.color);
        ugl::drawFullscreen();
    }

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progComp_, "uWorld", 0);
    set2f(progComp_, "uTexel", 1.f / worldT_.w, 1.f / worldT_.h);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // behind the well: the chips and dust falling away, the birds, the butterflies
    drawSprites(back_);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(lerp(pal_.stone, pal_.accent, 0.3f) * 0.45f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", 0.8f);
        set1f(progVeil_, "uColA", 0.42f * world);
        set1f(progVeil_, "uRailA", 0.5f);
        set1f(progVeil_, "uPx", pxPerUnit_);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.f, layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set3f(progCell_, "uLightC", vec3(1.f, 0.96f, 0.88f));
        set3f(progCell_, "uMossC", lerp(pal_.moss, pal_.moss2, 0.3f) * 1.3f);
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    drawSprites(front_);
    drawHud(f.hud, f.panels);
    glDisable(GL_BLEND);
    if (gpuQ) {
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
        static double acc = 0.0;
        static int n = 0;
        acc += ns * 1e-6;
        if (++n == 60) {
            std::fprintf(stderr, "[ruins] gpu %.2f ms/frame (%dx%d world)\n", acc / n, worldT_.w, worldT_.h);
            acc = 0.0;
            n = 0;
        }
    }

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

const UniverseEntry& universe_ruins() {
    static const UniverseEntry e{
        "RUINS", "ancient ruins in a jungle, in 3D: a temple, an avenue of broken columns, roots, moss, mist and light shafts; the blocks are carved column drums, and the drop turns the sun to golden hour",
        [] { return std::unique_ptr<Universe>(new RuinsUniverse()); },
        // Fits most songs; the calm, contemplative ones a little better.
        [](const SongLook& l) { return 0.8f + 0.25f * (1.f - l.density) + 0.1f * (1.f - l.dynamics); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
