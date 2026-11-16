// MICROSCOPE: the whole screen a microscope's field of view, in the soft look of a micrograph.
//
// The world: a pale, softly vignetted field of light (grey-teal, lilac... or black in dark-field), its illumination a
// little uneven, a haze of things out of focus drifting through, small round cells and bubbles ringed by phase-contrast
// halos and fine debris floating in the medium (kept out of the well, thinned behind the HUD). The well is a counting
// chamber: its walls and floor etched in pairs of fine lines, a faint ruling of one line per row and column, the
// chamber a hair clearer than the field.
//
// The pieces are living tissue, seen through the same optics as the life around them (its shading shared): until a
// drop each block one translucent cell at the organisms' own value, only tinted by its stain, in a darker membrane
// breathing a little, a pale phase halo outside it, a soft nucleus, the type's organelles as soft blobs and fine
// ringed vacuoles, fine granules; a drop brings the stains' colors out (each block then a cell of stained cytoplasm,
// denser along its walls, finely mottled, glowing in the drop's light); the four cells of a piece are one colony, joined by thin walls inside one outer membrane.
// The seven types are seven stains (methylene blue, iodine, crystal violet, malachite, safranin...), each with its
// own inner life: a vacuole, starch grains, a big nucleus with its nucleolus, chloroplasts along the wall, two
// nuclei, dense granules, a large vacuole pushing the nucleus aside. The falling piece is exactly a locked one, the
// ghost the colony's membrane alone, the previews the same colonies.
//
// The life: a few microorganisms wander calmly on the sides, each on its own side and at its own depth, steering
// clear of the well and the HUD: a large paramecium (a slipper fringed with cilia beating in waves, its nucleus and
// vacuoles; it backs up and turns when it bumps into something), a smaller one, an amoeba reaching out with its
// pseudopods, pennate and centric diatoms, a volvox rolling slowly. The focal plane drifts and refocuses at each new
// section, so some of them are soft.
//
// A cleared row's cells burst at once: each scatters into a spray of granules of its stain, flung out and fading. The song: calm is a still, dimmer field and slow life; mid wakes the cilia on
// the beat grid (every two beats); a peak brightens the lamp, the kicks flicking the cilia and making the paramecia
// dart. A DROP section (music.inDrop) switches the illumination to the variant's second one (dark-field or
// fluorescence: light on black, the membranes and nuclei glowing), eased in and out, with one swell of light, the
// organisms swimming faster. Slow motion stretches time (the host gives it its common look). Game over kills
// the tissue from the top: each cell's protoplast shrinks away from its membrane (plasmolysis), its stain drains to
// grey and it fades. Around a song the lamp warms up from a dim orange glow while the slide comes into focus (the
// setup), and the slide goes out of focus while the lamp goes down (the teardown). It leaves and enters with the
// field fading to the bridge, the stack in its ink.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/microscope/microscope.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include "layout.hpp"
#include "universes/microscope/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Lato (labels, numbers) and EB Garamond Italic (the song, like a species' name), SIL Open Font License, see
// OFL-Lato.txt and OFL-EBGaramond.txt.
const unsigned char FONT_SANS[] = {
#include "universes/microscope/Lato-Regular.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/microscope/EBGaramond12-Italic.ttf.inc"
};

namespace shaders = microscope_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// An illumination (sRGB): the field (center, edge), the seven piece types' stains (I O T S Z J L), the organisms
// (body, rim, organelles), the nuclei's glow in dark-field, dark-field or not, the phase halos' strength.
struct LightDef {
    unsigned field[2];
    unsigned piece[7];
    unsigned org[3];
    unsigned nucGlow;
    float dark, phase;
};
// A variant: its own illumination and the one a drop switches to, the HUD's inks (deep, pale) and accent.
struct VariantDef {
    const char* name;
    LightDef own, drop;
    unsigned deep, pale, accent;
};
const VariantDef VARIANTS[] = {
    // an even, pale field, pastel stains; the drop: dark-field
    {"BRIGHTFIELD",
     {{0xdce3df, 0xb3c0bc}, {0x6aa2d8, 0xe2b14e, 0xa486d6, 0x62bd84, 0xe0646a, 0x6670c8, 0xec8c4e}, {0x8a9c9a, 0x2e3c3e, 0x4a5658}, 0x8ab4ff, 0.f, 0.75f},
     {{0x0e1b1f, 0x030607}, {0x6cc8ff, 0xffd060, 0xc89aff, 0x6cf0a0, 0xff7a80, 0x8a90ff, 0xffa060}, {0x9fb8c0, 0xd8eef2, 0xf2e2c0}, 0x9ab8ff, 1.f, 0.f},
     0x1d2a2c, 0xeef3f1, 0x2f7fa8},
    // phase contrast: the grey-teal field, bright halos, paler stains; the drop: fluorescence
    {"PHASE",
     {{0xb9c8c5, 0x93a6a4}, {0x7eacd6, 0xdcb966, 0xab92d6, 0x7cc496, 0xdc7e84, 0x7a84c8, 0xe69a70}, {0x8b9d9b, 0x2f3d3b, 0x4a5856}, 0x5a7cff, 0.f, 1.f},
     {{0x080b14, 0x020306}, {0x40c8ff, 0xf6e04a, 0xd060ff, 0x50f080, 0xff4a6a, 0x5a6aff, 0xff8a3a}, {0x2c7a50, 0x5cf08e, 0x7a8cff}, 0x5a7cff, 1.f, 0.f},
     0x1b2826, 0xedf3f1, 0x2e8a7e},
    // dark-field: light scattered on black, pastel glows, silver life; the drop: fluorescence
    {"DARKFIELD",
     {{0x101c21, 0x040709}, {0x8fd2ec, 0xeed08a, 0xbca6ee, 0x8ee0b0, 0xf09a96, 0x96a6f0, 0xf2b07e}, {0x9ab0b8, 0xdcebf0, 0xefe2c8}, 0xe8f0ff, 1.f, 0.f},
     {{0x05060c, 0x010103}, {0x3fe0ff, 0xffe24a, 0xd070ff, 0x4cf07c, 0xff5a70, 0x5a7aff, 0xff9a3a}, {0x3aa860, 0x6cff9a, 0x7a96ff}, 0x4a6aff, 1.f, 0.f},
     0x1a2428, 0xe6eff2, 0x9ad8ec},
    // a Giemsa stain on a pale lilac field: azure, eosin, purples; the drop: dark-field, the eosin fluorescing
    {"GIEMSA",
     {{0xe8e0e8, 0xc4b9ca}, {0x5f8fd6, 0xd9a546, 0x8e5cb8, 0x4fa8a0, 0xcf4f72, 0x5048a0, 0xe8889e}, {0xa898b4, 0x3e3050, 0x5e3a78}, 0x9a8aff, 0.f, 0.55f},
     {{0x140b14, 0x040205}, {0x7aa8ff, 0xffc060, 0xc070ff, 0x50e0c8, 0xff5a8a, 0x8a70ff, 0xff9ab8}, {0xa07ab0, 0xf0c8f0, 0xffd0a0}, 0x9a8aff, 1.f, 0.f},
     0x2a1f30, 0xf3eef3, 0xa8407a},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float DIE_T = 2.2f;        // game over: a cell's plasmolysis
constexpr int FREE_COLONY = 1 << 20; // the colony of cells found on the board (inherited): joined by type

// The kinds of life: paramecium, amoeba, pennate diatom, centric diatom, rotifer, volvox.
enum { K_PARA, K_AMOEBA, K_PENNATE, K_CENTRIC, K_ROTIFER, K_VOLVOX, K_COUNT };
const float KIND_SPEED[K_COUNT] = {0.1f, 0.014f, 0.045f, 0.015f, 0.05f, 0.05f};   // body sizes per second
const float KIND_TURN[K_COUNT] = {0.45f, 0.25f, 0.06f, 0.15f, 0.35f, 0.25f};

float wrapAngle(float a) {
    while (a > PI) a -= TAU;
    while (a < -PI) a += TAU;
    return a;
}
float sdBox(vec2 p, vec2 c, vec2 h) {
    const float dx = std::fabs(p.x - c.x) - h.x, dy = std::fabs(p.y - c.y) - h.y;
    const float ox = std::max(dx, 0.f), oy = std::max(dy, 0.f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.f);
}
// A colony's offset in the membrane's noise (each colony breathes its own way).
vec2 colonyOffset(int colony) {
    const float c = (float)(colony % 4096);
    return vec2{std::fmod(c * 7.137f, 61.f), std::fmod(c * 3.771f + 13.f, 59.f)};
}
// The joined neighbors of cell (x, y) among cells whose membership `in(x, y)` tells (bits: left, right, up, down,
// up-left, up-right, down-left, down-right; y grows downward).
template <class F> int maskOf(int x, int y, F in) {
    int m = 0;
    if (in(x - 1, y)) m |= 1;
    if (in(x + 1, y)) m |= 2;
    if (in(x, y - 1)) m |= 4;
    if (in(x, y + 1)) m |= 8;
    if (in(x - 1, y - 1)) m |= 16;
    if (in(x + 1, y - 1)) m |= 32;
    if (in(x - 1, y + 1)) m |= 64;
    if (in(x + 1, y + 1)) m |= 128;
    return m;
}

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

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool MicroscopeUniverse::init(int width, int height) {
    using shaders::withCommon;
    for (auto& l : fallLook_) l = lookOf();
    std::fill(&has_[0][0], &has_[0][0] + Game::H * Game::W, (int8_t)-1);
    progField_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::FIELD_FS).c_str(), "microscope field");
    progOrg_ = ugl::program(shaders::ORG_VS, withCommon(shaders::ORG_FS).c_str(), "microscope life");
    progPiece_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::PIECE_FS).c_str(), "microscope cells");
    progSpeck_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::SPECK_FS).c_str(), "microscope granules");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "microscope down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "microscope blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "microscope final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "microscope shape");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    instVao(quadVbo_, pieceVao_, pieceVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, orgVao_, orgVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, speckVao_, speckVbo_, 4, sizeof(Inst));
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
    none.variant = 1;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void MicroscopeUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    blurA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    blurB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    for (Org& o : orgs_) o.goal = pickGoal(o.side, o.size);
}

MicroscopeUniverse::~MicroscopeUniverse() {
    for (ugl::Target* t : {&sceneT_, &blurA_, &blurB_}) t->destroy();
    for (GLuint p : {progField_, progOrg_, progPiece_, progSpeck_, progDown_, progBlur_, progFinal_, progShape_}) glDeleteProgram(p);
    for (GLuint v : {pieceVao_, orgVao_, speckVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, pieceVbo_, orgVbo_, speckVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void MicroscopeUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        // dark songs in dark-field; the others in bright-field, phase contrast or Giemsa, by the key's hue and mode
        if (!t.hasSong) v = 1;
        else if (t.brightness < 0.42f || (t.minor && t.brightness < 0.48f)) v = 2;
        else {
            const int pick = (int)(h * 3.f) % 3;
            v = pick == 0 ? (t.minor ? 1 : 0) : pick == 1 ? 1 : 3;
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
    }
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    auto fill = [&](Light& s, const LightDef& d) {
        for (int i = 0; i < 2; i++) s.field[i] = lin(d.field[i]);
        for (int i = 0; i < 7; i++) s.piece[i] = lin(d.piece[i]);
        for (int i = 0; i < 3; i++) s.org[i] = lin(d.org[i]);
        s.nucGlow = lin(d.nucGlow);
        s.dark = d.dark;
        s.phase = d.phase;
    };
    fill(ownT_, V.own);
    fill(dropT_, V.drop);
    const bool instant = !colorsSet_ || seconds <= 0.1f;
    if (instant) {
        own_ = ownT_;
        drop_ = dropT_;
        colorsSet_ = true;
    }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    rng_ = Rng(seed_ ^ 0x3C1C0Bull);
    if (orgs_.empty()) populate();
    focus_ = focusT_ = 0.f;
}

void MicroscopeUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    (void)energy;
    (void)impact;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    if (level != level_) focusT_ = rng_.range(-0.35f, 0.35f);   // a new section: the observer refocuses
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

void MicroscopeUniverse::refresh(float strength) {
    (void)strength;
    focusT_ = rng_.range(-0.4f, 0.4f);
}

vec2 MicroscopeUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

float MicroscopeUniverse::darkNow() const { return lerpf(own_.dark, drop_.dark, smoothstepf(0.f, 1.f, dropK_)); }

TransitionHints MicroscopeUniverse::hints() const {
    TransitionHints h;
    const bool dark = darkNow() > 0.5f;
    h.light = !dark;
    h.background = own_.field[0];
    vec3 ink(0.f);
    for (int i = 0; i < 7; i++) ink = ink + own_.piece[i] * (1.f / 7.f);
    h.ink = dark ? ink : ink * 0.6f;
    h.accent = lin(VARIANTS[variant_].accent);
    h.styles = TS_LUMA | TS_INK | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// The life
// =====================================================================================================

// The cast: on one side a large paramecium, a pennate diatom and a centric one; on the other a smaller paramecium,
// an amoeba and a volvox.
void MicroscopeUniverse::populate() {
    orgs_.clear();
    const int hero = rng_.chance(0.5f) ? -1 : 1;
    struct Role { int kind, side; float size, depth; };
    const Role cast[] = {{K_PARA, hero, 6.2f, 0.f},          {K_PENNATE, hero, 2.7f, 0.5f}, {K_CENTRIC, hero, 1.9f, -0.6f},
                         {K_PARA, -hero, 3.4f, -0.35f},     {K_AMOEBA, -hero, 3.4f, 0.3f}, {K_VOLVOX, -hero, 2.1f, 0.7f}};
    for (const Role& r : cast) {
        Org o;
        o.kind = r.kind;
        o.side = r.side;
        o.size = r.size * rng_.range(0.92f, 1.08f);
        o.depth = r.depth;
        o.seed = rng_.uniform();
        o.turnPhase = rng_.range(0.f, TAU);
        o.ang = rng_.range(0.f, TAU);
        o.speed = KIND_SPEED[r.kind] * o.size * rng_.range(0.85f, 1.15f);
        o.spin = rng_.range(0.f, TAU);
        o.p = pickGoal(o.side, o.size);
        o.goal = pickGoal(o.side, o.size);
        o.goalT = rng_.range(10.f, 30.f);
        orgs_.push_back(o);
    }
}

// A place for an organism to head for on its side: clear of the well and the HUD, rather away from the others.
vec2 MicroscopeUniverse::pickGoal(int side, float size) {
    vec2 best{side * (viewH_.x - size * 0.6f), 0.f};
    float bestScore = -1e9f;
    const float margin = 0.5f * size;
    for (int tryN = 0; tryN < 10; tryN++) {
        const vec2 h{viewC_.x + side * rng_.range(6.f, viewH_.x - 0.2f * size),
                     viewC_.y + rng_.range(-viewH_.y + 0.3f * size, viewH_.y - 0.3f * size)};
        const float zd = zoneDist(h, nullptr);
        float nearest = 30.f;
        for (const Org& o : orgs_) {
            if (o.side != side) continue;
            const float dx = o.goal.x - h.x, dy = o.goal.y - h.y;
            nearest = std::min(nearest, std::sqrt(dx * dx + dy * dy) - 0.4f * o.size);
        }
        const float score = std::min(zd - margin, 0.f) * 4.f + std::min(nearest, 6.f) + rng_.range(0.f, 1.5f);
        if (score > bestScore) { bestScore = score; best = h; }
    }
    return best;
}

// Distance from p to the places the life keeps away from (the well, the HUD's columns and corner lines, the screen's
// edge, a little beyond it): positive outside them, negative inside; its gradient (pointing out).
float MicroscopeUniverse::zoneDist(const vec2& p, vec2* grad) const {
    auto f = [&](const vec2& q) {
        float d = sdBox(q, vec2{0.f, 0.6f}, vec2{5.6f, 11.6f});
        d = std::min(d, sdBox(q, vec2{-8.6f, 1.8f}, vec2{2.8f, 8.8f}));
        d = std::min(d, sdBox(q, vec2{8.6f, 3.4f}, vec2{2.8f, 7.2f}));
        const float hx = viewH_.x + 2.f;
        d = std::min(d, sdBox(q, vec2{-hx + (hx - 11.5f) * 0.5f, 11.f}, vec2{(hx - 11.5f) * 0.5f, 1.9f}));
        d = std::min(d, sdBox(q, vec2{-hx + (hx - 11.5f) * 0.5f, -10.6f}, vec2{(hx - 11.5f) * 0.5f, 1.9f}));
        d = std::min(d, viewH_.x + 0.4f - std::fabs(q.x - viewC_.x));
        d = std::min(d, viewH_.y + 0.4f - std::fabs(q.y - viewC_.y));
        return d;
    };
    const float d = f(p);
    if (grad) {
        const float e = 0.15f;
        vec2 g{f(vec2{p.x + e, p.y}) - f(vec2{p.x - e, p.y}), f(vec2{p.x, p.y + e}) - f(vec2{p.x, p.y - e})};
        const float l = std::sqrt(g.x * g.x + g.y * g.y);
        *grad = l > 1e-5f ? vec2{g.x / l, g.y / l} : vec2{p.x < 0 ? -1.f : 1.f, 0.f};
    }
    return d;
}

// =====================================================================================================
// Game events
// =====================================================================================================

MicroscopeUniverse::CellLook MicroscopeUniverse::lookOf() {
    CellLook l;
    const float a = rng_.range(0.f, TAU), d = rng_.range(0.03f, 0.1f);
    l.nx = std::cos(a) * d;
    l.ny = std::sin(a) * d;
    l.seed = rng_.uniform();
    return l;
}

// Keeps the looks in step with the board (a new board, a stack inherited from another universe or played while this
// one was away: its cells joined by type).
void MicroscopeUniverse::syncLooks(const Game& g) {
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const int type = g.cell(x, y).type;
            if (type >= 0 && has_[y][x] != type) {
                look_[y][x] = lookOf();
                look_[y][x].colony = FREE_COLONY + type;
            }
            has_[y][x] = (int8_t)std::max(type, -1);
        }
}

void MicroscopeUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        // the falling colony settles as it is (its anchor: its cells' lowest x and y, as while it fell)
        int ax = Game::W, ay = Game::H;
        for (size_t i = 0; i < ev.cells.size() && i < 4; i++) { ax = std::min(ax, ev.cells[i].x); ay = std::min(ay, ev.cells[i].y); }
        for (size_t i = 0; i < ev.cells.size() && i < 4; i++) {
            const auto& c = ev.cells[i];
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            CellLook& l = look_[c.y][c.x];
            l = fallLook_[i];
            l.colony = colonies_;
            l.ax = ax;
            l.ay = ay;
            has_[c.y][c.x] = (int8_t)c.type;
        }
        colonies_ = (colonies_ + 1) % FREE_COLONY;
        for (auto& l : fallLook_) l = lookOf();
        break;
    }
    case GameEvent::HardDrop: {
        // a few granules pushed out from under the piece, at the contact
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
        const vec3 a = layout::cellPos((float)minX, (float)low), b = layout::cellPos((float)maxX, (float)low);
        for (int i = 0; i < 8; i++) {
            const float s = i % 2 ? 1.f : -1.f;
            Speck k;
            k.p = vec2{(s < 0 ? a.x - 0.45f : b.x + 0.45f) + rng_.range(-0.1f, 0.1f), a.y - 0.42f + rng_.range(0.f, 0.2f)};
            k.v = vec2{s * rng_.range(0.4f, 1.1f), rng_.range(0.05f, 0.4f)};
            k.t = 0.f;
            k.life = rng_.range(0.6f, 1.1f);
            k.r = rng_.range(0.04f, 0.07f);
            k.type = std::clamp(gp.type, 0, 6);
            specks_.push_back(k);
        }
        break;
    }
    case GameEvent::Clear: {
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= Game::HIDDEN && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        // the cells burst at once: each scatters into a spray of granules of its stain, flung out and fading
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int type = std::clamp(c.type, 0, 6);
            for (int i = 0; i < 9; i++) {
                const float an = rng_.range(0.f, TAU), r0 = rng_.range(0.f, 0.35f), sp = rng_.range(0.8f, 2.2f);
                Speck k;
                k.p = vec2{p.x + std::cos(an) * r0, p.y + std::sin(an) * r0};
                k.v = vec2{std::cos(an) * sp, std::sin(an) * sp};
                k.t = 0.f;
                k.life = rng_.range(0.6f, 1.1f);
                k.r = rng_.range(0.05f, 0.11f);
                k.type = type;
                specks_.push_back(k);
            }
        }
        if (ev.count >= 4) swell_ = std::max(swell_, 0.5f);
        // the looks of the rows above step down; a colony split by the cleared rows becomes two
        bool cleared[Game::H] = {};
        for (int y : rows) cleared[y] = true;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) cleared[c.y] = true;
        CellLook nl[Game::H][Game::W];
        int8_t nh[Game::H][Game::W];
        std::fill(&nh[0][0], &nh[0][0] + Game::H * Game::W, (int8_t)-1);
        std::map<std::pair<int, int>, int> split;
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (cleared[y]) continue;
            const int shift = dst - y;
            for (int x = 0; x < Game::W; x++) {
                nl[dst][x] = look_[y][x];
                nh[dst][x] = has_[y][x];
                nl[dst][x].ay += shift;   // (its noise moves down with it)
                int& col = nl[dst][x].colony;
                if (shift > 0 && nh[dst][x] >= 0 && col < FREE_COLONY) {
                    const auto key = std::make_pair(col, shift);
                    auto it = split.find(key);
                    if (it == split.end()) {
                        it = split.emplace(key, colonies_).first;
                        colonies_ = (colonies_ + 1) % FREE_COLONY;
                    }
                    col = it->second;
                }
            }
            dst--;
        }
        for (int y = 0; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) { look_[y][x] = nl[y][x]; has_[y][x] = nh[y][x]; }
        break;
    }
    case GameEvent::TopOut: {
        // the tissue dies from the top: plasmolysis, the stain drained
        overT_ = 0.f;
        dying_.clear();
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            CellLook l = has_[c.y][c.x] >= 0 ? look_[c.y][c.x] : lookOf();
            dying_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), l, 0.25f + 0.07f * (c.y - top) + 0.1f * rng_.uniform()});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void MicroscopeUniverse::addCell(float x, float y, float half, int type, const CellLook& l, int kind, float prog, float alpha,
                                 float flash, int mask, float gx, float gy) {
    if (alpha <= 0.003f) return;
    const vec2 off = colonyOffset(l.colony < 0 ? 0 : l.colony);
    cells_.push_back({{x, y, half, (float)kind}, {(float)type, l.seed, prog, alpha}, {l.nx, l.ny, flash, (float)mask}, {gx + off.x, gy + off.y, 0.f, 0.f}});
}

void MicroscopeUniverse::buildBoard(const Game& g) {
    // the falling colony: its cells, their anchor (their lowest x and y, as once locked) and joins
    int pc[4][2] = {};
    int ax = 0, ay = 0, fmask[4] = {};
    const bool piece = g.hasPiece() && !g.over();
    if (piece) {
        g.pieceCells(g.piece(), pc);
        ax = std::min({pc[0][0], pc[1][0], pc[2][0], pc[3][0]});
        ay = std::min({pc[0][1], pc[1][1], pc[2][1], pc[3][1]});
        for (int i = 0; i < 4; i++)
            fmask[i] = maskOf(pc[i][0], pc[i][1], [&](int x, int y) {
                for (auto& c : pc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            });
    }
    CellLook fall[4];
    for (int i = 0; i < 4; i++) { fall[i] = fallLook_[i]; fall[i].colony = colonies_; }
    // the ghost: the colony's membrane alone, where the piece will land
    if (piece) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (int i = 0; i < 4; i++) {
                if (gc[i][1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)gc[i][0], (float)gc[i][1]);
                addCell(w.x, w.y, 0.5f, p.type, fall[i], 1, 0.f, 1.f, 0.f, fmask[i], (float)(pc[i][0] - ax), (float)(ay - pc[i][1]));
            }
        }
    }
    // the stack (gone into the dying cells at a game over)
    if (overT_ < 0.f)
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Cell& c = g.cell(x, y);
                if (c.type < 0) continue;
                const int col = look_[y][x].colony;
                const int m = maskOf(x, y, [&](int nx, int ny) {
                    return nx >= 0 && nx < Game::W && ny >= 0 && ny < Game::H && has_[ny][nx] >= 0 && g.cell(nx, ny).type >= 0 &&
                           look_[ny][nx].colony == col;
                });
                const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
                const CellLook& l = look_[y][x];
                addCell(p.x, p.y, 0.5f, c.type, l, 0, 0.f, 1.f, std::pow(saturate(c.flash), 10.f), m, (float)(x - l.ax), (float)(l.ay - y));
            }
    // the falling piece: exactly a locked one at its place
    if (piece) {
        const Piece& p = g.piece();
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            const vec3 w = layout::cellPos(pc[i][0] + dx, pc[i][1] + dy); // (whole above the well too: it spawns there)
            addCell(w.x, w.y, 0.5f, p.type, fall[i], 0, 0.f, 1.f, 0.f, fmask[i], (float)(pc[i][0] - ax), (float)(ay - pc[i][1]));
        }
    }
    // the previews
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
            const float ox = cells[i][0] - (minx + maxx) * 0.5f, oy = cells[i][1] - (miny + maxy) * 0.5f;
            CellLook l;
            const float an = (float)(i * 2.1 + type);
            l.nx = std::cos(an) * 0.06f;
            l.ny = std::sin(an) * 0.06f;
            l.seed = std::fmod(type * 0.137f + i * 0.291f, 1.f);
            l.colony = 977 + type;
            const int m = maskOf(cells[i][0], cells[i][1], [&](int x, int y) {
                for (auto& c : cells)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            });
            addCell(center.x + ox * sc, center.y - oy * sc, 0.5f * sc, type, l, 0, 0.f, alpha, 0.f, m, (float)cells[i][0], (float)-cells[i][1]);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.45f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void MicroscopeUniverse::drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count) {
    if (!count) return;
    glUseProgram(prog);
    setMat(prog, "uVP", vp_);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
}

void MicroscopeUniverse::setLight(GLuint prog) {
    auto arr = [&](const char* name, const vec3* v, int n) {
        float f[21];
        for (int i = 0; i < n; i++) { f[i * 3] = v[i].x; f[i * 3 + 1] = v[i].y; f[i * 3 + 2] = v[i].z; }
        glUniform3fv(ugl::U(prog, name), n, f);
    };
    arr("uFieldA", own_.field, 2);
    arr("uFieldB", drop_.field, 2);
    arr("uPieceA", own_.piece, 7);
    arr("uPieceB", drop_.piece, 7);
    arr("uOrgA", own_.org, 3);
    arr("uOrgB", drop_.org, 3);
    set1f(prog, "uDropK", dropK_);
    set2f(prog, "uDarkAB", own_.dark, drop_.dark);
    set2f(prog, "uPhaseAB", own_.phase, drop_.phase);
}

// =====================================================================================================
// The HUD
// =====================================================================================================

void MicroscopeUniverse::drawPanels(const std::vector<HudRect>& panels) {
    if (panels.empty() || bare_) return;
    const VariantDef& V = VARIANTS[variant_];
    const vec3 acc = hex(V.accent), back = darkNow() > 0.5f ? vec3(0.02f) : ugl::toSrgb(own_.field[0]);
    std::vector<float> v;
    for (const HudRect& r : panels) {
        const vec3 c = r.custom ? r.color : r.accent ? acc : back;
        const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
        const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
        for (auto& q : qd) v.insert(v.end(), q, q + 6);
    }
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6));
}

// The unified HUD's inks: a deep slate of the field's own tint, a pale one, the stain's accent.
HudInk MicroscopeUniverse::hudInk(const HudText& h) {
    (void)h;
    const VariantDef& V = VARIANTS[variant_];
    HudInk k;
    k.deep = hex(V.deep);
    k.pale = hex(V.pale);
    k.accent = hex(V.accent);
    return k;
}

// The HUD's own lettering (REFALL_HUD=universe): Lato in spaced capitals for the names, the labels and the numbers, EB
// Garamond Italic for the song and the notices, like a slide's label; the deep ink on a lit field, the pale one in
// dark-field, on a faint halo of the field.
void MicroscopeUniverse::hudLine(const HudText& h, text::Line& out) {
    const float s = height_ / 1080.f;
    const VariantDef& V = VARIANTS[variant_];
    const bool dark = darkNow() > 0.5f;
    const vec3 ink = hex(dark ? V.pale : V.deep), accent = hex(V.accent);
    text::Font* sans = text::font(FONT_SANS);
    text::Font* it = text::font(FONT_IT);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(sans, text::upper(h.text), base * 0.66f, base * 0.3f, ink, saturate(0.3f + h.alpha * 1.5f) * 0.75f, base * 0.08f); break;
    case HUD_SECTION: put(sans, text::upper(h.text), base * 0.72f, base * 0.34f, accent, 0.3f + h.alpha * 1.7f); break;
    case HUD_TITLE: put(it, h.text, base * 1.12f, base * 0.01f, ink, h.alpha * 1.3f, -base * 0.12f); break;
    case HUD_INFO: put(sans, text::upper(h.text), base * 0.6f, base * 0.24f, ink, saturate(alpha * 1.2f) * 0.72f); break;
    case HUD_LABEL: put(sans, text::upper(h.text), base * 0.62f, base * 0.4f, ink, saturate(alpha * 1.2f) * 0.62f); break;
    case HUD_VALUE: put(sans, h.text, base * 1.0f, base * 0.06f, ink, alpha * 0.95f, -base * 0.04f); break;
    case HUD_NOTICE: put(it, h.text, base * 1.1f, base * 0.02f, accent, alpha); break;
    case HUD_KEY: put(sans, h.text, base * 0.85f, base * 0.1f, ink, alpha); break;
    case HUD_TOAST: put(sans, text::upper(h.text), base * 0.95f, base * 0.42f, ink, alpha, -base * 0.1f); break;
    case HUD_TOAST_SUB: put(it, h.text, base * 1.0f, base * 0.02f, accent, alpha * 0.9f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(sans, text::upper(h.text), base * 0.85f, base * 0.3f, ink, alpha);
        else put(it, h.text, base * 0.95f, base * 0.02f, h.accent ? accent : ink, alpha);
        break;
    default: put(sans, h.text, base * 0.75f, base * 0.06f, h.accent ? accent : ink, alpha); break;
    }
    out.look.weight = 0.006f;
    out.look.halo = ugl::toSrgb(dark ? vec3(0.f) : own_.field[0]);
    out.look.haloA = 0.4f;
    out.look.haloW = 0.08f;
    out.look.haloPow = 1.5f;
    out.alpha = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MicroscopeUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    if (overT_ < 0.f && !bare_) syncLooks(game); // the world alone: the cells' looks kept for the whole frame
    // a new variant eases in
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        auto ease = [&](Light& s, const Light& t) {
            for (int i = 0; i < 2; i++) s.field[i] = lerp(s.field[i], t.field[i], k);
            for (int i = 0; i < 7; i++) s.piece[i] = lerp(s.piece[i], t.piece[i], k);
            for (int i = 0; i < 3; i++) s.org[i] = lerp(s.org[i], t.org[i], k);
            s.nucGlow = lerp(s.nucGlow, t.nucGlow, k);
            s.dark = lerpf(s.dark, t.dark, k);
            s.phase = lerpf(s.phase, t.phase, k);
        };
        ease(own_, ownT_);
        ease(drop_, dropT_);
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) { overT_ = -1.f; dying_.clear(); }
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
    const double beat = music.hasSong ? music.beat : time_ * 2.0;
    // the cilia beat on the beat grid, every two beats (not in calm parts)
    const float half = (float)(beat * 0.5 - std::floor(beat * 0.5));
    const float pulse = std::exp(-half * 5.f) * (0.35f * wm + 0.65f * wp) * (1.f - 0.7f * slow_);
    kickEnv_ = std::max(kickEnv_ * std::exp(-dt * 5.f), std::max(kickIn_, music.kick) * wp);
    kickIn_ = 0.f;
    // the drop (a DROP section only): the second illumination eased in, one swell of light, a refocus
    const bool inDrop = music.hasSong && music.inDrop && overT_ < 0.f;
    if (inDrop && !inDrop_) {
        swell_ = std::max(swell_, 0.7f);
        focusT_ = rng_.range(-0.3f, 0.3f);
    }
    inDrop_ = inDrop;
    dropK_ = inDrop ? std::min(1.f, dropK_ + dt / 1.6f) : std::max(0.f, dropK_ - dt / 2.8f);
    const float dk = smoothstepf(0.f, 1.f, dropK_);
    swell_ = approach(swell_, 0.f, 0.9f, dt);
    focus_ += (focusT_ - focus_) * (1.f - std::exp(-dt * 1.3f));

    // ---- The life (world time: slow motion stretches it).
    const float act = (0.5f + 0.3f * wm + 0.4f * wp) * (1.f + 0.9f * dk);
    for (Org& o : orgs_) {
        o.goalT -= wdt;
        vec2 g;
        const float zd = zoneDist(o.p, &g);
        const float margin = 0.25f + o.size * 0.42f;
        const vec2 tg{o.goal.x - o.p.x, o.goal.y - o.p.y};
        const float gd = std::sqrt(tg.x * tg.x + tg.y * tg.y);
        if (gd < 0.6f + 0.2f * o.size || o.goalT <= 0.f) {
            o.goal = pickGoal(o.side, o.size);
            o.goalT = rng_.range(14.f, 34.f);
        }
        const vec2 gn = gd > 1e-4f ? vec2{tg.x / gd, tg.y / gd} : vec2{0.f, 0.f};
        // a slow meander, toward its goal, apart from the others, clear of the well and the HUD
        const float turn = KIND_TURN[o.kind] * (std::sin(o.turnPhase + worldTime_ * 0.23f) * 0.7f + std::sin(o.turnPhase * 1.7f + worldTime_ * 0.11f) * 0.5f);
        o.ang += turn * wdt * act;
        vec2 sep{0.f, 0.f};
        for (const Org& b : orgs_) {
            if (&b == &o) continue;
            const float dx = o.p.x - b.p.x, dy = o.p.y - b.p.y, R = (o.size + b.size) * 0.55f;
            const float dd = std::sqrt(dx * dx + dy * dy);
            if (dd < R && dd > 1e-4f) { sep.x += dx / dd * (1.f - dd / R); sep.y += dy / dd * (1.f - dd / R); }
        }
        const float wz = saturate((margin - zd) / margin), wg = smoothstepf(0.5f, 4.f, gd);
        const vec2 want{gn.x * wg * 0.7f + sep.x * 2.f + g.x * wz * 2.5f, gn.y * wg * 0.7f + sep.y * 2.f + g.y * wz * 2.5f};
        const float wl = std::sqrt(want.x * want.x + want.y * want.y);
        const bool turns = o.kind != K_CENTRIC && o.kind != K_VOLVOX;
        const float hd = o.dir < 0.f ? o.ang + PI : o.ang;
        // a paramecium bumping into something backs up a moment and turns away (its avoiding reaction)
        if (o.kind == K_PARA && o.back <= 0.f && wz > 0.55f && std::cos(hd) * g.x + std::sin(hd) * g.y < -0.2f) o.back = 1.1f;
        if (o.back > 0.f) {
            o.back -= wdt;
            o.ang += (o.seed < 0.5f ? 1.f : -1.f) * 0.9f * wdt;
        } else if (wl > 0.05f && turns) {
            const float target = std::atan2(want.y, want.x);
            const float diff = wrapAngle(target - hd);
            if (o.kind == K_PENNATE && std::fabs(diff) > 0.7f * PI && wz > 0.3f) o.dir = -o.dir;   // a diatom just backs up
            else o.ang += (diff > 0 ? 1.f : -1.f) * std::min(std::fabs(diff), (0.12f + 0.3f * std::min(wl, 1.f) + 1.2f * wz) * wdt);
        }
        float sp = o.speed * act * (1.f + 0.5f * pulse * (o.kind == K_PARA));
        if (o.kind == K_PARA) sp *= 1.f + 0.9f * kickEnv_;
        if (o.back > 0.f) sp *= -0.7f;
        const float dirSign = o.kind == K_PENNATE ? o.dir : 1.f;
        if (turns) {
            o.p.x += std::cos(o.ang) * sp * dirSign * wdt;
            o.p.y += std::sin(o.ang) * sp * dirSign * wdt;
        } else {
            // (a round one rolls along toward where it wants)
            const float a = wl > 0.05f ? std::atan2(want.y, want.x) : o.ang;
            o.ang += wrapAngle(a - o.ang) * (1.f - std::exp(-wdt * 0.5f));
            o.p.x += std::cos(o.ang) * sp * wdt;
            o.p.y += std::sin(o.ang) * sp * wdt;
        }
        o.p.x += sep.x * 0.3f * wdt;
        o.p.y += sep.y * 0.3f * wdt;
        // inside a forbidden place: eased out
        if (zd < 0.f) {
            o.p.x += g.x * 0.8f * wdt;
            o.p.y += g.y * 0.8f * wdt;
        }
        o.cilia = 0.15f + 0.3f * wm + 0.9f * pulse + 0.6f * kickEnv_ + 0.5f * dk;
        o.spin += wdt * (o.kind == K_CENTRIC ? 0.06f : 0.f);
    }
    for (Speck& k : specks_) {
        k.t += dt;
        if (k.t < 0.f) continue;
        k.p.x += k.v.x * dt;
        k.p.y += k.v.y * dt;
        k.v.x *= std::exp(-dt * 1.5f);
        k.v.y *= std::exp(-dt * 1.5f);
    }
    specks_.erase(std::remove_if(specks_.begin(), specks_.end(), [](const Speck& k) { return k.t > k.life; }), specks_.end());

    // ---- Instances.
    const float bridgeK = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    cells_.clear();
    std::vector<Inst> orgInst;
    const float spread = lerpf(1.1f, 0.6f, wp);
    const float calmBlur = 0.1f * (1.f - wm);
    for (const Org& o : orgs_) {
        vec2 g;
        const float zd = zoneDist(o.p, &g);
        float a = lerpf(0.45f, 1.f, smoothstepf(-1.f, 0.5f, zd)) * (1.f - bridgeK);
        if (a <= 0.003f) continue;
        const float blur = saturate(std::fabs(o.depth * spread - focus_) * 0.55f - 0.12f + calmBlur);
        const float ang = o.kind == K_CENTRIC || o.kind == K_VOLVOX ? 0.f : o.ang;
        const float anim = worldTime_ * (o.kind == K_AMOEBA ? 1.f : 0.8f + 0.5f * act) + o.seed * 100.f;
        orgInst.push_back({{o.p.x, o.p.y, o.size, ang}, {(float)o.kind, o.seed, anim, a}, {blur, o.cilia, 0.f, 0.f}, {o.spin, 0.f, 0.f, 0.f}});
    }
    if (!bare_) { // the world alone: no tissue, nothing dying
        if (overT_ >= 0.f)
            for (const Dying& d : dying_) {
                const float t = saturate((overT_ - d.delay) / DIE_T);
                if (t < 1.f) addCell(d.x, d.y, 0.5f, d.type, d.l, 3, t, 1.f, 0.f, 0, d.x, d.y);
            }
        buildBoard(game);
    }
    std::vector<Inst> speckInst;
    if (!bare_)
        for (const Speck& k : specks_) {
            if (k.t < 0.f) continue;
            const float s = k.t / k.life;
            const float a = smoothstepf(0.f, 0.15f, s) * (1.f - smoothstepf(0.45f, 1.f, s)) * 0.8f * (1.f - bridgeK);
            const vec3 c = lerp(own_.piece[k.type], drop_.piece[k.type], dk);
            speckInst.push_back({{k.p.x, k.p.y, k.r, 0.f}, {0.f, 0.f, 0.f, a}, {c.x, c.y, c.z, 0.f}, {0.f, 0.f, 0.f, 0.f}});
        }

    // ---- Draw.
    const float dark = darkNow();
    const float aaW = 2.f * viewH_.y / (float)height_;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progField_);
    setLight(progField_);
    set4f(progField_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progField_, "uTime", worldTime_);
    set1f(progField_, "uIllum", 0.93f + 0.04f * wm + 0.03f * wp);
    set1f(progField_, "uSwell", swell_ * (1.f - 0.6f * slow_));
    set1f(progField_, "uSeed", seedF_);
    set1f(progField_, "uAA", aaW);
    set1f(progField_, "uWell", bare_ ? 0.f : 1.f - bridgeK);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // the scene's alpha is the stack's (for the bridge)
    if (!orgInst.empty()) {
        glUseProgram(progOrg_);
        setLight(progOrg_);
        set1f(progOrg_, "uGlow", (0.85f + 0.12f * wm + 0.15f * wp + 0.2f * swell_));
        drawInst(progOrg_, orgVao_, orgVbo_, orgInst.data(), orgInst.size() * sizeof(Inst), orgInst.size());
    }
    if (!speckInst.empty()) {
        glUseProgram(progSpeck_);
        setLight(progSpeck_);
        drawInst(progSpeck_, speckVao_, speckVbo_, speckInst.data(), speckInst.size() * sizeof(Inst), speckInst.size());
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(progPiece_);
    setLight(progPiece_);
    set1f(progPiece_, "uTime", worldTime_);
    set1f(progPiece_, "uLife", 0.6f + 0.4f * wm);
    set1f(progPiece_, "uGlow", 0.95f + 0.05f * wm + 0.08f * wp + 0.15f * swell_);
    set3f(progPiece_, "uNucGlow", lerp(own_.nucGlow, drop_.nucGlow, dk));
    drawInst(progPiece_, pieceVao_, pieceVbo_, cells_.data(), cells_.size() * sizeof(Inst), cells_.size());

    // ---- The soft blur (the dark-field glow, the slide out of focus).
    glDisable(GL_BLEND);
    blurA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    ugl::drawFullscreen();
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    for (int i = 0; i < 2; i++) {
        const float st = 1.f + i;
        blurB_.bind();
        glBindTexture(GL_TEXTURE_2D, blurA_.tex);
        set2f(progBlur_, "uDir", st / blurA_.w, 0.f);
        ugl::drawFullscreen();
        blurA_.bind();
        glBindTexture(GL_TEXTURE_2D, blurB_.tex);
        set2f(progBlur_, "uDir", 0.f, st / blurA_.h);
        ugl::drawFullscreen();
    }

    // ---- The frame: the lamp warming up and the slide coming into focus around a song (and back).
    const float lamp = std::min(smoothstepf(0.f, 0.7f, f.setup), 1.f - smoothstepf(0.35f, 1.f, f.teardown));
    const float defocus = std::max(1.f - smoothstepf(0.25f, 0.95f, f.setup), smoothstepf(0.f, 0.65f, f.teardown));
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, blurA_.tex);
    set1i(progFinal_, "uBlur", 1);
    glActiveTexture(GL_TEXTURE0);
    set1f(progFinal_, "uGlowK", dark * (0.25f + 0.15f * wp + 0.4f * swell_));
    // (always a hair soft, like a micrograph)
    set1f(progFinal_, "uDefocus", std::max(defocus, 0.1f) * (1.f - bridgeK));
    set1f(progFinal_, "uLamp", lerpf(lamp, 1.f, bridgeK));
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", bridgeK);
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawPanels(f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_microscope() {
    static const UniverseEntry e{
        "MICROSCOPE", "a microscope's field of view in a soft micrograph look: pieces of living cells, a few organisms wandering by",
        [] { return std::unique_ptr<Universe>(new MicroscopeUniverse()); },
        // A quiet, precise world: fits most songs, a little more the calm and airy ones.
        [](const SongLook& l) { return 0.8f + 0.15f * l.airWeight + 0.15f * (1.f - l.density); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
