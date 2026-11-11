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
// The pieces are little umbrellas seen from above, one canopy per cell: eight wet nylon panels in two tones of the
// piece's color, scalloped between the rib tips, domed, a metal ferrule, a wet sheen, big beads sliding out along the
// panels (with the cell's wetness: a sheltered cell slowly dries). The previews are the same canopies, dry and plain.
//
// A cleared row's umbrellas are thrown out of the well, half to each side, on a scripted arc (behind the stack and the
// HUD) to a release over the side bands, then become rigid boxes in a small 2D solver (sequential impulses, friction,
// sleeping): they tumble and settle into messy heaps on the street, where the rain falls on their tilted faces and
// slides off the lower corners. Each heap is capped (the oldest fades away). A new song washes them away; game over
// throws the whole stack out, row by row from the top.
//
// The music sets the rain: a drizzle when calm, steady rain at mid, a downpour at a peak (with a gust on the kicks).
// The drop is a thunderstorm for the whole drop section: the heaviest rain, a steeper slant, the city's colors and
// lights intensified, soft lightning glowing in the clouds now and then on a bar line (a tinted glow, never a white
// flash); after the drop it eases back over a few seconds. Slow motion slows the rain until it hangs in the air.
//
// Cost: one fullscreen backdrop pass, the rain stepped on the CPU against per-column spans of the solids (a few
// dozen tests per drop), instanced shapes and tiles, SDF type.
#include "universes/rain/rain.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/rain/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

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
     {0x35d8ff, 0xffd23a, 0xb066ff, 0x5fe36a, 0xff3b6b, 0x3d6dff, 0xff8c2e}},
    {"FLOWER MARKET", 1, 0x1f2f66, 0xf08a5e, 0x5a3a6a, 0x2e1a2c, 0xffd08a, 0xb0607a, 0x1c1220, 0x100b12,
     {0xff4f7a, 0x3fcfa0, 0xffb33a, 0x9a7aff}, 0.06f,
     {0x5ad1e6, 0xffd447, 0xb98cff, 0x4fcf6e, 0xff5c8a, 0x4f7dff, 0xff8f3a}},
    {"MONSOON", 2, 0x082b3a, 0x1f7a70, 0x15504e, 0x0a2428, 0xffc06a, 0x3f8a80, 0x081a1c, 0x061012,
     {0xff6a3a, 0xffb03a, 0xff4f6a, 0xffd86a}, 0.16f,
     {0x2fe0d0, 0xffc531, 0xe0479e, 0x45d67c, 0xff5733, 0x5b6dff, 0x9d62ff}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float GAP = 0.025f;        // between the canopies, each side (board units)
constexpr float HORIZON = -8.2f;     // the street's far edge (as in the backdrop)

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
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "rain text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, int stride) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < 3; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(shapeVao_, shapeVbo_, sizeof(Shape));
    instanced(tileVao_, tileVbo_, sizeof(Tile));
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    buildFont(med_, FONT_MED, 96.f, 1536);
    buildFont(light_, FONT_LIGHT, 64.f, 1024);
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) seeds_[y][x] = rng_.range(0.f, 100.f);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void RainUniverse::buildFont(Font& f, const unsigned char* ttf, float pxSize, int AW) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] rain font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = pxSize;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AH = AW, PAD = (int)(f.px / 8.f);
    std::vector<unsigned char> atlas((size_t)AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    auto cpOf = [](int ch) { return ch == 127 ? 0xB7 : ch; };
    for (int ch = 32; ch < 128; ch++) {
        const int cp = cpOf(ch);
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, cp, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
        if (!sdf) { g.w = g.h = 0; continue; }
        if (x + w + 1 > AW) { x = 1; y += rowH + 1; rowH = 0; }
        if (y + h + 1 > AH) { stbtt_FreeSDF(sdf, nullptr); break; }
        for (int r = 0; r < h; r++) std::memcpy(&atlas[(size_t)(y + r) * AW + x], sdf + r * w, w);
        g.x0 = x / (float)AW; g.y0 = y / (float)AH; g.x1 = (x + w) / (float)AW; g.y1 = (y + h) / (float)AH;
        g.ox = (float)xo; g.oy = (float)yo; g.w = (float)w; g.h = (float)h;
        x += w + 1;
        rowH = std::max(rowH, h);
        stbtt_FreeSDF(sdf, nullptr);
    }
    for (int a = 0; a < 96; a++)
        for (int b = 0; b < 96; b++)
            f.kern[a][b] = stbtt_GetCodepointKernAdvance(info, cpOf(a + 32), cpOf(b + 32)) * f.scaleTT;
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
    for (GLuint p : {progBg_, progShape_, progTile_, progText_}) glDeleteProgram(p);
    for (GLuint v : {shapeVao_, tileVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, shapeVbo_, tileVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&med_, &light_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
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
    for (const Body& b : bodies_)
        if (b.solid && !b.thrown && std::fabs(x - b.x) < b.half * 1.42f && bodyInside(b, x, y)) return true;
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
    for (const Body& b : bodies_) {
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
    const float slant = V.slant + 0.07f * storm_;
    // spawn: a drizzle .. a downpour
    if (bridge_.amount < 0.98f) {
        const float rate = lerpf(220.f, 2600.f, std::pow(saturate(rain_), 1.4f)) * (rdt / std::max(dt, 1e-4f));
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
    const vec3 tint = lerp(hex(V.mist), vec3(0.95f, 0.97f, 1.f), 0.65f);
    const float w = std::max(1.f, 1.3f * s);
    const float slowLen = lerpf(1.f, 0.35f, slow_);
    for (const Drop& d : drops_) {
        const float sp = std::sqrt(d.vx * d.vx + d.vy * d.vy);
        const float L = d.drip ? std::min(0.3f, 0.05f + sp * 0.02f) : d.len * slowLen;
        const float tx = d.x - d.vx / sp * L, ty = d.y - d.vy / sp * L;
        const vec2 a = px(d.x, d.y), b = px(tx, ty);
        const bool inWell = std::fabs(d.x) < 5.f && d.y < 10.f;
        float al = d.a * (inWell ? 0.32f : 0.42f) * (0.7f + 0.3f * rain_);
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
    const int cap = side == 0 ? 22 : 32;
    if (n >= cap && oldest >= 0) { bodies_[oldest].fading = true; bodies_[oldest].fadeRate = 0.6f; }

    Body b{};
    b.id = nextBody_++;
    b.type = type;
    b.side = side;
    b.seed = seeds_[std::clamp(y, 0, Game::H - 1)][std::clamp(x, 0, Game::W - 1)];
    b.half = 0.46f;
    const vec3 p = layout::cellPos((float)x, (float)y);
    b.x = b.x0 = p.x;
    b.y = b.y0 = p.y;
    b.wet = 0.6f;
    b.fade = 1.f;
    b.solid = true;
    b.thrown = true;
    b.t = 0.f;
    b.delay = delay;
    b.dur = rng_.range(0.85f, 1.05f);
    b.tx0 = dir * rng_.range(9.f, 12.f);
    b.ty0 = rng_.range(3.5f, 6.5f);
    // the release: over the band (the left heap keeps clear of the HUD's corner lines), above the heap there
    const float f = side == 0 ? rng_.range(0.04f, 0.42f) : rng_.range(0.15f, 0.8f);
    b.x1 = dir * (bandIn_ + 0.6f + (bandOut_ - bandIn_ - 1.2f) * f);
    float top = ground_;
    for (const Body& o : bodies_)
        if (!o.thrown && o.solid && std::fabs(o.x - b.x1) < 1.4f) top = std::max(top, o.y + 0.7f);
    b.y1 = std::max(-5.f, top + 1.6f);
    b.tx1 = dir * rng_.range(0.5f, 2.5f);
    b.ty1 = -rng_.range(3.f, 6.f);
    b.w = rng_.range(2.f, 6.f) * (rng_.chance(0.5f) ? -1.f : 1.f);
    bodies_.push_back(b);
}

namespace {
struct Contact {
    int i, j;             // body indices (j < 0: the street or a wall)
    float px, py, nx, ny; // the point, the normal (from j to i)
    float depth, pn, pt, bias, kn, kt;
};
inline float cross2(float ax, float ay, float bx, float by) { return ax * by - ay * bx; }
} // namespace

// One step of the boxes: gravity, contacts (corners inside the other box, the street, the band's walls), a few
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
    auto pointIn = [&](int i, int j, float x, float y) {
        // a corner of i inside j: the contact on j's nearest face, its normal from j to i
        const Body& B = bodies_[j];
        const float c = std::cos(B.a), s = std::sin(B.a), dx = x - B.x, dy = y - B.y;
        const float lx = c * dx + s * dy, ly = -s * dx + c * dy;
        const float ox = B.half - std::fabs(lx), oy = B.half - std::fabs(ly);
        if (ox <= 0.f || oy <= 0.f) return;
        float nlx = 0.f, nly = 0.f, d;
        if (ox < oy) { nlx = lx < 0.f ? -1.f : 1.f; d = ox; }
        else { nly = ly < 0.f ? -1.f : 1.f; d = oy; }
        cs.push_back({i, j, x, y, c * nlx - s * nly, s * nlx + c * nly, d, 0, 0, 0, 0, 0});
    };
    for (int i = 0; i < N; i++) {
        if (!live(i) || bodies_[i].asleep) continue;
        const Body& A = bodies_[i];
        float P[4][2];
        corners(A, P);
        const float dir = A.side == 0 ? -1.f : 1.f;
        for (auto& c : P) {
            if (c[1] < ground_) cs.push_back({i, -1, c[0], c[1], 0.f, 1.f, ground_ - c[1], 0, 0, 0, 0, 0});
            const float in = dir * c[0] - bandIn_, out = bandOut_ - dir * c[0];
            if (in < 0.f) cs.push_back({i, -1, c[0], c[1], dir, 0.f, -in, 0, 0, 0, 0, 0});
            if (out < 0.f) cs.push_back({i, -1, c[0], c[1], -dir, 0.f, -out, 0, 0, 0, 0, 0});
        }
        for (int j = 0; j < N; j++) {
            if (j == i || !live(j)) continue;
            const Body& B = bodies_[j];
            if (!B.asleep && j < i) continue; // awake pairs once
            const float dx = B.x - A.x, dy = B.y - A.y, r = (A.half + B.half) * 1.42f;
            if (dx * dx + dy * dy > r * r) continue;
            float Q[4][2];
            corners(B, Q);
            for (auto& c : P) pointIn(i, j, c[0], c[1]);
            for (auto& c : Q) pointIn(j, i, c[0], c[1]);
        }
    }
    // masses (a sleeping box is static), a hard hit wakes it
    auto invM = [&](int i) { return i < 0 || bodies_[i].asleep ? 0.f : 1.f; };
    auto invI = [&](int i) { return i < 0 || bodies_[i].asleep ? 0.f : 6.f / (4.f * bodies_[i].half * bodies_[i].half); };
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
        for (int k : {c.i, c.j})
            if (k >= 0 && bodies_[k].asleep && vn < -3.f) { bodies_[k].asleep = false; bodies_[k].still = bodies_[k].awake = 0.f; }
        c.bias = 0.15f / h * std::max(0.f, c.depth - 0.01f);
        c.bias = std::min(c.bias, 2.f);
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
    for (Body& b : bodies_) {
        if (b.thrown || !b.solid || b.asleep) continue;
        b.vx *= std::exp(-0.3f * h);
        b.w *= std::exp(-1.2f * h);
        b.x += b.vx * h;
        b.y += b.vy * h;
        b.a += b.w * h;
        const bool still = b.vx * b.vx + b.vy * b.vy < 0.15f * 0.15f && std::fabs(b.w) < 0.3f;
        b.still = still ? b.still + h : 0.f;
        b.awake += h;
        if (b.still > 0.5f || b.awake > 7.f) { b.asleep = true; b.vx = b.vy = b.w = 0.f; }
    }
}

void RainUniverse::stepHeaps(float dt) {
    // the throws: an arc out of the well, then released into the physics
    for (Body& b : bodies_) {
        if (!b.thrown) continue;
        b.t += dt;
        if (b.t < b.delay) continue;
        const float T = b.dur, s = std::min(1.f, (b.t - b.delay) / T);
        const float s2 = s * s, s3 = s2 * s;
        const float h00 = 2 * s3 - 3 * s2 + 1, h10 = s3 - 2 * s2 + s, h01 = -2 * s3 + 3 * s2, h11 = s3 - s2;
        b.x = h00 * b.x0 + h10 * T * b.tx0 + h01 * b.x1 + h11 * T * b.tx1;
        b.y = h00 * b.y0 + h10 * T * b.ty0 + h01 * b.y1 + h11 * T * b.ty1;
        const float tau = s * T;
        b.a = 0.5f * b.w * tau * tau / T;
        if (s >= 1.f) {
            b.thrown = false;
            b.vx = b.tx1;
            b.vy = b.ty1;
        }
    }
    // the boxes, at a fixed step
    physAcc_ = std::min(physAcc_ + dt, 4.f / 120.f);
    while (physAcc_ >= 1.f / 120.f) {
        physicsStep(1.f / 120.f);
        physAcc_ -= 1.f / 120.f;
    }
    // fading tiles sink away; once gone, what rested on them falls again
    for (Body& b : bodies_) {
        b.wet = std::max(0.2f, b.wet - dt * 0.08f);
        if (!b.fading) continue;
        b.fade = std::max(0.f, b.fade - dt * b.fadeRate);
        if (b.solid && b.fade < 0.35f) {
            b.solid = false;
            for (Body& o : bodies_)
                if (o.side == b.side && o.asleep && o.y > b.y - 0.3f && std::fabs(o.x - b.x) < 2.5f) {
                    o.asleep = false;
                    o.still = o.awake = 0.f;
                }
        }
    }
    bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(), [](const Body& b) { return b.fading && b.fade <= 0.f; }), bodies_.end());
    // fallen out of the picture (a wall missed at a high speed): gone
    bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(), [&](const Body& b) { return b.y < ground_ - 3.f; }), bodies_.end());
}

void RainUniverse::drawHeaps() {
    const vec3 bInk = ugl::toSrgb(bridge_.ink);
    const float bk = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    for (const Body& b : bodies_) {
        float a = b.fading ? std::min(1.f, b.fade * 1.4f) : 1.f;
        a *= 1.f - 0.6f * hudCover(b.x, b.y);   // behind the HUD's columns
        a *= 1.f - 0.6f * textCover(b.x, b.y);  // and its corner lines
        const vec3 c = lerp(pieceColor(b.type), bInk, bk);
        if (!(b.thrown && b.t < b.delay)) a *= 1.f - smoothstepf(0.f, 0.6f, bridge_.amount);
        tile(b.x, b.y, b.half + 0.02f, b.half + 0.02f, c, a, b.seed, b.wet, false, b.a);
    }
    flushTiles();
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
                      {seed, wet, plain ? 1.f : 0.f, angle}});
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
    glBindVertexArray(tileVao_);
    glBindBuffer(GL_ARRAY_BUFFER, tileVbo_);
    glBufferData(GL_ARRAY_BUFFER, tiles_.size() * sizeof(Tile), tiles_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)tiles_.size());
    tiles_.clear();
}

float RainUniverse::capWidth(const Font& f, const std::string& s, float track) const {
    if (!f.ok || s.empty()) return s.size() * 0.7f;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv / f.capH + track;
        if (i + 1 < s.size()) {
            unsigned char nx = (unsigned char)s[i + 1];
            if (nx < 32 || nx >= 128) nx = '?';
            w += f.kern[ch - 32][nx - 32] / f.capH;
        }
    }
    return w - track;
}

void RainUniverse::textQuads(const Font& f, const std::string& s, float x, float base, float capPx, float track,
                             std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = capPx / f.capH;
    float pen = 0.f;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        if (g.w > 0) {
            const float x0 = x + pen + g.ox * k, y0 = base + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float P[4][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (int j : {0, 1, 2, 0, 2, 3}) out.insert(out.end(), P[j], P[j] + 4);
        }
        pen += g.adv * k + track * capPx;
        if (i + 1 < s.size()) {
            unsigned char nx = (unsigned char)s[i + 1];
            if (nx < 32 || nx >= 128) nx = '?';
            pen += f.kern[ch - 32][nx - 32] * k;
        }
    }
}

void RainUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float soft) {
    if (q.empty() || !f.ok || alpha <= 0.002f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set1f(progText_, "uAlpha", alpha);
    set1f(progText_, "uSoft", soft);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
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

// Manrope in a pale rain-light white over a soft dark halo (readable over the rain and the piles), labels light
// and spaced, the section in the street's first neon; the bonus gauge a ring of twelve drops filling with it.
void RainUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const VariantDef& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    const vec3 ink(0.92f, 0.93f, 0.96f), accent = lerp(hex(V.neon[0]), vec3(1.f), 0.15f), halo = hex(V.panel);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            box(r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.w * 0.5f, r.h * 0.5f, c, std::min(1.f, r.alpha));
        }
        flushShapes();
    }
    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        for (int i = 0; i < 12; i++) {
            const float a = -PI * 0.5f + i * TAU / 12.f;
            const float x = c.x + r * std::cos(a), y = c.y + r * std::sin(a);
            const float lit = saturate(fill * 12.f - i);
            const float dr = r * 0.11f;
            disc(x, y, dr, dr * 1.25f, ink, 0.22f * vis);
            if (lit > 0.f) disc(x, y, dr, dr * 1.25f, accent, lit * 0.95f * vis);
            if (g.bonusReady()) disc(x, y, dr * 2.6f, dr * 2.6f, accent, 0.25f * vis * (0.6f + 0.4f * std::sin(time_ * 2.f + i * 0.5f)), 1.f);
        }
        flushShapes();
    }

    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        const std::string text = ascii(h0.text);
        const float base = h0.scale * 12.f;
        const float alpha = saturate(h0.alpha) * vis;
        const Font* font = &light_;
        float cap = base * 0.5f, track = 0.04f, dy = 0.f, a = alpha;
        vec3 col = h0.accent ? accent : ink;
        switch (h0.role) {
        case HUD_UNIVERSE: cap = base * 0.5f; track = 0.14f; a = saturate(0.35f + h0.alpha * 1.4f) * vis; break;
        case HUD_SECTION: font = &med_; cap = base * 0.55f; track = 0.2f; col = accent; a = saturate(0.4f + h0.alpha * 1.5f) * vis; break;
        case HUD_TITLE: font = &med_; cap = base * 0.6f; track = 0.01f; a = saturate(h0.alpha * 1.6f) * vis; break;
        case HUD_INFO: cap = base * 0.5f; track = 0.06f; col = ink; a = saturate(alpha * 1.3f) * 0.85f; break;
        case HUD_LABEL: cap = base * 0.44f; track = 0.24f; dy = base * 0.1f; a = saturate(alpha * 1.3f) * 0.8f; break;
        case HUD_VALUE: font = &med_; cap = base * 0.64f; track = 0.02f; break;
        case HUD_NOTICE: font = &med_; cap = base * 0.55f; col = accent; break;
        case HUD_KEY: font = &med_; cap = base * 0.55f; break;
        case HUD_TOAST: font = &med_; cap = base * 0.72f; track = 0.08f; break;
        case HUD_TOAST_SUB: cap = base * 0.5f; col = accent; break;
        case HUD_DIALOG:
            if (h0.scale >= 3.5f * s) { font = &med_; cap = base * 0.62f; }
            break;
        default: break;
        }
        q.clear();
        const float w = capWidth(*font, text, track) * cap;
        const float left = ugl::hudLeft(h0, w, width_);
        textQuads(*font, text, left, h0.y + dy + cap, cap, track, q);
        drawText(*font, q, halo, a * 0.7f, 0.22f);
        drawText(*font, q, col, a);
    }
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
            sectionDrop = ascii(h.text) == "DROP";
        }
    const bool isDrop = !game.over() && level_ == 2 &&
                        (sectionKnown ? sectionDrop : dropMode_ && (!music.hasSong || music.speed > 1.26f));
    storm_ = approach(storm_, isDrop ? 1.f : 0.f, isDrop ? 1.1f : 0.3f, dt);
    {
        float target = level_ == 0 ? 0.16f + 0.12f * energy_ : level_ == 1 ? 0.48f : 0.74f;
        target = std::max(target, storm_);
        rain_ = approach(rain_, target, 0.7f, dt);
        glow_ = approach(glow_, 0.62f + 0.18f * level_ + 0.3f * storm_, 0.8f, dt);
        const float kick = std::max(music.kick, kickIn_);
        kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
        if (level_ == 2 && !f.paused) spawnAcc_ += kick * dt * 900.f; // a gust on the kicks
    }
    // soft lightning in the clouds, now and then on a bar line (only in the storm)
    {
        const double beat = music.hasSong ? music.beat : time_ * 2.0;
        const long bar = (long)std::floor(beat / 4.0);
        if (bar != flashBar_) {
            flashBar_ = bar;
            if (storm_ > 0.6f && !f.paused && rng_.chance(0.6f)) {
                flash_ = std::max(flash_, rng_.range(0.6f, 1.f));
                flashPos_ = vec2{rng_.range(-1.f, 1.f) * viewH_.x * 0.8f, rng_.range(4.f, 11.f)};
            }
        }
        flash_ = approach(flash_, 0.f, 1.4f, dt);
    }
    if (!bare_) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);
    // the HUD's bottom corner lines: the heaps dim under them
    if (!bare_) {
        textRects_.clear();
        for (const HudText& h : f.hud) {
            if (h.text.empty() || h.role == HUD_TOAST || h.role == HUD_TOAST_SUB || h.role == HUD_DIALOG) continue;
            const float base = h.scale * 12.f;
            const float w = capWidth(light_, ascii(h.text), 0.14f) * base * 0.55f;
            const float l = ugl::hudLeft(h, w, width_);
            const float x0 = viewC_.x + (l - 6.f - width_ * 0.5f) / unitPx_, x1 = viewC_.x + (l + w + 6.f - width_ * 0.5f) / unitPx_;
            const float y0 = viewC_.y + (height_ * 0.5f - (h.y + base * 0.9f)) / unitPx_, y1 = viewC_.y + (height_ * 0.5f - h.y + 4.f) / unitPx_;
            if (y0 > -3.f || std::fabs(0.5f * (x0 + x1)) < bandIn_) continue;
            textRects_.push_back({x0, y0, x1, y1});
        }
    }

    // ---- The world steps: the solids, the rain, the piles, the drying.
    buildSolids(game);
    stepRain(dt, rdt);
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
        set3f(progBg_, "uFlash", lerp(lerp(hex(V.skyLow), hex(V.neon[2]), 0.4f), vec3(0.85f, 0.85f, 1.f), 0.4f) * (0.6f * flash_));
        set2f(progBg_, "uFlashPos", flashPos_.x, flashPos_.y);
        set1f(progBg_, "uRain", rain_);
        set1f(progBg_, "uSlant", V.slant + 0.07f * storm_);
        set1f(progBg_, "uRainT", rainT_);
        set1f(progBg_, "uWell", bare_ ? 0.f : 1.f - smoothstepf(0.f, 0.8f, bridge_.amount));
        set3f(progBg_, "uBridge", ugl::toSrgb(bridge_.color));
        set1f(progBg_, "uBridgeK", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
        set1f(progBg_, "uDim", 0.f);
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
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_rain() {
    static const UniverseEntry e{
        "RAIN", "rain everywhere over a colorful street at dusk: it pours on little wet umbrellas (the falling piece shelters the cells under it), cleared rows are thrown out and tumble into heaps on the sides, the drop is a thunderstorm",
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
