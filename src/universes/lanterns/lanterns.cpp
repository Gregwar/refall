// LANTERNS: paper lanterns floating on a dark lake, the pieces lit from inside.
//
// One world, in real 3D: a calm lake at night seen through the layout's camera raised to a low eye height (a lens
// shift keeps the board's plane on its pixels), the lake a true plane at the well's floor receding to a far shore of
// low hills, a few faint stars. The water reflects the sky through rippled normals and a planar reflection of the
// scene (rendered mirrored). The well is two bamboo poles standing in the water, and the stack floats on the lake.
// The pieces are paper lanterns: every cell a thin opaque box, the faces inside a piece removed, so each piece is
// one volume of lit paper (darker toward its thin wooden frame, ribs and fibers seen through the paper, its candle at
// the piece's heart), each of the seven types its own paper. The cleared row becomes lanterns: each run of one piece
// in it slides out of the well to its side as a box of its paper (in the 0.3 s the row stays empty), folds into a box
// lantern as it floats down beside the well onto the water behind it, and drifts to a free place on the lake, where
// it stays lit for the whole song. So the lake fills up with the play. The drop lifts every lantern off the water
// into the sky, slowly, and sends more up from the whole lake for as long as it lasts: a sky full of lights; after it
// they go out as they rise away, and new ones gather again.
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

constexpr float EYE_Y = 3.0f;    // the camera's height: the horizon on screen (board units)
constexpr float WATER = -10.0f; // the lake's plane: the well's floor
constexpr float FLOAT_Y = 0.42f; // a floating lantern's center above the water
constexpr float CELL_D = 0.35f;  // a cell's depth behind the board's plane (cells: thin lantern panels)
constexpr int MAX_LAKE = 90;     // lanterns floating at once

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
    progEnv_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::ENV_FS).c_str(), "lanterns env");
    progBox_ = ugl::program(shaders::BOX_VS, withCommon(shaders::BOX_FS).c_str(), "lanterns boxes");
    progSprite_ = ugl::program(shaders::SPRITE_VS, shaders::SPRITE_FS, "lanterns sprites");
    progBright_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BRIGHT_FS, "lanterns bright");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "lanterns blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "lanterns final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "lanterns shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "lanterns text");

    // a unit cube, its six faces with their normals
    std::vector<float> cube;
    const float N[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    for (auto& n : N) {
        // two tangents of the face
        const vec3 nv(n[0], n[1], n[2]);
        const vec3 u = std::fabs(nv.y) > 0.5f ? vec3(1, 0, 0) : normalize(cross(vec3(0, 1, 0), nv));
        const vec3 v = cross(nv, u);
        const float c[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
        for (auto& k : c) {
            const vec3 p = nv * 0.5f + u * (0.5f * k[0]) + v * (0.5f * k[1]);
            const float vtx[6] = {p.x, p.y, p.z, nv.x, nv.y, nv.z};
            cube.insert(cube.end(), vtx, vtx + 6);
        }
    }
    glGenBuffers(1, &cubeVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glBufferData(GL_ARRAY_BUFFER, cube.size() * sizeof(float), cube.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &boxVao_);
    glGenBuffers(1, &boxVbo_);
    glBindVertexArray(boxVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glBindBuffer(GL_ARRAY_BUFFER, boxVbo_);
    for (int i = 0; i < 5; i++) {
        glEnableVertexAttribArray(2 + i);
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(BoxInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(2 + i, 1);
    }

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &spriteVao_);
    glGenBuffers(1, &spriteVbo_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(SpriteInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }

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


// The layout's reference camera raised to EYE_Y and looking straight ahead, its image shifted back down (a lens
// shift): the board's plane lands exactly on the reference camera's pixels, the horizon at EYE_Y on it, and the
// lake, a true plane at the well's floor, recedes up to it.
void LanternsUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera ref = layout::camera(width_, height_);
    const float aspect = (float)width_ / height_;
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    focal_ = 1.f / std::tan(half);
    dist_ = ref.dist;
    eye_ = vec3(ref.target.x, EYE_Y, ref.target.z + ref.dist);
    const mat4 view = mat4::lookAt(eye_, eye_ + vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));
    mat4 shift = mat4::identity();
    shift_ = focal_ * (EYE_Y - ref.target.y) / ref.dist;
    shift.at(1, 3) = shift_;
    vp_ = shift * mat4::perspective(2.f * half, aspect, 0.5f, 4000.f) * view;
    mat4 mirror = mat4::identity();
    mirror.at(1, 1) = -1.f;
    mirror.at(1, 3) = 2.f * WATER;
    mirrorVp_ = vp_ * mirror;
    const float hh = ref.dist * std::tan(half), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{ref.target.x, ref.target.y};
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
    reflT_.create(std::max(1, width_ / 2), std::max(1, height_ / 2), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
    bloomA_.create(std::max(1, width_ / 4), std::max(1, height_ / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(std::max(1, width_ / 4), std::max(1, height_ / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
}

LanternsUniverse::~LanternsUniverse() {
    sceneT_.destroy();
    reflT_.destroy();
    bloomA_.destroy();
    bloomB_.destroy();
    for (GLuint p : {progEnv_, progBox_, progSprite_, progBright_, progBlur_, progFinal_, progShape_, progText_}) glDeleteProgram(p);
    for (GLuint v : {boxVao_, spriteVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {cubeVbo_, quadVbo_, boxVbo_, spriteVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
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

TransitionHints LanternsUniverse::hints() const {
    TransitionHints h;
    h.background = mixDisplay(skyTop(), skyHor(), 0.4f);
    h.light = false;
    h.ink = lin(0xf3e6c8);
    h.accent = lin(VARIANTS[variant_].accent);
    h.styles = TS_RIPPLE | TS_LUMA | TS_INK;
    return h;
}


// The drop: every lantern on the lake lifts into the sky, more rise from the whole lake for as long as it lasts.
void LanternsUniverse::drop(float strength) {
    dropMode_ = true;
    swellIn_ = std::max(swellIn_, 0.6f + 0.4f * strength); // the swell rises from it over a second, never in one frame
    liftAll(2.5f);
    for (int i = 0; i < 8; i++) spawnSky(i % 2 ? 1 : -1, i >= 3);
}

// A new section: a far lantern goes up.
void LanternsUniverse::refresh(float strength) {
    if (strength > 0.3f && lv_ > 0.5f) spawnSky(rng_.chance(0.5f) ? 1 : -1, true);
}

void LanternsUniverse::levelUp() {
    for (int i = 0; i < 4; i++) spawnSky(i % 2 ? 1 : -1, false);
}

vec2 LanternsUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

// =====================================================================================================
// The lanterns
// =====================================================================================================

vec2 LanternsUniverse::screenBoard(const vec3& p) const {
    const vec2 s = project(p);
    return {viewC_.x + (s.x / width_ * 2.f - 1.f) * viewH_.x, viewC_.y + (1.f - s.y / height_ * 2.f) * viewH_.y};
}

// The HUD's lettering on screen (in the board plane's units): the score column down to its last value, the next
// column down to the gauge's word, the corner lines (the section, the universe's name) and the song's title.
float LanternsUniverse::hudDim(const vec3& p) const {
    const vec2 b = screenBoard(p);
    const float R[4][4] = {{-11.4f, -9.4f, -5.8f, 10.6f}, {5.8f, -5.9f, 11.4f, 10.6f}, {-23.f, -11.6f, -8.2f, -7.3f},
                           {-23.f, 8.3f, -12.f, 13.f}};
    float in = 0.f;
    for (auto& r : R) {
        const float d = std::min(std::min(b.x - r[0], r[2] - b.x), std::min(b.y - r[1], r[3] - b.y));
        in = std::max(in, smoothstepf(-0.4f, 0.4f, d));
    }
    return 1.f - 0.85f * in;
}

// A free place on the lake on one side (-1 left, 1 right), seen clear of the HUD and of the well: the best of a few
// candidates (the farthest from the lanterns already there).
bool LanternsUniverse::pickTarget(int side, float& bx, float& bz) {
    const float xmax = viewC_.x + viewH_.x - 0.4f;
    float best = -1.f;
    for (int k = 0; k < 30; k++) {
        const float u = rng_.uniform();
        const float z = -6.f - std::pow(u, 1.5f) * 140.f;
        const float f = dist_ / (dist_ - z);
        const float ys = EYE_Y + (WATER + FLOAT_Y - EYE_Y) * f;  // the lantern on screen (board plane units)
        const float hs = 0.5f * f;                               // its half size on screen
        float lo;
        if (side > 0) lo = (ys + 1.2f * f < -6.2f) ? 6.1f : 11.9f;
        else {
            if (ys - hs < -6.9f) continue;                       // the corner lettering below the score
            lo = 11.9f;
        }
        lo += hs;
        const float hi = xmax - hs;
        if (lo >= hi) continue;
        const float x = side * rng_.range(lo, hi) / f;
        float dmin = 1e9f;
        for (const Lantern& L : lanterns_) {
            if (L.sky || L.state == L_RISE || L.dying) continue;
            const float dx = x - L.tx, dz = (z - L.tz) * 0.5f;
            dmin = std::min(dmin, dx * dx + dz * dz);
        }
        if (dmin > best) { best = dmin; bx = x; bz = z; }
    }
    return best >= 0.f;
}

// A sky lantern going up from the lake (the drop, a Tetris): far away or nearer, seen beside the well.
void LanternsUniverse::spawnSky(int side, bool far) {
    const float xmax = viewC_.x + viewH_.x - 0.4f;
    Lantern L;
    L.sky = true;
    L.state = L_RISE;
    L.type = rng_.irange(0, 6);
    L.phase = rng_.range(0.f, TAU);
    L.seed = rng_.uniform();
    L.z = far ? -70.f - rng_.range(0.f, 260.f) : -14.f - rng_.range(0.f, 40.f);
    const float f = dist_ / (dist_ - L.z);
    const float lo = rng_.chance(0.9f) ? 12.2f : 7.f;
    const float xs = rng_.range(lo, std::max(lo + 0.5f, xmax - 0.5f * f));
    L.x = side * xs / f;
    L.y = WATER + FLOAT_Y;
    L.alpha = 0.f;
    lanterns_.push_back(L);
}

// Every lantern on the lake lifts off the water, each in its own time within `spread` seconds.
void LanternsUniverse::liftAll(float spread) {
    for (Lantern& L : lanterns_) {
        if (L.sky || L.state == L_RISE) continue;
        // still on its way: it lifts once there
        if (L.state != L_FLOAT) { L.pendingLift = true; continue; }
        liftOne(L, spread);
    }
}

void LanternsUniverse::liftOne(Lantern& L, float spread) {
    const float near = saturate((L.z + 150.f) / 145.f);
    L.state = L_RISE;
    L.vy = 0.f;
    L.t = 0.f;
    L.delay = rng_.range(0.f, spread) * (0.4f + 0.6f * (1.f - near)) + rng_.range(0.f, 0.3f);
}

void LanternsUniverse::updateLanterns(float dt, float wdt) {
    const float top = viewC_.y + viewH_.y + 1.5f;
    const float wm = smoothstepf(0.2f, 1.f, lv_);
    for (Lantern& L : lanterns_) {
        L.flick = approach(L.flick, 0.f, 2.5f, dt);
        if (L.dying) L.fade = std::min(1.f, L.fade + dt / 2.f);
        // out of a drop, the lanterns in the sky go out as they rise away
        else if (!dropMode_ && L.state == L_RISE && L.delay <= 0.f) L.fade = std::min(1.f, L.fade + wdt / 10.f);
        switch (L.state) {
        case L_LEAVE: {
            // the run slides out of the well to its side, a little back, in the time the row stays empty
            L.t += dt;
            const float e = smoothstepf(0.f, 1.f, L.t / L.dur);
            L.x = lerpf(L.x0, L.lx, e);
            L.z = lerpf(-0.5f * CELL_D, -1.7f, e);
            if (L.t >= L.dur) {
                L.state = L_DESCEND;
                L.t = 0.f;
                L.dur = 1.1f + 0.15f * L.slot;
                L.x0 = L.x; L.y0 = L.y; L.z0 = L.z;
                L.lx = L.lx > 0.f ? 1.f : -1.f; // the side, the landing place set below
                const float zl = -4.f - 1.6f * L.slot - rng_.range(0.f, 1.f);
                const float f = dist_ / (dist_ - zl);
                L.lx = L.lx * (5.8f + 0.3f * L.slot) / f;
                L.lz = zl;
            }
            break;
        }
        case L_DESCEND: {
            // it floats down beside the well to the water behind it, folding into a lantern on the way
            L.t += wdt;
            const float u = saturate(L.t / L.dur);
            const float ex = 1.f - (1.f - u) * (1.f - u), ey = smoothstepf(0.f, 1.f, u), es = smoothstepf(0.f, 0.7f, u);
            L.x = lerpf(L.x0, L.lx, ex);
            L.z = lerpf(L.z0, L.lz, ex);
            L.y = lerpf(L.y0, WATER + FLOAT_Y, ey);
            L.sx = lerpf(L.sx0, 0.75f, es);
            L.sy = lerpf(1.f, 0.9f, es);
            L.sz = lerpf(CELL_D, 0.75f, es);
            L.cap = smoothstepf(0.15f, 0.8f, u);
            if (u >= 1.f) {
                L.state = L_TRAVEL;
                L.t = 0.f;
                L.x0 = L.x; L.z0 = L.z;
                L.dur = 4.f + 0.07f * std::hypot(L.tx - L.x0, L.tz - L.z0) + rng_.range(0.f, 1.5f);
            }
            break;
        }
        case L_TRAVEL: {
            // then drifts across the lake to its place
            L.t += wdt;
            const float u = saturate(L.t / L.dur);
            const float k = 1.f - (1.f - u) * (1.f - u);
            L.x = lerpf(L.x0, L.tx, k);
            L.z = lerpf(L.z0, L.tz, k);
            L.y = WATER + FLOAT_Y + 0.03f * std::sin(worldTime_ * 1.3f + L.phase);
            if (u >= 1.f) {
                L.state = L_FLOAT;
                if (L.pendingLift || dropMode_) liftOne(L, 1.f);
            }
            break;
        }
        case L_FLOAT:
            L.x = L.tx + std::sin(worldTime_ * 0.05f + L.phase) * 0.3f;
            L.z = L.tz + std::sin(worldTime_ * 0.04f + L.phase * 1.7f) * 0.3f;
            // the water's slow swell, and the beat grid's ripple lifting it a little
            L.y = WATER + FLOAT_Y + 0.03f * std::sin(worldTime_ * 1.3f + L.phase) +
                  0.06f * pulse_ * wm * std::sin(worldTime_ * 2.f + L.phase * 3.f);
            break;
        case L_RISE: {
            if (L.delay > 0.f) { L.delay -= wdt; break; }
            L.t += wdt;
            L.alpha = std::min(1.f, L.alpha + wdt / 1.2f);
            // after the drop the last ones hurry away, leaving the sky to the next ones
            // (as seen: far ones rise as visibly as near ones)
            const float fz = dist_ / (dist_ - L.z);
            const float speed = std::min(10.f, (0.8f + 0.6f * L.seed) / fz) * (dropMode_ ? 1.f : 1.6f);
            L.vy = lerpf(L.vy, speed, 1.f - std::exp(-wdt * 1.1f));
            L.y += L.vy * wdt;
            L.z -= 0.15f * L.vy * wdt;
            L.x += std::sin(L.t * 0.45f + L.phase) * 0.12f * wdt;
            break;
        }
        }
    }
    lanterns_.erase(std::remove_if(lanterns_.begin(), lanterns_.end(),
                                   [&](const Lantern& L) {
                                       if (L.fade >= 1.f) return true;
                                       if (L.state == L_RISE && L.delay <= 0.f &&
                                           screenBoard(vec3(L.x, L.y - 0.5f, L.z)).y > top) return true;
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

// The lanterns as boxes (far ones first), the far lights and the embers as points of light.
void LanternsUniverse::buildLamps() {
    lamps_.clear();
    sprites_.clear();
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    std::vector<int> order(lanterns_.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return lanterns_[a].z < lanterns_[b].z; });
    for (int idx : order) {
        const Lantern& L = lanterns_[idx];
        if (L.state == L_RISE && L.delay > 0.f && L.sky) continue;
        const vec3 c = tint(L.type);
        const float a = L.alpha * (1.f - L.fade);
        if (a <= 0.003f) continue;
        // the runs leaving the well keep the stack's light (the link must read); elsewhere, dimmed behind the HUD
        const float dim = (L.state == L_LEAVE || L.state == L_DESCEND) ? 1.f : hudDim(vec3(L.x, L.y, L.z));
        const float l = light_ * (1.f + 0.35f * L.flick) * dim;
        lamps_.push_back({{L.x, L.y, L.z, 0.f}, {L.sx, L.sy, L.sz, 2.f}, {c.x, c.y, c.z, a * (0.4f + 0.6f * dim)},
                          {l, L.cap, L.seed * 17.f, 0.f},
                          {0.f, 0.f, 0.f, 1.f}});
    }
    // far lanterns along the opposite shore: more of them as the song rises
    {
        const float amount = 0.5f * wm + 0.5f * wp + 0.25f * dev_;
        const float xmax = viewC_.x + viewH_.x - 0.4f;
        Rng r(seed_ ^ 0xFA11ull);
        for (int i = 0; i < 30; i++) {
            const int side = i % 2 ? 1 : -1;
            const float z = -500.f - r.range(0.f, 500.f);
            const float f = dist_ / (dist_ - z);
            const float xs = r.range(12.2f, std::max(12.3f, xmax));
            const float th = r.uniform() * 0.95f;
            const int type = r.irange(0, 6);
            const float ph = r.range(0.f, TAU);
            const float rate = 0.4f + 0.3f * r.uniform(); // (every draw made before skipping an unlit one: the
                                                          // stream stays the same, so no light ever moves)
            const float a = saturate((amount - th) * 5.f);
            if (a <= 0.f) continue;
            const float tw = 0.85f + 0.15f * std::sin(worldTime_ * rate + ph);
            const vec3 c = tint(type) * (light_ * tw * a * 0.8f);
            sprites_.push_back({{side * xs / f, WATER + 1.5f, z, 3.2f}, {c.x, c.y, c.z, 1.f}});
        }
    }
    for (const Ember& e : embers_) {
        const float k = e.t / e.life;
        const vec3 c = tint(e.type) * (1.4f * (1.f - k) * (1.f - k));
        sprites_.push_back({{e.x, e.y, e.z, 0.16f * (1.f - 0.5f * k)}, {c.x, c.y, c.z, 1.f}});
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
    // a piece's id over two papers (the board replaced behind our back): every id is made again
    bool mixed = false;
    for (int y = 0; y < Game::H && !mixed; y++)
        for (int x = 0; x + 1 < Game::W; x++)
            if (ids_[y][x] && ids_[y][x] == ids_[y][x + 1] && g.cell(x, y).type != g.cell(x + 1, y).type) { mixed = true; break; }
    for (int y = 0; y + 1 < Game::H && !mixed; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x] && ids_[y][x] == ids_[y + 1][x] && g.cell(x, y).type != g.cell(x, y + 1).type) { mixed = true; break; }
    if (mixed) {
        std::memset(ids_, 0, sizeof(ids_));
        wrong++;
    }
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
        const vec3 p = layout::cellPos(sx / n, (float)bottom);
        const int type = ev.cells.empty() ? 0 : ev.cells[0].type;
        const float hw = 0.5f * n;
        for (int i = 0; i < 7; i++) {
            const float side = i % 2 ? 1.f : -1.f;
            Ember e;
            e.x = p.x + side * rng_.range(0.f, hw);
            e.y = p.y - 0.45f;
            e.z = 0.05f;
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
        auto typeAt = [&](int x, int y) {
            for (auto& c : ev.cells)
                if (c.x == x && c.y == y) return std::clamp(c.type, 0, 6);
            return 0;
        };
        // The cleared row becomes lanterns: each run of one piece in it is a box of its paper that slides out of
        // the well to the nearer side (each half as one block, in the 0.3 s the row stays empty), then floats down
        // beside the well onto the water behind it as a lantern, and drifts to its place on the lake.
        int rowK = -1;
        for (int y = Game::H - 1; y >= Game::HIDDEN; y--) {
            if (!rows[y]) continue;
            rowK++;
            struct Run { int x0, x1, type; };
            std::vector<Run> runs;
            int x = 0;
            while (x < Game::W) {
                const int id = ids_[y][x], type = typeAt(x, y);
                int x1 = x + 1;
                while (x1 < Game::W && (id ? ids_[y][x1] == id : (!ids_[y][x1] && typeAt(x1, y) == type))) x1++;
                runs.push_back({x, x1 - 1, type});
                x = x1;
            }
            // each side slides as one block: its innermost edge passes the pole
            float edgeL = -99.f, edgeR = 99.f;
            for (const Run& r : runs) {
                const float c = layout::cellPos(0.5f * (r.x0 + r.x1), (float)y).x;
                const float e0 = layout::cellPos((float)r.x0, (float)y).x - 0.5f, e1 = layout::cellPos((float)r.x1, (float)y).x + 0.5f;
                if (c < 0.f) edgeL = std::max(edgeL, e1);
                else edgeR = std::min(edgeR, e0);
            }
            int nRight = 0, iLeft = 0, iRight = 0;
            for (const Run& r : runs)
                if (layout::cellPos(0.5f * (r.x0 + r.x1), (float)y).x >= 0.f) nRight++;
            for (const Run& r : runs) {
                const vec3 p = layout::cellPos(0.5f * (r.x0 + r.x1), (float)y);
                const int side = p.x < 0.f ? -1 : 1;
                Lantern L;
                // its order on its side, nearest the pole first (several rows: the next row's after)
                L.slot = std::min(9, (side < 0 ? iLeft++ : nRight - 1 - iRight++) + 2 * rowK);
                L.type = r.type;
                L.state = L_LEAVE;
                L.x = L.x0 = p.x;
                L.y = L.y0 = p.y;
                L.z = -0.5f * CELL_D;
                L.sx = L.sx0 = (float)(r.x1 - r.x0 + 1);
                L.sy = 1.f;
                L.sz = CELL_D;
                L.cap = 0.f;
                L.dur = 0.3f;
                L.lx = side < 0 ? p.x - (edgeL + 5.75f) : p.x + (5.75f - edgeR);
                L.phase = rng_.range(0.f, TAU);
                L.seed = rng_.uniform();
                if (!pickTarget(side, L.tx, L.tz)) { L.tx = side * 14.f; L.tz = -20.f; }
                lanterns_.push_back(L);
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
        // too many on the lake: the oldest ones floating go out
        int floating = 0;
        for (const Lantern& q : lanterns_)
            if (!q.sky && q.state != L_RISE && !q.dying) floating++;
        for (Lantern& q : lanterns_) {
            if (floating <= MAX_LAKE) break;
            if (!q.sky && q.state == L_FLOAT && !q.dying) { q.dying = true; floating--; }
        }
        // a Tetris: a dozen lanterns go up from the lake
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
            const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, -1, -1, 1, 1}; // sides, then corners
            for (int i = 0; i < 8; i++) {
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
    stack_.clear();
    ghost_.clear();
    poles_.clear();
    const float light = light_;
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, -1, -1, 1, 1}; // sides, then corners
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    // a cell: a box one cell deep behind the board's plane
    auto push = [&](std::vector<BoxInst>& v, float x, float y, float s, int mask, const vec3& c, float alpha, int kind,
                    float l, float seed, float cx, float cy, float r) {
        if (alpha <= 0.003f) return;
        v.push_back({{x, y, -0.5f * CELL_D * s, (float)mask}, {s, s, CELL_D * s, (float)kind}, {c.x, c.y, c.z, alpha}, {l, 0.f, seed, 0.f},
                     {cx, cy, -0.5f * CELL_D * s, r}});
    };
    // the well's poles, standing in the water
    for (int side = -1; side <= 1; side += 2) {
        const float y0 = WATER - 0.8f, y1 = 10.25f;
        poles_.push_back({{side * 5.12f, 0.5f * (y0 + y1), -0.5f, 0.f}, {0.13f, y1 - y0, 0.13f, 3.f}, {0, 0, 0, 1}, {0, 0, 0, 0}, {0, 0, 0, 1}});
    }
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
        for (const OverCell& o : overCells_)
            push(stack_, o.x, o.y, 1.f, o.mask, tint(o.type), 1.f, 0, light * litOf(o.row), o.seed, o.cx, o.cy, o.r);
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
                push(ghost_, w.x, w.y, 1.f, maskOf(c[0], c[1], inGhost), tint(p.type), 0.8f, 1, 1.f, 0.f, w.x, w.y, 1.f);
            }
        }
    }
    // The stack.
    if (!(overT_ >= 0.f && !overCells_.empty()))
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Cell& c = g.cell(x, y);
                if (c.type < 0) continue;
                const float off = g.rowOffset(y);
                auto same = [&](int nx, int ny) {
                    if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H) return false;
                    if (std::fabs(g.rowOffset(ny) - off) > 0.02f) return false;
                    if (g.cell(nx, ny).type != c.type) return false; // (stale ids never merge two papers)
                    if (ids_[y][x] && ids_[ny][nx]) return ids_[ny][nx] == ids_[y][x];
                    return g.cell(nx, ny).type == c.type;
                };
                const vec3 p = layout::cellPos((float)x, y - off);
                float cx = p.x, cy = p.y, r = 0.9f;
                for (const Ctr& q : ctr)
                    if (q.id == ids_[y][x]) { cx = q.sx / q.n; cy = q.sy / q.n; r = 0.75f + 0.18f * q.n; break; }
                const float l = light * (1.f + flare_[y][x]) * litOf(y);
                push(stack_, p.x, p.y, 1.f, maskOf(x, y, same), tint(c.type), 1.f, 0, l, (float)(ids_[y][x] % 97), cx, cy, r);
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
            push(stack_, w.x, w.y, 1.f, maskOf(c[0], c[1], inPiece), tint(p.type), 1.f, 0, light * 1.05f, 7.f, cx, cy, 1.5f);
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
            push(stack_, px, py, sc, maskOf(c[0], c[1], inPiece), tint(type), 1.f, 0, light * alpha, 3.f, center.x, center.y,
                 1.4f * sc);
            // a preview is a thin panel (seen off axis in the margins, a deep box would show a slab of its side)
            BoxInst& b = stack_.back();
            b.b[2] = 0.3f * sc;
            b.a[2] = b.e[2] = -0.15f * sc;
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

void LanternsUniverse::drawBoxes(const std::vector<BoxInst>& v, const mat4& vp, bool mirror, float stackA) {
    if (v.empty()) return;
    glUseProgram(progBox_);
    setMat(progBox_, "uVP", vp);
    set1f(progBox_, "uMirror", mirror ? 1.f : 0.f);
    set1f(progBox_, "uWater", WATER);
    set1f(progBox_, "uStackA", stackA);
    set1f(progBox_, "uSeed", (float)(seed_ % 1000) * 0.137f);
    set3f(progBox_, "uFlame", lin(0xfff1c8));
    set3f(progBox_, "uWood", lin(VARIANTS[variant_].pole) * 0.35f);
    glBindVertexArray(boxVao_);
    glBindBuffer(GL_ARRAY_BUFFER, boxVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(BoxInst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)v.size());
}

void LanternsUniverse::drawSprites(const mat4& vp, bool mirror) {
    if (sprites_.empty()) return;
    glUseProgram(progSprite_);
    setMat(progSprite_, "uVP", vp);
    set1f(progSprite_, "uMirror", mirror ? 1.f : 0.f);
    set1f(progSprite_, "uWater", WATER);
    set3f(progSprite_, "uFlame", lin(0xfff1c8));
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
}

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
    swellIn_ = approach(swellIn_, 0.f, 0.45f, dt);
    swell_ = approach(swell_, swellIn_, 2.5f, dt);
    pulse_ = approach(pulse_, 0.f, 2.2f, dt);
    for (auto& row : flare_)
        for (float& v : row) v = approach(v, 0.f, 2.5f, dt);

    // ---- The beat grid (every two beats): a ripple through the water from mid on; in a drop, lanterns go up.
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

    ripple_ = (0.1f + 0.5f * wm + 0.4f * wp + 0.5f * pulse_ * (0.3f + 0.7f * wm)) * (1.f - 0.85f * slow_);
    light_ = (0.92f + 0.05f * wm + 0.06f * wp + 0.05f * swell_) * (1.f - 0.1f * slow_);
    updateLanterns(dt, wdt);
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);
    buildBoard(game);
    buildLamps();

    const VariantDef& V = VARIANTS[variant_];
    const float seedU = (float)(seed_ % 1000) * 0.137f;
    const vec3 glowCol = lin(V.dropGlow) * (0.012f + 0.02f * wp + 0.09f * dev_ + 0.07f * swell_);
    glDisable(GL_CULL_FACE);

    // ---- The scene mirrored under the water (the stack, the poles, the lanterns, the lights).
    reflT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    drawBoxes(poles_, mirrorVp_, true, 1.f);
    drawBoxes(stack_, mirrorVp_, true, 1.f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawBoxes(lamps_, mirrorVp_, true, 1.f);
    glDepthMask(GL_FALSE);
    drawSprites(mirrorVp_, true);
    glDepthMask(GL_TRUE);

    // ---- The night: the sky and the lake.
    sceneT_.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(progEnv_);
    set4f(progEnv_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progEnv_, "uEye", eye_);
    set1f(progEnv_, "uFocal", focal_);
    set1f(progEnv_, "uAspect", (float)width_ / height_);
    set1f(progEnv_, "uShift", shift_);
    set1f(progEnv_, "uSeed", seedU);
    set1f(progEnv_, "uWater", WATER);
    set1f(progEnv_, "uTime", worldTime_);
    set1f(progEnv_, "uRipple", ripple_);
    set1f(progEnv_, "uStars", (1.f - 0.5f * wp - 0.4f * dev_) * (1.f - 0.6f * slow_));
    set1f(progEnv_, "uMoon", smoothstepf(0.f, 1.f, slow_));
    set1f(progEnv_, "uPxAng", 2.f / (focal_ * height_));
    set3f(progEnv_, "uSkyTop", skyTop());
    set3f(progEnv_, "uSkyHor", skyHor());
    set3f(progEnv_, "uLake", lin(V.lake));
    set3f(progEnv_, "uShore", lin(V.shore));
    set3f(progEnv_, "uGlow", glowCol);
    set4f(progEnv_, "uColL", layout::LEFT_COLUMN[0], -9.4f, layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
    set4f(progEnv_, "uColR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    set4f(progEnv_, "uCorner", -23.f, 8.3f, -12.f, 13.f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, reflT_.tex);
    set1i(progEnv_, "uRefl", 0);
    ugl::drawFullscreen();

    // ---- The poles, the stack (opaque, the stack's mask in alpha), the lanterns, the lights, the ghost.
    glEnable(GL_DEPTH_TEST);
    drawBoxes(poles_, vp_, false, 0.f);
    drawBoxes(stack_, vp_, false, 1.f);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    drawBoxes(lamps_, vp_, false, 0.f);
    glDepthMask(GL_FALSE);
    drawSprites(vp_, false);
    drawBoxes(ghost_, vp_, false, 0.f);
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
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
    set1f(progFinal_, "uHorizonV", (EYE_Y - (viewC_.y - viewH_.y)) / (2.f * viewH_.y));
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

