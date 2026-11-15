// WOODSTOCK: 70s groovy bands, pastel and chill.
//
// The whole screen is one big slow pattern: huge rainbow arcs rising from below the screen in six soft pastel bands
// (peach, butter, sky... per variant), layered like cut paper (each band laid over the next one out, a soft shadow
// along its edge, a thin pale line between them), melting into slow waves that travel along them; the arcs widen
// as they rise, so the sides of the screen get the biggest bands. They drift upward very slowly, the waves roll on,
// the arcs' center wanders a little and the bands swell and settle with the bars. A faint paper grain. No motifs.
//
// The well is a calm pale panel inside a slim line, a soft shadow under it. The blocks are truly flat: one square
// of a single flat color per cell (no outline, no shading, no highlight), a hairline gap between cells, the corners
// barely rounded; seven soft retro colors, one per piece type. The ghost is a faint wash of the piece's color inside
// a thin line of it.
//
// The music: calm is very slow; mid flows a little more. A DROP section (music.inDrop only) is the trip, there within
// a second: the arcs' center rises behind the well and swings slowly, so the arcs close into clean full rings
// radiating outward at an even pace, folding into six shallow petals and twisting into a
// slow six-armed spiral, the waves calmer and simpler than outside it; the bands cross-fade (each one whole) into the
// variant's drop palette (a sunset; rose, violet and teal; turquoise, lime and lemon; neon pastels between bands of
// night), a soft light glows from behind the well and the pattern breathes very gently with each bar. It eases back to
// the calm arcs and colors over a few seconds once the drop ends; nothing strobes. A line clear sends one soft ripple out through the bands from the
// cleared rows (a stronger one for more lines); nothing flies. Slow motion is dusk (the colors drained toward lilac,
// the flow slowed); game over greys the stack from the top.
//
// Four pastel variants (SUNSHINE, APRICOT, SAGE, MIDNIGHT: a deep pastel night), from the key, mode, brightness and
// air. A fifth, BLOOM (by name only: --universe woodstock:BLOOM), has its own background, an infinite flower: a
// dahlia centered behind the well whose rings of petals repeat at every scale (log-polar, so one level of zoom gives
// the very same picture and the zoom loops forever), each ring one flat step of a smooth pastel gradient, laid over
// the next like cut paper. Each line clear zooms in a little (eased); calm turns it very slowly; a DROP zooms in
// steadily (eased in and to a stop after it), the colors sliding along the gradient. Cost: one fullscreen pass, the
// blocks as instanced quads.
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

// A variant's colors (sRGB), by slot: the six bands (from the inside out), the lines between them, the shadows'
// tint, the well and its line, the accent, the HUD's two inks, the seven pieces.
enum Slot {
    BAND0, DROP0 = BAND0 + 6, LINE = DROP0 + 6, SHADE, WELL, WELL_LINE, ACCENT, HUD_PALE, HUD_DEEP, PIECE0, SLOTS = PIECE0 + 7
};
struct VariantDef {
    const char* name;
    unsigned c[SLOTS];
};
const VariantDef VARIANTS[] = {
    // peach, butter, sky
    {"SUNSHINE",
     {0xfbe2a4, 0xf9c7a3, 0xf5b0a5, 0xfbead7, 0xbddcec, 0xf6cad6,
      // the drop: a sunset, saturated but still pastel (gold, tangerine, coral, magenta-pink, violet, peach)
      0xffd166, 0xffa25e, 0xff7f7a, 0xf47fb5, 0xb98be6, 0xffc2a8,
      0xfff8ee, 0xc58f86, 0xfffaf2, 0xe0ad94, 0xd9573a, 0xfff7e8, 0x55392f,
      // I O T S Z J L
      0x7fb2d3, 0xedae2c, 0xa47aa6, 0x3f9a84, 0xe2553b, 0x3f6c9c, 0xf0893c}},
    // dusk: apricot, rose, lilac, sand
    {"APRICOT",
     {0xf8c8a8, 0xf1b0b2, 0xd8bade, 0xf8dcc4, 0xf4bf9e, 0xe6c6df,
      // the drop: rose, violet, teal
      0xf58fa8, 0xc48be0, 0x6fc9c2, 0xffb3a1, 0x9b8ef0, 0xf7c6d9,
      0xfff3ec, 0xb07f8e, 0xfff6f0, 0xd7a3a3, 0xc4515b, 0xfff4ec, 0x4b2c37,
      0x93b0d8, 0xe6a43a, 0x9a7fc0, 0x6aa38d, 0xcf5a5e, 0x5a6ba4, 0xe8875c}},
    // sage, butter, seafoam, sand
    {"SAGE",
     {0xcfe3c6, 0xf2e5bd, 0xb7d7d1, 0xf5eddb, 0xe5cba8, 0xc8dcb2,
      // the drop: turquoise, lime, lemon
      0x5fd3c7, 0xb5e36b, 0xfbe97a, 0x8fe0b0, 0x43b8d1, 0xe3f59a,
      0xfbfaf0, 0x8c9c87, 0xfbfbf3, 0xa9bfa2, 0xb4572f, 0xfbf9ef, 0x2f3a31,
      0x7cb3ae, 0xd99f2b, 0x977aa0, 0x6a9a5a, 0xc0473a, 0x46708a, 0xe8894a}},
    // a deep pastel night: dusty indigo, violet, mauve, rose
    {"MIDNIGHT",
     {0x4a4275, 0x5f4f86, 0x7c6594, 0x3f4b78, 0x8a6c95, 0x585a8b,
      // the drop: glowing neon pastels (cyan, pink, lavender) between bands of deep night
      0x241d45, 0x62c9dc, 0x2e2556, 0xf27fc2, 0x2a2350, 0xa995f2,
      0xa898c8, 0x1c1630, 0x221c35, 0x8f80b4, 0xf3b6a0, 0xf6ead6, 0x221c35,
      0x7fb0d0, 0xeabb52, 0xc49ad4, 0x59ac8e, 0xe0604a, 0x6f8fd6, 0xf0904a}},
    // the infinite flower (its own background, BLOOM_FS): a pastel gradient through peach, pink, lilac, sky, mint
    // and butter, one step every other ring
    {"BLOOM",
     {0xf9c6a8, 0xf4adc4, 0xcdb5ea, 0xaed4f0, 0xb8e6d0, 0xf8e3a6,
      // the drop: the same gradient a little richer
      0xffb48f, 0xf795bb, 0xbb9ef0, 0x92c6f2, 0x99e0c0, 0xffd982,
      0xfff9f2, 0xb8919c, 0xfffaf4, 0xd8b2bd, 0xd0566e, 0xfff7ee, 0x4a3242,
      0x7fb2d3, 0xedae2c, 0xa47aa6, 0x3f9a84, 0xe2553b, 0x3f6c9c, 0xf0893c}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));
constexpr int V_SUNSHINE = 0, V_APRICOT = 1, V_SAGE = 2, V_MIDNIGHT = 3, V_BLOOM = 4; // (BLOOM: by name only)

constexpr float CLEAR_T = 0.4f;    // a cleared cell shrinks away in this long (real time)
const vec2 ARC_CENTER{0.f, -15.f}; // the arcs' center, below the screen (board units)

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
    // ---- Blocks.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, int type, float alpha, int mode, float scale, float k);
    void buildBoard(const Game& g);
    struct Pop { float x, y; int type; float t; };
    std::vector<Pop> pops_;

    // ---- The clears' ripples through the bands (radius they start at, age in seconds, strength).
    struct Ripple { float r0, age, strength; };
    std::vector<Ripple> ripples_;

    // ---- The HUD's plates (its lettering: the host's).
    float fade_ = 1.f;
    void drawHud(const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progBg_ = 0, progBloom_ = 0, progCell_ = 0, progSolid_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    void solid(float r, float g, float b, float a);

    // ---- Song, variant, colors (sRGB, eased on a change of song).
    int variant_ = 0;
    vec3 col_[SLOTS];
    bool colSet_ = false;

    // ---- Staging.
    int level_ = 1;
    float lv_ = 1.f;
    float dropAmt_ = 0.f, flow_ = 0.f, phase_ = 0.f, wobble_ = 1.2f, spin_ = 0.f, dropMix_ = 0.f;
    // BLOOM: the zoom (in levels) eased toward its target, the drop's steady zoom speed, the flower's turn
    float zoomLv_ = 0.f, levelTo_ = 0.f, zoomVel_ = 0.f, bloomSpin_ = 0.f;
    vec2 center_ = ARC_CENTER;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
};

// =====================================================================================================
// Setup
// =====================================================================================================

bool WoodstockUniverse::init(int width, int height) {
    const std::string head = "#version 330 core\n";
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, (head + sh::PAINT_GLSL + sh::BG_FS).c_str(), "woodstock bands");
    progBloom_ = ugl::program(ugl::FULLSCREEN_VS, (head + sh::PAINT_GLSL + sh::BLOOM_FS).c_str(), "woodstock bloom");
    progCell_ = ugl::program(sh::CELL_VS, (head + sh::PAINT_GLSL + sh::CELL_FS).c_str(), "woodstock blocks");
    progSolid_ = ugl::program(ugl::FULLSCREEN_VS, sh::SOLID_FS, "woodstock solid");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &cellVao_);
    glGenBuffers(1, &cellVbo_);
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    for (int i = 0; i < 3; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(CellInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
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
}

WoodstockUniverse::~WoodstockUniverse() {
    for (GLuint p : {progBg_, progBloom_, progCell_, progSolid_}) glDeleteProgram(p);
    glDeleteVertexArrays(1, &cellVao_);
    for (GLuint b : {quadVbo_, cellVbo_}) glDeleteBuffers(1, &b);
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
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % V_BLOOM;
    }
    variant_ = v;
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
    vec3 bg(0.f);
    for (int i = 0; i < 6; i++) bg += hex(V.c[BAND0 + i]) * (1.f / 6.f);
    h.background = ugl::toLinear(bg);
    h.ink = ugl::toLinear(hex(V.c[variant_ == V_MIDNIGHT ? HUD_PALE : HUD_DEEP]));
    h.accent = ugl::toLinear(hex(V.c[ACCENT]));
    h.light = variant_ != V_MIDNIGHT;
    h.styles = TS_RIPPLE | TS_INK;
    return h;
}

void WoodstockUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Clear: {
        float yMid = 0.f;
        int n = 0;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            pops_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), 0.f});
            yMid += p.y;
            n++;
        }
        if (!n) break;
        // one soft ripple out through the bands from the cleared rows
        if (ripples_.size() >= 4) ripples_.erase(ripples_.begin());
        ripples_.push_back({std::abs(yMid / n - center_.y), 0.f, 0.22f + 0.08f * std::min(ev.count, 4)});
        // BLOOM: the flower zooms in a little with each clear (eased: render)
        levelTo_ += 0.18f + 0.07f * (std::min(ev.count, 4) - 1);
        break;
    }
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

// =====================================================================================================
// Board
// =====================================================================================================

void WoodstockUniverse::cell(float x, float y, int type, float alpha, int mode, float scale, float k) {
    if (alpha <= 0.003f || scale <= 0.f) return;
    const vec3 c = col_[PIECE0 + std::clamp(type, 0, 6)];
    cells_.push_back({{x, y, 0.5f, 0.f}, {(float)type, alpha, (float)mode, scale}, {c.x, c.y, c.z, k}});
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
            cell(p.x, p.y, c.type, 1.f, 0, 1.f, k);
        }
    for (const Pop& p : pops_) {
        const float t = saturate(p.t / CLEAR_T);
        cell(p.x, p.y, p.type, 1.f - ease((t - 0.5f) / 0.5f), 0, 1.f - 0.6f * ease(t), -0.35f * (1.f - t));
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
                cell(w.x, w.y, p.type, 1.f, 1, 1.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cs) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, p.type, 1.f, 0, 1.f, 0.f);
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
            cell(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc, type,
                 alpha, 0, sc, 0.f);
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
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

// The unified HUD's inks: a warm cream, the variant's deep tone, its accent.
HudInk WoodstockUniverse::hudInk(const HudText& h) {
    (void)h;
    HudInk k;
    k.pale = col_[HUD_PALE];
    k.deep = col_[HUD_DEEP];
    k.accent = col_[ACCENT];
    return k;
}

// The universe's own lettering (ZEN_HUD=universe): the deep ink on the pastel bands (cream on MIDNIGHT), a soft
// halo of the bands; Comfortaa for the labels and numbers, Fraunces' soft italics for the song, the notices and the
// toasts.
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
    out.look.halo = variant_ == V_MIDNIGHT ? col_[WELL] : col_[HUD_PALE];
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
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += wdt;

    // ---- The colors: the variant's, eased in on a change of song.
    {
        const VariantDef& V = VARIANTS[variant_];
        const float k = colSet_ ? 1.f - std::exp(-dt * 1.2f) : 1.f;
        colSet_ = true;
        for (int i = 0; i < SLOTS; i++) col_[i] = lerp(col_[i], hex(V.c[i]), k);
    }

    // ---- Staging: the level eased, the drop (music.inDrop only), all of it smooth.
    lv_ = approach(lv_, (float)level_, 0.5f, dt);
    const float wm = smoothstepf(0.2f, 1.f, lv_);
    const bool inDrop = music.inDrop && overT_ < 0.f;
    // the drop: in within a second, out over a few once it ends
    dropAmt_ = approach(dropAmt_, inDrop ? 1.f : 0.f, inDrop ? 2.2f : 0.4f, dt);
    const float drop = ease(dropAmt_);
    dropMix_ = approach(dropMix_, inDrop ? 1.f : 0.f, inDrop ? 3.f : 0.6f, dt);   // the drop's palette, in ~1 s
    const double beats = music.hasSong ? music.beat : time_ * 2.0;
    const float bar1 = (float)std::fmod(beats / 4.0, 1.0);
    // bands a second, rising; in a drop the rings radiate out at an even pace
    flow_ += wdt * (0.03f + 0.03f * wm + drop * 0.2f);
    phase_ += wdt * (0.1f + 0.08f * wm + 0.06f * drop);    // the waves rolling along them
    spin_ += wdt * 0.06f * drop;                           // the drop's petals turning, slowly
    // the waves: calmer and simpler in a drop (cleaner rings)
    wobble_ = approach(wobble_, (1.2f + 0.4f * wm) * (1.f - 0.5f * drop), 0.8f, dt);
    // the swell with the bars: one slow breath every two bars
    const float bar = (float)std::fmod(beats / 8.0, 1.0) * TAU;
    const float breath = (0.06f + 0.05f * wm) * (0.5f - 0.5f * std::cos(bar));
    // the drop's slow breath, one per bar (the pattern swelling a hair, the light a little brighter): a smooth wave,
    // never a kick on the beat
    const float pulse = drop * (0.5f - 0.5f * std::cos(bar1 * TAU));
    // the arcs' center: below the screen, wandering; the drop raises it behind the well (the arcs close into
    // full rings around it) and swings it slowly
    const vec2 calm{ARC_CENTER.x + 1.6f * std::sin(time_ * 0.031f), ARC_CENTER.y + 0.8f * std::sin(time_ * 0.023f + 1.f)};
    const vec2 trip{1.2f * std::sin(time_ * 0.17f), -1.f + 0.9f * std::sin(time_ * 0.13f + 0.5f)};
    center_ = vec2{lerpf(calm.x, trip.x, drop), lerpf(calm.y, trip.y, drop)};
    const vec2 center = center_;
    // BLOOM: the zoom eases toward its target (the clears nudge it); a drop adds a steady zoom-in, eased in at the
    // drop and to a stop after it; the flower turns very slowly. (Kept small: the pattern is the same every level.)
    zoomVel_ = approach(zoomVel_, inDrop ? 0.4f : 0.f, inDrop ? 0.8f : 0.5f, dt);
    levelTo_ += zoomVel_ * wdt;
    zoomLv_ = approach(zoomLv_, levelTo_, 1.6f, wdt);
    if (zoomLv_ > 48.f) { zoomLv_ -= 48.f; levelTo_ -= 48.f; }   // (48 levels: the same picture, the same colors)
    bloomSpin_ += wdt * (0.012f + 0.006f * wm);

    // ---- Ripples, pops.
    for (Ripple& r : ripples_) r.age += wdt;
    ripples_.erase(std::remove_if(ripples_.begin(), ripples_.end(), [](const Ripple& r) { return r.age > 4.f; }), ripples_.end());
    for (Pop& p : pops_) p.t += dt;
    pops_.erase(std::remove_if(pops_.begin(), pops_.end(), [](const Pop& p) { return p.t > CLEAR_T; }), pops_.end());

    // =========================================================== the bands
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    if (variant_ == V_BLOOM) {
        glUseProgram(progBloom_);
        set4f(progBloom_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        float bands[36];
        for (int i = 0; i < 12; i++) {
            bands[i * 3] = col_[BAND0 + i].x;
            bands[i * 3 + 1] = col_[BAND0 + i].y;
            bands[i * 3 + 2] = col_[BAND0 + i].z;
        }
        glUniform3fv(ugl::U(progBloom_, "uBand"), 6, bands);
        glUniform3fv(ugl::U(progBloom_, "uDropBand"), 6, bands + 18);
        set1f(progBloom_, "uDropMix", ease(dropMix_));
        set3f(progBloom_, "uLine", col_[LINE]);
        set3f(progBloom_, "uShade", col_[SHADE]);
        set3f(progBloom_, "uWellFill", col_[WELL]);
        set3f(progBloom_, "uWellLine", col_[WELL_LINE]);
        set2f(progBloom_, "uCenter", 0.f, 0.f);
        set1f(progBloom_, "uLevel", zoomLv_);
        set1f(progBloom_, "uSpin", bloomSpin_);
        set1f(progBloom_, "uDesat", 0.45f * slow_ + 0.2f * over_);
        set1f(progBloom_, "uGrain", 0.022f);
        set4f(progBloom_, "uWell", 5.15f, 10.15f, 0.3f, f.bare ? 0.f : 1.f);
        ugl::drawFullscreen();
    } else {
        glUseProgram(progBg_);
        set4f(progBg_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        float bands[36];
        for (int i = 0; i < 12; i++) {
            bands[i * 3] = col_[BAND0 + i].x;
            bands[i * 3 + 1] = col_[BAND0 + i].y;
            bands[i * 3 + 2] = col_[BAND0 + i].z;
        }
        glUniform3fv(ugl::U(progBg_, "uBand"), 6, bands);
        glUniform3fv(ugl::U(progBg_, "uDropBand"), 6, bands + 18);
        set1f(progBg_, "uDropMix", ease(dropMix_));
        set3f(progBg_, "uLine", col_[LINE]);
        set3f(progBg_, "uShade", col_[SHADE]);
        set3f(progBg_, "uWellFill", col_[WELL]);
        set3f(progBg_, "uWellLine", col_[WELL_LINE]);
        set2f(progBg_, "uCenter", center.x, center.y);
        set1f(progBg_, "uFlow", flow_);
        set1f(progBg_, "uPhase", phase_);
        set1f(progBg_, "uWobble", wobble_);
        set1f(progBg_, "uBreath", breath);
        set1f(progBg_, "uTrip", drop);
        set1f(progBg_, "uSpin", spin_);
        set1f(progBg_, "uZoom", 1.f - 0.006f * pulse);
        set3f(progBg_, "uGlow", lerp(col_[LINE], vec3(1.f, 0.99f, 0.95f), 0.5f));
        set1f(progBg_, "uGlowAmt", drop * (0.75f + 0.08f * pulse));
        set1f(progBg_, "uDesat", 0.45f * slow_ + 0.2f * over_);
        set1f(progBg_, "uGrain", variant_ == V_MIDNIGHT ? 0.045f : 0.022f);
        float rip[16] = {};
        for (size_t i = 0; i < ripples_.size(); i++) {
            const Ripple& r = ripples_[i];
            rip[i * 4] = r.r0 + 7.f * r.age;                                   // the front's radius
            rip[i * 4 + 1] = r.strength * ease(r.age / 0.3f) * (1.f - ease(r.age / 4.f));
        }
        glUniform4fv(ugl::U(progBg_, "uRipple"), 4, rip);
        set4f(progBg_, "uWell", 5.15f, 10.15f, 0.3f, f.bare ? 0.f : 1.f);
        ugl::drawFullscreen();
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    if (!f.bare) {
        // =========================================================== the blocks
        cells_.clear();
        buildBoard(game);
        if (!cells_.empty()) {
            glUseProgram(progCell_);
            ugl::setMat(progCell_, "uVP", vp_);
            set3f(progCell_, "uShine", vec3(1.f, 0.98f, 0.94f));
            set1f(progCell_, "uGap", 0.06f);
            set1f(progCell_, "uLine", 0.05f);
            glBindVertexArray(cellVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
            glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
        }
    }
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
        "WOODSTOCK", "70s groovy bands, pastel and chill: huge layered rainbow arcs melting into slow waves",
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
