// ANTS: sugar cubes on the ground, and a colony finding them.
//
// A plain ground seen from straight above. The well is a hairline frame on it and the pieces are sugar cubes, each
// cell a small cube of tinted rock sugar (crystals catching the light). Two nests open at the edges of the frame, one
// on the left, one on the right, half out of the picture: a dark hole in a ring of dug-out pellets.
//
// The ants are a real (small) colony: they leave the nests and wander; the ones that touch a cube bite a crumb off
// it and carry it home, laying a pheromone on the way; the others follow the pheromone, so trails form between the
// nests and the stack, and they show as worn paths on the ground. The cubes get nibbled (a few bites at their
// corners and edges) but never eaten away.
//
// A cleared row's cubes crumble grain by grain and burst into loose grains; the ones thrown out of the well stay on
// the ground until ants fetch them. A drop pours the colony out of both nests: twice as many ants, much faster,
// surging on the kicks, the trails darker, and the sugar turns to vivid candy colors. A chorus is a busy colony, a
// break a few slow ants on fading trails. A lock scatters the ants around the impact; slow motion slows the colony
// at dusk; a game over crumbles the whole stack.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/ants/ants.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/ants/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// EB Garamond Italic (the song, the section, the celebrations) and Manrope Light (labels and numbers), SIL Open Font
// License, see OFL-*.txt beside this file.
const unsigned char FONT_SERIF[] = {
#include "universes/ants/EBGaramond12-Italic.ttf.inc"
};
const unsigned char FONT_SANS[] = {
#include "universes/ants/Manrope-Light.ttf.inc"
};

namespace sh = ants_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, sh::COMMON_GLSL);
    return ugl::program(vs, f.c_str(), name);
}

vec3 hex(uint32_t h) { return vec3(((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f); }
vec2 v2(float x, float y) { return vec2{x, y}; }
vec2 operator+(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 operator*(vec2 a, float k) { return {a.x * k, a.y * k}; }
float len(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
float len2(vec2 a) { return a.x * a.x + a.y * a.y; }
vec2 dirOf(float a) { return {std::cos(a), std::sin(a)}; }
float wrapAngle(float a) {
    while (a > PI) a -= TAU;
    while (a < -PI) a += TAU;
    return a;
}

// The grounds. Colors are sRGB (the whole frame is drawn in display colors). Each has its own seven sugars, and the
// drop's candy colors.
struct Variant {
    const char* name;
    bool light;
    uint32_t ground, mottle, trail, hole, mound, ant, ink, dim, accent, frame, slow;
    float frameA, grain, trailAmt, shadow, lustre;
    uint32_t piece[7], drop[7];
};
const Variant VARIANTS[] = {
    // Daylight on a pale linen-grey ground: black ants, rock-candy sugar.
    {"LINEN", true, 0xe9e8e4, 0xe1dfda, 0xc9c5bd, 0x2f2925, 0xcfc9bf, 0x1a1411, 0x2b2724, 0x8b857e, 0xd0482e,
     0x8f8a84, 0xc9d0dc, 0.32f, 0.03f, 0.75f, 0.2f, 0.16f,
     {0x3db4da, 0xf0b22c, 0x9a69d6, 0x55bd70, 0xe85a58, 0x4470df, 0xf0873a},
     {0x00c9ef, 0xffc800, 0xc04dff, 0x10d67a, 0xff3466, 0x2f55ff, 0xff6812}},
    // Night on dark slate: the ants darker still, the worn paths pale, pastel sugar glowing.
    {"NIGHT", false, 0x2b323c, 0x2f3641, 0x45515e, 0x0a0c0f, 0x363e49, 0x050607, 0xdfe3ea, 0x7b8693, 0x9ad0ff,
     0x8494a6, 0x161d2c, 0.22f, 0.025f, 0.7f, 0.5f, 0.22f,
     {0x8fd6ef, 0xffd68a, 0xc4a6f2, 0x9fe0ad, 0xf59c9c, 0x98b2f7, 0xffbb88},
     {0x3ff0ff, 0xffe04a, 0xd77cff, 0x55ff9c, 0xff5c88, 0x6e8bff, 0xff983a}},
    // Red clay: dusty paths, near-white candy sugar.
    {"TERRACOTTA", false, 0x9a5440, 0x934f3c, 0xb7735b, 0x2a140e, 0xb06650, 0x140907, 0xf7ebe4, 0xdcb3a4, 0xfff0c8,
     0xf0d0c0, 0x6d4a52, 0.3f, 0.03f, 0.7f, 0.35f, 0.18f,
     {0x93e4f2, 0xffe08a, 0xc9a6ff, 0xa8f0b8, 0xffa0c4, 0x7fa4ff, 0xffbf86},
     {0x4ff4ff, 0xfff05c, 0xb27cff, 0x6dffa0, 0xff74b4, 0x5f8bff, 0xffa94d}},
    // Pale ash, red ants, deep sugar.
    {"ASH", true, 0xd5d7d8, 0xcdd0d2, 0xbcc0c3, 0x2c2d2f, 0xbfc2c5, 0x8e2414, 0x2a2c2f, 0x80858a, 0xb3321f,
     0x7d8287, 0xbcc6d6, 0.34f, 0.03f, 0.75f, 0.22f, 0.2f,
     {0x1f7894, 0xd09a1c, 0x5d3c96, 0x2f8552, 0xb8333d, 0x2b4ea3, 0xcc5a20},
     {0x00a8cc, 0xffb300, 0x8a3df0, 0x00b45c, 0xf0263f, 0x1f4dff, 0xff6200}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float ANT_L = 0.3f;    // an ant's length unit (board units; the body is ~0.8 of it)
constexpr float CUBE = 0.43f;    // a sugar cube's half size (a cell is 0.5)
constexpr float TAU_FOOD = 22.f; // pheromone evaporation (seconds)
constexpr float TAU_HOME = 9.f;
constexpr int MAX_GRAINS = 1400;

std::string titleCase(const std::string& s) {
    std::string o = s;
    bool start = true;
    for (char& c : o) {
        if (std::isalpha((unsigned char)c)) {
            c = start ? (char)std::toupper((unsigned char)c) : (char)std::tolower((unsigned char)c);
            start = false;
        } else {
            start = c == ' ' || c == '-' || c == '(';
        }
    }
    return o;
}
std::string upper(const std::string& s) {
    std::string o = s;
    for (char& c : o) c = (char)std::toupper((unsigned char)c);
    return o;
}
// UTF-8 to ASCII for the glyph set (the middle dot kept as a slot of its own).
std::string ascii(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { o += (char)c; continue; }
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB7) { o += '\x7f'; i++; continue; }
        if (c == 0xC3 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0x97) { o += 'x'; i++; continue; }
        while (i + 1 < s.size() && ((unsigned char)s[i + 1] & 0xC0) == 0x80) i++;
        o += '?';
    }
    return o;
}

} // namespace

// =====================================================================================================
// GL
// =====================================================================================================

bool AntsUniverse::init(int width, int height) {
    progGround_ = compile(ugl::FULLSCREEN_VS, sh::GROUND_FS, "ants ground");
    progSprite_ = compile(sh::SPRITE_VS, sh::SPRITE_FS, "ants sprites");
    progText_ = compile(sh::TEXT_VS, sh::TEXT_FS, "ants text");
    progShape_ = compile(sh::SHAPE_VS, sh::SHAPE_FS, "ants shapes");
    progVeil_ = compile(ugl::FULLSCREEN_VS, sh::VEIL_FS, "ants veil");

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
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Sprite), (void*)(sizeof(float) * 4 * i));
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
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);
    glGenTextures(1, &pherTex_);
    buildFont(fontSerif_, FONT_SERIF);
    buildFont(fontSans_, FONT_SANS);
    for (auto& row : occ_)
        for (auto& c : row) c = -1;
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

AntsUniverse::~AntsUniverse() {
    for (GLuint p : {progGround_, progSprite_, progText_, progVeil_, progShape_}) glDeleteProgram(p);
    for (GLuint v : {spriteVao_, textVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, textVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
    if (pherTex_) glDeleteTextures(1, &pherTex_);
    for (Font* f : {&fontSerif_, &fontSans_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

void AntsUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = v2(hw, hh);
    viewC_ = v2(cam.target.x, cam.target.y);
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    px_ = 2.f * hh / height_;
    // the nests: on the left and right edges, half out of the picture, below the HUD columns
    nestR_ = 1.55f;
    nest_[0] = v2(viewC_.x - hw - 0.1f, -4.6f);
    nest_[1] = v2(viewC_.x + hw + 0.1f, -0.9f);
    // the pheromone fields cover the view
    const int gw = (int)std::ceil(2.f * hw / CELL) + 2, gh = (int)std::ceil(2.f * hh / CELL) + 2;
    if (gw != gw_ || gh != gh_) {
        gw_ = gw;
        gh_ = gh;
        food_.assign(gw_ * gh_, 0.f);
        home_.assign(gw_ * gh_, 0.f);
    }
    const int tw = (int)std::ceil(2.f * hw / TCELL) + 2, th = (int)std::ceil(2.f * hh / TCELL) + 2;
    if (tw != tw_ || th != th_) {
        tw_ = tw;
        th_ = th;
        trace_.assign(tw_ * th_, 0.f);
        glBindTexture(GL_TEXTURE_2D, pherTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, tw_, th_, 0, GL_RED, GL_FLOAT, trace_.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    g0_ = v2(viewC_.x - hw - CELL, viewC_.y - hh - CELL);
    t0_ = v2(viewC_.x - hw - TCELL, viewC_.y - hh - TCELL);
}

void AntsUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] ants font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = 64.f;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AW = 1024, AH = 1024, PAD = 10;
    std::vector<unsigned char> atlas(AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    for (int slot = 0; slot < 96; slot++) {
        const int ch = slot == 95 ? 0xB7 : 32 + slot; // slot 95 (0x7f): the middle dot
        Glyph& g = f.g[slot];
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

// =====================================================================================================
// Song, staging, events
// =====================================================================================================

void AntsUniverse::setSong(const SongLook& t, float seconds) {
    int v = 0;
    if (t.variant >= 0) {
        v = t.variant % VARIANT_COUNT;
    } else if (t.hasSong) {
        const int r = (int)((t.seed >> 9) % 100);
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.minor && t.brightness < 0.45f) v = r < 65 ? 1 : 2;      // dark songs: night, or the clay
        else if (!t.minor && t.brightness > 0.58f) v = r < 60 ? 0 : 3; // bright ones: daylight, or ash
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % (VARIANT_COUNT - 1))) % VARIANT_COUNT;
    }
    variant_ = v;
    rng_ = Rng(t.seed ^ 0xA175A175ull);
    if (seconds <= 0.f) warm_ = false;
}

void AntsUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)impact;
    level_ = std::clamp(level, 0, 2);
    energy_ = energy;
    lvTarget_ = (float)level_;
    if (level_ < 2) inDrop_ = false;
    if (seconds <= 0.1f) lv_ = lvTarget_;
}

// The drop's first downbeat: the colony pours out of both nests, the sugar turns to candy, the trails darken.
void AntsUniverse::drop(float strength) {
    if (level_ < 2) return;
    inDrop_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * saturate(strength));
    const int n = 45 + (int)(45 * saturate(strength));
    pour(0, n);
    pour(1, n);
}

void AntsUniverse::refresh(float strength) {
    const int n = 4 + (int)(10 * saturate(strength));
    pour(0, n);
    pour(1, n);
}

vec2 AntsUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints AntsUniverse::hints() const {
    const Variant& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = ugl::toLinear(hex(V.ground));
    h.ink = ugl::toLinear(V.light ? hex(V.ink) * 0.9f + hex(V.ground) * 0.1f : hex(V.piece[0]));
    h.accent = ugl::toLinear(hex(V.accent));
    h.light = V.light;
    h.styles = TS_INK | TS_LUMA | TS_REBUILD;
    return h;
}

vec3 AntsUniverse::pieceColor(int type) const {
    const Variant& V = VARIANTS[variant_];
    type = std::clamp(type, 0, 6);
    return lerp(hex(V.piece[type]), hex(V.drop[type]), dropMix_);
}

// Loose grains of a cube: thrown out mostly sideways, along the row (the ones that land outside the well stay there
// for the ants; inside the well they melt away at once).
void AntsUniverse::burst(float x, float y, int type, int n, float speedK, bool tetris) {
    const vec3 c = pieceColor(type);
    for (int k = 0; k < n && (int)grains_.size() < MAX_GRAINS; k++) {
        Grain g;
        g.p = v2(x + rng_.range(-0.35f, 0.35f), y + rng_.range(-0.35f, 0.35f));
        float side = x + rng_.range(-3.f, 3.f) > 0.f ? 1.f : -1.f;
        const float a = (side > 0 ? 0.f : PI) + rng_.range(-0.75f, 0.75f) + (tetris ? rng_.range(-0.6f, 0.6f) : 0.f);
        const float sp = rng_.range(0.6f, 3.4f) * speedK * (rng_.chance(0.25f) ? 2.2f : 1.f);
        g.v = dirOf(a) * sp;
        g.col = c * rng_.range(0.9f, 1.08f);
        g.size = rng_.range(0.035f, 0.07f);
        g.rot = rng_.range(0.f, TAU);
        g.spin = rng_.range(-9.f, 9.f);
        g.life = rng_.range(0.45f, 0.8f);
        g.delay = rng_.range(0.f, 0.18f);
        grains_.push_back(g);
    }
}

void AntsUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::HardDrop:
        hard_ = true;
        break;
    case GameEvent::Lock: {
        // fresh cubes; the ants around the impact scatter, a hard drop pushes a few grains out from under it
        float minY = 1e9f, sx = 0.f;
        for (const auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            nib_[c.y][c.x] = 0.f;
            seed_[c.y][c.x] = rng_.range(1.f, 97.f);
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            minY = std::min(minY, p.y);
        }
        int nb = 0;
        for (const auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > minY + 0.1f) continue;
            sx += p.x;
            nb++;
        }
        if (nb) {
            const vec2 at = v2(sx / nb, minY - 0.5f);
            lockAt_.push_back(at);
            if (hard_) {
                const vec3 col = pieceColor(ev.cells.empty() ? 0 : ev.cells[0].type);
                const int n = 5;
                for (int k = 0; k < n; k++) {
                    Grain g;
                    const float side = k % 2 ? 1.f : -1.f;
                    g.p = v2(at.x + side * rng_.range(0.2f, nb * 0.5f), at.y + rng_.range(0.f, 0.08f));
                    g.v = v2(side * rng_.range(0.6f, 1.6f), rng_.range(-0.3f, 0.2f));
                    g.col = col * 0.95f;
                    g.size = rng_.range(0.025f, 0.045f);
                    g.rot = rng_.range(0.f, TAU);
                    g.spin = rng_.range(-6.f, 6.f);
                    g.life = rng_.range(0.35f, 0.6f);
                    grains_.push_back(g);
                }
            }
        }
        hard_ = false;
        break;
    }
    case GameEvent::Clear: {
        bool cleared[Game::H] = {};
        float sy = 0.f;
        int n = 0;
        for (const auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            cleared[c.y] = true;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            Dying d{p.x, p.y, std::clamp(c.type, 0, 6), 0.f, std::fabs(c.x - 4.5f) * 0.012f, seed_[c.y][c.x],
                    nib_[c.y][c.x], false};
            if (d.seed <= 0.f) d.seed = rng_.range(1.f, 97.f);
            dying_.push_back(d);
            burst(p.x, p.y, d.type, ev.count >= 4 ? 13 : 10, ev.count >= 4 ? 1.35f : 1.f, ev.count >= 4);
            sy += p.y;
            n++;
        }
        if (n) {
            excite_ = v2(0.f, sy / n);
            exciteT_ = ev.count >= 4 ? 5.f : 2.5f;
        }
        if (ev.count >= 4) {
            pour(0, 45);
            pour(1, 45);
        }
        // the nibbles follow the cubes down
        float nn[Game::H][Game::W] = {}, ss[Game::H][Game::W] = {};
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (cleared[y]) continue;
            for (int x = 0; x < Game::W; x++) {
                nn[dst][x] = nib_[y][x];
                ss[dst][x] = seed_[y][x];
            }
            dst--;
        }
        std::memcpy(nib_, nn, sizeof(nib_));
        std::memcpy(seed_, ss, sizeof(seed_));
        break;
    }
    case GameEvent::TopOut: {
        // the whole stack crumbles, from the top down
        int top = Game::H;
        for (const auto& c : ev.cells) top = std::min(top, c.y);
        for (const auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            Dying d{p.x, p.y, std::clamp(c.type, 0, 6), 0.f, (c.y - top) * 0.07f + rng_.range(0.f, 0.06f),
                    seed_[c.y][c.x] > 0.f ? seed_[c.y][c.x] : rng_.range(1.f, 97.f), nib_[c.y][c.x], true};
            dying_.push_back(d);
        }
        for (auto& row : nib_)
            for (float& v : row) v = 0.f;
        overT_ = 0.f;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The colony
// =====================================================================================================

bool AntsUniverse::sugarAt(vec2 p, float half, int* gx, int* gy) const {
    const int cx = (int)std::floor(p.x + 5.f), cy = (int)std::floor(12.f - p.y); // the cell under p
    for (int y = cy - 1; y <= cy + 1; y++) {
        if (y < Game::HIDDEN || y >= Game::H) continue;
        for (int x = cx - 1; x <= cx + 1; x++) {
            if (x < 0 || x >= Game::W || occ_[y][x] < 0) continue;
            const float ccx = x - 4.5f, ccy = 11.5f - y;
            if (std::fabs(p.x - ccx) < half && std::fabs(p.y - ccy) < half) {
                if (gx) *gx = x;
                if (gy) *gy = y;
                return true;
            }
        }
    }
    return false;
}

bool AntsUniverse::blocked(vec2 p) const {
    const float x0 = viewC_.x - viewH_.x + 0.15f, x1 = viewC_.x + viewH_.x - 0.15f;
    const float y0 = viewC_.y - viewH_.y + 0.15f, y1 = viewC_.y + viewH_.y - 0.15f;
    if (p.x < x0 || p.x > x1 || p.y < y0 || p.y > y1) {
        const float r = nestR_ * 1.1f;
        if (len2(p - nest_[0]) > r * r && len2(p - nest_[1]) > r * r) return true;
    }
    // the HUD columns stay quiet
    if (p.x > layout::LEFT_COLUMN[0] - 0.2f && p.x < layout::LEFT_COLUMN[2] + 0.2f && p.y > layout::LEFT_COLUMN[1] - 1.2f &&
        p.y < layout::LEFT_COLUMN[3] + 0.4f)
        return true;
    if (p.x > layout::RIGHT_COLUMN[0] - 0.2f && p.x < layout::RIGHT_COLUMN[2] + 0.2f && p.y > layout::RIGHT_COLUMN[1] - 1.6f &&
        p.y < layout::RIGHT_COLUMN[3] + 0.4f)
        return true;
    // inside the well (and the lanes beside it, between the HUD columns), only just above the stack
    if (std::fabs(p.x) < 6.f && p.y > openTop_) return true;
    // nor over the HUD columns' tops (the ants keep to the lower part of the picture between the nests' sides)
    if (p.x > layout::LEFT_COLUMN[0] - 0.2f && p.x <= -6.f && p.y > layout::LEFT_COLUMN[1] - 1.2f) return true;
    if (p.x < layout::RIGHT_COLUMN[2] + 0.2f && p.x >= 6.f && p.y > layout::RIGHT_COLUMN[1] - 1.6f) return true;
    return sugarAt(p, 0.56f);
}

float AntsUniverse::pher(const std::vector<float>& f, vec2 p) const {
    const float fx = (p.x - g0_.x) / CELL - 0.5f, fy = (p.y - g0_.y) / CELL - 0.5f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    if (ix < 0 || iy < 0 || ix + 1 >= gw_ || iy + 1 >= gh_) return 0.f;
    const float tx = fx - ix, ty = fy - iy;
    const float* r0 = &f[iy * gw_ + ix];
    const float* r1 = r0 + gw_;
    return lerpf(lerpf(r0[0], r0[1], tx), lerpf(r1[0], r1[1], tx), ty);
}

void AntsUniverse::deposit(std::vector<float>& f, vec2 p, float amount) {
    const float fx = (p.x - g0_.x) / CELL - 0.5f, fy = (p.y - g0_.y) / CELL - 0.5f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    if (ix < 0 || iy < 0 || ix + 1 >= gw_ || iy + 1 >= gh_) return;
    const float tx = fx - ix, ty = fy - iy;
    float* r0 = &f[iy * gw_ + ix];
    float* r1 = r0 + gw_;
    r0[0] = std::min(3.f, r0[0] + amount * (1 - tx) * (1 - ty));
    r0[1] = std::min(3.f, r0[1] + amount * tx * (1 - ty));
    r1[0] = std::min(3.f, r1[0] + amount * (1 - tx) * ty);
    r1[1] = std::min(3.f, r1[1] + amount * tx * ty);
}

void AntsUniverse::wear(vec2 p, float amount) {
    const float fx = (p.x - t0_.x) / TCELL - 0.5f, fy = (p.y - t0_.y) / TCELL - 0.5f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    if (ix < 0 || iy < 0 || ix + 1 >= tw_ || iy + 1 >= th_) return;
    const float tx = fx - ix, ty = fy - iy;
    float* r0 = &trace_[iy * tw_ + ix];
    float* r1 = r0 + tw_;
    r0[0] = std::min(1.5f, r0[0] + amount * (1 - tx) * (1 - ty));
    r0[1] = std::min(1.5f, r0[1] + amount * tx * (1 - ty));
    r1[0] = std::min(1.5f, r1[0] + amount * (1 - tx) * ty);
    r1[1] = std::min(1.5f, r1[1] + amount * tx * ty);
}

void AntsUniverse::spawn(int nest, bool burstOut, bool scout) {
    Ant a;
    const vec2 c = nest_[nest];
    float toward = nest == 0 ? 0.f : PI;
    if (!scout) {
        // followers leave along the strongest trail
        float best = 0.03f;
        for (int k = 0; k < 24; k++) {
            const float t = toward + (k / 23.f - 0.5f) * 2.6f;
            const float v = pher(food_, c + dirOf(t) * (nestR_ * 0.9f));
            if (v > best) { best = v; toward = t; }
        }
    }
    const float ang = toward + rng_.range(-1.f, 1.f) * (scout ? 1.2f : 0.35f);
    a.p = c + dirOf(ang) * rng_.range(0.f, nestR_ * 0.3f);
    a.a = ang + rng_.range(-0.4f, 0.4f);
    a.speed = rng_.range(0.82f, 1.18f) * (burstOut ? 1.15f : 1.f);
    a.gait = rng_.range(0.f, TAU);
    a.nest = nest;
    a.lustre = rng_.range(0.08f, 0.2f);
    a.scout = scout;
    if (!scout && foodAge_[nest] < 45.f) {
        a.hasGoal = true;
        a.goal = food_at_[nest] + v2(rng_.range(-0.6f, 0.6f), rng_.range(-0.6f, 0.6f));
    } else if (!scout && burstOut && hasSugar_) {
        a.hasGoal = true; // poured out: straight for the sugar
        a.goal = v2(rng_.range(-4.f, 4.f), sugarC_.y);
    }
    ants_.push_back(a);
}

void AntsUniverse::syncCells(const Game& game) {
    float top = -10.f;
    int appeared = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = game.cell(x, y);
            const int8_t was = occ_[y][x];
            occ_[y][x] = (int8_t)(y >= Game::HIDDEN ? c.type : -1);
            if (was < 0 && occ_[y][x] >= 0) appeared++;
            if (c.type < 0) {
                nib_[y][x] = 0.f;
                seed_[y][x] = 0.f;
            } else {
                if (seed_[y][x] <= 0.f) seed_[y][x] = rng_.range(1.f, 97.f);
                if (y >= Game::HIDDEN) top = std::max(top, 11.5f - y + 0.5f);
            }
        }
    // a whole stack at once (a scene review's board): the colony has had time to find it
    if (appeared > 24) warm_ = false;
    openTop_ = top + 1.25f;
    // the sugar the scouts smell: the stack's top surface, near its middle
    hasSugar_ = top > -9.9f;
    sugarC_ = v2(0.f, top - 0.6f);
}

void AntsUniverse::stepAnts(const Game& game, float dt, float speedK) {
    (void)game;
    const float sense = 0.6f;
    const float nestIn = nestR_ * 0.6f;
    for (size_t i = 0; i < ants_.size(); i++) {
        Ant& a = ants_[i];
        a.since += dt;
        a.alpha = std::min(1.f, a.alpha + dt * 2.f);
        a.panic = std::max(0.f, a.panic - dt);
        // stuck inside a cube that just landed: out to the nearest free side, or gone
        if (sugarAt(a.p, 0.5f)) {
            bool out = false;
            const vec2 dirs[4] = {v2(0, 1), v2(-1, 0), v2(1, 0), v2(0, -1)};
            const int cx = (int)std::floor(a.p.x + 5.f), cy = (int)std::floor(12.f - a.p.y);
            const vec2 cc = v2(cx - 4.5f, 11.5f - cy);
            for (const vec2& d : dirs) {
                const vec2 q = cc + d * 0.62f;
                if (!blocked(q)) { a.p = q; a.a = std::atan2(d.y, d.x); out = true; break; }
            }
            if (!out) { a.alpha = -1.f; continue; }
        }
        const vec2 nest = nest_[a.nest];
        const bool homing = a.state != 0 || a.retire;
        // wander: a smooth random turn
        a.w += rng_.range(-1.f, 1.f) * (a.scout || homing ? 16.f : 10.f) * dt;
        a.w *= std::exp(-2.5f * dt);
        a.w = clampf(a.w, -2.4f, 2.4f);
        float turn = a.w;
        // the pheromone ahead: left, straight, right
        const std::vector<float>& F = homing ? home_ : food_;
        const float l = pher(F, a.p + dirOf(a.a + 0.6f) * sense), c = pher(F, a.p + dirOf(a.a) * sense),
                    r = pher(F, a.p + dirOf(a.a - 0.6f) * sense);
        const float mx = std::max(c, std::max(l, r));
        if (mx > 0.025f) {
            // an ant that knows its way only leans toward the trail (no trail loops on its own)
            const float k = homing ? 0.25f : a.hasGoal ? 0.35f : 1.f;
            if (c < mx) turn += (l > r ? 1.f : -1.f) * 4.f * k;
            else turn += (l - r) / (l + r + c + 0.05f) * 2.f * k;
            a.offTrail = 0.f;
        } else {
            a.offTrail += dt;
        }
        // the way around the HUD columns: under them
        auto via = [&](vec2 goal) {
            const float side = (std::fabs(goal.x) > std::fabs(a.p.x) ? goal.x : a.p.x) < 0.f ? -1.f : 1.f;
            const vec2 corner = side < 0.f ? v2(layout::LEFT_COLUMN[0] - 0.6f, layout::LEFT_COLUMN[1] - 1.9f)
                                           : v2(layout::RIGHT_COLUMN[2] + 0.6f, layout::RIGHT_COLUMN[1] - 2.3f);
            const bool inside = std::fabs(a.p.x) < std::fabs(corner.x) - 0.2f, beyond = std::fabs(goal.x) > std::fabs(corner.x);
            return inside == beyond && a.p.y > corner.y - 0.5f && a.p.y < 10.8f ? corner : goal;
        };
        if (homing) {
            const vec2 g = via(nest);
            const float dn = len(nest - a.p);
            turn += clampf(wrapAngle(std::atan2(g.y - a.p.y, g.x - a.p.x) - a.a), -1.f, 1.f) * (2.2f + 8.f / (dn + 0.5f));
        } else {
            // the smell of sugar close by: the stack, and loose grains
            float best = 1e9f;
            vec2 target{};
            if (std::fabs(a.p.x) < 7.f && a.p.y < openTop_ + 1.5f) {
                const int cx = (int)std::floor(a.p.x + 5.f), cy = (int)std::floor(12.f - a.p.y);
                for (int y = std::max(Game::HIDDEN, cy - 2); y <= std::min(Game::H - 1, cy + 2); y++)
                    for (int x = std::max(0, cx - 2); x <= std::min(Game::W - 1, cx + 2); x++) {
                        if (occ_[y][x] < 0) continue;
                        const vec2 q = v2(x - 4.5f, 11.5f - y);
                        const float d = len2(q - a.p);
                        if (d < best) { best = d; target = q; }
                    }
            }
            for (const Grain& g : grains_) {
                if (!g.food) continue;
                const float d = len2(g.p - a.p);
                if (d < 2.6f && d < best) { best = d; target = g.p; }
            }
            if (best < 6.25f) {
                const float k = 1.f - std::sqrt(best) / 2.5f;
                turn += clampf(wrapAngle(std::atan2(target.y - a.p.y, target.x - a.p.x) - a.a), -1.f, 1.f) * 5.f * k;
            }
            // led by memory toward the sugar
            if (a.hasGoal) {
                const vec2 g = via(a.goal);
                const float dg = len(a.goal - a.p);
                if (dg < 1.2f) a.hasGoal = false;
                else turn += clampf(wrapAngle(std::atan2(g.y - a.p.y, g.x - a.p.x) - a.a), -1.f, 1.f) * 2.6f;
                a.offTrail = 0.f;
            }
            // a scout smells the stack from afar
            if (a.scout && hasSugar_) {
                const float d2 = len2(sugarC_ - a.p);
                const vec2 g = via(sugarC_);
                if (d2 < 400.f)
                    turn += clampf(wrapAngle(std::atan2(g.y - a.p.y, g.x - a.p.x) - a.a), -1.f, 1.f) *
                            0.9f * (1.f - std::sqrt(d2) / 20.f);
            }
            // excited by a clear: toward it
            if (exciteT_ > 0.f && len2(excite_ - a.p) < 100.f)
                turn += clampf(wrapAngle(std::atan2(excite_.y - a.p.y, excite_.x - a.p.x) - a.a), -1.f, 1.f) * 1.2f;
        }
        if (a.panic > 0.f) turn = a.w * 0.2f;
        a.a = wrapAngle(a.a + turn * dt);
        const float excited = (exciteT_ > 0.f && len2(excite_ - a.p) < 64.f) ? 1.3f : 1.f;
        const float sp = a.speed * speedK * excited * (a.panic > 0.f ? 1.6f + a.panic * 2.f : 1.f);
        vec2 np = a.p + dirOf(a.a) * (sp * dt);
        if (blocked(np)) {
            // feel along the obstacle: the closest free heading
            bool ok = false;
            const float sgn = rng_.chance(0.5f) ? 1.f : -1.f;
            for (float t : {0.45f, 0.9f, 1.4f, 2.0f, 2.6f}) {
                for (float s : {sgn, -sgn}) {
                    const float na = a.a + s * t;
                    const vec2 q = a.p + dirOf(na) * (sp * dt);
                    if (!blocked(q)) { a.a = wrapAngle(na); np = q; ok = true; break; }
                }
                if (ok) break;
            }
            if (!ok) { a.a = wrapAngle(a.a + PI); np = a.p; }
            a.w *= 0.3f;
        }
        const float moved = len(np - a.p);
        a.p = np;
        a.gait += moved / (ANT_L * 0.42f) * PI;
        // the trails
        wear(a.p, dt * (a.state == 1 ? 1.2f : 0.45f));
        if (a.state == 1) deposit(food_, a.p, dt * 7.f * std::exp(-a.since / 30.f));
        else if (a.state == 0) deposit(home_, a.p, dt * 2.5f * std::exp(-a.since / 14.f));
        // sugar under the jaws
        if (a.state == 0 && !a.retire) {
            const vec2 head = a.p + dirOf(a.a) * (ANT_L * 0.42f);
            int gx = 0, gy = 0;
            bool found = false;
            if (sugarAt(head, CUBE + 0.07f, &gx, &gy)) {
                a.goal = head;
                a.crumb = lerp(pieceColor(occ_[gy][gx]), vec3(1.f), 0.15f);
                nib_[gy][gx] = std::min(1.f, nib_[gy][gx] + 0.06f);
                found = true;
            } else {
                for (Grain& g : grains_)
                    if (g.food && g.life > 0.f && len2(g.p - head) < 0.03f) {
                        a.goal = g.p;
                        a.crumb = g.col;
                        g.life = -1.f;
                        g.food = false;
                        found = true;
                        break;
                    }
            }
            if (found) {
                a.state = 1;
                a.since = 0.f;
                a.a = wrapAngle(a.a + PI + rng_.range(-0.5f, 0.5f));
                a.w = 0.f;
            } else if (a.since > 55.f) {
                a.state = 2; // lost: home
            } else if (!a.scout && !a.hasGoal && a.offTrail > 4.f) {
                a.state = 2; // no trail to follow: back to the nest
            }
        }
        // home
        if (homing && len2(a.p - nest) < nestIn * nestIn) {
            if (a.retire) { a.alpha = -1.f; continue; }
            if (a.state == 1) {
                recruits_[a.nest] += 1.f + 1.f * smoothstepf(0.5f, 2.f, lv_) + 2.f * dropMix_;
                food_at_[a.nest] = a.goal;
                foodAge_[a.nest] = 0.f;
                a.hasGoal = true; // back to the sugar
            } else if (!a.scout) {
                a.alpha = -1.f; // nothing to follow: it stays in
                continue;
            }
            a.state = 0;
            a.since = 0.f;
            a.a = wrapAngle(a.a + PI + rng_.range(-0.6f, 0.6f));
        }
    }
    ants_.erase(std::remove_if(ants_.begin(), ants_.end(), [](const Ant& a) { return a.alpha < 0.f; }), ants_.end());
}

void AntsUniverse::simulate(const Game& game, float wdt, float dt, const MusicState& music) {
    // the staging
    lv_ = approach(lv_, lvTarget_, 0.9f, dt);
    dropMix_ = approach(dropMix_, inDrop_ ? 1.f : 0.f, inDrop_ ? 5.f : 0.8f, dt);
    swell_ = approach(swell_, 0.f, 0.35f, dt);
    exciteT_ = std::max(0.f, exciteT_ - wdt);
    foodAge_[0] += wdt;
    foodAge_[1] += wdt;
    const float pm = smoothstepf(0.f, 1.f, lv_), pk = smoothstepf(1.f, 2.f, lv_);
    const float kick = music.hasSong ? music.kick : 0.f;
    const float target = 40.f + 70.f * pm + 80.f * pk + 120.f * dropMix_;
    const float speedK = (1.05f + 0.45f * pm + 0.4f * pk + 1.3f * dropMix_) * (1.f + 0.5f * kick * dropMix_);

    // the nests: a few scouts always out; followers called out by the crumbs brought home (recruitment), a stream
    // when the colony pours out
    int scouts[2] = {0, 0};
    for (const Ant& a : ants_)
        if (a.scout && !a.retire) scouts[a.nest]++;
    const float scoutWant = 7.f + 4.f * pm + 5.f * pk + 8.f * dropMix_;
    const float recruitRate = 3.f + 5.f * pk + 14.f * dropMix_;
    for (int n = 0; n < 2; n++) {
        if (scouts[n] < scoutWant) {
            scoutAcc_[n] += wdt * 1.2f;
            if (scoutAcc_[n] >= 1.f) { spawn(n, false, true); scoutAcc_[n] = 0.f; }
        }
        if (pourLeft_[n] > 0) {
            spawnAcc_[n] += wdt * 32.f;
            while (spawnAcc_[n] >= 1.f && pourLeft_[n] > 0) {
                spawn(n, true, rng_.chance(0.25f));
                pourLeft_[n]--;
                spawnAcc_[n] -= 1.f;
            }
        } else if (recruits_[n] >= 1.f && (float)ants_.size() < target) {
            spawnAcc_[n] += wdt * recruitRate;
            while (spawnAcc_[n] >= 1.f && recruits_[n] >= 1.f) {
                spawn(n, false, false);
                recruits_[n] -= 1.f;
                spawnAcc_[n] -= 1.f;
            }
        } else {
            spawnAcc_[n] = 0.f;
        }
        recruits_[n] = std::min(recruits_[n], 40.f);
    }
    // too many: some go back in
    {
        int excess = (int)ants_.size() - (int)(target * 1.08f) - 6;
        for (size_t i = 0; i < ants_.size() && excess > 0; i++) {
            Ant& a = ants_[(i * 7919 + (size_t)(clock_ * 13.f)) % ants_.size()];
            if (a.retire || a.state == 1) continue;
            if (rng_.chance(wdt * 0.6f)) { a.retire = true; excess--; }
            else excess--;
        }
    }
    // scattered by impacts
    for (const vec2& at : lockAt_)
        for (Ant& a : ants_) {
            const float d2 = len2(a.p - at);
            if (d2 < 2.6f) {
                a.a = std::atan2(a.p.y - at.y, a.p.x - at.x) + rng_.range(-0.4f, 0.4f);
                a.panic = 0.35f + 0.3f * (1.f - d2 / 2.6f);
            }
        }
    lockAt_.clear();

    stepAnts(game, wdt, speedK);

    // evaporation
    const float kf = std::exp(-wdt / TAU_FOOD), kh = std::exp(-wdt / TAU_HOME);
    for (float& v : food_) v *= kf;
    for (float& v : home_) v *= kh;
    const float kt = std::exp(-wdt / 50.f);
    for (float& v : trace_) v *= kt;
}

void AntsUniverse::warmUp(const Game& game) {
    // the colony has been there a while: its trails already lead to the stack
    MusicState quiet;
    syncCells(game);
    for (int i = 0; i < 900; i++) simulate(game, 0.05f, 0.05f, quiet);
    warm_ = true;
}

// =====================================================================================================
// Text
// =====================================================================================================

namespace {
int slotOf(unsigned char c) { return c == 0x7f ? 95 : (c >= 32 && c < 127 ? c - 32 : '?' - 32); }
int codeOf(unsigned char c) { return c == 0x7f ? 0xB7 : c; }
} // namespace

float AntsUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char ch = (unsigned char)s[i];
        w += f.g[slotOf(ch)].adv * k + track;
        if (i + 1 < s.size())
            w += stbtt_GetCodepointKernAdvance(info, codeOf(ch), codeOf((unsigned char)s[i + 1])) * f.scaleTT * k;
    }
    return w - track;
}

float AntsUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                              std::vector<float>& out) const {
    if (!f.ok) return 0.f;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    const float baseline = y + f.capH * k;
    float pen = x;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char ch = (unsigned char)s[i];
        const Glyph& g = f.g[slotOf(ch)];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) out.insert(out.end(), q, q + 4);
        }
        pen += g.adv * k + track;
        if (i + 1 < s.size())
            pen += stbtt_GetCodepointKernAdvance(info, codeOf(ch), codeOf((unsigned char)s[i + 1])) * f.scaleTT * k;
    }
    return pen - track - x;
}

void AntsUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& color, float alpha, float bias) {
    if (q.empty() || !f.ok) return;
    const Variant& V = VARIANTS[variant_];
    const float b = smoothstepf(0.f, 1.f, bridge_.amount);
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uColor", lerp(color, ugl::toSrgb(bridge_.ink), b));
    set3f(progText_, "uHalo", lerp(hex(V.ground), ugl::toSrgb(bridge_.color), b));
    set1f(progText_, "uAlpha", alpha);
    set1f(progText_, "uHaloAmt", 0.55f);
    set1f(progText_, "uBias", bias);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD as a naturalist's field notes: an italic serif for the song, the section and the celebrations, small spaced
// capitals for the labels, light numbers.
void AntsUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const Variant& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        std::vector<float> v;
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? hex(V.accent) : hex(V.ground) * (V.light ? 0.97f : 0.8f);
            const float a = r.accent || r.custom ? r.alpha : std::min(1.f, r.alpha * 1.1f);
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
            const float q[6][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y0}, {x1, y1}, {x0, y1}};
            for (auto& p : q) v.insert(v.end(), {p[0], p[1], c.x, c.y, c.z, a});
        }
        glUseProgram(progShape_);
        set2f(progShape_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(shapeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6));
    }
    const vec3 ink = hex(V.ink), dim = hex(V.dim);
    const vec3 accent = lerp(hex(V.accent), pieceColor(4), dropMix_ * 0.0f);
    std::vector<float> q;
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        const std::string text = ascii(h.text);
        q.clear();
        auto place = [&](const Font& f, const std::string& t, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, t, size, track);
            textQuads(f, t, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string name = text, var;
            const size_t dot = name.find('\x7f');
            if (dot != std::string::npos) { var = name.substr(dot + 1); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            name = titleCase(name);
            const float sz = base * 1.45f, vsz = base * 0.62f, vtr = vsz * 0.42f;
            const float wName = textWidth(fontSerif_, name, sz, 0.f);
            const float wVar = var.empty() ? 0.f : textWidth(fontSans_, var, vsz, vtr);
            const float gap = base * 0.8f;
            const float x = ugl::hudLeft(h, wName + (var.empty() ? 0.f : gap + wVar), width_);
            textQuads(fontSerif_, name, x, h.y - base * 0.12f, sz, 0.f, q);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            drawText(fontSerif_, q, ink, a, 0.04f);
            if (!var.empty()) {
                q.clear();
                textQuads(fontSans_, var, x + wName + gap, h.y + base * 0.26f, vsz, vtr, q);
                drawText(fontSans_, q, accent, a, 0.08f);
            }
            continue;
        }
        case HUD_SECTION:
            place(fontSerif_, titleCase(text), base * 1.4f, 0.f);
            drawText(fontSerif_, q, accent, saturate(0.2f + h.alpha * 1.9f), 0.04f);
            continue;
        case HUD_TITLE:
            place(fontSerif_, text, base * 1.3f, 0.f, -base * 0.14f);
            drawText(fontSerif_, q, ink, saturate(h.alpha * 1.3f), 0.04f);
            continue;
        case HUD_INFO:
            place(fontSans_, upper(text), base * 0.66f, base * 0.32f);
            drawText(fontSans_, q, dim, saturate(0.3f + alpha * 2.f), 0.1f);
            continue;
        case HUD_LABEL:
            place(fontSans_, text, base * 0.7f, base * 0.42f);
            drawText(fontSans_, q, dim, saturate(alpha * 1.3f), 0.08f);
            continue;
        case HUD_VALUE:
            place(fontSans_, text, base * 1.12f, base * 0.06f);
            drawText(fontSans_, q, ink, alpha, 0.07f);
            continue;
        case HUD_NOTICE:
            place(fontSerif_, text, base * 1.2f, 0.f);
            drawText(fontSerif_, q, accent, alpha, 0.04f);
            continue;
        case HUD_KEY:
            place(fontSans_, text, base * 0.85f, base * 0.1f);
            drawText(fontSans_, q, ink, alpha, 0.08f);
            continue;
        case HUD_TOAST:
            place(fontSerif_, titleCase(text), base * 1.55f, 0.f, -base * 0.15f);
            drawText(fontSerif_, q, ink, alpha, 0.04f);
            continue;
        case HUD_TOAST_SUB:
            place(fontSans_, text, base * 0.8f, base * 0.3f);
            drawText(fontSans_, q, accent, alpha * 0.9f, 0.08f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontSerif_, titleCase(text), base * 1.35f, 0.f, -base * 0.1f);
                drawText(fontSerif_, q, ink, alpha, 0.04f);
            } else {
                place(fontSans_, text, base * 0.82f, base * 0.16f);
                drawText(fontSans_, q, h.accent ? accent : ink, alpha, 0.08f);
            }
            continue;
        default:
            place(fontSans_, text, base * 0.8f, base * 0.12f);
            drawText(fontSans_, q, h.accent ? accent : ink, alpha, 0.08f);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void AntsUniverse::cube(std::vector<Sprite>& out, float x, float y, float scale, const vec3& col, float alpha, float nib,
                        float seed, float glint, bool ghost, float crumbled) {
    const float h = 0.5f * scale;
    out.push_back({{x, y, h, h}, {0.f, 0.f, seed, alpha}, {col.x, col.y, col.z, nib}, {CUBE * scale, glint, ghost ? 1.f : 0.f, crumbled}});
}

void AntsUniverse::shadow(std::vector<Sprite>& out, float x, float y, float half, float blur, float alpha) {
    const float h = half + blur * 1.5f;
    out.push_back({{x, y, h, h}, {0.f, 1.f, 0.f, alpha}, {0.f, 0.f, 0.f, 0.f}, {half, blur, 0.f, 0.f}});
}

void AntsUniverse::drawSprites(const std::vector<Sprite>& v) {
    if (v.empty()) return;
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Sprite), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

void AntsUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const Variant& V = VARIANTS[variant_];
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 2.f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.55f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    syncCells(game);
    if (!warm_) warmUp(game);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    if (!f.paused) {
        clock_ += wdt;
        simulate(game, wdt, dt, music);
        // grains: fly, settle; inside the well they melt away, outside they wait for the ants
        for (Grain& g : grains_) {
            g.t += dt; // real time: a clear is never slowed
            if (g.t < g.delay) continue;
            const float k = g.food ? wdt : dt;
            g.p = g.p + g.v * k;
            g.v = g.v * std::exp(-5.5f * k);
            g.rot += g.spin * k;
            g.spin *= std::exp(-4.f * k);
            if (!g.food && g.t > g.delay + 0.3f && len2(g.v) < 0.04f) {
                const bool inWell = std::fabs(g.p.x) < 5.15f && g.p.y > -10.15f && g.p.y < 10.f;
                if (!inWell && !blocked(g.p) && g.life > 0.f && g.size > 0.04f) {
                    g.food = true;
                    g.life = 30.f;
                    g.t = g.delay;
                }
            }
        }
        grains_.erase(std::remove_if(grains_.begin(), grains_.end(),
                                     [](const Grain& g) { return g.life < 0.f || g.t > g.delay + g.life; }),
                      grains_.end());
        for (Dying& d : dying_) d.t += dt;
        dying_.erase(std::remove_if(dying_.begin(), dying_.end(),
                                    [](const Dying& d) { return d.t > d.delay + (d.over ? 1.4f : 0.5f); }),
                     dying_.end());
        gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);
        // the trails, as the ground shows them
        glBindTexture(GL_TEXTURE_2D, pherTex_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tw_, th_, GL_RED, GL_FLOAT, trace_.data());
    }

    const float b = smoothstepf(0.f, 1.f, bridge_.amount);
    const float world = 1.f - b;
    const vec3 bridgeCol = ugl::toSrgb(bridge_.color), bridgeInk = ugl::toSrgb(bridge_.ink);
    const float pk = smoothstepf(1.f, 2.f, lv_);
    const float kick = music.hasSong ? music.kick : 0.f;

    // ---- build the sprites
    under_.clear();
    stack_.clear();
    antsV_.clear();
    over_.clear();
    top_.clear();
    const float shA = V.shadow;
    const float glint = 0.25f * pk + 0.5f * dropMix_;
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = game.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - game.rowOffset(y));
            shadow(under_, p.x + 0.07f, p.y - 0.1f, CUBE, 0.08f, shA);
            shadow(under_, p.x + 0.015f, p.y - 0.06f, CUBE, 0.02f, shA * 0.7f); // the contact
            cube(stack_, p.x, p.y, 1.f, pieceColor(c.type), 1.f, nib_[y][x], seed_[y][x], glint, false, 0.f);
        }
    // cleared cubes crumbling
    for (const Dying& d : dying_) {
        const float t = d.t - d.delay;
        const float k = t < 0.f ? 0.f : saturate(t / (d.over ? 1.1f : 0.32f));
        if (k >= 1.f) continue;
        cube(stack_, d.x, d.y, 1.f, pieceColor(d.type), 1.f, d.nib, d.seed, glint, false, std::max(0.02f, k));
    }
    // the ghost and the falling piece (in the air: its shadow further off and softer)
    if (game.hasPiece() && !game.over()) {
        const Piece& p = game.piece();
        int cells[4][2];
        game.pieceCells(p, cells);
        const vec3 col = pieceColor(p.type);
        Piece gp = p;
        gp.y = game.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            game.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cube(under_, w.x, w.y, 1.f, col, 0.9f, 0.f, 3.f, 0.f, true, 0.f);
            }
        }
        const float dx = game.pieceVisualX() - p.x, dy = game.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            shadow(over_, w.x + 0.16f, w.y - 0.22f, CUBE, 0.16f, shA * 0.8f);
        }
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cube(top_, w.x, w.y, 1.f, col, 1.f, 0.f, 11.f + c[0] * 3.f + c[1] * 7.f, glint, false, 0.f);
        }
    }
    // previews
    auto preview = [&](int type, vec2 center, float sc, float alpha) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        game.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (auto& c : cells) {
            const vec2 pos = v2(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc);
            shadow(under_, pos.x + 0.06f * sc, pos.y - 0.09f * sc, CUBE * sc, 0.07f * sc, shA * alpha);
            shadow(under_, pos.x + 0.015f * sc, pos.y - 0.06f * sc, CUBE * sc, 0.02f * sc, shA * 0.7f * alpha);
            cube(top_, pos.x, pos.y, sc, pieceColor(type), alpha, 0.f, 5.f + type * 13.f + c[0] * 3.f + c[1], glint, false, 0.f);
        }
    };
    preview(game.holdType(), v2(layout::HOLD_CENTER.x, layout::HOLD_CENTER.y), layout::previewScale(0), game.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(game.next(i), v2(c.x, c.y), layout::previewScale(i), 1.f);
    }
    // grains: lying on the ground (under the ants), flying (over everything)
    for (const Grain& g : grains_) {
        if (g.t < g.delay) continue;
        const float k = (g.t - g.delay) / g.life;
        float a = g.food ? (1.f - smoothstepf(0.93f, 1.f, k)) * world : 1.f - smoothstepf(0.55f, 1.f, k);
        const float s = g.food ? g.size : g.size * (1.f - 0.5f * smoothstepf(0.5f, 1.f, k));
        if (a <= 0.f) continue;
        Sprite sp{{g.p.x, g.p.y, s * 1.6f, s * 1.6f}, {g.rot, 3.f, 0.f, a}, {g.col.x, g.col.y, g.col.z, 0.f}, {s, 0.f, 0.f, 0.f}};
        const bool inWell = std::fabs(g.p.x) < 5.05f && g.p.y > -10.05f;
        (g.food || inWell ? under_ : over_).push_back(sp);
    }
    // the ants
    for (const Ant& a : ants_) {
        // inside the well (over the blocks) an ant is a faint shadow, fading in across the border
        const float inside = std::min(std::min(5.f - std::fabs(a.p.x), a.p.y + 10.f), 10.f - a.p.y);
        const float al = saturate(a.alpha) * world * (1.f - 0.66f * smoothstepf(-0.3f, 0.3f, inside));
        if (al <= 0.f) continue;
        const float h = ANT_L * 0.62f;
        antsV_.push_back({{a.p.x, a.p.y, h, h}, {a.a, 2.f, 0.f, al}, {a.crumb.x, a.crumb.y, a.crumb.z, a.lustre * V.lustre * 6.f},
                          {ANT_L, a.gait, a.state == 1 ? 1.f : 0.f, 0.f}});
    }
    // the bonus gauge: twelve grains of sugar in a ring
    {
        const vec3 gc = layout::GAUGE_CENTER;
        const float R = layout::GAUGE_R * 0.78f;
        const int N = Game::BONUS_LINES;
        const float lvl = saturate(gaugeShown_) * N;
        const bool ready = game.bonusReady(), active = game.bonusActive();
        const vec3 lit = ready || active ? hex(V.accent) : hex(V.ink);
        for (int i = 0; i < N; i++) {
            const float ang = PI * 0.5f - TAU * i / N;
            const vec2 p = v2(gc.x + std::cos(ang) * R, gc.y + std::sin(ang) * R);
            const float fill = saturate(lvl - i);
            const float s = 0.075f;
            top_.push_back({{p.x, p.y, 0.14f, 0.14f}, {0.f, 4.f, 0.f, 0.35f}, {hex(V.dim).x, hex(V.dim).y, hex(V.dim).z, 1.f},
                            {s * 0.75f, 0.012f, 0.f, 0.f}});
            if (fill > 0.f)
                top_.push_back({{p.x, p.y, s * 1.6f, s * 1.6f}, {ang + PI * 0.25f, 3.f, 0.f, fill}, {lit.x, lit.y, lit.z, 0.f}, {s, 0.f, 0.f, 0.f}});
        }
    }

    // ---- draw
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    {
        vec3 ground = hex(V.ground), mottle = hex(V.mottle);
        if (slow_ > 0.001f) {
            ground = lerp(ground, hex(V.slow), 0.35f * slow_);
            mottle = lerp(mottle, hex(V.slow), 0.35f * slow_);
        }
        glUseProgram(progGround_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pherTex_);
        set1i(progGround_, "uPher", 0);
        set4f(progGround_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set4f(progGround_, "uPherRect", t0_.x, t0_.y, tw_ * TCELL, th_ * TCELL);
        set1f(progGround_, "uPx", px_);
        set3f(progGround_, "uGround", ground);
        set3f(progGround_, "uMottle", mottle);
        set3f(progGround_, "uTrail", hex(V.trail));
        set3f(progGround_, "uHole", hex(V.hole));
        set3f(progGround_, "uMound", hex(V.mound));
        set3f(progGround_, "uFrame", hex(V.frame));
        set1f(progGround_, "uFrameA", V.frameA);
        set1f(progGround_, "uGrain", V.grain);
        set1f(progGround_, "uTrailAmt", V.trailAmt * saturate(0.75f + 0.15f * pk + 0.25f * dropMix_ + 0.3f * swell_));
        set1f(progGround_, "uWorld", world);
        set4f(progGround_, "uNest0", nest_[0].x, nest_[0].y, nestR_, 0.f);
        set4f(progGround_, "uNest1", nest_[1].x, nest_[1].y, nestR_, 0.f);
        set3f(progGround_, "uBridgeCol", bridgeCol);
        set1f(progGround_, "uBridge", b);
        set1f(progGround_, "uLight", V.light ? 1.f : 0.f);
        ugl::drawFullscreen();
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progSprite_);
    setMat(progSprite_, "uVP", vp_);
    set1f(progSprite_, "uPx", px_);
    set3f(progSprite_, "uAnt", hex(V.ant));
    const vec3 ld = normalize(vec3(-0.45f, 0.6f, 0.8f));
    set3f(progSprite_, "uLightDir", ld);
    set1f(progSprite_, "uBridge", b);
    set3f(progSprite_, "uBridgeInk", bridgeInk);
    set1f(progSprite_, "uSparkle", kick * dropMix_);
    drawSprites(under_);
    drawSprites(stack_);
    drawSprites(antsV_);
    drawSprites(over_);
    drawSprites(top_);
    drawHud(f.hud, f.panels);

    // ---- the veil: startup fade, pause / game-over dim
    const float veil = std::max(1.f - saturate(f.fade), dimT_ * 0.5f);
    if (veil > 0.001f) {
        glUseProgram(progVeil_);
        const vec3 vc = V.light ? hex(V.ink) * 0.3f : vec3(0.f);
        set4f(progVeil_, "uVeil", vc.x, vc.y, vc.z, veil);
        ugl::drawFullscreen();
    }
    glDisable(GL_BLEND);
    // hand the frame over now (the headless EGL readback of large frames came back black without it)
    glFlush();
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_ants() {
    static const UniverseEntry e{
        "ANTS", "sugar cubes on the ground, and a colony finding them",
        [] { return std::unique_ptr<Universe>(new AntsUniverse()); },
        // busy, lively songs suit the colony a little better
        [](const SongLook& l) { return 0.8f + 0.3f * l.density; },
        [] {
            std::vector<std::string> v;
            for (const Variant& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
