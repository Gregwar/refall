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
// The pieces are wet glazed ceramic tiles in pastel glazes, one tile per cell: the glaze deepens at the rim, the
// street's neon is caught in a soft sheen, a film of water lies along the top edge, beads of water sit on the face and
// now and then a rivulet slides down it (with the cell's wetness: a sheltered cell slowly dries). The previews are the
// same tiles, dry and plain.
//
// A cleared row's tiles are thrown out of the well, half to each side, on a scripted arc (behind the stack and the
// HUD) to a release over the side bands, then become rigid boxes in a small 2D solver (sequential impulses, friction,
// sleeping): they tumble and settle into messy heaps on the street, where the rain falls on their tilted faces and
// slides off the lower corners. The rain soaks them like cardboard: dark water stains creep down from their tops, they
// go matte and limp (sagging in the middle, bulging, their turn relaxing toward lying flat) and are crushed thinner, so
// the heaps slump and pack down little by little over the song. Each heap is capped (the oldest fades away). A new song washes them away; game over
// throws the whole stack out, row by row from the top.
//
// The music sets the rain: a drizzle when calm, steady rain at mid, a downpour at a peak (with a gust on the kicks).
// The drop is a thunderstorm for the whole drop section: the heaviest rain, a steeper slant, the city's colors and
// lights intensified, soft lightning glowing in the clouds now and then on a bar line (a tinted glow, never a white
// flash); after the drop it eases back over a few seconds. Slow motion slows the rain until it hangs in the air.
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
        for (auto& o : order) eject(o.second.x, o.second.y, o.second.type, o.first);
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
    const vec3 tint = lerp(lerp(hex(V.mist), vec3(0.95f, 0.97f, 1.f), 0.65f), vec3(0.85f, 0.92f, 1.f), flash_);
    const float w = std::max(1.f, 1.3f * s);
    const float slowLen = lerpf(1.f, 0.35f, slow_);
    for (const Drop& d : drops_) {
        const float sp = std::sqrt(d.vx * d.vx + d.vy * d.vy);
        const float L = d.drip ? std::min(0.3f, 0.05f + sp * 0.02f) : d.len * slowLen;
        const float tx = d.x - d.vx / sp * L, ty = d.y - d.vy / sp * L;
        const vec2 a = px(d.x, d.y), b = px(tx, ty);
        const bool inWell = std::fabs(d.x) < 5.f && d.y < 10.f;
        float al = d.a * (inWell ? 0.32f : 0.42f) * (0.7f + 0.3f * rain_) * (1.f + 1.4f * flash_);
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
    set3f(progTile_, "uSheen", hex(V.neon[0]) * (1.f - bridge_.amount));
    set3f(progTile_, "uSheen2", hex(V.neon[1]) * (1.f - bridge_.amount));
    set1f(progTile_, "uGlow", glow_);
    {
        const vec2 a = px(signPos_[0].x, signPos_[0].y), b = px(signPos_[1].x, signPos_[1].y);
        const float lp[4] = {a.x, a.y, b.x, b.y};
        glUniform2fv(ugl::U(progTile_, "uLightPx"), 2, lp);
        const vec3 c0 = hex(V.neon[1]) * glow_ * neonGain_, c1 = hex(V.neon[0]) * glow_ * neonGain_;
        set3f(progTile_, "uFlashRim", vec3(0.75f, 0.83f, 1.f) * flash_);
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

    // the well: a hairline of wet metal on its sides and floor, catching the neon
    {
        const float th = std::max(1.f, std::round(1.5f * s));
        const vec2 a = px(-5.f, 10.f), b = px(5.f, -10.f);
        const vec3 c = lerp(vec3(0.75f, 0.78f, 0.85f), hex(V.neon[1]), 0.25f);
        const float al = 0.5f * (1.f - br);
        box(a.x - 2.f * th, 0.5f * (a.y + b.y), th * 0.5f, 0.5f * (b.y - a.y), c, al);
        box(b.x + 2.f * th, 0.5f * (a.y + b.y), th * 0.5f, 0.5f * (b.y - a.y), c, al);
        box(0.5f * (a.x + b.x), b.y + 2.f * th, 0.5f * (b.x - a.x) + 2.5f * th, th * 0.5f, c, al);
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
        // the neon surging on the beats, flickering now and then; the light sweeping across the street
        const long bt = (long)std::floor(beat);
        if (bt != surgeBeat_) { surgeBeat_ = bt; if (storm_ > 0.5f) surge_ = 1.f; }
        surge_ = approach(surge_, 0.f, 5.f, dt);
        const float flicker = storm_ > 0.5f && std::fmod(time_ * 7.3f, 3.1f) < 0.12f ? 0.35f : 1.f;
        neonGain_ = lerpf(1.f, (0.75f + 0.8f * surge_) * flicker, ease(storm_));
        sweepX_ = (float)std::sin(beat * PI / 8.0) * viewH_.x;
    }
    // the HUD's bottom corner lines: the heaps dim under them
    if (!bare_) {
        textRects_.clear();
        for (size_t i = 0; f.hudBoxes && i < f.hud.size() && i < f.hudBoxes->size(); i++) {
            const HudText& h = f.hud[i];
            if (h.text.empty() || h.role == HUD_TOAST || h.role == HUD_TOAST_SUB || h.role == HUD_DIALOG) continue;
            const float base = h.scale * 12.f;
            const vec4& bx = (*f.hudBoxes)[i]; // the line as lettered
            const float x0 = viewC_.x + (bx.x - 6.f - width_ * 0.5f) / unitPx_, x1 = viewC_.x + (bx.z + 6.f - width_ * 0.5f) / unitPx_;
            const float y0 = viewC_.y + (height_ * 0.5f - std::max(bx.w, h.y + base * 0.9f)) / unitPx_, y1 = viewC_.y + (height_ * 0.5f - h.y + 4.f) / unitPx_;
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
        set3f(progBg_, "uFlash", vec3(0.72f, 0.8f, 1.f) * (1.6f * flash_));
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
        const vec3 s0 = hex(V.neon[1]) * neonGain_, s1 = hex(V.neon[0]) * neonGain_;
        const float sc[6] = {s0.x, s0.y, s0.z, s1.x, s1.y, s1.z};
        glUniform3fv(ugl::U(progBg_, "uSignCol"), 2, sc);
        set1f(progBg_, "uRainT", rainT_);
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
        "RAIN", "rain everywhere over a colorful street at dusk: it pours on wet pastel glazed tiles (the falling piece shelters the cells under it), cleared rows are thrown out and tumble into heaps on the sides, the drop is a thunderstorm",
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
