// SPECIMEN: plump lumps of living tissue on a dark, damp bench, and on each side a loupe looking into one of them.
//
// One world: a dark bench under one soft lamp, its surface faintly mottled; the well a darker backing, its walls fine
// pale lines. The pieces are lumps of tissue: each block a plump, glossy rounded cube in a saturated dye, lit from the
// top left, its fine cells showing faintly through the skin. The seven types are seven dyes of the variant. The
// falling piece is exactly a locked one, the ghost a faint outline, the previews the same lumps.
//
// The loupes: one on each side, in the band beyond the HUD's column, looking into one block of its half of the stack
// (rather one with life in it, on the surface), joined to it by a fine line, four corner brackets around the block.
// In it, the block's tissue magnified in bright-field: cells in the block's dye, nuclei, plastids streaming slowly
// along the walls, the slide drifting; about one block in five has a microorganism living in it, in a clear pool in
// the middle, in full detail (paramecium, amoeba, pennate and centric diatoms, rotifer, volvox). A loupe stays about
// twelve seconds on a block, then fades and finds another, gliding calmly to its block's height.
//
// Each cleared row bursts (its lumps round off and fade in a third of a second) and frees the organisms living in it:
// tiny creatures glowing in their cell's dye, they crawl out of the well and off the screen by the nearer side, and the
// loupe of that side follows one out (the creature magnified, crawling over the bare bench sliding by under it).
//
// The song: calm parts are a dimmer bench and slow life; mid wakes the cilia on the beat grid (every two beats); a
// peak brightens the lamp, the kicks flicking the cilia. The drop washes in the variant's second set of dyes, soaking
// unevenly into the tissue, with one swell of light; it is rinsed out slowly after the peak. Slow motion drains the
// dyes to silver, time stretched. Game over kills the tissue from the top: the lumps shrink, drain and fade.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/specimen/specimen.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/specimen/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Lato (labels, numbers) and EB Garamond Italic (the song, like a species' name), SIL Open Font License, see
// OFL-Lato.txt and OFL-EBGaramond.txt.
const unsigned char FONT_SANS[] = {
#include "universes/specimen/Lato-Regular.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/specimen/EBGaramond12-Italic.ttf.inc"
};

namespace shaders = specimen_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// A stain (sRGB): the field (center, edge), the seven piece types (I O T S Z J L), the organisms (body, rim,
// organelles).
struct StainDef {
    unsigned field[2];
    unsigned piece[7];
    unsigned org[3];
};
// A variant: its bench (under the lamp, at the edges), its own dyes and the ones the drop washes in, the HUD's accent.
struct VariantDef {
    const char* name;
    unsigned back[2];
    StainDef own, drop;
    unsigned accent;
};
const VariantDef VARIANTS[] = {
    // a deep teal bench; the drop: greener, warmer dyes
    {"TEAL", {0x0f2f3a, 0x03090c},
     {{0xf1efe6, 0xdcdcd2}, {0x2fb4d0, 0xf2c23a, 0xb44ad0, 0x5cc848, 0xe8404e, 0x4a62e0, 0xf27a30}, {0x3c6ea8, 0x1f3d70, 0x14284e}},
     {{0xf2eef0, 0xded8dc}, {0x3ad6b0, 0xf29a3a, 0xe24aa2, 0xa8d23e, 0xf2503a, 0x7a52e2, 0xf2c83e}, {0xb04a7a, 0x6a2a5a, 0x3a1a4a}},
     0xf0c060},
    // an indigo bench, violet stains under the loupes; the drop: hot pinks and limes
    {"INDIGO", {0x1c1a44, 0x05040f},
     {{0xf0eef4, 0xdcdae4}, {0x3ec6e0, 0xf5d040, 0xd04ad8, 0x52d07a, 0xf0466a, 0x5a6cf2, 0xf5893a}, {0x5a3aa8, 0x3a2470, 0x24164e}},
     {{0xf4eef2, 0xe2d8de}, {0x5ae0d0, 0xf2b03a, 0xf05ac0, 0xb4e040, 0xff5a48, 0x8a62f2, 0xf5d050}, {0xc04a8a, 0x7a2a5a, 0x4a1a3a}},
     0x9aa8ff},
    // a moss-green bench, malachite under the loupes; the drop: autumn dyes
    {"MOSS", {0x15321e, 0x040b06},
     {{0xeff2ea, 0xd8ded2}, {0x36c2c8, 0xf2cc3a, 0xb850d8, 0x8ad040, 0xec4a42, 0x4a72e0, 0xf28a2e}, {0x3a8a5a, 0x1f4a3a, 0x8a2a3a}},
     {{0xf2efe6, 0xdedad0}, {0x40e0a0, 0xf2a832, 0xe256b0, 0xc8e03a, 0xf25a5a, 0x6a5ae8, 0xf2c43e}, {0xa8582a, 0x6a3418, 0x3a1a0a}},
     0xa8e070},
    // an oxblood bench, eosin under the loupes; the drop: brighter, lighter dyes
    {"OXBLOOD", {0x3a1620, 0x0c0406},
     {{0xf4eeea, 0xe2d8d2}, {0x3ab8e0, 0xf5c84a, 0xc458e0, 0x5ccc6a, 0xff6a4a, 0x5a70f0, 0xf59a3a}, {0xb04a7a, 0x6a2a5a, 0x3a1a4a}},
     {{0xf4eef0, 0xe4d8dc}, {0x5ad8f0, 0xf5e05a, 0xe878f0, 0x8ae07a, 0xff8a5a, 0x7a8af8, 0xf5b85a}, {0x6a4ab0, 0x3a2a6a, 0x24164e}},
     0xf2a070},
    // a graphite bench, methylene blue under the loupes; the drop: a cold set of dyes
    {"GRAPHITE", {0x24262c, 0x07080a},
     {{0xf2f2ee, 0xdcdcd6}, {0x2fc0e8, 0xf8d23a, 0xc04ae8, 0x46d46a, 0xf43e5a, 0x4a60f0, 0xf8842a}, {0x2f6fc0, 0x1f3d70, 0x14284e}},
     {{0xeef2f4, 0xd8dee2}, {0x48e0e8, 0xd8f040, 0x9a5af8, 0x40e0a8, 0xff4a8a, 0x5a88ff, 0xffb040}, {0x2a8a8a, 0x1a4a5a, 0x0e2a3a}},
     0xe8e8e0},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));
constexpr float LENS_HOLD = 12.f;    // a loupe looks into one block this long

constexpr float LYSE_T = 0.32f;      // a cleared cell is gone in this long (real time)
constexpr float DIE_T = 2.2f;        // game over: a cell's plasmolysis
constexpr float LIFE_SHARE = 0.22f;   // the share of the cells with an organism living inside
constexpr float DWELL_SIZE = 0.82f;   // an organism's size inside its cell (one bold shape, in a clear pool)

// The kinds of life: paramecium, amoeba, pennate diatom, centric diatom, rotifer, volvox.
enum { K_PARA, K_AMOEBA, K_PENNATE, K_CENTRIC, K_ROTIFER, K_VOLVOX, K_COUNT };

float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }
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

bool SpecimenUniverse::init(int width, int height) {
    using shaders::withCommon;
    for (auto& l : fallLook_) l = lookOf(0, 0);
    progField_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::FIELD_FS).c_str(), "specimen field");
    progOrg_ = ugl::program(shaders::ORG_VS, withCommon(shaders::ORG_FS).c_str(), "specimen life");
    progPiece_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::PIECE_FS).c_str(), "specimen cells");
    progSpeck_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::SPECK_FS).c_str(), "specimen specks");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "specimen down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "specimen blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "specimen final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "specimen shape");
    progLens_ = ugl::program(shaders::LENS_VS, withCommon(shaders::LENS_FS).c_str(), "specimen loupes");
    progLine_ = ugl::program(shaders::LINE_VS, shaders::LINE_FS, "specimen lines");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    instVao(quadVbo_, pieceVao_, pieceVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, orgVao_, orgVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, speckVao_, speckVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, lensVao_, lensVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, lineVao_, lineVbo_, 4, sizeof(Inst));
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

void SpecimenUniverse::resize(int width, int height) {
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
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
}

SpecimenUniverse::~SpecimenUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progField_, progOrg_, progPiece_, progSpeck_, progDown_, progBlur_, progFinal_, progShape_,
                      progLens_, progLine_})
        glDeleteProgram(p);
    for (GLuint v : {pieceVao_, orgVao_, speckVao_, shapeVao_, lensVao_, lineVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, pieceVbo_, orgVbo_, speckVbo_, shapeVbo_, lensVbo_, lineVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void SpecimenUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        // by the song's color: its hue picks the bench, a dark minor song rather the deeper ones
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (t.hasSong && t.minor && t.brightness < 0.5f && v == 4) v = 1;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
    }
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    auto fill = [&](Stain& s, const StainDef& d) {
        for (int i = 0; i < 2; i++) s.field[i] = lin(d.field[i]);
        for (int i = 0; i < 7; i++) s.piece[i] = lin(d.piece[i]);
        for (int i = 0; i < 3; i++) s.org[i] = lin(d.org[i]);
    };
    fill(baseT_, V.own);
    fill(dropT_, V.drop);
    for (int i = 0; i < 2; i++) backT_[i] = lin(V.back[i]);
    const bool instant = !colorsSet_ || seconds <= 0.1f;
    if (instant) {
        base_ = baseT_;
        dropS_ = dropT_;
        back_[0] = backT_[0];
        back_[1] = backT_[1];
        colorsSet_ = true;
    }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    rng_ = Rng(seed_ ^ 0x3C1C0Bull);
    if (instant) crawlers_.clear();
    stain_ = 0.f;
    dropMode_ = false;
}

void SpecimenUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    (void)energy;
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
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

// The drop: the variant's second set of dyes soaks into the tissue, with one swell of light.
void SpecimenUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
}

void SpecimenUniverse::refresh(float strength) {
    (void)strength;
}

vec2 SpecimenUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints SpecimenUniverse::hints() const {
    TransitionHints h;
    h.light = false;
    h.background = lerp(back_[0], back_[1], 0.5f);
    vec3 ink(0.f);
    for (int i = 0; i < 7; i++) ink = ink + base_.piece[i] * (1.f / 7.f);
    h.ink = ink;
    h.accent = lin(VARIANTS[variant_].accent);
    h.styles = TS_LUMA | TS_INK | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

SpecimenUniverse::CellLook SpecimenUniverse::lookOf(int x, int y) {
    (void)x;
    (void)y;
    CellLook l;
    Rng& r = rng_;
    const float a = r.range(0.f, TAU), d = r.range(0.05f, 0.15f);
    l.nx = std::cos(a) * d;
    l.ny = std::sin(a) * d;
    l.seed = r.uniform();
    if (r.chance(LIFE_SHARE)) l.life = pickKind();
    return l;
}

// Keeps the looks in step with the board (a new board, a stack inherited from another universe).
void SpecimenUniverse::syncLooks(const Game& g) {
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const bool on = g.cell(x, y).type >= 0;
            if (on && !has_[y][x]) look_[y][x] = lookOf(x, y);
            has_[y][x] = on;
        }
}

int SpecimenUniverse::pickKind() {
    const float w[K_COUNT] = {0.27f, 0.15f, 0.2f, 0.14f, 0.16f, 0.08f};
    int k = rng_.weighted(w);
    // not the same kind twice in a row
    if (k == lastKind_) k = (k + 1 + rng_.irange(0, 3)) % K_COUNT;
    lastKind_ = k;
    return k;
}

// A cell's organism as it is now: small, turning slowly about the middle of the cell (the same on every frame for a
// given look and time, so that it leaves the cell exactly as it was in it).
SpecimenUniverse::Org SpecimenUniverse::dweller(const CellLook& l, float x, float y, int type) const {
    Org o;
    const int kind = std::clamp(l.life, 0, (int)K_COUNT - 1);
    const float t = worldTime_ + l.seed * 50.f, s = l.seed * 37.f;
    o.kind = kind;
    o.seed = l.seed;
    o.size = DWELL_SIZE;
    o.p = vec2{x + 0.07f * std::sin(t * 0.31f + s), y + 0.07f * std::cos(t * 0.27f + s * 1.3f)};
    o.ang = s + t * (l.seed < 0.5f ? 0.23f : -0.23f) + 0.6f * std::sin(t * 0.17f + s);
    o.type = std::clamp(type, 0, 6);
    o.depth = 0.f;
    o.turnPhase = s;
    o.tint = 0.1f;   // its own colors, not the cell's dye (it would vanish in it)
    o.spin = s;
    return o;
}

void SpecimenUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        for (size_t i = 0; i < ev.cells.size() && i < 4; i++) {
            const auto& c = ev.cells[i];
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            look_[c.y][c.x] = fallLook_[i];
            has_[c.y][c.x] = true;
        }
        for (auto& l : fallLook_) l = lookOf(0, 0);
        break;
    }
    case GameEvent::HardDrop: {
        // fine debris pushed out from under the piece, at the contact
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
        for (int i = 0; i < 12; i++) {
            const float s = i % 2 ? 1.f : -1.f;
            Speck k;
            k.p = vec2{(s < 0 ? a.x - 0.45f : b.x + 0.45f) + rng_.range(-0.1f, 0.1f), a.y - 0.42f + rng_.range(0.f, 0.2f)};
            k.v = vec2{s * rng_.range(0.5f, 1.4f), rng_.range(0.05f, 0.5f)};
            k.t = 0.f;
            k.life = rng_.range(0.45f, 0.8f);
            k.r = rng_.range(0.05f, 0.1f);
            specks_.push_back(k);
        }
        break;
    }
    case GameEvent::Clear: {
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= Game::HIDDEN && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        std::sort(rows.begin(), rows.end());
        const bool quadClear = ev.count >= 4;
        // the cells burst
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const CellLook l = has_[c.y][c.x] ? look_[c.y][c.x] : lookOf(c.x, c.y);
            lyses_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), l, 0.f});
        }
        // the loupes: one on a cleared row has lost its block, the others follow theirs down
        for (Lens& L : lenses_) {
            if (L.x < 0 || L.lost) continue;
            int below = 0;
            for (int y : rows) below += y > L.y;
            if (std::find(rows.begin(), rows.end(), L.y) != rows.end()) { L.lost = true; L.leaving = true; }
            else L.y += below;
        }
        // the organisms living in the cleared cells crawl out of the screen (a row with none frees nothing), the loupe
        // of their side following one
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            if (!has_[c.y][c.x] || look_[c.y][c.x].life < 0) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            Crawler cr;
            cr.o = dweller(look_[c.y][c.x], p.x, p.y, c.type);
            const float sx = p.x < 0.f ? -1.f : 1.f;
            cr.dest = vec2{sx * (viewH_.x + 3.f), std::clamp(p.y + rng_.range(-1.f, 3.f), viewC_.y - viewH_.y + 2.f, viewC_.y + viewH_.y - 2.f)};
            cr.o.ang = std::atan2(cr.dest.y - p.y, cr.dest.x - p.x);
            cr.speed = (cr.o.kind == K_AMOEBA ? 2.2f : cr.o.kind == K_PARA ? 3.4f : 2.8f) * rng_.range(0.85f, 1.15f);
            cr.id = ++crawlerIds_;
            crawlers_.push_back(cr);
            Lens& L = lenses_[sx < 0.f ? 0 : 1];
            if (L.follow < 0 || !crawler(L.follow)) {
                L.follow = cr.id;
                L.x = -1;
                L.leaving = L.lost = false;
                L.a = std::min(L.a, 0.3f);
                L.at = vec2{p.x, p.y};
            }
        }
        if (quadClear) swell_ = std::max(swell_, 0.55f);
        // the looks of the rows above step down
        {
            bool cleared[Game::H] = {};
            for (int y : rows) cleared[y] = true;
            for (auto& c : ev.cells)
                if (c.y >= 0 && c.y < Game::H) cleared[c.y] = true;
            CellLook nl[Game::H][Game::W];
            bool nh[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                for (int x = 0; x < Game::W; x++) { nl[dst][x] = look_[y][x]; nh[dst][x] = has_[y][x]; }
                dst--;
            }
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) { look_[y][x] = nl[y][x]; has_[y][x] = nh[y][x]; }
        }
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
            const CellLook l = has_[c.y][c.x] ? look_[c.y][c.x] : lookOf(c.x, c.y);
            dying_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), l, 0.25f + 0.07f * (c.y - top) + 0.1f * hashf((uint64_t)(c.x * 31 + c.y))});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The loupes and the crawlers
// =====================================================================================================

// In the middle of the band beyond the HUD's column, at the height of its block (clear of the corners' text).
const SpecimenUniverse::Crawler* SpecimenUniverse::crawler(int id) const {
    for (const Crawler& c : crawlers_)
        if (c.id == id) return &c;
    return nullptr;
}

vec2 SpecimenUniverse::lensCenter(int side) const {
    const float sx = side == 0 ? -1.f : 1.f;
    return vec2{sx * (11.9f + viewH_.x) * 0.5f, lenses_[side].ly};
}

void SpecimenUniverse::updateLenses(const Game& g, float dt, float wdt) {
    lensR_ = std::clamp((viewH_.x - 11.9f) * 0.5f - 0.6f, 2.2f, 4.3f);
    const float ylo = -6.6f + lensR_, yhi = 8.8f - lensR_;
    const bool can = overT_ < 0.f && !g.over();
    for (int s = 0; s < 2; s++) {
        Lens& L = lenses_[s];
        if (L.follow >= 0) {
            // following an organism out: on it as it crawls, until it has left the screen
            const Crawler* c = crawler(L.follow);
            if (c && !L.leaving) {
                L.at = c->o.p;
                L.a = std::min(1.f, L.a + dt / 0.5f);
            } else {
                L.leaving = true;
                L.a = std::max(0.f, L.a - dt / 0.7f);
                if (L.a <= 0.f) { L.follow = -1; L.leaving = false; }
            }
            L.ly += (std::clamp(L.at.y, ylo, std::max(ylo, yhi)) - L.ly) * (1.f - std::exp(-dt * 1.2f));
            continue;
        }
        const bool valid = L.x >= 0 && !L.lost && L.y >= 0 && L.y < Game::H && g.cell(L.x, L.y).type >= 0;
        if (valid) {
            const vec3 p = layout::cellPos((float)L.x, L.y - g.rowOffset(L.y));
            L.at = vec2{p.x, p.y};
        }
        L.age += dt;
        L.pan += wdt * 0.05f;
        if (L.x >= 0 && (!valid || !can || L.age > LENS_HOLD)) L.leaving = true;
        if (L.leaving) {
            L.a = std::max(0.f, L.a - dt / 0.7f);
            if (L.a <= 0.f) { L.x = -1; L.leaving = L.lost = false; }
        } else if (L.x >= 0) L.a = std::min(1.f, L.a + dt / 1.2f);
        // a new block to look into: one of its half of the stack, rather one with life in it, on the surface
        if (L.x < 0 && can) {
            const Lens& o = lenses_[1 - s];
            float best = -1.f;
            for (int y = Game::HIDDEN; y < Game::H; y++)
                for (int x = s * Game::W / 2; x < (s + 1) * Game::W / 2; x++) {
                    if (g.cell(x, y).type < 0 || !has_[y][x] || (o.x == x && o.y == y)) continue;
                    const bool top = y == 0 || g.cell(x, y - 1).type < 0;
                    const float score = (look_[y][x].life >= 0 ? 2.f : 0.f) + (top ? 1.f : 0.f) + rng_.uniform() * 1.5f;
                    if (score > best) { best = score; L.x = x; L.y = y; }
                }
            if (L.x >= 0) {
                L.type = std::clamp((int)g.cell(L.x, L.y).type, 0, 6);
                L.l = look_[L.y][L.x];
                L.age = 0.f;
                L.a = 0.f;
                const vec3 p = layout::cellPos((float)L.x, L.y - g.rowOffset(L.y));
                L.at = vec2{p.x, p.y};
                if (!L.placed) { L.ly = std::clamp(L.at.y, ylo, std::max(ylo, yhi)); L.placed = true; }
            }
        }
        // it glides calmly to the height of its block
        const float ty = std::clamp(L.at.y, ylo, std::max(ylo, yhi));
        L.ly += (ty - L.ly) * (1.f - std::exp(-dt * 0.8f));
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void SpecimenUniverse::addCell(float x, float y, float half, int type, const CellLook& l, int kind, float prog, float alpha,
                                 float flash) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)kind}, {(float)type, l.seed, prog, alpha}, {l.nx, l.ny, flash, l.life >= 0 ? 1.f : 0.f}, {x, y, 0.f, 0.f}});
}

void SpecimenUniverse::buildBoard(const Game& g) {
    // the ghost: the wall alone, where the piece will land
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            CellLook l;
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                addCell(w.x, w.y, 0.5f, p.type, l, 1, 0.f, 1.f, 0.f);
            }
        }
    }
    // the stack (gone into the dying cells at a game over)
    if (overT_ < 0.f)
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Cell& c = g.cell(x, y);
                if (c.type < 0) continue;
                const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
                addCell(p.x, p.y, 0.5f, c.type, look_[y][x], 0, 0.f, 1.f, std::pow(saturate(c.flash), 14.f));
            }
    // the falling piece: exactly a locked one at its place (its cells keep their looks when it locks)
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy); // (whole above the well too: it spawns there)
            addCell(w.x, w.y, 0.5f, p.type, fallLook_[i], 0, 0.f, 1.f, 0.f);
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
            l.nx = std::cos(an) * 0.07f;
            l.ny = std::sin(an) * 0.07f;
            l.seed = hashf((uint64_t)(type * 7 + i));
            addCell(center.x + ox * sc, center.y - oy * sc, 0.5f * sc, type, l, 0, 0.f, alpha, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.45f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void SpecimenUniverse::drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count) {
    if (!count) return;
    glUseProgram(prog);
    setMat(prog, "uVP", vp_);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
}

void SpecimenUniverse::setStain(GLuint prog) {
    auto arr = [&](const char* name, const vec3* v, int n) {
        float f[21];
        for (int i = 0; i < n; i++) { f[i * 3] = v[i].x; f[i * 3 + 1] = v[i].y; f[i * 3 + 2] = v[i].z; }
        glUniform3fv(ugl::U(prog, name), n, f);
    };
    arr("uFieldA", base_.field, 2);
    arr("uFieldB", dropS_.field, 2);
    arr("uPieceA", base_.piece, 7);
    arr("uPieceB", dropS_.piece, 7);
    arr("uOrgA", base_.org, 3);
    arr("uOrgB", dropS_.org, 3);
    set1f(prog, "uStain", stain_);
    set1f(prog, "uStainSeed", seedF_);
    set1f(prog, "uDark", 1.f);   // the crawlers glow on the dark bench (the loupes' organisms set it back to bright-field)
    set1f(prog, "uSlow", slow_);
    glUniform3fv(ugl::U(prog, "uBack"), 2, &back_[0].x);
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

void SpecimenUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void SpecimenUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const vec3 acc = hex(VARIANTS[variant_].accent);
    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? acc : vec3(0.02f);
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
}

// The lettering, by the host (text.hpp): Lato in spaced capitals for the names, the labels and the numbers, EB
// Garamond Italic for the song and the notices, like a slide's label; pale ink (or the stain's accent) on a faint
// black halo against the bench.
void SpecimenUniverse::hudLine(const HudText& h, text::Line& out) {
    const float s = height_ / 1080.f;
    const vec3 ink = hex(0xc6ccd4), accent = lerp(hex(VARIANTS[variant_].accent), vec3(1.f), 0.15f);
    text::Font* sans = text::font(FONT_SANS);
    text::Font* it = text::font(FONT_IT);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    // a run: its size and letter spacing in pixels, moved down by dy
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
    // the letters a hair full, a faint black halo just around them
    out.look.weight = 0.0075f;
    out.look.halo = vec3(0.f);
    out.look.haloA = 0.35f;
    out.look.haloW = 0.065f;
    out.look.haloPow = 1.5f;
    out.alpha = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void SpecimenUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    if (overT_ < 0.f && !bare_) syncLooks(game); // the world alone: the cells' looks kept for the whole frame
    // a new variant eases in
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        auto ease = [&](Stain& s, const Stain& t) {
            for (int i = 0; i < 2; i++) s.field[i] = lerp(s.field[i], t.field[i], k);
            for (int i = 0; i < 7; i++) s.piece[i] = lerp(s.piece[i], t.piece[i], k);
            for (int i = 0; i < 3; i++) s.org[i] = lerp(s.org[i], t.org[i], k);
        };
        ease(base_, baseT_);
        ease(dropS_, dropT_);
        for (int i = 0; i < 2; i++) back_[i] = lerp(back_[i], backT_[i], k);
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
    stain_ = approach(stain_, dropMode_ ? 1.f : 0.f, dropMode_ ? 0.42f : 0.12f, dt);
    swell_ = approach(swell_, 0.f, 0.22f, dt);
    const float act = (0.45f + 0.35f * wm + 0.4f * wp);
    for (Lyse& l : lyses_) l.t += dt;
    lyses_.erase(std::remove_if(lyses_.begin(), lyses_.end(), [](const Lyse& l) { return l.t > LYSE_T; }), lyses_.end());
    for (Speck& k : specks_) {
        k.t += dt;
        k.p.x += k.v.x * dt;
        k.p.y += k.v.y * dt;
        k.v.x *= std::exp(-dt * 3.f);
        k.v.y *= std::exp(-dt * 3.f);
    }
    specks_.erase(std::remove_if(specks_.begin(), specks_.end(), [](const Speck& k) { return k.t > k.life; }), specks_.end());
    // the crawlers: toward their way out, wandering a little, in a crawling pulse
    for (Crawler& c : crawlers_) {
        c.age += wdt;
        const float want = std::atan2(c.dest.y - c.o.p.y, c.dest.x - c.o.p.x) + 0.45f * std::sin(c.age * 1.1f + c.o.seed * 20.f);
        c.o.ang += wrapAngle(want - c.o.ang) * (1.f - std::exp(-wdt * 2.f));
        const float sp = c.speed * smoothstepf(0.f, 0.8f, c.age) * (0.75f + 0.35f * std::sin(c.age * 5.f + c.o.seed * 9.f));
        c.o.p.x += std::cos(c.o.ang) * sp * wdt;
        c.o.p.y += std::sin(c.o.ang) * sp * wdt;
    }
    crawlers_.erase(std::remove_if(crawlers_.begin(), crawlers_.end(), [&](const Crawler& c) {
        return std::fabs(c.o.p.x - viewC_.x) > viewH_.x + 1.5f || std::fabs(c.o.p.y - viewC_.y) > viewH_.y + 1.5f || c.age > 40.f;
    }), crawlers_.end());
    if (!bare_) updateLenses(game, dt, wdt);
    lensShown_ = std::min(1.f, lensShown_ + ((lenses_[0].placed || lenses_[1].placed) ? dt / 1.5f : 0.f));

    // ---- Instances.
    const float bridgeK = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    cells_.clear();
    std::vector<Inst> dwellInst;   // drawn over the tissue: the crawlers
    if (!bare_) { // the world alone: no tissue, nothing bursting or dying
        for (const Lyse& l : lyses_) addCell(l.x, l.y, 0.5f, l.type, l.l, 2, saturate(l.t / LYSE_T), 1.f, 0.f);
        if (overT_ >= 0.f)
            for (const Dying& d : dying_) {
                const float t = saturate((overT_ - d.delay) / DIE_T);
                if (t < 1.f) addCell(d.x, d.y, 0.5f, d.type, d.l, 3, t, 1.f, 0.f);
            }
        buildBoard(game);
    }
    // the crawlers on the bench: tiny, glowing in their cell's dye
    if (!bare_)
        for (const Crawler& c : crawlers_) {
            const Org& o = c.o;
            const float a = smoothstepf(0.1f, 0.5f, c.age) * (1.f - bridgeK);
            const float ang = o.kind == K_CENTRIC || o.kind == K_VOLVOX ? 0.f : o.ang;
            const float anim = worldTime_ * (o.kind == K_AMOEBA ? 1.f : 1.2f) + o.seed * 100.f;
            dwellInst.push_back({{o.p.x, o.p.y, 0.6f, ang}, {(float)o.kind, o.seed, anim, a}, {0.f, 0.8f, 0.75f, (float)o.type}, {o.spin, 1.f, 0.f, 0.f}});
        }
    std::vector<Inst> speckInst;
    for (const Speck& k : specks_) {
        if (bare_) break; // (pushed out from under a dropped piece)
        const float s = k.t / k.life;
        const float a = smoothstepf(0.f, 0.1f, s) * (1.f - smoothstepf(0.4f, 1.f, s)) * 0.5f;
        speckInst.push_back({{k.p.x, k.p.y, k.r, 0.f}, {0.f, 0.f, 0.f, a}, {0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}});
    }

    // ---- Draw.
    const float aaW = 2.f * viewH_.y / (float)height_;
    // the loupes: the fine lines to their blocks (under the tissue), brackets around the blocks (over it), the
    // loupes, the organisms in them, their bezels over those
    std::vector<Inst> leadInst, bracketInst, lensInst, lensOrgInst, rimInst;
    if (!bare_)
        for (int s = 0; s < 2; s++) {
            const Lens& L = lenses_[s];
            // the loupe stays (once it has found a first block), what it shows fades
            const float shown = lensShown_ * (1.f - bridgeK), a = smoothstepf(0.f, 1.f, L.a) * shown;
            if (shown <= 0.003f) continue;
            const vec2 c = lensCenter(s);
            const Crawler* cr = L.follow >= 0 ? crawler(L.follow) : nullptr;
            vec2 d{L.at.x - c.x, L.at.y - c.y};
            const float dl = std::max(1e-3f, std::sqrt(d.x * d.x + d.y * d.y));
            d = vec2{d.x / dl, d.y / dl};
            leadInst.push_back({{c.x + d.x * lensR_ * 1.04f, c.y + d.y * lensR_ * 1.04f, L.at.x, L.at.y}, {aaW * 1.1f, 0.5f * a, 0.f, 0.f}, {}, {}});
            const float h = cr ? 0.42f : 0.6f, arm = cr ? 0.14f : 0.2f;
            for (int k = 0; k < 4; k++) {
                const float sx = k % 2 ? 1.f : -1.f, sy = k / 2 ? 1.f : -1.f;
                const vec2 q{L.at.x + sx * h, L.at.y + sy * h};
                bracketInst.push_back({{q.x, q.y, q.x - sx * arm, q.y}, {aaW * 1.4f, 0.85f * a, 0.f, 0.f}, {}, {}});
                bracketInst.push_back({{q.x, q.y, q.x, q.y - sy * arm}, {aaW * 1.4f, 0.85f * a, 0.f, 0.f}, {}, {}});
            }
            const bool live = L.l.life >= 0;
            if (cr) {
                // the organism crawling out, over the bare bench sliding by under it
                lensInst.push_back({{c.x, c.y, lensR_, 0.f}, {(float)cr->o.type, cr->o.seed, shown, 0.f}, {cr->o.p.x * 1.5f, cr->o.p.y * 1.5f, 0.f, a}, {}});
                rimInst.push_back({{c.x, c.y, lensR_, 1.f}, {(float)cr->o.type, cr->o.seed, shown, 0.f}, {}, {}});
                const Org& o = cr->o;
                const float ang = o.kind == K_CENTRIC || o.kind == K_VOLVOX ? 0.f : o.ang;
                const float anim = worldTime_ * (o.kind == K_AMOEBA ? 1.f : 0.8f + 0.5f * act) + o.seed * 100.f;
                lensOrgInst.push_back({{c.x, c.y, lensR_ * 0.95f, ang}, {(float)o.kind, o.seed, anim, a},
                                       {0.f, 0.5f + 0.3f * wm + 0.9f * pulse, 0.45f, (float)o.type}, {o.spin, 0.f, 0.f, 0.f}});
                continue;
            }
            lensInst.push_back({{c.x, c.y, lensR_, 0.f}, {(float)L.type, L.l.seed, shown, live ? 1.f : 0.f}, {L.pan, L.pan * 0.6f, 1.f, a}, {}});
            rimInst.push_back({{c.x, c.y, lensR_, 1.f}, {(float)L.type, L.l.seed, shown, 0.f}, {}, {}});
            if (live) {
                const Org o = dweller(L.l, c.x, c.y, L.type);
                const float t = worldTime_ + L.l.seed * 50.f, sd = L.l.seed * 37.f;
                const vec2 p{c.x + lensR_ * 0.08f * std::sin(t * 0.21f + sd), c.y + lensR_ * 0.08f * std::cos(t * 0.17f + sd)};
                const float ang = o.kind == K_CENTRIC || o.kind == K_VOLVOX ? 0.f : o.ang;
                const float anim = worldTime_ * (o.kind == K_AMOEBA ? 1.f : 0.8f + 0.5f * act) + o.seed * 100.f;
                lensOrgInst.push_back({{p.x, p.y, lensR_ * 0.95f, ang}, {(float)o.kind, o.seed, anim, a},
                                       {0.f, 0.15f + 0.3f * wm + 0.9f * pulse + 0.6f * kickEnv_, o.tint, (float)o.type}, {o.spin, 0.f, 0.f, 0.f}});
            }
        }
    auto drawLines = [&](const std::vector<Inst>& v) {
        if (v.empty()) return;
        glUseProgram(progLine_);
        set1f(progLine_, "uAA", aaW);
        set3f(progLine_, "uCol", ugl::toLinear(hex(0xdfe6e8)) * (1.f - 0.4f * dimT_));
        drawInst(progLine_, lineVao_, lineVbo_, v.data(), v.size() * sizeof(Inst), v.size());
    };
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progField_);
    setStain(progField_);
    set4f(progField_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progField_, "uTime", worldTime_);
    set1f(progField_, "uIllum", 0.93f + 0.045f * wm + 0.03f * wp);
    set1f(progField_, "uSwell", swell_ * (1.f - 0.6f * slow_));
    set1f(progField_, "uSeed", seedF_);
    set1f(progField_, "uAA", aaW);
    set1f(progField_, "uReticle", bare_ ? 0.f : 1.f - bridgeK);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // the scene's alpha is the stack's (for the bridge)
    if (!speckInst.empty()) {
        glUseProgram(progSpeck_);
        setStain(progSpeck_);
        drawInst(progSpeck_, speckVao_, speckVbo_, speckInst.data(), speckInst.size() * sizeof(Inst), speckInst.size());
    }
    drawLines(leadInst);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(progPiece_);
    setStain(progPiece_);
    set1f(progPiece_, "uGlow", (0.95f + 0.05f * wm + 0.08f * wp + 0.1f * swell_) * (1.f - 0.1f * slow_));
    drawInst(progPiece_, pieceVao_, pieceVbo_, cells_.data(), cells_.size() * sizeof(Inst), cells_.size());
    if (!dwellInst.empty()) {
        glUseProgram(progOrg_);
        setStain(progOrg_);
        set1f(progOrg_, "uGlow", (0.85f + 0.12f * wm + 0.15f * wp + 0.2f * swell_) * (1.f - 0.2f * slow_));
        drawInst(progOrg_, orgVao_, orgVbo_, dwellInst.data(), dwellInst.size() * sizeof(Inst), dwellInst.size());
    }
    if (!lensInst.empty()) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        drawLines(bracketInst);
        glUseProgram(progLens_);
        setStain(progLens_);
        set1f(progLens_, "uTime", worldTime_);
        drawInst(progLens_, lensVao_, lensVbo_, lensInst.data(), lensInst.size() * sizeof(Inst), lensInst.size());
        if (!lensOrgInst.empty()) {
            glUseProgram(progOrg_);
            setStain(progOrg_);
            set1f(progOrg_, "uDark", slow_);   // in bright-field under the loupe
            set1f(progOrg_, "uGlow", 0.95f + 0.1f * wp + 0.15f * swell_);
            drawInst(progOrg_, orgVao_, orgVbo_, lensOrgInst.data(), lensOrgInst.size() * sizeof(Inst), lensOrgInst.size());
        }
        glUseProgram(progLens_);
        drawInst(progLens_, lensVao_, lensVbo_, rimInst.data(), rimInst.size() * sizeof(Inst), rimInst.size());
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    // ---- The glow (a faint one: the gloss, the crawlers).
    glDisable(GL_BLEND);
    const float bloomK = 0.2f * (0.22f + 0.1f * wp + 0.2f * swell_);
    if (bloomK > 0.01f) {
        bloomA_.bind();
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
            bloomB_.bind();
            glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
            set2f(progBlur_, "uDir", st / bloomA_.w, 0.f);
            ugl::drawFullscreen();
            bloomA_.bind();
            glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
            set2f(progBlur_, "uDir", 0.f, st / bloomA_.h);
            ugl::drawFullscreen();
        }
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
    set1f(progFinal_, "uBloomK", bloomK);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", bridgeK);
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_specimen() {
    static const UniverseEntry e{
        "SPECIMEN", "lumps of living tissue on a dark bench, loupes on the sides looking into them; a cleared row's organisms crawl off the screen",
        [] { return std::unique_ptr<Universe>(new SpecimenUniverse()); },
        // A quiet, precise world: fits most songs, a little more the calm and airy ones.
        [](const SongLook& l) { return 0.8f + 0.15f * l.airWeight + 0.15f * (1.f - l.density); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
