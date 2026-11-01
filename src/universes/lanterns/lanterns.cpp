// LANTERNS: paper lanterns floating on a dark lake, the pieces lit from inside.
//
// One world: a calm lake at night, a far shore of low hills, a few faint stars. The well is two bamboo poles standing
// in the water, and the stack floats on the lake: its bottom row sits on the waterline and its lowest rows are
// mirrored under it, rippled. The pieces are paper lanterns: every piece is one panel of lit paper (darker toward its
// thin wooden frame, ribs and fibers seen through the paper, its candle burning at the piece's heart), each of the
// seven types its own paper. Clears release lanterns: a cleared row goes out (its cells' light fades and the paper
// folds away in a third of a second, nothing drawn over the row) and two small box lanterns, one for each half of
// the row in its main paper, light up on the water at the inner edge of the outer margins (clear of the HUD columns
// and the corner lettering) and drift to a free place on the lake, where they stay lit for the whole song, smaller
// and smaller toward the far shore, each with its streak of light in the water. So the lake fills up with the play.
// The drop lifts every lantern off the water into the sky, slowly, and sends more up from the whole lake for as long
// as it lasts: a sky full of lights; after it they go out as they rise away, and new ones gather again.
//
// The song is the water: a calm part is a mirror (still reflections, more stars); mid parts ripple the reflections on
// the beat grid (every two beats) and light far lanterns along the opposite shore; a peak ripples harder, lights more
// of them, and its kicks make candles flicker here and there. A lock is the piece's candle catching (a brief warm
// rise, on that piece only); a hard drop pushes a few embers out from under it. A Tetris sends a dozen lanterns up
// from the lake. Slow motion is moonlight (a full moon and its path on the water) and mist lying on the lake; game
// over puts the candles out one by one from the top. A new song lifts the old song's lanterns away.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/lanterns/lanterns.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/lanterns/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Josefin Sans (labels, numbers) and Cormorant Garamond Italic (the song, the section, the celebrations), SIL Open
// Font License, see OFL-JosefinSans.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_REG[] = {
#include "universes/lanterns/JosefinSans-Regular.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/lanterns/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = lanterns_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

constexpr float HORIZON = 3.0f; // the far shore (board units)
constexpr float WATER = -10.0f; // the waterline: the well's floor
constexpr int MAX_LAKE = 90;    // lanterns floating at once

// The nights: the sky (top, horizon), the near water, the far shore, the poles, the seven papers, the accent, and
// the drop's sky (its top, the glow the lanterns throw on the horizon).
struct VariantDef {
    const char* name;
    unsigned skyTop, skyHor, lake, shore, pole;
    unsigned paper[7]; // I O T S Z J L
    unsigned accent, dropTop, dropGlow;
};
const VariantDef VARIANTS[] = {
    // a deep blue night, the classic papers
    {"MIDNIGHT", 0x060a17, 0x18223d, 0x03050c, 0x070a14, 0x3a3a40,
     {0xffb347, 0xfff0d2, 0xb68cff, 0x7fe0a0, 0xff5a3c, 0x6fa8ff, 0xff8fb8}, 0xffc46b, 0x150f2c, 0xff8a3d},
    // a teal night over a lotus pond, pastel papers
    {"LOTUS POND", 0x041113, 0x0f3436, 0x02090a, 0x041012, 0x34403e,
     {0xffc39a, 0xfff6e0, 0xd8a6ff, 0xa6f0c0, 0xff7c8c, 0x8fcfff, 0xffe27a}, 0xff9fb0, 0x16122a, 0xff9a86},
    // a moonless night, black water, jewel papers
    {"MOONLESS", 0x040405, 0x131418, 0x020203, 0x060607, 0x2f2f33,
     {0xffa01a, 0xffe14d, 0xc24dff, 0x3ddc84, 0xff2d55, 0x3f7fff, 0xff6ec7}, 0xffe14d, 0x1a0b0c, 0xff6a1a},
    // dusk: a plum sky, the last light low on the water, warm papers
    {"DUSK", 0x0d0a21, 0x3a1e3c, 0x070519, 0x0b0819, 0x45384a,
     {0xffb070, 0xffeec8, 0xa6a0ff, 0x9fe8b8, 0xff6a5a, 0x7cc4ff, 0xffc24d}, 0xffb070, 0x2a1438, 0xffaa4a},
    // a winter night, ice papers and one warm one
    {"FROST", 0x060b12, 0x22303f, 0x04070c, 0x080c13, 0x48505c,
     {0x86d4ff, 0xf4f8ff, 0xc8b6ff, 0x96eed8, 0xff9a8a, 0x6c98ff, 0xffd58a}, 0xbfe6ff, 0x101a30, 0xffc890},
    // a festival on the river: indigo night, red and gold papers
    {"FESTIVAL", 0x09081f, 0x281a46, 0x05041a, 0x08061a, 0x463a52,
     {0xff3b30, 0xffd04a, 0xff7ab0, 0x5fe0a0, 0xff9a2a, 0x5aa0ff, 0xfff0c8}, 0xffd04a, 0x2a1030, 0xff6040},
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

// Mixes two display colors by an amount in display space (a blend toward a dark color stays rich, not grey).
vec3 mixDisplay(const vec3& a, const vec3& b, float t) { return ugl::toLinear(lerp(ugl::toSrgb(a), ugl::toSrgb(b), t)); }

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool LanternsUniverse::init(int width, int height) {
    using shaders::withCommon;
    progLake_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::LAKE_FS).c_str(), "lanterns lake");
    progCell_ = ugl::program(shaders::CELL_VS, withCommon(shaders::CELL_FS).c_str(), "lanterns cells");
    progSprite_ = ugl::program(shaders::SPRITE_VS, shaders::SPRITE_FS, "lanterns sprites");
    progBright_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BRIGHT_FS, "lanterns bright");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "lanterns blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "lanterns final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "lanterns shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "lanterns text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, size_t stride, int attribs) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < attribs; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(cellVao_, cellVbo_, sizeof(CellInst), 4);
    instanced(spriteVao_, spriteVbo_, sizeof(SpriteInst), 3);
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

    buildFont(fontReg_, FONT_REG);
    buildFont(fontIt_, FONT_IT);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void LanternsUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] lanterns font init failed\n");
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

void LanternsUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomA_.create(std::max(1, width_ / 4), std::max(1, height_ / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(std::max(1, width_ / 4), std::max(1, height_ / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
}

LanternsUniverse::~LanternsUniverse() {
    sceneT_.destroy();
    bloomA_.destroy();
    bloomB_.destroy();
    for (GLuint p : {progLake_, progCell_, progSprite_, progBright_, progBlur_, progFinal_, progShape_, progText_}) glDeleteProgram(p);
    for (GLuint v : {cellVao_, spriteVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, spriteVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontReg_, &fontIt_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 LanternsUniverse::tint(int i) const { return lin(VARIANTS[variant_].paper[std::clamp(i, 0, 6)]); }

vec3 LanternsUniverse::skyHor() const {
    const VariantDef& V = VARIANTS[variant_];
    return lin(V.skyHor);
}

vec3 LanternsUniverse::skyTop() const {
    const VariantDef& V = VARIANTS[variant_];
    return mixDisplay(lin(V.skyTop), lin(V.dropTop), smoothstepf(0.f, 1.f, dev_));
}

void LanternsUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.4f) v = h < 0.5f ? 2 : 4;       // moonless, frost
        else if (t.hasSong && t.brightness > 0.62f) v = h < 0.5f ? 5 : 1;            // festival, lotus
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x1A7ull);
    if (seconds <= 0.1f) {
        lanterns_.clear();
    } else {
        // a new song: the old song's lanterns are lifted away
        liftAll(4.f);
    }
}

void LanternsUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

// The drop: every lantern on the lake lifts into the sky, more rise from the whole lake for as long as it lasts.
void LanternsUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    liftAll(2.5f);
    for (int i = 0; i < 6; i++) spawnSky(i % 2 ? 1 : -1, i >= 2);
}

// A new section: a few far lanterns go up.
void LanternsUniverse::refresh(float strength) {
    if (strength > 0.3f && lv_ > 0.5f) spawnSky(rng_.chance(0.5f) ? 1 : -1, true);
}

void LanternsUniverse::levelUp() {
    for (int i = 0; i < 4; i++) spawnSky(i % 2 ? 1 : -1, false);
}

vec2 LanternsUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints LanternsUniverse::hints() const {
    TransitionHints h;
    h.background = mixDisplay(skyTop(), skyHor(), 0.4f);
    h.light = false;
    h.ink = lin(0xf3e6c8);
    h.accent = lin(VARIANTS[variant_].accent);
    h.styles = TS_RIPPLE | TS_LUMA | TS_INK;
    return h;
}

// =====================================================================================================
// The lanterns
// =====================================================================================================

// The perspective of the lake: 0 at the far shore .. 1 at the near edge of the view.
float LanternsUniverse::lakeScale(float y) const {
    const float yN = viewC_.y - viewH_.y;
    return std::clamp((HORIZON - y) / (HORIZON - yN), 0.03f, 1.1f);
}

// A free place on the lake on one side (-1 left, 1 right), clear of the HUD columns: the best of a few candidates
// (the farthest from the lanterns already there, by the lake's perspective).
bool LanternsUniverse::pickTarget(int side, float& bx, float& by) {
    const float yN = viewC_.y - viewH_.y;
    const float xmax = viewC_.x + viewH_.x - 0.6f;
    const float colBottom = side < 0 ? layout::LEFT_COLUMN[1] : layout::RIGHT_COLUMN[1];
    float best = -1.f;
    for (int k = 0; k < 28; k++) {
        const float s = rng_.range(0.07f, 0.9f);
        const float y = HORIZON - s * (HORIZON - yN);
        const float w = 0.8f * s;
        // the left side keeps clear of the score column and of the universe's name in the corner below it
        if (side < 0 && y < -6.6f) continue;
        (void)colBottom;
        const float xmin = 11.9f;
        if (xmin + 0.6f * w > xmax - 0.6f * w) continue;
        const float x = side * rng_.range(xmin + 0.6f * w, xmax - 0.6f * w);
        float dmin = 1e9f;
        for (const Lantern& L : lanterns_) {
            if (L.sky || L.state == L_RISE || L.dying) continue;
            const float ls = 0.5f * (s + lakeScale(L.ty));
            const float dx = (x - L.tx) / ls, dy = (y - L.ty) / (ls * 0.55f);
            dmin = std::min(dmin, dx * dx + dy * dy);
        }
        if (dmin > best) { best = dmin; bx = x; by = y; }
    }
    return best >= 0.f;
}

// A lantern released by a clear: it lights up on the water at the inner edge of the outer margin (just outside the
// HUD columns and the corner lettering) and drifts away to its place.
void LanternsUniverse::release(int type, int side, float delay, float jitter) {
    Lantern L;
    L.type = type;
    L.x0 = side * 12.5f;
    L.y0 = (side < 0 ? -6.0f : -9.6f) - jitter;
    L.x = L.x0;
    L.y = L.y0;
    if (!pickTarget(side, L.tx, L.ty)) return;
    L.travel = 4.f + 0.25f * std::hypot(L.tx - L.x0, L.ty - L.y0) + rng_.range(0.f, 1.f);
    L.delay = delay;
    L.phase = rng_.range(0.f, TAU);
    L.seed = rng_.uniform();
    L.state = L_TRAVEL;
    lanterns_.push_back(L);
    // too many on the lake: the oldest one floating goes out
    int floating = 0;
    for (const Lantern& q : lanterns_)
        if (!q.sky && q.state != L_RISE && !q.dying) floating++;
    if (floating > MAX_LAKE)
        for (Lantern& q : lanterns_)
            if (!q.sky && q.state == L_FLOAT && !q.dying) { q.dying = true; break; }
}

// A sky lantern going up from the lake: from far away (small, over the far shore) or near (from the water).
void LanternsUniverse::spawnSky(int side, bool far) {
    const float yN = viewC_.y - viewH_.y;
    const float xmax = viewC_.x + viewH_.x - 0.6f;
    Lantern L;
    L.sky = true;
    L.state = L_RISE;
    L.type = rng_.irange(0, 6);
    L.phase = rng_.range(0.f, TAU);
    L.seed = rng_.uniform();
    if (far) {
        L.w = rng_.range(0.13f, 0.28f);
        L.y = HORIZON + rng_.range(-0.2f, 0.6f);
        L.wy = L.y;
        L.vy = 0.f;
    } else {
        // (on the left, clear of the universe's name in the corner)
        const float smax = side < 0 ? std::min(0.85f, (HORIZON + 6.6f) / (HORIZON - yN)) : 0.85f;
        const float s = rng_.range(0.35f, smax);
        L.w = 0.8f * s;
        L.y = HORIZON - s * (HORIZON - yN);
        L.wy = L.y;
    }
    const float xmin = 12.0f + 0.6f * L.w;
    if (xmin > xmax) return;
    L.x = side * rng_.range(xmin, xmax - 0.4f * L.w);
    L.alpha = 0.f;
    lanterns_.push_back(L);
}

// Every lantern on the lake lifts off the water, each in its own time within `spread` seconds.
void LanternsUniverse::liftAll(float spread) {
    for (Lantern& L : lanterns_) {
        if (L.sky || L.state == L_RISE) continue;
        // still on its way (perhaps under the HUD column): it lifts once there
        if (L.state == L_TRAVEL) { L.pendingLift = true; continue; }
        liftOne(L, spread);
    }
}

void LanternsUniverse::liftOne(Lantern& L, float spread) {
    const float s = lakeScale(L.y);
    L.state = L_RISE;
    L.w = 0.8f * s;
    L.wy = L.y;
    L.vy = 0.f;
    L.rise = 0.f;
    L.delay = rng_.range(0.f, spread) * (0.4f + 0.6f * (1.f - s)) + rng_.range(0.f, 0.3f);
}

void LanternsUniverse::updateLanterns(float dt, float wdt) {
    const float top = viewC_.y + viewH_.y + 1.5f;
    for (Lantern& L : lanterns_) {
        L.flick = approach(L.flick, 0.f, 2.5f, dt);
        if (L.dying) L.fade = std::min(1.f, L.fade + dt / 2.f);
        // out of a drop, the lanterns in the sky go out as they rise away
        else if (!dropMode_ && L.state == L_RISE && L.delay <= 0.f) L.fade = std::min(1.f, L.fade + wdt / 10.f);
        if (L.state == L_TRAVEL) {
            L.t += wdt;
            const float u = saturate((L.t - L.delay) / L.travel);
            const float k = 1.f - (1.f - u) * (1.f - u);
            L.x = L.x0 + (L.tx - L.x0) * (1.f - (1.f - k) * (1.f - k));
            L.y = L.y0 + (L.ty - L.y0) * k * k * k;
            if (u >= 1.f) {
                L.state = L_FLOAT;
                if (L.pendingLift || dropMode_) liftOne(L, 1.f);
            }
        } else if (L.state == L_FLOAT) {
            L.t += wdt;
            const float s = lakeScale(L.ty);
            L.x = L.tx + std::sin(worldTime_ * 0.05f + L.phase) * 0.25f * s;
            L.y = L.ty + std::sin(worldTime_ * 0.04f + L.phase * 1.7f) * 0.05f * s;
        } else {
            if (L.delay > 0.f) { L.delay -= wdt; continue; }
            L.rise += wdt;
            L.alpha = std::min(1.f, L.alpha + wdt / 1.2f);
            const float s = L.sky ? L.w : lakeScale(L.wy);
            // after the drop the last ones hurry away, leaving the sky to the next ones
            const float speed = (L.sky ? 0.3f + 0.75f * L.w : 0.35f + 0.55f * s) * (dropMode_ ? 1.f : 1.8f);
            L.vy = lerpf(L.vy, speed, 1.f - std::exp(-wdt * 0.5f));
            L.y += L.vy * wdt;
            L.x += std::sin(L.rise * 0.45f + L.phase) * 0.09f * wdt;
            L.w *= std::exp(-0.02f * wdt);
        }
    }
    lanterns_.erase(std::remove_if(lanterns_.begin(), lanterns_.end(),
                                   [&](const Lantern& L) {
                                       if (L.fade >= 1.f) return true;
                                       if (L.state == L_RISE && L.y - L.w > top) return true;
                                       return false;
                                   }),
                    lanterns_.end());
    for (Ember& e : embers_) {
        e.t += dt;
        e.x += e.vx * dt;
        e.y += e.vy * dt;
        e.vx *= std::exp(-2.5f * dt);
        e.vy = e.vy * std::exp(-2.f * dt) + 0.25f * dt;
    }
    embers_.erase(std::remove_if(embers_.begin(), embers_.end(), [](const Ember& e) { return e.t > e.life; }), embers_.end());
}

// The lake's lanterns, their reflections, the sky lanterns, the far lights, the embers.
void LanternsUniverse::buildSprites(float ripple) {
    sprites_.clear();
    glows_.clear();
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const float light = (0.9f + 0.08f * wm + 0.14f * wp + 0.12f * swell_) * (1.f - 0.25f * slow_);
    const float yN = viewC_.y - viewH_.y;
    const float xmax = viewC_.x + viewH_.x - 0.6f;
    auto glow = [&](float x, float y, float w, int kind, const vec3& c, float a, float l, float rip, float seed, float len) {
        if (a <= 0.003f) return;
        glows_.push_back({{x, y, w, (float)kind}, {c.x, c.y, c.z, a}, {l, rip, seed, len}});
    };
    auto body = [&](float x, float y, float w, int kind, const vec3& c, float a, float l, float seed) {
        if (a <= 0.003f) return;
        sprites_.push_back({{x, y, w, (float)kind}, {c.x, c.y, c.z, a}, {l, 0.f, seed, 0.f}});
    };
    // far lanterns along the opposite shore: more of them as the song rises
    {
        const float amount = 0.5f * wm + 0.5f * wp + 0.25f * dev_;
        Rng r(seed_ ^ 0xFA11ull);
        for (int i = 0; i < 28; i++) {
            const int side = i % 2 ? 1 : -1;
            const float x = side * r.range(12.2f, std::max(12.3f, xmax));
            const float y = HORIZON + r.range(0.02f, 0.14f);
            const float th = r.uniform() * 0.95f;
            const int type = r.irange(0, 6);
            const float ph = r.range(0.f, TAU);
            const float a = saturate((amount - th) * 5.f);
            if (a <= 0.f) continue;
            const float tw = 0.85f + 0.15f * std::sin(worldTime_ * (0.4f + 0.3f * r.uniform()) + ph);
            glow(x, y, 0.32f, 2, tint(type), a * 0.8f, light * tw, 0.f, 0.f, 0.f);
            glow(x, y - 0.03f, 0.1f, 1, tint(type), a * 0.5f, light, ripple * 0.5f, ph, 4.f);
        }
    }
    // the lanterns, far ones first
    std::vector<int> order(lanterns_.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const Lantern& A = lanterns_[a];
        const Lantern& B = lanterns_[b];
        const float da = A.state == L_RISE ? A.w : lakeScale(A.y), db = B.state == L_RISE ? B.w : lakeScale(B.y);
        return da < db;
    });
    // the song's title in the upper left corner: the lanterns rising behind it dim
    auto titleClear = [&](float x, float y) {
        const float d = std::min(-12.f - x, y - 8.4f);
        return 1.f - 0.65f * smoothstepf(-0.5f, 0.8f, d);
    };
    for (int idx : order) {
        const Lantern& L = lanterns_[idx];
        const vec3 c = tint(L.type);
        const float fl = light * (1.f + 0.35f * L.flick) * (1.f - L.fade);
        if (L.state == L_TRAVEL || L.state == L_FLOAT) {
            if (L.t < L.delay) continue;
            const float age = L.t - L.delay;
            const float grow = smoothstepf(0.f, 0.7f, age);
            const float s = lakeScale(L.y);
            const float w = 0.8f * s * (0.3f + 0.7f * grow);
            // the beat grid's ripple lifts it a little
            const float bob = pulse_ * 0.05f * s * std::sin(worldTime_ * 2.f + L.phase * 3.f) * wm;
            const float a = grow * (1.f - L.fade);
            glow(L.x, L.y + bob, w, 1, c, a, fl, ripple, L.seed, 2.4f);
            body(L.x, L.y + bob, w, 0, c, a, fl, L.seed);
        } else {
            const float a = L.alpha * (1.f - L.fade);
            if (L.sky) {
                // its reflection: on the water it left (near), mirrored over the far shore (far)
                const float h = L.y - L.wy;
                if (L.wy < HORIZON - 0.3f) glow(L.x, L.wy, L.w, 1, c, a * std::exp(-h / (L.w * 2.f)), fl, ripple, L.seed, 2.4f);
                else {
                    const float ry = 2.f * HORIZON - L.y;
                    if (ry > yN) glow(L.x, ry, L.w * 0.8f, 1, c, a * 0.35f, fl, ripple, L.seed, 1.5f);
                }
                body(L.x, L.y, L.w, 3, c, a * titleClear(L.x, L.y), fl, L.seed);
            } else {
                const float h = L.y - L.wy;
                glow(L.x, L.wy, L.w, 1, c, (1.f - L.fade) * std::exp(-h / (L.w * 1.5f + 0.05f)), fl, ripple, L.seed, 2.4f);
                body(L.x, L.y, L.w, 0, c, (1.f - L.fade) * titleClear(L.x, L.y), fl, L.seed);
            }
        }
    }
    for (const Ember& e : embers_) {
        const float k = e.t / e.life;
        glow(e.x, e.y, 0.16f * (1.f - 0.5f * k), 2, tint(e.type), (1.f - k) * (1.f - k), 1.4f, 0.f, 0.f, 0.f);
    }
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the pieces' ids in step with the board (a new board, or one changed behind our back, gets fresh ids).
void LanternsUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) { ids_[y][x] = 0; flare_[y][x] = 0.f; }
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

void LanternsUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            flare_[c.y][c.x] = 0.3f; // the candle catching
        }
        break;
    }
    case GameEvent::HardDrop: {
        // a few embers pushed out from under the piece, at the contact
        int bottom = -1;
        float sx = 0.f;
        int n = 0;
        for (auto& c : ev.cells) bottom = std::max(bottom, c.y);
        for (auto& c : ev.cells)
            if (c.y == bottom) { sx += c.x; n++; }
        if (n == 0 || bottom < Game::HIDDEN) break;
        const float cx = sx / n;
        const vec3 p = layout::cellPos(cx, (float)bottom);
        const int type = ev.cells.empty() ? 0 : ev.cells[0].type;
        const float hw = 0.5f * n;
        for (int i = 0; i < 7; i++) {
            const float side = i % 2 ? 1.f : -1.f;
            Ember e;
            e.x = p.x + side * rng_.range(0.f, hw);
            e.y = p.y - 0.45f;
            e.vx = side * rng_.range(0.8f, 2.2f);
            e.vy = rng_.range(0.1f, 0.6f);
            e.t = 0.f;
            e.life = rng_.range(0.45f, 0.8f);
            e.type = type;
            embers_.push_back(e);
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // the candles' centers of the pieces in the cleared rows
        struct Ctr { int id; float sx, sy; int n; };
        std::vector<Ctr> ctr;
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const int id = ids_[y][x];
                if (!id) continue;
                const vec3 p = layout::cellPos((float)x, (float)y);
                bool found = false;
                for (Ctr& c : ctr)
                    if (c.id == id) { c.sx += p.x; c.sy += p.y; c.n++; found = true; break; }
                if (!found) ctr.push_back({id, p.x, p.y, 1});
            }
        auto centerOf = [&](int id, float& cx, float& cy, float& r) {
            for (const Ctr& c : ctr)
                if (c.id == id) { cx = c.sx / c.n; cy = c.sy / c.n; r = 0.75f + 0.18f * c.n; return; }
            r = 0.9f;
        };
        // the cleared cells go out
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
                if (c.y < Game::H - 1 && rows[c.y + 1] && ids_[c.y + 1][c.x] == id) m |= 4;
                if (c.y > 0 && rows[c.y - 1] && ids_[c.y - 1][c.x] == id) m |= 8;
            }
            float cx = p.x, cy = p.y, r = 0.9f;
            if (id) centerOf(id, cx, cy, r);
            fades_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f, cx, cy, r, (float)(id % 97)});
        }
        // the lanterns they release: two a row, one to each side, in the paper most of that half was made of
        {
            float delayL = 0.f, delayR = 0.f;
            for (int y = Game::H - 1; y >= Game::HIDDEN; y--) {
                if (!rows[y]) continue;
                for (int side = -1; side <= 1; side += 2) {
                    int count[7] = {};
                    for (auto& c : ev.cells)
                        if (c.y == y && (c.x < 5) == (side < 0)) count[std::clamp(c.type, 0, 6)]++;
                    int best = 0;
                    for (int i = 1; i < 7; i++)
                        if (count[i] > count[best]) best = i;
                    float& dl = side < 0 ? delayL : delayR;
                    release(best, side, dl, rng_.range(0.f, 0.6f));
                    dl += 0.22f;
                }
            }
        }
        {
            int nb[Game::H][Game::W] = {};
            float nf[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(nf[dst], flare_[y], sizeof(flare_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(flare_, nf, sizeof(flare_));
        }
        // a Tetris: a dozen lanterns go up from the lake beside the well
        if (ev.count >= 4)
            for (int i = 0; i < 12; i++) spawnSky(i % 2 ? 1 : -1, i % 3 == 2);
        break;
    }
    case GameEvent::TopOut: {
        // the stack stays on the water, its candles going out from the top down
        overT_ = 0.f;
        overCells_.clear();
        auto idAt = [&](int x, int y) { return (x < 0 || y < 0 || x >= Game::W || y >= Game::H) ? -1 : ids_[y][x]; };
        auto inEv = [&](int x, int y) {
            for (auto& c : ev.cells)
                if (c.x == x && c.y == y) return true;
            return false;
        };
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int i = 0; i < 4; i++) {
                const int nx = c.x + dx[i], ny = c.y + dy[i];
                if (id && idAt(nx, ny) == id && inEv(nx, ny)) m |= 1 << i;
            }
            float sx = 0.f, sy = 0.f;
            int n = 0;
            for (auto& o : ev.cells)
                if (id && o.y >= 0 && o.y < Game::H && o.x >= 0 && o.x < Game::W && ids_[o.y][o.x] == id) {
                    const vec3 q = layout::cellPos((float)o.x, (float)o.y);
                    sx += q.x; sy += q.y; n++;
                }
            OverCell oc;
            oc.x = p.x; oc.y = p.y; oc.row = c.y; oc.type = std::clamp(c.type, 0, 6); oc.mask = m;
            oc.cx = n ? sx / n : p.x; oc.cy = n ? sy / n : p.y; oc.r = 0.75f + 0.18f * std::max(n, 1);
            oc.seed = (float)(id % 97);
            overCells_.push_back(oc);
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void LanternsUniverse::buildBoard(const Game& g) {
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const float light = (0.92f + 0.05f * wm + 0.06f * wp + 0.05f * swell_) * (1.f - 0.1f * slow_);
    const float rip = (0.1f + 0.5f * wm + 0.4f * wp + 0.5f * pulse_ * (0.3f + 0.7f * wm)) * (1.f - 0.85f * slow_);
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, 1, -1};
        int m = 0;
        for (int i = 0; i < 4; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    auto push = [&](float x, float y, float half, int mask, const vec3& c, float alpha, int kind, float prog, float l,
                    float seed, float cx, float cy, float r) {
        if (alpha <= 0.003f) return;
        cells_.push_back({{x, y, half, (float)mask}, {c.x, c.y, c.z, alpha}, {(float)kind, prog, l, seed}, {cx, cy, r, rip}});
    };
    auto reflect = [&](float x, float y, int mask, const vec3& c, float alpha, float l, float seed, float cx, float cy, float r) {
        if (y > WATER + 2.8f) return;
        push(x, y, 0.5f, mask, c, alpha, 3, 0.f, l, seed, cx, cy, r);
    };
    // the candles' centers of the stack's pieces
    struct Ctr { int id; float sx, sy; int n; };
    std::vector<Ctr> ctr;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const int id = ids_[y][x];
            if (!id || g.cell(x, y).type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            bool found = false;
            for (Ctr& c : ctr)
                if (c.id == id) { c.sx += p.x; c.sy += p.y; c.n++; found = true; break; }
            if (!found) ctr.push_back({id, p.x, p.y, 1});
        }
    // the candles going out at game over, from the top down
    auto litOf = [&](int y) {
        if (overT_ < 0.f) return 1.f;
        const float t0 = 0.6f + 0.11f * (y - Game::HIDDEN);
        return 1.f - smoothstepf(t0, t0 + 0.5f, overT_);
    };

    // The stack at game over, its candles going out.
    if (overT_ >= 0.f)
        for (const OverCell& o : overCells_) {
            const float l = light * litOf(o.row);
            push(o.x, o.y, 0.5f, o.mask, tint(o.type), 1.f, 0, 0.f, l, o.seed, o.cx, o.cy, o.r);
            reflect(o.x, o.y, o.mask, tint(o.type), 1.f, l, o.seed, o.cx, o.cy, o.r);
        }
    // The ghost.
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
                push(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), tint(p.type), 0.8f, 1, 0.f, 1.f, 0.f, w.x, w.y, 1.f);
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
                if (ids_[y][x] && ids_[ny][nx]) return ids_[ny][nx] == ids_[y][x];
                return g.cell(nx, ny).type == c.type;
            };
            const vec3 p = layout::cellPos((float)x, y - off);
            float cx = p.x, cy = p.y, r = 0.9f;
            for (const Ctr& q : ctr)
                if (q.id == ids_[y][x]) { cx = q.sx / q.n; cy = q.sy / q.n; r = 0.75f + 0.18f * q.n; break; }
            if (overT_ >= 0.f && !overCells_.empty()) continue;
            const float l = light * (1.f + flare_[y][x]) * litOf(y);
            const int m = maskOf(x, y, same);
            const float seed = (float)(ids_[y][x] % 97);
            push(p.x, p.y, 0.5f, m, tint(c.type), 1.f, 0, 0.f, l, seed, cx, cy, r);
            reflect(p.x, p.y, m, tint(c.type), 1.f, l, seed, cx, cy, r);
        }
    // Cells going out (a clear, at the real speed).
    for (const Fade& u : fades_) {
        const float k = saturate(u.t / 0.34f);
        push(u.x, u.y, 0.5f, u.mask, tint(u.type), 1.f, 2, k, light, u.seed, u.cx, u.cy, u.r);
        reflect(u.x, u.y, u.mask, tint(u.type), 1.f - k, light * (1.f - k), u.seed, u.cx, u.cy, u.r);
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
        float cx = 0.f, cy = 0.f;
        for (auto& c : cells) {
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cx += w.x * 0.25f;
            cy += w.y * 0.25f;
        }
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            const int m = maskOf(c[0], c[1], inPiece);
            push(w.x, w.y, 0.5f, m, tint(p.type), 1.f, 0, 0.f, light * 1.05f, 7.f, cx, cy, 1.5f);
            reflect(w.x, w.y, m, tint(p.type), 1.f, light, 7.f, cx, cy, 1.5f);
        }
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
            push(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), tint(type), alpha, 0, 0.f, light, 3.f, center.x, center.y, 1.4f * sc);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float LanternsUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void LanternsUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void LanternsUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", 0.45f);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void LanternsUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The lettering in warm paper light over the night (dark ink if the night ever got pale behind it); the bonus gauge a
// thin ring of rice paper whose charge glows in the accent.
void LanternsUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    // the backdrop behind the HUD columns: the sky between the horizon and the top, as the shader draws it
    const vec3 bg = mixDisplay(skyHor(), skyTop(), 0.45f) + lin(VARIANTS[variant_].dropGlow) * (0.25f * dev_ + 0.2f * swell_) * 0.15f;
    const bool pale = ugl::luma(bg) > 0.3f;
    const vec3 ink = ugl::toSrgb(pale ? lin(0x1d1a22) : lin(0xf3e6c8)), halo = ugl::toSrgb(pale ? lin(0xf3e6c8) : lin(0x05060b));
    const vec3 accent = ugl::toSrgb(pale ? lin(0x7a3a10) : lin(VARIANTS[variant_].accent));
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
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
        const int N = 72;
        const float th = std::max(1.f, 1.3f * s), th2 = std::max(2.f, 3.4f * s);
        for (int i = 0; i < N; i++) {
            const bool on = (i + 0.5f) / N <= fill;
            const float a0 = TAU * i / N, a1 = TAU * (i + 1.02f) / N;
            const float w = on ? th2 : th;
            const vec3 col = on ? accent : ink;
            const float al = (on ? 0.95f : (g.bonusReady() ? 0.6f : 0.3f)) * vis * (1.f - 0.5f * dimT_);
            const float s0 = std::sin(a0), c0 = std::cos(a0), s1 = std::sin(a1), c1 = std::cos(a1);
            const float ri = r - w * 0.5f, ro = r + w * 0.5f;
            const float pts[4][2] = {{c.x + s0 * ri, c.y - c0 * ri}, {c.x + s0 * ro, c.y - c0 * ro},
                                     {c.x + s1 * ro, c.y - c1 * ro}, {c.x + s1 * ri, c.y - c1 * ri}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
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
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(fontReg_, upper(h.text), base * 0.74f, base * 0.2f, base * 0.1f);
            drawText(fontReg_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f));
            continue;
        case HUD_SECTION:
            place(fontIt_, h.text, base * 1.05f, base * 0.04f, -base * 0.1f);
            drawText(fontIt_, q, accent, halo, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(fontIt_, h.text, base * 1.12f, 0.f, -base * 0.12f);
            drawText(fontIt_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontReg_, h.text, base * 0.7f, base * 0.08f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_LABEL:
            place(fontReg_, upper(h.text), base * 0.66f, base * 0.26f, base * 0.06f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_VALUE:
            place(fontReg_, h.text, base * 0.98f, base * 0.04f, 0.f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontIt_, h.text, base * 1.05f, base * 0.02f, -base * 0.08f);
            drawText(fontIt_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontReg_, h.text, base * 0.85f, 0.f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontIt_, h.text, base * 1.2f, base * 0.08f, -base * 0.15f);
            drawText(fontIt_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontIt_, h.text, base * 0.98f, base * 0.03f, -base * 0.05f);
            drawText(fontIt_, q, accent, halo, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontIt_, h.text, base * 1.12f, base * 0.04f, -base * 0.1f);
                drawText(fontIt_, q, ink, halo, alpha);
            } else {
                place(fontReg_, h.text, base * 0.8f, base * 0.04f);
                drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            }
            continue;
        default:
            place(fontReg_, h.text, base * 0.78f, base * 0.02f);
            drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void LanternsUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) { overT_ = -1.f; overCells_.clear(); }
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += dt;
    worldTime_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    dev_ = approach(dev_, dropMode_ ? wp : 0.f, dropMode_ ? 1.2f : 0.25f, dt);
    swell_ = approach(swell_, 0.f, 0.45f, dt);
    pulse_ = approach(pulse_, 0.f, 2.2f, dt);
    for (auto& row : flare_)
        for (float& v : row) v = approach(v, 0.f, 2.5f, dt);

    // ---- The beat grid (every two beats): a ripple through the reflections from mid on; in a drop, lanterns go up.
    if (!f.paused) {
        bool tick = false;
        if (music.hasSong) {
            const double hb = std::floor(music.beat / 2.0 + 1e-4);
            if (lastHalf_ < 0.0 || hb < lastHalf_ - 8.0) lastHalf_ = hb;
            if (hb > lastHalf_) { lastHalf_ = hb; tick = true; }
        } else {
            beatClock_ += wdt;
            if (beatClock_ > 1.f) { beatClock_ = 0.f; tick = true; }
        }
        if (tick && overT_ < 0.f) {
            if (lv_ > 0.6f) pulse_ = std::max(pulse_, 0.6f + 0.4f * wp);
            if (dropMode_) {
                spawnSky(rng_.chance(0.5f) ? 1 : -1, rng_.chance(0.6f));
                spawnSky(rng_.chance(0.5f) ? 1 : -1, true);
            }
        }
    }
    // ---- The kicks of a peak: candles flicker here and there (lanterns on the lake, one piece of the stack).
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (!f.paused && overT_ < 0.f && wp > 0.5f && kick > 0.45f && kickPrev_ <= 0.3f) {
        for (Lantern& L : lanterns_)
            if (rng_.chance(0.2f)) L.flick = std::max(L.flick, 0.6f * kick);
        int n = 0, pick = -1;
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++)
                if (ids_[y][x] && rng_.irange(0, n++) == 0) pick = ids_[y][x];
        if (pick >= 0)
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++)
                    if (ids_[y][x] == pick) flare_[y][x] = std::max(flare_[y][x], 0.22f * kick);
    }
    kickPrev_ = kick;

    const float ripple = (0.1f + 0.5f * wm + 0.4f * wp + 0.5f * pulse_ * (0.3f + 0.7f * wm)) * (1.f - 0.85f * slow_);
    updateLanterns(dt, wdt);
    for (Fade& u : fades_) u.t += dt;
    fades_.erase(std::remove_if(fades_.begin(), fades_.end(), [](const Fade& u) { return u.t > 0.36f; }), fades_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The night.
    const VariantDef& V = VARIANTS[variant_];
    const float px = 2.f * viewH_.y / height_;
    const float seedU = (float)(seed_ % 1000) * 0.137f;
    const vec3 glowCol = lin(V.dropGlow) * (0.012f + 0.02f * wp + 0.09f * dev_ + 0.07f * swell_);
    sceneT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(progLake_);
    set4f(progLake_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progLake_, "uRes", (float)width_, (float)height_);
    set1f(progLake_, "uSeed", seedU);
    set1f(progLake_, "uHorizon", HORIZON);
    set1f(progLake_, "uWater", WATER);
    set1f(progLake_, "uTime", worldTime_);
    set1f(progLake_, "uRipple", ripple);
    set1f(progLake_, "uStars", (1.f - 0.5f * wp - 0.4f * dev_) * (1.f - 0.6f * slow_));
    set1f(progLake_, "uPole", 1.f);
    set1f(progLake_, "uMoon", smoothstepf(0.f, 1.f, slow_));
    set3f(progLake_, "uSkyTop", skyTop());
    set3f(progLake_, "uSkyHor", skyHor());
    set3f(progLake_, "uLakeNear", lin(V.lake));
    set3f(progLake_, "uShore", lin(V.shore));
    set3f(progLake_, "uGlow", glowCol);
    set3f(progLake_, "uPoleCol", lin(V.pole));
    set4f(progLake_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
    set4f(progLake_, "uColR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    ugl::drawFullscreen();

    const vec3 flame = lin(0xfff1c8);
    // ---- The lanterns of the lake and the sky (the world: they leave the scene's alpha, the stack's mask, alone).
    buildSprites(ripple);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    auto drawSprites = [&](const std::vector<SpriteInst>& v) {
        if (v.empty()) return;
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        set1f(progSprite_, "uPx", px);
        set1f(progSprite_, "uTime", worldTime_);
        set3f(progSprite_, "uFlame", flame);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(SpriteInst), v.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
    };
    drawSprites(glows_);
    drawSprites(sprites_);

    // ---- The pieces, their reflections, the ghost.
    cells_.clear();
    buildBoard(game);
    if (!cells_.empty()) {
        // reflections and the ghost first (light only), then the paper
        std::stable_partition(cells_.begin(), cells_.end(), [](const CellInst& c) { return c.c[0] == 3.f || c.c[0] == 1.f; });
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uSeed", seedU);
        set1f(progCell_, "uPx", px);
        set1f(progCell_, "uTime", worldTime_);
        set1f(progCell_, "uWater", WATER);
        set3f(progCell_, "uFlame", flame);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    glDisable(GL_BLEND);

    // ---- The glow of the lights.
    bloomA_.bind();
    glUseProgram(progBright_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progBright_, "uScene", 0);
    set2f(progBright_, "uTexel", 1.f / width_, 1.f / height_);
    ugl::drawFullscreen();
    glUseProgram(progBlur_);
    set1i(progBlur_, "uTex", 0);
    for (int pass = 0; pass < 2; pass++) {
        bloomB_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        set2f(progBlur_, "uDir", (1.f + pass) / bloomA_.w, 0.f);
        ugl::drawFullscreen();
        bloomA_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        set2f(progBlur_, "uDir", 0.f, (1.f + pass) / bloomA_.h);
        ugl::drawFullscreen();
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
    set1f(progFinal_, "uBloomK", (0.45f + 0.1f * wp + 0.15f * swell_ + 0.3f * slow_) * (1.f - saturate(bridge_.amount)));
    set1f(progFinal_, "uSlow", slow_ * 0.9f);
    set1f(progFinal_, "uHorizonV", (HORIZON - (viewC_.y - viewH_.y)) / (2.f * viewH_.y));
    set3f(progFinal_, "uMist", lin(0x6a7a96) * 0.09f);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
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

const UniverseEntry& universe_lanterns() {
    static const UniverseEntry e{
        "LANTERNS", "paper lanterns on a night lake: the pieces lit from inside, each clear releasing its lanterns onto the water",
        [] { return std::unique_ptr<Universe>(new LanternsUniverse()); },
        // Gentle and emotional songs suit the lake best; it fits most songs.
        [](const SongLook& l) { return 0.8f + 0.2f * (1.f - l.brightness) + 0.15f * (1.f - l.dynamics); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
