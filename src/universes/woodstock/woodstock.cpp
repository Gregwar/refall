// WOODSTOCK: a calm 70s peace-and-love sticker sheet, flat and sober.
//
// Cream paper (a faint grain, a thin double border framing the screen) and, in the free space beyond the HUD
// columns, a few flat motifs in one even ink outline and a short warm palette: on the left a big sunburst disc (two
// rings of scallops turning very slowly around a yellow ring, a pale sky and an upright peace sign, on a soft halo);
// on the right two daisies on slim stems with two leaves each, swaying gently with the bars; a small heart and two
// small bolts. Lots of calm paper around them; no lettering in the scene. Now and then a petal comes off a daisy and
// drifts down.
//
// The well is a pale panel inside the same ink line. The blocks are flat stickers in the same language: one rounded
// square of flat color inside the ink outline (the same weight as the motifs'), a small pale glint in its corner;
// seven soft retro colors, one per piece type. The ghost is a faint wash of the piece's color inside a thin line of it.
//
// The music: calm and mid stay sparse and slow (the sway a little wider at mid). A DROP section (music.inDrop
// only) warms the paper and the halo, opens the halo into a burst of soft rays around the disc, makes the
// scallops pulse gently on the beat, the disc turn a little faster and the daisies shed more petals; it settles
// over a few seconds once the drop ends. A cleared row's cells shrink away, a petal drifts out of the well's side
// for each line and a heart pops softly over the rows (three for a quad); slow motion is dusk (the colors drained
// toward lilac, the motion slowed); game over greys the stack from the top.
//
// Four variants (SUNSHINE, APRICOT, SAGE, MIDNIGHT: cream motifs' lines on deep plum), from the key, mode, brightness
// and air. Original motifs only (generic period iconography). Cost: one fullscreen pass, a few instanced quads.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "layout.hpp"
#include "ugl.hpp"
#include "universes/registry.hpp"
#include "universes/woodstock/shaders.hpp"

namespace {

// Fraunces Soft Italic (a soft, Windsor-like 70s serif) and Comfortaa Bold (rounded), SIL Open Font License, see
// OFL-Fraunces.txt and OFL-Comfortaa.txt: the universe's own lettering (ZEN_HUD=universe).
const unsigned char FONT_DISPLAY[] = {
#include "universes/woodstock/Fraunces-SoftItalic.ttf.inc"
};
const unsigned char FONT_ROUND[] = {
#include "universes/woodstock/Comfortaa-Bold.ttf.inc"
};

namespace sh = woodstock_shaders;
using ugl::set1f;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant's colors (sRGB), by slot.
enum Slot {
    GROUND, FRAME, HALO, WELL, INK, BLOCK_INK, SCALLOP, SCALLOP2, RING, PEACE, SKY, GLINT, PETAL, CENTER, CENTER2, LEAF,
    HEART, BOLT, ACCENT, HUD_PALE, HUD_DEEP, PIECE0, SLOTS = PIECE0 + 7
};
struct VariantDef {
    const char* name;
    unsigned c[SLOTS];
};
const VariantDef VARIANTS[] = {
    // the reference: cream, red-orange, orange, yellow, a pale sky, teal leaves, a dark brown line
    {"SUNSHINE",
     {0xfbf0dc, 0xf1b48e, 0xf7dcc0, 0xfff8eb, 0x3b2723, 0x3b2723, 0xe2553b, 0xf08038, 0xf5bf47, 0xee7a36, 0x88b7d4,
      0xfcf3e2, 0xe5573c, 0xf5b843, 0xee7f35, 0x2e8b80, 0xe2553b, 0xf5bf47, 0xd9573a, 0xfff7e8, 0x3b2723,
      // I O T S Z J L
      0x7fb2d3, 0xf4be47, 0xa47aa6, 0x3f9a84, 0xe2553b, 0x3f6c9c, 0xf0893c}},
    // dusk: peach paper, dusty rose and apricot, a lilac sky, a plum line
    {"APRICOT",
     {0xf7e2d2, 0xdea08f, 0xf1cdb9, 0xfdf1e7, 0x4b2c37, 0x4b2c37, 0xcf6460, 0xe58e69, 0xf1c27f, 0xdc7c5e, 0xab9dcf,
      0xfbefe7, 0xd7696a, 0xf1c27f, 0xe08a5e, 0x5d8d84, 0xcf6460, 0xf0b56c, 0xc4515b, 0xfff4ec, 0x4b2c37,
      0x93b0d8, 0xf1c27f, 0x9a7fc0, 0x6aa38d, 0xcf5a5e, 0x5a6ba4, 0xe8875c}},
    // sage paper, white daisies, mustard and rust, a pale teal sky, a deep green line
    {"SAGE",
     {0xeeecd8, 0xaebc93, 0xdfe2c4, 0xf9f7ea, 0x2f3a31, 0x2f3a31, 0x5f9a7c, 0x8db68f, 0xe3b04b, 0xc8693b, 0x9cc4bd,
      0xf8f4e3, 0xfbf7ea, 0xe3b04b, 0xc8693b, 0x5c8e69, 0xc8693b, 0xe3b04b, 0xb4572f, 0xfbf9ef, 0x2f3a31,
      0x7cb3ae, 0xe3b04b, 0x977aa0, 0x6a9a5a, 0xc0473a, 0x46708a, 0xe8894a}},
    // night: deep plum paper, cream lines, the warm motifs dimmed a little
    {"MIDNIGHT",
     {0x2b1e33, 0x5d4362, 0x3a2944, 0x22172a, 0xf1e1c4, 0x170f1c, 0xd2603f, 0xe48a42, 0xe6b650, 0xe48a42, 0x7f9ec0,
      0xf1e1c4, 0xf1e1c4, 0xe6b650, 0xd2603f, 0x4f9a8c, 0xd2603f, 0xe6b650, 0xf0b04a, 0xf6ead6, 0x2b1e33,
      0x7fb0d0, 0xeabb52, 0xb48cc4, 0x59ac8e, 0xe0604a, 0x6383c6, 0xf0904a}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));
constexpr int V_SUNSHINE = 0, V_APRICOT = 1, V_SAGE = 2, V_MIDNIGHT = 3;

constexpr float CLEAR_T = 0.4f;    // a cleared cell shrinks away in this long (real time)
constexpr float OUTLINE = 0.085f;  // the ink line, everywhere (board units)

float ease(float t) {
    t = saturate(t);
    return t * t * (3.f - 2.f * t);
}

class WoodstockUniverse : public Universe {
public:
    ~WoodstockUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override { return layout::project(vp_, p, width_, height_); }
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    HudInk hudInk(const HudText& h) override;

private:
    // ---- Petals and hearts (board units).
    struct Sprite { float a[4], b[4], c[4], d[4]; };
    std::vector<Sprite> sprites_;
    void flushSprites();
    struct Part {
        float x, y, vx, vy, rot, size, age, life, phase;
        int kind;   // 0 a petal, 1 a heart
        bool drift; // a daisy's petal drifting down (else one of the clears')
        vec3 col;
    };
    std::vector<Part> parts_;
    void drawParts(int kind);
    float nextPetal_ = 4.f;

    // ---- Blocks.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float rot, int type, float alpha, int mode, float scale, float k);
    void buildBoard(const Game& g);
    struct Pop { float x, y; int type; float t; };
    std::vector<Pop> pops_;

    // ---- The HUD's plates (its lettering: the host's).
    float fade_ = 1.f;
    void drawHud(const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progBg_ = 0, progSprite_ = 0, progCell_ = 0, progSolid_ = 0;
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    void solid(float r, float g, float b, float a);

    // ---- The motifs' places (board units, from the view: resize).
    float zoneX_ = 16.1f, zoneS_ = 1.f;
    vec2 head_[2], foot_[2];

    // ---- Song, variant, colors (sRGB, eased on a change of song).
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 col_[SLOTS];
    bool colSet_ = false;

    // ---- Staging.
    int level_ = 1;
    float lv_ = 1.f;
    float dropAmt_ = 0.f, spin_ = 0.f, turn_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Rng rng_{0x60D5u};
};

// =====================================================================================================
// Setup
// =====================================================================================================

bool WoodstockUniverse::init(int width, int height) {
    const std::string head = "#version 330 core\n";
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, (head + sh::PAINT_GLSL + sh::BG_FS).c_str(), "woodstock paper");
    progSprite_ = ugl::program(sh::SPRITE_VS, (head + sh::PAINT_GLSL + sh::SPRITE_FS).c_str(), "woodstock petals");
    progCell_ = ugl::program(sh::CELL_VS, (head + sh::PAINT_GLSL + sh::CELL_FS).c_str(), "woodstock blocks");
    progSolid_ = ugl::program(ugl::FULLSCREEN_VS, sh::SOLID_FS, "woodstock solid");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, int vec4s) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < vec4s; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4 * vec4s, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(spriteVao_, spriteVbo_, 4);
    instanced(cellVao_, cellVbo_, 3);
    glBindVertexArray(0);

    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void WoodstockUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    // the free zones beyond the HUD columns (9.8 wide at 16:9): the motifs centered in them, scaled to fit
    const float zoneW = hw - layout::RIGHT_COLUMN[2];
    zoneX_ = layout::RIGHT_COLUMN[2] + zoneW * 0.5f;
    zoneS_ = std::clamp(zoneW / 9.8f, 0.3f, 1.25f);
}

WoodstockUniverse::~WoodstockUniverse() {
    for (GLuint p : {progBg_, progSprite_, progCell_, progSolid_}) glDeleteProgram(p);
    for (GLuint v : {spriteVao_, cellVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, cellVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level, events
// =====================================================================================================

void WoodstockUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = V_MIDNIGHT;            // a dark song: night
        else if (t.hasSong && !t.minor && t.brightness > 0.6f) v = V_SUNSHINE;       // a bright, major one
        else if (t.hasSong && t.airWeight > 0.6f) v = V_SAGE;                        // airy
        else {
            static const int BY_HUE[] = {V_SUNSHINE, V_APRICOT, V_SAGE, V_APRICOT, V_SAGE, V_SUNSHINE};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x5EACEull);
    if (seconds <= 0.f) colSet_ = false;
}

void WoodstockUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)energy;
    (void)seconds;
    (void)impact;
    level_ = std::clamp(level, 0, 2);
}

TransitionHints WoodstockUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = ugl::toLinear(hex(V.c[GROUND]));
    h.ink = ugl::toLinear(hex(variant_ == V_MIDNIGHT ? V.c[INK] : V.c[BLOCK_INK]));
    h.accent = ugl::toLinear(hex(V.c[ACCENT]));
    h.light = variant_ != V_MIDNIGHT;
    h.styles = TS_INK | TS_TILES;
    return h;
}

void WoodstockUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Clear: {
        float yTop = -99.f;
        std::vector<int> rows;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            pops_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), 0.f});
            yTop = std::max(yTop, p.y);
            if (std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        }
        if (rows.empty()) break;
        // a petal drifting out of the well's side for each line, in the color of one of its cells
        for (int r : rows) {
            const float side = rng_.chance(0.5f) ? 1.f : -1.f;
            std::vector<int> types;
            for (auto& c : ev.cells) if (c.y == r) types.push_back(c.type);
            const int type = types[rng_.irange(0, (int)types.size() - 1)];
            Part q;
            q.x = side * 5.2f;
            q.y = layout::cellPos(0.f, (float)r).y;
            q.vx = side * rng_.range(1.4f, 2.2f);
            q.vy = rng_.range(0.4f, 0.9f);
            q.rot = side > 0.f ? rng_.range(-0.4f, 0.4f) : PI + rng_.range(-0.4f, 0.4f);
            q.size = 0.3f;
            q.age = -rng_.range(0.f, 0.15f);
            q.life = rng_.range(2.6f, 3.4f);
            q.phase = rng_.range(0.f, TAU);
            q.kind = 0;
            q.drift = false;
            q.col = col_[PIECE0 + std::clamp(type, 0, 6)];
            parts_.push_back(q);
        }
        // a heart popping softly over the rows (three for a quad)
        const int hearts = ev.count >= 4 ? 3 : 1;
        for (int i = 0; i < hearts; i++) {
            Part q;
            q.x = hearts == 1 ? rng_.range(-1.5f, 1.5f) : (i - 1) * 2.6f + rng_.range(-0.4f, 0.4f);
            q.y = yTop + 0.3f;
            q.vx = 0.f;
            q.vy = 1.1f;
            q.rot = rng_.range(-0.15f, 0.15f);
            q.size = hearts == 1 ? 0.62f : (i == 1 ? 0.75f : 0.55f);
            q.age = -0.12f * i;
            q.life = 1.5f;
            q.phase = 0.f;
            q.kind = 1;
            q.drift = false;
            q.col = col_[HEART];
            parts_.push_back(q);
        }
        break;
    }
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

// =====================================================================================================
// Board
// =====================================================================================================

void WoodstockUniverse::cell(float x, float y, float rot, int type, float alpha, int mode, float scale, float k) {
    if (alpha <= 0.003f || scale <= 0.f) return;
    const vec3 c = col_[PIECE0 + std::clamp(type, 0, 6)];
    cells_.push_back({{x, y, 0.5f, rot}, {(float)type, alpha, (float)mode, scale}, {c.x, c.y, c.z, k}});
}

void WoodstockUniverse::buildBoard(const Game& g) {
    const float overRows = over_ * 26.f;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            float k = -0.3f * std::pow(saturate(c.flash), 8.f);
            if (over_ > 0.f) k = 0.7f * saturate(overRows - (y - Game::HIDDEN));
            cell(p.x, p.y, 0.f, c.type, 1.f, 0, 1.f, k);
        }
    for (const Pop& p : pops_) {
        const float t = saturate(p.t / CLEAR_T);
        cell(p.x, p.y, 0.f, p.type, 1.f - ease((t - 0.5f) / 0.5f), 0, 1.f - 0.6f * ease(t), -0.35f * (1.f - t));
    }
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cs[4][2];
        g.pieceCells(p, cs);
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cell(w.x, w.y, 0.f, p.type, 1.f, 1, 1.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cs) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.f, p.type, 1.f, 0, 1.f, 0.f);
        }
    }
    auto preview = [&](int type, vec2 center, float sc, float alpha) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cs[4][2];
        g.pieceCells(p, cs);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cs) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (auto& c : cs)
            cell(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc, 0.f, type,
                 alpha, 0, sc, 0.f);
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void WoodstockUniverse::drawParts(int kind) {
    const vec3 ink = col_[INK];
    for (const Part& p : parts_) {
        if (p.kind != kind || p.age < 0.f) continue;
        float a = saturate(p.age / 0.25f) * (1.f - ease((p.age - p.life + 1.f) / 1.f));
        float hx = p.size, hy = p.size * 0.48f;
        if (kind == 1) {
            // the heart pops in (a soft overshoot) and fades as it rises
            const float t = saturate(p.age / 0.35f);
            const float s = 1.f + 0.15f * std::sin(t * PI) * (t < 1.f ? 1.f : 0.f);
            hx = hy = p.size * ease(t * 1.6f) * s;
            a = saturate(p.age / 0.12f) * (1.f - ease((p.age - p.life + 0.6f) / 0.6f)) * 0.95f;
        }
        if (a <= 0.004f || hx <= 0.f) continue;
        sprites_.push_back({{p.x, p.y, hx, hy}, {p.rot, (float)kind, a, OUTLINE}, {p.col.x, p.col.y, p.col.z, 0.f},
                            {ink.x, ink.y, ink.z, 0.f}});
    }
    flushSprites();
}

void WoodstockUniverse::flushSprites() {
    if (sprites_.empty()) return;
    glUseProgram(progSprite_);
    ugl::setMat(progSprite_, "uVP", vp_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(Sprite), sprites_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    sprites_.clear();
}

void WoodstockUniverse::solid(float r, float g, float b, float a) {
    if (a <= 0.002f) return;
    glUseProgram(progSolid_);
    set4f(progSolid_, "uColor", r * a, g * a, b * a, a);
    ugl::drawFullscreen();
}

// =====================================================================================================
// Text
// =====================================================================================================

// The HUD's plates: plain rectangles in the well's color (its lettering: the host's).
void WoodstockUniverse::drawHud(const std::vector<HudRect>& panels) {
    if (panels.empty()) return;
    glUseProgram(progSolid_);
    glEnable(GL_SCISSOR_TEST);
    for (const HudRect& r : panels) {
        const vec3 c = r.custom ? r.color : r.accent ? col_[ACCENT] : col_[WELL];
        glScissor((GLint)r.x, (GLint)(height_ - r.y - r.h), (GLsizei)std::max(0.f, r.w), (GLsizei)std::max(0.f, r.h));
        solid(c.x, c.y, c.z, std::min(1.f, r.alpha));
    }
    glDisable(GL_SCISSOR_TEST);
}

// The unified HUD's inks: the paper's dark line, a warm cream, the variant's accent.
HudInk WoodstockUniverse::hudInk(const HudText& h) {
    (void)h;
    HudInk k;
    k.pale = col_[HUD_PALE];
    k.deep = col_[HUD_DEEP];
    k.accent = col_[ACCENT];
    return k;
}

// The universe's own lettering (ZEN_HUD=universe): the line's ink on the paper (cream on MIDNIGHT), a soft halo of
// the paper; Comfortaa for the labels and numbers, Fraunces' soft italics for the song, the notices and the toasts.
void WoodstockUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* disp = text::font(FONT_DISPLAY);
    text::Font* round = text::font(FONT_ROUND);
    const vec3 ink = variant_ == V_MIDNIGHT ? col_[HUD_PALE] : col_[HUD_DEEP];
    const vec3 acc = col_[ACCENT];
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        if (!var.empty()) {
            size_t n = 1;
            while (n < var.size() && ((unsigned char)var[n] & 0xC0) == 0x80) n++;
            var = var.substr(0, n) + text::lower(var.substr(n));
        }
        const float sz = base * 0.95f, vsz = base * 1.3f, gap = base * 0.7f, a = saturate(0.35f + h.alpha * 1.6f);
        const float vdy = -sz * 0.22f + text::capHeight(disp) * vsz - text::capHeight(round) * sz;
        out.add(name, round, sz, ink, 0.18f, a);
        if (!var.empty()) {
            text::Span& v = out.add(var, disp, vsz, acc, 0.f, a);
            v.dy = vdy;
            v.gap = (gap - 0.18f * sz) / vsz;
        }
        break;
    }
    case HUD_SECTION: put(round, h.text, base * 0.9f, base * 0.3f, acc, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(disp, h.text, base * 1.7f, 0.f, ink, h.alpha * 1.3f, -base * 0.35f); break;
    case HUD_INFO: put(round, h.text, base * 0.7f, base * 0.08f, ink, alpha * 1.3f); break;
    case HUD_LABEL: put(round, h.text, base * 0.8f, base * 0.28f, acc, alpha * 1.2f); break;
    case HUD_VALUE: put(round, h.text, base * 1.1f, base * 0.04f, ink, alpha); break;
    case HUD_NOTICE: put(disp, h.text, base * 1.6f, 0.f, acc, alpha); break;
    case HUD_KEY: put(round, h.text, base * 0.75f, base * 0.06f, ink, alpha); break;
    case HUD_TOAST: put(disp, h.text, base * 1.6f, base * 0.06f, acc, alpha, -base * 0.3f); break;
    case HUD_TOAST_SUB: put(round, h.text, base * 1.1f, base * 0.1f, ink, alpha * 0.95f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * height_ / 1080.f) put(disp, h.text, base * 1.3f, 0.f, ink, alpha, -base * 0.1f);
        else put(round, h.text, base * 0.85f, base * 0.1f, h.accent ? acc : ink, alpha);
        break;
    default: put(round, h.text, base * 0.85f, base * 0.08f, h.accent ? acc : ink, alpha); break;
    }
    out.look.halo = col_[GROUND];
    out.look.haloA = 0.6f;
    out.look.haloW = 0.14f;
    out.alpha = saturate(fade_);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void WoodstockUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);

    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 2.5f) : 0.f, overT_ >= 0.f ? 3.f : 1.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    time_ += wdt;

    // ---- The colors: the variant's, eased in on a change of song.
    {
        const VariantDef& V = VARIANTS[variant_];
        const float k = colSet_ ? 1.f - std::exp(-dt * 1.2f) : 1.f;
        colSet_ = true;
        for (int i = 0; i < SLOTS; i++) col_[i] = lerp(col_[i], hex(V.c[i]), k);
    }

    // ---- Staging: the level eased (the sway's width), the drop (music.inDrop only).
    lv_ = approach(lv_, (float)level_, 0.6f, dt);
    const float wm = smoothstepf(0.2f, 1.f, lv_);
    const bool inDrop = music.inDrop && overT_ < 0.f;
    dropAmt_ = approach(dropAmt_, inDrop ? 1.f : 0.f, inDrop ? 1.2f : 0.5f, dt);
    const float drop = ease(dropAmt_);
    const double beats = music.hasSong ? music.beat : time_ * 2.0;
    // the scallops' gentle pulse on each beat of the drop
    const float bf = (float)(beats - std::floor(beats));
    const float pulse = drop * std::exp(-bf * 4.f);
    spin_ += wdt * (0.035f + 0.02f * wm + 0.05f * drop);

    // ---- The motifs' places (they lean with the bars: a two-bar sway).
    const float zx = zoneX_, s = zoneS_;
    const float bar = (float)std::fmod(beats / 8.0, 1.0) * TAU;
    const float sway = 0.022f + 0.018f * wm + 0.025f * drop;
    head_[0] = vec2{zx + 1.3f * s, 3.9f * s};
    foot_[0] = vec2{zx - 0.5f * s, -1.3f * s};
    head_[1] = vec2{zx - 1.6f * s, -3.1f * s};
    foot_[1] = vec2{zx + 0.3f * s, -7.6f * s};
    const float lean[2] = {sway * std::sin(bar), sway * std::sin(bar - 1.7f)};
    turn_ += wdt * 0.04f;

    // ---- Petals now and then off the daisies (more in a drop), drifting down.
    nextPetal_ -= wdt;
    if (nextPetal_ <= 0.f && !f.paused) {
        const int i = rng_.irange(0, 1);
        const vec2 ft = foot_[i];
        const float hdx = head_[i].x - ft.x, hdy = head_[i].y - ft.y;
        const float c = std::cos(lean[i]), sn = std::sin(lean[i]);
        const vec2 h{ft.x + c * hdx - sn * hdy, ft.y + sn * hdx + c * hdy};
        Part q;
        const float an = rng_.range(0.f, TAU);
        const float r = (i ? 1.35f : 1.75f) * s * 0.75f;
        q.x = h.x + std::cos(an) * r;
        q.y = h.y + std::sin(an) * r;
        q.vx = rng_.range(-0.35f, 0.05f);
        q.vy = -rng_.range(0.35f, 0.55f);
        q.rot = an;
        q.size = 0.34f * s;
        q.age = 0.f;
        q.life = rng_.range(6.f, 8.f);
        q.phase = rng_.range(0.f, TAU);
        q.kind = 0;
        q.drift = true;
        q.col = col_[PETAL];
        parts_.push_back(q);
        nextPetal_ = drop > 0.5f ? rng_.range(1.2f, 2.4f) : wm > 0.5f ? rng_.range(6.f, 9.f) : rng_.range(9.f, 14.f);
    }

    // ---- Particles, pops.
    for (Pop& p : pops_) p.t += dt;
    pops_.erase(std::remove_if(pops_.begin(), pops_.end(), [](const Pop& p) { return p.t > CLEAR_T; }), pops_.end());
    for (Part& p : parts_) {
        p.age += wdt;
        if (p.age < 0.f) continue;
        if (p.drift) {
            // a slow fall, swaying from side to side
            const float w = std::sin(p.age * 1.3f + p.phase);
            p.x += (p.vx + 0.45f * w) * wdt;
            p.y += p.vy * wdt;
            p.rot += 0.6f * std::cos(p.age * 1.3f + p.phase) * wdt;
        } else if (p.kind == 0) {
            // out of the well's side, slowing, then falling softly
            p.x += p.vx * wdt;
            p.y += p.vy * wdt;
            p.vx *= std::exp(-1.1f * wdt);
            p.vy = approach(p.vy, -0.45f, 0.9f, wdt);
            p.rot += 0.5f * std::sin(p.age * 2.f + p.phase) * wdt;
        } else {
            p.y += p.vy * wdt;
            p.vy *= std::exp(-1.2f * wdt);
        }
    }
    parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Part& p) { return p.age >= p.life; }), parts_.end());

    // =========================================================== the paper and its motifs
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    {
        // a drop warms the paper and the halo
        const vec3 ground = lerp(col_[GROUND], col_[HALO], 0.3f * drop);
        const vec3 halo = lerp(col_[HALO], col_[SCALLOP2], 0.38f * drop);
        glUseProgram(progBg_);
        set4f(progBg_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set1f(progBg_, "uW", OUTLINE);
        set3f(progBg_, "uGround", ground);
        set3f(progBg_, "uFrame", col_[FRAME]);
        set3f(progBg_, "uHalo", halo);
        set3f(progBg_, "uWellFill", col_[WELL]);
        set3f(progBg_, "uInk", col_[INK]);
        set3f(progBg_, "uScallop", col_[SCALLOP]);
        set3f(progBg_, "uScallop2", col_[SCALLOP2]);
        set3f(progBg_, "uRing", col_[RING]);
        set3f(progBg_, "uPeace", col_[PEACE]);
        set3f(progBg_, "uSky", col_[SKY]);
        set3f(progBg_, "uGlint", col_[GLINT]);
        set3f(progBg_, "uPetal", col_[PETAL]);
        set3f(progBg_, "uCenter", col_[CENTER]);
        set3f(progBg_, "uCenter2", col_[CENTER2]);
        set3f(progBg_, "uLeaf", col_[LEAF]);
        set3f(progBg_, "uHeart", col_[HEART]);
        set3f(progBg_, "uBolt", col_[BOLT]);
        set4f(progBg_, "uDisc", -zx + 0.35f * s, 1.2f * s, 3.4f * s, spin_);
        set2f(progBg_, "uDiscFx", drop, pulse);
        const float heads[8] = {head_[0].x, head_[0].y, 1.75f * s, turn_ + lean[0] * 2.f,
                                head_[1].x, head_[1].y, 1.35f * s, -turn_ * 0.8f + lean[1] * 2.f};
        glUniform4fv(ugl::U(progBg_, "uHead"), 2, heads);
        const float stems[8] = {foot_[0].x, foot_[0].y, 1.5f * s, lean[0], foot_[1].x, foot_[1].y, 1.25f * s, lean[1]};
        glUniform4fv(ugl::U(progBg_, "uStem"), 2, stems);
        set4f(progBg_, "uHeartAt", -zx - 2.4f * s, -6.4f * s, 0.7f * s, 0.22f + 0.04f * std::sin(bar + 0.6f));
        const float bolts[8] = {-zx + 2.8f * s, 7.7f, 0.85f * s, -0.18f + 0.03f * std::sin(bar + 2.4f),
                                zx - 2.5f * s, 7.9f, 0.8f * s, 0.22f + 0.03f * std::sin(bar + 4.f)};
        glUniform4fv(ugl::U(progBg_, "uBoltAt"), 2, bolts);
        set4f(progBg_, "uWell", 5.15f, 10.15f, 0.3f, f.bare ? 0.f : 1.f);
        set1f(progBg_, "uDesat", 0.4f * slow_ + 0.2f * over_);
        set1f(progBg_, "uGrain", variant_ == V_MIDNIGHT ? 0.05f : 0.025f);
        ugl::drawFullscreen();
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    drawParts(0);
    if (!f.bare) {
        // =========================================================== the blocks
        cells_.clear();
        buildBoard(game);
        if (!cells_.empty()) {
            glUseProgram(progCell_);
            ugl::setMat(progCell_, "uVP", vp_);
            set3f(progCell_, "uInk", col_[BLOCK_INK]);
            set3f(progCell_, "uShine", vec3(1.f, 0.98f, 0.94f));
            set1f(progCell_, "uW", OUTLINE);
            glBindVertexArray(cellVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
            glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
        }
    }
    drawParts(1);
    solid(0.f, 0.f, 0.f, 0.4f * dimT_);
    drawHud(f.panels);
    fade_ = f.fade;
    solid(0.f, 0.f, 0.f, 1.f - saturate(f.fade));
    glDisable(GL_BLEND);
}

} // namespace

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_woodstock() {
    static const UniverseEntry e{
        "WOODSTOCK", "a calm 70s peace-and-love sticker sheet: cream paper, a slow sunburst peace disc, swaying daisies",
        [] { return std::unique_ptr<Universe>(new WoodstockUniverse()); },
        // Bright, happy, easy songs suit it best.
        [](const SongLook& l) { return 0.65f + 0.35f * l.brightness + (l.minor ? 0.f : 0.15f) + 0.1f * (1.f - l.bassWeight); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
