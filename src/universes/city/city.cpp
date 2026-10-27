// CITY: a tower's facade at night, its windows the board.
//
// One world: a modern tower seen straight on fills the well, its 10 x 20 windows are the board. A block is a lit
// window: a room behind the glass in the piece's own light, each piece type its own interior (venetian blinds,
// an open living room, drapes, vertical blinds, a roller blind, a paper screen, an office under a strip light),
// someone passing in a room now and then. An empty cell is a dark pane reflecting the sky and the city. The
// falling piece is four windows lighting up as it moves (blinds raised, brighter), the ghost four window frames lit
// in its color. Lights switch like lights: a quick warm-up (a fluorescent flicker when the stack's floors step
// down), a short fade when they go out. Around it the city in three layers of depth (the far skyline in the haze,
// the blocks behind, the buildings flanking the tower), their small windows lit and put out one by one, slowly; red
// aircraft beacons, a crane, a highway's traffic low between the buildings.
//
// Staging: a break is the city asleep (few windows, the beacons blinking on their own); mid wakes it (more
// windows, the traffic, the beacons on the beat grid every two beats); a chorus lights the crowns of the towers and
// the tower's piers, two searchlights sweeping slowly behind the skyline; a drop switches the whole city at once
// into its pattern: every building's windows turn to the drop's two colors in stripes, checks, diagonals or a level
// meter following the music, changing every two bars, with four colored searchlights, the crowns and piers lit, the
// beacons hitting with the kicks. Nothing pulses the whole scene.
//
// Events: a lock surges the piece's windows and lights the ledge under it (a hard drop makes the lights below it
// flicker); a cleared floor's windows go out (a last surge, a flicker, dark: nothing drawn over the row), then the
// floors above step down one by one, switching; a Tetris sends fireworks up beside the skyline and a chase of light
// up the tower's piers; slow motion is a long exposure (light trails, a colder sky, the city's lights low); game over
// is a blackout floor by floor from the top, the city going dark with it.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane (like LIGHTSOURCE).
#include "universes/city/city.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/city/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Barlow, SIL Open Font License, see OFL-Barlow.txt.
const unsigned char FONT_BARLOW[] = {
#include "universes/city/Barlow-Medium.ttf.inc"
};

namespace shaders = city_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(vs, f.c_str(), name);
}

vec2 v2(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// The variants: the sky (top, middle, horizon, the city's glow, fog), the three layers of buildings, the tower
// (frame, glass, what the glass reflects), the city's window light, the accent (crowns, piers), the HUD's text and
// its halo, the lobby, the seven pieces' lights (I O T S Z J L), the facade's mullions, spandrels and the depth of
// its windows, the glass's reflection, stars, fog, rain, snow.
struct VariantDef {
    const char* name;
    unsigned top, mid, hor, glow, fog, far, midB, nearB, frame, glass, reflTop, reflBot, city, accent, text, shade, lobby;
    unsigned piece[7];
    float mull, spand, recess, reflK, stars, fogK, rain, snow, glowK;
};
const VariantDef VARIANTS[] = {
    // a glass tower in the blue hour, a warm band left on the horizon
    {"BLUE HOUR", 0x0a1530, 0x1c3866, 0x6c8fb8, 0xe0a080, 0x5a7aa0, 0x2c4670, 0x1a2c4e, 0x0e182c, 0x1c2534, 0x0c1626,
     0x3a5a8a, 0x1a2a48, 0xffd6a0, 0x8fd8ff, 0xe8f0fa, 0x040810, 0xffcf98,
     {0xe2f4ff, 0xffc066, 0xff8cae, 0x86f0bc, 0xff8a58, 0x76b4ff, 0xf4ea76},
     0.1f, 0.2f, 0.1f, 0.55f, 0.25f, 0.f, 0.f, 0.f, 0.35f},
    // downtown at midnight: bronze frames, warm rooms, a sodium glow
    {"MIDNIGHT", 0x04060c, 0x0b1020, 0x2c2230, 0xff9040, 0x2c2230, 0x1a1a26, 0x10111a, 0x07080d, 0x2a221c, 0x080a10,
     0x26222a, 0x0e0e14, 0xffbf6a, 0xffb45a, 0xf2e6d6, 0x040302, 0xffb060,
     {0xfff0d2, 0xffae40, 0xff5e4e, 0x52e0c4, 0xffd860, 0x9a88ff, 0xff94cc},
     0.12f, 0.24f, 0.25f, 0.4f, 0.6f, 0.f, 0.f, 0.f, 0.3f},
    // a rainy night in neon: magenta haze, wet glass
    {"NEON RAIN", 0x090514, 0x1c0c30, 0x5a2468, 0xff40a8, 0x3a1850, 0x2a1844, 0x180e2a, 0x0c0716, 0x15131e, 0x0e0a18,
     0x4a2a6a, 0x1a1030, 0x80eeff, 0xff4ad8, 0xf4e8ff, 0x050209, 0xff60c0,
     {0x4cf0ff, 0xffe04c, 0xff4ad4, 0x66ff78, 0xff683a, 0x5a78ff, 0xc47aff},
     0.08f, 0.18f, 0.05f, 0.6f, 0.f, 0.25f, 1.f, 0.f, 0.4f},
    // a foggy dawn: a pale sky, the far towers lost in the fog, the last lights on
    {"FOG DAWN", 0x7f93aa, 0xb8c0ca, 0xead2c0, 0xffc8a0, 0xd6d4d2, 0xaab0ba, 0x8a929e, 0x56606c, 0x3a424e, 0x1a222e,
     0x7a8aa0, 0x2e3a4a, 0xffd090, 0xff9a62, 0x1a222c, 0xe8e6e4, 0xffd8a8,
     {0xfff0c8, 0xffa830, 0xff5c70, 0x3ed0a0, 0xff7a28, 0x4a92ff, 0xe6dc4a},
     0.1f, 0.2f, 0.1f, 0.5f, 0.f, 0.55f, 0.f, 0.f, 0.2f},
    // a brutalist block at dusk: raw concrete, deep-set windows, colored loggias
    {"BRUTALIST", 0x1a2030, 0x46465a, 0xd88a58, 0xff8a48, 0x6a5a5a, 0x5a5462, 0x3a3842, 0x222129, 0x77746e, 0x101114,
     0x5a5260, 0x202024, 0xffc888, 0xff4a30, 0xf2ede6, 0x0a0908, 0xffc890,
     {0xf6f0e2, 0xffc014, 0xe8381e, 0x38a864, 0xff7614, 0x2c6ae0, 0x62c4e8},
     0.26f, 0.34f, 1.f, 0.3f, 0.1f, 0.f, 0.f, 0.f, 0.3f},
    // a snowy evening: brick, snow on the ledges, warm rooms
    {"SNOWFALL", 0x0e1530, 0x283258, 0x6a6290, 0xffa868, 0x5a6080, 0x545a7c, 0x323856, 0x1c2036, 0x2c2426, 0x10141f,
     0x3c4468, 0x161a2a, 0xffc47c, 0xbfdcff, 0xeef2ff, 0x04050a, 0xffc080,
     {0xfff0d8, 0xffaa48, 0xff5858, 0x7ce09c, 0xffd06a, 0x86acff, 0xd68cff},
     0.14f, 0.26f, 0.4f, 0.4f, 0.f, 0.15f, 0.f, 1.f, 0.3f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));
constexpr int FOG_DAWN = 3;

// A drop's two colors (linear): designed pairs of saturated hues, a new pair each drop.
const vec3 HUES[] = {{1.f, 0.05f, 0.55f}, {0.f, 0.85f, 1.f}, {1.f, 0.55f, 0.f}, {0.35f, 0.15f, 1.f},
                     {0.3f, 1.f, 0.2f}, {1.f, 0.85f, 0.05f}, {1.f, 0.1f, 0.15f}, {0.05f, 0.4f, 1.f}};
const int PAIRS[][2] = {{0, 1}, {2, 3}, {4, 0}, {5, 7}, {6, 1}, {3, 2}, {1, 5}, {7, 6}};
constexpr int PAIR_COUNT = (int)(sizeof(PAIRS) / sizeof(PAIRS[0]));
constexpr float STREET_Y = -11.8f, HIGHWAY_Y = -9.35f, HORIZON_Y = -9.f;
const vec3 BEACON_RED{1.f, 0.07f, 0.02f};

float hashf(float x) {
    const float s = std::sin(x * 127.1f + 311.7f) * 43758.5453f;
    return s - std::floor(s);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool CityUniverse::init(int width, int height) {
    progSky_ = compile(ugl::FULLSCREEN_VS, shaders::SKY_FS, "city sky");
    progBldg_ = compile(shaders::RECT_VS, shaders::BLDG_FS, "city buildings");
    progTower_ = compile(shaders::RECT_VS, shaders::TOWER_FS, "city tower");
    progWin_ = compile(shaders::RECT_VS, shaders::WIN_FS, "city windows");
    progDot_ = compile(shaders::DOT_VS, shaders::DOT_FS, "city dots");
    progFinal_ = compile(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "city final");
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "city text");
    progPanel_ = compile(shaders::PANEL_VS, shaders::PANEL_FS, "city panel");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &instVao_);
    glGenBuffers(1, &instVbo_);
    glBindVertexArray(instVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Inst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glGenVertexArrays(1, &panelVao_);
    glGenBuffers(1, &panelVbo_);
    glBindVertexArray(panelVao_);
    glBindBuffer(GL_ARRAY_BUFFER, panelVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    GLuint* texs[2] = {&cellA_, &cellB_};
    for (int i = 0; i < 2; i++) {
        glGenTextures(1, texs[i]);
        glBindTexture(GL_TEXTURE_2D, *texs[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, COLS, ROWS, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            pendingOff_[r][c] = 0;
            pendingDelay_[r][c] = 0.f;
            win_[r][c].seed = hashf(r * 17.f + c * 3.1f);
        }
    for (int& o : off_) o = 0;
    Rng fr(99);
    for (int i = 0; i < 220; i++) flakes_.push_back({v2(fr.range(-22.f, 22.f), fr.range(-12.f, 13.f)), fr.range(0.6f, 1.4f), fr.range(0.f, TAU)});
    buildFont(font_, FONT_BARLOW);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void CityUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] city font init failed\n");
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
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, ch, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, ch, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
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

void CityUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = v2(hw, hh);
    viewC_ = v2(cam.target.x, cam.target.y);
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.destroy();
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
}

CityUniverse::~CityUniverse() {
    sceneT_.destroy();
    for (GLuint p : {progSky_, progBldg_, progTower_, progWin_, progDot_, progFinal_, progText_, progPanel_}) glDeleteProgram(p);
    for (GLuint v : {instVao_, textVao_, panelVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, instVbo_, textVbo_, panelVbo_}) glDeleteBuffers(1, &b);
    for (GLuint t : {cellA_, cellB_}) glDeleteTextures(1, &t);
    if (font_.tex) glDeleteTextures(1, &font_.tex);
    delete (stbtt_fontinfo*)font_.info;
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void CityUniverse::setSong(const SongLook& t, float seconds) {
    if (t.variant >= 0) variant_ = t.variant % VARIANT_COUNT;
    else {
        // night songs downtown or in the rain, bright ones in the blue hour or the dawn fog, heavy ones in concrete,
        // slow quiet ones in the snow
        int v;
        if (t.bassWeight > 0.62f && t.dynamics > 0.5f) v = 4;
        else if (t.bpm < 100.f && t.density < 0.5f) v = 5;
        else if (t.minor && t.brightness < 0.55f) v = (t.bpm > 118.f || t.airWeight > 0.55f) ? 2 : 1;
        else v = (t.brightness > 0.6f && t.dynamics < 0.5f) ? 3 : 0;
        if ((t.seed % 5) == 0) v = (v + 1 + (int)((t.seed >> 8) % 4)) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 4)) % VARIANT_COUNT;
        variant_ = v;
    }
    rng_ = Rng(t.seed ^ 0xC17Full);
    songSeed_ = t.seed;
    dropIndex_ = 0;
    pickDropColors();
    if (seconds <= 0.f) palSet_ = false;
}

void CityUniverse::setLevel(int level, float, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    if (level < 2) inDrop_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        dropWait_ = 0.f;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    if (rising && level >= 2) dropWait_ = 1.f; // the peak is announced just before its downbeat: wait for drop()
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 0.8f + 0.8f * (1.f - impact) : 2.5f, 6.f);
}

void CityUniverse::drop(float strength) {
    if (level_ >= 2) inDrop_ = true;
    dropWait_ = 0.f;
    dropT_ = 0.f;
    dropAmp_ = 0.6f + 0.4f * saturate(strength);
    if (level_ >= 2) {
        pickDropColors();
        drop_ = std::max(drop_, 0.85f); // the city switches at once, on the downbeat
    }
}

void CityUniverse::refresh(float strength) {
    // a new section: the city's windows are put out and lit again elsewhere (the same number of them)
    if (strength > 0.2f) cityShift_ += 97.f;
    refreshT_ = 0.f;
}

void CityUniverse::forceFx(int slow, int tetris) {
    (void)slow;
    if (tetris >= 0) {
        tetrisT_ = 0.f;
        spawnFireworks();
    }
}

void CityUniverse::pickDropColors() {
    Rng r(songSeed_ ^ (0x9E3779B97F4A7C15ull * (uint64_t)(dropIndex_ + 1)) ^ (uint64_t)variant_);
    int h = r.irange(0, PAIR_COUNT - 1);
    if (h == lastPair_) h = (h + 1 + r.irange(0, PAIR_COUNT - 3)) % PAIR_COUNT;
    lastPair_ = h;
    const bool swap = r.uniform() < 0.5f;
    dropA_ = HUES[PAIRS[h][swap ? 1 : 0]];
    dropB_ = HUES[PAIRS[h][swap ? 0 : 1]];
    dropIndex_++;
}

vec2 CityUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints CityUniverse::hints() const {
    const VariantDef& d = VARIANTS[variant_];
    TransitionHints h;
    h.light = variant_ == FOG_DAWN;
    h.background = h.light ? lin(d.mid) : lin(d.mid) * 0.5f;
    h.ink = h.light ? lin(d.nearB) : lin(d.text) * 0.8f;
    h.accent = lin(d.accent);
    h.styles = TS_TILES | TS_LUMA | TS_REBUILD;
    return h;
}

// =====================================================================================================
// The city
// =====================================================================================================

void CityUniverse::buildCity(int variant) {
    bldgs_.clear();
    Rng r(0xC1C1ull + (uint64_t)variant * 7919ull);
    auto add = [&](float x0, float x1, float top, int layer, int style, float lit, float crown, bool beacon) {
        Bldg b;
        b.x0 = x0; b.x1 = x1; b.y0 = STREET_Y; b.y1 = top;
        b.layer = layer; b.style = style;
        b.seed = r.uniform();
        b.lit = lit; b.crown = crown; b.beacon = beacon;
        bldgs_.push_back(b);
    };
    // the far skyline, in the haze, two landmark spires
    for (float x = -22.5f; x < 22.5f;) {
        const float w = r.range(1.0f, 2.6f);
        const float side = std::fabs(x + w * 0.5f);
        float top = r.range(-3.5f, 3.5f) + (side > 12.f ? r.range(0.f, 2.f) : 0.f);
        const int st = r.chance(0.2f) ? 1 : (r.chance(0.12f) ? 2 : 0);
        const bool hud = side > 5.f && side < 12.f; // (quiet under the HUD's columns)
        add(x, x + w, top, 2, st, r.range(0.6f, 1.2f) * (hud ? 0.45f : 1.f), r.chance(0.35f) && !hud ? 1.f : 0.f, false);
        x += w + r.range(0.f, 0.5f);
    }
    const float sx = (variant % 2) ? 1.f : -1.f;
    add(sx * 15.6f - 0.8f, sx * 15.6f + 0.8f, 9.6f + r.range(-0.5f, 0.6f), 2, 4, 1.f, 1.f, true);
    add(-sx * 17.2f - 0.9f, -sx * 17.2f + 0.9f, 7.4f + r.range(-0.5f, 0.6f), 2, 1, 1.f, 1.f, true);
    // the blocks behind
    for (float x = -22.5f; x < 22.5f;) {
        const float w = r.range(1.6f, 3.4f);
        const float cx = std::fabs(x + w * 0.5f);
        float top = r.range(-6.5f, 0.5f);
        if (cx > 12.f) top += r.range(0.f, 2.5f);
        else if (cx > 5.f) top = std::min(top, r.range(-5.5f, -3.5f)); // (low under the HUD's columns)
        const int st = r.chance(0.18f) ? 1 : (r.chance(0.12f) ? 2 : 0);
        add(x, x + w, top, 1, st, r.range(0.6f, 1.2f), r.chance(0.4f) ? 1.f : 0.f, top > 1.5f && r.chance(0.5f));
        x += w + r.range(0.1f, 0.8f);
    }
    // the buildings flanking the tower: low under the HUD's columns, tall at the edges of the screen
    add(-21.8f, -16.0f, 5.4f, 0, 0, 0.8f, 1.f, true);
    add(-15.8f, -12.6f, -1.2f, 0, 2, 0.9f, 0.f, false);
    add(-12.4f, -6.1f, -8.3f, 0, 0, 1.f, 1.f, false);
    add(6.1f, 12.3f, -8.7f, 0, 0, 1.f, 1.f, false);
    add(12.5f, 15.4f, 0.6f, 0, 1, 0.9f, 1.f, false);
    add(15.6f, 21.8f, 3.6f, 0, 0, 0.8f, 1.f, true);
    builtFor_ = variant;
}

void CityUniverse::spawnFireworks() {
    Rng r(songSeed_ ^ (uint64_t)(time_ * 1000.f));
    const VariantDef& d = VARIANTS[variant_];
    for (int b = 0; b < 6; b++) {
        const float side = (b % 2) ? 1.f : -1.f;
        const vec2 c = v2(side * r.range(12.8f, 19.5f), r.range(4.5f, 10.5f));
        const vec3 col = lin(d.piece[r.irange(0, 6)]);
        const int n = 46;
        for (int i = 0; i < n; i++) {
            const float a = TAU * (i + r.uniform() * 0.5f) / n;
            const float s = r.range(2.4f, 4.0f);
            Particle p;
            p.p = c;
            p.v = v2(std::cos(a) * s, std::sin(a) * s);
            p.c = col;
            p.t = 0.f;
            p.life = r.range(1.2f, 1.8f);
            p.delay = b * 0.3f + r.range(0.f, 0.03f);
            sparks_.push_back(p);
        }
    }
}

// =====================================================================================================
// The facade's windows
// =====================================================================================================

float CityUniverse::onLevel(const Win& w) const {
    const float t = w.t;
    if (w.onKind == 0) return lerpf(w.from, 1.f, smoothstepf(0.f, 0.035f, t));
    // a tube warming up: on, a flicker, on
    float v = smoothstepf(0.f, 0.03f, t);
    if (t > 0.05f && t < 0.085f) v *= 0.25f;
    if (t > 0.13f && t < 0.15f) v *= 0.6f;
    return std::max(v, w.from * (1.f - smoothstepf(0.f, 0.03f, t)));
}

float CityUniverse::offLevel(const Win& w) const {
    const float t = w.t, a = w.from;
    if (w.offKind == 1) {
        // a cleared floor: a last surge, a flicker, out (under a third of a second)
        if (t < 0.06f) return a * 1.4f;
        if (t < 0.1f) return a * 0.1f;
        if (t < 0.15f) return a * 0.9f;
        return a * 0.7f * (1.f - smoothstepf(0.15f, 0.32f, t));
    }
    if (w.offKind == 2) {
        // the blackout
        if (t < 0.f) return a;
        if (t < 0.08f) return a * 0.3f;
        if (t < 0.16f) return a * 0.85f;
        return a * 0.85f * (1.f - smoothstepf(0.16f, 0.6f, t));
    }
    // (the falling piece's windows go out at once behind it: no trail)
    if (w.offKind == 3) return t > 0.06f ? 0.f : a * std::exp(-t / 0.018f);
    return t > 0.25f ? 0.f : a * std::exp(-t / 0.045f);
}

void CityUniverse::updateWindows(const Game& g, float dt) {
    // the stack's floors stepping down after a clear
    bool stepping = false;
    for (int y = 0; y < Game::H; y++) stepping |= off_[y] > 0;
    if (stepping) {
        if (offHold_ > 0.f) offHold_ -= dt;
        else {
            offStep_ += dt;
            while (offStep_ >= 0.05f) {
                offStep_ -= 0.05f;
                for (int& o : off_) o = std::max(0, o - 1);
            }
        }
    }
    struct Want { bool on = false, falling = false; int type = -1; };
    Want want[ROWS][COLS];
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < COLS; x++) {
            const int t = g.cell(x, y).type;
            if (t < 0) continue;
            const int vr = y - off_[y] - Game::HIDDEN;
            if (vr < 0 || vr >= ROWS) continue;
            want[vr][x].on = true;
            want[vr][x].type = t;
        }
    float ghost[ROWS][COLS] = {};
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        for (auto& c : cells) {
            const int vr = c[1] - Game::HIDDEN;
            if (vr < 0 || vr >= ROWS || c[0] < 0 || c[0] >= COLS) continue;
            want[vr][c[0]] = {true, true, p.type};
        }
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (auto& c : gc) {
                const int vr = c[1] - Game::HIDDEN;
                if (vr < 0 || vr >= ROWS || c[0] < 0 || c[0] >= COLS || want[vr][c[0]].on) continue;
                ghost[vr][c[0]] = 1.f;
            }
        }
    }
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            Win& w = win_[r][c];
            const Want& q = want[r][c];
            if (q.on) {
                if (!w.on || w.style != q.type) {
                    w.from = w.style == q.type ? w.level : w.level * 0.3f;
                    w.on = true;
                    w.t = 0.f;
                    w.onKind = q.falling ? 0 : 1;
                    if (w.style != q.type) w.seed = hashf(r * 7.3f + c * 1.9f + time_ * 3.7f);
                    w.style = q.type;
                }
                w.falling = q.falling;
            } else if (w.on) {
                w.on = false;
                w.from = w.level;
                w.offKind = pendingOff_[r][c] ? pendingOff_[r][c] : (w.falling ? 3 : 0);
                w.t = -pendingDelay_[r][c];
                w.falling = false;
            }
            pendingOff_[r][c] = 0;
            pendingDelay_[r][c] = 0.f;
            w.t += dt;
            w.level = w.on ? onLevel(w) : offLevel(w);
            if (!w.on && w.level < 0.002f && w.t > 0.f) w.level = 0.f;
            w.surge = std::max(0.f, w.surge - dt / 0.35f);
            w.dip = std::max(0.f, w.dip - dt / 0.25f);
            w.ghost = ghost[r][c] > 0.f ? 1.f : approach(w.ghost, 0.f, 30.f, dt);
            w.col = lin(VARIANTS[variant_].piece[std::clamp(w.style, 0, 6)]);
        }
}

void CityUniverse::onEvent(const GameEvent& ev, const Game& g) {
    (void)g;
    switch (ev.type) {
    case GameEvent::HardDrop: hardPending_ = true; break;
    case GameEvent::Lock: {
        float lowY = 1e9f, x0 = 1e9f, x1 = -1e9f;
        int type = 0;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= COLS) continue;
            type = c.type;
            const int vr = c.y - off_[c.y] - Game::HIDDEN;
            if (vr >= 0 && vr < ROWS) win_[vr][c.x].surge = hardPending_ ? 1.f : 0.6f;
            const float y = (float)(Game::H - 1 - c.y) - 9.5f;
            if (y < lowY - 0.1f) { lowY = y; x0 = x1 = c.x - 4.5f; }
            else if (std::fabs(y - lowY) < 0.1f) { x0 = std::min(x0, c.x - 4.5f); x1 = std::max(x1, c.x - 4.5f); }
            // a hard drop shakes the lights of the floors under it
            if (hardPending_) {
                for (int k = 1; k <= 2; k++) {
                    const int below = vr + k;
                    if (below >= 0 && below < ROWS) win_[below][c.x].dip = std::max(win_[below][c.x].dip, k == 1 ? 1.f : 0.5f);
                }
            }
        }
        if (lowY < 1e8f && lowY > -10.f) {
            Contact ct;
            ct.x0 = x0 - 0.5f;
            ct.x1 = x1 + 0.5f;
            ct.y = lowY - 0.5f;
            ct.col = lin(VARIANTS[variant_].piece[std::clamp(type, 0, 6)]);
            ct.power = hardPending_ ? 1.f : 0.5f;
            ct.life = hardPending_ ? 0.45f : 0.3f;
            contacts_.push_back(ct);
            if (contacts_.size() > 4) contacts_.erase(contacts_.begin());
        }
        hardPending_ = false;
        break;
    }
    case GameEvent::Clear: {
        bool cleared[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) {
                cleared[c.y] = true;
                const int vr = c.y - off_[c.y] - Game::HIDDEN;
                if (vr >= 0 && vr < ROWS && c.x >= 0 && c.x < COLS) pendingOff_[vr][c.x] = 1;
            }
        // the floors above keep their windows until the cleared floor is out, then step down
        int noff[Game::H] = {};
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (cleared[y]) continue;
            noff[dst] = off_[y] + (dst - y);
            dst--;
        }
        std::memcpy(off_, noff, sizeof(noff));
        offHold_ = 0.24f;
        offStep_ = 0.f;
        if (ev.count >= 4) {
            tetrisT_ = 0.f;
            spawnFireworks();
        }
        break;
    }
    case GameEvent::TopOut: {
        // the blackout, floor by floor from the top
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            const int vr = c.y - off_[c.y] - Game::HIDDEN;
            if (vr < 0 || vr >= ROWS || c.x < 0 || c.x >= COLS) continue;
            pendingOff_[vr][c.x] = 2;
            pendingDelay_[vr][c.x] = (c.y - top) * 0.08f + rng_.range(0.f, 0.04f);
        }
        for (int& o : off_) o = 0;
        overT_ = 0.f;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Text
// =====================================================================================================

float CityUniverse::textWidth(const std::string& s, float size, float track) const {
    const Font& f = font_;
    if (!f.ok || s.empty()) return s.size() * size * 0.6f;
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
            w += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
    return w - track;
}

void CityUniverse::textQuads(const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const {
    const Font& f = font_;
    if (!f.ok) return;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
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
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            pen += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
}

void CityUniverse::drawText(const std::vector<float>& q, const vec3& srgb, float alpha, float bias, float soft) {
    if (q.empty() || !font_.ok || alpha <= 0.003f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set4f(progText_, "uInk", srgb.x, srgb.y, srgb.z, saturate(alpha));
    set1f(progText_, "uBias", bias);
    set1f(progText_, "uSoft", soft);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD in CITY's typography: Barlow (a signage face), the labels in small widely tracked capitals, the numbers
// plain, each line on a soft halo of the night (of the fog's pale at dawn) so it reads over the lit buildings; the
// section, the notices and the universe's name in the accent, like a building's sign.
void CityUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels, float mul) {
    const float bridge = saturate(bridge_.amount);
    const vec3 ink = lerp(pal_.text, bridge_.ink, bridge) * mul, acc = lerp(pal_.accent, bridge_.ink, bridge) * mul;
    const vec3 shadow = ugl::toSrgb(lerp(pal_.shade, bridge_.color, bridge));
    if (!panels.empty()) {
        glUseProgram(progPanel_);
        set2f(progPanel_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(panelVao_);
        glBindBuffer(GL_ARRAY_BUFFER, panelVbo_);
        for (const HudRect& r : panels) {
            const float q[12] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
            glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STREAM_DRAW);
            const vec3 c = r.custom ? r.color : r.accent ? ugl::toSrgb(acc) : shadow;
            const float a = saturate(r.accent ? r.alpha * 0.35f : r.alpha * 0.9f);
            glUniform4f(ugl::U(progPanel_, "uColor"), c.x * a, c.y * a, c.z * a, a);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }
    const vec3 inkS = ugl::toSrgb(ink), accS = ugl::toSrgb(acc);
    const float s = height_ / 1080.f;
    std::vector<float> q;
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        auto putText = [&](const std::string& text, float size, float track, float dy, const vec3& c, float a, float bias) {
            q.clear();
            const float w = textWidth(text, size, track);
            textQuads(text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
            drawText(q, shadow, a * 0.85f, 0.32f, 0.36f);
            drawText(q, c, a, bias, 0.f);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string name = h.text, var;
            const size_t dot = name.find("\xC2\xB7");
            if (dot != std::string::npos) { var = name.substr(dot + 2); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            const float sz = base * 0.9f, tr = base * 0.4f, vsz = base * 0.68f, vtr = base * 0.3f;
            const float wName = textWidth(name, sz, tr), gap = base * 0.9f;
            const float wVar = var.empty() ? 0.f : textWidth(var, vsz, vtr);
            const float x = ugl::hudLeft(h, wName + (var.empty() ? 0.f : gap + wVar), width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            q.clear();
            textQuads(name, x, h.y, sz, tr, q);
            drawText(q, shadow, a * 0.85f, 0.32f, 0.36f);
            drawText(q, accS, a, 0.03f, 0.f);
            if (!var.empty()) {
                q.clear();
                textQuads(var, x + wName + gap, h.y + (sz - vsz) * 0.75f, vsz, vtr, q);
                drawText(q, shadow, a * 0.8f, 0.32f, 0.36f);
                drawText(q, inkS, a * 0.75f, 0.02f, 0.f);
            }
            continue;
        }
        case HUD_TITLE: putText(h.text, base * 1.0f, base * 0.04f, 0.f, inkS, saturate(h.alpha * 1.3f), 0.03f); continue;
        case HUD_INFO: putText(h.text, base * 0.74f, base * 0.14f, 0.f, inkS, alpha * 0.65f, 0.01f); continue;
        case HUD_SECTION: putText(h.text, base * 0.9f, base * 0.45f, 0.f, accS, saturate(0.2f + h.alpha * 1.9f), 0.04f); continue;
        case HUD_LABEL: putText(h.text, base * 0.64f, base * 0.42f, 0.f, inkS, alpha * 0.85f, 0.04f); continue;
        case HUD_VALUE: putText(h.text, base * 1.05f, base * 0.06f, 0.f, inkS, alpha * 0.96f, 0.03f); continue;
        case HUD_NOTICE: putText(h.text, base * 0.95f, base * 0.16f, 0.f, accS, alpha, 0.03f); continue;
        case HUD_TOAST: putText(h.text, base * 1.2f, base * 0.36f, -base * 0.1f, accS, alpha, 0.05f); continue;
        case HUD_TOAST_SUB: putText(h.text, base * 0.82f, base * 0.14f, 0.f, inkS, alpha, 0.02f); continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) putText(h.text, base * 1.05f, base * 0.2f, -base * 0.1f, inkS, alpha, 0.03f);
            else putText(h.text, base * 0.8f, base * 0.06f, 0.f, h.accent ? accS : inkS, alpha, 0.02f);
            continue;
        default: putText(h.text, base * 0.8f, base * 0.05f, 0.f, h.accent ? accS : inkS, alpha, 0.02f); continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void CityUniverse::drawInst(const std::vector<Inst>& v) {
    if (v.empty()) return;
    glBindVertexArray(instVao_);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Inst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

void CityUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const VariantDef& d = VARIANTS[variant_];
    if (builtFor_ != variant_) buildCity(variant_);
    {
        const float k = palSet_ ? 1.f - std::exp(-dt * 1.2f) : 1.f;
        palSet_ = true;
        auto ease = [&](vec3& a, unsigned v) { a = lerp(a, lin(v), k); };
        auto easef = [&](float& a, float v) { a = lerpf(a, v, k); };
        ease(pal_.top, d.top); ease(pal_.mid, d.mid); ease(pal_.hor, d.hor); ease(pal_.glow, d.glow); ease(pal_.fog, d.fog);
        ease(pal_.far, d.far); ease(pal_.midB, d.midB); ease(pal_.nearB, d.nearB); ease(pal_.frame, d.frame);
        ease(pal_.glass, d.glass); ease(pal_.reflTop, d.reflTop); ease(pal_.reflBot, d.reflBot); ease(pal_.city, d.city);
        ease(pal_.accent, d.accent); ease(pal_.text, d.text); ease(pal_.shade, d.shade); ease(pal_.lobby, d.lobby);
        for (int i = 0; i < 7; i++) ease(pal_.piece[i], d.piece[i]);
        easef(pal_.mull, d.mull); easef(pal_.spand, d.spand); easef(pal_.recess, d.recess); easef(pal_.reflK, d.reflK);
        easef(pal_.stars, d.stars); easef(pal_.fogK, d.fogK); easef(pal_.rain, d.rain); easef(pal_.snow, d.snow);
    }
    // the section, from the HUD (a drop, or any other peak: a chorus)
    sectionKnown_ = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown_ = true;
            sectionDrop_ = h.text == "DROP";
        }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    const float bridge = saturate(bridge_.amount);

    if (!f.paused) {
        time_ += dt;
        world_ += wdt;
        if (lvT_ < 1.f) {
            lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
            lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
        }
        dropT_ += dt;
        tetrisT_ += dt;
        overT_ += dt;
        refreshT_ += dt;
        dropWait_ = std::max(0.f, dropWait_ - dt);
        const bool dropish = sectionKnown_ ? sectionDrop_
                                           : (music.hasSong ? music.speed > 1.33f || (inDrop_ && music.speed > 1.26f) : inDrop_);
        const bool hot = level_ >= 2 && dropWait_ <= 0.f && dropish;
        drop_ = hot ? std::min(1.f, drop_ + dt / 0.25f) : std::max(0.f, drop_ - dt / 2.0f);
        chorus_ = approach(chorus_, level_ >= 2 && !hot ? 1.f : 0.f, 1.2f, dt);
        // the level meters: the music's bands, smoothed (no flicker)
        for (int i = 0; i < 8; i++) {
            const float v = 0.5f * (music.bandsFast[i * 2] + music.bandsFast[i * 2 + 1]);
            eq_[i] = approach(eq_[i], 0.25f + 0.7f * saturate(v * 1.1f), 5.f, dt);
        }
        // the patterns: a new one every two bars, a step every two beats
        if (music.hasSong) {
            patEpoch_ = (float)std::floor(music.beat / 8.0);
            patPhase_ = (float)std::fmod(std::floor(music.beat / 2.0), 4.0);
        } else {
            patEpoch_ = std::floor(world_ / 8.f);
            patPhase_ = std::fmod(std::floor(world_ / 2.f), 4.f);
        }
        for (Contact& c : contacts_) c.t += dt;
        contacts_.erase(std::remove_if(contacts_.begin(), contacts_.end(), [](const Contact& c) { return c.t >= c.life; }), contacts_.end());
        for (Particle& p : sparks_) {
            if (p.delay > 0.f) { p.delay -= dt; continue; }
            p.t += dt;
            p.v.y -= 1.6f * dt;
            const float dr = std::exp(-1.1f * dt);
            p.v.x *= dr; p.v.y *= dr;
            p.p.x += p.v.x * dt; p.p.y += p.v.y * dt;
        }
        sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Particle& p) { return p.delay <= 0.f && p.t >= p.life; }), sparks_.end());
        gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 2.5f, dt);
        updateWindows(game, dt);
    }

    // ---- The staging.
    const float lvm = smoothstepf(0.f, 1.f, lv_), lvp = smoothstepf(1.f, 2.f, lv_);
    const float worldOn = (1.f - smoothstepf(0.f, 0.8f, bridge)) * (1.f - 0.35f * dimT_);
    const float swell = inDrop_ && dropT_ < 12.f ? smoothstepf(0.f, 0.15f, dropT_) * std::exp(-std::max(0.f, dropT_ - 0.15f) / 2.5f) * dropAmp_ : 0.f;
    const float flash = dropT_ < 3.f ? std::exp(-dropT_ / 0.5f) * dropAmp_ : 0.f;
    const float tet = tetrisT_ < 6.f ? smoothstepf(0.f, 0.2f, tetrisT_) * (1.f - smoothstepf(1.5f, 3.f, tetrisT_)) : 0.f;
    // the blackout (game over): the city goes dark, then wakes up again
    const float black = overT_ < 9.f ? smoothstepf(0.2f, 1.8f, overT_) * (1.f - smoothstepf(4.5f, 8.f, overT_)) : 0.f;
    const float peakK = std::max(chorus_ * lvp, drop_);
    const float kickK = music.kick * lvp; // (small lights only: the beacons, the crowns)
    const float crown = std::max(0.f, 0.55f * chorus_ * lvp + 1.1f * drop_ + 0.6f * tet) * worldOn * (1.f - black);
    const vec3 crownCol = lerp(pal_.accent, dropA_, smoothstepf(0.f, 0.5f, drop_)) * (1.f + 0.5f * kickK * drop_ + 1.2f * flash);
    const float litFrac = (lerpf(0.09f, 0.22f, lvm) + 0.14f * chorus_ * lvp) * lerpf(1.f, 0.45f, slow_);
    const float cityTime = world_ + cityShift_;

    // ---- The sky.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    sceneT_.bind();
    const vec3 slowSky = lin(0x0a1a48);
    vec3 top = pal_.top, mid = pal_.mid, hor = pal_.hor;
    if (slow_ > 0.001f) {
        // slow motion, a long exposure: a colder, deeper sky
        const float k = 0.75f * slow_;
        top = lerp(top, slowSky * 0.5f, k);
        mid = lerp(mid, slowSky, k);
        hor = lerp(hor, slowSky * 2.f, k);
    }
    glUseProgram(progSky_);
    set4f(progSky_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progSky_, "uTop", top);
    set3f(progSky_, "uMid", mid);
    set3f(progSky_, "uHor", hor);
    const vec3 glowCol = lerp(pal_.glow, (dropA_ + dropB_) * 0.5f, 0.6f * smoothstepf(0.f, 0.5f, drop_));
    set3f(progSky_, "uGlowCol", glowCol);
    set1f(progSky_, "uGlow", d.glowK * (lerpf(0.5f, 0.8f, lvm) + 0.25f * peakK + 0.5f * swell) * (1.f - 0.6f * black) * (1.f - 0.4f * slow_));
    set3f(progSky_, "uFogCol", pal_.fog);
    set1f(progSky_, "uFog", pal_.fogK * (1.f + 0.4f * (1.f - lvm)));
    set1f(progSky_, "uHorY", HORIZON_Y);
    set1f(progSky_, "uStars", pal_.stars * (1.f - 0.5f * peakK));
    set1f(progSky_, "uTime", world_);
    {
        // the searchlights: two in a chorus, four in a drop, swaying slowly behind the skyline
        float beam[16] = {}, bcol[12] = {};
        const float xs[4] = {-14.f, 14.5f, -19.f, 18.5f};
        const float periods[4] = {13.f, 17.f, 11.f, 15.f};
        for (int i = 0; i < 4; i++) {
            const float on = (i < 2 ? std::max(chorus_ * lvp, drop_) : drop_) * worldOn * (1.f - black);
            if (on < 0.002f) continue;
            const float sgn = xs[i] < 0.f ? 1.f : -1.f;
            beam[i * 4 + 0] = xs[i];
            beam[i * 4 + 1] = sgn * (0.25f + 0.3f * std::sin(world_ * TAU / periods[i] + i * 1.7f));
            beam[i * 4 + 2] = 0.55f;
            beam[i * 4 + 3] = on * (0.18f + 0.1f * swell);
            vec3 c = drop_ > 0.01f ? lerp(pal_.accent, (i % 2) ? dropB_ : dropA_, smoothstepf(0.f, 0.6f, drop_)) : pal_.accent;
            c = c * (0.6f / std::max(0.05f, ugl::luma(c)));
            c = lerp(c, vec3(1.f), 0.35f);
            bcol[i * 3] = c.x; bcol[i * 3 + 1] = c.y; bcol[i * 3 + 2] = c.z;
        }
        glUniform4fv(ugl::U(progSky_, "uBeam"), 4, beam);
        glUniform3fv(ugl::U(progSky_, "uBeamCol"), 4, bcol);
    }
    set1f(progSky_, "uBridge", smoothstepf(0.f, 1.f, bridge));
    set3f(progSky_, "uBridgeColor", bridge_.color);
    ugl::drawFullscreen();

    // ---- The buildings, from the back.
    glUseProgram(progBldg_);
    setMat(progBldg_, "uVP", vp_);
    set3f(progBldg_, "uHaze", lerp(pal_.fog, pal_.hor, 0.4f));
    set1f(progBldg_, "uTime", cityTime);
    set1f(progBldg_, "uDropK", smoothstepf(0.f, 0.4f, drop_) * (1.f - black));
    set1f(progBldg_, "uPatEpoch", patEpoch_);
    set1f(progBldg_, "uPatPhase", patPhase_);
    set1f(progBldg_, "uBlack", black);
    set1f(progBldg_, "uFlash", flash);
    set1f(progBldg_, "uSnow", pal_.snow);
    set3f(progBldg_, "uSnowCol", lin(0xdfe6f4) * 0.55f);
    set1f(progBldg_, "uStreetY", STREET_Y);
    glUniform1fv(ugl::U(progBldg_, "uEq"), 8, eq_);
    set3f(progBldg_, "uDropA", dropA_);
    set3f(progBldg_, "uDropB", dropB_);
    set3f(progBldg_, "uCrownCol", crownCol);
    set1f(progBldg_, "uBridge", smoothstepf(0.f, 1.f, bridge));
    set3f(progBldg_, "uBridgeColor", bridge_.color);
    const float hazeK[3] = {0.06f + 0.25f * pal_.fogK, 0.2f + 0.45f * pal_.fogK, 0.42f + 0.45f * pal_.fogK};
    // (slow motion: the city's lights turn cold, a time stretch)
    const vec3 cityLight = lerp(pal_.city, vec3(0.45f, 0.7f, 1.f), 0.7f * slow_) * (worldOn * lerpf(1.f, 1.4f, peakK));
    auto bldgInst = [&](const Bldg& b) {
        const vec3 body = b.layer == 0 ? pal_.nearB : (b.layer == 1 ? pal_.midB : pal_.far);
        Inst in{{b.x0, b.y0, b.x1, b.y1},
                {body.x, body.y, body.z, std::min(0.92f, hazeK[b.layer] * (1.f + 0.3f * (1.f - lvm) * pal_.fogK))},
                {cityLight.x, cityLight.y, cityLight.z, litFrac * b.lit},
                {b.seed, (float)b.style, (float)b.layer, b.crown * crown}};
        inst_.push_back(in);
    };
    auto solid = [&](float x0, float y0, float x1, float y1, const vec3& c, float haze) {
        Inst in{{x0, y0, x1, y1}, {c.x, c.y, c.z, haze}, {0.f, 0.f, 0.f, 0.f}, {0.f, 3.f, 0.f, 0.f}};
        inst_.push_back(in);
    };
    inst_.clear();
    for (int layer = 2; layer >= 1; layer--)
        for (const Bldg& b : bldgs_)
            if (b.layer == layer) bldgInst(b);
    // the highway's deck, low between the buildings
    solid(-23.f, HIGHWAY_Y - 0.32f, 23.f, HIGHWAY_Y - 0.1f, pal_.nearB * 0.9f, hazeK[1] * 0.6f);
    drawInst(inst_);

    // ---- The traffic (behind the near buildings): headlights one way, tail lights the other; in slow motion, a
    // long exposure (light trails).
    dots_.clear();
    auto dot = [&](float x, float y, float r, float len, const vec3& c, float a, float dx, float dy, float kind) {
        Inst in{{x, y, r, len}, {c.x, c.y, c.z, a}, {dx, dy, kind, 0.f}, {0.f, 0.f, 0.f, 0.f}};
        dots_.push_back(in);
    };
    {
        const float traffic = lerpf(0.25f, 1.f, lvm) * worldOn * (1.f - black);
        const float trail = lerpf(0.25f, 4.f, slow_);
        for (int i = 0; i < 36; i++) {
            const float h = hashf(i * 3.17f + 0.5f);
            if (h > lerpf(0.35f, 1.f, lvm)) continue; // fewer cars in a break
            const bool head = i % 2 == 0;
            const float speed = (head ? 1.f : -1.f) * (2.6f + 1.6f * hashf(i * 1.3f));
            float x = std::fmod(h * 46.f + speed * world_, 46.f);
            if (x < 0.f) x += 46.f;
            x -= 23.f;
            const float y = HIGHWAY_Y + (head ? -0.04f : 0.1f);
            const vec3 c = head ? vec3(1.f, 0.92f, 0.8f) : vec3(1.f, 0.1f, 0.05f);
            dot(x, y, 0.05f, trail, c, traffic * (head ? 0.9f : 0.7f), speed, 0.f, 1.f);
        }
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(progDot_);
    setMat(progDot_, "uVP", vp_);
    drawInst(dots_);
    glDisable(GL_BLEND);

    // ---- The near buildings, the crane.
    inst_.clear();
    for (const Bldg& b : bldgs_)
        if (b.layer == 0) bldgInst(b);
    {
        // the crane on the tall building at the left, its jib turning very slowly
        const Bldg* host = nullptr;
        for (const Bldg& b : bldgs_)
            if (b.layer == 0 && b.beacon && (!host || b.x0 > host->x0)) host = &b;
        if (host) {
            const vec3 c = pal_.nearB * 1.15f;
            const float mx = host->x0 + (host->x1 - host->x0) * 0.62f, base = host->y1;
            solid(mx - 0.07f, base, mx + 0.07f, base + 3.2f, c, 0.f);
            const float turn = std::cos(world_ * TAU / 150.f + 2.f);
            const float jl = 4.4f * (0.55f + 0.45f * std::fabs(turn)), cl = 1.5f * (0.55f + 0.45f * std::fabs(turn));
            const float sgn = turn >= 0.f ? 1.f : -1.f;
            const float jy = base + 2.9f;
            solid(std::min(mx, mx + sgn * jl), jy, std::max(mx, mx + sgn * jl), jy + 0.09f, c, 0.f);
            solid(std::min(mx, mx - sgn * cl), jy, std::max(mx, mx - sgn * cl), jy + 0.12f, c, 0.f);
            solid(mx - sgn * cl * 0.8f - 0.18f, jy - 0.28f, mx - sgn * cl * 0.8f + 0.18f, jy, c * 0.9f, 0.f);
            solid(mx - 0.04f, jy, mx + 0.04f, jy + 0.7f, c, 0.f);
            const float blink = 0.55f + 0.45f * (std::fmod(world_ * 0.5f, 1.f) < 0.5f ? 1.f : 0.f);
            dot(mx + sgn * jl, jy + 0.06f, 0.07f, 0.f, BEACON_RED * 2.f, blink * worldOn, 1.f, 0.f, 3.f);
        }
    }
    glUseProgram(progBldg_);
    drawInst(inst_);

    // ---- The weather, behind the tower: rain, snow.
    {
        dots_.clear();
        const float wv = worldOn * (pal_.rain > 0.01f ? pal_.rain : pal_.snow);
        if (pal_.rain > 0.01f) {
            const vec3 rc = lerp(pal_.city, pal_.accent, 0.3f) * 0.07f;
            for (int i = 0; i < 160; i++) {
                const Flake& fl = flakes_[i];
                float y = std::fmod(fl.p.y + 12.f - world_ * 24.f * fl.s, 25.f);
                if (y < 0.f) y += 25.f;
                y -= 12.f;
                const float x = fl.p.x + (y + 12.f) * 0.12f;
                dot(x, y, 0.018f, 0.9f, rc, wv * (0.5f + 0.5f * fl.s), 0.12f, -1.f, 0.f);
            }
        }
        if (pal_.snow > 0.01f) {
            const vec3 sc = lin(0xe8eeff) * 0.4f;
            for (const Flake& fl : flakes_) {
                float y = std::fmod(fl.p.y + 12.f - world_ * 0.7f * fl.s, 25.f);
                if (y < 0.f) y += 25.f;
                y -= 12.f;
                const float x = fl.p.x + 0.5f * std::sin(world_ * 0.4f * fl.s + fl.ph);
                dot(x, y, 0.035f + 0.025f * fl.s, 0.f, sc, wv * (0.4f + 0.4f * fl.s), 1.f, 0.f, 0.f);
            }
        }
        if (!dots_.empty()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glUseProgram(progDot_);
            drawInst(dots_);
            glDisable(GL_BLEND);
        }
    }

    // ---- The tower: the windows' light into the two textures, then the facade.
    {
        std::vector<float> a(ROWS * COLS * 4), b(ROWS * COLS * 4);
        const float bright = (1.f + 0.12f * peakK) * worldOn * (1.f - 0.25f * slow_);
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++) {
                const Win& w = win_[r][c];
                const int i = (r * COLS + c) * 4;
                float lv = w.level * (w.falling ? 1.55f : 1.f) * (1.f + 0.6f * w.surge);
                if (w.dip > 0.f) lv *= 1.f - 0.6f * w.dip * (0.6f + 0.4f * std::sin(time_ * 90.f + c));
                const vec3 col = w.col * (lv * bright);
                a[i] = col.x; a[i + 1] = col.y; a[i + 2] = col.z;
                a[i + 3] = (w.level > 0.003f && w.style >= 0) ? (float)(w.style + 1) : 0.f;
                b[i] = w.ghost * worldOn;
                b[i + 1] = w.falling ? 1.f : 0.f;
                b[i + 2] = w.seed;
                b[i + 3] = 0.f;
            }
        glBindTexture(GL_TEXTURE_2D, cellA_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, COLS, ROWS, GL_RGBA, GL_FLOAT, a.data());
        glBindTexture(GL_TEXTURE_2D, cellB_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, COLS, ROWS, GL_RGBA, GL_FLOAT, b.data());
    }
    glUseProgram(progTower_);
    setMat(progTower_, "uVP", vp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cellA_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, cellB_);
    glActiveTexture(GL_TEXTURE0);
    set1i(progTower_, "uCellA", 0);
    set1i(progTower_, "uCellB", 1);
    set3f(progTower_, "uFrame", pal_.frame * (1.f - 0.5f * black));
    set3f(progTower_, "uGlass", pal_.glass);
    set3f(progTower_, "uReflTop", lerp(pal_.reflTop, mid, 0.3f * slow_));
    set3f(progTower_, "uReflBot", pal_.reflBot);
    set3f(progTower_, "uReflLights", pal_.city * (0.12f * (1.f - black)) + crownCol * (0.05f * crown));
    {
        vec3 gc = lin(d.piece[0]);
        if (game.hasPiece()) gc = lin(d.piece[std::clamp(game.piece().type, 0, 6)]);
        set3f(progTower_, "uGhostCol", gc * 0.9f);
    }
    set3f(progTower_, "uPierCol", crownCol);
    set3f(progTower_, "uCrownCol", crownCol);
    set3f(progTower_, "uLobby", pal_.lobby * (0.14f * worldOn * (1.f - black)));
    set3f(progTower_, "uSnowCol", lin(0xdfe6f4) * 0.5f);
    set1f(progTower_, "uMull", pal_.mull);
    set1f(progTower_, "uSpand", pal_.spand);
    set1f(progTower_, "uRecess", pal_.recess);
    set1f(progTower_, "uReflK", pal_.reflK * (1.f - 0.4f * black));
    set1f(progTower_, "uSpill", 0.22f);
    set1f(progTower_, "uPier", (0.5f * chorus_ * lvp + 0.9f * drop_ + 0.4f * kickK * drop_) * worldOn * (1.f - black));
    set1f(progTower_, "uCrown", (0.25f + crown) * worldOn * (1.f - black));
    set1f(progTower_, "uSnow", pal_.snow);
    set1f(progTower_, "uRain", pal_.rain);
    set1f(progTower_, "uTime", world_);
    set1f(progTower_, "uChase", tet * worldOn);
    {
        float cc[21];
        for (int i = 0; i < 7; i++) { cc[i * 3] = pal_.piece[i].x; cc[i * 3 + 1] = pal_.piece[i].y; cc[i * 3 + 2] = pal_.piece[i].z; }
        glUniform3fv(ugl::U(progTower_, "uChaseCol"), 7, cc);
        float ct[16] = {}, ccol[12] = {};
        for (size_t i = 0; i < contacts_.size() && i < 4; i++) {
            const Contact& c = contacts_[i];
            const float u = c.t / c.life;
            ct[i * 4] = c.x0; ct[i * 4 + 1] = c.x1; ct[i * 4 + 2] = c.y;
            ct[i * 4 + 3] = c.power * smoothstepf(0.f, 0.1f, u) * (1.f - u) * (1.f - u) * worldOn * 0.9f;
            ccol[i * 3] = c.col.x; ccol[i * 3 + 1] = c.col.y; ccol[i * 3 + 2] = c.col.z;
        }
        glUniform4fv(ugl::U(progTower_, "uContact"), 4, ct);
        glUniform3fv(ugl::U(progTower_, "uContactCol"), 4, ccol);
    }
    set1f(progTower_, "uBridge", smoothstepf(0.f, 1.f, bridge));
    set3f(progTower_, "uBridgeColor", bridge_.color);
    set3f(progTower_, "uBridgeInk", bridge_.ink);
    inst_.clear();
    inst_.push_back(Inst{{-5.5f, -12.5f, 5.5f, 13.6f}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}});
    drawInst(inst_);

    // ---- The previews: small lit windows, the next one brightest.
    wins_.clear();
    auto preview = [&](int type, vec2 center, float sc, float level) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        game.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        const vec3 L = pal_.piece[type] * (level * worldOn);
        for (int i = 0; i < 4; i++) {
            const vec2 pos = v2(center.x + (cells[i][0] - (minx + maxx) * 0.5f) * sc, center.y - (cells[i][1] - (miny + maxy) * 0.5f) * sc);
            const float h = sc * 0.5f;
            wins_.push_back(Inst{{pos.x - h, pos.y - h, pos.x + h, pos.y + h}, {L.x, L.y, L.z, (float)type},
                                 {hashf(pos.x * 3.f + pos.y), 1.f, 0.f, 0.f}, {0, 0, 0, 0}});
        }
    };
    preview(game.holdType(), v2(layout::HOLD_CENTER.x, layout::HOLD_CENTER.y), layout::previewScale(0), game.holdUsed() ? 0.35f : 0.9f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(game.next(i), v2(c.x, c.y), layout::previewScale(i), i == 0 ? 1.1f : 0.8f);
    }
    if (!wins_.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(progWin_);
        setMat(progWin_, "uVP", vp_);
        set3f(progWin_, "uFrame", pal_.frame * 0.8f);
        set1f(progWin_, "uTime", world_);
        set1f(progWin_, "uBridge", smoothstepf(0.f, 1.f, bridge));
        set3f(progWin_, "uBridgeInk", bridge_.ink);
        drawInst(wins_);
        glDisable(GL_BLEND);
    }

    // ---- The lights: the windows' glow in the night, the beacons, the fireworks, the gauge.
    dots_.clear();
    {
        // the lit windows glow a little into the night (more in slow motion: a long exposure)
        const float gk = (0.05f + 0.06f * slow_) * worldOn;
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++) {
                const Win& w = win_[r][c];
                if (w.level < 0.01f) continue;
                const float lv = w.level * (w.falling ? 1.8f : 1.f) * (1.f + 0.8f * w.surge);
                dot(c - 4.5f, 9.5f - r, 0.42f + 0.2f * slow_, 0.f, w.col, gk * lv, 1.f, 0.f, 0.f);
            }
        for (const Inst& wi : wins_) {
            const vec3 L(wi.a1[0], wi.a1[1], wi.a1[2]);
            dot((wi.a0[0] + wi.a0[2]) * 0.5f, (wi.a0[1] + wi.a0[3]) * 0.5f, 0.3f, 0.f, L, 0.05f, 1.f, 0.f, 0.f);
        }
    }
    {
        // the beacons: blinking on their own in a break, every two beats on the beat grid from mid, hitting with
        // the kicks at a peak; a long exposure leaves them as soft smears
        std::vector<vec2> bs;
        for (const Bldg& b : bldgs_)
            if (b.beacon) bs.push_back(v2((b.x0 + b.x1) * 0.5f, b.y1 + 0.08f));
        bs.push_back(v2(-5.38f, 11.18f));
        bs.push_back(v2(5.38f, 11.18f));
        bs.push_back(v2(2.6f, 12.1f));
        const bool grid = music.hasSong && lv_ > 0.5f;
        for (size_t i = 0; i < bs.size(); i++) {
            float on;
            if (grid) {
                const float ph = (float)std::fmod(music.beat / 2.0 + (i % 2) * 0.5, 1.0);
                on = std::exp(-ph * 5.f);
            } else {
                const float ph = std::fmod(world_ / 2.4f + hashf((float)i) , 1.f);
                on = ph < 0.18f ? 1.f : std::exp(-(ph - 0.18f) * 12.f);
            }
            on = std::max(on, 0.08f) * (1.f + 0.8f * kickK * drop_);
            const float far = bs[i].y > 8.f && std::fabs(bs[i].x) > 10.f ? 0.7f : 1.f;
            dot(bs[i].x, bs[i].y, 0.08f * far + 0.06f * slow_, 0.f, BEACON_RED * 2.2f, on * worldOn * (1.f - 0.8f * black) * far, 1.f, 0.f, 3.f);
        }
    }
    for (const Particle& p : sparks_) {
        if (p.delay > 0.f) continue;
        const float u = p.t / p.life;
        const float a = std::pow(1.f - u, 1.5f) * smoothstepf(0.f, 0.05f, p.t) * worldOn;
        const float sp = std::sqrt(p.v.x * p.v.x + p.v.y * p.v.y);
        dot(p.p.x, p.p.y, 0.055f, sp * 0.14f, p.c * 2.2f, a, p.v.x, p.v.y, 1.f);
    }
    {
        // the gauge: twelve small windows in a ring, lit one by one as it charges (a step per line)
        const vec3 gc = layout::GAUGE_CENTER;
        const bool ready = game.bonusReady(), active = game.bonusActive();
        const float fill = active ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const float pulse = ready && music.hasSong ? 0.75f + 0.25f * std::cos((float)std::fmod(music.beat / 2.0, 1.0) * TAU) : 1.f;
        for (int i = 0; i < 12; i++) {
            const float a = PI * 0.5f - TAU * (i + 0.5f) / 12.f;
            const float x = gc.x + std::cos(a) * layout::GAUGE_R, y = gc.y + std::sin(a) * layout::GAUGE_R;
            const float k = ready ? 1.f : saturate(fill * 12.f - i);
            const vec3 lc = lerp(pal_.text * 0.06f, pal_.accent * (active ? 1.2f : 0.9f) * pulse, k);
            dot(x, y, 0.1f, 0.f, lc, worldOn * (1.f - 0.4f * dimT_), 1.f, 0.f, 2.f);
        }
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(progDot_);
    setMat(progDot_, "uVP", vp_);
    drawInst(dots_);
    glDisable(GL_BLEND);

    // ---- The final pass, then the HUD.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    const float fade = (1.f - 0.5f * dimT_) * saturate(f.fade);
    set1f(progFinal_, "uMul", fade);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(f.hud, f.panels, fade);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_city() {
    static const UniverseEntry e{
        "CITY", "a tower's facade at night, its lit windows the blocks, a skyline around it",
        [] { return std::unique_ptr<Universe>(new CityUniverse()); },
        // Busy, driving songs suit it best (a city at night), any tempo.
        [](const SongLook& l) { return 0.75f + 0.25f * l.density + 0.15f * (1.f - l.brightness); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
