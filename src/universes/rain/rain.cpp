// RAIN: it rains everywhere, over a colorful street at dusk, and the rain falls on the game.
//
// One world, seen flat in the board's plane: a rainy street at dusk behind everything (a procedural city blurred by
// the rain, its windows turned to bokeh, its neon signs, market awnings or paper lanterns glowing on the wet facades;
// the wet street below mirrors it in puddles rippled by the drops), the far rain as thin slanted streaks everywhere,
// the well a dark glass backing that keeps the stack readable.
//
// The near rain has physics: a few hundred to a few thousand thin streaks (the music sets how many) fall over the
// whole screen until they meet a solid: the top of the stack in their column, the falling piece's cells (which
// shelter the cells under them: a dry rain shadow follows the piece down the well), the floor of the well, the side
// piles or the street. Where they land they splash (a small crown and two or three droplets), and near an exposed
// edge some of the water runs down the face as a thin drip and falls off the bottom of the face, splashing again
// below. The rain never crosses a cell: it stops on it.
//
// The pieces are soft porous sugar cubes in pastel colors, one per cell: a matte grainy body (pores, a few glinting
// crystals), soft bevelled edges lit from above, the street's neon caught faintly, a film of water along the top edge,
// beads of water on the face and now and then a rivulet sliding down it (with the cell's wetness: a sheltered cell
// slowly dries). The previews are the same cubes, dry and plain. The well is a wet glass case: dark slate glass with a
// faint grid behind the stack, thick glass edges with beads of water on them and drips hanging from the top edge.
//
// A cleared row's tiles are thrown out of the well, half to each side, on a scripted arc (behind the stack and the
// HUD) to a release over the side bands, then become rigid boxes in a small 2D solver (sequential impulses, friction,
// sleeping): they tumble and settle into messy heaps on the street, where the rain falls on their tilted faces and
// slides off the lower corners. The tiles nearest the walls come out toward the camera instead (the foreground): they
// fall along the well's outer wall clear of the HUD, grow as they come closer, land with a big splash at the bottom of
// the picture beside the well and melt there in the rain (soaking, sagging, crumbling to mush in a puddle) until
// nothing is left, so they never pile up. The rain soaks the heaped ones like cardboard: dark water stains creep down from their tops, they
// go matte and limp (sagging in the middle, bulging, their turn relaxing toward lying flat) and are crushed thinner, so
// the heaps slump and pack down little by little over the song. Each heap is capped (the oldest fades away). A new song washes them away; game over
// throws the whole stack out, row by row from the top.
//
// The music sets the rain: a drizzle when calm, steady rain at mid, a downpour at a peak (with a gust on the kicks).
// The drop swings the whole street to another colour mood for the whole drop section: a grade at the end of the
// backdrop pass (a YIQ hue rotation, saturation, a duotone wash, an exposure; the well's glass, the pieces and the HUD
// untouched), on the downbeat in well under a second, then gliding between the variant's two moods every 8 bars;
// with it a thunderstorm: the heaviest rain, a steeper slant, the flooding street, soft lightning glowing in the clouds
// now and then on a bar line (a tinted glow, held back around the HUD's lines, never a white flash); after the drop it
// all eases back over a few seconds. Slow motion slows the rain until it hangs in the air.
// Around a song the street's lights come on one by one (windows, shops, signs) as the rain sets in, and go out one by
// one after it, the rain thinning.
//
// Cost: one fullscreen backdrop pass, the rain stepped on the CPU against per-column spans of the solids (a few
// dozen tests per drop), instanced shapes and tiles, SDF type.
#include "universes/rain/rain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/rain/shaders.hpp"

namespace {

// Manrope (Medium for the values, Light for the labels), SIL Open Font License, see OFL-Manrope.txt.
const unsigned char FONT_MED[] = {
#include "universes/rain/Manrope-Medium.ttf.inc"
};
const unsigned char FONT_LIGHT[] = {
#include "universes/rain/Manrope-Light.ttf.inc"
};

namespace shaders = rain_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

float ease(float t) {
    t = saturate(t);
    return t * t * (3.f - 2.f * t);
}

struct VariantDef {
    const char* name;
    int style; // 0 neon street, 1 flower market, 2 monsoon
    unsigned skyTop, skyLow, far, near, win, mist, street, panel;
    unsigned neon[4];
    float slant;
    unsigned piece[7]; // I O T S Z J L
};
const VariantDef VARIANTS[] = {
    {"NEON STREET", 0, 0x1a1240, 0x7a2a6e, 0x2a1d52, 0x140e2a, 0xffb35a, 0x5a3d7a, 0x120c20, 0x0b0914,
     {0xff3fa4, 0x2fe6ff, 0xb46bff, 0xffd23a}, 0.09f,
     {0x8ee6f5, 0xfbe58a, 0xc9a8f7, 0x9fe3a4, 0xf7a1b5, 0x93aef7, 0xf9bf8c}},
    {"FLOWER MARKET", 1, 0x1f2f66, 0xf08a5e, 0x5a3a6a, 0x2e1a2c, 0xffd08a, 0xb0607a, 0x1c1220, 0x100b12,
     {0xff4f7a, 0x3fcfa0, 0xffb33a, 0x9a7aff}, 0.06f,
     {0x9fdde9, 0xfde48f, 0xd2b3fa, 0xa8e2a0, 0xf8a9c4, 0x9cb6f5, 0xfbc493}},
    {"MONSOON", 2, 0x082b3a, 0x1f7a70, 0x15504e, 0x0a2428, 0xffc06a, 0x3f8a80, 0x081a1c, 0x061012,
     {0xff6a3a, 0xffb03a, 0xff4f6a, 0xffd86a}, 0.16f,
     {0x8fe8de, 0xfcdc8a, 0xeea5cb, 0xa3e5b5, 0xf9ad96, 0x9faaf6, 0xc6a9f5}},
};
// The drop's colour moods, two per variant (A on the downbeat, then B 8 bars later, alternating, gliding): a hue
// rotation (degrees of hue), a saturation gain, a duotone wash from a shadow to a highlight colour, an exposure.
struct Mood { float hue, sat; unsigned lo, hi; float duo, expo; };
const Mood MOODS[][2] = {
    // NEON STREET: from its purple / cyan / magenta night to a hot red-orange inferno, then an acid green-teal
    {{60.f, 1.5f, 0x500010, 0xff5020, 0.5f, 1.1f}, {-100.f, 1.4f, 0x012624, 0xb4ff3a, 0.4f, 1.06f}},
    // FLOWER MARKET: from its blue dusk and orange awnings to an ultraviolet-pink bloom, then a chartreuse spring
    {{-30.f, 1.5f, 0x2a0650, 0xff50d0, 0.5f, 1.08f}, {-80.f, 1.5f, 0x062a14, 0xdcff4a, 0.42f, 1.06f}},
    // MONSOON: from its teal night and orange lanterns to a crimson storm, then an ultramarine night with gold lanterns
    {{180.f, 1.4f, 0x400412, 0xff4050, 0.45f, 1.08f}, {40.f, 1.4f, 0x060c50, 0xffc83a, 0.45f, 1.08f}},
};

constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float GAP = 0.07f;         // between tiles, each side (board units)
constexpr float HEAP_HALF = 0.38f;   // a heaped tile's half size, dry (board units)
constexpr float HORIZON = -8.2f;     // the street's far edge (as in the backdrop)

// The HUD columns, softened (0 outside .. 1 well inside), for what passes behind them.
float hudCover(float x, float y) {
    auto inside = [](const float* r, float x, float y) {
        const float dx = std::max(r[0] - x, x - r[2]), dy = std::max(r[1] - y, y - r[3]);
        return 1.f - smoothstepf(-0.6f, 0.4f, std::max(dx, dy));
    };
    return std::max(inside(layout::LEFT_COLUMN, x, y), inside(layout::RIGHT_COLUMN, x, y));
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool RainUniverse::init(int width, int height) {
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BG_FS, "rain backdrop");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "rain shapes");
    progTile_ = ugl::program(shaders::TILE_VS, shaders::TILE_FS, "rain tiles");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, int stride, int attribs) {
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
    instanced(shapeVao_, shapeVbo_, sizeof(Shape), 3);
    instanced(tileVao_, tileVbo_, sizeof(Tile), 5);
    glBindVertexArray(0);

    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) seeds_[y][x] = rng_.range(0.f, 100.f);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void RainUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    unitPx_ = height_ / (2.f * hh);
    // the heaps: on the street in the bands beyond the HUD columns
    bandIn_ = 11.9f;
    bandOut_ = std::max(bandIn_ + 4.f, hw - 0.2f);
    ground_ = viewC_.y - hh + 0.2f;
}

RainUniverse::~RainUniverse() {
    for (GLuint p : {progBg_, progShape_, progTile_}) glDeleteProgram(p);
    for (GLuint v : {shapeVao_, tileVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, shapeVbo_, tileVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void RainUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 2;   // dark and minor: the monsoon
        else if (t.hasSong && t.brightness > 0.58f) v = 1;         // bright: the flower market
        else v = (int)(t.seed % 2) == 0 ? 0 : (t.airWeight > 0.55f ? 1 : 0);
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x5A1Full);
    // a new song washes the piles away
    if (seconds > 0.f)
        for (Body& b : bodies_)
            if (!b.fading) { b.fading = true; b.fadeRate = 0.5f; }
    if (seconds > 0.f)
        for (Fore& f : fores_) f.rate = std::max(f.rate, 0.6f);
}

void RainUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)impact;
    level_ = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level_ < 2) dropMode_ = false;
    if (seconds < 0.1f) {
        rain_ = level_ == 0 ? 0.2f : level_ == 1 ? 0.5f : 0.78f;
        glow_ = 0.65f + 0.2f * level_;
    }
}

void RainUniverse::drop(float strength) {
    (void)strength;
    dropMode_ = true;
}

vec2 RainUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints RainUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = ugl::toLinear(lerp(hex(V.near), hex(V.skyLow), 0.3f));
    h.ink = ugl::toLinear(vec3(0.88f, 0.9f, 0.95f));
    h.accent = ugl::toLinear(hex(V.neon[0]));
    h.light = false;
    h.styles = TS_RIPPLE | TS_INK;
    return h;
}

vec3 RainUniverse::pieceColor(int type) const { return hex(VARIANTS[variant_].piece[std::clamp(type, 0, 6)]); }

// =====================================================================================================
// Game events
// =====================================================================================================

void RainUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock:
    case GameEvent::HardDrop: {
        const bool hard = ev.type == GameEvent::HardDrop;
        // water squeezed out from under the piece, at its contact
        for (const auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            if (ev.type == GameEvent::Lock) {
                wet_[c.y][c.x] = 0.7f;
                seeds_[c.y][c.x] = rng_.range(0.f, 100.f);
            }
            bool bottom = true;
            for (const auto& o : ev.cells)
                if (o.x == c.x && o.y == c.y + 1) bottom = false;
            if (!bottom) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int n = hard ? 3 : 1;
            for (int i = 0; i < n; i++) {
                const float sx = rng_.chance(0.5f) ? -1.f : 1.f;
                sparks_.push_back({p.x + sx * 0.45f, p.y - 0.5f, sx * rng_.range(0.8f, 2.2f), rng_.range(1.2f, hard ? 4.f : 2.5f),
                                   0.f, rng_.range(0.25f, 0.4f), 0.035f});
            }
            if (hard) crowns_.push_back({p.x, p.y - 0.5f, 0.f, 0.6f});
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // half to each side, the tiles nearest the walls first, row after row
        std::vector<std::pair<float, GameEvent::CellInfo>> order;
        int rank = 0;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (!rows[y]) continue;
            for (auto& c : ev.cells)
                if (c.y == y) {
                    const int fromWall = c.x < Game::W / 2 ? c.x : Game::W - 1 - c.x;
                    order.push_back({rank * 0.12f + fromWall * 0.06f, c});
                }
            rank++;
        }
        std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        // the tiles nearest the walls come out toward the camera (the foreground, while it has room), the others
        // fly to the heaps
        for (auto& o : order) {
            const auto& c = o.second;
            const int fromWall = c.x < Game::W / 2 ? c.x : Game::W - 1 - c.x;
            if (fromWall <= 1 && !bare_ && foreTake(c.x < Game::W / 2 ? 0 : 1)) ejectFore(c.x, c.y, c.type, o.first);
            else eject(c.x, c.y, c.type, o.first);
        }
        // compact the wetness and the beads like the game did
        float nw[Game::H][Game::W] = {}, ns[Game::H][Game::W] = {};
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (rows[y]) continue;
            std::memcpy(nw[dst], wet_[y], sizeof(wet_[y]));
            std::memcpy(ns[dst], seeds_[y], sizeof(seeds_[y]));
            dst--;
        }
        std::memcpy(wet_, nw, sizeof(wet_));
        std::memcpy(seeds_, ns, sizeof(seeds_));
        break;
    }
    case GameEvent::TopOut: {
        // the whole stack is thrown out to the piles, row by row from the top
        int rank = 0, lastY = -1;
        std::vector<GameEvent::CellInfo> cells = ev.cells;
        std::stable_sort(cells.begin(), cells.end(), [](const auto& a, const auto& b) { return a.y < b.y; });
        for (auto& c : cells) {
            if (c.y != lastY) { lastY = c.y; rank++; }
            const int fromWall = c.x < Game::W / 2 ? c.x : Game::W - 1 - c.x;
            eject(c.x, c.y, c.type, 0.3f + rank * 0.1f + fromWall * 0.02f);
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The rain
// =====================================================================================================

void RainUniverse::buildSolids(const Game& g) {
    for (auto& c : cols_) {
        c.clear();
        c.push_back({-30.f, -10.f, 2, 0, 0}); // the well's floor
    }
    if (bare_) return;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0) continue;
            const float cy = layout::cellPos((float)x, y - g.rowOffset(y)).y;
            cols_[x].push_back({cy - 0.5f, cy + 0.5f, 0, x, y});
        }
    if (g.hasPiece() && !g.over()) {
        int cells[4][2];
        g.pieceCells(g.piece(), cells);
        const float dx = g.pieceVisualX() - g.piece().x, dy = g.pieceVisualY() - g.piece().y;
        for (auto& c : cells) {
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            const int col = std::clamp((int)std::floor(w.x + 5.f), 0, Game::W - 1);
            cols_[col].push_back({w.y - 0.5f, w.y + 0.5f, 1, c[0], c[1]});
        }
    }
}

bool RainUniverse::solidAt(float x, float y) const {
    if (x >= -5.f && x < 5.f) {
        const int c = std::clamp((int)std::floor(x + 5.f), 0, Game::W - 1);
        for (const Span& s : cols_[c])
            if (y > s.b && y < s.t) return true;
        return false;
    }
    const int c = std::clamp((int)std::floor(x - binX0_), 0, 95);
    for (int i : bins_[c]) {
        const Body& b = bodies_[i];
        if (b.solid && !b.thrown && std::fabs(x - b.x) < b.half * 1.42f && bodyInside(b, x, y)) return true;
    }
    return false;
}

float RainUniverse::surfaceBelow(float x, float y, int* kind, int* cx, int* cy, bool* inside) const {
    *inside = false;
    if (x >= -5.f && x < 5.f) {
        const int c = std::clamp((int)std::floor(x + 5.f), 0, Game::W - 1);
        float best = -1e9f;
        for (const Span& s : cols_[c]) {
            if (y > s.b + 0.02f && y < s.t - 0.03f) { *inside = true; return y; }
            if (s.t <= y + 0.03f && s.t > best) { best = s.t; *kind = s.kind; *cx = s.cx; *cy = s.cy; }
        }
        return best;
    }
    // the heaped tiles, their tilted faces
    float best = -1e9f;
    *kind = 4;
    const int bc = std::clamp((int)std::floor(x - binX0_), 0, 95);
    for (int i : bins_[bc]) {
        const Body& b = bodies_[i];
        if (!b.solid || b.thrown || std::fabs(x - b.x) > b.half * 1.42f) continue;
        if (bodyInside(b, x, y - 0.03f)) { *inside = true; return y; }
        const float t = bodyTop(b, x, nullptr);
        if (t <= y + 0.03f && t > best) { best = t; *kind = 3; *cx = b.id; *cy = 0; }
    }
    if (best < -1e8f && std::fabs(x) > bandIn_ && std::fabs(x) < bandOut_) { *kind = 5; return ground_; } // the street in the bands
    return best;
}

void RainUniverse::impact(float x, float y, float speed, int kind, int cx, int cy) {
    if (sparks_.size() < 1400) {
        const int n = 1 + (rng_.chance(0.55f) ? 1 : 0);
        const float sp = std::min(1.f, speed / 25.f);
        for (int i = 0; i < n; i++) {
            const float sx = rng_.chance(0.5f) ? -1.f : 1.f;
            sparks_.push_back({x, y + 0.02f, sx * rng_.range(0.4f, 1.6f), rng_.range(1.2f, 3.2f) * sp, 0.f,
                               rng_.range(0.18f, 0.32f), rng_.range(0.022f, 0.036f)});
        }
    }
    if (crowns_.size() < 900) crowns_.push_back({x, y, 0.f, rng_.range(0.16f, 0.3f)});
    if (kind == 0 && cy >= 0 && cy < Game::H && cx >= 0 && cx < Game::W) wet_[cy][cx] = std::min(1.f, wet_[cy][cx] + 0.06f);
    if (kind == 3) {
        // on a heaped tile: wetter, and on a tilted face the water slides down the slope and drips off the corner
        Body* b = bodyById(cx);
        if (b) {
            b->wet = std::min(1.f, b->wet + 0.06f);
            b->hits++;
            float sl = 0.f;
            bodyTop(*b, x, &sl);
            if (std::fabs(sl) > 0.12f && runs_.size() < 160 && rng_.chance(0.35f))
                runs_.push_back({x, y, sl > 0.f ? -1.f : 1.f, 0.5f + 2.5f * std::min(1.f, std::fabs(sl)), 0.f, b->id, sl});
        }
    }
    // near an exposed edge the water runs down the face
    if ((kind == 0 || kind == 1) && runs_.size() < 160 && rng_.chance(0.22f)) {
        const float el = std::floor(x), er = el + 1.f;
        if (x - el < 0.22f && !solidAt(el - 0.08f, y - 0.3f)) runs_.push_back({el, y, -1.f, rng_.range(0.8f, 1.6f), 0.f, -1, 0.f});
        else if (er - x < 0.22f && !solidAt(er + 0.08f, y - 0.3f)) runs_.push_back({er, y, 1.f, rng_.range(0.8f, 1.6f), 0.f, -1, 0.f});
    }
}

void RainUniverse::stepRain(float dt, float rdt) {
    const VariantDef& V = VARIANTS[variant_];
    const float hw = viewH_.x, hh = viewH_.y;
    const float slant = V.slant + 0.22f * storm_;
    // spawn: a drizzle .. a downpour
    if (bridge_.amount < 0.98f) {
        const float rate = lerpf(220.f, 2600.f, std::pow(saturate(rain_), 1.4f)) * (1.f + 0.6f * storm_) * (rdt / std::max(dt, 1e-4f)) * shower_;
        spawnAcc_ += rate * dt;
        int n = (int)spawnAcc_;
        spawnAcc_ -= n;
        n = std::min(n, 400);
        for (int i = 0; i < n && drops_.size() < 4000; i++) {
            Drop d;
            d.vy = -rng_.range(24.f, 31.f);
            d.vx = slant * d.vy;
            d.y = viewC_.y + hh + rng_.range(0.2f, 3.f);
            const float fall = d.y - (viewC_.y - hh);
            d.x = rng_.range(-hw - 1.f, hw + 1.f + slant * fall);
            d.len = rng_.range(0.6f, 1.15f);
            d.a = rng_.range(0.45f, 1.f);
            d.endY = rng_.range(viewC_.y - hh, HORIZON + 0.4f); // where it meets the street (its depth)
            d.drip = false;
            // the storm: sheets of rain sweeping across in gusts (as the far rain's)
            const float gx = std::sin(d.x * 0.11f + rainT_ * 1.7f) * 0.7f + std::sin(d.x * 0.05f - rainT_ * 0.6f) * 0.3f;
            if (storm_ > 0.01f && rng_.uniform() > lerpf(1.f, 0.3f + 1.4f * smoothstepf(-0.2f, 0.9f, gx), storm_)) continue;
            drops_.push_back(d);
        }
    }
    // fall, meet the solids, splash
    for (size_t i = 0; i < drops_.size();) {
        Drop& d = drops_[i];
        if (d.drip) d.vy -= 32.f * rdt;
        const float nx = d.x + d.vx * rdt, ny = d.y + d.vy * rdt;
        int kind = 4, cx = 0, cy = 0;
        bool inside = false;
        const float s = surfaceBelow(nx, d.y, &kind, &cx, &cy, &inside);
        bool dead = false;
        if (inside) dead = true;
        else if (s > -1e8f && ny <= s) {
            impact(nx, s, -d.vy, kind, cx, cy);
            dead = true;
        } else if (kind == 4 && ny <= d.endY && std::fabs(nx) > 5.2f) {
            if (crowns_.size() < 900) crowns_.push_back({nx, d.endY, 0.f, -rng_.range(0.14f, 0.26f)}); // (on the street: w < 0)
            dead = true;
        } else if (ny < viewC_.y - hh - 1.5f) dead = true;
        if (dead) {
            drops_[i] = drops_.back();
            drops_.pop_back();
            continue;
        }
        d.x = nx;
        d.y = ny;
        i++;
    }
    // droplets of the splashes
    for (size_t i = 0; i < sparks_.size();) {
        Spark& s = sparks_[i];
        s.t += rdt;
        s.vy -= 22.f * rdt;
        s.x += s.vx * rdt;
        s.y += s.vy * rdt;
        if (s.t > s.life) { sparks_[i] = sparks_.back(); sparks_.pop_back(); continue; }
        i++;
    }
    for (size_t i = 0; i < crowns_.size();) {
        crowns_[i].t += rdt;
        if (crowns_[i].t > 0.16f) { crowns_[i] = crowns_.back(); crowns_.pop_back(); continue; }
        i++;
    }
    // the drips run down their faces and fall off the bottom
    for (size_t i = 0; i < runs_.size();) {
        Run& r = runs_[i];
        r.t += rdt;
        if (r.body >= 0) {
            // sliding down a heaped tile's slope, dripping off its lower corner
            Body* b = bodyById(r.body);
            bool off = !b || !b->solid || r.t > 5.f;
            if (!off) {
                const float nx = r.x + r.side * r.v * rdt;
                float sl = r.sl;
                const float t = bodyTop(*b, nx, &sl);
                if (t < r.y - 0.4f) {
                    off = true;
                    if (drops_.size() < 4000) drops_.push_back({r.x + r.side * 0.03f, r.y, r.side * 0.4f, -0.8f, 0.18f, 0.9f, viewC_.y - viewH_.y, true});
                } else {
                    r.x = nx;
                    r.y = t;
                    r.sl = sl;
                }
            }
            if (off) { runs_[i] = runs_.back(); runs_.pop_back(); continue; }
            i++;
            continue;
        }
        r.y -= r.v * rdt;
        const bool face = solidAt(r.x - r.side * 0.1f, r.y - 0.05f);
        const bool landed = solidAt(r.x + r.side * 0.1f, r.y - 0.05f); // it reached the neighbour's top: soaked in
        if (!face || landed || r.t > 6.f) {
            if (!face && !landed && drops_.size() < 4000)
                drops_.push_back({r.x + r.side * 0.04f, r.y, 0.f, -1.f, 0.18f, 0.9f, viewC_.y - viewH_.y, true});
            runs_[i] = runs_.back();
            runs_.pop_back();
            continue;
        }
        i++;
    }
}

void RainUniverse::drawRain(bool) {
    const VariantDef& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    const float vis = 1.f - smoothstepf(0.f, 0.7f, bridge_.amount);
    if (vis <= 0.01f) return;
    const vec3 tint = graded(lerp(lerp(hex(V.mist), vec3(0.95f, 0.97f, 1.f), 0.65f), vec3(0.85f, 0.92f, 1.f), flash_));
    const float w = std::max(1.f, 1.3f * s);
    const float slowLen = lerpf(1.f, 0.35f, slow_);
    for (const Drop& d : drops_) {
        const float sp = std::sqrt(d.vx * d.vx + d.vy * d.vy);
        const float L = d.drip ? std::min(0.3f, 0.05f + sp * 0.02f) : d.len * slowLen;
        const float tx = d.x - d.vx / sp * L, ty = d.y - d.vy / sp * L;
        const vec2 a = px(d.x, d.y), b = px(tx, ty);
        const bool inWell = std::fabs(d.x) < 5.f && d.y < 10.f;
        float al = d.a * (inWell ? 0.32f : 0.42f) * (0.7f + 0.3f * rain_) * (1.f + 0.8f * flash_);
        al *= 1.f - 0.55f * hudCover(d.x, d.y);
        seg(a.x, a.y, b.x, b.y, d.drip ? w * 1.4f : w, tint, al * vis, 0.f);
    }
    for (const Spark& p : sparks_) {
        const vec2 a = px(p.x, p.y);
        const float k = 1.f - p.t / p.life;
        disc(a.x, a.y, p.r * unitPx_, p.r * unitPx_, tint, 0.55f * k * vis);
    }
    for (const Crown& c : crowns_) {
        const vec2 a = px(c.x, c.y);
        const float k = c.t / 0.16f;
        const bool street = c.w < 0.f;
        const float rx = (0.3f + 0.7f * k) * std::fabs(c.w) * unitPx_, ry = std::max(1.f, rx * (street ? 0.18f : 0.22f));
        const float al = (street ? 0.18f * (1.f - 0.6f * hudCover(c.x, c.y)) : 0.4f) * (1.f - k);
        box(a.x, a.y - ry, rx, ry, tint, al * vis, ry, std::max(1.f, 1.1f * s));
    }
    for (const Run& r : runs_) {
        if (r.body >= 0) {
            const float L = 0.22f / std::sqrt(1.f + r.sl * r.sl);
            const vec2 a = px(r.x, r.y + 0.02f), b = px(r.x - r.side * L, r.y + 0.02f + r.side * -L * r.sl * -1.f);
            seg(a.x, a.y, b.x, b.y, w * 1.6f, tint, 0.5f * vis, 0.f);
            disc(a.x, a.y, w * 1.5f, w * 1.5f, tint, 0.8f * vis);
            continue;
        }
        const vec2 a = px(r.x + r.side * 0.035f, r.y), b = px(r.x + r.side * 0.035f, r.y + 0.28f);
        seg(a.x, a.y, b.x, b.y, w * 1.6f, tint, 0.55f * vis, 0.f);
        disc(a.x, a.y, w * 1.4f, w * 1.6f, tint, 0.75f * vis);
    }
    flushShapes();
}

// =====================================================================================================
// The heaps: thrown tiles, then rigid boxes
// =====================================================================================================

RainUniverse::Body* RainUniverse::bodyById(int id) {
    for (Body& b : bodies_)
        if (b.id == id) return &b;
    return nullptr;
}

bool RainUniverse::bodyInside(const Body& b, float x, float y) const {
    const float c = std::cos(b.a), s = std::sin(b.a), dx = x - b.x, dy = y - b.y;
    const float lx = c * dx + s * dy, ly = -s * dx + c * dy;
    return std::fabs(lx) < b.half && std::fabs(ly) < b.half;
}

float RainUniverse::bodyTop(const Body& b, float x, float* slope) const {
    const float c = std::cos(b.a), s = std::sin(b.a), h = b.half;
    float P[4][2];
    const float L[4][2] = {{-h, -h}, {h, -h}, {h, h}, {-h, h}};
    for (int i = 0; i < 4; i++) {
        P[i][0] = b.x + c * L[i][0] - s * L[i][1];
        P[i][1] = b.y + s * L[i][0] + c * L[i][1];
    }
    float best = -1e9f;
    for (int i = 0; i < 4; i++) {
        const float* A = P[i];
        const float* B = P[(i + 1) & 3];
        const float lo = std::min(A[0], B[0]), hi = std::max(A[0], B[0]);
        if (x < lo || x > hi || hi - lo < 1e-4f) continue;
        const float k = (B[1] - A[1]) / (B[0] - A[0]);
        const float y = A[1] + (x - A[0]) * k;
        if (y > best) { best = y; if (slope) *slope = k; }
    }
    return best;
}

float RainUniverse::textCover(float x, float y) const {
    float c = 0.f;
    for (const vec4& r : textRects_) {
        const float dx = std::max(r.x - x, x - r.z), dy = std::max(r.y - y, y - r.w);
        c = std::max(c, 1.f - smoothstepf(-0.3f, 0.5f, std::max(dx, dy)));
    }
    return c;
}

// A cleared tile is thrown out of the well (an arc out and up, behind the stack and the HUD) to its release over its
// band, where it falls, tumbling, onto the heap.
void RainUniverse::eject(int x, int y, int type, float delay) {
    const int side = x < Game::W / 2 ? 0 : 1;
    const float dir = side == 0 ? -1.f : 1.f;
    // a full heap: its oldest tile sinks and fades away
    int n = 0, oldest = -1;
    for (size_t i = 0; i < bodies_.size(); i++) {
        const Body& b = bodies_[i];
        if (b.side != side || b.fading) continue;
        n++;
        if (oldest < 0 || b.id < bodies_[oldest].id) oldest = (int)i;
    }
    // the throw aims at the lowest place of the band (of a few tried): the heap spreads into a mound before it climbs
    const float out = side == 0 ? leftOut_ : bandOut_;
    float tx = 0.f, top = 1e9f;
    for (int k = 0; k < 6; k++) {
        const float cx = dir * rng_.range(bandIn_ + 0.6f, std::max(bandIn_ + 0.8f, out - 0.5f));
        float t = ground_;
        for (const Body& o : bodies_)
            if (!o.thrown && o.solid && std::fabs(o.x - cx) < 0.9f) t = std::max(t, o.y + o.half);
        if (t + rng_.range(0.f, 0.5f) < top) { top = t; tx = cx; }
    }
    const int cap = side == 0 ? 150 : 170;
    if ((n >= cap || top > -4.f) && oldest >= 0) { bodies_[oldest].fading = true; bodies_[oldest].fadeRate = 0.6f; }

    Body b{};
    b.id = nextBody_++;
    b.type = type;
    b.side = side;
    b.seed = seeds_[std::clamp(y, 0, Game::H - 1)][std::clamp(x, 0, Game::W - 1)];
    b.half = HEAP_HALF;
    const vec3 p = layout::cellPos((float)x, (float)y);
    b.x = b.x0 = p.x;
    b.y = b.y0 = p.y;
    b.wet = 0.6f;
    b.fade = 1.f;
    b.solid = true;
    b.thrown = true;
    b.t = 0.f;
    b.delay = delay;
    // a real ballistic flight whose apex clears the heap all along the way (from the band's inner edge to the
    // target), landing on its top: a low row gets a stronger kick
    float pathTop = top;
    for (const Body& o : bodies_)
        if (!o.thrown && o.solid && o.side == side && dir * o.x < dir * tx + 0.9f) pathTop = std::max(pathTop, o.y + o.half);
    const float g = 22.f;
    const float apex = std::max(pathTop, p.y) + rng_.range(2.f, 3.f);
    const float vy = std::sqrt(2.f * g * (apex - p.y));
    const float land = top + 0.6f;
    const float T = vy / g + std::sqrt(2.f * std::max(0.1f, apex - land) / g);
    b.tx1 = (tx - p.x) / T;
    b.ty1 = vy;
    b.w = rng_.range(4.f, 11.f) * (rng_.chance(0.5f) ? -1.f : 1.f);
    bodies_.push_back(b);
}

void RainUniverse::buildBins() {
    for (auto& v : bins_) v.clear();
    binX0_ = -48.f;
    for (int i = 0; i < (int)bodies_.size(); i++) {
        const Body& b = bodies_[i];
        if (!b.solid || b.thrown) continue;
        const int c0 = std::clamp((int)std::floor(b.x - b.half * 1.42f - binX0_), 0, 95);
        const int c1 = std::clamp((int)std::floor(b.x + b.half * 1.42f - binX0_), 0, 95);
        for (int c = c0; c <= c1; c++) bins_[c].push_back(i);
    }
}

namespace {
struct Contact {
    int i, j;             // body indices (j < 0: the street or a wall)
    float px, py, nx, ny; // the point, the normal (from j to i)
    float depth, pn, pt, bias, kn, kt;
};
inline float cross2(float ax, float ay, float bx, float by) { return ax * by - ay * bx; }
} // namespace

// One step of the boxes: gravity, contacts (box-box by SAT and clipping, corners against the street and the band's walls), a few
// iterations of sequential impulses with friction, sleeping. A sleeping box is static for the others; one hit
// hard wakes up.
void RainUniverse::physicsStep(float h) {
    const int N = (int)bodies_.size();
    auto live = [&](int i) { const Body& b = bodies_[i]; return b.solid && !b.thrown; };
    for (Body& b : bodies_)
        if (!b.thrown && b.solid && !b.asleep) b.vy -= 22.f * h;
    std::vector<Contact> cs;
    auto corners = [&](const Body& b, float P[4][2]) {
        const float c = std::cos(b.a), s = std::sin(b.a), q = b.half;
        const float L[4][2] = {{-q, -q}, {q, -q}, {q, q}, {-q, q}};
        for (int k = 0; k < 4; k++) {
            P[k][0] = b.x + c * L[k][0] - s * L[k][1];
            P[k][1] = b.y + s * L[k][0] + c * L[k][1];
        }
    };
    // Two rotated boxes (SAT): the axis of least penetration among both boxes' face normals gives the reference
    // face; the incident box's most anti-parallel edge is clipped to the reference face's side planes, each clipped
    // point below the face a contact (up to two), its normal from the reference box to the incident one.
    auto boxBox = [&](int ia, int ib) {
        const Body& A = bodies_[ia];
        const Body& B = bodies_[ib];
        const float ax[2][2] = {{std::cos(A.a), std::sin(A.a)}, {-std::sin(A.a), std::cos(A.a)}};
        const float bx[2][2] = {{std::cos(B.a), std::sin(B.a)}, {-std::sin(B.a), std::cos(B.a)}};
        const float dx = B.x - A.x, dy = B.y - A.y;
        float bestSep = -1e9f;
        int ref = -1, axis = 0;
        for (int k = 0; k < 4; k++) {
            const float* u = k < 2 ? ax[k] : bx[k - 2];
            const float hu = k < 2 ? A.half : B.half;        // the box owning the axis
            const float* o0 = k < 2 ? bx[0] : ax[0];
            const float* o1 = k < 2 ? bx[1] : ax[1];
            const float ho = k < 2 ? B.half : A.half;
            const float sep = std::fabs(dx * u[0] + dy * u[1]) - hu -
                              ho * (std::fabs(o0[0] * u[0] + o0[1] * u[1]) + std::fabs(o1[0] * u[0] + o1[1] * u[1]));
            if (sep > 0.f) return;
            if (sep > bestSep + (k < 2 ? 0.f : 1e-3f)) { bestSep = sep; ref = k < 2 ? 0 : 1; axis = k & 1; }
        }
        const int ri = ref == 0 ? ia : ib, ii = ref == 0 ? ib : ia;
        const Body& R = bodies_[ri];
        const Body& I = bodies_[ii];
        const float (*rx)[2] = ref == 0 ? ax : bx;
        const float (*iv)[2] = ref == 0 ? bx : ax;
        float nx = rx[axis][0], ny = rx[axis][1];
        if ((I.x - R.x) * nx + (I.y - R.y) * ny < 0.f) { nx = -nx; ny = -ny; }
        // the incident face: its normal most against n
        int fk = 0;
        float fs = 1e9f, fsg = 1.f;
        for (int k = 0; k < 2; k++)
            for (int sg = -1; sg <= 1; sg += 2) {
                const float dd = sg * (iv[k][0] * nx + iv[k][1] * ny);
                if (dd < fs) { fs = dd; fk = k; fsg = (float)sg; }
            }
        const float fnx = fsg * iv[fk][0], fny = fsg * iv[fk][1];   // the face normal
        const float tx = iv[1 - fk][0], ty = iv[1 - fk][1];           // along the face
        const float cx = I.x + fnx * I.half, cy = I.y + fny * I.half;
        float v[2][2] = {{cx - tx * I.half, cy - ty * I.half}, {cx + tx * I.half, cy + ty * I.half}};
        // clip against the reference face's side planes
        const float sx = -ny, sy = nx;
        const float sc = R.x * sx + R.y * sy;
        for (int side = -1; side <= 1; side += 2) {
            const float px_ = side * sx, py_ = side * sy, off = side * sc + R.half;
            const float d0 = v[0][0] * px_ + v[0][1] * py_ - off, d1 = v[1][0] * px_ + v[1][1] * py_ - off;
            if (d0 > 0.f && d1 > 0.f) return;
            if (d0 > 0.f || d1 > 0.f) {
                const float t = d0 / (d0 - d1);
                const float qx = v[0][0] + (v[1][0] - v[0][0]) * t, qy = v[0][1] + (v[1][1] - v[0][1]) * t;
                if (d0 > 0.f) { v[0][0] = qx; v[0][1] = qy; } else { v[1][0] = qx; v[1][1] = qy; }
            }
        }
        const float front = R.x * nx + R.y * ny + R.half;
        for (auto& p : v) {
            const float sep = p[0] * nx + p[1] * ny - front;
            if (sep <= 0.f) cs.push_back({ii, ri, p[0], p[1], nx, ny, -sep, 0, 0, 0, 0, 0});
        }
    };
    for (int i = 0; i < N; i++) {
        if (!live(i) || bodies_[i].asleep) continue;
        const Body& A = bodies_[i];
        float P[4][2];
        corners(A, P);
        const float dir = A.side == 0 ? -1.f : 1.f;
        if (!A.inBand && dir * A.x > bandIn_ + A.half * 1.5f) bodies_[i].inBand = true;
        if (!A.inBand && !A.fading && A.y < ground_ + 1.2f && std::fabs(A.x) > 5.6f) { bodies_[i].fading = true; bodies_[i].fadeRate = 1.5f; } // fell short
        const float outW = A.side == 0 ? leftOut_ : bandOut_;
        for (auto& c : P) {
            if (c[1] < ground_) cs.push_back({i, -1, c[0], c[1], 0.f, 1.f, ground_ - c[1], 0, 0, 0, 0, 0});
            const float in = dir * c[0] - bandIn_, out = outW - dir * c[0];
            if (!A.inBand) continue;
            if (in < 0.f) cs.push_back({i, -1, c[0], c[1], dir, 0.f, -in, 0, 0, 0, 0, 0});
            if (out < 0.f) cs.push_back({i, -1, c[0], c[1], -dir, 0.f, -out, 0, 0, 0, 0, 0});
        }
        for (int j = 0; j < N; j++) {
            if (j == i || !live(j)) continue;
            const Body& B = bodies_[j];
            if (!B.asleep && j < i) continue; // awake pairs once
            const float dx = B.x - A.x, dy = B.y - A.y, r = (A.half + B.half) * 1.42f;
            if (dx * dx + dy * dy > r * r) continue;
            boxBox(i, j);
        }
    }
    // masses (a sleeping box is static), a hard hit wakes it
    auto invM = [&](int i) { return i < 0 ? 0.f : 1.f; };
    auto invI = [&](int i) { return i < 0 ? 0.f : 6.f / (4.f * bodies_[i].half * bodies_[i].half); };
    auto velAt = [&](int i, float px, float py, float& vx, float& vy) {
        if (i < 0) { vx = vy = 0.f; return; }
        const Body& b = bodies_[i];
        vx = b.vx - b.w * (py - b.y);
        vy = b.vy + b.w * (px - b.x);
    };
    for (Contact& c : cs) {
        float vix, viy, vjx, vjy;
        velAt(c.i, c.px, c.py, vix, viy);
        velAt(c.j, c.px, c.py, vjx, vjy);
        const float vn = (vix - vjx) * c.nx + (viy - vjy) * c.ny;
        c.bias = 0.2f / h * std::max(0.f, c.depth - 0.004f);
        c.bias = std::min(c.bias, 1.f); // (a gentle push out, never a shove)
        if (vn < -1.5f) c.bias = std::max(c.bias, -0.15f * vn); // a small bounce
    }
    for (Contact& c : cs) {
        const float tx = -c.ny, ty = c.nx;
        float rn = 0.f, rt = 0.f, k = invM(c.i) + invM(c.j), kt = k;
        for (int s = 0; s < 2; s++) {
            const int b = s == 0 ? c.i : c.j;
            if (b < 0) continue;
            const float rx = c.px - bodies_[b].x, ry = c.py - bodies_[b].y;
            rn = cross2(rx, ry, c.nx, c.ny);
            rt = cross2(rx, ry, tx, ty);
            k += invI(b) * rn * rn;
            kt += invI(b) * rt * rt;
        }
        c.kn = k > 1e-6f ? 1.f / k : 0.f;
        c.kt = kt > 1e-6f ? 1.f / kt : 0.f;
    }
    auto apply = [&](int b, float sgn, float px, float py, float jx, float jy) {
        if (b < 0) return;
        Body& B = bodies_[b];
        const float m = invM(b), I = invI(b);
        B.vx += sgn * m * jx;
        B.vy += sgn * m * jy;
        B.w += sgn * I * cross2(px - B.x, py - B.y, jx, jy);
    };
    for (int it = 0; it < 10; it++)
        for (Contact& c : cs) {
            float vix, viy, vjx, vjy;
            velAt(c.i, c.px, c.py, vix, viy);
            velAt(c.j, c.px, c.py, vjx, vjy);
            const float rvx = vix - vjx, rvy = viy - vjy;
            const float vn = rvx * c.nx + rvy * c.ny;
            float dp = c.kn * (-vn + c.bias);
            const float p0 = c.pn;
            c.pn = std::max(0.f, p0 + dp);
            dp = c.pn - p0;
            apply(c.i, 1.f, c.px, c.py, c.nx * dp, c.ny * dp);
            apply(c.j, -1.f, c.px, c.py, c.nx * dp, c.ny * dp);
            const float tx = -c.ny, ty = c.nx;
            velAt(c.i, c.px, c.py, vix, viy);
            velAt(c.j, c.px, c.py, vjx, vjy);
            const float vt = (vix - vjx) * tx + (viy - vjy) * ty;
            float dt = -c.kt * vt;
            const float t0 = c.pt, lim = 0.7f * c.pn;
            c.pt = std::clamp(t0 + dt, -lim, lim);
            dt = c.pt - t0;
            apply(c.i, 1.f, c.px, c.py, tx * dt, ty * dt);
            apply(c.j, -1.f, c.px, c.py, tx * dt, ty * dt);
        }
    // a sleeping tile that the contacts set moving for real wakes; otherwise it stays put
    for (Body& b : bodies_) {
        if (!b.asleep) continue;
        if (b.vx * b.vx + b.vy * b.vy > 0.3f * 0.3f || std::fabs(b.w) > 0.6f) { b.asleep = false; b.still = b.awake = 0.f; }
        else b.vx = b.vy = b.w = 0.f;
    }
    for (Body& b : bodies_) {
        if (b.thrown || !b.solid || b.asleep) continue;
        b.vx *= std::exp(-0.3f * h);
        b.w *= std::exp(-1.2f * h);
        b.x += b.vx * h;
        b.y += b.vy * h;
        b.a += b.w * h;
        const bool still = b.vx * b.vx + b.vy * b.vy < 0.06f * 0.06f && std::fabs(b.w) < 0.12f;
        b.still = still ? b.still + h : 0.f;
        b.awake += h;
        if (b.still > 0.6f || b.awake > 20.f) { b.asleep = true; b.vx = b.vy = b.w = 0.f; }
    }
}

void RainUniverse::stepHeaps(float dt) {
    // the throws: each tile waits its turn in its row, then gets its impulse (out, up, a spin) and flies
    for (Body& b : bodies_) {
        if (!b.thrown) continue;
        b.t += dt;
        if (b.t < b.delay) continue;
        b.thrown = false;
        b.vx = b.tx1;
        b.vy = b.ty1;
        b.awake = -1.5f; // (no sleep before it lands)
    }
    // the boxes, at a fixed step
    physAcc_ = std::min(physAcc_ + dt, 8.f / 240.f);
    while (physAcc_ >= 1.f / 240.f) {
        physicsStep(1.f / 240.f);
        physAcc_ -= 1.f / 240.f;
    }
    // fading tiles sink away; once gone, what rested on them falls again
    for (Body& b : bodies_) {
        b.wet = std::max(0.2f, b.wet - dt * 0.08f);
        if (!b.fading) continue;
        b.fade = std::max(0.f, b.fade - dt * b.fadeRate);
        if (b.solid && b.fade < 0.12f) {
            b.solid = false;
            for (Body& o : bodies_)
                if (o.side == b.side && o.asleep && o.y > b.y - 0.3f && std::fabs(o.x - b.x) < 3.5f) { // what rested on it settles again
                    o.asleep = false;
                    o.still = o.awake = 0.f;
                }
        }
    }
    bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(), [](const Body& b) { return b.fading && b.fade <= 0.f; }), bodies_.end());
    buildBins();
    // the rain soaks the heaped tiles like cardboard: each drop and the steady rain soften them; a soggy tile goes
    // limp (its turn relaxes toward lying flat) and is crushed thinner under the ones above, so the heap slumps and
    // packs down little by little
    for (Body& b : bodies_) {
        if (!b.solid || b.thrown) { b.hits = 0; continue; }
        b.soak = std::min(1.f, b.soak + b.hits * 0.004f + dt * 0.012f * rain_);
        b.hits = 0;
        const float half = HEAP_HALF * (1.f - 0.32f * smoothstepf(0.f, 1.f, b.soak));
        if (half < b.half) { b.slump += b.half - half; b.half = half; }
        if (b.soak > 0.25f) {
            const float flat = std::round(b.a / (PI * 0.5f)) * (PI * 0.5f);
            const float da = (flat - b.a) * std::min(1.f, dt * 0.25f * b.soak);
            b.a += da;
            b.slump += std::fabs(da) * b.half;
        }
        if (b.asleep && b.slump > 0.02f) { b.asleep = false; b.still = b.awake = 0.f; } // it sinks onto what holds it
        if (!b.asleep) b.slump = 0.f;
    }
    // a sleeping tile left without support (what held it went away) wakes and falls again
    for (Body& b : bodies_) {
        if (!b.asleep || !b.solid) continue;
        const float c = std::cos(b.a), s = std::sin(b.a), q = b.half;
        float lx = 0.f, ly = 1e9f;
        for (int k = 0; k < 4; k++) {
            const float ux = (k & 1) ? q : -q, uy = (k & 2) ? q : -q;
            const float x = b.x + c * ux - s * uy, y = b.y + s * ux + c * uy;
            if (y < ly) { ly = y; lx = x; }
        }
        bool held = ly - ground_ < 0.06f;
        for (const Body& o : bodies_)
            if (!held && &o != &b && o.solid && !o.thrown && std::fabs(o.x - lx) < 1.f && std::fabs(o.y - ly) < 1.f)
                held = bodyInside(o, lx, ly - 0.06f) || bodyInside(o, lx - 0.15f, ly - 0.06f) || bodyInside(o, lx + 0.15f, ly - 0.06f);
        if (!held) { b.asleep = false; b.still = b.awake = 0.f; }
    }
    // fallen out of the picture (a wall missed at a high speed): gone
    bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(), [&](const Body& b) { return b.y < ground_ - 3.f; }), bodies_.end());
}

void RainUniverse::drawHeaps() {
    const vec3 bInk = ugl::toSrgb(bridge_.ink);
    const float bk = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    for (const Body& b : bodies_) {
        float a = b.fading ? b.fade : 1.f;
        a *= 1.f - 0.6f * hudCover(b.x, b.y);   // behind the HUD's columns
        a *= 1.f - 0.75f * textCover(b.x, b.y); // and its corner lines
        const bool waiting = b.thrown;
        const vec3 c = lerp(pieceColor(b.type), bInk, bk);
        if (!waiting) a *= 1.f - smoothstepf(0.f, 0.6f, bridge_.amount);
        if (a <= 0.002f) continue;
        const vec2 p = px(b.x, b.y);
        // a soggy tile is crushed: as tall as its box, a little wider than it (bulging over its neighbours)
        const float hy = (waiting ? 0.5f - GAP : b.half - 0.01f) * unitPx_;
        const float hx = (waiting ? 0.5f - GAP : HEAP_HALF * (1.f + 0.1f * b.soak) - 0.01f) * unitPx_;
        tiles_.push_back({{p.x, p.y, hx, hy}, {c.x, c.y, c.z, a}, {b.seed, b.wet, 0.f, b.a}, {0.f, 0.f, 0.f, 0.f},
                          {0.f, waiting ? 0.f : 1.f, waiting ? 0.f : b.soak, 0.f}});
    }
    flushTiles();
    // the storm floods the street: the water rises around the heaps
    if (flood_ > 0.01f) {
        const float top = ground_ + 0.15f + 0.9f * flood_;
        for (int side = 0; side < 2; side++) {
            const float dir = side == 0 ? -1.f : 1.f;
            const vec2 a = px(dir * (bandIn_ - 0.6f), top), b2 = px(dir * (viewH_.x + 1.f), ground_ - 2.f);
            const VariantDef& V = VARIANTS[variant_];
            const vec3 water = lerp(hex(V.street), hex(V.mist), 0.35f);
            box(0.5f * (a.x + b2.x), 0.5f * (a.y + b2.y), 0.5f * std::fabs(b2.x - a.x), 0.5f * (b2.y - a.y), water, 0.55f * flood_);
            box(0.5f * (a.x + b2.x), a.y, 0.5f * std::fabs(b2.x - a.x), 1.f, lerp(hex(V.neon[side]), vec3(1.f), 0.5f), 0.35f * flood_);
        }
    }
    flushShapes();
}

// =====================================================================================================
// The foreground: tiles thrown toward the camera, melting in the rain beside the well
// =====================================================================================================

namespace {
constexpr int FORE_CAP = 5;          // per side, at once (the oldest melts faster beyond it)
constexpr float FORE_WELL = 5.3f;    // the glass case's outer edge (board units): the foreground stays out of it
constexpr float FORE_OUT = 14.5f;    // the foreground's outer limit (in front of the heaps)
constexpr float FORE_DEPTH = 0.34f;  // the cube's visible depth (of its half size)
} // namespace

bool RainUniverse::foreTake(int side) {
    int n = 0;
    for (const Fore& f : fores_)
        if (f.side == side && f.melt < 0.7f) n++;
    return n < FORE_CAP;
}

void RainUniverse::ejectFore(int x, int y, int type, float delay) {
    Fore f{};
    f.type = type;
    f.side = x < Game::W / 2 ? 0 : 1;
    f.seed = seeds_[std::clamp(y, 0, Game::H - 1)][std::clamp(x, 0, Game::W - 1)];
    f.half = 0.5f - GAP;
    const vec3 p = layout::cellPos((float)x, (float)y);
    f.x = f.x0 = p.x;
    f.y = f.y0 = p.y;
    f.s = 1.f;
    f.sMax = rng_.range(1.3f, 1.5f);
    f.delay = delay;
    f.wet = 0.7f;
    f.rate = 1.f / rng_.range(8.5f, 11.f);
    fores_.push_back(f);
}

// Half the tile's width as drawn at scale s (its turn, the cube's depth toward the vanishing point included).
float RainUniverse::foreExt(const Fore& f, float s) const {
    const float h = f.half * s;
    return h * (std::fabs(std::cos(f.a)) + std::fabs(std::sin(f.a))) + FORE_DEPTH * h;
}

// Keeps a foreground tile (its center x, its half extent) out of the glass case and clear of the HUD: the lettering
// beside it, and above the columns' bottoms the previews and the bonus ring.
float RainUniverse::foreClampX(int side, float x, float y, float ext) const {
    const float dir = side == 0 ? -1.f : 1.f;
    const float lo = y - ext < 10.4f && y + ext > -10.35f ? FORE_WELL + ext : 0.f;
    float hi = FORE_OUT - ext;
    const float colBottom = side == 0 ? layout::LEFT_COLUMN[1] : layout::RIGHT_COLUMN[1];
    if (y + ext > colBottom - 0.2f) hi = std::min(hi, 7.25f - ext);
    for (const vec4& r : hudRects_) {
        if (dir * (r.x + r.z) < 0.f) continue;                     // the other side
        if (r.y > y + ext + 0.2f || r.w < y - ext - 0.2f) continue; // not at this height
        const float inner = side == 0 ? -r.z : r.x;
        hi = std::min(hi, inner - 0.2f - ext);
    }
    float ax = dir * x;
    ax = std::min(ax, hi);
    ax = std::max(ax, lo); // (the well wins)
    return dir * ax;
}

void RainUniverse::foreSplash(float x, float y, float s, float strength, const vec3& col) {
    (void)col;
    const int n = (int)(14 + 40 * strength);
    for (int i = 0; i < n && fsparks_.size() < 900; i++) {
        const float a = rng_.range(0.15f, PI - 0.15f);
        const float sp = rng_.range(3.f, 9.f) * (0.45f + 0.7f * strength) * std::sqrt(s);
        fsparks_.push_back({x + rng_.range(-0.6f, 0.6f) * s * 0.5f, y + 0.05f, std::cos(a) * sp * 0.7f, std::sin(a) * sp,
                            0.f, rng_.range(0.35f, 0.75f), rng_.range(0.03f, 0.07f) * s});
    }
    // the crown: a ring of big drops thrown up and out around the impact
    const int nc = (int)(8 + 14 * strength);
    for (int i = 0; i < nc && fsparks_.size() < 900; i++) {
        const float side = i & 1 ? 1.f : -1.f;
        const float sp = rng_.range(4.f, 8.f) * (0.5f + 0.6f * strength) * std::sqrt(s);
        fsparks_.push_back({x + side * s * rng_.range(0.3f, 0.55f), y + 0.05f, side * sp * rng_.range(0.25f, 0.6f), sp, 0.f,
                            rng_.range(0.5f, 0.8f), rng_.range(0.07f, 0.12f) * s});
    }
    fcrowns_.push_back({x, y, 0.f, (0.9f + 0.8f * strength) * s});
    fcrowns_.push_back({x, y - 0.05f, -0.06f, (1.4f + 1.f * strength) * s});
}

void RainUniverse::stepFore(float dt, float rdt) {
    const float g = 22.f;
    foreGround_ = viewC_.y - viewH_.y + 0.5f;
    // the cap: beyond it the oldest on that side melts away fast
    int count[2] = {0, 0};
    for (int i = (int)fores_.size() - 1; i >= 0; i--) {
        Fore& f = fores_[i];
        if (f.melt >= 1.f) continue;
        if (++count[f.side] > FORE_CAP) f.rate = std::max(f.rate, 0.45f);
    }
    for (Fore& f : fores_) {
        const float dir = f.side == 0 ? -1.f : 1.f;
        f.t += dt;
        switch (f.stage) {
        case 0:
            if (f.t >= f.delay) {
                f.stage = 1;
                f.t = 0.f;
                f.xOut = dir * (FORE_WELL + foreExt(f, 1.f) + 0.05f);
            }
            break;
        case 1: {
            // out through the glass, a little hop
            const float e = std::min(1.f, f.t / 0.12f);
            f.x = lerpf(f.x0, f.xOut, e);
            f.y = f.y0 + 0.3f * std::sin(PI * e);
            if (e >= 1.f) {
                f.stage = 2;
                f.t = 0.f;
                f.vx = dir * rng_.range(0.5f, 1.3f);
                f.vy = rng_.range(1.f, 3.f);
                f.w = -dir * rng_.range(1.f, 2.4f);
            }
            break;
        }
        case 2: {
            // falling along the well's outer wall, coming closer (larger) as it comes down
            f.vy -= g * dt;
            f.x += f.vx * dt;
            f.y += f.vy * dt;
            f.a += f.w * dt;
            const float colBottom = f.side == 0 ? layout::LEFT_COLUMN[1] : layout::RIGHT_COLUMN[1];
            const float target = lerpf(1.f, f.sMax, smoothstepf(colBottom + 0.5f, foreGround_ + 2.2f, f.y));
            if (target > f.s) f.s += (target - f.s) * std::min(1.f, dt * 10.f);
            const float ext = foreExt(f, f.s);
            const float nx = foreClampX(f.side, f.x, f.y, ext);
            if (nx != f.x) { f.x = nx; f.vx *= 0.5f; }
            const float ey = f.half * f.s * (std::fabs(std::cos(f.a)) + std::fabs(std::sin(f.a)));
            if (f.vy < 0.f && f.y - ey <= foreGround_) {
                f.y = foreGround_ + ey;
                foreSplash(f.x, foreGround_, f.s, std::min(1.f, -f.vy / 14.f), pieceColor(f.type));
                // the bounce: out to its own spot, landing flat (or nearly)
                const float extF = f.half * f.sMax * 1.15f + FORE_DEPTH * f.half * f.sMax;
                float best = -1.f, tx = f.x;
                for (int k = 0; k < 10; k++) {
                    float cand = dir * rng_.range(FORE_WELL + extF, FORE_OUT - extF);
                    cand = foreClampX(f.side, cand, foreGround_ + extF * 0.9f, extF);
                    float sc = 9.f;
                    for (const Fore& o : fores_)
                        if (&o != &f && o.side == f.side && o.stage >= 3 && o.melt < 0.9f) sc = std::min(sc, std::fabs(cand - o.tx));
                    sc += rng_.range(0.f, 0.4f);
                    if (sc > best) { best = sc; tx = cand; }
                }
                const float vy = std::clamp(-f.vy * 0.32f, 2.6f, 6.f);
                f.T = 2.f * vy / g;
                f.vy = vy;
                f.x0 = f.x;
                f.y0 = f.y;
                f.a0 = f.a;
                f.tx = tx;
                const float a1 = f.a + f.w * f.T * 1.5f;
                f.aT = std::round(a1 / (PI * 0.5f)) * (PI * 0.5f) + rng_.range(-0.13f, 0.13f);
                f.stage = 3;
                f.t = 0.f;
            }
            break;
        }
        case 3: {
            const float u = std::min(1.f, f.t / f.T);
            const float tilt = f.aT - std::round(f.aT / (PI * 0.5f)) * (PI * 0.5f);
            const float yRest = foreGround_ + f.half * f.sMax * (std::cos(tilt) + std::fabs(std::sin(tilt)));
            f.s += (f.sMax - f.s) * std::min(1.f, dt * 8.f);
            f.x = lerpf(f.x0, f.tx, u);
            f.y = u < 1.f ? lerpf(f.y0, yRest, u) + f.vy * f.t - 0.5f * g * f.t * f.t : yRest;
            f.a = lerpf(f.a0, f.aT, ease(u));
            f.x = foreClampX(f.side, f.x, f.y, foreExt(f, f.s));
            if (u >= 1.f) {
                f.stage = 4;
                f.t = 0.f;
                f.wob = 1.f;
                f.s = f.sMax;
                foreSplash(f.x, foreGround_, f.s, 0.35f, pieceColor(f.type));
            }
            break;
        }
        default: {
            // melting in the rain
            f.wob = approach(f.wob, 0.f, 4.f, dt);
            if (f.melt < 1.f) {
                f.melt = std::min(1.f, f.melt + f.rate * dt * (0.7f + 0.6f * rain_));
                f.wet = std::min(1.f, f.wet + dt * 0.2f);
            } else f.gone += dt * 0.7f;
            // the rain hits its top: little splashes, bigger than the far ones (it is closer)
            const float m = f.melt;
            const float hx = f.half * f.s * (1.f + 0.35f * smoothstepf(0.3f, 1.f, m));
            const float hy = f.half * f.s * (1.f - 0.72f * smoothstepf(0.3f, 1.f, m));
            if (m < 0.97f) {
                f.rainAcc += rdt * (2.f + 9.f * rain_) * f.s * shower_;
                while (f.rainAcc >= 1.f && fsparks_.size() < 900) {
                    f.rainAcc -= 1.f;
                    const float x = f.x + rng_.range(-0.9f, 0.9f) * hx, y = foreGround_ + 2.f * hy - 0.02f;
                    for (int i = 0; i < 2; i++)
                        fsparks_.push_back({x, y, rng_.range(-1.5f, 1.5f), rng_.range(1.f, 3.f), 0.f, rng_.range(0.2f, 0.35f), 0.022f * f.s});
                    if (rng_.chance(0.4f)) fcrowns_.push_back({x, y, 0.2f, 0.16f * f.s}); // (a short one)
                }
            }
            // crumbs falling off its soft edges
            if (m > 0.22f && m < 0.93f && crumbs_.size() < 260) {
                f.crumbAcc += dt * (2.f + 9.f * smoothstepf(0.22f, 0.6f, m));
                while (f.crumbAcc >= 1.f) {
                    f.crumbAcc -= 1.f;
                    const float sx = rng_.chance(0.5f) ? -1.f : 1.f;
                    Crumb c{};
                    c.x = f.x + sx * hx * rng_.range(0.75f, 1.f);
                    c.y = foreGround_ + hy * rng_.range(0.6f, 1.9f);
                    c.vx = sx * rng_.range(0.3f, 1.4f);
                    c.vy = rng_.range(-0.5f, 1.2f);
                    c.w = rng_.range(-6.f, 6.f);
                    c.r = f.half * f.s * rng_.range(0.1f, 0.22f);
                    c.life = rng_.range(1.6f, 3.f);
                    c.type = f.type;
                    c.seed = rng_.range(0.f, 100.f);
                    crumbs_.push_back(c);
                }
            }
            break;
        }
        }
    }
    fores_.erase(std::remove_if(fores_.begin(), fores_.end(), [](const Fore& f) { return f.gone >= 1.f; }), fores_.end());
    for (size_t i = 0; i < crumbs_.size();) {
        Crumb& c = crumbs_[i];
        c.t += dt;
        if (!c.rest) {
            c.vy -= g * dt;
            c.x += c.vx * dt;
            c.y += c.vy * dt;
            c.a += c.w * dt;
            if (c.y - c.r < foreGround_) {
                c.y = foreGround_ + c.r * 0.7f;
                if (std::fabs(c.vy) > 2.f && fsparks_.size() < 900)
                    for (int k = 0; k < 3; k++)
                        fsparks_.push_back({c.x, foreGround_, rng_.range(-1.5f, 1.5f), rng_.range(1.f, 2.5f), 0.f, 0.3f, 0.2f * c.r});
                c.rest = true;
            }
        }
        if (c.t > c.life) { crumbs_[i] = crumbs_.back(); crumbs_.pop_back(); continue; }
        i++;
    }
    auto inWell = [](float x, float y) { return std::fabs(x) < FORE_WELL && y > -10.3f && y < 10.4f; };
    for (size_t i = 0; i < fsparks_.size();) {
        Spark& s = fsparks_[i];
        s.t += dt;
        s.vy -= g * dt;
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        if (s.t > s.life || inWell(s.x, s.y) || (s.y < foreGround_ - 0.1f && s.vy < 0.f)) { fsparks_[i] = fsparks_.back(); fsparks_.pop_back(); continue; }
        i++;
    }
    for (size_t i = 0; i < fcrowns_.size();) {
        fcrowns_[i].t += dt;
        if (fcrowns_[i].t > 0.4f) { fcrowns_[i] = fcrowns_.back(); fcrowns_.pop_back(); continue; }
        i++;
    }
    // the near rain: a few big streaks right in front of the lens, out of focus
    {
        const int want = (int)((6.f + 26.f * rain_) * shower_ * (1.f - bridge_.amount));
        while ((int)nearRain_.size() < want)
            nearRain_.push_back({rng_.range(-viewH_.x, viewH_.x), viewC_.y + viewH_.y + rng_.range(0.f, 25.f), rng_.range(38.f, 52.f),
                                 rng_.range(2.5f, 4.5f), rng_.range(3.f, 6.f), rng_.range(0.04f, 0.09f)});
        if ((int)nearRain_.size() > want) nearRain_.resize(std::max(0, want));
        const float slant = VARIANTS[variant_].slant + 0.22f * storm_;
        for (Streak& s : nearRain_) {
            s.y -= s.v * rdt;
            s.x -= slant * s.v * rdt;
            if (s.y + s.len < viewC_.y - viewH_.y) {
                s.y = viewC_.y + viewH_.y + rng_.range(0.f, 6.f);
                s.x = rng_.range(-viewH_.x, viewH_.x + slant * 26.f);
            }
        }
    }
}

void RainUniverse::drawFore() {
    const VariantDef& V = VARIANTS[variant_];
    const vec3 bInk = ugl::toSrgb(bridge_.ink);
    const float bk = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float vis = 1.f - smoothstepf(0.f, 0.6f, bridge_.amount);
    const float s1080 = height_ / 1080.f;
    const vec2 vp = px(0.f, 1.5f); // the vanishing point: the cubes' depth recedes toward it
    auto shape = [&](const Fore& f, float& hx, float& hy, float& cx, float& cy, float& ang) {
        const float m = f.melt;
        hx = f.half * f.s * (1.f + 0.35f * smoothstepf(0.3f, 1.f, m));
        hy = f.half * f.s * (1.f - 0.72f * smoothstepf(0.3f, 1.f, m));
        cx = f.x;
        cy = f.y;
        ang = f.a;
        if (f.stage == 4) {
            const float flat = std::round(f.aT / (PI * 0.5f)) * (PI * 0.5f);
            const float tilt = (f.aT - flat) * (1.f - smoothstepf(0.2f, 0.75f, m)) + f.wob * 0.12f * std::sin(f.t * 22.f);
            ang = flat + tilt;
            cy = foreGround_ + hy * std::cos(tilt) + hx * std::fabs(std::sin(tilt)) - 0.04f * f.s * smoothstepf(0.5f, 1.f, m);
        }
    };
    // the puddles of melted sugar, the contact shadows
    for (const Fore& f : fores_) {
        if (f.stage < 2) continue;
        float hx, hy, cx, cy, ang;
        shape(f, hx, hy, cx, cy, ang);
        const vec3 col = lerp(pieceColor(f.type), bInk, bk);
        const float lift = saturate((cy - hy - foreGround_) / 4.f);
        const vec2 g = px(cx, foreGround_ - 0.04f);
        const float sw = f.half * f.s * 1.3f * unitPx_;
        disc(g.x, g.y, sw * (1.f + lift), sw * 0.2f * (1.f + lift), vec3(0.f), 0.5f * (1.f - lift) * vis * (1.f - f.gone), 0.8f);
        if (f.stage == 4) {
            const float m = f.melt;
            const float rx = f.half * f.s * (1.f + 1.8f * smoothstepf(0.05f, 0.95f, m)) * unitPx_;
            const float a = smoothstepf(0.03f, 0.25f, m) * (1.f - f.gone) * vis;
            const vec3 mush = lerp(hex(V.street), col * 0.75f, 0.7f);
            disc(g.x, g.y + 0.04f * unitPx_, rx, rx * 0.2f, mush, 0.75f * a, 0.35f);
            disc(g.x, g.y - 0.02f * unitPx_, rx * 0.7f, rx * 0.08f, lerp(col, vec3(1.f), 0.6f), 0.18f * a, 1.f);
        }
    }
    flushShapes();
    // the crumbs, the tiles
    for (const Crumb& c : crumbs_) {
        const vec3 col = lerp(pieceColor(c.type), bInk, bk);
        const float k = c.t / c.life;
        const vec2 p = px(c.x, c.y);
        const float r = c.r * unitPx_ * (1.f - 0.3f * k);
        tiles_.push_back({{p.x, p.y, r, r * (c.rest ? 1.f - 0.5f * k : 1.f)}, {col.x, col.y, col.z, vis * (1.f - smoothstepf(0.7f, 1.f, k))},
                          {c.seed, 0.9f, 0.f, c.a}, {0.f, 0.f, 0.45f + 0.5f * k, 1.f}, {0.f, 0.f, 0.6f, 0.f}});
    }
    for (const Fore& f : fores_) {
        if (f.melt >= 1.f) continue;
        float hx, hy, cx, cy, ang;
        shape(f, hx, hy, cx, cy, ang);
        const vec3 col = lerp(pieceColor(f.type), bInk, bk);
        const vec2 p = px(cx, cy);
        const float depth = f.stage == 0 ? 0.f : f.stage == 1 ? std::min(1.f, f.t / 0.12f) : 1.f;
        vec2 o{vp.x - p.x, vp.y - p.y};
        const float ol = std::max(1.f, std::sqrt(o.x * o.x + o.y * o.y));
        const float k = FORE_DEPTH * f.half * f.s * unitPx_ * depth * (1.f - 0.4f * f.melt);
        o = vec2{o.x / ol * k, o.y / ol * k};
        const float a = vis * (1.f - smoothstepf(0.86f, 1.f, f.melt));
        tiles_.push_back({{p.x, p.y, hx * unitPx_, hy * unitPx_}, {col.x, col.y, col.z, a}, {f.seed, f.wet, 0.f, ang},
                          {o.x, o.y, f.melt, 1.f}, {0.f, 0.f, std::min(1.f, f.melt * 1.4f), 0.f}});
    }
    flushTiles();
    // the splashes, in front of everything
    const vec3 tint = graded(lerp(lerp(hex(V.mist), vec3(0.95f, 0.97f, 1.f), 0.7f), vec3(0.85f, 0.92f, 1.f), flash_));
    for (const Spark& s : fsparks_) {
        const float k = 1.f - s.t / s.life;
        const vec2 a = px(s.x, s.y), b = px(s.x - s.vx * 0.05f, s.y - s.vy * 0.05f);
        seg(a.x, a.y, b.x, b.y, std::max(1.f, s.r * unitPx_ * 1.5f), tint, 0.85f * k * vis, 0.15f);
        disc(a.x, a.y, s.r * unitPx_ * 0.8f, s.r * unitPx_ * 0.8f, vec3(1.f), 0.6f * k * vis);
    }
    for (const Crown& c : fcrowns_) {
        if (c.t < 0.f) continue;
        const vec2 a = px(c.x, c.y);
        const float k = saturate(c.t / 0.4f);
        const float rx = (0.25f + 0.75f * std::sqrt(k)) * c.w * unitPx_, ry = std::max(1.f, rx * 0.2f);
        box(a.x, a.y - ry, rx, ry, tint, 0.55f * (1.f - k) * vis, ry, std::max(1.f, 1.6f * s1080));
        if (c.w > 0.75f && c.t < 0.32f) {
            // a big impact: a sheet of water thrown up around the cube, a fan of streams breaking into drops
            const float k2 = c.t / 0.32f;
            const float H = c.w * 0.8f * std::sin(PI * std::min(1.f, k2 * 1.25f));
            for (int j = 0; j < 12; j++) {
                const float u = j / 11.f * 2.f - 1.f;
                const float wob = std::sin(j * 7.3f + c.x * 3.f) * 0.12f;
                const vec2 b0 = px(c.x + u * c.w * 0.42f, c.y), b1 = px(c.x + (u + wob) * c.w * (0.6f + 0.4f * k2), c.y + H * (1.f - 0.55f * u * u));
                seg(b1.x, b1.y, b0.x, b0.y, std::max(1.5f, 3.2f * s1080 * (1.f - 0.5f * k2)), tint, 0.4f * (1.f - k2) * vis, 0.25f);
                disc(b1.x, b1.y, 2.6f * s1080, 2.6f * s1080, vec3(1.f), 0.5f * (1.f - k2) * vis);
            }
        }
    }
    // the near rain (never over the well or the HUD's columns)
    const float slant = V.slant + 0.22f * storm_;
    for (const Streak& s : nearRain_) {
        const float off = smoothstepf(FORE_WELL + 0.2f, FORE_WELL + 1.2f, std::fabs(s.x));
        const float a = s.a * off * (1.f - 0.8f * hudCover(s.x, s.y)) * vis * (1.f + 1.5f * flash_);
        if (a <= 0.003f) continue;
        const vec2 p0 = px(s.x, s.y), p1 = px(s.x + slant * s.len, s.y + s.len);
        seg(p0.x, p0.y, p1.x, p1.y, s.w * s1080, tint, a, 0.f);
    }
    flushShapes();
}

// =====================================================================================================
// Drawing primitives
// =====================================================================================================

vec2 RainUniverse::px(float x, float y) const {
    return {(x - viewC_.x) * unitPx_ + width_ * 0.5f, height_ * 0.5f - (y - viewC_.y) * unitPx_};
}

void RainUniverse::box(float cx, float cy, float hx, float hy, const vec3& c, float a, float round, float outline) {
    if (a <= 0.002f) return;
    shapes_.push_back({{cx, cy, hx, hy}, {c.x, c.y, c.z, a}, {0.f, round, outline, 1.f}});
}

void RainUniverse::disc(float cx, float cy, float rx, float ry, const vec3& c, float a, float soft) {
    if (a <= 0.002f) return;
    shapes_.push_back({{cx, cy, rx, ry}, {c.x, c.y, c.z, a}, {1.f, 0.f, 0.f, soft}});
}

void RainUniverse::seg(float x0, float y0, float x1, float y1, float w, const vec3& c, float a, float tail) {
    if (a <= 0.002f) return;
    shapes_.push_back({{x0, y0, x1, y1}, {c.x, c.y, c.z, a}, {2.f, w, 0.f, tail}});
}

void RainUniverse::tile(float bx, float by, float hx, float hy, const vec3& c, float a, float seed, float wet, bool plain,
                        float angle) {
    if (a <= 0.002f) return;
    const vec2 p = px(bx, by);
    const float g = GAP * unitPx_ * std::min(1.f, hx * 2.f);
    tiles_.push_back({{p.x, p.y, std::max(1.f, hx * unitPx_ - g), std::max(0.5f, hy * unitPx_ - g)}, {c.x, c.y, c.z, a},
                      {seed, wet, plain ? 1.f : 0.f, angle}, {0.f, 0.f, 0.f, 0.f}, {1.f, 0.f, 0.f, 0.f}});
}

void RainUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(Shape), shapes_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)shapes_.size());
    shapes_.clear();
}

void RainUniverse::flushTiles() {
    if (tiles_.empty()) return;
    const VariantDef& V = VARIANTS[variant_];
    glUseProgram(progTile_);
    set2f(progTile_, "uRes", (float)width_, (float)height_);
    set1f(progTile_, "uTime", rainT_);
    set3f(progTile_, "uSheen", graded(hex(V.neon[0])) * (1.f - bridge_.amount));
    set3f(progTile_, "uSheen2", graded(hex(V.neon[1])) * (1.f - bridge_.amount));
    set1f(progTile_, "uGlow", glow_);
    {
        const vec2 a = px(signPos_[0].x, signPos_[0].y), b = px(signPos_[1].x, signPos_[1].y);
        const float lp[4] = {a.x, a.y, b.x, b.y};
        glUniform2fv(ugl::U(progTile_, "uLightPx"), 2, lp);
        const vec3 c0 = graded(hex(V.neon[1])) * glow_ * neonGain_, c1 = graded(hex(V.neon[0])) * glow_ * neonGain_;
        set3f(progTile_, "uFlashRim", graded(vec3(0.75f, 0.83f, 1.f)) * flash_);
        const float lc[6] = {c0.x, c0.y, c0.z, c1.x, c1.y, c1.z};
        glUniform3fv(ugl::U(progTile_, "uLightCol"), 2, lc);
        set1f(progTile_, "uLightR", 6.f * unitPx_);
        set1f(progTile_, "uGroundPx", px(0.f, ground_).y);
    }
    glBindVertexArray(tileVao_);
    glBindBuffer(GL_ARRAY_BUFFER, tileVbo_);
    glBufferData(GL_ARRAY_BUFFER, tiles_.size() * sizeof(Tile), tiles_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)tiles_.size());
    tiles_.clear();
}

// =====================================================================================================
// The board
// =====================================================================================================

void RainUniverse::drawBoard(const Game& g) {
    const float br = saturate(bridge_.amount);
    const vec3 bInk = ugl::toSrgb(bridge_.ink);
    const float bk = smoothstepf(0.f, 1.f, br);
    auto tone = [&](const vec3& c) { return lerp(c, bInk, bk); };
    const float s = height_ / 1080.f;
    const VariantDef& V = VARIANTS[variant_];

    // the well: a wet glass case, its thick glass edges catching the neon, beads of water on them (some sliding down
    // slowly), drips hanging from the top edge and the floor's lip
    {
        const float th = std::max(1.f, std::round(1.4f * s));
        const float ft = 0.2f * unitPx_;                  // the glass edge's thickness (pixels)
        const vec2 a = px(-5.f, 10.f), b = px(5.f, -10.f);
        const vec3 c = lerp(vec3(0.78f, 0.82f, 0.9f), hex(V.neon[1]), 0.25f);
        const float al = 1.f - br;
        const float cy = 0.5f * (a.y + b.y), hy = 0.5f * (b.y - a.y) + ft;
        for (int side = 0; side < 2; side++) {
            const float x = side == 0 ? a.x - 0.5f * ft : b.x + 0.5f * ft;
            box(x, cy, 0.5f * ft, hy, c, 0.1f * al);                                  // the glass
            box(side == 0 ? a.x - th : b.x + th, cy, th * 0.5f, hy, c, 0.5f * al);   // its inner face
            box(side == 0 ? a.x - ft : b.x + ft, cy, th * 0.5f, hy, c, 0.28f * al);  // its outer face
        }
        box(0.5f * (a.x + b.x), b.y + 0.5f * ft, 0.5f * (b.x - a.x) + ft, 0.5f * ft, c, 0.12f * al);
        box(0.5f * (a.x + b.x), b.y + th, 0.5f * (b.x - a.x), th * 0.5f, c, 0.5f * al);
        box(0.5f * (a.x + b.x), b.y + ft, 0.5f * (b.x - a.x) + ft, th * 0.5f, c, 0.3f * al);
        box(0.5f * (a.x + b.x), a.y - 0.5f * ft, 0.5f * (b.x - a.x) + ft, 0.5f * ft, c, 0.07f * al);
        box(0.5f * (a.x + b.x), a.y - ft, 0.5f * (b.x - a.x) + ft, th * 0.5f, c, 0.22f * al);
        // a highlight running down the left edge, the neon on the right one
        box(a.x - 0.5f * ft, cy - 0.25f * hy, th * 0.7f, hy * 0.55f, vec3(1.f), 0.22f * al);
        box(b.x + 0.5f * ft, cy + 0.3f * hy, th * 0.7f, hy * 0.4f, lerp(hex(V.neon[0]), vec3(1.f), 0.4f), 0.2f * al);
        // the beads on the glass edges
        const vec3 water = lerp(hex(V.mist), vec3(0.9f, 0.95f, 1.f), 0.6f);
        for (int i = 0; i < 64; i++) {
            const float h1 = std::fmod(std::sin(i * 12.9898f) * 43758.5453f, 1.f), h2 = std::fmod(std::sin(i * 78.233f) * 12543.31f, 1.f);
            const float u = std::fabs(h1), v = std::fabs(h2);
            float bx, by;
            const float r = (0.03f + 0.06f * v * v) * unitPx_;
            if (i < 44) { // the sides, a third sliding down slowly
                const int side = i & 1;
                bx = (side == 0 ? a.x - 0.5f * ft : b.x + 0.5f * ft) + (v - 0.5f) * ft * 0.6f;
                float yy = u;
                if (i % 3 == 0) yy = std::fmod(u + time_ * (0.004f + 0.01f * v), 1.f);
                by = a.y - ft + yy * (b.y - a.y + 2.f * ft);
            } else { // the top edge and the floor's lip
                bx = a.x - ft + u * (b.x - a.x + 2.f * ft);
                by = i < 56 ? a.y - 0.5f * ft : b.y + 0.5f * ft;
            }
            disc(bx, by, r, r * 1.1f, water * 0.5f, 0.3f * al);
            disc(bx - 0.3f * r, by - 0.35f * r, r * 0.35f, r * 0.35f, vec3(1.f), 0.6f * al);
        }
        // drips hanging from the top edge and the floor's lip: they swell, stretch and let go
        for (int i = 0; i < 9; i++) {
            const float u = std::fabs(std::fmod(std::sin(i * 31.7f + 4.f) * 9123.7f, 1.f));
            const float per = 2.5f + 3.f * u;
            const float ph = std::fmod(rainT_ / per + u * 7.f, 1.f);
            const bool top = i < 5;
            const float x = a.x + (0.06f + 0.88f * std::fabs(std::fmod(u * 13.1f, 1.f))) * (b.x - a.x);
            const float y0 = top ? a.y : b.y + ft;
            const float L = (top ? 0.28f : 0.4f) * unitPx_ * ph * ph;
            seg(x, y0, x, y0 + L, std::max(1.f, 2.2f * s), water, 0.4f * al, 0.6f);
            disc(x, y0 + L, (0.035f + 0.03f * ph) * unitPx_, (0.04f + 0.035f * ph) * unitPx_, water * 0.6f, 0.45f * al);
            disc(x - 0.01f * unitPx_, y0 + L - 0.012f * unitPx_, 0.012f * unitPx_, 0.012f * unitPx_, vec3(1.f), 0.7f * al);
        }
    }
    // the ghost: the piece's tiles as fine outlines
    if (g.hasPiece() && !g.over()) {
        Piece gp = g.piece();
        gp.y = g.ghostY();
        if (gp.y > g.piece().y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            const vec3 c = tone(lerp(pieceColor(gp.type), vec3(1.f), 0.3f));
            for (auto& cc : gc) {
                if (cc[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)cc[0], (float)cc[1]);
                const vec2 p = px(w.x, w.y);
                const float h = (0.5f - GAP) * unitPx_;
                box(p.x, p.y, h, h, c, 0.5f, 0.2f * h, std::max(1.f, 1.6f * s));
            }
        }
    }
    flushShapes();

    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            tile(p.x, p.y, 0.5f, 0.5f, tone(pieceColor(c.type)), 1.f, seeds_[y][x], wet_[y][x], false);
        }
    // the falling piece
    if (g.hasPiece() && !g.over()) {
        int cells[4][2];
        g.pieceCells(g.piece(), cells);
        const float dx = g.pieceVisualX() - g.piece().x, dy = g.pieceVisualY() - g.piece().y;
        for (int i = 0; i < 4; i++) {
            if (cells[i][1] < 0) continue;
            const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
            tile(w.x, w.y, 0.5f, 0.5f, tone(pieceColor(g.piece().type)), 1.f, 37.f + i * 11.f, 0.75f, false);
        }
    }
    flushTiles();
}

void RainUniverse::drawPreviews(const Game& g) {
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    if (vis <= 0.01f) return;
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
        for (const auto& c : cells) {
            const float x = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, y = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            tile(x, y, 0.5f * sc, 0.5f * sc, pieceColor(type), alpha * vis, 0.f, 0.f, true);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
    flushTiles();
}

// =====================================================================================================
// The HUD
// =====================================================================================================

// The HUD's panels (the lettering: hudLine, the host letters it).
void RainUniverse::drawHud(const std::vector<HudRect>& panels) {
    const VariantDef& V = VARIANTS[variant_];
    const vec3 accent = lerp(hex(V.neon[0]), vec3(1.f), 0.15f), halo = hex(V.panel);

    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            box(r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.5f, r.h * 0.5f, c, std::min(1.f, r.alpha));
        }
        flushShapes();
    }
}

// Manrope in a pale rain-light white over a soft dark halo (readable over the rain and the piles), labels light and
// spaced, the section in the street's first neon. Sizes are the capitals' heights, the tracking in capitals.
void RainUniverse::hudLine(const HudText& h, text::Line& out) {
    const VariantDef& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    const vec3 ink(0.92f, 0.93f, 0.96f), accent = lerp(hex(V.neon[0]), vec3(1.f), 0.15f), halo = hex(V.panel);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    text::Font* light = text::font(FONT_LIGHT);
    text::Font* med = text::font(FONT_MED);
    const float base = h.scale * 12.f;
    const float alpha = saturate(h.alpha) * vis;
    text::Font* font = light;
    float cap = base * 0.5f, track = 0.04f, dy = 0.f, a = alpha;
    vec3 col = h.accent ? accent : ink;
    switch (h.role) {
    case HUD_UNIVERSE: cap = base * 0.5f; track = 0.14f; a = saturate(0.35f + h.alpha * 1.4f) * vis; break;
    case HUD_SECTION: font = med; cap = base * 0.55f; track = 0.2f; col = accent; a = saturate(0.4f + h.alpha * 1.5f) * vis; break;
    case HUD_TITLE: font = med; cap = base * 0.6f; track = 0.01f; a = saturate(h.alpha * 1.6f) * vis; break;
    case HUD_INFO: cap = base * 0.5f; track = 0.06f; col = ink; a = saturate(alpha * 1.3f) * 0.85f; break;
    case HUD_LABEL: cap = base * 0.44f; track = 0.24f; dy = base * 0.1f; a = saturate(alpha * 1.3f) * 0.8f; break;
    case HUD_VALUE: font = med; cap = base * 0.64f; track = 0.02f; break;
    case HUD_NOTICE: font = med; cap = base * 0.55f; col = accent; break;
    case HUD_KEY: font = med; cap = base * 0.55f; break;
    case HUD_TOAST: font = med; cap = base * 0.72f; track = 0.08f; break;
    case HUD_TOAST_SUB: cap = base * 0.5f; col = accent; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) { font = med; cap = base * 0.62f; }
        break;
    default: break;
    }
    // Manrope's capitals: 0.527 of its size
    constexpr float CAP = 0.527f;
    out.add(h.text, font, cap / CAP, col, track * CAP, a);
    out.dy = dy;
    // a soft pass in the panel's color under the letters (on the old field: from .22 out to .066 in, 4 em-units of
    // spread per 1000), then the crisp fill
    out.look.shadow = halo;
    out.look.shadowA = 0.7f;
    out.look.shadowW = 0.077f * 0.249f;
    out.look.shadowSoft = 0.143f * 0.249f;
}

// =====================================================================================================
// Frame
// =====================================================================================================

// The drop's grade on the CPU, as BG_FS's grade(): a YIQ hue rotation, saturation, the duotone wash, the exposure.
vec3 RainUniverse::graded(const vec3& c) const {
    const Grade& g = grade_;
    const float y = 0.299f * c.x + 0.587f * c.y + 0.114f * c.z;
    float i = 0.596f * c.x - 0.274f * c.y - 0.322f * c.z, q = 0.211f * c.x - 0.523f * c.y + 0.312f * c.z;
    const float i2 = (g.c * i - g.s * q) * g.sat, q2 = (g.s * i + g.c * q) * g.sat;
    i = i2; q = q2;
    vec3 r{std::max(0.f, y + 0.956f * i + 0.621f * q), std::max(0.f, y - 0.272f * i - 0.647f * q), std::max(0.f, y - 1.106f * i + 1.703f * q)};
    const vec3 m = lerp(g.lo, g.hi, smoothstepf(0.02f, 0.45f, y));
    const vec3 duo = m * (y / std::max(dot(m, vec3(0.299f, 0.587f, 0.114f)), 0.02f));
    return lerp(r, duo, g.duo) * g.expo;
}

void RainUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    const VariantDef& V = VARIANTS[variant_];
    bare_ = f.bare;
    time_ += dt;
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    const float rdt = f.paused ? 0.f : dt * lerpf(1.f, 0.2f, slow_);
    rainT_ += rdt;

    // ---- The music: drizzle, steady rain, downpour; the drop's thunderstorm held for the whole drop section.
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = !game.over() && level_ == 2 &&
                        (sectionKnown ? sectionDrop : dropMode_ && (!music.hasSong || music.speed > 1.26f));
    storm_ = approach(storm_, isDrop ? 1.f : 0.f, isDrop ? 1.1f : 0.3f, dt);
    {
        float target = level_ == 0 ? 0.16f + 0.12f * energy_ : level_ == 1 ? 0.48f : 0.74f;
        target = std::max(target, storm_);
        rain_ = approach(rain_, target, 0.7f, dt);
        glow_ = approach(glow_, 0.62f + 0.18f * level_ + 0.15f * storm_, 0.8f, dt);
        flood_ = approach(flood_, storm_, 0.35f, dt);
        const float kick = std::max(music.kick, kickIn_);
        kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
        if (level_ == 2 && !f.paused) spawnAcc_ += kick * dt * 900.f; // a gust on the kicks
    }
    // soft lightning in the clouds, now and then on a bar line (only in the storm)
    {
        const double beat = music.hasSong ? music.beat : time_ * 2.0;
        const long half = (long)std::floor(beat / 2.0);
        if (half != flashBar_) {
            const bool onBar = (half & 1) == 0;
            flashBar_ = half;
            if (storm_ > 0.6f && !f.paused && rng_.chance(onBar ? 0.85f : 0.3f)) {
                strikeT_ = 0.f;
                strikeAmp_ = rng_.range(0.75f, 1.f) * (onBar ? 1.f : 0.6f);
                flashPos_ = vec2{rng_.range(-1.f, 1.f) * viewH_.x * 0.8f, rng_.range(5.f, 11.f)};
            }
        }
        strikeT_ += dt;
        {
            const float t = strikeT_;
            const float pulse = std::exp(-t * 7.f) + (t > 0.12f ? 0.65f * std::exp(-(t - 0.12f) * 9.f) : 0.f);
            flash_ = strikeAmp_ * std::min(1.f, pulse) * smoothstepf(0.f, 0.02f, t) * smoothstepf(0.3f, 0.7f, storm_);
        }
        // the neon brighter in the storm, breathing slowly (no beat-by-beat surge, no flicker); the light sweeping
        // across the street
        neonGain_ = lerpf(1.f, 1.2f + 0.12f * std::sin(time_ * 0.7f), ease(storm_));
        sweepX_ = (float)std::sin(beat * PI / 8.0) * viewH_.x;

        // the drop's colour mood: on the downbeat the whole street swings to the variant's mood A (well under a
        // second), glides to mood B 8 bars later and back every 8 bars (a few seconds each, the hue drifting slowly
        // meanwhile), and eases back to the street's own colours over a few seconds after the drop
        if (isDrop && !wasDrop_) {
            dropBeat0_ = beat;
            if (gradeK_ < 0.05f) gradeAB_ = 0.f;
        }
        wasDrop_ = isDrop;
        gradeK_ = approach(gradeK_, isDrop ? 1.f : 0.f, isDrop ? 5.f : 1.1f, dt);
        if (isDrop) {
            const long seg = (long)std::floor((beat - dropBeat0_ + 0.5) / 32.0);
            gradeAB_ = approach(gradeAB_, (seg & 1) ? 1.f : 0.f, 1.1f, dt);
        }
        const Mood& A = MOODS[variant_][0];
        const Mood& B = MOODS[variant_][1];
        const float ab = ease(gradeAB_), k = ease(gradeK_);
        const float hue = (lerpf(A.hue, B.hue, ab) + 12.f * (float)std::sin((beat - dropBeat0_) * PI / 32.0)) * k;
        const float th = -hue * PI / 180.f; // (in YIQ, a positive angle goes down the hue wheel: roughly -hue)
        grade_.c = std::cos(th);
        grade_.s = std::sin(th);
        grade_.sat = 1.f + k * (lerpf(A.sat, B.sat, ab) - 1.f);
        grade_.duo = k * lerpf(A.duo, B.duo, ab);
        grade_.expo = 1.f + k * (lerpf(A.expo, B.expo, ab) - 1.f);
        grade_.lo = lerp(hex(A.lo), hex(B.lo), ab);
        grade_.hi = lerp(hex(A.hi), hex(B.hi), ab);
    }
    // the HUD's bottom corner lines: the heaps dim under them
    if (!bare_) {
        textRects_.clear();
        hudRects_.clear();
        for (size_t i = 0; f.hudBoxes && i < f.hud.size() && i < f.hudBoxes->size(); i++) {
            const HudText& h = f.hud[i];
            if (h.text.empty() || h.role == HUD_TOAST || h.role == HUD_TOAST_SUB || h.role == HUD_DIALOG) continue;
            const float base = h.scale * 12.f;
            const vec4& bx = (*f.hudBoxes)[i]; // the line as lettered
            const float x0 = viewC_.x + (bx.x - 6.f - width_ * 0.5f) / unitPx_, x1 = viewC_.x + (bx.z + 6.f - width_ * 0.5f) / unitPx_;
            const float y0 = viewC_.y + (height_ * 0.5f - std::max(bx.w, h.y + base * 0.9f)) / unitPx_, y1 = viewC_.y + (height_ * 0.5f - h.y + 4.f) / unitPx_;
            if (std::fabs(0.5f * (x0 + x1)) > 5.5f) hudRects_.push_back({x0, y0, x1, y1});
            if (y0 > -3.f || std::fabs(0.5f * (x0 + x1)) < bandIn_) continue;
            textRects_.push_back({x0, y0, x1, y1});
        }
        // the left heap's outer wall keeps it clear of those lines; the blade signs over the heaps
        leftOut_ = bandOut_;
        signPos_[0] = vec2{-(bandIn_ + std::max(1.2f, 0.5f * (leftOut_ - bandIn_))), 1.6f};
        signPos_[1] = vec2{bandIn_ + 0.55f * (bandOut_ - bandIn_), 0.8f};
    }

    // ---- The setup: the street's lights come on one by one (windows, shops, signs) as the rain sets in; the
    // teardown: they go out one by one, the rain thinning, the street sinking into the dusk.
    lights_ = smoothstepf(0.1f, 0.62f, f.setup) * (1.f - smoothstepf(0.3f, 0.85f, f.teardown));
    shower_ = smoothstepf(0.f, 0.45f, f.setup) * (1.f - 0.85f * smoothstepf(0.35f, 0.9f, f.teardown));

    // ---- The world steps: the solids, the rain, the piles, the drying.
    buildSolids(game);
    stepRain(dt, rdt);
    // (test hook: REFALL_RAIN_HEAP=N throws N tiles onto the heaps 90 frames in)
    static int heapTest = std::getenv("REFALL_RAIN_HEAP") ? std::atoi(std::getenv("REFALL_RAIN_HEAP")) : 0;
    static int heapFrame = 0;
    if (heapTest > 0 && ++heapFrame >= 90) // (after the start's settling)
        for (; heapTest > 0; heapTest--) eject(heapTest % 2 ? 0 : Game::W - 1, Game::HIDDEN + 2 + heapTest % 6, heapTest % 7, 0.04f * heapTest);
    stepHeaps(dt);
    stepFore(dt, rdt);
    for (auto& row : wet_)
        for (float& w : row) w = std::max(0.12f, w - dt * 0.1f);

    // ---- The backdrop.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(progBg_);
    {
        const float sn = smoothstepf(0.f, 1.f, slow_);
        auto night = [&](unsigned c) { return lerp(hex(c), hex(c) * vec3(0.55f, 0.65f, 1.f), sn * 0.6f); };
        set2f(progBg_, "uRes", (float)width_, (float)height_);
        set2f(progBg_, "uViewC", viewC_.x, viewC_.y);
        set1f(progBg_, "uUnitPx", unitPx_);
        set1f(progBg_, "uTime", time_);
        // the storm: a darker sky over a horizon glowing in the street's colors
        const float st = ease(storm_);
        set3f(progBg_, "uSkyTop", night(V.skyTop) * (1.f - 0.25f * st));
        set3f(progBg_, "uSkyLow", lerp(night(V.skyLow), hex(V.neon[0]) * 0.8f, 0.25f * st));
        set3f(progBg_, "uFar", night(V.far));
        set3f(progBg_, "uNear", night(V.near));
        set3f(progBg_, "uWin", hex(V.win));
        set3f(progBg_, "uMist", night(V.mist));
        set3f(progBg_, "uStreet", night(V.street));
        set3f(progBg_, "uPanel", hex(V.panel));
        float neon[12];
        for (int i = 0; i < 4; i++) {
            const vec3 c = hex(V.neon[i]);
            neon[i * 3] = c.x; neon[i * 3 + 1] = c.y; neon[i * 3 + 2] = c.z;
        }
        glUniform3fv(ugl::U(progBg_, "uNeon"), 4, neon);
        set1i(progBg_, "uStyle", V.style);
        set1f(progBg_, "uSat", 1.f + 0.4f * storm_);
        set1f(progBg_, "uGlow", glow_ * lerpf(1.f, 0.8f, sn));
        set3f(progBg_, "uFlash", vec3(0.72f, 0.8f, 1.f) * (0.6f * flash_));
        set1f(progBg_, "uStorm", ease(storm_));
        set1f(progBg_, "uNeonGain", neonGain_);
        set1f(progBg_, "uLights", lights_ * 1.001f);
        set1f(progBg_, "uSweepX", sweepX_);
        set3f(progBg_, "uSweepCol", hex(V.neon[((long)std::floor(time_ / 4.f)) & 1 ? 0 : 1]));
        set2f(progBg_, "uFlashPos", flashPos_.x, flashPos_.y);
        set1f(progBg_, "uRain", rain_ * shower_);
        set1f(progBg_, "uSlant", V.slant + 0.22f * storm_);
        set1f(progBg_, "uFlood", flood_);
        set1f(progBg_, "uGust", storm_);
        const float sp[4] = {signPos_[0].x, signPos_[0].y, signPos_[1].x, signPos_[1].y};
        glUniform2fv(ugl::U(progBg_, "uSignPos"), 2, sp);
        const vec3 s0 = graded(hex(V.neon[1])) * neonGain_, s1 = graded(hex(V.neon[0])) * neonGain_;
        const float sc[6] = {s0.x, s0.y, s0.z, s1.x, s1.y, s1.z};
        glUniform3fv(ugl::U(progBg_, "uSignCol"), 2, sc);
        set1f(progBg_, "uRainT", rainT_);
        set2f(progBg_, "uGradeRot", grade_.c, grade_.s);
        set1f(progBg_, "uGradeSat", grade_.sat);
        set3f(progBg_, "uDuoLo", grade_.lo);
        set3f(progBg_, "uDuoHi", grade_.hi);
        set1f(progBg_, "uDuoK", grade_.duo);
        set1f(progBg_, "uGradeExpo", grade_.expo);
        {
            float r[24 * 4];
            int n = 0;
            for (size_t i = 0; i < hudRects_.size() && n < 24; i++, n++) {
                r[n * 4] = hudRects_[i].x; r[n * 4 + 1] = hudRects_[i].y; r[n * 4 + 2] = hudRects_[i].z; r[n * 4 + 3] = hudRects_[i].w;
            }
            if (bare_) n = 0;
            if (n) glUniform4fv(ugl::U(progBg_, "uHudRect"), n, r);
            set1i(progBg_, "uHudN", n);
        }
        set1f(progBg_, "uWell", bare_ ? 0.f : 1.f - smoothstepf(0.f, 0.8f, bridge_.amount));
        set3f(progBg_, "uBridge", ugl::toSrgb(bridge_.color));
        set1f(progBg_, "uBridgeK", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
        set1f(progBg_, "uDim", 0.55f * (1.f - smoothstepf(0.f, 0.4f, f.setup)) + 0.55f * smoothstepf(0.55f, 1.f, f.teardown));
    }
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // ---- The piles, the tiles in flight (behind the stack and the HUD), the game, the rain over all of it.
    drawHeaps();
    if (!bare_) drawBoard(game);
    drawRain(true);
    if (!bare_) drawPreviews(game);
    drawFore();

    // ---- Dim (pause, game over screen) and the startup fade.
    {
        const float d = 0.45f * dimT_;
        if (d > 0.002f) box(width_ * 0.5f, height_ * 0.5f, width_ * 0.5f + 2.f, height_ * 0.5f + 2.f, hex(V.panel), d);
        if (f.fade < 0.999f) box(width_ * 0.5f, height_ * 0.5f, width_ * 0.5f + 2.f, height_ * 0.5f + 2.f, vec3(0.f), 1.f - saturate(f.fade));
        flushShapes();
    }
    drawHud(f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_rain() {
    static const UniverseEntry e{
        "RAIN", "rain everywhere over a colorful street at dusk: it pours on soft pastel sugar cubes in a wet glass case (the falling piece shelters the cells under it), cleared rows are thrown out to heaps on the sides and toward the camera, where they melt in the rain, the drop swings the whole street to another colour mood in a thunderstorm",
        [] { return std::unique_ptr<Universe>(new RainUniverse()); },
        // An atmosphere for most songs, moody and mid-tempo ones best.
        [](const SongLook& l) { return 0.85f + 0.15f * (1.f - l.brightness) + 0.1f * l.dynamics; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
