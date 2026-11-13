// CELESTIAL OCEAN: a sea of clouds at night, where every cleared row rises as a creature of light.
//
// One world, seen from just above the cloud layer: soft rolling cumulus tops to the horizon, lit by a big moon (a
// silver lining on the tops facing it, a broad column of moonlight on the sea of clouds under it, blue shade in the
// valleys, the far rows fading into haze), a deep starry sky with the Milky Way and thin wisps drifting across the
// moon. The rows of clouds drift sideways, the near ones faster.
//
// The pieces are moonstone: each piece one translucent glowing shape (its cells merge), lit toward its edges, a
// bright inner rim, a slow sheen of moonlight drifting across the stack. The well is a calm veil of deeper night
// edged with a hairline of moonlight; the HUD's columns sit on softer veils, so the stack and the lettering stay
// readable whatever swims behind them.
//
// **Every cleared row becomes a creature of light**: its cells dissolve into light and flow out of the well as
// motes to the cloud tops on one side, and a creature rises out of the clouds there (breaking through a misty edge,
// lighting the tops under it) and swims away through the sky, translucent and glowing (a bright membrane, a soft
// halo, its own lights, a trail of motes): a single row brings a school of small fish, two rows a pair of jellyfish,
// three a manta ray, a quad a whale (with a school of fish around it). They swim on, bobbing gently, and fade
// into the night or leave the screen. When the sky stays empty a while, a small creature drifts by in the distance.
//
// The song: calm drifts the clouds slowly under steady stars, a few motes rising from the clouds; mid drifts them
// faster, twinkles the stars and sends up more motes; a peak brightens the creatures, the haze and the moonlit
// tops, the creatures' light swelling softly on the beat and the stars twinkling on it. **The drop brings a giant
// whale**: it rises at one edge of the sky and passes slowly across the whole of it, huge and glowing, its tail
// beating once a bar, the cloud sea lit beneath it and motes streaming off it; when the drop ends it dives back into
// the clouds (only on a real drop: a chorus rising into a peak brings no whale). Slow motion slows everything and
// brightens the moon; game over sends every creature diving into the clouds and drains the stack's light from the
// top. It leaves and enters through the bridge.
//
// Cost: the sky and the clouds (14 rows of domes, each a few square roots, front to back with an early exit) are
// drawn at a reduced resolution (at most 540 lines); the stars, the creatures (one quad each, a signed distance),
// the motes, the veil and the pieces at full resolution; no blur pass.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane (like STAR CHART).
#include "universes/skyocean/skyocean.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/skyocean/shaders.hpp"

namespace {

// Josefin Sans Light (the HUD's airy capitals) and Cormorant Garamond Italic (the song, the universe's name, the
// notices), SIL Open Font License, see OFL-JosefinSans.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_SANS[] = {
#include "universes/skyocean/JosefinSans-Light.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/skyocean/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = skyocean_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(vs, f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 sub(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
float len(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
vec2 lerp2(vec2 a, vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
float hashf(float n) { float x = std::sin(n * 127.1f + 0.37f) * 43758.5453f; return x - std::floor(x); }

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A night (sRGB): the sky (top, middle, horizon, haze), the moon and its glow, the clouds (lit, middle, shade), the
// creatures' light (main, second, core), the aurora (low, high), the HUD's ink and accent, the seven pieces (I O T S
// Z J L); how much aurora; the moon's place (x as a fraction of the half width, y of the sky's height) and radius.
struct NightDef {
    const char* name;
    unsigned skyTop, skyMid, horizon, haze, moon, moonGlow, cloudLit, cloudMid, cloudShadow;
    unsigned life[3];
    unsigned aur1, aur2, ink, accent;
    unsigned piece[7];
    float aurora, moonX, moonY, moonR;
};
const NightDef VARIANTS[] = {
    // Moonlit blue: a cold white moon, silver tops, cyan and lilac creatures.
    {"MOONLIGHT", 0x03071a, 0x0a1a3e, 0x2c4a7c, 0x5c7cb4, 0xfff4dc, 0x8eaaf0, 0xd6e2ff, 0x3c5488, 0x0e1834,
     {0x7fe6ff, 0xb3a0ff, 0xeaffff}, 0x000000, 0x000000, 0xe6eeff, 0x9fe8ff,
     {0x6fd8f6, 0xf6d98a, 0xb79cf4, 0x84e6b0, 0xf48ea6, 0x7f9cf4, 0xf6ac7c}, 0.f, -0.7f, 0.6f, 1.5f},
    // Before dawn: a rose and peach horizon, a big golden moon low in the sky, gold and pink creatures.
    {"DAWN", 0x0c0a26, 0x2c1c4e, 0xb87478, 0xe0a090, 0xfff0d0, 0xffbe98, 0xffe0c4, 0x6c4a74, 0x1e1232,
     {0xffd07a, 0xff98c0, 0xfff6e6}, 0x000000, 0x000000, 0xfff0e4, 0xffc890,
     {0x7cd6e8, 0xf8d27a, 0xc8a0f0, 0x98deaa, 0xf88aa0, 0x8aa0f0, 0xf8a070}, 0.f, 0.66f, 0.3f, 2.1f},
    // Aurora: green and violet curtains over a teal night, sea-green creatures.
    {"AURORA", 0x01060c, 0x05161e, 0x123844, 0x37767a, 0xeefff6, 0x8fe0cc, 0xc8eee4, 0x2c5660, 0x08161c,
     {0x78ffc4, 0x88d4ff, 0xeafff6}, 0x34f090, 0x7a50f0, 0xe2fff4, 0x8cffd0,
     {0x6ce0e8, 0xf0e08a, 0xb49cf4, 0x7cf0a8, 0xf48aa0, 0x80a8f4, 0xf6b07a}, 1.f, 0.72f, 0.72f, 1.2f},
    // Deep violet: an amethyst night, pink and periwinkle creatures.
    {"AMETHYST", 0x06031a, 0x1a0c3a, 0x4a2c72, 0x8664bc, 0xfbecff, 0xc89cff, 0xeadcff, 0x4c3478, 0x140a2c,
     {0xff9cec, 0x9cb4ff, 0xfff0fc}, 0x000000, 0x000000, 0xf6eaff, 0xffa8f0,
     {0x7cd8f0, 0xf8d888, 0xd0a0f8, 0x8ce8b8, 0xf890b8, 0x8ca4f8, 0xf8aa80}, 0.f, -0.68f, 0.64f, 1.7f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = SkyOceanUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const NightDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.skyTop = L(d.skyTop); p.skyMid = L(d.skyMid); p.horizon = L(d.horizon); p.haze = L(d.haze);
    p.moon = L(d.moon); p.moonGlow = L(d.moonGlow); p.cloudLit = L(d.cloudLit); p.cloudMid = L(d.cloudMid);
    p.cloudShadow = L(d.cloudShadow);
    for (int i = 0; i < 3; i++) p.life[i] = L(d.life[i]);
    p.aur1 = L(d.aur1); p.aur2 = L(d.aur2); p.ink = L(d.ink); p.accent = L(d.accent);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.aurora = d.aurora; p.moonX = d.moonX; p.moonY = d.moonY; p.moonR = d.moonR;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

constexpr float CLEAR_T = 0.38f;     // a cleared row dissolves into light in this long (real time)
constexpr float FLOW_T = 1.15f;      // its motes flow to the cloud tops in this long
constexpr int MAX_CREATURES = 10;
constexpr int MAX_MOTES = 1600;
constexpr int MAX_LIGHTS = 10;

// The body's extent (body units, half sizes) around its center, before the halo.
vec2 bodyExtent(int kind) {
    switch (kind) {
    case SkyOceanUniverse::FISH: return V(0.56f, 0.17f);
    case SkyOceanUniverse::JELLY: return V(0.5f, 0.27f);
    case SkyOceanUniverse::MANTA: return V(0.5f, 0.56f);
    default: return V(0.6f, 0.3f);
    }
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool SkyOceanUniverse::init(int width, int height) {
    progSky_ = compile(ugl::FULLSCREEN_VS, shaders::SKY_FS, "skyocean sky");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "skyocean stars");
    progBody_ = compile(shaders::BODY_VS, shaders::BODY_FS, "skyocean body");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "skyocean sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "skyocean veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "skyocean cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "skyocean shape");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "skyocean post");

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
    instanced(bodyVao_, bodyVbo_, 4, sizeof(BodyInst));
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
    glBindVertexArray(0);

    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void SkyOceanUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    horizon_ = viewC_.y - hh * 0.1f;
    // the sky and the clouds at most 540 lines
    const int sh = std::max(90, std::min(540, height_ / 2));
    const int sw = std::max(160, (int)std::lround((float)sh * width_ / height_));
    skyT_.create(sw, sh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

SkyOceanUniverse::~SkyOceanUniverse() {
    skyT_.destroy();
    sceneT_.destroy();
    for (GLuint p : {progSky_, progComp_, progBody_, progSprite_, progVeil_, progCell_, progShape_, progPost_})
        glDeleteProgram(p);
    for (GLuint v : {bodyVao_, spriteVao_, cellVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, bodyVbo_, spriteVbo_, cellVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Creatures
// =====================================================================================================

// Where a creature is `t` seconds after it began to rise: up out of the clouds to its height while it gets under
// way, then swimming on, bobbing gently; diving back down at the end.
vec2 SkyOceanUniverse::pathAt(const Creature& c, float t) const {
    const float tau = c.kind == FISH ? 0.5f : std::max(0.3f, c.riseT * 0.55f);
    const float s = t - tau * (1.f - std::exp(-std::max(t, 0.f) / tau));
    const float r = smoothstepf(0.f, c.riseT, t);
    float x = c.start.x + c.dir * c.speed * s;
    float y = lerpf(c.start.y, c.cruiseY, r) + c.bobA * std::sin(c.bobW * t + c.seed * 6.f) * r;
    if (c.diveAt >= 0.f && t > c.diveAt) {
        const float k = t - c.diveAt;
        y -= 0.45f * k * k;
    }
    return {x, y};
}

// A new creature rising on one side (side -1 left, 1 right); `far`: a small one drifting by in the distance.
void SkyOceanUniverse::spawnCreature(int kind, float side, float scale, bool far) {
    Creature c;
    c.kind = kind;
    c.seed = rng_.uniform();
    const float hw = viewH_.x, top = viewC_.y + viewH_.y;
    const float k = far ? 0.5f : 1.f;
    switch (kind) {
    case FISH: c.size = 0.72f * k; c.speed = 2.4f; c.count = far ? 7 : rng_.irange(9, 13); c.phaseRate = 9.f; c.riseT = 4.5f; c.life = 26.f; break;
    case JELLY: c.size = rng_.range(2.3f, 2.8f) * k; c.speed = 0.32f; c.phaseRate = 2.1f; c.riseT = 9.f; c.life = 34.f; break;
    case MANTA: c.size = 4.4f * k; c.speed = 1.5f; c.phaseRate = 1.6f; c.riseT = 4.5f; c.life = 34.f; break;
    default: c.size = 10.5f * k; c.speed = 1.05f; c.phaseRate = 1.2f; c.riseT = 6.f; c.life = 44.f; break;
    }
    c.size *= scale;
    c.speed *= far ? 0.6f : 1.f;
    c.dim = far ? 0.55f : 1.f;
    // it rises from the clouds beside the well, on that side, and swims across (fish and jellyfish either way)
    const float x0 = side * rng_.range(7.5f, std::max(8.f, hw - 2.5f));
    c.dir = (kind == FISH || kind == JELLY) ? (rng_.chance(0.5f) ? side : -side) : -side;
    if (std::fabs(x0) > hw - 4.f) c.dir = -side;
    c.surface = horizon_ - (far ? 0.5f : rng_.range(2.5f, 5.f));
    c.start = V(x0, c.surface - (kind == JELLY ? 1.2f : 0.6f) * c.size - 0.5f);
    const float sky = top - horizon_;
    if (far) c.cruiseY = horizon_ + sky * rng_.range(0.12f, 0.3f);
    else if (kind == WHALE) c.cruiseY = horizon_ + sky * rng_.range(0.35f, 0.5f);
    else if (kind == JELLY) c.cruiseY = horizon_ + sky * rng_.range(0.45f, 0.8f);
    else c.cruiseY = horizon_ + sky * rng_.range(0.25f, 0.75f);
    c.bobA = kind == JELLY ? 0.6f : kind == FISH ? 0.5f : 0.35f + 0.02f * c.size;
    c.bobW = kind == FISH ? 0.9f : 0.35f;
    c.phase = rng_.range(0.f, TAU);
    c.flash = far ? 0.3f : 1.f;
    // the oldest makes room
    int alive = 0;
    for (const Creature& o : creatures_)
        if (!o.leaving) alive++;
    if (alive >= MAX_CREATURES) {
        for (Creature& o : creatures_)
            if (!o.leaving) { o.leaving = true; break; }
    }
    creatures_.push_back(c);
}

// =====================================================================================================
// The drop's whale
// =====================================================================================================

void SkyOceanUniverse::startGiant() {
    Giant& g = giant_;
    if (g.on && g.diveAt < 0.f) return;
    g.on = true;
    g.dir = (g.dir > 0.f) ? -1.f : 1.f;
    g.size = std::max(26.f, viewH_.x * 1.45f);
    g.age = 0.f;
    g.diveAt = -1.f;
    g.alpha = 0.f;
    g.x = -g.dir * (viewH_.x + g.size * 0.3f);
    g.y = horizon_ + (viewC_.y + viewH_.y - horizon_) * 0.47f;
}

vec2 SkyOceanUniverse::giantPos(float age) const {
    const Giant& g = giant_;
    // across the whole sky in about a minute (a drop usually ends before: it dives on the way)
    const float speed = (2.f * viewH_.x + g.size * 0.9f) / 56.f;
    float x = g.x + g.dir * speed * age;
    float y = g.y - 3.5f * (1.f - smoothstepf(0.f, 6.f, age)) + 0.6f * std::sin(age * 0.22f);
    if (g.diveAt >= 0.f && age > g.diveAt) {
        const float k = age - g.diveAt;
        y -= 0.32f * k * k;
    }
    return {x, y};
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void SkyOceanUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && !t.minor && t.brightness > 0.6f) v = 1;          // dawn for bright, major songs
        else if (t.hasSong && t.minor && t.brightness < 0.42f) v = 3;     // amethyst for dark ones
        else {
            static const int BY_HUE[] = {0, 2, 0, 3, 0, 2};                 // moonlight the most often
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x5C0Eull);
    if (seconds <= 0.f) palSet_ = false;
}

void SkyOceanUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

// The drop: the giant whale comes (only on a real drop: see render).
void SkyOceanUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
}

void SkyOceanUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 SkyOceanUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints SkyOceanUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = p.skyMid;
    h.light = false;
    h.ink = p.ink;
    h.accent = p.life[0];
    h.styles = TS_INK | TS_LUMA | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void SkyOceanUniverse::syncIds(const Game& g) {
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

void SkyOceanUniverse::addMote(const Mote& m) {
    if ((int)motes_.size() >= MAX_MOTES) return;
    motes_.push_back(m);
}

void SkyOceanUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::HardDrop: {
        // a few motes puff out from under the piece
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
                m.kind = 3;  // in front of the veil
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
        // the creature: where it will rise
        const int lines = std::max(1, ev.count);
        const float side = nextSide_;
        nextSide_ = -nextSide_;
        const int kind = lines >= 4 ? WHALE : lines == 3 ? MANTA : lines == 2 ? JELLY : FISH;
        spawnCreature(kind, side, 1.f, false);
        const Creature c = creatures_.back();
        creatures_.pop_back();
        spawns_.push_back({FLOW_T * 0.85f, c});
        // the cells dissolve into light and flow out of the well to the cloud tops where it will rise
        const vec2 to = V(c.start.x, c.surface + 0.3f);
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
                mt.to = add(to, V(rng_.range(-0.8f, 0.8f) * (0.5f + 0.15f * c.size), rng_.range(-0.4f, 0.3f)));
                // out through the well's side, then down in an arc to the clouds
                mt.ctl = V(side * rng_.range(6.5f, 8.5f), std::max(mt.from.y, to.y) + rng_.range(0.5f, 3.f));
                mt.p = mt.from;
                mt.life = FLOW_T * rng_.range(0.75f, 1.05f);
                mt.age = -rng_.range(0.f, 0.25f) - (float)(Game::W - 1 - (side > 0 ? cl.x : Game::W - 1 - cl.x)) * 0.015f;
                mt.size = rng_.range(0.1f, 0.17f);
                mt.col = lerp(pal_.piece[std::clamp(cl.type, 0, 6)], pal_.life[0], 0.5f);
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
        if (lines >= 4) {
            quadGlow_ = 1.f;
            // a school of fish keeps the whale company
            spawnCreature(FISH, side, 1.f, false);
            Creature f = creatures_.back();
            creatures_.pop_back();
            f.start = add(c.start, V(c.dir * -2.f, -1.f));
            f.surface = c.surface;
            f.dir = c.dir;
            f.cruiseY = c.cruiseY + 2.5f;
            f.speed = c.speed * 1.25f;
            spawns_.push_back({FLOW_T * 0.85f + 0.8f, f});
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
        // every creature dives back into the clouds
        for (Creature& cr : creatures_)
            if (cr.diveAt < 0.f) cr.diveAt = cr.age;
        if (giant_.on && giant_.diveAt < 0.f) giant_.diveAt = giant_.age;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void SkyOceanUniverse::cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase,
                            float shine) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, shine, 0.f}});
}

void SkyOceanUniverse::buildBoard(const Game& g) {
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
    // rows dissolving into light (real time)
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

void SkyOceanUniverse::body(int kind, vec2 p, float size, float angle, float flip, const vec3& col, float alpha, float phase,
                            float seed, float surface, float glow) {
    if (alpha <= 0.004f || size <= 0.f) return;
    const vec2 e = bodyExtent(kind);
    const float m = std::min(0.06f, 0.5f / size) * 9.f;
    bodies_.push_back({{p.x, p.y, size, angle}, {col.x, col.y, col.z, alpha}, {(float)kind, phase, seed * 37.f, surface},
                       {e.x + m, e.y + m, flip, glow}});
}

void SkyOceanUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param, float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

// =====================================================================================================
// The HUD
// =====================================================================================================

void SkyOceanUniverse::drawHud(const std::vector<HudRect>& panels) {
    if (panels.empty()) return;
    std::vector<float> v;
    for (const HudRect& r : panels) {
        const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal_.accent : pal_.skyTop;
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

// The lettering, by the host (text.hpp), in moonlight: Josefin Sans Light capitals, widely spaced, the song, the
// variant and the notices in Cormorant italics; each line on a soft halo of the sky, in a glow of its ink. The inks
// are the scene's (linear), toned as the post tones the scene: dimmed, their highlights rolled off, to sRGB.
void SkyOceanUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* sans = text::font(FONT_SANS);
    text::Font* ital = text::font(FONT_ITAL);
    const float s = height_ / 1080.f;
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount), dim = 1.f - dimT_ * 0.5f;
    auto tone = [&](const vec3& c0) {
        vec3 c = c0 * dim;
        for (float* v : {&c.x, &c.y, &c.z}) *v /= 1.f + std::max(*v - 0.8f, 0.f) * 0.6f;
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
        const float sz = base * 1.0f, tr = sz * 0.3f, vsz = base * 0.95f * 1.3f, gap = base * 0.7f;
        const float a = saturate(0.35f + h.alpha * 1.6f);
        put(sans, name, sz, tr, ink, a);
        if (!var.empty()) {
            // (the name's spacing stays after its last letter: the gap takes it back; each run was placed by its own
            // cap line)
            text::Span& v = out.add(var, ital, vsz, accent, 0.f, a * 0.9f);
            v.gap = (gap - tr) / vsz;
            v.dy = -sz * 0.15f + text::capHeight(ital) * vsz - text::capHeight(sans) * sz;
        }
        break;
    }
    case HUD_SECTION: put(sans, h.text, base * 0.9f, base * 0.55f, ink, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(ital, h.text, base * 1.5f, base * 0.02f, ink, h.alpha * 1.3f, -base * 0.25f); break;
    case HUD_INFO: put(sans, h.text, base * 0.72f, base * 0.25f, ink, saturate(alpha * 1.3f) * 0.85f); break;
    case HUD_LABEL: put(sans, h.text, base * 0.74f, base * 0.45f, accent, saturate(alpha * 1.2f) * 0.85f); break;
    case HUD_VALUE: put(sans, h.text, base * 1.12f, base * 0.1f, ink, alpha); break;
    case HUD_NOTICE: put(ital, h.text, base * 1.4f, base * 0.02f, accent, alpha); break;
    case HUD_KEY: put(sans, h.text, base * 0.8f, base * 0.1f, ink, alpha); break;
    case HUD_TOAST: put(sans, h.text, base * 1.25f, base * 0.55f, ink, alpha, -base * 0.15f); full = true; break;
    case HUD_TOAST_SUB: put(ital, h.text, base * 1.25f, base * 0.02f, accent, alpha * 0.9f); full = true; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(sans, h.text, base * 1.15f, base * 0.22f, ink, alpha, -base * 0.1f);
        else put(sans, h.text, base * 0.8f, base * 0.14f, h.accent ? accent : ink, alpha);
        full = true;
        break;
    default: put(sans, h.text, base * 0.8f, base * 0.12f, h.accent ? accent : ink, alpha); break;
    }
    // the letters as the scene drew them, on a halo of the sky, in a glow (gone with the bridge)
    text::Lettering& l = lettering_[full ? 1 : 0];
    l.glsl = shaders::LETTER_GLSL;
    l.p0 = vec4(tone(lerp(pal_.skyTop * 0.6f, bridge_.color, b)), full ? 0.8f : 0.55f);
    l.p1 = vec4(0.5f * (1.f - b), ugl::luma(lerp((pal_.skyMid + pal_.cloudMid) * 0.5f, bridge_.color, b)) * dim, 0.f, 0.f);
    out.lettering = &l;
    out.alpha = saturate(fade_);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void SkyOceanUniverse::render(const Frame& f, GLuint fbo) {
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
    if (isDrop && !wasDrop_) startGiant();
    if (!isDrop && wasDrop_ && giant_.on && giant_.diveAt < 0.f) giant_.diveAt = giant_.age;
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 2.f : 0.5f, dt);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        if (slow_ > 0.01f) {
            // slow motion: a hush, the moon brighter and cooler
            const float k = slow_;
            pal_.moonGlow = lerp(pal_.moonGlow, pal_.moonGlow * vec3(0.9f, 1.f, 1.15f) * 1.35f, k);
            pal_.cloudLit = lerp(pal_.cloudLit, pal_.cloudLit * vec3(0.92f, 1.f, 1.1f) * 1.15f, k);
        }
    }
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    quadGlow_ = approach(quadGlow_, 0.f, 0.4f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    kickGlow_ = std::max(approach(kickGlow_, 0.f, 4.f, dt), kick * wp);
    twinkleBeat_ = approach(twinkleBeat_, 0.f, 3.f, dt);
    drift_ += wdt * (0.010f + 0.022f * wm + 0.03f * wp + 0.02f * life);
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    const float hw = viewH_.x, top = viewC_.y + viewH_.y, bot = viewC_.y - viewH_.y;

    // the beat: a soft swell of the creatures' light and a twinkle at a peak
    float beatPulse = music.hasSong ? music.beatPulse : 0.f;
    if (!f.paused) {
        auto onBeat = [&](int beat) {
            beatCount_ = beat;
            if (wp > 0.5f) twinkleBeat_ = std::max(twinkleBeat_, 0.5f + 0.3f * life);
            else if (wm > 0.5f && beat % 2 == 0) twinkleBeat_ = std::max(twinkleBeat_, 0.3f);
        };
        if (music.hasSong) {
            const double b = std::floor(music.beat + 1e-4);
            if (lastBeat_ < -0.5 || b < lastBeat_ - 8.0 || b > lastBeat_ + 8.0) lastBeat_ = b;
            while (b > lastBeat_) {
                lastBeat_ += 1.0;
                onBeat((int)lastBeat_ & 0x7fffffff);
            }
        } else {
            beatClock_ += dt;
            if (beatClock_ > 0.5f) {
                beatClock_ -= 0.5f;
                onBeat(beatCount_ + 1);
            }
            beatPulse = std::max(0.f, 1.f - beatClock_ * 4.f);
        }
    }
    const float lifeB = (0.78f + 0.12f * wm + 0.22f * wp + 0.2f * life + 0.25f * quadGlow_) * (1.f + (0.05f * wm + 0.1f * wp) * beatPulse);

    // ---- Spawns (the clears' creatures rise once their motes reach the clouds), ambient visitors.
    for (size_t i = 0; i < spawns_.size();) {
        spawns_[i].delay -= wdt;
        if (spawns_[i].delay <= 0.f) {
            const Creature c = spawns_[i].c;
            int alive = 0;
            for (const Creature& o : creatures_)
                if (!o.leaving) alive++;
            if (alive >= MAX_CREATURES)
                for (Creature& o : creatures_)
                    if (!o.leaving) { o.leaving = true; break; }
            creatures_.push_back(c);
            // a burst of light where it breaks through
            for (int k = 0; k < 14 + (int)(c.size * 2.f); k++) {
                Mote m;
                m.p = add(V(c.start.x, c.surface), V(rng_.range(-1.f, 1.f) * (0.4f + 0.2f * c.size), rng_.range(-0.3f, 0.3f)));
                m.v = V(rng_.range(-0.5f, 0.5f), rng_.range(0.3f, 1.4f));
                m.life = rng_.range(1.2f, 2.6f);
                m.size = rng_.range(0.08f, 0.18f);
                m.col = lerp(pal_.life[0], pal_.life[2], rng_.uniform() * 0.6f);
                m.seed = rng_.uniform();
                addMote(m);
            }
            spawns_.erase(spawns_.begin() + i);
            continue;
        }
        i++;
    }
    if (!f.paused && overT_ < 0.f) {
        ambientT_ -= wdt;
        int alive = 0;
        for (const Creature& o : creatures_)
            if (!o.leaving && o.dim > 0.9f) alive++;
        if (ambientT_ <= 0.f) {
            if (alive < 2) {
                const float r = rng_.uniform();
                spawnCreature(r < 0.45f ? JELLY : r < 0.8f ? FISH : r < 0.93f ? MANTA : WHALE, rng_.chance(0.5f) ? 1.f : -1.f, 1.f, true);
                creatures_.back().start.x = (rng_.chance(0.5f) ? 1.f : -1.f) * rng_.range(8.f, hw - 2.f);
                creatures_.back().surface = horizon_ - 0.4f;
                creatures_.back().start.y = horizon_ - 0.4f - creatures_.back().size * 0.8f;
            }
            ambientT_ = rng_.range(9.f, 16.f);
        }
    }

    // ---- Creatures move.
    for (Creature& c : creatures_) {
        c.age += wdt;
        c.phase += wdt * c.phaseRate * (c.kind == WHALE || c.kind == MANTA ? (0.85f + 0.3f * wp) : 1.f);
        c.flash = approach(c.flash, 0.f, 0.6f, dt);
        if (c.age > c.life) c.leaving = true;
        if (c.leaving || overT_ >= 0.f) c.fade = std::max(0.f, c.fade - wdt / (c.leaving ? 3.5f : 4.f));
        const vec2 p = pathAt(c, c.age);
        if (c.age > c.riseT && std::fabs(p.x) > hw + c.size * 0.9f + (c.kind == FISH ? 3.f : 0.f)) c.fade = 0.f;
        if (c.diveAt >= 0.f && p.y < c.surface - c.size * 0.8f - 1.f) c.fade = 0.f;
    }
    creatures_.erase(std::remove_if(creatures_.begin(), creatures_.end(), [](const Creature& c) { return c.fade <= 0.f; }),
                     creatures_.end());
    // the giant
    if (giant_.on) {
        Giant& g = giant_;
        g.age += wdt;
        g.alpha = approach(g.alpha, 1.f, 0.7f, dt);
        // its tail beats once a bar, on the beat grid
        if (music.hasSong) g.phase = (float)std::fmod(music.beat * (PI * 0.5), (double)TAU * 64.0);
        else g.phase += wdt * PI;
        const vec2 p = giantPos(g.age);
        if ((g.dir > 0.f ? p.x - g.size * 0.6f > hw : p.x + g.size * 0.6f < -hw) ||
            (g.diveAt >= 0.f && p.y < horizon_ - g.size * 0.35f))
            g.on = false;
    }

    // ---- Motes.
    {
        auto ambient = [&](float rate) {
            float n = rate * wdt;
            while (n > 0.f) {
                if (rng_.uniform() < n) {
                    Mote m;
                    m.p = V(rng_.range(-hw, hw), rng_.range(bot, horizon_ - 0.5f));
                    m.v = V(rng_.range(-0.1f, 0.1f), rng_.range(0.15f, 0.45f));
                    m.life = rng_.range(5.f, 9.f);
                    m.size = rng_.range(0.05f, 0.11f) * (1.f + 0.5f * (horizon_ - m.p.y) / (horizon_ - bot));
                    m.col = lerp(pal_.life[rng_.chance(0.5f) ? 0 : 1], pal_.life[2], rng_.range(0.f, 0.5f));
                    m.bright = rng_.range(0.4f, 0.9f);
                    m.seed = rng_.uniform();
                    addMote(m);
                }
                n -= 1.f;
            }
        };
        if (!f.paused && overT_ < 0.f) ambient(1.2f + 3.5f * wm + 6.f * wp + 10.f * life + 14.f * kickGlow_);
        // trails
        if (!f.paused) {
            for (Creature& c : creatures_) {
                if (c.fade < 0.2f) continue;
                const float rate = (c.kind == FISH ? 7.f : c.kind == JELLY ? 4.f : c.kind == MANTA ? 9.f : 14.f) * c.dim * (1.f + 0.6f * wp);
                c.trail += rate * wdt;
                const vec2 p = pathAt(c, c.age), q = pathAt(c, c.age - 0.1f);
                vec2 dir = sub(p, q);
                const float dl = len(dir);
                dir = dl > 1e-4f ? mul(dir, 1.f / dl) : V(c.dir, 0.f);
                if (p.y < c.surface + 0.2f) { c.trail = 0.f; continue; }
                while (c.trail >= 1.f) {
                    c.trail -= 1.f;
                    Mote m;
                    const float back = c.kind == JELLY ? 0.f : c.size * 0.45f;
                    vec2 at = sub(p, mul(dir, back));
                    if (c.kind == JELLY) at = add(p, V(rng_.range(-0.3f, 0.3f) * c.size, -c.size * rng_.range(0.1f, 0.5f)));
                    if (c.kind == FISH) at = add(at, V(rng_.range(-1.2f, 1.2f), rng_.range(-0.6f, 0.6f)));
                    m.p = add(at, V(rng_.range(-0.15f, 0.15f) * c.size * 0.3f, rng_.range(-0.15f, 0.15f) * c.size * 0.3f));
                    m.v = add(mul(dir, -0.15f), V(rng_.range(-0.12f, 0.12f), rng_.range(0.0f, 0.2f)));
                    m.life = rng_.range(2.f, 3.8f) * (1.f + slow_);
                    m.size = rng_.range(0.05f, 0.11f) * (c.kind == WHALE ? 1.5f : 1.f) * (c.dim < 0.9f ? 0.7f : 1.f);
                    m.col = lerp(pal_.life[c.kind == MANTA || c.kind == JELLY ? 1 : 0], pal_.life[2], rng_.range(0.f, 0.5f));
                    m.bright = c.dim * c.fade;
                    m.seed = rng_.uniform();
                    m.kind = rng_.chance(0.15f) ? 2 : 0;
                    addMote(m);
                }
            }
            if (giant_.on && giant_.alpha > 0.1f) {
                Giant& g = giant_;
                g.trail += (60.f + 40.f * kickGlow_) * wdt;
                const vec2 p = giantPos(g.age), q = giantPos(g.age - 0.2f);
                vec2 dir = sub(p, q);
                const float dl = len(dir);
                dir = dl > 1e-4f ? mul(dir, 1.f / dl) : V(g.dir, 0.f);
                const vec2 nrm = V(-dir.y, dir.x);
                while (g.trail >= 1.f) {
                    g.trail -= 1.f;
                    Mote m;
                    const float u = rng_.range(-0.45f, 0.5f);
                    m.p = add(add(p, mul(dir, u * g.size)), mul(nrm, rng_.range(-0.09f, 0.08f) * g.size * (1.f - std::fabs(u))));
                    m.v = add(mul(dir, -0.3f), V(rng_.range(-0.2f, 0.2f), rng_.range(-0.1f, 0.35f)));
                    m.life = rng_.range(2.5f, 5.f);
                    m.size = rng_.range(0.06f, 0.16f);
                    m.col = lerp(pal_.life[0], pal_.life[2], rng_.range(0.f, 0.7f));
                    m.bright = g.alpha;
                    m.seed = rng_.uniform();
                    m.kind = rng_.chance(0.2f) ? 2 : 0;
                    addMote(m);
                }
            }
        }
        for (Mote& m : motes_) {
            const float mdt = m.kind == 1 || m.kind == 3 ? dt : wdt;
            m.age += mdt;
            if (m.kind == 1) {
                const float t = smoothstepf(0.f, 1.f, saturate(m.age / m.life));
                const vec2 a = lerp2(m.from, m.ctl, t), b = lerp2(m.ctl, m.to, t);
                m.p = lerp2(a, b, t);
            } else {
                m.p = add(m.p, mul(m.v, mdt));
                m.v.x += std::sin(time_ * 0.8f + m.seed * 30.f) * 0.08f * mdt;
                if (m.kind == 3) m.v = mul(m.v, std::exp(-2.5f * mdt));
            }
        }
        motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.age >= m.life; }), motes_.end());
    }
    for (auto& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The moon.
    const float sky = top - horizon_;
    moonP_ = V(viewC_.x + hw * pal_.moonX, horizon_ + sky * pal_.moonY);
    moonR_ = pal_.moonR;

    // ---- Creatures to draw, and their light on the clouds.
    bodies_.clear();
    sprites_.clear();
    struct Light { vec2 p; float r, k; vec3 c; };
    std::vector<Light> lights;
    const float creatureA = world * (1.f - 0.6f * over_);
    auto headingOf = [](vec2 d) { return std::atan2(d.y, d.x); };
    auto lightBelow = [&](vec2 p, float r, float k, const vec3& c) {
        const float h = std::max(0.f, p.y - horizon_);
        lights.push_back({V(p.x, std::min(p.y, horizon_ - 0.4f)), r + 0.25f * h, k * std::exp(-h / 6.f), c});
    };
    if (giant_.on) {
        const Giant& g = giant_;
        const vec2 p = giantPos(g.age), q = giantPos(g.age - 0.25f);
        const vec2 d = sub(p, q);
        float ang = headingOf(d);
        const float flip = std::cos(ang) < 0.f ? -1.f : 1.f;
        const float a = g.alpha * creatureA;
        body(WHALE, p, g.size, ang, flip, lerp(pal_.life[0], pal_.life[2], 0.1f), a * std::min(1.2f, lifeB * 1.05f), g.phase, 0.37f,
             horizon_ - 0.6f, 1.f);
        for (int i = -1; i <= 1; i++)
            lightBelow(add(p, V(std::cos(ang) * i * g.size * 0.28f, 0.f)), 6.5f, 0.5f * a * (1.f + 0.3f * kickGlow_), pal_.life[0]);
    }
    // the bigger ones behind the smaller
    std::vector<int> order(creatures_.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return creatures_[a].size * (creatures_[a].dim < 0.9f ? 0.1f : 1.f) > creatures_[b].size * (creatures_[b].dim < 0.9f ? 0.1f : 1.f);
    });
    for (int idx : order) {
        const Creature& c = creatures_[idx];
        const float a = smoothstepf(0.f, 1.f, c.fade) * c.dim * creatureA * smoothstepf(0.f, 0.8f, c.age);
        const vec3 col = c.kind == MANTA ? lerp(pal_.life[1], pal_.life[0], 0.25f)
                       : c.kind == JELLY ? lerp(pal_.life[1], pal_.life[2], 0.15f)
                       : c.kind == FISH ? lerp(pal_.life[0], pal_.life[2], 0.25f) : pal_.life[0];
        const float bright = lifeB * (1.f + 0.8f * c.flash);
        if (c.kind == FISH) {
            vec2 center{0, 0};
            for (int i = 0; i < c.count; i++) {
                const float hi = hashf(c.seed * 91.f + i * 7.3f), hj = hashf(c.seed * 53.f + i * 3.1f);
                const float lag = (i == 0 ? 0.f : 0.12f + 0.75f * hi);
                const float t = c.age - lag;
                const vec2 p = pathAt(c, t), q = pathAt(c, t - 0.08f);
                const vec2 d = sub(p, q);
                const float dl = len(d);
                const vec2 n = dl > 1e-4f ? V(-d.y / dl, d.x / dl) : V(0.f, 1.f);
                const float off = (hj - 0.5f) * 2.2f + 0.25f * std::sin(c.age * 1.3f + i);
                const vec2 at = add(p, mul(n, off));
                float ang = headingOf(d);
                const float flip = std::cos(ang) < 0.f ? -1.f : 1.f;
                const float sz = c.size * (0.8f + 0.4f * hashf(c.seed * 17.f + i));
                body(FISH, at, sz, ang, flip, col, a * bright * (t > 0.f ? 1.f : 0.f), c.phase + i * 1.7f, c.seed + i * 0.1f,
                     c.surface, 1.f);
                center = add(center, at);
            }
            center = mul(center, 1.f / std::max(1, c.count));
            lightBelow(center, 3.f, 0.25f * a * bright, col);
        } else {
            const vec2 p = pathAt(c, c.age), q = pathAt(c, c.age - 0.12f);
            const vec2 d = sub(p, q);
            float ang = headingOf(d);
            float flip = 1.f;
            if (c.kind == JELLY) ang = PI * 0.5f - std::clamp(d.x * 2.f, -0.35f, 0.35f) + 0.08f * std::sin(c.phase * 0.5f);
            else {
                // level off: no steeper than 65 degrees
                const float lim = 1.13f;
                const float cs = std::cos(ang), sn = std::sin(ang);
                const float pitch = std::atan2(sn, std::fabs(cs));
                const float cp = std::clamp(pitch, -lim, lim);
                ang = cs < 0.f ? PI - cp : cp;
                if (c.kind == WHALE) flip = std::cos(ang) < 0.f ? -1.f : 1.f;
            }
            body(c.kind, p, c.size, ang, flip, col, a * bright, c.phase, c.seed, c.surface, 1.f);
            lightBelow(p, 1.5f + c.size * 0.5f, (0.3f + 0.9f * c.flash) * a * bright, col);
        }
    }
    // the brightest lights on the clouds
    std::sort(lights.begin(), lights.end(), [](const Light& a, const Light& b) { return a.k > b.k; });
    if ((int)lights.size() > MAX_LIGHTS) lights.resize(MAX_LIGHTS);

    // ---- Motes into sprites (back: behind the veil; front: the clears' motes and the gauge).
    std::vector<SpriteInst> front;
    for (const Mote& m : motes_) {
        if (m.age < 0.f) continue;
        const float t = m.age / m.life;
        const float tw = 0.75f + 0.25f * std::sin(time_ * 4.f + m.seed * 40.f);
        float a = smoothstepf(0.f, 0.12f, t) * (1.f - smoothstepf(0.6f, 1.f, t)) * m.bright * tw;
        if (m.kind == 1) a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.85f, 1.f, t)) * m.bright;
        const int mode = m.kind == 2 ? 2 : 0;
        const float size = m.size * (mode == 2 ? 2.6f : 1.6f) * (m.kind == 1 ? 1.3f : 1.f);
        if (m.kind == 1 || m.kind == 3) {
            const size_t n = sprites_.size();
            sprite(m.p, size, m.col, a * world, mode);
            if (sprites_.size() > n) { front.push_back(sprites_.back()); sprites_.pop_back(); }
        } else sprite(m.p, size, m.col, a * creatureA * (0.8f + 0.3f * wp + 0.3f * life), mode);
    }
    if (!f.bare) {
        // the bonus gauge: a faint ring of moonlight, the charge a bright arc
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        const size_t n = sprites_.size();
        sprite(V(gc.x, gc.y), layout::GAUGE_R * 1.25f, on ? pal_.life[2] : pal_.accent, 1.f, 1, fill, on ? 1.f : 0.f);
        if (sprites_.size() > n) { front.push_back(sprites_.back()); sprites_.pop_back(); }
    }

    cells_.clear();
    // a break (bare): the sky alone, no pieces, no previews, no clear motes
    if (!f.bare) buildBoard(game);
    else front.clear();

    // ---- The sky and the sea of clouds (reduced resolution).
    skyT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(progSky_);
    set4f(progSky_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progSky_, "uHorizon", horizon_);
    glUniform3f(ugl::U(progSky_, "uMoon"), moonP_.x, moonP_.y, moonR_);
    set3f(progSky_, "uSkyTop", pal_.skyTop);
    set3f(progSky_, "uSkyMid", pal_.skyMid);
    set3f(progSky_, "uHorizonC", pal_.horizon);
    set3f(progSky_, "uHaze", pal_.haze);
    set3f(progSky_, "uMoonC", pal_.moon);
    set3f(progSky_, "uMoonGlow", pal_.moonGlow);
    set3f(progSky_, "uCloudLit", pal_.cloudLit);
    set3f(progSky_, "uCloudMid", pal_.cloudMid);
    set3f(progSky_, "uCloudShadow", pal_.cloudShadow);
    set3f(progSky_, "uAur1", pal_.aur1);
    set3f(progSky_, "uAur2", pal_.aur2);
    set1f(progSky_, "uAurora", pal_.aurora * (0.7f + 0.2f * wm + 0.3f * wp + 0.3f * life));
    set1f(progSky_, "uDrift", drift_);
    set1f(progSky_, "uTime", time_);
    set1f(progSky_, "uGlow", 0.25f * wm + 0.45f * wp + 0.6f * life + 0.5f * quadGlow_ + 0.2f * swell_);
    set1f(progSky_, "uPx", skyT_.h / (2.f * viewH_.y));
    set1f(progSky_, "uSeed", (float)(seed_ % 97));
    set1f(progSky_, "uMoonLight", 1.f + 0.12f * wp + 0.25f * slow_);
    {
        float lp[MAX_LIGHTS * 4] = {}, lc[MAX_LIGHTS * 3] = {};
        for (size_t i = 0; i < lights.size(); i++) {
            lp[i * 4 + 0] = lights[i].p.x; lp[i * 4 + 1] = lights[i].p.y;
            lp[i * 4 + 2] = lights[i].r; lp[i * 4 + 3] = lights[i].k;
            lc[i * 3 + 0] = lights[i].c.x; lc[i * 3 + 1] = lights[i].c.y; lc[i * 3 + 2] = lights[i].c.z;
        }
        glUniform4fv(ugl::U(progSky_, "uLights"), MAX_LIGHTS, lp);
        glUniform3fv(ugl::U(progSky_, "uLightCol"), MAX_LIGHTS, lc);
        set1i(progSky_, "uLightN", (int)lights.size());
    }
    set1f(progSky_, "uBridge", bridge);
    set3f(progSky_, "uBridgeCol", bridge_.color);
    ugl::drawFullscreen();

    // ---- The scene: the sky upscaled, the stars.
    sceneT_.bind();
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyT_.tex);
    set1i(progComp_, "uSky", 0);
    set2f(progComp_, "uRes", (float)width_, (float)height_);
    set1f(progComp_, "uTwinkle", std::min(0.9f, 0.18f + 0.15f * wm + 0.15f * wp + twinkleBeat_ * 0.5f));
    set1f(progComp_, "uStarA", (0.85f + 0.1f * wp + 0.2f * life) * (1.f - 0.5f * over_));
    set1f(progComp_, "uTime", time_);
    set1f(progComp_, "uSeed", (float)(seed_ % 61));
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    // ---- Creatures (translucent: a little cover, mostly light).
    if (!bodies_.empty()) {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(progBody_);
        setMat(progBody_, "uVP", vp_);
        set1f(progBody_, "uTime", time_);
        set1f(progBody_, "uPxPerUnit", pxPerUnit_);
        set3f(progBody_, "uTint2", pal_.life[1]);
        glBindVertexArray(bodyVao_);
        glBindBuffer(GL_ARRAY_BUFFER, bodyVbo_);
        glBufferData(GL_ARRAY_BUFFER, bodies_.size() * sizeof(BodyInst), bodies_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)bodies_.size());
    }
    auto drawSprites = [&](const std::vector<SpriteInst>& v) {
        if (v.empty()) return;
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        set1f(progSprite_, "uPxPerUnit", pxPerUnit_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(SpriteInst), v.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
    };
    drawSprites(sprites_);

    // ---- The veil behind the well and the HUD's columns (none in a break).
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.skyTop * 0.55f, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(pal_.moonGlow * 0.55f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", 0.84f);
        set1f(progVeil_, "uColA", (0.38f + 0.2f * (giant_.on ? giant_.alpha : 0.f)) * world);
        set1f(progVeil_, "uRailA", 0.55f);
        set1f(progVeil_, "uPx", pxPerUnit_);
        // the columns' veils reach the well's (no lighter strip between them)
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], 5.f * -1.f, layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }

    // ---- Pieces, previews.
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uTime", time_);
        set3f(progCell_, "uMoonC", pal_.moon);
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    drawSprites(front);
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

const UniverseEntry& universe_skyocean() {
    static const UniverseEntry e{
        "CELESTIAL OCEAN", "a sea of clouds at night under a big moon: cleared rows rise as creatures of light swimming through the sky",
        [] { return std::unique_ptr<Universe>(new SkyOceanUniverse()); },
        // Fits most songs; the airy, spacious ones a little better.
        [](const SongLook& l) { return 0.85f + 0.25f * (1.f - l.density) + 0.15f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const NightDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
