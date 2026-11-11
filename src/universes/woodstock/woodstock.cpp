// WOODSTOCK: a late-60s psychedelic cartoon, peace and love, flower power.
//
// One world, flat and outlined: every shape is a flat candy color inside a thick white cartoon outline. Behind
// everything a kaleidoscope centered on the well: concentric bands in the variant's colors, folded into sectors, its
// rings flowing slowly outward (a log-polar tunnel) and morphing between circles and squircles over a minute or so;
// the music makes its bands breathe into scalloped petals (on the half-time beat grid, deeper at a peak). Around it,
// in the free zones beyond the HUD columns, a garden grows with the cleared lines (paceLines: complete in one song):
// flowers on stems rising from the bottom edge (daisies, mandala flowers, daisies with kawaii faces), one every line
// or so, taller and bigger as the song goes; a rainbow (line 7), a smiling sun (15), mushrooms with faces (22..28), a
// pair of doves (32), then two giant mandalas blooming behind the garden (40, 46). Floating up the sides: peace signs,
// hearts, sparkles and lava-lamp blobs, a few at calm, more at mid, many at a peak.
//
// The well is a calm deep panel with a white cartoon outline, so the stack pops off the busy kaleidoscope. The blocks
// are candy tiles, one per cell, each in a thick white outline with a dark rim, its fill faintly tie-dyed, and one
// bold pale motif per piece type: I a daisy, O a heart, T a peace sign, S a sparkle, Z a tie-dye spiral, J a rainbow,
// L a kawaii face; one hue per type.
//
// The music: calm is a soft, slow kaleidoscope; mid flows and breathes with the beat, the flowers sway; a peak
// saturates it and swells the flowers' heads on the kicks. A drop is the trip: the kaleidoscope folds into a mandala
// (the bands scalloping into petals, alternate sectors offset like a checkerboard, the dots turning to sparkles) and
// the palette swings to the variant's drop colors, both eased in over a second and out over a few. A cleared row's
// tiles pop and burst into its motifs (petals, hearts, peace signs, sparkles) flying out sideways behind the well; a
// Tetris turns the kaleidoscope's colors one band on and sends a shower of hearts and peace signs; slow motion is
// dusk (the colors drained to lilac, the flow slowed); game over greys the stack from the top.
//
// Original motifs only (generic period iconography: daisies, peace signs, hearts, rainbows, a smiling sun, doves,
// mushrooms, lava-lamp blobs). Cost: one fullscreen pass, a few hundred instanced SDF quads, SDF type.
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
#include "universes/woodstock/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Fraunces Soft Italic (a soft, Windsor-like 70s serif: the song, the toasts) and Comfortaa Bold (rounded: labels,
// numbers), SIL Open Font License, see OFL-Fraunces.txt and OFL-Comfortaa.txt.
const unsigned char FONT_DISPLAY[] = {
#include "universes/woodstock/Fraunces-SoftItalic.ttf.inc"
};
const unsigned char FONT_ROUND[] = {
#include "universes/woodstock/Comfortaa-Bold.ttf.inc"
};

namespace sh = woodstock_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant: the kaleidoscope's five bands (and the drop's), its lines and dots, the well, the ink (outlines of the
// faces, the lettering's stroke), the accent, the seven pieces (I O T S Z J L), the flowers' colors.
struct VariantDef {
    const char* name;
    unsigned band[5], dropBand[5];
    unsigned line, dot, dropLine;
    unsigned wellTop, wellBottom, ink, accent;
    unsigned piece[7];
    unsigned flower[6];
    unsigned center, green, leaf;
};
const VariantDef VARIANTS[] = {
    {"LILAC",
     {0x8a5ae0, 0xc3a2f4, 0xf3e9ff, 0xee86c4, 0x6c89f0},
     {0xff6fae, 0xffc94d, 0x4fdcc0, 0x7766f2, 0xfff1dc},
     0xffffff, 0xffd84d, 0xffffff,
     0x33205a, 0x1d1036, 0x2a1548, 0xffd23f,
     {0x24c6cc, 0xffc93c, 0xa45cf5, 0x5fd36c, 0xff4f7b, 0x3b78f4, 0xff8a33},
     {0xff4f7b, 0xff9fcf, 0x2ec4c4, 0x7ed957, 0xffc93c, 0xff8f3a},
     0xffe14d, 0x46b85a, 0x7ed957},
    {"SUNSHINE",
     {0xff8a3d, 0xffc54d, 0xfff2d8, 0xff6fa5, 0xffa9c9},
     {0x9b5de5, 0x00bbf9, 0xfee440, 0xf15bb5, 0xfff2d8},
     0xffffff, 0xff4f8b, 0xffffff,
     0x45163a, 0x290c22, 0x3d1233, 0xfff04d,
     {0x16b9d8, 0xffd60a, 0x8f5cf2, 0x38c172, 0xf0386b, 0x3a6ff0, 0xff7b1c},
     {0xf0386b, 0x9b5de5, 0x16b9d8, 0xffffff, 0xff7b1c, 0x38c172},
     0xffe14d, 0x2f9e4f, 0x6fd36a},
    {"MINT",
     {0x36cfb0, 0xa3eedb, 0xf0fff9, 0x4ab3e6, 0xb8e66e},
     {0xff7eb6, 0xffd36e, 0x8f7cff, 0x36cfb0, 0xfff4fb},
     0xffffff, 0xff7eb6, 0xffffff,
     0x103848, 0x08202b, 0x0d2c3a, 0xffe24a,
     {0x1fbde6, 0xffd23f, 0xb06bff, 0x4bd463, 0xff5577, 0x3b6ff0, 0xff8c42},
     {0xff5577, 0xffd23f, 0xb06bff, 0xff8c42, 0xff9fcf, 0x3b6ff0},
     0xffe14d, 0x2e9e6a, 0x8fe07a},
    {"BLACKLIGHT",
     {0x1f0b46, 0x3c1076, 0x10144f, 0x5a1580, 0x0b2a60},
     {0x46107a, 0x0a3b6e, 0x6e0f6a, 0x15154f, 0x2a0f5a},
     0xe6d8ff, 0x7dff3a, 0x9dff4a,
     0x0e0820, 0x060312, 0x120826, 0xc8ff3a,
     {0x2ef2ff, 0xfff23a, 0xc45cff, 0x5dff6a, 0xff3fa4, 0x5a86ff, 0xff9a2e},
     {0xff3fa4, 0x2ef2ff, 0xfff23a, 0xc45cff, 0x5dff6a, 0xff9a2e},
     0xfff23a, 0x2bd96a, 0x7dff3a},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

enum Kind { K_DAISY = 0, K_MANDALA, K_PEACE, K_HEART, K_RAINBOW, K_SUN, K_MUSHROOM, K_DOVE, K_LAVA, K_PETAL, K_SPARKLE,
            K_GAUGE, K_STEM, K_PANEL };

constexpr float CLEAR_T = 0.38f;   // a cleared tile pops away in this long (real time)
constexpr int N_FLOWERS = 36;      // the garden's flowers at scene completion
constexpr int N_FLOATERS = 18;     // icons floating up the sides
constexpr float OUTLINE = 0.1f;    // the white cartoon outline (board units)

float ease(float t) {
    t = saturate(t);
    return t * t * (3.f - 2.f * t);
}

// UTF-8 to the font's characters (the middle dot to '-' spaced, accents dropped).
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

// The garden's stages (on the 50-line scale), for the HUD's corner.
const char* stageName(float L) {
    if (L < 4.f) return "seeds";
    if (L < 12.f) return "first blooms";
    if (L < 24.f) return "sunshine";
    if (L < 36.f) return "flower power";
    if (L < 48.f) return "summer of love";
    return "peace & love";
}

class WoodstockUniverse : public Universe {
public:
    ~WoodstockUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override { dropMode_ = true; swell_ = std::max(swell_, 0.5f + 0.5f * strength); }
    void refresh(float strength) override { swell_ = std::max(swell_, 0.3f * strength); }
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override { return layout::project(vp_, p, width_, height_); }
    TransitionHints hints() const override;
    int variant() const override { return variant_; }

private:
    // ---- Sprites (board units).
    struct Sprite { float a[4], b[4], c[4], d[4], e[4]; };
    std::vector<Sprite> sprites_;
    void sprite(int kind, float x, float y, float hx, float hy, float rot, float alpha, const vec3& A,
                const vec3& B = vec3(1.f), const vec3& C = vec3(1.f), float p1 = 0.f, float p2 = 0.f, float p3 = 0.f,
                float outline = OUTLINE) {
        if (alpha <= 0.004f || hx <= 0.f || hy <= 0.f) return;
        sprites_.push_back({{x, y, hx, hy}, {rot, (float)kind, alpha, p1}, {A.x, A.y, A.z, p2}, {B.x, B.y, B.z, p3},
                            {C.x, C.y, C.z, outline}});
    }
    void flushSprites();

    // ---- Blocks.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, float rot, int type, float alpha, int mode, float scale, float k);
    void buildBoard(const Game& g);
    struct Pop { float x, y; int type; float t, spin; };
    std::vector<Pop> pops_;

    // ---- The garden (slots laid out once; positions as fractions of the side zones).
    struct Flower {
        float birth, side, u, stem, head, seed, grow;
        int kind, petals, col, col2;
        bool face;
        float startT;
    };
    std::vector<Flower> flowers_;
    struct Special { int kind; float birth, side, u, y, size, seed, grow, startT; };
    std::vector<Special> specials_;
    void buildGarden();
    float lines_ = 0.f, shown_ = -1.f;

    // ---- Particles: the clears' bursts (behind the well), the hard drops' puffs (at the contact).
    struct Part { float x, y, vx, vy, rot, spin, size, age, life; int kind; vec3 col; bool front; };
    std::vector<Part> parts_;

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font disp_, round_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& q, const vec3& fill, float alpha, float stroke);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progBg_ = 0, progSprite_ = 0, progCell_ = 0, progText_ = 0, progSolid_ = 0;
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    void solid(float r, float g, float b, float a);

    // ---- Song, variant, colors (sRGB).
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 band_[5], line_{1.f}, dot_{1.f}, wellTop_{0.f}, wellBottom_{0.f}, ink_{0.f}, accent_{1.f}, piece_[7], flower_[6],
        center_{1.f}, green_{0.f}, leaf_{0.f};
    bool colSet_ = false;

    // ---- Staging.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool dropMode_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f;
    float kickIn_ = 0.f, kickK_ = 0.f;
    float flow_ = 0.f, breath_ = 0.f, shift_ = 0.f, shiftTo_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Rng rng_{0x60D5u};
};

// =====================================================================================================
// Setup
// =====================================================================================================

bool WoodstockUniverse::init(int width, int height) {
    const std::string head = "#version 330 core\n";
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, (head + sh::PAINT_GLSL + sh::BG_FS).c_str(), "woodstock kaleidoscope");
    progSprite_ = ugl::program(sh::SPRITE_VS, (head + sh::PAINT_GLSL + sh::SPRITE_FS).c_str(), "woodstock sprites");
    progCell_ = ugl::program(sh::CELL_VS, (head + sh::PAINT_GLSL + sh::CELL_FS).c_str(), "woodstock blocks");
    progText_ = ugl::program(sh::TEXT_VS, sh::TEXT_FS, "woodstock text");
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
    instanced(spriteVao_, spriteVbo_, 5);
    instanced(cellVao_, cellVbo_, 3);
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    buildFont(disp_, FONT_DISPLAY);
    buildFont(round_, FONT_ROUND);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void WoodstockUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] woodstock font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = 64.f;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AW = 1024, AH = 1024, PAD = 12;
    std::vector<unsigned char> atlas((size_t)AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    for (int ch = 32; ch < 128; ch++) {
        const int cp = ch == 127 ? 0xB7 : ch;
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
    for (GLuint p : {progBg_, progSprite_, progCell_, progText_, progSolid_}) glDeleteProgram(p);
    for (GLuint v : {spriteVao_, cellVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, cellVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&disp_, &round_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
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
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 3;              // a dark song: blacklight
        else if (t.hasSong && !t.minor && t.brightness > 0.6f) v = 1;         // a bright, major one: sunshine
        else if (t.hasSong && t.airWeight > 0.6f) v = 2;                      // airy: mint
        else {
            static const int BY_HUE[] = {0, 1, 2, 0, 2, 1};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x5EACEull);
    if (flowers_.empty()) buildGarden();
    if (seconds <= 0.f) colSet_ = false;
}

void WoodstockUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

TransitionHints WoodstockUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    vec3 bg(0.f);
    for (unsigned c : V.band) bg += hex(c) * 0.2f;
    h.background = ugl::toLinear(bg);
    h.ink = ugl::toLinear(hex(0xffffff));
    h.accent = ugl::toLinear(hex(V.accent));
    h.light = variant_ != 3;
    h.styles = TS_INK | TS_RIPPLE | TS_TILES;
    return h;
}

void WoodstockUniverse::buildGarden() {
    Rng r(0xF10E5ull);
    flowers_.clear();
    for (int i = 0; i < N_FLOWERS; i++) {
        Flower fl;
        const float k = (i + 0.5f) / N_FLOWERS;
        fl.birth = 0.6f + 49.f * k * (0.92f + 0.08f * k);
        fl.side = (i % 2) ? 1.f : -1.f;
        // spread over the zone's width, interleaved so neighbors in time sit apart
        fl.u = std::fmod(0.13f + 0.618034f * (float)(i / 2) + r.range(-0.06f, 0.06f), 1.f);
        if (fl.u < 0.f) fl.u += 1.f;
        fl.stem = r.range(2.4f, 3.4f) + 4.6f * k * r.range(0.45f, 1.f);
        fl.head = r.range(0.7f, 0.95f) + 0.55f * k * r.range(0.5f, 1.f);
        fl.seed = r.uniform();
        fl.kind = r.chance(0.24f) ? K_MANDALA : K_DAISY;
        fl.face = fl.kind == K_DAISY && r.chance(0.28f);
        fl.petals = fl.kind == K_MANDALA ? r.irange(8, 12) : r.irange(6, 10);
        fl.col = r.irange(0, 5);
        fl.col2 = (fl.col + r.irange(1, 5)) % 6;
        fl.grow = 0.f;
        fl.startT = -1.f;
        flowers_.push_back(fl);
    }
    // the taller behind the shorter
    std::sort(flowers_.begin(), flowers_.end(), [](const Flower& a, const Flower& b) { return a.stem > b.stem; });
    specials_.clear();
    auto add = [&](int kind, float birth, float side, float u, float y, float size) {
        specials_.push_back({kind, birth, side, u, y, size, r.uniform(), 0.f, -1.f});
    };
    add(K_RAINBOW, 7.f, -1.f, 0.5f, 5.6f, 3.4f);
    add(K_SUN, 15.f, 1.f, 0.55f, 7.4f, 2.3f);
    add(K_MUSHROOM, 22.f, 1.f, 0.2f, -10.6f, 1.0f);
    add(K_MUSHROOM, 25.f, -1.f, 0.75f, -10.7f, 0.85f);
    add(K_MUSHROOM, 28.f, 1.f, 0.82f, -10.75f, 0.75f);
    add(K_DOVE, 32.f, -1.f, 0.45f, 2.2f, 0.9f);
    add(K_DOVE, 32.5f, 1.f, 0.5f, 3.6f, 0.8f);
    add(K_MANDALA, 40.f, -1.f, 0.5f, -1.0f, 2.7f);
    add(K_MANDALA, 46.f, 1.f, 0.45f, 0.2f, 2.6f);
}

void WoodstockUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    auto motifKind = [](int type) {
        static const int K[7] = {K_DAISY, K_HEART, K_PEACE, K_SPARKLE, K_PETAL, K_PETAL, K_HEART};
        return K[std::clamp(type, 0, 6)];
    };
    switch (ev.type) {
    case GameEvent::HardDrop: {
        // two petals pushed out from under the piece, at its lowest cells' outer sides
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
            p.x = (s ? xHi + 0.5f : xLo - 0.5f);
            p.y = yLo - 0.38f;
            p.vx = sx * rng_.range(1.2f, 1.8f);
            p.vy = rng_.range(0.6f, 1.0f);
            p.rot = s ? 0.3f : PI - 0.3f;
            p.spin = sx * 2.f;
            p.size = 0.16f;
            p.age = 0.f;
            p.life = 0.55f;
            p.kind = K_PETAL;
            p.col = piece_[std::clamp(type, 0, 6)];
            p.front = true;
            parts_.push_back(p);
        }
        break;
    }
    case GameEvent::Clear: {
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            pops_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), 0.f, rng_.range(-1.f, 1.f)});
            const float side = p.x + rng_.range(-0.8f, 0.8f) < 0.f ? -1.f : 1.f;
            for (int i = 0; i < 2; i++) {
                Part q;
                q.x = p.x;
                q.y = p.y;
                q.vx = side * rng_.range(5.f, 11.f);
                q.vy = rng_.range(1.5f, 6.5f);
                q.rot = rng_.range(0.f, TAU);
                q.spin = rng_.range(-2.5f, 2.5f);
                q.size = rng_.range(0.35f, 0.6f);
                q.age = -rng_.range(0.f, 0.15f);
                q.life = rng_.range(1.8f, 2.6f);
                q.kind = i == 0 ? motifKind(c.type) : K_PETAL;
                q.col = i == 0 ? piece_[std::clamp(c.type, 0, 6)] : flower_[rng_.irange(0, 5)];
                q.front = false;
                parts_.push_back(q);
            }
        }
        if (ev.count >= 4) {
            // a Tetris: the kaleidoscope's colors turn a band on; hearts and peace signs shower out on both sides
            shiftTo_ += 1.f;
            for (int i = 0; i < 28; i++) {
                Part q;
                const float side = i % 2 ? 1.f : -1.f;
                q.x = side * rng_.range(4.f, 5.f);
                q.y = rng_.range(-8.f, 4.f);
                q.vx = side * rng_.range(4.f, 9.f);
                q.vy = rng_.range(3.f, 9.f);
                q.rot = rng_.range(-0.4f, 0.4f);
                q.spin = rng_.range(-1.f, 1.f);
                q.size = rng_.range(0.55f, 0.95f);
                q.age = -rng_.range(0.f, 0.6f);
                q.life = rng_.range(2.4f, 3.2f);
                q.kind = i % 3 == 0 ? K_PEACE : i % 3 == 1 ? K_HEART : K_DAISY;
                q.col = flower_[rng_.irange(0, 5)];
                q.front = false;
                parts_.push_back(q);
            }
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

void WoodstockUniverse::cell(float x, float y, float half, float rot, int type, float alpha, int mode, float scale, float k) {
    if (alpha <= 0.003f || scale <= 0.f) return;
    const vec3 c = piece_[std::clamp(type, 0, 6)];
    cells_.push_back({{x, y, half, rot}, {(float)type, alpha, (float)mode, scale}, {c.x, c.y, c.z, k}});
}

void WoodstockUniverse::buildBoard(const Game& g) {
    const float overRows = over_ * 26.f;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            float k = -0.35f * std::pow(saturate(c.flash), 10.f);
            if (over_ > 0.f) k = 0.75f * saturate(overRows - (y - Game::HIDDEN));
            cell(p.x, p.y, 0.5f, 0.f, c.type, 1.f, 0, 1.f, k);
        }
    for (const Pop& p : pops_) {
        const float t = saturate(p.t / CLEAR_T);
        const float sc = (1.f + 0.28f * std::sin(std::min(t * 2.2f, 1.f) * PI)) * (1.f - ease((t - 0.35f) / 0.65f));
        cell(p.x, p.y, 0.5f, p.spin * t * 1.2f, p.type, 1.f - ease((t - 0.6f) / 0.4f), 0, sc, -0.3f * (1.f - t));
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
            cell(w.x, w.y, 0.5f, 0.f, p.type, 1.f, 0, 1.f, 0.f);
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

void WoodstockUniverse::flushSprites() {
    if (sprites_.empty()) return;
    glUseProgram(progSprite_);
    ugl::setMat(progSprite_, "uVP", vp_);
    set3f(progSprite_, "uInk", ink_);
    set3f(progSprite_, "uWhite", vec3(1.f));
    set1f(progSprite_, "uTime", time_);
    set1f(progSprite_, "uFade", 1.f);
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

float WoodstockUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            if (ch != 127 && n != 127) w += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
    return w - track;
}

void WoodstockUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                                  std::vector<float>& out) const {
    if (!f.ok) return;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    const float baseline = f.capH * k;
    float pen = 0.f;
    auto place = [&](float u, float v, float tu, float tv) {
        out.push_back(x + u); out.push_back(y + v); out.push_back(tu); out.push_back(tv);
    };
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            place(x0, y0, g.x0, g.y0); place(x1, y0, g.x1, g.y0); place(x1, y1, g.x1, g.y1);
            place(x0, y0, g.x0, g.y0); place(x1, y1, g.x1, g.y1); place(x0, y1, g.x0, g.y1);
        }
        pen += g.adv * k + track;
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            if (ch != 127 && n != 127) pen += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
}

void WoodstockUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& fill, float alpha, float stroke) {
    if (q.empty() || !f.ok || alpha <= 0.004f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uFill", fill);
    set3f(progText_, "uStroke", ink_);
    set1f(progText_, "uStrokeW", stroke);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD: cream letters in a thick dark cartoon stroke (they read over any band of the kaleidoscope): Comfortaa
// for the labels and numbers, Fraunces' soft italics for the song, the variant, the notices and the toasts.
void WoodstockUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    if (!panels.empty()) {
        // panels: plain rectangles in the well's color
        glUseProgram(progSolid_);
        glEnable(GL_SCISSOR_TEST);
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent_ : wellBottom_;
            glScissor((GLint)r.x, (GLint)(height_ - r.y - r.h), (GLsizei)std::max(0.f, r.w), (GLsizei)std::max(0.f, r.h));
            solid(c.x, c.y, c.z, std::min(1.f, r.alpha));
        }
        glDisable(GL_SCISSOR_TEST);
    }
    const vec3 cream = hex(0xfff8ec);
    std::vector<float> q;
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        const std::string text = ascii(h.text);
        q.clear();
        auto place = [&](const Font& f, const std::string& s, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, s, size, track);
            textQuads(f, s, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string name = h.text, var;
            const size_t dot = name.find("\xC2\xB7");
            if (dot != std::string::npos) { var = name.substr(dot + 2); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            name = ascii(name);
            std::string vt = ascii(var);
            for (size_t i = 1; i < vt.size(); i++) vt[i] = (char)std::tolower((unsigned char)vt[i]);
            const float sz = base * 0.95f, tr = sz * 0.18f, vsz = base * 1.3f, gap = base * 0.7f;
            const std::string st = stageName(std::max(0.f, shown_));
            const float wName = textWidth(round_, name, sz, tr);
            const float wVar = vt.empty() ? 0.f : textWidth(disp_, vt, vsz, 0.f);
            const float wSt = textWidth(disp_, st, vsz, 0.f);
            const float total = wName + (vt.empty() ? 0.f : gap + wVar) + gap * 1.3f + wSt;
            const float x = ugl::hudLeft(h, total, width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(round_, name, x, h.y, sz, tr, q);
            drawText(round_, q, cream, a, 0.4f);
            float cx = x + wName;
            if (!vt.empty()) {
                q.clear();
                textQuads(disp_, vt, cx + gap, h.y - sz * 0.22f, vsz, 0.f, q);
                drawText(disp_, q, accent_, a, 0.4f);
                cx += gap + wVar;
            }
            q.clear();
            textQuads(disp_, st, cx + gap * 1.3f, h.y - sz * 0.22f, vsz, 0.f, q);
            drawText(disp_, q, cream, a * 0.9f, 0.4f);
            continue;
        }
        case HUD_SECTION:
            place(round_, text, base * 0.9f, base * 0.3f);
            drawText(round_, q, accent_, saturate(0.2f + h.alpha * 1.9f), 0.4f);
            continue;
        case HUD_TITLE:
            place(disp_, text, base * 1.7f, 0.f, -base * 0.35f);
            drawText(disp_, q, cream, saturate(h.alpha * 1.3f), 0.3f);
            continue;
        case HUD_INFO:
            place(round_, text, base * 0.7f, base * 0.08f);
            drawText(round_, q, cream, saturate(alpha * 1.3f), 0.32f);
            continue;
        case HUD_LABEL:
            place(round_, text, base * 0.8f, base * 0.28f);
            drawText(round_, q, accent_, saturate(alpha * 1.2f), 0.32f);
            continue;
        case HUD_VALUE:
            place(round_, text, base * 1.1f, base * 0.04f);
            drawText(round_, q, cream, alpha, 0.3f);
            continue;
        case HUD_NOTICE:
            place(disp_, text, base * 1.6f, 0.f);
            drawText(disp_, q, accent_, alpha, 0.3f);
            continue;
        case HUD_KEY:
            place(round_, text, base * 0.75f, base * 0.06f);
            drawText(round_, q, cream, alpha, 0.32f);
            continue;
        case HUD_TOAST:
            place(disp_, text, base * 1.6f, base * 0.06f, -base * 0.3f);
            drawText(disp_, q, accent_, alpha, 0.32f);
            continue;
        case HUD_TOAST_SUB:
            place(round_, text, base * 1.1f, base * 0.1f);
            drawText(round_, q, cream, alpha * 0.95f, 0.32f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * height_ / 1080.f) {
                place(disp_, text, base * 1.3f, 0.f, -base * 0.1f);
                drawText(disp_, q, cream, alpha, 0.3f);
            } else {
                place(round_, text, base * 0.85f, base * 0.1f);
                drawText(round_, q, h.accent ? accent_ : cream, alpha, 0.32f);
            }
            continue;
        default:
            place(round_, text, base * 0.85f, base * 0.08f);
            drawText(round_, q, h.accent ? accent_ : cream, alpha, 0.32f);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void WoodstockUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    static const bool gpuTime = std::getenv("ZEN_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    static int gpuN = 0;
    static double gpuSum = 0.0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);

    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 2.5f) : 0.f, overT_ >= 0.f ? 3.f : 1.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += wdt;
    if (flowers_.empty()) buildGarden();

    // ---- The colors: the variant's, eased in on a change of song.
    {
        const VariantDef& V = VARIANTS[variant_];
        const float k = colSet_ ? 1.f - std::exp(-dt * 1.2f) : 1.f;
        colSet_ = true;
        auto mixc = [&](vec3& c, unsigned target) { c = lerp(c, hex(target), k); };
        for (int i = 0; i < 5; i++) mixc(band_[i], V.band[i]);
        mixc(line_, V.line); mixc(dot_, V.dot);
        mixc(wellTop_, V.wellTop); mixc(wellBottom_, V.wellBottom); mixc(ink_, V.ink); mixc(accent_, V.accent);
        for (int i = 0; i < 7; i++) mixc(piece_[i], V.piece[i]);
        for (int i = 0; i < 6; i++) mixc(flower_[i], V.flower[i]);
        mixc(center_, V.center); mixc(green_, V.green); mixc(leaf_, V.leaf);
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
    const bool isDrop = dropMode_ && overT_ < 0.f && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.6f : 0.45f, dt);
    const float trip = ease(dropAmt_);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    const float kick = std::max(music.kick, kickIn_) * (0.3f * wm + 0.7f * wp);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    kickK_ = std::max(approach(kickK_, 0.f, 3.f, dt), kick);
    // the breath: on the half-time beat grid, deeper at a peak
    {
        const double beats = music.hasSong ? music.beat : time_ * 2.0;
        const float ph = (float)std::fmod(beats * 0.5, 1.0);
        const float b = 0.5f - 0.5f * std::cos(ph * TAU);
        const float amp = 0.15f + 0.35f * wm + 0.5f * wp + 0.3f * trip;
        breath_ = approach(breath_, b * amp, 6.f, dt);
    }
    flow_ += wdt * (0.06f + 0.14f * wm + 0.2f * wp + 0.15f * trip);
    shift_ = approach(shift_, shiftTo_, 1.4f, dt);

    // ---- The garden: lines -> flowers (springing after the paced lines, the speed capped).
    {
        // (test hook: ZEN_WOODSTOCK_LINES=N adds N real lines)
        static const float extra = std::getenv("ZEN_WOODSTOCK_LINES") ? (float)std::atof(std::getenv("ZEN_WOODSTOCK_LINES")) : 0.f;
        const float lines = f.bare ? lines_ : paceLines(game, extra);
        if (shown_ >= 0.f && lines < lines_ - 0.01f && !game.over()) {
            for (Flower& fl : flowers_) fl.startT = -1.f; // a new game: the garden wilts and grows again
            for (Special& s : specials_) s.startT = -1.f;
        }
        lines_ = lines;
        if (shown_ < 0.f) {
            shown_ = lines_;
            for (Flower& fl : flowers_)
                if (fl.birth <= shown_) { fl.startT = time_ - 10.f; fl.grow = 1.f; }
            for (Special& s : specials_)
                if (s.birth <= shown_) { s.startT = time_ - 10.f; s.grow = 1.f; }
        }
        const float cap = 1.6f * std::max(1.f, paceScale());
        shown_ += std::clamp((lines_ - shown_) * 1.5f * wdt, -8.f * wdt, cap * wdt);
        if (lines_ < shown_ - 5.f) shown_ = approach(shown_, lines_, 6.f, dt);
        for (Flower& fl : flowers_) {
            if (fl.startT < 0.f && fl.birth <= shown_) fl.startT = time_;
            fl.grow = fl.startT >= 0.f ? saturate((time_ - fl.startT) / 1.8f) : approach(fl.grow, 0.f, 0.6f, wdt);
        }
        for (Special& s : specials_) {
            if (s.startT < 0.f && s.birth <= shown_) s.startT = time_;
            s.grow = s.startT >= 0.f ? saturate((time_ - s.startT) / 2.2f) : approach(s.grow, 0.f, 0.6f, wdt);
        }
    }

    // ---- Particles, pops.
    for (Pop& p : pops_) p.t += dt;
    pops_.erase(std::remove_if(pops_.begin(), pops_.end(), [](const Pop& p) { return p.t > CLEAR_T; }), pops_.end());
    for (Part& p : parts_) {
        p.age += dt;
        if (p.age < 0.f) continue;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        const float drag = std::exp(-(p.front ? 3.f : 0.9f) * dt);
        p.vx *= drag;
        p.vy = p.vy * drag - (p.front ? 0.f : 2.2f) * dt;
        p.rot += p.spin * dt;
    }
    parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Part& p) { return p.age >= p.life; }), parts_.end());
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // =========================================================== the kaleidoscope
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    {
        const VariantDef& V = VARIANTS[variant_];
        glUseProgram(progBg_);
        set4f(progBg_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        float bands[15];
        for (int i = 0; i < 5; i++) {
            const vec3 c = lerp(band_[i], hex(V.dropBand[i]), trip);
            bands[i * 3] = c.x; bands[i * 3 + 1] = c.y; bands[i * 3 + 2] = c.z;
        }
        glUniform3fv(ugl::U(progBg_, "uBand"), 5, bands);
        set3f(progBg_, "uLine", lerp(line_, hex(V.dropLine), trip));
        set3f(progBg_, "uDot", dot_);
        set1f(progBg_, "uFlow", flow_);
        set1f(progBg_, "uBreath", breath_ + 0.25f * swell_);
        set1f(progBg_, "uMorph", 0.5f + 0.5f * std::sin(time_ * 0.07f));
        set1f(progBg_, "uMandala", trip);
        set1f(progBg_, "uShift", shift_);
        set1f(progBg_, "uTime", time_);
        set1f(progBg_, "uFolds", 10.f);
        set1f(progBg_, "uWarp", 0.4f + 0.6f * wm);
        set1f(progBg_, "uCalm", 0.35f * (1.f - wm) * (1.f - trip));
        set1f(progBg_, "uDesat", 0.55f * slow_ + 0.3f * over_);
        set1f(progBg_, "uDim", 1.f);
        set4f(progBg_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
        set4f(progBg_, "uColR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        set3f(progBg_, "uVeil", wellTop_);
        ugl::drawFullscreen();
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // =========================================================== the garden and the floating icons
    const float zoneIn = layout::RIGHT_COLUMN[2] + 0.6f;
    const float zoneOut = std::max(zoneIn + 0.5f, viewH_.x - 0.9f);
    auto zoneX = [&](float side, float u) { return side * lerpf(zoneIn + 0.6f, zoneOut - 0.4f, u); };
    const float bottom = viewC_.y - viewH_.y;
    const float beatSway = std::sin((float)(music.hasSong ? music.beat : time_ * 2.0) * PI * 0.5f) * (0.04f + 0.06f * wm);
    // the big specials behind: the mandalas, the rainbow, the sun
    for (const Special& s : specials_) {
        if (s.grow <= 0.f) continue;
        const float g = ease(s.grow);
        const float x = zoneX(s.side, s.u);
        if (s.kind == K_MANDALA) {
            const float r = s.size * (0.3f + 0.7f * g) * (1.f + 0.04f * kickK_);
            sprite(K_MANDALA, x, s.y, r, r, 0.f, g, flower_[(int)(s.seed * 6) % 6], flower_[((int)(s.seed * 6) + 2) % 6],
                   center_, 12.f, time_ * 0.15f * s.side, 0.f);
        } else if (s.kind == K_RAINBOW) {
            const float r = s.size * (0.5f + 0.5f * g);
            sprite(K_RAINBOW, x, s.y, r, r, 0.f, g, hex(0xc9b6ff));
        } else if (s.kind == K_SUN) {
            const float r = s.size * (0.4f + 0.6f * g) * (1.f + 0.05f * kickK_);
            sprite(K_SUN, x, s.y, r, r, 0.f, g, center_, hex(0xff9a3c), hex(0xff6f7d), 0.f, time_ * 0.12f, s.seed);
        }
    }
    // floating icons (peace signs, hearts, sparkles, lava blobs) rising slowly up the sides
    {
        const float shown = 3.f + 6.f * wm + 9.f * wp;
        const float H = 2.f * viewH_.y + 3.f;
        Rng fr(0xF10A7ull);
        for (int i = 0; i < N_FLOATERS; i++) {
            const float side = i % 2 ? 1.f : -1.f, u = fr.uniform(), sp = fr.range(0.35f, 0.7f), ph = fr.uniform();
            const float size = fr.range(0.45f, 0.85f);
            const int kinds[6] = {K_PEACE, K_HEART, K_LAVA, K_SPARKLE, K_HEART, K_LAVA};
            const int kind = kinds[i % 6];
            const int ci = fr.irange(0, 5);
            const float vis = saturate(shown - (float)i);
            if (vis <= 0.f) continue;
            const float y = bottom - 1.5f + std::fmod(ph * H + time_ * sp * (0.6f + 0.4f * wm + 0.4f * wp), H);
            const float x = zoneX(side, u) + 0.5f * std::sin(time_ * 0.5f + ph * 9.f);
            const float edge = saturate((y - bottom + 1.f) / 2.f) * saturate((viewC_.y + viewH_.y + 1.f - y) / 2.f);
            const float r = size * (1.f + 0.12f * kickK_ * wp);
            sprite(kind, x, y, r, r, 0.3f * std::sin(time_ * 0.7f + ph * 5.f), vis * edge, flower_[ci], vec3(1.f),
                   vec3(1.f), 0.f, 0.f, ph, OUTLINE * 0.8f);
        }
    }
    // the doves circling
    for (const Special& s : specials_) {
        if (s.kind != K_DOVE || s.grow <= 0.f) continue;
        const float g = ease(s.grow);
        const float a = time_ * 0.35f + s.seed * TAU;
        const float x = zoneX(s.side, s.u) + 2.2f * std::cos(a), y = s.y + 1.2f * std::sin(a) + (1.f - g) * -3.f;
        const float dir = -std::sin(a) >= 0.f ? 1.f : -1.f;
        const float r = s.size;
        sprite(K_DOVE, x, y, r, r, 0.f, g, lerp(ink_, vec3(0.75f, 0.65f, 1.f), 0.5f), vec3(1.f), vec3(1.f), 0.f,
               time_ * 7.f + s.seed * 5.f, dir, OUTLINE * 0.8f);
    }
    // the flowers on their stems, from the bottom edge
    for (const Flower& fl : flowers_) {
        if (fl.grow <= 0.f) continue;
        const float gs = ease(fl.grow / 0.6f), gh = ease((fl.grow - 0.35f) / 0.65f);
        const float x = zoneX(fl.side, fl.u);
        const float h = fl.stem * gs;
        const float bend = (0.25f + 0.15f * std::sin(time_ * 0.6f + fl.seed * 20.f) + beatSway * 2.f) * (fl.seed < 0.5f ? 1.f : -1.f);
        if (h > 0.05f) sprite(K_STEM, x, bottom + h * 0.5f - 0.3f, 1.0f, h * 0.5f + 0.3f, 0.f, 1.f, green_, leaf_, vec3(1.f),
                              ease(fl.grow / 0.8f), bend, 0.f, 0.07f);
        if (gh > 0.f) {
            const float r = fl.head * (0.25f + 0.75f * gh) * (1.f + 0.07f * kickK_ * (0.4f + 0.6f * wp));
            const vec3 a = flower_[fl.col], b = flower_[fl.col2];
            const float tilt = 0.08f * std::sin(time_ * 0.6f + fl.seed * 20.f) + beatSway;
            if (fl.kind == K_MANDALA) sprite(K_MANDALA, x + bend, bottom + h - 0.3f, r, r, tilt, 1.f, a, b, center_,
                                             (float)fl.petals, time_ * 0.2f * (fl.seed - 0.5f), 0.f);
            else sprite(K_DAISY, x + bend, bottom + h - 0.3f, r, r, tilt, 1.f, a, fl.face ? a : b, center_, (float)fl.petals,
                        fl.face ? 1.f : 0.f, gh);
        }
    }
    for (const Special& s : specials_) {
        if (s.kind != K_MUSHROOM || s.grow <= 0.f) continue;
        const float g = ease(s.grow);
        const float r = s.size * g;
        sprite(K_MUSHROOM, zoneX(s.side, s.u), bottom + 0.15f + r * 0.95f, r, r, 0.f, 1.f, flower_[0], hex(0xfff1dc),
               vec3(1.f), 0.f, 0.f, s.seed);
    }
    // the clears' bursts (behind the well)
    for (const Part& p : parts_) {
        if (p.front || p.age < 0.f) continue;
        const float a = saturate(p.age / 0.1f) * (1.f - ease((p.age - p.life + 0.6f) / 0.6f));
        const float r = p.size;
        if (p.kind == K_DAISY) sprite(K_DAISY, p.x, p.y, r, r, p.rot, a, p.col, lerp(p.col, vec3(1.f), 0.5f), center_, 6.f, 0.f, 1.f, 0.07f);
        else if (p.kind == K_PETAL) sprite(K_PETAL, p.x, p.y, r, r * 0.55f, p.rot, a, p.col, vec3(1.f), vec3(1.f), 0.f, 0.f, 0.f, 0.06f);
        else sprite(p.kind, p.x, p.y, r, r, p.rot * 0.3f, a, p.col, vec3(1.f), vec3(1.f), 0.f, 0.f, 0.f, 0.07f);
    }
    flushSprites();

    if (!f.bare) {
        // =========================================================== the well
        sprite(K_PANEL, 0.f, 0.f, 5.12f, 10.12f, 0.f, 1.f, wellTop_, wellBottom_, accent_, 0.f, 0.f, 0.f, 0.06f);
        for (const Part& p : parts_) {
            if (!p.front) continue;
            const float a = 1.f - ease((p.age / p.life - 0.4f) / 0.6f);
            sprite(K_PETAL, p.x, p.y, p.size, p.size * 0.55f, p.rot, a, p.col, vec3(1.f), vec3(1.f), 0.f, 0.f, 0.f, 0.04f);
        }
        // the bonus flower: a petal per step, a face when it's ready
        {
            const vec3 gc = layout::GAUGE_CENTER;
            const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
            const bool on = game.bonusReady() || game.bonusActive();
            const float r = layout::GAUGE_R * 1.05f * (on ? 1.f + 0.04f * std::sin(time_ * 3.f) : 1.f);
            sprite(K_GAUGE, gc.x, gc.y, r, r, 0.f, 1.f, accent_, lerp(wellTop_, vec3(1.f), 0.25f), on ? center_ : lerp(wellTop_, center_, 0.5f),
                   fill, on ? 1.f : 0.f, 0.f, 0.08f);
        }
        flushSprites();

        // =========================================================== the blocks
        cells_.clear();
        buildBoard(game);
        if (!cells_.empty()) {
            glUseProgram(progCell_);
            ugl::setMat(progCell_, "uVP", vp_);
            set3f(progCell_, "uInk", ink_);
            set3f(progCell_, "uWhite", vec3(1.f));
            set1f(progCell_, "uFade", 1.f);
            glBindVertexArray(cellVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
            glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
        }
    }
    solid(0.f, 0.f, 0.f, 0.45f * dimT_);
    drawHud(f.hud, f.panels);
    solid(0.f, 0.f, 0.f, 1.f - saturate(f.fade));
    glDisable(GL_BLEND);

    if (gpuQ) {
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
        gpuSum += ns / 1e6;
        if (++gpuN % 120 == 0) {
            std::fprintf(stderr, "[woodstock] gpu %.2f ms/frame (lines %.1f)\n", gpuSum / 120.0, shown_);
            gpuSum = 0.0;
        }
    }
}

} // namespace

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_woodstock() {
    static const UniverseEntry e{
        "WOODSTOCK", "a late-60s psychedelic cartoon: a breathing kaleidoscope, flower power and peace signs, a garden growing with the lines",
        [] { return std::unique_ptr<Universe>(new WoodstockUniverse()); },
        // Bright, happy, groovy songs suit it best.
        [](const SongLook& l) { return 0.65f + 0.35f * l.brightness + (l.minor ? 0.f : 0.15f) + 0.1f * (1.f - l.bassWeight); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
