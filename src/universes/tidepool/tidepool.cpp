// TIDE POOL: attention as a journey. Behind the well, a humble rock pool at low tide, almost nothing to see: a
// little boulder in clear water. Every cleared line zooms deeper into it, one continuous dive inward through nine
// scales: the pool; the boulder, beadlet anemones swaying on its flanks, limpets, a shore crab, a drop of water
// glistening on the rock; inside the drop, plankton (copepods, nauplii, chains of diatoms); a diatom's glass
// frustule, a rose window of pores with the golden cell glowing behind them; the cell's cytoplasm streaming,
// chloroplasts and granules; molecules jostling, a double helix winding into the focus like a spiral staircase of
// light; an atom, its orbital clouds breathing in nested shells; inside the nucleus, a boiling quantum foam whose
// bubbles burst into ripples that interfere like rain on a pool; strings of light vibrating to the music; and in
// the smallest loop of string, a rock pool again: the zoom never ends.
//
// The dive is one similarity transform: depth z (octaves of zoom) drifts slowly inward all the time, each cleared
// line pushes it 2/3 of an octave deeper (eased by a spring), a Tetris plunges through a whole stage. Each stage
// lives in its own frame, 16 times smaller than the one before, in a disc around the focus of the zoom; the
// shader draws up to three nested stages per pixel, innermost first (shaders.hpp). The focus sits right of the
// well's HUD column, wandering slowly; the zoom turns a little as it dives. Game over: the zoom pulls all the way
// back out to the rock pool, and the next game starts there.
//
// The music is the tide: the water sloshes once a bar at every scale (the waterline creeping over the rock, the
// anemones opening, the plankton hopping every two beats, the light sweeping round the glass, the cytoplasm
// streaming, the molecules' Brownian jitter, the orbitals breathing, the foam boiling, the strings ringing in
// harmonics fed by the song's bands). The drop is a wave: water washing up over the boulder with a lace of foam,
// and deeper down a slow ripple through the fabric. Kicks (in a peak only) light small things: dinoflagellates
// sparking, glints in the diatom's pores, virtual pairs of particles, the strings plucked.
//
// The pieces are sea glass: frosted, worn round, the water's light moving through them. Cleared rows melt into
// bubbles.
#include "universes/tidepool/tidepool.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/tidepool/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Instrument Serif (the labels, the song, the notices) and DM Mono Light (the numbers, the scale), SIL Open Font
// License, see OFL-InstrumentSerif.txt and OFL-DMMono.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/tidepool/InstrumentSerif-Regular.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/tidepool/InstrumentSerif-Italic.ttf.inc"
};
const unsigned char FONT_MONO[] = {
#include "universes/tidepool/DMMono-Light.ttf.inc"
};

namespace shaders = tidepool_shaders;
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

vec2 V(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): the deep, the water, the shallows, the light; rock, sand; the warm life (anemones), the golden
// one (ochre, chloroplasts), the weed, the cool accent, the glow; the HUD's ink and accent, the veil; the seven
// pieces; bioluminescence, fever (the hues turning with the depth).
struct VariantDef {
    const char* name;
    unsigned deep, water, shallow, light, rock, sand, warm, warm2, weed, cool, glow;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float lum, fever;
};
const VariantDef VARIANTS[] = {
    // A clear morning: teal water over warm sand, rose anemones, golden light.
    {"MORNING", 0x0b2a33, 0x2c8a8f, 0x9fe0d6, 0xfff1cf, 0x5f5b58, 0xd9c39a, 0xe8566a, 0xd29a4a, 0x5f7d3a, 0x56b8e8, 0xffe7a6,
     0xf6f1e6, 0xffcf96, 0x082026,
     {0x7fd6c8, 0xf2d27a, 0xb59be8, 0x8ed58a, 0xf28f8f, 0x7aa8f0, 0xf5b06a}, 0.f, 0.f},
    // Midnight: black water lit by living things, magenta anemones, the plankton sparking blue-green.
    {"MIDNIGHT", 0x020812, 0x07263c, 0x1f6582, 0xcfe6ff, 0x1d2532, 0x52626c, 0xc84aa0, 0x7d8c5c, 0x23463a, 0x40d8ff, 0x5effd6,
     0xe4f2ff, 0x6ff0d8, 0x020a14,
     {0x5ef0e0, 0xffe48a, 0xc8a0ff, 0x7cf2a0, 0xff8ab0, 0x6aa8ff, 0xffb07a}, 1.f, 0.f},
    // The cold north sea: slate water, kelp ochre, dark red anemones, a pale steel light.
    {"NORTH SEA", 0x0c181d, 0x3a585b, 0x8aa6a2, 0xe8eef0, 0x3e4447, 0xaaa28c, 0xb5503e, 0xa8843e, 0x4f5a2e, 0x7fa8c0, 0xdff2ff,
     0xeef2f2, 0xc9dfe2, 0x0b161a,
     {0x9fd0c6, 0xe0cc8a, 0xaea2d2, 0xa2cc8e, 0xdc9a90, 0x8eaed2, 0xe0aa76}, 0.f, 0.f},
    // Fever: a psychedelic pool, violet water, acid life, the hues turning as the dive goes deeper.
    {"FEVER", 0x14052a, 0x5a1a8a, 0xff7ad0, 0xfff27a, 0x3a1650, 0xff9a5a, 0xff3a6a, 0xffd23a, 0x3affb0, 0x3ad8ff, 0xff6af0,
     0xfff4ff, 0xffe23a, 0x10041f,
     {0x3af0ff, 0xffe23a, 0xd27aff, 0x6aff8a, 0xff5a9a, 0x5a8aff, 0xff9a3a}, 0.f, 1.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = TidePoolUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.deep = L(d.deep); p.water = L(d.water); p.shallow = L(d.shallow); p.light = L(d.light); p.rock = L(d.rock);
    p.sand = L(d.sand); p.warm = L(d.warm); p.warm2 = L(d.warm2); p.weed = L(d.weed); p.cool = L(d.cool); p.glow = L(d.glow);
    p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.lum = d.lum; p.fever = d.fever;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

// The dive.
constexpr double STAGE_OCT = 4.0;                 // octaves per stage (16 times smaller each)
constexpr int STAGES = 9;
constexpr double LOOP = STAGE_OCT * STAGES;       // a loop of the nine stages: back to a rock pool
constexpr double STEP = STAGE_OCT / 6.0;          // a line: two thirds of an octave (six lines a stage)
constexpr double TETRIS_EXTRA = STAGE_OCT / 3.0;  // a Tetris plunges a whole stage (4 lines + this)
constexpr float OMEGA = 0.07f;            // the zoom's turn (radians an octave)
constexpr float CLEAR_T = 0.42f;          // a cleared row melts in this long (real time)
constexpr int MAX_MOTES = 900;

// What each stage is and how big (meters, a half screen at its start).
const char* STAGE_NAME[STAGES] = {"a rock pool", "a boulder", "a drop of water", "a diatom", "a living cell",
                                  "molecules", "an atom", "quantum foam", "strings"};
const double STAGE_M[STAGES + 1] = {1.0, 6e-2, 4e-3, 2.5e-4, 1.5e-5, 2e-8, 3e-10, 2e-15, 1.6e-35, 1e-36};

// Test hooks: ZEN_TIDEPOOL_LINES=N adds N lines to the dive, ZEN_GROW_RATE=R adds R lines a second,
// ZEN_TIDEPOOL_DEPTH=Z holds the dive at depth Z (octaves; 4 a stage).
int extraLines(double t) {
    static const int n = std::getenv("ZEN_TIDEPOOL_LINES") ? std::atoi(std::getenv("ZEN_TIDEPOOL_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool TidePoolUniverse::init(int width, int height) {
    progWorld_ = compile(ugl::FULLSCREEN_VS, shaders::WORLD_FS, "tidepool world");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "tidepool comp");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "tidepool sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "tidepool veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "tidepool cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "tidepool shape");
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "tidepool text");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "tidepool post");
    if (const char* d = std::getenv("ZEN_TIDEPOOL_DEPTH")) debugZ_ = std::atof(d);

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
    buildFont(mono_, FONT_MONO);
    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void TidePoolUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] tidepool font init failed\n");
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

void TidePoolUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * (float)width_ / height_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the focus: in the free band right of the HUD's right column
    const float x0 = layout::RIGHT_COLUMN[2], x1 = viewC_.x + hw;
    focus_ = V(x0 + std::max(1.5f, (x1 - x0) * 0.46f), viewC_.y - 0.8f);
    // the world at a reduced resolution (at most 810 lines)
    const int wh = std::max(180, std::min(810, (int)(height_ * 0.75f)));
    const int ww = std::max(320, (int)std::lround((float)wh * width_ / height_));
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

TidePoolUniverse::~TidePoolUniverse() {
    for (ugl::Target* t : {&worldT_, &sceneT_}) t->destroy();
    for (GLuint p : {progWorld_, progComp_, progSprite_, progVeil_, progCell_, progShape_, progText_, progPost_}) glDeleteProgram(p);
    for (GLuint v : {spriteVao_, cellVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, cellVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&caps_, &ital_, &mono_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void TidePoolUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 1;                     // midnight for dark songs
        else if (t.hasSong && !t.minor && t.brightness > 0.58f) v = 0;              // morning for bright ones
        else if (t.hasSong && t.bassWeight > 0.62f && t.dynamics > 0.55f) v = 3;    // fever for heavy, dynamic ones
        else if (t.hasSong && t.airWeight < 0.4f) v = 2;                            // the north sea for dull, cold ones
        else {
            static const int BY_HUE[] = {0, 2, 1, 0, 3, 2};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x7A1DEull);
    if (seconds <= 0.f) palSet_ = false;
}

void TidePoolUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

void TidePoolUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void TidePoolUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 TidePoolUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints TidePoolUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.deep, p.water, 0.45f);
    h.light = false;
    h.ink = p.ink;
    h.accent = p.glow;
    h.styles = TS_RIPPLE | TS_INK | TS_LUMA;
    return h;
}

double TidePoolUniverse::zTarget(int lines) const { return zBase_ + lines * STEP + bonus_ + creep_ + breath_; }

// =====================================================================================================
// Game events
// =====================================================================================================

void TidePoolUniverse::syncIds(const Game& g) {
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

void TidePoolUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::HardDrop: {
        // a puff of silt pushed out from under the piece
        float yLo = 99.f;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f || (int)motes_.size() >= MAX_MOTES) continue;
            for (int i = 0; i < 2; i++) {
                Mote m;
                m.p = V(p.x + rng_.range(-0.45f, 0.45f), p.y - 0.5f);
                m.v = V(rng_.range(-0.7f, 0.7f), rng_.range(0.05f, 0.35f));
                m.life = rng_.range(0.6f, 1.0f);
                m.size = rng_.range(0.12f, 0.2f);
                m.col = lerp(pal_.sand, pal_.piece[std::clamp(c.type, 0, 6)], 0.4f);
                m.alpha = 0.35f;
                m.kind = 0;
                m.seed = rng_.uniform();
                motes_.push_back(m);
            }
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        const int lines = std::max(1, ev.count);
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
            // the glass melts into a few bubbles that rise and are gone
            const int nb = lines >= 4 ? 3 : 2;
            for (int i = 0; i < nb && (int)motes_.size() < MAX_MOTES; i++) {
                Mote b;
                b.kind = 2;
                b.p = V(p.x + rng_.range(-0.35f, 0.35f), p.y + rng_.range(-0.3f, 0.3f));
                b.v = V(rng_.range(-0.3f, 0.3f), rng_.range(1.6f, 3.2f));
                b.life = rng_.range(0.45f, 0.8f);
                b.age = -rng_.range(0.f, 0.15f);
                b.size = rng_.range(0.09f, 0.2f) * (lines >= 4 ? 1.2f : 1.f);
                b.col = lerp(pal_.piece[std::clamp(cl.type, 0, 6)], vec3(1.f), 0.5f);
                b.alpha = 0.9f;
                b.seed = rng_.uniform();
                motes_.push_back(b);
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
        // a Tetris: a plunge through a whole scale
        if (lines >= 4) {
            bonus_ += TETRIS_EXTRA;
            if (std::getenv("ZEN_GPUTIME")) std::fprintf(stderr, "[tidepool] tetris at %.1f s\n", time_);
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

void TidePoolUniverse::cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, 0.f, 0.f}});
}

void TidePoolUniverse::buildBoard(const Game& g) {
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
    // game over: the glass dulls from the top down, like sea glass left to dry
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
            if (c[1] < Game::HIDDEN - 1) continue;
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

void TidePoolUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param, float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

// =====================================================================================================
// Text
// =====================================================================================================

float TidePoolUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void TidePoolUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void TidePoolUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float halo) {
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

// The HUD: Instrument Serif capitals, widely spaced; the song and the notices in its italics; the numbers in DM Mono.
// The universe's line ends with the scale of what is behind the well (a power of ten, in meters).
void TidePoolUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
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
    const float halo = 0.7f;
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
            const float sz = base * 1.0f, tr = sz * 0.3f;
            const float wName = textWidth(caps_, name, sz, tr);
            std::string vt = var;
            for (size_t i = 1; i < vt.size(); i++) vt[i] = (char)std::tolower((unsigned char)vt[i]);
            const float vsz = base * 1.35f;
            const float wVar = vt.empty() ? 0.f : textWidth(ital_, vt, vsz, 0.f);
            const float gap = base * 0.7f;
            // the scale: "10" and its exponent raised, then the unit; after it the stage's name
            const float msz = base * 0.8f, esz = base * 0.58f;
            std::string mant = "10", expo, unit = " m";
            if (scaleText_.empty() || scaleText_ == "0") { mant = "1"; expo.clear(); }
            else expo = scaleText_;
            const float wM = textWidth(mono_, mant, msz, 0.f), wE = expo.empty() ? 0.f : textWidth(mono_, expo, esz, 0.f);
            const float wU = textWidth(mono_, unit, msz, 0.f);
            const float wSt = textWidth(ital_, stageName_, vsz, 0.f);
            const float total = wName + (vt.empty() ? 0.f : gap + wVar) + gap * 1.4f + wM + wE + wU + gap + wSt;
            const float x = ugl::hudLeft(h, total, width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(caps_, name, x, h.y, sz, tr, q);
            drawText(caps_, q, ink, a, halo);
            float cx = x + wName;
            if (!vt.empty()) {
                q.clear();
                textQuads(ital_, vt, cx + gap, h.y - sz * 0.2f, vsz, 0.f, q);
                drawText(ital_, q, accent, a * 0.9f, halo);
                cx += gap + wVar;
            }
            cx += gap * 1.4f;
            q.clear();
            textQuads(mono_, mant, cx, h.y + sz * 0.1f, msz, 0.f, q);
            if (!expo.empty()) textQuads(mono_, expo, cx + wM + 1.f * s, h.y - sz * 0.2f, esz, 0.f, q);
            textQuads(mono_, unit, cx + wM + wE, h.y + sz * 0.1f, msz, 0.f, q);
            drawText(mono_, q, ink, a * 0.75f, halo);
            cx += wM + wE + wU + gap;
            q.clear();
            textQuads(ital_, stageName_, cx, h.y - sz * 0.2f, vsz, 0.f, q);
            drawText(ital_, q, ink, a * 0.7f, halo);
            continue;
        }
        case HUD_SECTION:
            place(caps_, h.text, base * 0.95f, base * 0.5f);
            drawText(caps_, q, ink, saturate(0.2f + h.alpha * 1.9f), halo);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.75f, base * 0.01f, -base * 0.35f);
            drawText(ital_, q, ink, saturate(h.alpha * 1.3f), halo);
            continue;
        case HUD_INFO:
            place(mono_, h.text, base * 0.66f, base * 0.08f);
            drawText(mono_, q, ink, saturate(alpha * 1.3f) * 0.8f, halo);
            continue;
        case HUD_LABEL:
            place(caps_, h.text, base * 0.82f, base * 0.45f);
            drawText(caps_, q, lerp(ink, accent, 0.45f), saturate(alpha * 1.2f) * 0.92f, halo);
            continue;
        case HUD_VALUE:
            place(mono_, h.text, base * 1.0f, base * 0.02f);
            drawText(mono_, q, ink, alpha, halo);
            continue;
        case HUD_NOTICE:
            place(ital_, h.text, base * 1.6f, base * 0.01f);
            drawText(ital_, q, accent, alpha, halo);
            continue;
        case HUD_KEY:
            place(mono_, h.text, base * 0.75f, base * 0.05f);
            drawText(mono_, q, ink, alpha, halo);
            continue;
        case HUD_TOAST:
            place(caps_, h.text, base * 1.35f, base * 0.5f, -base * 0.2f);
            drawText(caps_, q, ink, alpha, 0.85f);
            continue;
        case HUD_TOAST_SUB:
            place(ital_, h.text, base * 1.4f, base * 0.01f);
            drawText(ital_, q, accent, alpha * 0.9f, 0.85f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(caps_, h.text, base * 1.2f, base * 0.25f, -base * 0.1f);
                drawText(caps_, q, ink, alpha, 0.85f);
            } else {
                place(caps_, h.text, base * 0.85f, base * 0.14f);
                drawText(caps_, q, h.accent ? accent : ink, alpha, 0.85f);
            }
            continue;
        default:
            place(caps_, h.text, base * 0.85f, base * 0.12f);
            drawText(caps_, q, h.accent ? accent : ink, alpha, halo);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void TidePoolUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    // ZEN_GPUTIME: the GPU time of a frame, printed every 60 frames
    static const bool gpuTime = std::getenv("ZEN_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
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
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && overT_ < 0.f && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    if (isDrop && !wasDrop_) {
        waveT_ = 0.f;
        waveK_ = 1.f;
        if (std::getenv("ZEN_GPUTIME")) std::fprintf(stderr, "[tidepool] drop at %.1f s\n", time_);
    }
    wasDrop_ = isDrop;
    waveT_ += wdt;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.5f : 0.4f, dt);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
    }

    // Test hook: ZEN_TIDEPOOL_TETRIS=T plays a Tetris's plunge (and its four rows melting) T seconds in.
    {
        static const float tAt = std::getenv("ZEN_TIDEPOOL_TETRIS") ? (float)std::atof(std::getenv("ZEN_TIDEPOOL_TETRIS")) : -1.f;
        if (tAt >= 0.f && !f.bare && time_ >= tAt && time_ - wdt < tAt) {
            GameEvent ev{GameEvent::Clear};
            ev.count = 4;
            for (int y = Game::H - 4; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) ev.cells.push_back({x, y, std::max(0, (int)game.cell(x, y).type)});
            onEvent(ev, game);
        }
    }
    // ---- The beat grid: the tide sloshes once a bar.
    double beat;
    if (music.hasSong) beat = music.beat;
    else {
        if (!f.paused) ownBeat_ += dt * 2.0;
        beat = ownBeat_;
    }
    const float tide = (float)std::sin(beat / 4.0 * TAU);
    const float tideAmp = 0.25f + 0.4f * wm + 0.45f * wp + 0.2f * dropAmt_;
    const float kick = std::max(music.kick, kickIn_) * wp;
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);

    // ---- The dive: the game's lines -> the depth (a new game starts again from the rock pool).
    {
        const int lines = f.bare ? lines_ : linesHere(game) + extraLines(time_);
        if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
            // a new game: start from the rock pool the last one pulled back to
            zBase_ = std::floor(z_ / LOOP + 0.02) * LOOP;
            creep_ = bonus_ = 0.0;
        }
        lines_ = lines;
        if (!f.paused && overT_ < 0.f) creep_ += wdt * (0.004 + 0.006 * wm + 0.008 * wp);
        breath_ = 0.06 * std::sin(time_ * 0.11) * (0.5 + 0.5 * wm);
        double target = zTarget(lines);
        if (overT_ >= 0.f) target = std::floor(z_ / LOOP + 0.02) * LOOP; // game over: all the way back out
        if (debugZ_ >= 0.0) target = debugZ_;
        if (!zSet_) { z_ = target; zVel_ = 0.0; zSet_ = true; }
        linesShown_ = lines;
        // a critically damped spring: each clear an eased step inward, a Tetris a plunge
        const double w = overT_ >= 0.f ? 1.1 : 1.7;
        zVel_ += (w * w * (target - z_) - 2.0 * w * zVel_) * wdt;
        const double vmax = overT_ >= 0.f ? 7.0 : 3.0;
        zVel_ = std::clamp(zVel_, -vmax, vmax);
        z_ += zVel_ * wdt;
        if (debugZ_ >= 0.0) { z_ = debugZ_; zVel_ = 0.0; }
    }
    // the scale shown in the HUD
    {
        const double zz = std::max(0.0, z_);
        const double loopN = std::floor(zz / LOOP);
        const double inLoop = zz - loopN * LOOP;
        const int st = std::min(STAGES - 1, (int)std::floor(inLoop / STAGE_OCT));
        const double u = (inLoop - st * STAGE_OCT) / STAGE_OCT;
        const double lg = std::log10(STAGE_M[st]) * (1.0 - u) + std::log10(STAGE_M[st + 1]) * u - 36.0 * loopN;
        const int e = (int)std::lround(lg);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", e);
        scaleText_ = buf;
        stageName_ = STAGE_NAME[u > 0.75 ? (st + 1) % STAGES : st];
    }

    // ---- Motes over the board.
    for (Mote& m : motes_) {
        m.age += dt;
        if (m.age < 0.f) continue;
        m.p = vec2{m.p.x + m.v.x * dt, m.p.y + m.v.y * dt};
        if (m.kind == 2) m.p.x += std::sin(m.age * 14.f + m.seed * 20.f) * 0.6f * dt;
        else m.v = vec2{m.v.x * std::exp(-2.5f * dt), m.v.y * std::exp(-2.5f * dt)};
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.age >= m.life; }), motes_.end());
    for (auto& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    sprites_.clear();
    if (!f.bare) {
        for (const Mote& m : motes_) {
            if (m.age < 0.f) continue;
            const float t = m.age / m.life;
            const float a = smoothstepf(0.f, 0.1f, t) * (1.f - smoothstepf(0.55f, 1.f, t)) * m.alpha;
            sprite(m.p, m.size * (m.kind == 2 ? 1.f + 0.4f * t : 1.f + t), m.col, a, m.kind);
        }
        // the bonus gauge: a fine ring, the charge an arc of light
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        sprite(V(gc.x, gc.y), layout::GAUGE_R * 1.25f, on ? pal_.glow : pal_.accent, 1.f, 1, fill, on ? 1.f : 0.f);
    }
    cells_.clear();
    if (!f.bare) buildBoard(game); // (a break: the world alone)

    // =========================================================== the world (reduced resolution)
    {
        // the focus wanders slowly; the layers' frames from the depth
        const vec2 F = V(focus_.x + 0.7f * std::sin(time_ * 0.031f), focus_.y + 0.5f * std::sin(time_ * 0.023f + 1.f));
        float far = 0.f;
        for (int i = 0; i < 4; i++) {
            const float cx = viewC_.x + ((i & 1) ? viewH_.x : -viewH_.x), cy = viewC_.y + ((i & 2) ? viewH_.y : -viewH_.y);
            far = std::max(far, std::sqrt((cx - F.x) * (cx - F.x) + (cy - F.y) * (cy - F.y)) / viewH_.y);
        }
        h0_ = 3.05f / far;
        const double zz = std::max(0.0, z_);
        const double a = std::floor(zz / STAGE_OCT);
        // the stage seen whole, the one around it (its soft edge), the two inside it
        float layers[16];
        const double first = a >= 1.0 ? a - 1.0 : a;
        const int count = a >= 1.0 ? 4 : 3;
        for (int i = 0; i < count; i++) {
            const double l = first + i;
            const double u = zz - l * STAGE_OCT;
            const int st = (int)std::fmod(l, (double)STAGES);
            layers[i * 4 + 0] = (float)st;
            layers[i * 4 + 1] = (float)u;
            layers[i * 4 + 2] = h0_ * (float)std::exp2(-u);
            layers[i * 4 + 3] = (float)(OMEGA * u + st * 0.9);
        }
        worldT_.bind();
        glDisable(GL_BLEND);
        glUseProgram(progWorld_);
        set2f(progWorld_, "uViewC", viewC_.x, viewC_.y);
        set2f(progWorld_, "uViewH", viewH_.x, viewH_.y);
        set2f(progWorld_, "uFocus", F.x, F.y);
        set1f(progWorld_, "uPxP", 2.f / worldT_.h);
        glUniform4fv(ugl::U(progWorld_, "uLayer"), count, layers);
        set1i(progWorld_, "uLayers", count);
        set1f(progWorld_, "uTime", time_);
        set1f(progWorld_, "uTide", tide);
        set1f(progWorld_, "uTideAmp", tideAmp);
        set1f(progWorld_, "uLife", (float)std::fmod(beat, 4096.0));
        set1f(progWorld_, "uPeak", wp + 0.5f * dropAmt_);
        set1f(progWorld_, "uKick", kick);
        set1f(progWorld_, "uSlow", slow_);
        set4f(progWorld_, "uWave", waveT_, waveK_ * (waveT_ < 9.f ? 1.f : 0.f), 0.f, 0.f);
        set3f(progWorld_, "uDeep", pal_.deep);
        set3f(progWorld_, "uWater", pal_.water);
        set3f(progWorld_, "uShallow", pal_.shallow);
        set3f(progWorld_, "uLight", pal_.light * (1.f + 0.12f * wp + 0.2f * swell_));
        set3f(progWorld_, "uRock", pal_.rock);
        set3f(progWorld_, "uSand", pal_.sand);
        set3f(progWorld_, "uWarm", pal_.warm);
        set3f(progWorld_, "uWarm2", pal_.warm2);
        set3f(progWorld_, "uWeed", pal_.weed);
        set3f(progWorld_, "uCool", pal_.cool);
        set3f(progWorld_, "uGlow", pal_.glow * (0.8f + 0.2f * wm + 0.3f * wp));
        set3f(progWorld_, "uBridgeCol", bridge_.color);
        set1f(progWorld_, "uWorldA", world);
        set1f(progWorld_, "uLum", pal_.lum);
        set1f(progWorld_, "uFever", pal_.fever);
        set1f(progWorld_, "uDepth", (float)std::fmod(zz, 1000.0));
        {
            float b[4] = {0, 0, 0, 0};
            for (int i = 0; i < NUM_BANDS; i++) b[std::min(3, i * 4 / NUM_BANDS)] += music.bandsFast[i] * (4.f / NUM_BANDS);
            set4f(progWorld_, "uBands", saturate(b[0]), saturate(b[1] * 1.3f), saturate(b[2] * 1.6f), saturate(b[3] * 2.f));
        }
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
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(lerp(pal_.light, pal_.glow, 0.5f) * 0.45f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", 0.8f);
        set1f(progVeil_, "uColA", 0.55f * world);
        set1f(progVeil_, "uRailA", 0.5f);
        set1f(progVeil_, "uPx", pxPerUnit_);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.f, layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uTime", time_);
        set3f(progCell_, "uLightC", lerp(pal_.light, vec3(1.f), 0.3f));
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    if (!sprites_.empty()) {
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        set1f(progSprite_, "uPxPerUnit", pxPerUnit_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
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
            std::fprintf(stderr, "[tidepool] gpu %.2f ms/frame (depth %.1f)\n", acc / n, z_);
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

const UniverseEntry& universe_tidepool() {
    static const UniverseEntry e{
        "TIDE POOL", "a rock pool at low tide; every cleared line zooms deeper into it, from anemones to a drop, a diatom, a cell, molecules, an atom and beyond",
        [] { return std::unique_ptr<Universe>(new TidePoolUniverse()); },
        // Fits most songs; the calm, airy ones a little better.
        [](const SongLook& l) { return 0.85f + 0.15f * (1.f - l.density) + 0.15f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
