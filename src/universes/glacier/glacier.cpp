// GLACIER: a frozen cave seen from inside, the blocks frozen cubes; cleared rows melt into water, and the water rises
// in the cave over the run.
//
// The cave (one full-screen pass at a reduced resolution): arches of translucent ice receding to a bright opening
// behind the well, front to back the near arch (the screen's sides and top, long icicles hanging across the top), a
// middle arch with smaller icicles, a snowy floor rising toward the back, a far arch and the opening, the light at the
// end of the cave. The ice is deep where it is thick and glows at its thin edges, where the light from the opening comes
// through, draped in long vertical folds, frosted on the faces turned up, glinting here and there; shafts of cold light
// fan down from the opening; a few ice chunks lie in the foreground. The HUD's columns are kept a little quieter.
//
// The water fills the bottom of the cave: a dark, still body mirroring the ice above it (rippled), the submerged floor
// seen through it just under the surface, its surface a thin bright line running across the whole screen, behind and
// through the well. Everything under it looks submerged (deeper, tinted: a pass over the finished scene), and the line
// fades behind the HUD's text, which stays legible over the water. Its level is the run's melted ice: it starts as a
// pool under the well and rises with the lines cleared (paceLines(): about a third of the screen at the scene's 50
// lines, then barely more), eased, a surge running out from the well with every clear; a new game drains it.
//
// The well is a clear glass case (a dark blue back with a fine grid, a thick rim of clear glass), the previews sit in
// frosted ice frames. The pieces are thick cubes of clear ice, one per cell, seen a little from the front: rounded
// edges frosted almost white, a band of rime on the top face, a glossy translucent body (the colour deepening toward
// the centre, cloudy refracting ice, four back facets, fracture planes, trapped bubbles, caustics) with fractures and
// bubbles of its own that go with it as rows fall, a soft highlight; under the water a deeper, saturated teal; seven
// icy tints that stay apart (aqua, frost white, pale blue, mint, deep blue, sea green, ocean blue in ICE). The falling
// piece trails faint streaks of cold light and a few frost motes; the ghost is a frosted outline. A lock frosts the
// cubes white for a moment; a hard drop breathes a little mist out at the contact. A cleared row melts in 0.8 s, with a
// stagger from the middle out: clear and glossy, rounded off, down into a thin film that fades, kept under the rows
// settling above it; clean drops fall from the films into the water and ring; the water rises a little.
//
// The music: the cave's light breathes slowly with the energy (never with the beats), the water's swell grows with the
// level, a ripple spreads every two beats at mid and peak, and at a peak the kicks shake frost loose from the icicles
// and catch glints in the walls. The drop enlightens the cave: for the whole DROP section the ice lights up from
// within (caustic veins, the sun through the ice in ICE and SUNSET, aurora curtains in AURORA, the glacier's cold heart
// in DEEP), the opening blazes, the shafts brighten, easing back over a few seconds after; the well stays dark, so the
// stack never washes out. Slow motion is a deep blue hush; game over is rime creeping down the stack.
//
// Around a song the light reaches the cave from the opening outward (the far ice first, the near arch last) and leaves
// it the other way. It leaves and enters through the bridge.
//
// Cost: the cave at up to 720 lines (a few fbm and the scallops per pixel, two Voronoi more in a drop, four samples on
// the silhouettes), the water at the full resolution, the case, instanced cubes and particles, the post: 0.7 to 1 ms
// of GPU a frame at 1080p.
#include "universes/glacier/glacier.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/glacier/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Josefin Sans Light and Regular, SIL Open Font License, see OFL-JosefinSans.txt.
const unsigned char FONT_LIGHT[] = {
#include "universes/glacier/JosefinSans-Light.ttf.inc"
};
const unsigned char FONT_REGULAR[] = {
#include "universes/glacier/JosefinSans-Regular.ttf.inc"
};

namespace shaders = glacier_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name, bool water = false) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, std::string(shaders::COMMON_GLSL) + (water ? shaders::WATER_GLSL : ""));
    std::string v(vs);
    v.insert(v.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): the thick ice, its glowing edge, the far ice, the opening; the floor; the water and its light; the
// shafts; the drop's two lights; the well's back, its grid, the glass; the HUD's ink, accent and veil; the seven
// cubes; night (a darker cave), aurora (the drop's light in curtains).
struct VariantDef {
    const char* name;
    unsigned deep, edge, far, open, floor, water, waterHi, shaft, glowA, glowB, well, grid, frame;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float night, aurora;
};
const VariantDef VARIANTS[] = {
    // Daylight through turquoise ice, a white opening; the drop is the sun through the ice.
    {"ICE", 0x0f6a80, 0x98ecf4, 0xa8e8f0, 0xf2ffff, 0xcfe6ec, 0x06121f, 0x3f8aa0, 0xd8fbff, 0xffd690, 0x7cecff,
     0x0c1a30, 0x5f90b8, 0xc8f2ff, 0xf2fbff, 0xbff4ff, 0x06202c,
     {0x5ad4e6, 0xe6f4f8, 0x9ccaf2, 0x82e4ae, 0x3f78dc, 0x22a88c, 0x2a9ccc}, 0.f, 0.f},
    // A glacier at night: sapphire ice, moonlight at the opening; the drop is the glacier's cold heart lighting up.
    {"DEEP", 0x0a1a48, 0x4f86d8, 0x24427e, 0x9cc4ff, 0x3a5288, 0x040a1a, 0x23447a, 0x9cc4ff, 0xc8ecff, 0x4aa8ff,
     0x060c20, 0x3a5a96, 0x8fb8ff, 0xe6f0ff, 0x9fd0ff, 0x030818,
     {0x52cdf0, 0xe0ecff, 0x92b8f6, 0x6edcc0, 0x3a62d8, 0x1f9c98, 0x2a86d0}, 1.f, 0.f},
    // Dark green-teal ice under the polar night; the drop pours aurora curtains through the walls.
    {"AURORA", 0x0a3434, 0x5cc8b4, 0x1c5258, 0x9ff0dc, 0x3a666c, 0x041414, 0x1f5654, 0xa8ffe6, 0x5cff9a, 0xc860ff,
     0x061618, 0x3a7a78, 0x9ff0e0, 0xeafff8, 0x9affd8, 0x031010,
     {0x56dce4, 0xe4f8f4, 0x98d0f0, 0x86eca0, 0x3c80d8, 0x22b090, 0x28a4c4}, 0.7f, 1.f},
    // Ice lit by a low sun: lilac walls, rose edges, a peach opening; the drop sets the ice ablaze in amber and rose.
    {"SUNSET", 0x2e2a5c, 0xd898c0, 0xdcaabc, 0xfff0d8, 0xe2ccd8, 0x140e26, 0x6a4c7c, 0xffe0c0, 0xffb060, 0xff78a8,
     0x120e26, 0x8a6aa4, 0xffe0ec, 0xfff6f0, 0xffd4b4, 0x140a1c,
     {0x66d2e8, 0xf4f2f6, 0xa6c8f4, 0x8ee0b8, 0x4a7ce0, 0x2aa898, 0x34a0cc}, 0.f, 0.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = GlacierUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.deep = L(d.deep); p.edge = L(d.edge); p.far = L(d.far); p.open = L(d.open); p.floor = L(d.floor);
    p.water = L(d.water); p.waterHi = L(d.waterHi); p.shaft = L(d.shaft); p.glowA = L(d.glowA); p.glowB = L(d.glowB);
    p.well = L(d.well); p.grid = L(d.grid); p.frame = L(d.frame);
    p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.night = d.night; p.aurora = d.aurora;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

// A cleared cube melts in MELT_T (real time): MELT_U of its own after a stagger from the middle out (MELT_STAGGER a
// column).
constexpr float MELT_T = 0.82f, MELT_U = 0.66f, MELT_STAGGER = 0.035f;
constexpr int MAX_MOTES = 700;
constexpr int MAX_RIPPLES = 16;
// The previews' frosted frames (board units: center, half size).
const vec4 BOX_HOLD{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y, 1.55f, 1.55f};
const vec4 BOX_NEXT{8.6f, 4.05f, 1.55f, 4.7f};

// Test hooks: REFALL_GLACIER_LINES=N adds N lines to the water, REFALL_GROW_RATE=R adds R lines a second.
float extraLines(double t) {
    static const int n = std::getenv("REFALL_GLACIER_LINES") ? std::atoi(std::getenv("REFALL_GLACIER_LINES")) : 0;
    static const double rate = std::getenv("REFALL_GROW_RATE") ? std::atof(std::getenv("REFALL_GROW_RATE")) : 0.0;
    return (float)(n + (int)(rate * t));
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool GlacierUniverse::init(int width, int height) {
    progCave_ = compile(ugl::FULLSCREEN_VS, shaders::CAVE_FS, "glacier cave");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "glacier water", true);
    progCase_ = compile(ugl::FULLSCREEN_VS, shaders::CASE_FS, "glacier case");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "glacier cube");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "glacier sprite");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "glacier shape");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "glacier post", true);

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
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));
    instanced(spriteVao_, spriteVbo_, 3, sizeof(SpriteInst));
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    pal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void GlacierUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * (float)width_ / height_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the water: a pool under the well at first, about a third of the screen at the scene's end
    const float bottom = viewC_.y - hh;
    waterBottom_ = bottom + 0.85f;
    waterTop_ = bottom + 0.34f * 2.f * hh;
    // the cave at up to 720 lines (soft ice: it upscales well), mipmapped for the frosted frames and the reflections
    const int ch = std::max(180, std::min(720, height_));
    const int cw = std::max(320, (int)std::lround((float)ch * width_ / height_));
    caveT_.create(cw, ch, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindTexture(GL_TEXTURE_2D, caveT_.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GlacierUniverse::~GlacierUniverse() {
    for (ugl::Target* t : {&caveT_, &sceneT_}) t->destroy();
    for (GLuint p : {progCave_, progComp_, progCase_, progCell_, progSprite_, progShape_, progPost_}) glDeleteProgram(p);
    for (GLuint v : {cellVao_, spriteVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, spriteVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void GlacierUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 1;                 // the night glacier for dark songs
        else if (t.hasSong && t.minor && t.airWeight > 0.55f) v = 2;            // the aurora for airy minor ones
        else if (t.hasSong && !t.minor && t.brightness > 0.6f) v = 3;           // the low sun for bright major ones
        else if (t.hasSong && t.brightness > 0.45f) v = 0;                      // daylight
        else {
            static const int BY_HUE[] = {0, 2, 1, 0, 3, 2};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x1CEC0DEull);
    if (seconds <= 0.f) palSet_ = false;
}

void GlacierUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropMode_ = false;
    (void)impact;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        lvl_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        energyS_ = energy;
        return;
    }
    const bool rising = level > lvl_;
    lvl_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.f, 6.f);
}

void GlacierUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void GlacierUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 GlacierUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints GlacierUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.deep, p.edge, 0.35f);
    h.light = false;
    h.ink = lerp(p.piece[1], p.edge, 0.3f);
    h.accent = p.edge;
    h.styles = TS_RIPPLE | TS_INK | TS_LUMA;
    return h;
}

// =====================================================================================================
// The water
// =====================================================================================================

float GlacierUniverse::levelFor(float lines) const {
    const float k = std::pow(std::clamp(lines, 0.f, SCENE_REF_LINES) / SCENE_REF_LINES, 0.85f);
    const float extra = lines > SCENE_REF_LINES ? 0.12f * (1.f - std::exp(-(lines - SCENE_REF_LINES) / 40.f)) : 0.f;
    return waterBottom_ + (waterTop_ - waterBottom_) * (k + extra);
}

void GlacierUniverse::ripple(float x, float amp, float width) {
    if ((int)ripples_.size() >= MAX_RIPPLES) {
        // the oldest gives way
        auto it = std::max_element(ripples_.begin(), ripples_.end(), [](const Ripple& a, const Ripple& b) { return a.age < b.age; });
        *it = {x, 0.f, amp, width};
        return;
    }
    ripples_.push_back({x, 0.f, amp, width});
}

float GlacierUniverse::surface(float x) const {
    float s = level_;
    for (const Ripple& r : ripples_) {
        const float d = std::fabs(x - r.x), front = 2.2f * r.age;
        const float env = std::exp(-((d - front) / (r.width + 0.5f * r.age)) * ((d - front) / (r.width + 0.5f * r.age))) * std::exp(-1.4f * r.age);
        s += r.amp * env * std::cos((d - front) * 3.f / r.width);
    }
    return s;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void GlacierUniverse::syncIds(const Game& g) {
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const bool full = g.cell(x, y).type >= 0;
            if (full && !ids_[y][x]) ids_[y][x] = nextId_++;
            else if (!full) { ids_[y][x] = 0; flash_[y][x] = 0.f; }
        }
}

void GlacierUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock:
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) {
                ids_[c.y][c.x] = nextId_++;
                flash_[c.y][c.x] = 1.f;
            }
        break;
    case GameEvent::HardDrop: {
        // a breath of mist pushed out at the contact, under the piece
        float yLo = 99.f, xLo = 99.f, xHi = -99.f;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f) continue;
            xLo = std::min(xLo, p.x);
            xHi = std::max(xHi, p.x);
        }
        if (xLo > xHi) break;
        for (int i = 0; i < 6 && (int)motes_.size() < MAX_MOTES; i++) {
            Mote m;
            const float side = i % 2 ? 1.f : -1.f;
            m.p = V((side > 0 ? xHi + 0.4f : xLo - 0.4f) + rng_.range(-0.2f, 0.2f), yLo - 0.42f + rng_.range(0.f, 0.2f));
            m.v = V(side * rng_.range(0.8f, 1.6f), rng_.range(0.1f, 0.4f));
            m.life = rng_.range(0.7f, 1.1f);
            m.size = rng_.range(0.35f, 0.55f);
            m.col = lerp(pal_.edge, pal_.open, 0.6f);
            m.alpha = 0.12f;
            m.kind = 4;
            m.seed = rng_.uniform();
            motes_.push_back(m);
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        const int lines = std::max(1, ev.count);
        int n = 0;
        for (auto& cl : ev.cells) {
            if (cl.y < Game::HIDDEN || cl.y >= Game::H || cl.x < 0 || cl.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)cl.x, (float)cl.y);
            const int id = ids_[cl.y][cl.x] ? ids_[cl.y][cl.x] : nextId_++;
            int low = cl.y;
            while (low + 1 < Game::H && rows[low + 1]) low++;
            const float delay = MELT_STAGGER * std::fabs(cl.x - 4.5f);
            melting_.push_back({p.x, p.y, std::clamp(cl.type, 0, 6), (float)(id % 997) * 0.731f, 0.f, delay, cl.y, low});
            // one cube in two of the clear's lowest row lets a clean drop fall from its film into the water
            const int nd = (cl.x + cl.y) % 2 == 0 && cl.y == low ? 1 : 0;
            for (int i = 0; i < nd && (int)motes_.size() < MAX_MOTES; i++) {
                Mote d;
                d.kind = 1;
                d.p = V(p.x, p.y - 0.47f);
                d.v = V(0.f, -0.4f);
                d.age = -(delay + MELT_U * 0.62f);
                d.life = 3.f;
                d.size = 0.085f;
                d.col = lerp(pal_.piece[std::clamp(cl.type, 0, 6)], pal_.open, 0.45f);
                d.alpha = 0.95f;
                d.seed = 0.f; // rings the water where it falls
                n++;
                motes_.push_back(d);
            }
        }
        {
            int nb[Game::H][Game::W] = {};
            float nf[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(nf[dst], flash_[y], sizeof(flash_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(flash_, nf, sizeof(flash_));
        }
        // the melt reaches the water: a surge running out from the well
        ripple(0.f, 0.05f + 0.025f * lines, 1.4f);
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        overCells_.clear();
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            overCells_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), (float)(ids_[c.y][c.x] % 997) * 0.731f, (float)c.y});
        }
        break;
    default: break;
    }
}

// =====================================================================================================
// Cubes
// =====================================================================================================

void GlacierUniverse::cube(float x, float y, float half, float seed, const vec3& tint, float alpha, int mode, float erase,
                           float flash, float frost) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, seed}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, flash, frost}});
}

void GlacierUniverse::buildBoard(const Game& g) {
    auto tintOf = [&](int t) { return pal_.piece[std::clamp(t, 0, 6)]; };
    // the melting cubes first (the rows falling into their room cover their films), each kept under the rows above
    for (const Melting& m : melting_) {
        const float u = saturate((m.t - m.delay) / MELT_U);
        const float room = std::clamp(m.row - m.low + g.rowOffset(m.low), 0.f, 1.f);
        cube(m.x, m.y, 0.5f, m.seed, tintOf(m.type), 1.f, 2, u, room, 0.f);
    }
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            cube(p.x, p.y, 0.5f, (float)(ids_[y][x] % 997) * 0.731f, tintOf(c.type), 1.f, 0, 0.f, flash_[y][x], 0.f);
        }
    // game over: rime creeps down the stack from the top
    if (overT_ >= 0.f)
        for (const OverCell& o : overCells_) {
            const float rowFrac = (o.row - Game::HIDDEN) / (float)(Game::H - Game::HIDDEN);
            cube(o.x, o.y, 0.5f, o.seed, tintOf(o.type), 1.f, 0, 0.f, 0.f, smoothstepf(rowFrac * 0.6f, rowFrac * 0.6f + 0.35f, over_));
        }
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
                cube(w.x, w.y, 0.5f, 0.f, tintOf(p.type), 0.85f, 1, 0.f, 0.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            if (cells[i][1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
            cube(w.x, w.y, 0.5f, 400.f + p.type * 7.f + i * 1.9f, tintOf(p.type), 1.f, 0, 0.f, 0.f, 0.f);
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
        for (int i = 0; i < 4; i++) {
            const float px = center.x + (cells[i][0] - (minx + maxx) * 0.5f) * sc, py = center.y - (cells[i][1] - (miny + maxy) * 0.5f) * sc;
            cube(px, py, 0.5f * sc, 600.f + type * 7.f + i * 1.9f, tintOf(type), alpha, 0, 0.f, 0.f, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void GlacierUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float stretch, float seed) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {stretch, seed, 0.f, 0.f}});
}

// =====================================================================================================
// Text
// =====================================================================================================

void GlacierUniverse::drawPanels(const std::vector<HudRect>& panels) {
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

// The HUD in Josefin Sans: light, widely spaced capitals in an icy white, the numbers in its regular weight, the
// variant's name in the ice's pale accent; a soft halo of the cave's shadow under every line, so it reads over the
// bright ice as over the dark water.
void GlacierUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* light = text::font(FONT_LIGHT);
    text::Font* regular = text::font(FONT_REGULAR);
    const float s = height_ / 1080.f, base = h.scale * 12.f, alpha = saturate(h.alpha);
    const float br = smoothstepf(0.3f, 0.95f, bridge_.amount);
    const vec3 ink = ugl::toSrgb(lerp(pal_.ink, bridge_.ink, br)), accent = ugl::toSrgb(lerp(pal_.accent, bridge_.ink, br));
    float halo = 0.6f;
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        const float sz = base * 0.8f, vsz = base * 0.66f, a = 0.35f + h.alpha * 1.6f;
        put(light, text::upper(name), sz, sz * 0.2f, ink, a);
        if (!var.empty()) {
            text::Span& v = out.add(var, regular, vsz, accent, 0.12f, saturate(a));
            v.gap = (base * 0.6f - sz * 0.2f) / vsz;
            v.dy = (sz - vsz) * 0.5f + 0.723f * vsz - 0.71f * sz;
        }
        halo = 0.7f;
        break;
    }
    case HUD_SECTION: put(light, text::upper(h.text), base * 1.05f, base * 0.42f, ink, 0.2f + h.alpha * 1.9f); halo = 0.7f; break;
    case HUD_TITLE: put(regular, h.text, base * 1.05f, base * 0.04f, ink, h.alpha * 1.3f, -base * 0.2f); halo = 0.7f; break;
    case HUD_INFO: put(regular, text::upper(h.text), base * 0.66f, base * 0.25f, ink, saturate(alpha * 1.2f) * 0.8f); break;
    case HUD_LABEL: put(regular, text::upper(h.text), base * 0.7f, base * 0.4f, ink, saturate(alpha * 1.2f) * 0.8f); break;
    case HUD_VALUE: put(regular, h.text, base * 1.15f, base * 0.06f, ink, alpha); break;
    case HUD_NOTICE: put(light, text::upper(h.text), base * 1.0f, base * 0.25f, accent, alpha); break;
    case HUD_KEY: put(regular, h.text, base * 0.85f, base * 0.1f, ink, alpha); break;
    case HUD_TOAST: put(light, text::upper(h.text), base * 1.3f, base * 0.5f, ink, alpha, -base * 0.1f); halo = 0.8f; break;
    case HUD_TOAST_SUB: put(regular, h.text, base * 0.85f, base * 0.25f, accent, alpha * 0.95f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(light, text::upper(h.text), base * 1.15f, base * 0.4f, ink, alpha, -base * 0.08f);
        else put(regular, h.text, base * 0.85f, base * 0.12f, h.accent ? accent : ink, alpha);
        halo = 0.75f;
        break;
    default: put(regular, h.text, base * 0.82f, base * 0.1f, h.accent ? accent : ink, alpha); break;
    }
    text::Look& k = out.look;
    k.weight = 0.012f + 0.25f / std::max(1.f, out.spans[0].size);
    k.halo = ugl::toSrgb(lerp(pal_.veil, bridge_.color, br));
    k.haloA = halo * (1.f - br);
    k.haloW = 0.12f;
    k.haloPow = 2.f;
    out.alpha = (1.f - dimT_ * 0.5f) * fade_;
}

// The unified HUD's inks: the icy white over the dark water and the deep ice, the cave's deep teal over the bright
// ice, the pale accent.
HudInk GlacierUniverse::hudInk(const HudText& h) {
    (void)h;
    const float br = smoothstepf(0.3f, 0.95f, bridge_.amount);
    HudInk k;
    k.pale = ugl::toSrgb(lerp(pal_.ink, bridge_.ink, br));
    k.deep = k.pale; // (white over the bright ice too: the host's veil keeps it legible)
    k.accent = ugl::toSrgb(lerp(pal_.accent, bridge_.ink, br));
    return k;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void GlacierUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    if (!f.bare) syncIds(game); // (a break's blank board keeps the stack's ids)
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
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 0.9f : 0.35f, dt);
    swell_ = approach(swell_, 0.f, 0.4f, dt);
    energyS_ = approach(energyS_, music.hasSong ? music.energy : energy_, 0.6f, dt);
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
    // the kicks (at a peak): frost shaken loose from the icicles, glints in the walls
    const float kickNow = std::max(music.kick, kickIn_) * wp;
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    const bool kickHit = kickNow > 0.45f && kickPrev_ <= 0.45f;
    kickPrev_ = kickNow;
    glint_ = approach(glint_, kickNow, kickNow > glint_ ? 8.f : 1.5f, dt);

    // ---- The water's level: the lines melted (on the 50-line scale; a break: as it was).
    {
        const float lines = f.bare ? std::max(lines_, extraLines(time_) * paceScale()) : paceLines(game, extraLines(time_));
        lines_ = lines;
        float target = levelFor(lines);
        // (the song's teardown keeps the water it melted: a new game drains it only once the next song plays)
        if (f.teardown > 0.f) target = std::max(target, level_);
        if (!levelSet_) { level_ = target; levelVel_ = 0.f; levelSet_ = true; }
        // eased: a clear is a soft rise, a quad a longer one; a new game drains it slowly
        const float vmax = target > level_ ? 0.35f * std::clamp(paceScale(), 1.f, 2.f) : 0.8f;
        const float want = std::clamp((target - level_) * 1.2f, -vmax, vmax);
        levelVel_ += (want - levelVel_) * (1.f - std::exp(-3.f * dt));
        level_ += levelVel_ * dt;
    }
    // ripples: age, a ring every two beats at mid and peak
    for (Ripple& r : ripples_) r.age += wdt;
    ripples_.erase(std::remove_if(ripples_.begin(), ripples_.end(), [](const Ripple& r) { return r.age > 4.5f; }), ripples_.end());
    {
        const double b2 = std::floor(beat / 2.0);
        if (b2 != lastRippleBeat_ && lastRippleBeat_ >= 0.0 && wm > 0.3f && !f.paused) {
            float x = rng_.range(6.5f, viewH_.x - 1.f) * (rng_.uniform() < 0.5f ? -1.f : 1.f);
            ripple(x, 0.015f + 0.02f * wp + 0.01f * dropAmt_, 0.5f);
        }
        lastRippleBeat_ = b2;
    }

    // ---- Particles.
    const bool live = !f.bare && !f.paused;
    if (live && kickHit) {
        for (int i = 0; i < 8 && (int)motes_.size() < MAX_MOTES; i++) {
            Mote m;
            m.kind = 3;
            float x = rng_.range(-viewH_.x + 1.f, viewH_.x - 1.f);
            m.p = V(x, viewC_.y + viewH_.y - rng_.range(0.3f, 2.5f));
            m.v = V(rng_.range(-0.2f, 0.2f), rng_.range(-1.2f, -0.6f));
            m.life = rng_.range(3.f, 5.f);
            m.size = rng_.range(0.05f, 0.1f);
            m.col = lerp(pal_.open, vec3(1.f), 0.3f);
            m.alpha = 0.7f;
            m.seed = rng_.uniform();
            motes_.push_back(m);
        }
    }
    if (!f.paused) {
        // a slow fall of frost through the cave, a little more with the energy
        ambientAcc_ += wdt * (1.f + 3.f * energyS_ + 2.f * dropAmt_);
        while (ambientAcc_ >= 1.f) {
            ambientAcc_ -= 1.f;
            if ((int)motes_.size() >= MAX_MOTES) continue;
            Mote m;
            m.kind = 3;
            m.p = V(rng_.range(-viewH_.x, viewH_.x), viewC_.y + viewH_.y + 0.3f);
            m.v = V(rng_.range(-0.15f, 0.15f), rng_.range(-0.7f, -0.35f));
            m.life = rng_.range(8.f, 14.f);
            m.size = rng_.range(0.04f, 0.08f);
            m.col = lerp(pal_.open, vec3(1.f), 0.2f);
            m.alpha = 0.45f;
            m.seed = rng_.uniform();
            motes_.push_back(m);
        }
    }
    if (live && game.hasPiece() && !game.over()) {
        // the falling piece sheds a few frost motes
        pieceFrostAcc_ += dt * 7.f;
        int cells[4][2];
        game.pieceCells(game.piece(), cells);
        const float dx = game.pieceVisualX() - game.piece().x, dy = game.pieceVisualY() - game.piece().y;
        while (pieceFrostAcc_ >= 1.f) {
            pieceFrostAcc_ -= 1.f;
            if ((int)motes_.size() >= MAX_MOTES) continue;
            const int i = (int)(rng_.uniform() * 4) & 3;
            if (cells[i][1] < Game::HIDDEN) continue;
            const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
            Mote m;
            m.kind = 0;
            m.p = V(w.x + rng_.range(-0.45f, 0.45f), w.y + rng_.range(-0.5f, 0.2f));
            m.v = V(rng_.range(-0.25f, 0.25f), rng_.range(-0.6f, -0.1f));
            m.life = rng_.range(0.8f, 1.4f);
            m.size = rng_.range(0.06f, 0.1f);
            m.col = lerp(pal_.piece[std::clamp((int)game.piece().type, 0, 6)], vec3(1.f), 0.6f);
            m.alpha = 0.8f;
            m.seed = rng_.uniform();
            motes_.push_back(m);
        }
    }
    for (Mote& m : motes_) {
        const float mdt = m.kind == 1 ? dt : wdt; // (drips at the real speed, like the melt)
        m.age += mdt;
        if (m.age < 0.f) continue;
        if (m.kind == 1) {
            m.v.y -= 16.f * mdt;
            m.p = vec2{m.p.x + m.v.x * mdt, m.p.y + m.v.y * mdt};
            if (m.p.y < surface(m.p.x)) {
                if (m.seed < 0.5f) ripple(m.p.x, 0.016f, 0.28f);
                m.age = m.life; // gone into the water
            }
            continue;
        }
        m.p = vec2{m.p.x + m.v.x * mdt, m.p.y + m.v.y * mdt};
        if (m.kind == 3) m.p.x += std::sin(m.age * 1.3f + m.seed * 20.f) * 0.25f * mdt;
        if (m.kind == 4) m.v = vec2{m.v.x * std::exp(-2.2f * mdt), m.v.y * std::exp(-1.f * mdt)};
        // frost settles on the water: gone
        if (m.kind == 3 && m.p.y < level_) m.age = m.life;
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.age >= m.life; }), motes_.end());
    for (auto& md : melting_) md.t += dt;
    melting_.erase(std::remove_if(melting_.begin(), melting_.end(), [](const Melting& x) { return x.t > MELT_T; }), melting_.end());
    for (auto& row : flash_)
        for (float& v : row) v = approach(v, 0.f, 5.f, dt);

    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    sprites_.clear();
    for (const Mote& m : motes_) {
        if (m.age < 0.f) continue;
        if (f.bare && m.kind != 3) continue;
        const float t = m.age / m.life;
        float a = m.alpha;
        if (m.kind == 1) a *= 1.f;
        else a *= smoothstepf(0.f, 0.12f, t) * (1.f - smoothstepf(0.6f, 1.f, t));
        if (m.kind == 3) a *= world;
        sprite(m.p, m.kind == 4 ? m.size * (1.f + 1.2f * t) : m.size, m.col, a, m.kind, m.kind == 1 ? 1.4f : 1.f, m.seed);
    }
    // streaks of cold light trailing above the falling piece (one per column, from its top cell)
    if (!f.bare && game.hasPiece() && !game.over()) {
        int cells[4][2];
        game.pieceCells(game.piece(), cells);
        const float dx = game.pieceVisualX() - game.piece().x, dy = game.pieceVisualY() - game.piece().y;
        for (int i = 0; i < 4; i++) {
            bool top = true;
            for (int j = 0; j < 4; j++)
                if (j != i && cells[j][0] == cells[i][0] && cells[j][1] < cells[i][1]) top = false;
            if (!top || cells[i][1] < Game::HIDDEN) continue;
            const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
            sprite(V(w.x, w.y + 0.45f), 0.3f, lerp(pal_.piece[std::clamp((int)game.piece().type, 0, 6)], pal_.open, 0.35f),
                   0.34f * (1.f - bridge), 2, 5.f);
        }
    }
    cells_.clear();
    if (!f.bare) buildBoard(game);

    // the light reaching the cave around a song: from the opening outward, and back
    const float su = f.setup, td = f.teardown;
    const float rOpen = smoothstepf(0.f, 0.4f, su) * (1.f - smoothstepf(0.7f, 1.f, td));
    const float rFar = smoothstepf(0.12f, 0.55f, su) * (1.f - smoothstepf(0.5f, 0.9f, td));
    const float rMid = smoothstepf(0.28f, 0.7f, su) * (1.f - smoothstepf(0.3f, 0.75f, td));
    const float rNear = smoothstepf(0.42f, 0.85f, su) * (1.f - smoothstepf(0.1f, 0.6f, td));
    const float breath = (0.72f + 0.16f * energyS_ + 0.1f * wm + 0.06f * wp + 0.06f * swell_ + 0.3f * dropAmt_) * lerpf(1.f, 0.7f, pal_.night * 0.3f);

    // =========================================================== the cave (reduced resolution)
    {
        caveT_.bind();
        glDisable(GL_BLEND);
        glUseProgram(progCave_);
        set2f(progCave_, "uViewC", viewC_.x, viewC_.y);
        set2f(progCave_, "uViewH", viewH_.x, viewH_.y);
        set1f(progCave_, "uPx", 2.f * viewH_.y / caveT_.h);
        set1f(progCave_, "uTime", time_);
        set1f(progCave_, "uBreath", breath);
        set1f(progCave_, "uDrop", dropAmt_ * (1.f - over_));
        set1f(progCave_, "uShaft", (0.16f + 0.12f * wm + 0.12f * wp + 0.6f * dropAmt_ + 0.15f * swell_) * (1.f - 0.5f * pal_.night));
        set1f(progCave_, "uSlow", slow_);
        set1f(progCave_, "uAurora", pal_.aurora);
        set1f(progCave_, "uNight", pal_.night);
        set1f(progCave_, "uGlint", glint_);
        set1i(progCave_, "uSamples", 4);
        set1f(progCave_, "uWorldA", world);
        set4f(progCave_, "uReveal", rOpen, rFar, rMid, rNear);
        set3f(progCave_, "uDeep", pal_.deep);
        set3f(progCave_, "uEdge", pal_.edge);
        set3f(progCave_, "uFar", pal_.far);
        set3f(progCave_, "uOpen", pal_.open);
        set3f(progCave_, "uFloor", pal_.floor);
        set3f(progCave_, "uShaftC", lerp(pal_.shaft, pal_.glowA, 0.45f * dropAmt_));
        set3f(progCave_, "uGlowA", pal_.glowA);
        set3f(progCave_, "uGlowB", pal_.glowB);
        set3f(progCave_, "uBridgeCol", bridge_.color);
        set4f(progCave_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
        set4f(progCave_, "uColR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
        glBindTexture(GL_TEXTURE_2D, caveT_.tex);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    // the water's uniforms (the same surface in the water's body and in the post)
    float rip[MAX_RIPPLES * 4] = {};
    for (int i = 0; i < (int)ripples_.size() && i < MAX_RIPPLES; i++) {
        rip[i * 4 + 0] = ripples_[i].x;
        rip[i * 4 + 1] = ripples_[i].age;
        rip[i * 4 + 2] = ripples_[i].amp;
        rip[i * 4 + 3] = ripples_[i].width;
    }
    const float waveA = (0.035f + 0.025f * wm + 0.02f * wp) * (1.f - over_ * 0.8f);
    auto waterUniforms = [&](GLuint prog) {
        set1f(prog, "uLevel", level_);
        set1f(prog, "uWaveA", waveA);
        set1f(prog, "uWaveT", time_);
        glUniform4fv(ugl::U(prog, "uRip[0]"), MAX_RIPPLES, rip);
    };
    const float waterA = world;

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, caveT_.tex);
    set1i(progComp_, "uCave", 0);
    set2f(progComp_, "uViewC", viewC_.x, viewC_.y);
    set2f(progComp_, "uViewH", viewH_.x, viewH_.y);
    set3f(progComp_, "uWater", pal_.water);
    set3f(progComp_, "uWaterHi", pal_.waterHi);
    set3f(progComp_, "uOpen", pal_.open);
    set1f(progComp_, "uTime", time_);
    set1f(progComp_, "uWaterA", waterA);
    set3f(progComp_, "uDeep", pal_.deep);
    set3f(progComp_, "uEdge", pal_.edge);
    set2f(progComp_, "uRes", (float)width_, (float)height_);
    set1f(progComp_, "uLight", breath * lerpf(0.04f, 1.f, rNear));
    {
        // (clear of the HUD's corner lines and of the well)
        const float fl[5 * 4] = {-14.2f, 0.f, 1.1f, 0.08f, -11.9f, 0.f, 0.7f, -0.12f, 12.8f, 0.f, 1.3f, -0.05f,
                                 16.3f, 0.f, 0.85f, 0.15f, 19.4f, 0.f, 1.6f, 0.04f};
        glUniform4fv(ugl::U(progComp_, "uFloat[0]"), 5, fl);
    }
    waterUniforms(progComp_);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progCase_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, caveT_.tex);
        set1i(progCase_, "uCave", 0);
        set2f(progCase_, "uViewC", viewC_.x, viewC_.y);
        set2f(progCase_, "uViewH", viewH_.x, viewH_.y);
        set1f(progCase_, "uPx", 1.f / pxPerUnit_);
        set1f(progCase_, "uA", 1.f);
        set1f(progCase_, "uFrameA", world);
        set1f(progCase_, "uTime", time_);
        set3f(progCase_, "uWell", lerp(pal_.well, bridge_.color, bridge));
        set3f(progCase_, "uGrid", lerp(pal_.grid, bridge_.ink * 0.5f, bridge));
        set3f(progCase_, "uFrame", lerp(pal_.frame, bridge_.ink, bridge));
        set3f(progCase_, "uEdge", pal_.edge);
        set4f(progCase_, "uBoxH", BOX_HOLD.x, BOX_HOLD.y, BOX_HOLD.z, BOX_HOLD.w);
        set4f(progCase_, "uBoxN", BOX_NEXT.x, BOX_NEXT.y, BOX_NEXT.z, BOX_NEXT.w);
        ugl::drawFullscreen();
    }
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uTime", time_);
        set1f(progCell_, "uDrop", dropAmt_);
        set3f(progCell_, "uGlow", lerp(pal_.glowB, pal_.glowA, 0.5f));
        set1f(progCell_, "uLevel", level_);
        set1f(progCell_, "uWaterA", world);
        {
            const vec3 hi = pal_.waterHi;
            const float mx = std::max(1e-3f, std::max(hi.x, std::max(hi.y, hi.z)));
            set3f(progCell_, "uDeepTint", lerp(vec3(1.f), hi * (1.f / mx), 0.45f) * 0.95f);
        }
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
        set1f(progSprite_, "uTime", time_);
        set1f(progSprite_, "uPxPerUnit", pxPerUnit_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    drawPanels(f.panels);
    fade_ = saturate(f.fade);
    glDisable(GL_BLEND);

    // ---- Post: the water in front, the tone, the fade.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progPost_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progPost_, "uScene", 0);
    set2f(progPost_, "uViewC", viewC_.x, viewC_.y);
    set2f(progPost_, "uViewH", viewH_.x, viewH_.y);
    set2f(progPost_, "uRes", (float)width_, (float)height_);
    set1f(progPost_, "uFade", saturate(f.fade));
    set1f(progPost_, "uDim", dimT_);
    set1f(progPost_, "uWaterA", waterA);
    {
        const vec3 hi = pal_.waterHi;
        const float mx = std::max(1e-3f, std::max(hi.x, std::max(hi.y, hi.z)));
        set3f(progPost_, "uTint", lerp(vec3(1.f), hi * (1.f / mx), 0.5f) * 0.8f);
        set3f(progPost_, "uLine", lerp(pal_.edge, pal_.open, 0.5f) * 0.85f);
    }
    {
        float boxes[24 * 4] = {};
        int n = 0;
        if (f.hudBoxes)
            for (size_t i = 0; i < f.hudBoxes->size() && n < 24; i++) {
                const vec4& b = (*f.hudBoxes)[i];
                if (i < f.hud.size() && f.hud[i].alpha <= 0.01f) continue;
                boxes[n * 4 + 0] = b.x; boxes[n * 4 + 1] = b.y; boxes[n * 4 + 2] = b.z; boxes[n * 4 + 3] = b.w;
                n++;
            }
        glUniform4fv(ugl::U(progPost_, "uBox[0]"), 24, boxes);
        set1i(progPost_, "uBoxN", n);
    }
    waterUniforms(progPost_);
    ugl::drawFullscreen();
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_glacier() {
    static const UniverseEntry e{
        "GLACIER", "a frozen cave seen from inside, the blocks ice cubes; cleared rows melt into the water that rises in the cave over the run, the drop lights the ice from within",
        [] { return std::unique_ptr<Universe>(new GlacierUniverse()); },
        // Fits most songs; the cold, airy, calmer ones a little better.
        [](const SongLook& l) { return 0.85f + 0.15f * l.airWeight + 0.1f * (1.f - l.density) + (l.minor ? 0.05f : 0.f); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
