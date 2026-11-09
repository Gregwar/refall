// RAINY WINDOW: a window pane at night, the city out of focus behind it, the rain on the glass.
//
// One world: we look out through a window at night. Behind the glass the city is out of focus, only soft discs of
// light (street lamps, windows, cars drifting slowly along a street far below); in front of it the pane, with the
// water on it, is the subject. Small beads of rain sit everywhere on the glass; bigger drops hang still, swell as
// the rain feeds them, and on the music's beat grid some of them let go and run down, meandering, leaving a trail of
// tiny beads that dry away, wiping the mist off the glass as they go (the clear trails fog over again slowly). Each
// drop is a small lens: the city is upside down inside it, sharper than through the mist. The well is the middle
// pane of the window, between two thin mullions, above the bottom rail. The pieces are stained glass stuck on the
// pane: one slab of tinted cathedral glass per piece, leaded around, lit from behind by the city's light (the bokeh
// glows through the tiles). The stack is an obstacle to the water: a drop that runs onto a piece slides along its
// top to its edge and runs down its side, and leaves some water resting on the piece, so beads pool along the tops
// of the stack (the glass remembers where the water has been).
//
// Staging: a calm part is a light rain (a drop lets go every two bars, a thick mist, a few lights out); mid sets the
// drops running on the beat grid, every two beats; a peak is a downpour (runs of several drops every two beats, the
// rain thick on the glass, the mist washed off) and its kicks light a few of the city's lights. The drop shifts the
// whole city to its night's other colors (sodium to neon...) on the downbeat and lets every hanging drop go at once;
// the colors come back slowly after the peak. A cleared row melts: its glass runs down in drips and clears to water
// in a third of a second, the water running down over the stack. A Tetris lets every drop go and sends the rows'
// colors into the city as four lanterns of light rising away. Slow motion is a later, colder hour: the rain almost
// still, the city drained to blue. Game over melts the stack from the top and the pane mists over.
//
// Cost: the city at half resolution (instanced soft discs) blurred at a quarter, the water as a height field of
// instanced domes (a few hundred), a 400 x 225 mist grid on the CPU, one full-screen pass for the pane.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/rainy/rainy.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/rainy/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Josefin Sans (Light: the labels, the song; Regular: the numbers), SIL Open Font License, see OFL-JosefinSans.txt.
const unsigned char FONT_LIGHT[] = {
#include "universes/rainy/JosefinSans-Light.ttf.inc"
};
const unsigned char FONT_REG[] = {
#include "universes/rainy/JosefinSans-Regular.ttf.inc"
};

namespace shaders = rainy_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
// A blend of two palette colors in display space (a linear mix toward a dark color looks washed at mid values).
vec3 mixDisplay(unsigned a, unsigned b, float t) { return ugl::toLinear(lerp(hex(a), hex(b), t)); }

// The nights: the sky (top, low), the mist's tint, the city's four light colors and the drop's, the seven glasses
// of the pieces, the accent.
struct VariantDef {
    const char* name;
    unsigned skyTop, skyLow, mist;
    unsigned lamp[4], dropLamp[4];
    unsigned glass[7]; // I O T S Z J L
    unsigned accent;
};
const VariantDef VARIANTS[] = {
    // sodium street lamps and warm windows; the drop turns the city to neon
    {"SODIUM", 0x06080f, 0x1a1418, 0x2a221e,
     {0xffa64a, 0xffd9a8, 0xff5a46, 0xdfe6ff}, {0xff4fb4, 0x4fd6ff, 0x9a6bff, 0xf4ecff},
     {0x7fd6ec, 0xf6d56a, 0xc49cf2, 0x8edd8a, 0xf38b80, 0x7c9ef6, 0xf8a95c}, 0xffb35c},
    // a neon district in pink and cyan; the drop turns it to gold
    {"NEON", 0x0a0614, 0x1e0e26, 0x2a1830,
     {0xff4fa8, 0x46d4ff, 0x7c6bff, 0xffe2f0}, {0xffbf4a, 0xfff0c8, 0xff7838, 0x9fffd0},
     {0x9cf0ee, 0xfff09c, 0xe2a8ff, 0xa6f4b4, 0xff9aa6, 0xa2b4ff, 0xffc690}, 0xff6fb8},
    // the blue hour, warm windows coming on; the drop: rose and violet lights
    {"BLUE HOUR", 0x0c1830, 0x26344e, 0x2c3a52,
     {0xffc878, 0xfff0d4, 0xff9a5a, 0xa8d2ff}, {0xff7a9c, 0xffd2e2, 0xc08aff, 0xffffff},
     {0x9ce8ff, 0xffe27a, 0xe8a2ff, 0xa4f08a, 0xff8c82, 0xc4ccff, 0xffb25c}, 0xffc878},
    // tail lights and amber signals in the dark; the drop: every light turns green
    {"TAIL LIGHTS", 0x050506, 0x150a0a, 0x1e1414,
     {0xff3b2e, 0xffa040, 0xfff2de, 0xc01e28}, {0x3cff9c, 0x40e0d0, 0xe8fff4, 0xb8ff6a},
     {0x8fe2ff, 0xffe9a6, 0xd4aaff, 0x9cf2c0, 0xff9e9e, 0x90a8ff, 0xffc88a}, 0xff5a46},
    // a harbor in the fog, pale lamps; the drop: rose and gold
    {"HARBOR FOG", 0x0b1214, 0x1e2a2a, 0x34403e,
     {0xffe2a0, 0xd8fff0, 0xffae6c, 0x86ccff}, {0xff8fb2, 0xffd6a0, 0xb4a2ff, 0xffffff},
     {0x82e2d8, 0xf6dc8a, 0xd4a4ea, 0xaae28a, 0xf2927c, 0x8eaaea, 0xf2b272}, 0xffe2a0},
    // after midnight: a deep indigo, a few cold lights; the drop: warm gold and red
    {"AFTER MIDNIGHT", 0x05060f, 0x0e1024, 0x181a2e,
     {0xbcd2ff, 0xffffff, 0x6f8cff, 0xffd9a0}, {0xffc24a, 0xff6a4a, 0xffe6b0, 0xff9a3c},
     {0x52d2ff, 0xffd040, 0xb67eff, 0x62e28c, 0xff6070, 0x6482ff, 0xff9c40}, 0xbcd2ff},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// UTF-8 to the font's characters: the middle dot is glyph 127, accented Latin letters lose their accent, anything
// else outside ASCII becomes a '?'.
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

// Board space <-> game cells.
inline int colOf(float x) { return (int)std::floor(x + 5.f); }
inline int rowOf(float y) { return (int)std::floor(12.f - y); }
constexpr float MULLION = 5.17f, MULLION_HW = 0.13f, RAIL = -10.02f;

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool RainyUniverse::init(int width, int height) {
    using shaders::withCommon;
    progSky_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::SKY_FS).c_str(), "rainy sky");
    progBokeh_ = ugl::program(shaders::BOKEH_VS, withCommon(shaders::BOKEH_FS).c_str(), "rainy bokeh");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "rainy blur");
    progWater_ = ugl::program(shaders::WATER_VS, shaders::WATER_FS, "rainy water");
    progGlass_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::GLASS_FS).c_str(), "rainy glass");
    progCell_ = ugl::program(shaders::CELL_VS, withCommon(shaders::CELL_FS).c_str(), "rainy cells");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "rainy final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "rainy shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "rainy text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, size_t stride, int attrs) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < attrs; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(cellVao_, cellVbo_, sizeof(CellInst), 3);
    instanced(waterVao_, waterVbo_, sizeof(WInst), 2);
    instanced(bokehVao_, bokehVbo_, sizeof(BInst), 2);
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    glGenTextures(1, &mistTexId_);
    glBindTexture(GL_TEXTURE_2D, mistTexId_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    buildFont(fontLight_, FONT_LIGHT);
    buildFont(fontReg_, FONT_REG);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void RainyUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] rainy font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = 64.f;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AW = 1024, AH = 1024, PAD = 8;
    std::vector<unsigned char> atlas(AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    for (int ch = 32; ch < 128; ch++) {
        const int cp = ch == 127 ? 0xB7 : ch; // the middle dot in DEL's slot
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, cp, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
        if (!sdf) { g.w = g.h = 0; continue; }
        if (x + w + 1 > AW) { x = 1; y += rowH + 1; rowH = 0; }
        if (y + h + 1 > AH) { stbtt_FreeSDF(sdf, nullptr); break; }
        for (int r = 0; r < h; r++) std::memcpy(&atlas[(y + r) * AW + x], sdf + r * w, w);
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

void RainyUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    const int hw2 = std::max(1, width_ / 2), hh2 = std::max(1, height_ / 2);
    cityT_.create(hw2, hh2, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    blurA_.create(std::max(1, width_ / 4), std::max(1, height_ / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    blurB_.create(std::max(1, width_ / 4), std::max(1, height_ / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    waterT_.create(width_, height_, GL_R16F, GL_RED, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    // the mist: a coarse grid over the whole view
    mw_ = 400;
    mh_ = std::max(16, (int)std::lround(400.f / aspect));
    mist_.assign((size_t)mw_ * mh_, 0.5f);
    mistTex_.assign((size_t)mw_ * mh_, 128);
    planCity();
}

RainyUniverse::~RainyUniverse() {
    for (ugl::Target* t : {&cityT_, &blurA_, &blurB_, &waterT_, &sceneT_}) t->destroy();
    for (GLuint p : {progSky_, progBokeh_, progBlur_, progWater_, progGlass_, progCell_, progFinal_, progShape_, progText_})
        glDeleteProgram(p);
    for (GLuint v : {cellVao_, waterVao_, bokehVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, waterVbo_, bokehVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    if (mistTexId_) glDeleteTextures(1, &mistTexId_);
    for (Font* f : {&fontLight_, &fontReg_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The city
// =====================================================================================================

// The city of this song, out of focus: far lights (small discs, most of them low, where the streets are), a few
// near ones (big and dim), some up high (towers), cars drifting along a street far below.
void RainyUniverse::planCity() {
    lights_.clear();
    Rng r(seed_ ^ 0xB0E4ull);
    const float X = viewH_.x + 1.5f;
    auto pal = [&]() {
        const float w[4] = {0.45f, 0.25f, 0.14f, 0.16f};
        return r.weighted(w);
    };
    for (int i = 0; i < 38; i++) {
        Light l;
        l.x = r.range(-X, X);
        l.y = -11.5f + 15.f * std::pow(r.uniform(), 1.5f);
        l.r = r.range(0.45f, 0.95f);
        l.base = r.range(0.35f, 0.8f);
        l.pal = pal();
        l.soft = 0.14f;
        lights_.push_back(l);
    }
    for (int i = 0; i < 9; i++) {
        Light l;
        l.x = r.range(-X, X);
        l.y = r.range(-12.f, 5.f);
        l.r = r.range(1.6f, 3.0f);
        l.base = r.range(0.1f, 0.2f);
        l.pal = pal();
        l.soft = 0.3f;
        lights_.push_back(l);
    }
    for (int i = 0; i < 6; i++) {
        Light l;
        l.x = r.range(-X, X);
        l.y = r.range(4.f, 11.f);
        l.r = r.range(0.35f, 0.6f);
        l.base = r.range(0.25f, 0.5f);
        l.pal = r.chance(0.5f) ? 3 : 0;
        l.soft = 0.14f;
        lights_.push_back(l);
    }
    // cars: head lights one way, tail lights the other
    for (int i = 0; i < 6; i++) {
        Light l;
        const bool right = i % 2 == 0;
        l.x = r.range(-X, X);
        l.y = -9.3f + r.range(-0.4f, 0.4f) + (right ? 0.f : -0.5f);
        l.r = r.range(0.5f, 0.75f);
        l.base = 0.55f;
        l.pal = right ? 2 : 1;
        l.vx = (right ? 1.f : -1.f) * r.range(0.22f, 0.4f);
        l.soft = 0.16f;
        lights_.push_back(l);
    }
    // a few of the far lights glow with the kicks of a peak
    for (int i = 0; i < 7; i++) lights_[(size_t)r.irange(0, 37)].bass = true;
    for (Light& l : lights_) {
        l.onTo = r.chance(0.8f) ? 1.f : 0.f;
        l.on = 0.f; // a new city fades in
    }
}

vec3 RainyUniverse::sky(int i) const {
    const VariantDef& V = VARIANTS[variant_];
    return lin(i == 0 ? V.skyTop : V.skyLow);
}

vec3 RainyUniverse::lamp(int i) const {
    const VariantDef& V = VARIANTS[variant_];
    return mixDisplay(V.lamp[i & 3], V.dropLamp[i & 3], smoothstepf(0.f, 1.f, dev_));
}

vec3 RainyUniverse::tint(int i) const { return lin(VARIANTS[variant_].glass[std::clamp(i, 0, 6)]); }

vec3 RainyUniverse::accent() const {
    const VariantDef& V = VARIANTS[variant_];
    return mixDisplay(V.accent, V.dropLamp[0], smoothstepf(0.f, 1.f, dev_));
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void RainyUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.4f) v = h < 0.5f ? 3 : 5;
        else if (t.hasSong && t.brightness > 0.62f) v = 2;
        else if (t.hasSong && t.bassWeight > 0.62f) v = 1;
        else if (t.hasSong && t.dynamics < 0.3f) v = 4;
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x3A1Bull);
    planCity();
    lanterns_.clear();
    if (seconds <= 0.1f)
        for (Light& l : lights_) l.on = l.onTo;
}

void RainyUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
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
    (void)impact;
}

// The drop: the city's lights shift to the night's other colors, every hanging drop lets go, the rain thickens.
void RainyUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    for (Drop& d : drops_)
        if (d.st == STUCK && d.r > 0.17f) { d.st = RUN; d.vy = 0.5f; }
    releaseRuns(6, true);
    for (int i = 0; i < 40; i++)
        spawnBead(rng_.range(-viewH_.x, viewH_.x), rng_.range(-10.f, viewC_.y + viewH_.y), 0.05f + 0.08f * rng_.uniform(), 0.f);
}

// A new section: a few of the city's lights go out and others come on.
void RainyUniverse::refresh(float strength) {
    const int n = 3 + (int)(5 * strength);
    for (int i = 0; i < n && !lights_.empty(); i++) {
        Light& l = lights_[(size_t)rng_.irange(0, (int)lights_.size() - 1)];
        if (l.vx == 0.f) l.onTo = l.onTo > 0.5f ? 0.f : 1.f;
    }
}

void RainyUniverse::levelUp() { releaseRuns(4, true); }

vec2 RainyUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints RainyUniverse::hints() const {
    TransitionHints h;
    h.background = lerp(sky(0), sky(1), 0.6f);
    h.light = false;
    vec3 ink(0.f);
    for (int i = 0; i < 7; i++) ink = ink + tint(i) * (1.f / 7.f);
    h.ink = ink;
    h.accent = accent();
    h.styles = TS_RIPPLE | TS_LUMA | TS_INK;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the pieces' ids in step with the board (a new board, or one changed behind our back, gets fresh ids).
void RainyUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) { ids_[y][x] = 0; glint_[y][x] = 0.f; }
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
                const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& dd : d) {
                    const int nx = cx + dd[0], ny = cy + dd[1];
                    if (nx < 0 || ny < 0 || nx >= Game::W || ny >= Game::H || ids_[ny][nx]) continue;
                    if (g.cell(nx, ny).type != g.cell(x, y).type || sp >= 8 || count + sp >= 4) continue;
                    ids_[ny][nx] = id;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

void RainyUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    // a melting tile's neighbour mask (the shader's bits), among the cells melting with it (rows: the cleared rows,
    // null: the whole board)
    auto maskAt = [&](int x, int y, const bool* rows) {
        const int id = ids_[y][x];
        if (!id) return 0;
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, -1, 1, -1, -1, 1, 1};
        int m = 0;
        for (int i = 0; i < 8; i++) {
            const int nx = x + dx[i], ny = y + dy[i];
            if (nx < 0 || ny < 0 || nx >= Game::W || ny >= Game::H) continue;
            if (rows && !rows[ny]) continue;
            if (ids_[ny][nx] == id) m |= 1 << i;
        }
        return m;
    };
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            glint_[c.y][c.x] = 0.22f; // the glass pressed on the pane
        }
        break;
    }
    case GameEvent::HardDrop: {
        // the water under the tile is squeezed out at its lower corners and runs down
        if (ev.cells.empty()) break;
        int minx = 99, maxx = -99;
        for (auto& c : ev.cells) { minx = std::min(minx, c.x); maxx = std::max(maxx, c.x); }
        int lowL = -99, lowR = -99;
        for (auto& c : ev.cells) {
            if (c.x == minx) lowL = std::max(lowL, c.y);
            if (c.x == maxx) lowR = std::max(lowR, c.y);
        }
        for (int side = 0; side < 2; side++) {
            const int cx = side == 0 ? minx - 1 : maxx + 1, cy = side == 0 ? lowL : lowR;
            if (cx < 0 || cx >= Game::W || cy < Game::HIDDEN) continue;
            Drop d;
            d.r = rng_.range(0.15f, 0.2f);
            d.x = (side == 0 ? (float)minx - 5.f - d.r * 0.6f : (float)(maxx + 1) - 5.f + d.r * 0.6f);
            d.y = 12.f - cy - 0.6f;
            d.st = RUN;
            d.vy = 1.5f;
            d.grow = 0.f;
            d.wig = rng_.range(0.f, 6.f);
            drops_.push_back(d);
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // the cleared rows melt into water
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int m = maskAt(c.x, c.y, rows);
            melt_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f, 0.f, (float)(ids_[c.y][c.x] % 97)});
            if (rng_.chance(0.55f))
                splash_.push_back({p.x + rng_.range(-0.3f, 0.3f), p.y - 0.2f, rng_.range(0.1f, 0.17f), rng_.range(0.5f, 1.5f), 0.f,
                                   rng_.range(0.45f, 0.6f)});
        }
        {
            int nb[Game::H][Game::W] = {};
            float ng[Game::H][Game::W] = {}, np[Game::H][Game::W] = {}, ns[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(ng[dst], glint_[y], sizeof(glint_[y]));
                std::memcpy(np[dst], pool_[y], sizeof(pool_[y]));
                std::memcpy(ns[dst], poolShow_[y], sizeof(poolShow_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(glint_, ng, sizeof(glint_));
            std::memcpy(pool_, np, sizeof(pool_));
            std::memcpy(poolShow_, ns, sizeof(poolShow_));
        }
        if (ev.count >= 4) {
            // a Tetris: every drop lets go, and the rows' colors rise into the city as lanterns
            for (Drop& d : drops_)
                if (d.st == STUCK && d.r > 0.15f) { d.st = RUN; d.vy = 0.3f; }
            releaseRuns(8, true);
            int types[Game::W * 4];
            int nt = 0;
            for (auto& c : ev.cells)
                if (nt < Game::W * 4) types[nt++] = std::clamp(c.type, 0, 6);
            float ymid = 0.f;
            int ny = 0;
            for (auto& c : ev.cells) { ymid += layout::cellPos(0.f, (float)c.y).y; ny++; }
            ymid = ny ? ymid / ny : 0.f;
            for (int i = 0; i < 12; i++) {
                Lantern l;
                l.x = (i % 2 ? 1.f : -1.f) * rng_.range(11.8f, std::max(12.f, viewH_.x - 1.f));
                l.y = ymid + rng_.range(-1.f, 3.f);
                l.r = rng_.range(1.0f, 1.7f);
                l.t = -0.08f * i;
                l.life = rng_.range(3.8f, 5.0f);
                {
                    // the glass color, deepened (a pale tint added to the city would read white)
                    const vec3 t = tint(nt ? types[rng_.irange(0, nt - 1)] : i % 7);
                    const float mx = std::max(t.x, std::max(t.y, t.z));
                    l.col = vec3(std::pow(t.x / mx, 2.2f), std::pow(t.y / mx, 2.2f), std::pow(t.z / mx, 2.2f));
                }
                lanterns_.push_back(l);
            }
        }
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        // the stack stays a moment, then melts row by row from the top
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            const int m = maskAt(c.x, c.y, nullptr);
            melt_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f, 0.7f + 0.11f * (c.y - Game::HIDDEN), (float)(id % 97)});
        }
        for (Drop& d : drops_)
            if (d.st == STUCK && d.r > 0.15f) { d.st = RUN; d.vy = 0.3f; }
        break;
    default: break;
    }
}

// =====================================================================================================
// The water on the glass
// =====================================================================================================

bool RainyUniverse::occupied(const Game& g, int cx, int cy) const {
    if (cx < 0 || cx >= Game::W || cy < 0) return false;
    if (cy >= Game::H) return true;
    return g.cell(cx, cy).type >= 0;
}

// The HUD's places in board space (x0, y0, x1, y1): the two columns, the top left corner (the song), the bottom left
// corner (the section, the universe).
float RainyUniverse::hudBox(int b, int k) const {
    const float left = viewC_.x - viewH_.x, bottom = viewC_.y - viewH_.y, top = viewC_.y + viewH_.y;
    const float boxes[4][4] = {
        {layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]},
        {layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]},
        {left, top - 2.4f, left + 9.5f, top},
        {left, bottom, left + 11.f, bottom + 3.2f},
    };
    return boxes[b & 3][k & 3];
}

float RainyUniverse::hudMask(float x, float y) const {
    float m = 0.f;
    for (int b = 0; b < 4; b++) {
        const float dx = std::max(hudBox(b, 0) - x, x - hudBox(b, 2)), dy = std::max(hudBox(b, 1) - y, y - hudBox(b, 3));
        m = std::max(m, 1.f - smoothstepf(0.f, 1.6f, std::max(dx, dy)));
    }
    return m;
}

void RainyUniverse::spawnBead(float x, float y, float r, float grow) {
    if (std::fabs(std::fabs(x) - MULLION) < MULLION_HW + r || y - r < RAIL) return;
    const size_t cap = 900;
    if (beads_.size() >= cap) {
        // the glass is full: an old bead dries away for each new one
        Bead& o = beads_[(size_t)rng_.irange(0, (int)beads_.size() - 1)];
        if (o.dryMax <= 0.f) o.dry = o.dryMax = 0.4f;
        if (beads_.size() >= cap + 200) return;
    }
    beads_.push_back(Bead{x, y, r, grow, 0.f, 0.f});
}

// A drop hanging on the glass (anywhere: a new one fed by the rain; else from the top of the pane).
bool RainyUniverse::spawnStuck(bool anywhere) {
    for (int tries = 0; tries < 6; tries++) {
        Drop d;
        d.x = rng_.range(-viewH_.x + 0.5f, viewH_.x - 0.5f);
        d.y = anywhere ? rng_.range(-8.f, viewC_.y + viewH_.y - 0.3f) : viewC_.y + viewH_.y + 0.4f + rng_.range(0.f, 3.5f);
        d.r = 0.2f + 0.24f * rng_.uniform() * rng_.uniform() + 0.06f * rng_.uniform();
        if (std::fabs(std::fabs(d.x) - MULLION) < MULLION_HW + d.r) continue;
        if (rng_.uniform() < 0.65f * hudMask(d.x, d.y)) continue;
        d.st = STUCK;
        d.grow = anywhere ? 0.f : 1.f;
        d.wig = rng_.range(0.f, 6.28f);
        drops_.push_back(d);
        return true;
    }
    return false;
}

// Lets n hanging drops go (the biggest, the highest first), or new ones from above the pane when there are too few.
void RainyUniverse::releaseRuns(int n, bool upper) {
    for (int k = 0; k < n; k++) {
        int best = -1;
        float bs = -1e9f;
        for (size_t i = 0; i < drops_.size(); i++) {
            const Drop& d = drops_[i];
            if (d.st != STUCK || d.r < 0.18f || d.grow < 0.9f) continue;
            const float s = d.r * 6.f + (upper ? d.y * 0.08f : 0.f) + rng_.uniform() * 1.2f;
            if (s > bs) { bs = s; best = (int)i; }
        }
        if (best >= 0) {
            drops_[(size_t)best].st = RUN;
            drops_[(size_t)best].vy = 0.4f;
        } else {
            if (!spawnStuck(false)) continue;
            drops_.back().st = RUN;
            drops_.back().vy = 2.f;
            drops_.back().r = rng_.range(0.24f, 0.36f);
        }
    }
}

void RainyUniverse::clearMist(float x, float y, float r) {
    if (mist_.empty()) return;
    const float cw = 2.f * viewH_.x / mw_, ch = 2.f * viewH_.y / mh_;
    const float gx = (x - (viewC_.x - viewH_.x)) / cw, gy = (y - (viewC_.y - viewH_.y)) / ch;
    const int rx = (int)std::ceil(r * 1.3f / cw) + 1, ry = (int)std::ceil(r * 1.3f / ch) + 1;
    for (int j = (int)gy - ry; j <= (int)gy + ry; j++) {
        if (j < 0 || j >= mh_) continue;
        for (int i = (int)gx - rx; i <= (int)gx + rx; i++) {
            if (i < 0 || i >= mw_) continue;
            const float dx = (i + 0.5f - gx) * cw, dy = (j + 0.5f - gy) * ch;
            const float d = std::sqrt(dx * dx + dy * dy);
            float& m = mist_[(size_t)j * mw_ + i];
            m *= smoothstepf(r * 0.5f, r * 1.3f, d);
        }
    }
}

void RainyUniverse::stepWater(const Game& g, float dt) {
    if (dt <= 0.f) return;
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const float top = viewC_.y + viewH_.y;
    // the rain: new beads landing on the glass (feeding the hanging drops they hit)
    const float rate = (5.f + 13.f * wm + 30.f * wp + 12.f * (dropMode_ ? 1.f : 0.f)) * (1.f - 0.85f * over_);
    rainAcc_ += rate * dt;
    while (rainAcc_ >= 1.f) {
        rainAcc_ -= 1.f;
        const float x = rng_.range(-viewH_.x, viewH_.x), y = rng_.range(-10.f, top);
        const float r = 0.055f + 0.1f * rng_.uniform() * rng_.uniform() + 0.025f * rng_.uniform();
        if (rng_.uniform() < 0.7f * hudMask(x, y)) continue;
        if (std::fabs(x) < 5.f && occupied(g, colOf(x), rowOf(y))) continue;
        bool fed = false;
        for (Drop& d : drops_) {
            if (d.st != STUCK) continue;
            const float dx = d.x - x, dy = d.y - y;
            if (dx * dx + dy * dy < (d.r + r) * (d.r + r)) {
                d.r = std::min(0.52f, std::sqrt(d.r * d.r + r * r * 0.7f));
                fed = true;
                break;
            }
        }
        if (!fed) spawnBead(x, y, r, 0.f);
    }
    // hanging drops: the population follows the rain
    int stuck = 0;
    for (const Drop& d : drops_)
        if (d.st == STUCK) stuck++;
    const int want = (int)(24.f + 14.f * wm + 12.f * wp);
    if (stuck < want) {
        stuckAcc_ += dt * (3.f + 4.f * wp);
        while (stuckAcc_ >= 1.f) { stuckAcc_ -= 1.f; spawnStuck(true); }
    }

    for (size_t i = 0; i < drops_.size(); i++) {
        Drop& d = drops_[i];
        d.grow = std::min(1.f, d.grow + dt * 5.f);
        if (d.st == GONE) {
            if (d.merging) {
                d.x += (d.mx - d.x) * (1.f - std::exp(-dt * 12.f));
                d.y += (d.my - d.y) * (1.f - std::exp(-dt * 12.f));
                d.r = approach(d.r, 0.f, 11.f, dt);
                d.fade -= dt * 3.5f;
            } else d.fade -= dt * 2.2f;
            continue;
        }
        const bool inWell = std::fabs(d.x) < 5.f;
        if (d.st == STUCK) {
            if (inWell && occupied(g, colOf(d.x), rowOf(d.y)) && rowOf(d.y) < Game::H) { d.st = GONE; }
            continue;
        }
        if (d.st == SLIDE) {
            const int cx = colOf(d.x);
            if (occupied(g, cx, d.row - 1) && d.row - 1 >= 0) { d.st = GONE; continue; }
            const float dir = d.tx > d.x ? 1.f : -1.f;
            d.x += dir * std::min(std::fabs(d.tx - d.x), dt * 2.4f);
            if (occupied(g, cx, d.row) && d.row < Game::H) pool_[d.row][cx] = std::min(1.f, pool_[d.row][cx] + dt * 0.35f);
            d.r = std::max(0.15f, d.r - dt * 0.03f);
            if (!occupied(g, colOf(d.x), d.row) || std::fabs(d.tx - d.x) < 1e-3f) { d.st = RUN; d.vy = 0.6f; }
            clearMist(d.x, d.y, d.r * 1.1f);
            continue;
        }
        // RUN
        const float vmax = std::clamp(2.6f + 16.f * (d.r - 0.15f), 1.6f, 8.5f);
        d.vy = approach(d.vy, vmax, 3.f, dt);
        d.wig += dt * d.vy * 1.6f;
        d.vx = std::sin(d.wig + d.r * 40.f) * 0.3f;
        d.x += d.vx * dt;
        d.y -= d.vy * dt;
        // the mullions: a drop runs beside them, never over
        const float ax = std::fabs(d.x), sx = d.x < 0 ? -1.f : 1.f;
        if (ax < 5.f) d.x = sx * std::min(ax, 5.f - d.r * 0.55f);
        else if (ax < MULLION + MULLION_HW + d.r * 0.55f) d.x = sx * (MULLION + MULLION_HW + d.r * 0.55f);
        d.travel += d.vy * dt;
        if (d.travel > d.gap) {
            // a trail bead now and then, at irregular steps
            d.travel = 0.f;
            d.gap = rng_.range(0.18f, 0.7f);
            const float tr = std::max(0.045f, d.r * rng_.range(0.16f, 0.34f));
            Bead b{d.x + rng_.range(-0.09f, 0.09f), d.y + d.r * 0.9f, tr, 1.f, 0.f, 0.f};
            b.dryMax = b.dry = rng_.range(3.f, 7.f);
            if (beads_.size() < 1100) beads_.push_back(b);
            d.r = std::sqrt(std::max(0.01f, d.r * d.r - tr * tr * 0.3f));
        }
        clearMist(d.x, d.y, d.r * 0.95f);
        // it picks up the beads it meets and the hanging drops it hits
        for (size_t k = 0; k < beads_.size();) {
            const Bead& b = beads_[k];
            const float dx = b.x - d.x, dy = b.y - d.y;
            if (b.dry <= 0.f && dx * dx + dy * dy < (d.r * 0.9f + b.r) * (d.r * 0.9f + b.r)) {
                d.r = std::min(0.48f, std::sqrt(d.r * d.r + b.r * b.r * 0.6f));
                // drawn away into the drop in a tenth of a second
                beads_[k].dry = beads_[k].dryMax = 0.1f;
            }
            k++;
        }
        for (size_t k = 0; k < drops_.size(); k++) {
            if (k == i) continue;
            Drop& o = drops_[k];
            if (o.st != STUCK) continue;
            const float dx = o.x - d.x, dy = o.y - d.y;
            if (dx * dx + dy * dy < (d.r + o.r) * (d.r + o.r) * 0.6f) {
                d.r = std::min(0.5f, std::sqrt(d.r * d.r + o.r * o.r));
                o.st = GONE;
                o.fade = 1.f;
                o.merging = true;
                o.mx = d.x;
                o.my = d.y;
            }
        }
        if (d.r < 0.13f) { d.st = STUCK; d.vy = 0.f; continue; }
        // the bottom rail
        const float bottom = d.y - d.r * 0.85f;
        if (bottom < RAIL) { d.st = GONE; continue; }
        // the stack: onto a piece's top, then along it to its edge (or into a pool in a pit)
        if (std::fabs(d.x) < 5.f) {
            const int cx = colOf(d.x), cy = rowOf(bottom);
            if (cy >= Game::HIDDEN && occupied(g, cx, cy) && (12.f - cy) - bottom > 0.35f) {
                // deep inside a cell: a piece locked over it, it fades out under the glass
                d.st = GONE;
            } else if (cy >= Game::HIDDEN && occupied(g, cx, cy)) {
                const float ny = 12.f - cy + d.r * 0.85f;
                d.oy -= ny - d.y;
                d.y = ny;
                if (cy < Game::H) pool_[cy][cx] = std::min(1.f, pool_[cy][cx] + d.r * 0.8f);
                float bestD = 1e9f, bestX = 0.f;
                for (int dir = -1; dir <= 1; dir += 2) {
                    int k = cx;
                    while (occupied(g, k + dir, cy) && !occupied(g, k + dir, cy - 1)) k += dir;
                    const int nk = k + dir;
                    if (nk < 0 || nk >= Game::W || occupied(g, nk, cy - 1) || occupied(g, nk, cy)) continue;
                    const float bx = dir > 0 ? (float)(k + 1) - 5.f : (float)k - 5.f;
                    const float tx = bx + dir * d.r * 0.7f;
                    const float dist = std::fabs(tx - d.x) + rng_.uniform() * 0.3f;
                    if (dist < bestD) { bestD = dist; bestX = tx; }
                }
                if (bestD < 1e8f) { d.st = SLIDE; d.tx = bestX; d.row = cy; d.vy = 0.f; }
                else d.st = GONE;
            }
        }
    }
    // what is drawn eases toward the simulation
    for (Drop& d : drops_) {
        const float target = d.st == RUN ? std::min(0.7f, d.vy * 0.09f) : d.st == SLIDE ? 0.1f : 0.f;
        if (d.rv < 0.f) { d.rv = d.r; d.el = target; }
        d.rv = approach(d.rv, d.r, d.merging ? 18.f : 9.f, dt);
        d.el = approach(d.el, target, 9.f, dt);
        d.oy = approach(d.oy, 0.f, 12.f, dt);
    }
    drops_.erase(std::remove_if(drops_.begin(), drops_.end(), [](const Drop& d) {
                     return (d.st == GONE && d.fade <= 0.f) || d.y < -14.f;
                 }), drops_.end());
    if (drops_.size() > 160)
        for (size_t k = 0, n = drops_.size() - 160; k < drops_.size() && n > 0; k++)
            if (drops_[k].st == STUCK) { drops_[k].st = GONE; drops_[k].fade = std::min(drops_[k].fade, 1.f); n--; }

    // beads: appear, trails dry away, the stack covers some
    for (size_t k = 0; k < beads_.size();) {
        Bead& b = beads_[k];
        b.grow = std::min(1.f, b.grow + dt * 8.f);
        bool dead = false;
        if (b.dryMax > 0.f) { b.dry -= dt; dead = b.dry <= 0.f; }
        if (!dead && std::fabs(b.x) < 5.f && occupied(g, colOf(b.x), rowOf(b.y)) && rowOf(b.y) < Game::H) dead = true;
        if (dead) { beads_[k] = beads_.back(); beads_.pop_back(); }
        else k++;
    }

    // pools: water resting on the top faces of the stack evaporates very slowly; a covered face loses it
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            float& p = pool_[y][x];
            if (p <= 0.f) continue;
            const bool face = occupied(g, x, y) && (y == 0 || !occupied(g, x, y - 1));
            p = face ? std::max(0.f, p - dt * 0.004f) : std::max(0.f, p - dt * 3.f);
        }

    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) poolShow_[y][x] = approach(poolShow_[y][x], pool_[y][x], 5.f, dt);

    // the mist grows back over the trails
    const float mistTo = std::clamp(0.62f - 0.12f * wm - 0.2f * wp + 0.25f * slow_ + 0.4f * over_, 0.f, 1.f);
    const float k = 1.f - std::exp(-dt * 0.11f);
    for (float& m : mist_) m += (mistTo - m) * k;
}

// The water's height field: hanging and running drops, beads, the pools on the stack.
void RainyUniverse::buildWater(const Game& g) {
    water_.clear();
    auto push = [&](float x, float y, float r, float elong, float h) {
        if (r <= 0.01f || h <= 0.01f) return;
        water_.push_back({{x, y, r, elong}, {h, 0.f, 0.f, 0.f}});
    };
    for (const Bead& b : beads_) {
        float r = b.r * smoothstepf(0.f, 1.f, b.grow);
        if (b.dryMax > 0.f) r *= std::sqrt(saturate(b.dry / b.dryMax));
        push(b.x, b.y, r, 0.f, 0.8f);
    }
    for (const Drop& d : drops_) {
        const float r = (d.rv < 0.f ? d.r : d.rv) * smoothstepf(0.f, 1.f, d.grow);
        const float elong = d.el;
        const float h = d.st == GONE ? smoothstepf(0.f, 1.f, d.fade) : 1.f;
        push(d.x, d.y + d.oy, r, elong, h);
    }
    // the pools resting on the tops of the pieces
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const float p = poolShow_[y][x];
            if (p < 0.005f || g.cell(x, y).type < 0) continue;
            const float off = g.rowOffset(y);
            const float topY = 12.f - (y - off);
            const float h = (float)((x * 37 + 11) % 100) / 100.f;
            const float r = (0.07f + 0.17f * std::sqrt(p)) * smoothstepf(0.f, 0.06f, p);
            push((float)x - 4.5f + (h - 0.5f) * 0.5f, topY + r * 0.5f, r, 0.f, 0.9f);
            const float r2 = (0.06f + 0.08f * p) * smoothstepf(0.35f, 0.6f, p);
            if (r2 > 0.01f) push((float)x - 4.5f + (0.5f - h) * 0.6f, topY + r2 * 0.5f, r2, 0.f, 0.9f);
        }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void RainyUniverse::buildBoard(const Game& g) {
    // neighbour mask (board space bits: 0 -x, 1 +x, 2 up, 3 down, 4 -x up, 5 +x up, 6 -x down, 7 +x down), in game
    // coordinates (y grows down)
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, -1, 1, -1, -1, 1, 1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    auto push = [&](float x, float y, float half, int mask, const vec3& c, float alpha, int kind, float prog, float glint, float seed) {
        if (alpha <= 0.003f) return;
        cells_.push_back({{x, y, half, (float)mask}, {c.x, c.y, c.z, alpha}, {(float)kind, prog, glint, seed}});
    };
    // The ghost (under the pieces).
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
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
                push(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), tint(p.type), 0.6f, 1, 0.f, 0.f, 0.f);
            }
        }
    }
    // The stack.
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            auto same = [&](int nx, int ny) {
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H) return false;
                if (std::fabs(g.rowOffset(ny) - off) > 0.02f) return false;
                if (g.cell(nx, ny).type != c.type) return false;
                if (ids_[y][x] && ids_[ny][nx]) return ids_[ny][nx] == ids_[y][x];
                return g.cell(nx, ny).type == c.type;
            };
            const vec3 p = layout::cellPos((float)x, y - off);
            push(p.x, p.y, 0.5f, maskOf(x, y, same), tint(c.type), 1.f, 0, 0.f, glint_[y][x], (float)(ids_[y][x] % 97));
        }
    // Tiles melting (a clear at the real speed; game over from the top down).
    for (const Melt& u : melt_) {
        const float k = saturate((u.t - u.delay) / (u.delay > 0.f ? 0.5f : 0.34f));
        push(u.x, u.y, 0.5f, u.mask, tint(u.type), 1.f, 3, k, 0.f, u.seed);
    }
    // The falling piece.
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            push(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), tint(p.type), 1.f, 0, 0.f, 0.1f, 7.f);
        }
    }
    // Water running over the tiles (a melted row).
    for (const Splash& s : splash_) {
        const float a = 1.f - smoothstepf(0.55f, 1.f, s.t / s.life);
        push(s.x, s.y, s.r, 0, vec3(0.f), a, 4, 0.f, 0.f, 0.f);
    }
    // Previews.
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
            push(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), tint(type), alpha, 0, 0.f, 0.f, 3.f + type);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float RainyUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
    }
    return w - track;
}

void RainyUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                              std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = size / f.px;
    const float baseline = y + f.capH * k;
    float pen = x;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) out.insert(out.end(), q, q + 4);
        }
        pen += g.adv * k + track;
    }
}

void RainyUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", haloA);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void RainyUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// A filled disc (pixels), with a one-pixel feather.
void RainyUniverse::disc(float cx, float cy, float r, const vec3& c, float a) {
    const int N = 16;
    for (int i = 0; i < N; i++) {
        const float a0 = TAU * i / N, a1 = TAU * (i + 1) / N;
        const float v[3][6] = {{cx, cy, c.x, c.y, c.z, a},
                               {cx + std::cos(a0) * r, cy + std::sin(a0) * r, c.x, c.y, c.z, a},
                               {cx + std::cos(a1) * r, cy + std::sin(a1) * r, c.x, c.y, c.z, a}};
        for (auto& q : v) shapes_.insert(shapes_.end(), q, q + 6);
    }
}

// The lettering pale on the night (a soft dark halo); the bonus gauge a ring of beads of water, those charged lit
// in the accent.
void RainyUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    const vec3 bg = lerp(sky(0), sky(1), 0.5f);
    const bool pale = ugl::luma(bg) > 0.3f;
    const vec3 ink = ugl::toSrgb(pale ? lin(0x1e2230) : lin(0xe8ecf3)), halo = ugl::toSrgb(pale ? lin(0xf0f2f6) : bg * 0.6f);
    const vec3 acc = ugl::toSrgb(accent());
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? acc : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                   {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : q) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }

    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 24;
        for (int i = 0; i < N; i++) {
            const bool on = (i + 0.5f) / N <= fill;
            const float a = TAU * (i + 0.5f) / N;
            const float px = c.x + std::sin(a) * r, py = c.y - std::cos(a) * r;
            const float al = (on ? 0.95f : (g.bonusReady() ? 0.7f : 0.38f)) * vis * (1.f - 0.5f * dimT_);
            const float br = on ? std::max(2.f, 4.2f * s) : std::max(1.2f, 2.2f * s);
            disc(px, py, br, on ? acc : ink, al);
            // a bead's highlight
            if (on) disc(px - br * 0.3f, py - br * 0.35f, br * 0.32f, vec3(1.f), al * 0.8f);
        }
        flushShapes();
    }

    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        auto upper = [&](std::string t) {
            for (char& ch : t) ch = (char)std::toupper((unsigned char)ch);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(fontReg_, upper(h.text), base * 0.74f, base * 0.2f, base * 0.1f);
            drawText(fontReg_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.85f);
            continue;
        case HUD_SECTION:
            place(fontReg_, upper(h.text), base * 0.82f, base * 0.24f, base * 0.02f);
            drawText(fontReg_, q, acc, halo, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(fontLight_, h.text, base * 1.05f, base * 0.02f, -base * 0.06f);
            drawText(fontLight_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontLight_, h.text, base * 0.8f, base * 0.06f);
            drawText(fontLight_, q, ink, halo, saturate(alpha * 1.2f) * 0.75f);
            continue;
        case HUD_LABEL:
            place(fontReg_, upper(h.text), base * 0.68f, base * 0.26f, base * 0.05f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_VALUE:
            place(fontLight_, h.text, base * 1.08f, base * 0.02f, -base * 0.06f);
            drawText(fontLight_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontLight_, h.text, base * 0.95f, base * 0.06f);
            drawText(fontLight_, q, acc, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontReg_, h.text, base * 0.85f, 0.f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontLight_, upper(h.text), base * 1.05f, base * 0.22f, -base * 0.08f);
            drawText(fontLight_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontLight_, h.text, base * 0.9f, base * 0.06f);
            drawText(fontLight_, q, acc, halo, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontLight_, h.text, base * 1.0f, base * 0.08f);
                drawText(fontLight_, q, ink, halo, alpha, 0.8f);
            } else {
                place(fontReg_, h.text, base * 0.82f, base * 0.04f);
                drawText(fontReg_, q, h.accent ? acc : ink, halo, alpha, 0.8f);
            }
            continue;
        default:
            place(fontReg_, h.text, base * 0.8f, 0.f);
            drawText(fontReg_, q, h.accent ? acc : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void RainyUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) {
            overT_ = -1.f;
            melt_.erase(std::remove_if(melt_.begin(), melt_.end(), [](const Melt& u) { return u.delay > 0.f; }), melt_.end());
        } else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? 1.f : 0.f, overT_ >= 0.f ? 0.8f : 0.5f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.28f, slow_);
    time_ += dt;
    worldTime_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    // the city's colors shift on the drop's downbeat (in a second) and come back slowly after the peak
    dev_ = approach(dev_, dropMode_ ? wp : 0.f, dropMode_ ? 2.2f : 0.25f, dt);
    swell_ = approach(swell_, 0.f, 0.45f, dt);
    for (auto& row : glint_)
        for (float& gl : row) gl = approach(gl, 0.f, 2.5f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);

    // ---- The beat grid: drops let go every two beats (every eight in a calm part), lights change on the bars.
    if (!f.paused && overT_ < 0.f) {
        bool two = false, bar = false;
        if (music.hasSong) {
            const double hb = std::floor(music.beat / 2.0 + 1e-4);
            if (lastHalf_ < 0.0 || hb < lastHalf_ - 8.0) lastHalf_ = hb;
            if (hb > lastHalf_) { lastHalf_ = hb; two = true; bar = std::fmod(hb, 2.0) < 0.5; }
        } else {
            beatClock_ += wdt;
            if (beatClock_ > 1.f) { beatClock_ = 0.f; two = true; lastHalf_ += 1.0; bar = std::fmod(lastHalf_, 2.0) < 0.5; }
        }
        if (two) {
            const long idx = (long)lastHalf_;
            int n;
            if (level_ == 0) n = idx % 4 == 0 ? 1 : 0;
            else if (level_ == 1) n = 1 + (int)(idx % 2);
            else n = dropMode_ ? 5 : 3;
            if (slowOn_) n = std::min(n, 1);
            releaseRuns(n, true);
        }
        if (bar && (level_ > 0 || std::fmod(lastHalf_, 4.0) < 0.5)) {
            // one light switches, keeping the share of lit windows of the section
            const float share = 0.6f + 0.25f * wm + 0.15f * wp;
            int lit = 0, n = 0;
            for (const Light& l : lights_)
                if (l.vx == 0.f) { n++; if (l.onTo > 0.5f) lit++; }
            const bool turnOn = lit < share * n;
            for (int tries = 0; tries < 12 && !lights_.empty(); tries++) {
                Light& l = lights_[(size_t)rng_.irange(0, (int)lights_.size() - 1)];
                if (l.vx != 0.f || (l.onTo > 0.5f) == turnOn) continue;
                l.onTo = turnOn ? 1.f : 0.f;
                break;
            }
        }
    }

    // ---- The water.
    stepWater(game, wdt);
    for (Melt& u : melt_) u.t += dt;
    melt_.erase(std::remove_if(melt_.begin(), melt_.end(),
                               [](const Melt& u) { return u.t > u.delay + (u.delay > 0.f ? 0.52f : 0.36f); }),
                melt_.end());
    for (Splash& s : splash_) {
        s.t += dt;
        s.vy += 16.f * dt;
        s.y -= s.vy * dt;
    }
    splash_.erase(std::remove_if(splash_.begin(), splash_.end(), [](const Splash& s) { return s.t > s.life; }), splash_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    if (!mist_.empty()) {
        for (size_t i = 0; i < mist_.size(); i++) mistTex_[i] = (unsigned char)(saturate(mist_[i]) * 255.f + 0.5f);
        glBindTexture(GL_TEXTURE_2D, mistTexId_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, mw_, mh_, 0, GL_RED, GL_UNSIGNED_BYTE, mistTex_.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    // ---- The city out of focus.
    const float seedU = (float)(seed_ % 1000) * 0.137f;
    float hudBoxes[16];
    for (int b = 0; b < 4; b++)
        for (int k = 0; k < 4; k++) hudBoxes[b * 4 + k] = hudBox(b, k);
    auto columns = [&](GLuint p) {
        glUniform4fv(ugl::U(p, "uHud"), 4, hudBoxes);
        set1f(p, "uSeed", seedU);
    };
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    cityT_.bind();
    glUseProgram(progSky_);
    columns(progSky_);
    set4f(progSky_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progSky_, "uTop", sky(0));
    set3f(progSky_, "uBottom", sky(1));
    set3f(progSky_, "uGlow", lerp(lamp(0), lamp(1), 0.4f));
    set1f(progSky_, "uGlowA", (0.05f + 0.025f * wm + 0.035f * wp) * (1.f - 0.5f * over_));
    ugl::drawFullscreen();

    const float bright = (0.62f + 0.25f * wm + 0.22f * wp) * (1.f + 0.35f * swell_) * (1.f - 0.55f * over_);
    bokeh_.clear();
    const float X = viewH_.x + 3.f;
    for (Light& l : lights_) {
        l.on = approach(l.on, l.onTo, 1.4f, dt);
        if (l.vx != 0.f) {
            l.x += l.vx * wdt * (0.6f + 0.6f * wm);
            if (l.x > X) l.x -= 2.f * X;
            if (l.x < -X) l.x += 2.f * X;
        }
        float k = l.base * l.on * bright;
        if (l.bass) k *= 1.f + 0.9f * kick * wp;
        k = std::min(k, 0.95f);
        if (k < 0.004f) continue;
        const vec3 c = lamp(l.pal);
        bokeh_.push_back({{l.x, l.y, l.r, k}, {c.x, c.y, c.z, l.soft}});
    }
    for (Lantern& l : lanterns_) {
        l.t += dt;
        l.y += 0.7f * dt;
        if (l.t <= 0.f) continue;
        const float k = std::sin(PI * saturate(l.t / l.life)) * 0.85f;
        bokeh_.push_back({{l.x, l.y, l.r * (1.f + 0.2f * l.t / l.life), k}, {l.col.x, l.col.y, l.col.z, 0.2f}});
    }
    lanterns_.erase(std::remove_if(lanterns_.begin(), lanterns_.end(), [](const Lantern& l) { return l.t > l.life; }),
                    lanterns_.end());
    if (!bokeh_.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(progBokeh_);
        columns(progBokeh_);
        setMat(progBokeh_, "uVP", vp_);
        glBindVertexArray(bokehVao_);
        glBindBuffer(GL_ARRAY_BUFFER, bokehVbo_);
        glBufferData(GL_ARRAY_BUFFER, bokeh_.size() * sizeof(BInst), bokeh_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)bokeh_.size());
        glDisable(GL_BLEND);
    }
    // the city through the mist: blurred at a quarter
    glUseProgram(progBlur_);
    set1i(progBlur_, "uTex", 0);
    glActiveTexture(GL_TEXTURE0);
    for (int pass = 0; pass < 2; pass++) {
        blurA_.bind();
        glBindTexture(GL_TEXTURE_2D, pass == 0 ? cityT_.tex : blurB_.tex);
        set2f(progBlur_, "uDir", 1.5f / blurA_.w, 0.f);
        ugl::drawFullscreen();
        blurB_.bind();
        glBindTexture(GL_TEXTURE_2D, blurA_.tex);
        set2f(progBlur_, "uDir", 0.f, 1.5f / blurA_.h);
        ugl::drawFullscreen();
    }

    // ---- The water's height field.
    buildWater(game);
    waterT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!water_.empty()) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_MAX);
        glUseProgram(progWater_);
        setMat(progWater_, "uVP", vp_);
        glBindVertexArray(waterVao_);
        glBindBuffer(GL_ARRAY_BUFFER, waterVbo_);
        glBufferData(GL_ARRAY_BUFFER, water_.size() * sizeof(WInst), water_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)water_.size());
        glBlendEquation(GL_FUNC_ADD);
        glDisable(GL_BLEND);
    }

    // ---- The pane.
    const VariantDef& V = VARIANTS[variant_];
    sceneT_.bind();
    glUseProgram(progGlass_);
    columns(progGlass_);
    set4f(progGlass_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progGlass_, "uRes", (float)width_, (float)height_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cityT_.tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, blurB_.tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, waterT_.tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, mistTexId_);
    set1i(progGlass_, "uCity", 0);
    set1i(progGlass_, "uBlur", 1);
    set1i(progGlass_, "uWater", 2);
    set1i(progGlass_, "uMist", 3);
    set3f(progGlass_, "uMistTint", lin(V.mist) * 0.35f);
    set3f(progGlass_, "uFrame", lin(V.skyTop) * 0.5f + vec3(0.004f));
    set3f(progGlass_, "uFrameRim", lamp(0) * 0.08f);
    set1f(progGlass_, "uMistA", 0.85f);
    set1f(progGlass_, "uLens", 0.9f);
    set1f(progGlass_, "uSpec", 0.5f + 0.2f * wp);
    ugl::drawFullscreen();

    // ---- The glass tiles, the ghost, the water over them.
    cells_.clear();
    buildBoard(game);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        columns(progCell_);
        setMat(progCell_, "uVP", vp_);
        set2f(progCell_, "uRes", (float)width_, (float)height_);
        set1f(progCell_, "uPxCell", 2.f * viewH_.y / height_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cityT_.tex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, blurB_.tex);
        set1i(progCell_, "uCity", 0);
        set1i(progCell_, "uBlur", 1);
        set3f(progCell_, "uLead", lin(V.skyTop) * 0.4f + vec3(0.006f));
        set1f(progCell_, "uBack", 0.6f * (1.f - 0.35f * over_));
        set1f(progCell_, "uGain", 1.6f);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }

    // ---- The frame.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_BLEND);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    set1f(progFinal_, "uSlow", slow_ * 0.8f);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uSwell", swell_);
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
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

const UniverseEntry& universe_rainy() {
    static const UniverseEntry e{
        "RAINY WINDOW", "a night window pane in the rain, the city out of focus behind it, the pieces stained glass on the glass",
        [] { return std::unique_ptr<Universe>(new RainyUniverse()); },
        // Quiet, melancholic and mid-tempo songs suit the rain best; it fits most songs.
        [](const SongLook& l) {
            return 0.75f + 0.2f * (1.f - l.brightness) + (l.minor ? 0.1f : 0.f) + 0.1f * (1.f - std::fabs(l.bpm - 110.f) / 60.f);
        },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
