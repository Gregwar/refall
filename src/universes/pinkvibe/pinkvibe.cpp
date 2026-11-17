// PINK VIBE: a soft, girly pink world of satin ribbons and pearls.
//
// Behind everything wide ribbons of pinks wave diagonally across the screen, from pale blush through rose and
// salmon-pink to a deep raspberry-wine band and back to blush, in soft relief lit from the top left: each ribbon lies
// over the one below it, its lower edge a rounded lip rolling down onto it, its satin face sloping gently toward the
// light (a faint sheen where it turns to it) and curving away under the ribbon above, which casts a soft shadow on
// it, longer where the light comes across the ribbons. A warm light glows in the top left corner. The ribbons undulate
// slowly and continuously: the waves breathe with the section (almost still at a calm part, rolling at a peak) and
// sway gently on the half-time beat grid from mid on. A few soap bubbles, sparkles and glints drift very sparsely
// upward in the free zones (never behind the well, fading out near the HUD columns).
//
// The well is a translucent deep wine pane in a rounded rose neon frame glowing softly inside and out, the ribbons
// faintly showing through. The blocks are candy cubes facing us like dice, one per cell: a flat opaque creamy
// fondant face, an even soft rounded bevel (lit top left, shaded bottom right), the very edge a little translucent
// and glowing like sugared gummy candy, a faint sugar grain with pearly micro sparkles, and a nearly flat gold foil
// stamp on the face: a quiet fine frame and a motif per type (I three pearls, O a four-pointed star, T a diamond,
// S an X, Z four dots, J a heart, L a ring). Every block pools a soft shadow under it (all shadows drawn under all
// blocks); the falling piece glows softly rose instead. The seven types are seven pinks of clearly different
// lightness and hue (ivory, peach, rose, deep raspberry, hot pink, lilac, coral), per variant.
//
// The drop is the sunset, held for the whole drop section: the ribbons swirl around the well into one big rolling
// wave and the palette swings to the variant's deeper rose-gold / magenta tones, eased in over a second or two and
// out over a few once the drop ends. A landing leaves a little gold sparkle dust under the piece. A cleared row's
// cubes swell softly and vanish, each releasing sparkle dust and bokeh motes rising high, then petals and bubbles
// that drift out behind the well and float up; a quad sends a soft shower of hearts and petals from both sides and swells the waves; slow motion is
// a lavender dusk with slower waves; game over greys the stack from the top. Around a song the ribbons are laid one
// after another over the blush, from the bottom left (the setup), and go under a deep plum dusk the same way (the
// teardown). HUD: Quicksand and Nunito (rounded), deep plum on a feathered blush halo.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "layout.hpp"
#include "ugl.hpp"
#include "universes/registry.hpp"
#include "universes/pinkvibe/shaders.hpp"

namespace {

// Quicksand Medium (the song, labels, toasts) and Nunito SemiBold (numbers, the universe's name), SIL Open Font
// License, see OFL-Quicksand.txt and OFL-Nunito.txt.
const unsigned char FONT_SOFT[] = {
#include "universes/pinkvibe/Quicksand-Medium.ttf.inc"
};
const unsigned char FONT_ROUND[] = {
#include "universes/pinkvibe/Nunito-SemiBold.ttf.inc"
};

namespace sh = pinkvibe_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant: the five bands (blush .. deepest) and the drop's, the well and its neon frame, the rim (the HUD's halo,
// the bubbles), the HUD ink and accent, the seven pieces (I O T S Z J L).
struct VariantDef {
    const char* name;
    unsigned band[5], dropBand[5];
    unsigned well, neon, rim, ink, accent;
    unsigned piece[7], dropPiece[7];
};
const VariantDef VARIANTS[] = {
    {"BLUSH",
     {0xf9d9df, 0xefb0c1, 0xdf819c, 0xbd4c74, 0x80234b},
     {0xf9c7b4, 0xf09aa6, 0xe0668d, 0xc23f78, 0x8a2862},
     0x4a1239, 0xff86b0, 0xffe1ea, 0x5a1f45, 0xb8336a,
     {0xffe0e8, 0xf9bba8, 0xfaa8c6, 0xbc426f, 0xec62a0, 0xdcbdf0, 0xf58c94},
     {0xffb0cc, 0xff8a5c, 0xff3d8f, 0xf2338a, 0xc24aae, 0xa874ff, 0xff4545}},
    {"ROSE",
     {0xf9dccf, 0xefb2a3, 0xde8580, 0xbb5064, 0x7e2440},
     {0xf6c391, 0xee9c86, 0xd86d82, 0xae4870, 0x6c2954},
     0x4c1433, 0xff8a9c, 0xffe6dc, 0x5c2234, 0xb54360,
     {0xffe4d8, 0xf6b89c, 0xf8a8b8, 0xb8445f, 0xea6896, 0xe9c3dc, 0xef8c7c},
     {0xffc39e, 0xff8a3d, 0xff5470, 0xf03a6c, 0xb54f9a, 0xd77ce0, 0xff4a2e}},
    {"CANDY",
     {0xf7dbee, 0xecaed4, 0xd97db6, 0xb24b8f, 0x732464},
     {0xd9b6f2, 0xe58dcb, 0xd0569f, 0xa63b86, 0x6a2670},
     0x3e1145, 0xff84cc, 0xffe3f3, 0x4e1a4e, 0xb02f86,
     {0xffdcf0, 0xffb9b0, 0xffacd9, 0xb43a80, 0xee5fb0, 0xcdb8f8, 0xf8829f},
     {0xffb8e2, 0xff7f6e, 0xff3fae, 0xf22e98, 0x9a45d6, 0x9c6bff, 0xff2f6a}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

enum Kind { K_PANEL = 0, K_PETAL, K_HEART, K_BUBBLE, K_SPARKLE, K_MOTE };

constexpr float CLEAR_T = 0.36f;   // a cleared tile vanishes in this long (real time)

float ease(float t) {
    t = saturate(t);
    return t * t * (3.f - 2.f * t);
}

class PinkVibeUniverse : public Universe {
public:
    ~PinkVibeUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override { dropMode_ = true; swell_ = std::max(swell_, 0.4f + 0.4f * strength); }
    void refresh(float strength) override { swell_ = std::max(swell_, 0.25f * strength); }
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void render(const Frame& f, GLuint fbo) override;
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    vec2 project(const vec3& p) const override { return layout::project(vp_, p, width_, height_); }
    TransitionHints hints() const override;
    int variant() const override { return variant_; }

private:
    struct Sprite { float a[4], b[4], c[4], d[4]; };
    std::vector<Sprite> sprites_;
    void sprite(int kind, float x, float y, float hx, float hy, float rot, float alpha, const vec3& A,
                const vec3& B = vec3(1.f), float p1 = 0.f) {
        if (alpha <= 0.004f || hx <= 0.f || hy <= 0.f) return;
        sprites_.push_back({{x, y, hx, hy}, {rot, (float)kind, alpha, p1}, {A.x, A.y, A.z, 0.f}, {B.x, B.y, B.z, 0.f}});
    }
    void flushSprites();

    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, float rot, int type, float alpha, int mode, float scale, float k);
    void buildBoard(const Game& g);
    struct Pop { float x, y; int type; float t; };
    std::vector<Pop> pops_;

    // Particles: the clears' petals and bubbles (behind the well), the hard drops' petals (at the contact, under the
    // blocks), the sparkle dust of a landing or a clear (over the blocks).
    enum Layer { L_BACK, L_UNDER, L_OVER };
    struct Part { float x, y, vx, vy, rot, spin, size, age, life, seed; int kind; vec3 col; Layer layer; };
    std::vector<Part> parts_;
    void dust(float x, float y, int count, float spread);

    // the HUD (its lettering: hudLine, the host letters it)
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    GLuint progBg_ = 0, progSprite_ = 0, progCell_ = 0, progSolid_ = 0;
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    void solid(float r, float g, float b, float a);

    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 band_[5], dropBand_[5], well_{0.f}, neon_{1.f}, rim_{1.f}, ink_{0.f}, accent_{1.f}, piece_[7], dropPiece_[7], shownPiece_[7];
    float night_ = 0.f;   // the drop's share in the HUD's colors and the blocks' vividness
    float fade_ = 1.f;    // the scene faded in from black (the HUD's lines with it)
    // the drop's waves of color on the blocks: pulses of vividness rolling up through the stack on the beat (the
    // blocks themselves stay put)
    float wave_ = 0.f;
    double waveBeat_ = 0.0;
    bool colSet_ = false;

    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool dropMode_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f;
    float flow_ = 0.f, amp_ = 0.5f, sway_ = 0.f, intensity_ = 0.3f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Rng rng_{0x9171u};
};

// =====================================================================================================
// Setup
// =====================================================================================================

bool PinkVibeUniverse::init(int width, int height) {
    const std::string head = "#version 330 core\n";
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, (head + sh::PAINT_GLSL + sh::BG_FS).c_str(), "pinkvibe bands");
    progSprite_ = ugl::program(sh::SPRITE_VS, (head + sh::PAINT_GLSL + sh::SPRITE_FS).c_str(), "pinkvibe sprites");
    progCell_ = ugl::program(sh::CELL_VS, (head + sh::PAINT_GLSL + sh::CELL_FS).c_str(), "pinkvibe blocks");
    progSolid_ = ugl::program(ugl::FULLSCREEN_VS, sh::SOLID_FS, "pinkvibe solid");

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

void PinkVibeUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
}

PinkVibeUniverse::~PinkVibeUniverse() {
    for (GLuint p : {progBg_, progSprite_, progCell_, progSolid_}) glDeleteProgram(p);
    for (GLuint v : {spriteVao_, cellVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, cellVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level, events
// =====================================================================================================

void PinkVibeUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.5f) v = 1;           // a darker, minor song: the warm rosé
        else if (t.hasSong && t.brightness > 0.62f) v = 2;                // a bright one: candy
        else {
            static const int BY_HUE[] = {0, 2, 1, 0, 1, 2};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x91AEull);
    if (seconds <= 0.f) colSet_ = false;
}

void PinkVibeUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)energy;
    (void)impact;
    level = std::clamp(level, 0, 2);
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
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 2.5f, 5.f);
}

TransitionHints PinkVibeUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    vec3 bg(0.f);
    for (unsigned c : V.band) bg += hex(c) * 0.2f;
    h.background = ugl::toLinear(bg);
    h.ink = ugl::toLinear(hex(V.ink));
    h.accent = ugl::toLinear(hex(V.accent));
    h.light = true;
    h.styles = TS_INK | TS_RIPPLE;
    return h;
}

void PinkVibeUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::HardDrop: {
        // two small petals pushed out from under the piece, at its lowest cells' outer sides
        float yLo = 99.f, xLo = 99.f, xHi = -99.f;
        int type = 0;
        for (auto& c : ev.cells) { yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y); type = c.type; }
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f) continue;
            xLo = std::min(xLo, p.x);
            xHi = std::max(xHi, p.x);
        }
        if (xLo > xHi) break;
        for (int s = 0; s < 2; s++) {
            const float sx = s ? 1.f : -1.f;
            Part p;
            p.x = s ? xHi + 0.5f : xLo - 0.5f;
            p.y = yLo - 0.38f;
            p.vx = sx * rng_.range(1.1f, 1.6f);
            p.vy = rng_.range(0.5f, 0.9f);
            p.rot = s ? 0.3f : PI - 0.3f;
            p.spin = sx * 1.6f;
            p.size = 0.15f;
            p.age = 0.f;
            p.life = 0.55f;
            p.seed = rng_.uniform();
            p.kind = K_PETAL;
            p.col = shownPiece_[std::clamp(type, 0, 6)];
            p.layer = L_UNDER;
            parts_.push_back(p);
        }
        break;
    }
    case GameEvent::Clear: {
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            pops_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), 0.f});
            const float side = p.x + rng_.range(-1.f, 1.f) < 0.f ? -1.f : 1.f;
            for (int i = 0; i < 2; i++) {
                Part q;
                q.x = p.x;
                q.y = p.y;
                q.vx = side * rng_.range(4.5f, 9.f);
                q.vy = rng_.range(0.5f, 3.f);
                q.rot = rng_.range(0.f, TAU);
                q.spin = rng_.range(-1.2f, 1.2f);
                q.size = i == 0 ? rng_.range(0.3f, 0.45f) : rng_.range(0.2f, 0.32f);
                q.age = -rng_.range(0.f, 0.12f);
                q.life = rng_.range(2.6f, 3.4f);
                q.seed = rng_.uniform();
                q.kind = i == 0 ? K_PETAL : (rng_.chance(0.6f) ? K_BUBBLE : K_PETAL);
                q.col = i == 0 ? shownPiece_[std::clamp(c.type, 0, 6)] : rim_;
                q.layer = L_BACK;
                parts_.push_back(q);
            }
            dust(p.x, p.y, 3, 0.45f);
            // and a few slower bokeh motes rising high above the stack
            for (int i = 0; i < 2; i++) {
                Part q;
                q.x = p.x + rng_.range(-0.45f, 0.45f);
                q.y = p.y + rng_.range(-0.3f, 0.3f);
                q.vx = rng_.range(-0.25f, 0.25f);
                q.vy = rng_.range(1.4f, 2.6f);
                q.rot = q.spin = 0.f;
                q.age = -rng_.range(0.f, 0.35f);
                q.life = rng_.range(1.8f, 2.8f);
                q.seed = rng_.uniform();
                q.kind = rng_.chance(0.2f) ? K_SPARKLE : K_MOTE;
                q.size = q.kind == K_SPARKLE ? rng_.range(0.12f, 0.18f) : rng_.range(0.1f, 0.2f);
                q.col = lerp(vec3(1.f, 0.85f, 0.62f), neon_, rng_.uniform() * 0.7f);
                q.layer = L_OVER;
                parts_.push_back(q);
            }
        }
        if (ev.count >= 4) {
            // a quad: a soft shower of hearts and petals from both sides; the waves swell
            swell_ = std::max(swell_, 0.8f);
            for (int i = 0; i < 26; i++) {
                Part q;
                const float side = i % 2 ? 1.f : -1.f;
                q.x = side * rng_.range(4.f, 5.f);
                q.y = rng_.range(-8.f, 4.f);
                q.vx = side * rng_.range(3.5f, 7.5f);
                q.vy = rng_.range(1.5f, 5.f);
                q.rot = rng_.range(-0.3f, 0.3f);
                q.spin = rng_.range(-0.8f, 0.8f);
                q.size = rng_.range(0.4f, 0.7f);
                q.age = -rng_.range(0.f, 0.6f);
                q.life = rng_.range(3.f, 3.8f);
                q.seed = rng_.uniform();
                q.kind = i % 3 == 0 ? K_HEART : i % 3 == 1 ? K_PETAL : K_BUBBLE;
                q.col = q.kind == K_BUBBLE ? rim_ : shownPiece_[rng_.irange(0, 6)];
                q.layer = L_BACK;
                parts_.push_back(q);
            }
        }
        break;
    }
    case GameEvent::Lock:
        // a little sparkle dust where the piece lands, under its lowest cells
        for (auto& c : ev.cells) {
            bool lowest = true;
            for (auto& o : ev.cells) lowest &= !(o.x == c.x && o.y == c.y + 1);
            if (!lowest || c.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            dust(p.x, p.y - 0.45f, 2, 0.4f);
        }
        break;
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

void PinkVibeUniverse::dust(float x, float y, int count, float spread) {
    for (int i = 0; i < count; i++) {
        Part q;
        q.x = x + rng_.range(-spread, spread);
        q.y = y + rng_.range(-0.2f, 0.2f);
        q.vx = rng_.range(-0.35f, 0.35f);
        q.vy = rng_.range(0.4f, 1.1f);
        q.rot = 0.f;
        q.spin = 0.f;
        q.age = -rng_.range(0.f, 0.15f);
        q.life = rng_.range(0.8f, 1.4f);
        q.seed = rng_.uniform();
        q.kind = rng_.chance(0.25f) ? K_SPARKLE : K_MOTE;
        q.size = q.kind == K_SPARKLE ? rng_.range(0.1f, 0.15f) : rng_.range(0.05f, 0.09f);
        q.col = lerp(vec3(1.f, 0.82f, 0.55f), neon_, rng_.uniform() * 0.6f);
        q.layer = L_OVER;
        parts_.push_back(q);
    }
}

// =====================================================================================================
// Board
// =====================================================================================================

void PinkVibeUniverse::cell(float x, float y, float half, float rot, int type, float alpha, int mode, float scale, float k) {
    if (alpha <= 0.003f || scale <= 0.f) return;
    const vec3 c = piece_[std::clamp(type, 0, 6)];
    cells_.push_back({{x, y, half, rot}, {(float)type, alpha, (float)mode, scale}, {c.x, c.y, c.z, k}});
}

void PinkVibeUniverse::buildBoard(const Game& g) {
    const float overRows = over_ * 26.f;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            float k = -0.3f * std::pow(saturate(c.flash), 10.f);
            if (over_ > 0.f) k = 0.75f * saturate(overRows - (y - Game::HIDDEN));
            cell(p.x, p.y, 0.5f, 0.f, c.type, 1.f, 0, 1.f, k);
        }
    for (const Pop& p : pops_) {
        // a soft swell, then the tile shrinks away and fades
        const float t = saturate(p.t / CLEAR_T);
        const float sc = (1.f + 0.12f * std::sin(std::min(t * 2.5f, 1.f) * PI)) * (1.f - 0.6f * ease((t - 0.3f) / 0.7f));
        cell(p.x, p.y, 0.5f, 0.f, p.type, 1.f - ease((t - 0.35f) / 0.65f), 0, sc, -0.35f * (1.f - t));
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
                cell(w.x, w.y, 0.5f, 0.f, p.type, 1.f, 1, 1.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cs) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, 0.f, p.type, 1.f, 3, 1.f, 0.f);
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
            cell(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc, 0.5f, 0.f, type,
                 alpha, 2, sc, 0.f);
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void PinkVibeUniverse::flushSprites() {
    if (sprites_.empty()) return;
    glUseProgram(progSprite_);
    ugl::setMat(progSprite_, "uVP", vp_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(Sprite), sprites_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    sprites_.clear();
}

void PinkVibeUniverse::solid(float r, float g, float b, float a) {
    if (a <= 0.002f) return;
    glUseProgram(progSolid_);
    set4f(progSolid_, "uColor", r * a, g * a, b * a, a);
    ugl::drawFullscreen();
}

// =====================================================================================================
// Text
// =====================================================================================================

// The HUD: deep plum letters on a feathered blush halo; Quicksand for the words, Nunito for the numbers.
void PinkVibeUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    if (!panels.empty()) {
        glUseProgram(progSolid_);
        glEnable(GL_SCISSOR_TEST);
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent_ : rim_;
            glScissor((GLint)r.x, (GLint)(height_ - r.y - r.h), (GLsizei)std::max(0.f, r.w), (GLsizei)std::max(0.f, r.h));
            solid(c.x, c.y, c.z, std::min(1.f, r.alpha));
        }
        glDisable(GL_SCISSOR_TEST);
    }
}


// The lettering, by the host (text.hpp): deep plum letters on a wide feathered blush halo, Quicksand for the words,
// Nunito for the numbers and the universe's name (its variant after it, larger, in Quicksand and the accent); at the
// drop's sunset the letters pale and the halo turns to the well's dusk. The scene's fade to black goes over them.
void PinkVibeUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* soft = text::font(FONT_SOFT);
    text::Font* round = text::font(FONT_ROUND);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    auto ink = [&](const vec3& c) { return lerp(c, lerp(c, vec3(1.f, 0.97f, 0.98f), 0.8f), night_); };
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, ink(c), track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        // the name, then its variant ("Blush": only its first letter a capital), larger and set a little higher
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        var = text::lower(var);
        if (!var.empty()) var = text::upper(var.substr(0, 1)) + var.substr(1);
        const float sz = base * 0.9f, vsz = base * 1.15f, a = saturate(0.35f + h.alpha * 1.6f);
        out.add(name, round, sz, ink(ink_), 0.2f, a);
        if (!var.empty()) {
            out.add("", soft, base, ink(ink_), 0.52f, 0.f); // the gap: 0.7 base after the name's last letter
            // (its cap line base * 0.15 above the name's: the two fonts' capitals are 0.517 and 0.56 of their size)
            out.add(var, soft, vsz, ink(accent_), 0.f, a).dy = -sz * 0.15f + vsz * 0.56f - sz * 0.517f;
        }
        break;
    }
    case HUD_SECTION: put(soft, h.text, base * 0.9f, base * 0.34f, accent_, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(soft, h.text, base * 1.6f, base * 0.02f, ink_, h.alpha * 1.3f, -base * 0.3f); break;
    case HUD_INFO: put(soft, h.text, base * 0.75f, base * 0.1f, ink_, saturate(alpha * 1.3f) * 0.85f); break;
    case HUD_LABEL: put(soft, h.text, base * 0.8f, base * 0.3f, accent_, alpha * 1.2f); break;
    case HUD_VALUE: put(round, h.text, base * 1.1f, base * 0.04f, ink_, alpha); break;
    case HUD_NOTICE: put(soft, h.text, base * 1.5f, base * 0.04f, accent_, alpha); break;
    case HUD_KEY: put(round, h.text, base * 0.75f, base * 0.06f, ink_, alpha); break;
    case HUD_TOAST: put(soft, h.text, base * 1.55f, base * 0.12f, accent_, alpha, -base * 0.3f); break;
    case HUD_TOAST_SUB: put(round, h.text, base * 1.05f, base * 0.1f, ink_, alpha * 0.95f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * height_ / 1080.f) put(soft, h.text, base * 1.3f, 0.f, ink_, alpha, -base * 0.1f);
        else put(soft, h.text, base * 0.85f, base * 0.1f, h.accent ? accent_ : ink_, alpha);
        break;
    default: put(soft, h.text, base * 0.85f, base * 0.08f, h.accent ? accent_ : ink_, alpha); break;
    }
    out.look.halo = lerp(lerp(rim_, vec3(1.f), 0.4f), well_ * 0.75f, night_);
    out.look.haloA = 0.6f + 0.15f * night_;
    out.look.haloW = 0.146f;
    out.alpha = fade_;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void PinkVibeUniverse::render(const Frame& f, GLuint fbo) {
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
        auto mixc = [&](vec3& c, unsigned target) { c = lerp(c, hex(target), k); };
        for (int i = 0; i < 5; i++) { mixc(band_[i], V.band[i]); mixc(dropBand_[i], V.dropBand[i]); }
        mixc(well_, V.well); mixc(neon_, V.neon); mixc(rim_, V.rim); mixc(ink_, V.ink); mixc(accent_, V.accent);
        for (int i = 0; i < 7; i++) { mixc(piece_[i], V.piece[i]); mixc(dropPiece_[i], V.dropPiece[i]); }
    }

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, ease(lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    // the sunset holds for the whole drop section (whether or not its first downbeat was heard: a seek, a review shot)
    const bool isDrop = overT_ < 0.f && level_ == 2 && (sectionKnown ? sectionDrop : dropMode_ && (!music.hasSong || music.speed > 1.26f));
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.1f : 0.4f, dt);
    const float sunset = ease(dropAmt_);
    night_ = sunset;
    // the petals and bubbles glow more vivid with the drop's palette; the blocks keep theirs, the drop's colors
    // crossing them in waves (see the cell shader)
    for (int i = 0; i < 7; i++) shownPiece_[i] = lerp(piece_[i], dropPiece_[i], sunset);
    wave_ = sunset;
    waveBeat_ = music.hasSong ? music.beat : time_ * 2.0;
    swell_ = approach(swell_, 0.f, 0.45f, dt);
    // the waves: their height breathes with the section's energy (slow envelopes), their flow speeds up from mid on
    intensity_ = approach(intensity_, music.hasSong ? music.intensity : 0.5f, 0.6f, dt);
    const float ampT = (0.35f + 0.45f * wm + 0.4f * wp) * (0.75f + 0.5f * saturate(intensity_)) + 0.3f * sunset;
    amp_ = approach(amp_, ampT + 0.35f * swell_, 0.8f, dt);
    flow_ += wdt * (0.05f + 0.22f * wm + 0.2f * wp + 0.1f * sunset) * lerpf(1.f, 0.5f, slow_);
    // the sway: a gentle eased swing on the half-time beat grid, from mid on
    {
        const double beats = music.hasSong ? music.beat : time_ * 2.0;
        const float ph = (float)std::fmod(beats * 0.5, 1.0);
        const float target = std::sin(ph * TAU) * (0.08f + 0.18f * wm + 0.14f * wp);
        sway_ = approach(sway_, target, 5.f, dt);
    }

    // ---- Particles, pops.
    for (Pop& p : pops_) p.t += dt;
    pops_.erase(std::remove_if(pops_.begin(), pops_.end(), [](const Pop& p) { return p.t > CLEAR_T; }), pops_.end());
    for (Part& p : parts_) {
        p.age += dt;
        if (p.age < 0.f) continue;
        const bool back = p.layer == L_BACK;
        p.x += (p.vx + (back ? 0.5f * std::sin(p.age * 1.7f + p.seed * 9.f) : 0.f)) * dt;
        p.y += p.vy * dt;
        const float drag = std::exp(-(back ? 1.3f : p.layer == L_UNDER ? 3.f : 1.f) * dt);
        p.vx *= drag;
        p.vy = p.vy * drag + (back ? 0.5f : 0.f) * dt;   // the petals float up a little
        p.rot += p.spin * dt;
    }
    parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Part& p) { return p.age >= p.life; }), parts_.end());

    // =========================================================== the bands
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    {
        glUseProgram(progBg_);
        set4f(progBg_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        float bands[15];
        for (int i = 0; i < 5; i++) {
            const vec3 c = lerp(band_[i], dropBand_[i], sunset);
            bands[i * 3] = c.x; bands[i * 3 + 1] = c.y; bands[i * 3 + 2] = c.z;
        }
        glUniform3fv(ugl::U(progBg_, "uBand"), 5, bands);
        set1f(progBg_, "uTime", time_);
        set1f(progBg_, "uFlow", flow_);
        set1f(progBg_, "uAmp", amp_);
        set1f(progBg_, "uSway", sway_);
        set1f(progBg_, "uSwirl", sunset);
        set1f(progBg_, "uDesat", 0.5f * slow_ + 0.3f * over_);
        set1f(progBg_, "uDecor", 0.55f + 0.25f * wm + 0.2f * sunset);
        // around a song: the bands laid one after another, from the bottom left (the setup), and going under the
        // plum dusk the same way (the teardown); some 13 bands span the view, its waves included
        set1f(progBg_, "uLaid", f.setup >= 1.f ? 99.f : lerpf(-6.f, 7.f, smoothstepf(0.05f, 0.9f, f.setup)));
        set1f(progBg_, "uDusk", f.teardown <= 0.f ? -99.f : lerpf(-6.f, 7.f, smoothstepf(0.05f, 0.95f, f.teardown)));
        set3f(progBg_, "uDuskCol", well_ * 0.6f);
        set3f(progBg_, "uLight", lerp(vec3(1.f, 0.96f, 0.9f), dropBand_[0], 0.5f * sunset));
        set4f(progBg_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
        set4f(progBg_, "uColR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // the clears' petals and bubbles, behind the well (softened where they cross a HUD column)
    auto inColumn = [&](float x, float y) {
        auto in = [&](const float* c) { return x > c[0] - 0.3f && x < c[2] + 0.3f && y > c[1] - 0.3f && y < c[3] + 0.3f; };
        return in(layout::LEFT_COLUMN) || in(layout::RIGHT_COLUMN);
    };
    for (const Part& p : parts_) {
        if (p.layer != L_BACK || p.age < 0.f) continue;
        float a = saturate(p.age / 0.15f) * (1.f - ease((p.age - p.life + 0.9f) / 0.9f));
        if (inColumn(p.x, p.y)) a *= 0.4f;
        const float r = p.size;
        const vec3 hi = lerp(p.col, vec3(1.f), 0.55f);
        if (p.kind == K_PETAL) sprite(K_PETAL, p.x, p.y, r, r * 0.6f, p.rot, a * 0.95f, p.col, hi);
        else if (p.kind == K_HEART) sprite(K_HEART, p.x, p.y, r, r, p.rot * 0.3f, a * 0.95f, p.col, hi);
        else sprite(K_BUBBLE, p.x, p.y, r, r, 0.f, a * 0.85f, p.col, vec3(1.f));
    }
    flushSprites();

    if (!f.bare) {
        // =========================================================== the well
        sprite(K_PANEL, 0.f, 0.f, 5.15f, 10.15f, 0.f, 1.f, lerp(well_, dropBand_[4] * 0.55f, 0.25f * sunset), neon_, 0.92f);
        for (const Part& p : parts_) {
            if (p.layer != L_UNDER) continue;
            const float a = 1.f - ease((p.age / p.life - 0.4f) / 0.6f);
            sprite(K_PETAL, p.x, p.y, p.size, p.size * 0.6f, p.rot, a, p.col, lerp(p.col, vec3(1.f), 0.5f));
        }
        flushSprites();

        // =========================================================== the blocks
        cells_.clear();
        buildBoard(game);
        if (!cells_.empty()) {
            glUseProgram(progCell_);
            ugl::setMat(progCell_, "uVP", vp_);
            set3f(progCell_, "uShade", well_ * 0.35f);
            set3f(progCell_, "uGlow", neon_);
            set1f(progCell_, "uWave", wave_);
            set1f(progCell_, "uBeat", (float)std::fmod(waveBeat_, 512.0));
            for (int i = 0; i < 7; i++) {
                const std::string n = "uVivid[" + std::to_string(i) + "]";
                set3f(progCell_, n.c_str(), dropPiece_[i]);
            }
            glBindVertexArray(cellVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
            glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
            // the soft shadows (the falling piece's glow) first, all of them under every block
            for (int pass = 0; pass < 2; pass++) {
                set1i(progCell_, "uPass", pass);
                glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
            }
        }

        // the sparkle dust of the landings and the clears, twinkling as it rises
        for (const Part& p : parts_) {
            if (p.layer != L_OVER || p.age < 0.f) continue;
            const float a = saturate(p.age / 0.12f) * (1.f - ease((p.age / p.life - 0.35f) / 0.65f)) *
                            (0.75f + 0.25f * std::sin(p.age * 14.f + p.seed * 20.f));
            sprite(p.kind, p.x, p.y, p.size, p.size, 0.f, a, p.col, vec3(1.f, 0.98f, 0.95f));
        }
        flushSprites();
    }
    solid(0.f, 0.f, 0.f, 0.45f * dimT_);
    drawHud(f.hud, f.panels);
    solid(0.f, 0.f, 0.f, 1.f - saturate(f.fade));
    fade_ = saturate(f.fade);
    glDisable(GL_BLEND);
}

} // namespace

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_pinkvibe() {
    static const UniverseEntry e{
        "PINK VIBE", "a soft pink world of satin ribbons in relief, pearl blocks with gold inlays in a rose neon frame, a rose sunset on the drop",
        [] { return std::unique_ptr<Universe>(new PinkVibeUniverse()); },
        // Bright, sweet, airy songs suit it best.
        [](const SongLook& l) { return 0.6f + 0.35f * l.brightness + (l.minor ? 0.f : 0.15f) + 0.15f * l.airWeight - 0.1f * l.bassWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
