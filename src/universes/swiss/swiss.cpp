// SWISS POSTER: the International Typographic Style (Mueller-Brockmann, Neue Grafik), a poster set by the song.
//
// One world: a poster on a strict modular grid, the grid of the well's own cells (one unit per cell, every element
// sits on whole units). Huge flush-left lowercase words taken from the song (its title, its artist, the section's
// name), flat color fields, a disc, thin rules and small blocks of information, in asymmetric balance with a lot of
// empty ground. The poster runs at full strength everywhere except behind what is played or read: the well and a
// crisp box behind each HUD element (previews, gauge, lettering) veil it, so the words and fields run behind them
// without ever competing with the stack or the HUD.
//
// The pieces are flat, crisp color blocks on the grid: one clean shape per piece (its cells merge, a narrow gutter
// of ground around it, so two pieces never fuse), no bevels, no gradients. Every section of the song has its own
// poster palette (three colors plus the ink); the falling piece takes the current palette and keeps it when it locks,
// so the stack records the song's structure, band after band. Within a palette the seven types read by color and
// value (each color, a shade of the first, a tint of the second, the ink and a grey).
//
// The song sets the composition. On the bar grid the words and fields recompose: they slide and resize along the
// grid lines with slow eased motion (a big move crosses by a quick fade instead of sliding across the screen, a
// field wipes shut along its length and opens elsewhere), small changes every bar, the structure every two or four.
// Calm is sparse (a small title stacked in one zone, one field, much ground); mid adds the section's name set
// vertically up the other zone and a second field; a peak is dense (bigger type running across, a stripe across the
// whole poster, a disc, three fields). Every two beats a thin rule steps along the grid and a small square moves along
// it; at a peak the kicks thicken the rules for an instant. The drop is a hard cut on its downbeat to the negative
// print (ground and ink swap, the stack and the HUD with them), the song's title set giant across the poster in the
// drop's palette; it holds while the drop lasts and eases back to the positive after. A cleared row closes into a flat
// bar and slides out of the well sideways (alternating sides) in 0.3 s, clipped at the well's edge; a quad sets
// "quad" up the free zone in the signature color for two bars. Slow motion is the negative print (ground and ink
// swap). Game over slides the stack out row by row from the top.
//
// Cost: flat instanced shapes and SDF type into one full-resolution poster target, one composite pass.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "title.hpp"
#include "universes/swiss/swiss.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/swiss/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Inter (Bold: the poster's words, the values; Regular: the information, the labels), SIL Open Font License, see
// OFL-Inter.txt.
const unsigned char FONT_BOLD[] = {
#include "universes/swiss/Inter-Bold.ttf.inc"
};
const unsigned char FONT_REG[] = {
#include "universes/swiss/Inter-Regular.ttf.inc"
};

namespace shaders = swiss_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// The posters: a ground, an ink, and one palette of three colors per kind of section (0 intro / outro, 1 verse,
// 2 build / break, 3 chorus, 4 drop). The signature color leads every palette but the drop's.
struct VariantDef {
    const char* name;
    unsigned ground, ink;
    unsigned pal[5][3];
};
const VariantDef VARIANTS[] = {
    // white, black and red
    {"KUNSTHALLE", 0xf4f4f2, 0x121212,
     {{0xe2231a, 0x2a4fa8, 0xee8a1e}, {0xe2231a, 0x1f3f95, 0xe8a700}, {0xe2231a, 0x23875a, 0x2e9fd0},
      {0xe2231a, 0x1d4fd8, 0xff7300}, {0xd8004c, 0x0b2fb0, 0xffb000}}},
    // a cool white, blue and black
    {"ZURICH", 0xedf1f4, 0x0d1520,
     {{0x1d4fd8, 0x1f9ec9, 0xe2231a}, {0x1d4fd8, 0xe2231a, 0x00a074}, {0x1d4fd8, 0x00a074, 0xf0a400},
      {0x1d4fd8, 0xff5a1f, 0x10a6d8}, {0x2d1bff, 0xe6007e, 0x00b3a4}}},
    // a black ground, white type, signal orange
    {"NIGHT", 0x121314, 0xf1f1ee,
     {{0xff5a1f, 0x3b7ddb, 0xc9c9c4}, {0xff5a1f, 0x2fa8ff, 0x35c27c}, {0xff5a1f, 0x35c27c, 0xb98cff},
      {0xff5a1f, 0xffd23f, 0x4a7dff}, {0xff2e4d, 0x00d2c0, 0xffe14d}}},
    // white, black and magenta
    {"LAUSANNE", 0xf4f4f4, 0x161616,
     {{0xe6007e, 0x3a57c4, 0x14907f}, {0xe6007e, 0x14907f, 0xeca400}, {0xe6007e, 0x4a4fa8, 0xff6a13},
      {0xe6007e, 0xff6a13, 0x0094d8}, {0x7a00e6, 0xff2e7e, 0x00c2a8}}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The gutter around a piece (board units, each side), the clear's slide (seconds).
constexpr float GAP = 0.055f;
constexpr float WIPE_S = 0.3f;
// How much of the poster is veiled behind the well and the HUD's boxes.
constexpr float VEIL = 0.93f;

float ease(float t) {
    t = saturate(t);
    return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
}

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

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
std::string upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> v;
    size_t a = 0;
    while (a <= s.size()) {
        size_t b = s.find('\n', a);
        if (b == std::string::npos) b = s.size();
        v.push_back(s.substr(a, b - a));
        a = b + 1;
    }
    return v;
}

int sectionPalette(const std::string& s) {
    if (s == "INTRO" || s == "OUTRO") return 0;
    if (s == "VERSE") return 1;
    if (s == "BUILD" || s == "BREAK") return 2;
    if (s == "CHORUS") return 3;
    if (s == "DROP") return 4;
    return -1;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool SwissUniverse::init(int width, int height) {
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "swiss shapes");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "swiss text");
    progComp_ = ugl::program(ugl::FULLSCREEN_VS, shaders::COMP_FS, "swiss composite");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Shape), (void*)(sizeof(float) * 4 * i));
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
    glBindVertexArray(0);

    buildFont(bold_, FONT_BOLD, 128.f, 2048);
    buildFont(reg_, FONT_REG, 64.f, 1024);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void SwissUniverse::buildFont(Font& f, const unsigned char* ttf, float pxSize, int AW) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] swiss font init failed\n");
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
    auto cpOf = [](int ch) { return ch == 127 ? 0xB7 : ch; }; // the middle dot in DEL's slot
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

void SwissUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    unitPx_ = height_ / (2.f * hh);
    posterT_.create(width_, height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    if (bar_ > -1000) compose(bar_, 0.f, true);
}

SwissUniverse::~SwissUniverse() {
    posterT_.destroy();
    for (GLuint p : {progShape_, progText_, progComp_}) glDeleteProgram(p);
    for (GLuint v : {shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&bold_, &reg_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void SwissUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        static const int LIGHT[3] = {0, 1, 3};
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 2;
        else v = LIGHT[(int)(h * 3) % 3];
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x5A155ull);
    bpm_ = t.bpm > 30.f ? t.bpm : 120.f;
    if (seconds <= 0.1f) palInit_ = false;
    bar_ = -1000; // recompose on the next frame
}

void SwissUniverse::setLevel(int level, float energy, float seconds, float impact) {
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
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, 1.f, 5.f);
}

// The drop: a hard cut on its downbeat to the negative print, the title set giant across the poster in the drop's
// palette; it holds while the drop lasts.
void SwissUniverse::drop(float strength) {
    (void)strength;
    dropMode_ = true;
    dropPending_ = true;
}

void SwissUniverse::refresh(float strength) { (void)strength; }

vec2 SwissUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints SwissUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = ugl::toLinear(hex(V.ground));
    h.ink = ugl::toLinear(hex(V.ink));
    h.accent = ugl::toLinear(hex(V.pal[palIdx_][0]));
    h.light = ugl::luma(hex(V.ground)) > 0.4f;
    h.styles = TS_TILES | TS_CURTAIN | TS_REBUILD;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the pieces' ids in step with the board (a new board, or one changed behind our back, gets fresh ids and the
// current palette).
void SwissUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) ids_[y][x] = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0 || ids_[y][x]) continue;
            const int id = nextId_++;
            int stack[8][2], sp = 0, count = 0;
            stack[sp][0] = x; stack[sp][1] = y; sp++;
            ids_[y][x] = id;
            pal_[y][x] = palIdx_;
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
                    pal_[ny][nx] = palIdx_;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

void SwissUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            pal_[c.y][c.x] = palIdx_;
        }
        break;
    }
    case GameEvent::HardDrop: {
        // a thin rule at the contact, closing toward the middle
        float x0 = 99, x1 = -99, yl = 99;
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            x0 = std::min(x0, p.x - 0.5f);
            x1 = std::max(x1, p.x + 0.5f);
            yl = std::min(yl, p.y - 0.5f);
        }
        if (x1 > x0) { lockLine_ = 1.f; lockX0_ = x0; lockX1_ = x1; lockY_ = yl; }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // each cleared row closes into a flat bar and slides out sideways
        int k = 0;
        for (int y = Game::HIDDEN; y < Game::H; y++) {
            if (!rows[y]) continue;
            Wipe w;
            w.y = layout::cellPos(0.f, (float)y).y;
            for (int x = 0; x < Game::W; x++) { w.type[x] = -1; w.pal[x] = palIdx_; w.mask[x] = 0; }
            for (auto& c : ev.cells)
                if (c.y == y && c.x >= 0 && c.x < Game::W) {
                    w.type[c.x] = std::clamp(c.type, 0, 6);
                    w.pal[c.x] = ids_[y][c.x] ? pal_[y][c.x] : palIdx_;
                }
            w.t = 0.f;
            w.delay = 0.f;
            w.dir = ((y + k) & 1) ? 1.f : -1.f;
            k++;
            wipes_.push_back(w);
        }
        // compact the ids like the game did
        {
            int nb[Game::H][Game::W] = {}, np[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(np[dst], pal_[y], sizeof(pal_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(pal_, np, sizeof(pal_));
        }
        if (ev.count >= 4) {
            quadUntil_ = std::max(bar_, 0L) + 2;
            compose(bar_, 1.2f, false);
        }
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        // the stack slides out row by row from the top
        for (int y = Game::HIDDEN; y < Game::H; y++) {
            Wipe w;
            bool any = false;
            w.y = layout::cellPos(0.f, (float)y).y;
            for (int x = 0; x < Game::W; x++) { w.type[x] = -1; w.pal[x] = palIdx_; }
            for (auto& c : ev.cells)
                if (c.y == y && c.x >= 0 && c.x < Game::W) {
                    w.type[c.x] = std::clamp(c.type, 0, 6);
                    w.pal[c.x] = ids_[y][c.x] ? pal_[y][c.x] : palIdx_;
                    any = true;
                }
            if (!any) continue;
            // the pieces keep their shapes until their row leaves
            for (int x = 0; x < Game::W; x++) {
                const int id = ids_[y][x];
                auto same = [&](int nx, int ny) {
                    return id && nx >= 0 && nx < Game::W && ny >= Game::HIDDEN && ny < Game::H && ids_[ny][nx] == id;
                };
                const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
                int m = 0;
                for (int i = 0; i < 8; i++)
                    if (same(x + dx[i], y + dy[i])) m |= 1 << i;
                w.mask[x] = m;
            }
            w.t = 0.f;
            w.delay = 0.5f + 0.06f * (y - Game::HIDDEN);
            w.dir = (y & 1) ? 1.f : -1.f;
            wipes_.push_back(w);
        }
        break;
    default: break;
    }
}

// =====================================================================================================
// Shapes
// =====================================================================================================

vec2 SwissUniverse::px(float x, float y) const {
    return {(x - viewC_.x) * unitPx_ + width_ * 0.5f, height_ * 0.5f - (y - viewC_.y) * unitPx_};
}

void SwissUniverse::rectPx(float x0, float y0, float x1, float y1, const vec3& c, float a) {
    const float clip[4] = {-1e5f, -1e5f, 1e5f, 1e5f};
    rectPxClip(x0, y0, x1, y1, c, a, clip);
}

void SwissUniverse::rectPxClip(float x0, float y0, float x1, float y1, const vec3& c, float a, const float clip[4]) {
    if (a <= 0.002f || x1 <= x0 || y1 <= y0) return;
    Shape s{{x0, y0, x1, y1}, {c.x, c.y, c.z, a}, {0, 0, 0, 0}, {clip[0], clip[1], clip[2], clip[3]}};
    shapes_.push_back(s);
}

void SwissUniverse::rectB(float x0, float y0, float x1, float y1, const vec3& c, float a) {
    const vec2 p0 = px(std::min(x0, x1), std::max(y0, y1)), p1 = px(std::max(x0, x1), std::min(y0, y1));
    rectPx(p0.x, p0.y, p1.x, p1.y, c, a);
}

void SwissUniverse::discB(float cx, float cy, float r, const vec3& c, float a) {
    if (r <= 0.01f || a <= 0.002f) return;
    const vec2 p = px(cx, cy);
    const float rp = r * unitPx_;
    Shape s{{p.x - rp, p.y - rp, p.x + rp, p.y + rp}, {c.x, c.y, c.z, a}, {2, rp, 0, 0}, {-1e5f, -1e5f, 1e5f, 1e5f}};
    shapes_.push_back(s);
}

void SwissUniverse::ringPx(float cx, float cy, float rIn, float rOut, float fill, const vec3& c, float a) {
    if (a <= 0.002f || fill <= 0.f) return;
    Shape s{{cx - rOut, cy - rOut, cx + rOut, cy + rOut}, {c.x, c.y, c.z, a}, {1, rIn, rOut, fill}, {-1e5f, -1e5f, 1e5f, 1e5f}};
    shapes_.push_back(s);
}

void SwissUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(Shape), shapes_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)shapes_.size());
    shapes_.clear();
}

// =====================================================================================================
// Type
// =====================================================================================================

float SwissUniverse::capWidth(const Font& f, const std::string& s, float track) const {
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

void SwissUniverse::textQuads(const Font& f, const std::string& s, float x, float base, float capPx, float track, int rot,
                              std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = capPx / f.capH;
    float pen = 0.f;
    auto map = [&](float u, float v, float& X, float& Y) {
        if (rot == 0) { X = x + u; Y = base + v; }
        else { X = x + v; Y = base - u; }
    };
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        if (g.w > 0) {
            const float u0 = pen + g.ox * k, v0 = g.oy * k, u1 = u0 + g.w * k, v1 = v0 + g.h * k;
            float P[4][2];
            map(u0, v0, P[0][0], P[0][1]);
            map(u1, v0, P[1][0], P[1][1]);
            map(u1, v1, P[2][0], P[2][1]);
            map(u0, v1, P[3][0], P[3][1]);
            const float T[4][2] = {{g.x0, g.y0}, {g.x1, g.y0}, {g.x1, g.y1}, {g.x0, g.y1}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int j : idx) {
                const float v[4] = {P[j][0], P[j][1], T[j][0], T[j][1]};
                out.insert(out.end(), v, v + 4);
            }
        }
        pen += g.adv * k + track * capPx;
        if (i + 1 < s.size()) {
            unsigned char nx = (unsigned char)s[i + 1];
            if (nx < 32 || nx >= 128) nx = '?';
            pen += f.kern[ch - 32][nx - 32] * k;
        }
    }
}

void SwissUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, bool adaptive) {
    (void)adaptive;
    if (q.empty() || !f.ok || alpha <= 0.002f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// =====================================================================================================
// The composition
// =====================================================================================================

// The title's words, lower case, without what is in brackets (an artist before " - " goes to artist_).
std::vector<std::string> SwissUniverse::titleWords() const {
    std::vector<std::string> w;
    std::string cur;
    for (char c : title_) {
        if (c == ' ') {
            if (!cur.empty()) w.push_back(cur);
            cur.clear();
        } else cur += c;
    }
    if (!cur.empty()) w.push_back(cur);
    if (w.size() > 6) w.resize(6);
    return w;
}

namespace {
// Greedy lines of words no wider than maxW (in cap heights).
template <class F>
std::vector<std::string> breakLines(const std::vector<std::string>& words, float maxW, F width) {
    std::vector<std::string> lines;
    std::string line;
    for (const std::string& w : words) {
        if (line.empty()) { line = w; continue; }
        if (width(line + " " + w) <= maxW) line += " " + w;
        else { lines.push_back(line); line = w; }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}
std::string joinLines(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) s += (i ? "\n" : "") + v[i];
    return s;
}
} // namespace

// A layout on the grid for this bar: the structure (the title's family and zone, the type's size) changes every two
// or four bars, the details (fields' lengths, the stripe, the disc's height) on every bar.
//
// The zones: the free columns left and right of the game's band (x beyond +-12). The left one runs from the HUD's top
// corner to its bottom corner (y -9 .. 10); the right one keeps its foot (y -9.5 .. -6) for the information block.
// Families of the title: STACK (horizontal, small, stacked in a zone: calm), VERT (turned up the left zone, as big
// as the zone allows, wholly in the free space), ACROSS (horizontal from the left edge, big, running on behind the
// veil), GIANT (the drop: across the whole poster).
void SwissUniverse::compose(long bar, float dur, bool instant) {
    const float W = viewH_.x;
    const float Wi = std::floor(W - 0.8f);
    const float ZX = 12.f;                    // the zones start on the grid line past the HUD columns
    const float zw = std::max(4.f, Wi - ZX);  // narrow screens: the zones run under the veil
    const float LX = -(ZX + zw), RX = ZX;     // left edges of the left and right zones
    const float TT = 10.f, BB = -9.f;         // the type's top and bottom lines (clear of the HUD's corners)
    const float RB = -5.f;                    // the right zone's type starts above its information block
    const int lvl = level_;
    const bool drop = dropMode_;
    const int period = lvl >= 1 ? 2 : 4;
    const long era = (long)std::floor((double)bar / period);
    Rng rb(seed_ ^ ((uint64_t)(era + 100000) * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)lvl << 40) ^ (drop ? 0xD0Dull : 0ull));
    Rng rs(seed_ ^ ((uint64_t)(bar + 100000) * 0xC2B2AE3D27D4EB4Full));
    composeCount_++;

    Target T[E_COUNT];
    std::vector<std::string> words = titleWords();
    if (words.empty()) words.push_back(lower(VARIANTS[variant_].name));
    auto wBold = [&](const std::string& s) { return capWidth(bold_, s, -0.02f); };
    const float step = 1.2f;

    enum { STACK, VERT, ACROSS, GIANT };
    int fam;
    if (drop) fam = GIANT;
    else if (lvl == 0) fam = rb.chance(0.5f) ? STACK : ACROSS;
    else if (lvl == 1) fam = rb.chance(0.5f) ? VERT : ACROSS;
    else fam = rb.chance(0.4f) ? VERT : ACROSS;
    const bool stackRight = fam == STACK && rb.chance(0.5f);
    bool leftBusy = fam != STACK || !stackRight; // the title holds the left zone

    // Best line breaking of the words into 1..3 lines for a box (maxW along the lines, maxH across them, in units).
    auto fit = [&](float maxW, float maxH, float capMax, std::vector<std::string>& out) {
        float best = 0.f;
        const float total = wBold(joinLines(words));
        for (int n = 1; n <= std::min<int>(3, (int)words.size()); n++) {
            std::vector<std::string> ls = breakLines(words, total / n + 0.8f, wBold);
            if ((int)ls.size() > 3) continue;
            float wmax = 0.f;
            for (auto& l : ls) wmax = std::max(wmax, wBold(l));
            const float c = std::min({capMax, maxW / std::max(0.1f, wmax), maxH / (1.f + step * (ls.size() - 1))});
            if (c > best + 0.05f) { best = c; out = ls; }
        }
        return std::floor(best * 4.f) * 0.25f;
    };

    float titleBottom = BB, titleX = LX, titleCap = 2.f;
    int titleLines = 1;
    {
        Target& t = T[E_TITLE];
        t.on = true;
        t.color = 0;
        std::vector<std::string> lines;
        if (fam == GIANT) {
            t.v[2] = fit(2.f * Wi, TT - BB + 1.f, 8.5f, lines);
            t.v[0] = -Wi;
            t.v[1] = TT + 0.5f;
            t.color = 1;
        } else if (fam == ACROSS) {
            const float capMax = lvl >= 2 ? (float)rb.irange(4, 5) : lvl == 1 ? 3.f : 2.5f;
            t.v[2] = fit(2.f * Wi - 1.f, TT - BB - 3.f, capMax, lines);
            t.v[0] = -Wi;
            const float h = t.v[2] * (1.f + step * (lines.size() - 1));
            t.v[1] = rb.chance(0.5f) ? TT : std::ceil(BB + 3.f + h);
            t.color = lvl >= 2 && rb.chance(0.4f) ? 1 : 0;
        } else if (fam == VERT) {
            // turned up the left zone, from its foot
            t.rot = 1;
            t.v[2] = fit(TT - BB - 1.f, zw - 0.5f, 6.f, lines);
            t.v[0] = LX + t.v[2];
            t.v[1] = BB + 1.f;
            t.color = lvl >= 2 && rb.chance(0.5f) ? 1 : 0;
        } else {
            const float x0 = stackRight ? RX : LX;
            const float hMax = stackRight ? TT - RB : TT - BB - 3.f;
            t.v[2] = std::max(0.75f, fit(zw - 0.3f, hMax, 1.75f, lines));
            t.v[0] = x0;
            const float h = t.v[2] * (1.f + step * (lines.size() - 1));
            t.v[1] = stackRight || rb.chance(0.5f) ? TT : std::ceil(BB + 3.f + h);
        }
        t.v[3] = step;
        t.text = joinLines(lines);
        titleLines = (int)lines.size();
        titleCap = t.v[2];
        titleX = t.v[0];
        titleBottom = t.rot ? BB : t.v[1] - titleCap * (1.f + step * (titleLines - 1));
    }

    // ---- The artist, small, under a horizontal title.
    if (!artist_.empty() && T[E_TITLE].rot == 0) {
        Target& t = T[E_ARTIST];
        const float cap = std::max(0.5f, std::min(1.f, titleCap * 0.25f));
        t.text = lower(artist_);
        t.v[0] = titleX;
        t.v[1] = std::floor(titleBottom - std::max(0.6f, titleCap * 0.3f));
        t.v[2] = cap;
        t.v[3] = 1.3f;
        t.color = drop ? 0 : 1;
        t.on = t.v[1] - cap > BB;
        if (t.on) titleBottom = t.v[1] - cap;
    }

    // ---- The section's name, turned up the right zone (or "quad" after a four-line clear); the left zone when the title
    // is stacked on the right.
    {
        Target& t = T[E_SECTION];
        const bool quadClear = bar <= quadUntil_;
        std::string word = quadClear ? lower(QUAD_NAME) : lower(section_);
        if (word.empty()) word = lvl == 0 ? "calm" : lvl == 1 ? "tempo" : "forte";
        t.on = !drop && (quadClear || lvl >= 1 || (fam == STACK && rb.chance(0.5f)));
        const bool inLeft = stackRight;
        if (inLeft && leftBusy) t.on = false;
        if (t.on) {
            const float y0 = inLeft ? BB : RB, len = TT - y0;
            float cap = lvl >= 2 || quadClear ? std::min(zw - 1.f, 6.f) : lvl == 1 ? std::min(zw - 2.f, 4.f) : 2.5f;
            cap = std::min(cap, len / std::max(0.1f, wBold(word)));
            cap = std::max(1.f, std::floor(cap * 2.f) * 0.5f);
            const float x0 = inLeft ? LX : RX;
            const float inset = rb.chance(0.5f) ? 0.f : std::max(0.f, std::floor(zw - cap - 0.5f));
            t.text = word;
            t.rot = 1;
            t.v[0] = x0 + inset + cap; // the baseline (the caps fill to its left)
            const float spare = std::max(0.f, std::floor(len - cap * wBold(word)));
            t.v[1] = y0 + std::min(spare, (float)rs.irange(0, 2));
            t.v[2] = cap;
            t.v[3] = step;
            t.color = quadClear ? 1 : 0;
            if (inLeft) leftBusy = true;
        }
    }

    // ---- Fields: flat color on whole units, bleeding off the poster's outer edges.
    auto field = [&](int slot, float x0, float y0, float x1, float y1, int color) {
        Target& t = T[slot];
        t.on = true;
        t.v[0] = x0; t.v[1] = y0; t.v[2] = x1; t.v[3] = y1;
        t.color = color;
    };
    const float bleedR = W + 1.f, bleedL = -W - 1.f;
    const bool disc = !drop && lvl >= 1 && rb.chance(0.5f); // the right zone takes a disc or a block, not both
    if (drop) {
        // the title over a block bleeding off the right edge and a band across
        field(E_FIELD0, RX + 2.f, (float)rs.irange(-5, -4), bleedR, (float)rs.irange(2, 4), 2);
        const float sy = (float)rs.irange(-8, -6);
        field(E_FIELD1, bleedL, sy, bleedR, sy + 1.f, 3);
    } else if (lvl == 0) {
        // one column, in the free zone
        const float w = (float)rb.irange(2, 3);
        if (!leftBusy || (fam == ACROSS && rb.chance(0.5f))) {
            const float x0 = LX + (rb.chance(0.5f) ? 0.f : std::floor(zw - w));
            field(E_FIELD0, x0, BB + (float)rs.irange(0, 1), x0 + w, TT - (float)rb.irange(2, 5) - (float)rs.irange(0, 1), 1);
        } else {
            const float x0 = RX + (rb.chance(0.5f) ? 0.f : std::floor(zw - w));
            field(E_FIELD0, x0, RB + (float)rs.irange(0, 2), x0 + w, TT - (float)rb.irange(1, 4), 1);
        }
    } else if (lvl == 1) {
        // a block bleeding off the right edge, a column in the left zone
        const float y0 = (float)rs.irange(-3, 0);
        if (!disc) field(E_FIELD0, RX + (float)rb.irange(2, 4), y0, bleedR, y0 + (float)rs.irange(3, 5), 1);
        const float w = (float)rb.irange(2, 3);
        const float x0 = LX + (rb.chance(0.5f) ? 0.f : std::floor(zw - w));
        field(E_FIELD1, x0, (float)rs.irange(1, 3), x0 + w, TT + 0.f, 2);
    } else {
        // dense: a block bleeding off the right edge, a stripe across the poster, a square in the left zone
        const float y0 = (float)rs.irange(-4, -1);
        if (!disc) field(E_FIELD0, RX + (float)rb.irange(1, 3), y0, bleedR, y0 + (float)rb.irange(5, 8), 1);
        const float sy = (float)rs.irange(-6, 6);
        field(E_FIELD1, bleedL, sy, bleedR, sy + (float)rb.irange(1, 2), 2);
        const float s = (float)rb.irange(3, 4);
        const float sy2 = (float)rb.irange(0, 4);
        const float sx = LX + (float)rs.irange(0, 1) + (rb.chance(0.5f) ? std::floor(zw - s - 1.f) : 0.f);
        field(E_FIELD2, sx, sy2, sx + s, sy2 + s, 3);
    }

    // ---- A disc (from mid on), in the right zone.
    if (disc) {
        Target& t = T[E_DISC];
        t.on = true;
        const float r = std::max(1.f, std::min(std::floor(zw * 0.5f) - 0.5f, (float)rb.irange(2, 3)));
        t.v[0] = RX + std::floor(zw * 0.5f) + (float)rb.irange(-1, 1);
        t.v[1] = (float)rs.irange((int)RB + 1 + (int)r, (int)(TT - r));
        t.v[2] = r;
        t.color = lvl >= 2 ? 3 : 1;
    }

    // ---- The information block at the foot of the right zone, a rule above it, a small square stepping along it.
    {
        Target& info = T[E_INFO];
        info.on = true;
        std::string s = upper(section_.empty() ? std::string(VARIANTS[variant_].name) : section_);
        if (!info1_.empty()) s += "\n" + info1_;
        if (!info2_.empty()) s += "\n" + info2_;
        info.text = s;
        info.v[0] = RX;
        info.v[1] = -7.5f;
        info.v[2] = 0.36f;
        info.v[3] = 1.7f;
        info.color = 0;
        Target& r0 = T[E_RULE0];
        r0.on = true;
        r0.v[0] = RX;
        r0.v[1] = -6.5f;
        r0.v[2] = RX + std::min(zw, (float)rb.irange(4, 6));
        Target& b = T[E_BULLET];
        b.on = lvl >= 1 || drop;
        b.v[0] = RX;
        b.v[1] = r0.v[1] + 0.3f;
        b.v[2] = RX + 0.6f;
        b.v[3] = r0.v[1] + 0.9f;
        b.color = 1;
        // a rule under a horizontal title
        Target& r1 = T[E_RULE1];
        r1.on = (lvl >= 1 || drop) && T[E_TITLE].rot == 0;
        r1.v[0] = titleX;
        r1.v[1] = std::floor(titleBottom - 0.5f);
        r1.v[2] = titleX + zw;
        if (r1.v[1] < BB) r1.on = false;
        // a long rule across the poster (peak)
        Target& r2 = T[E_RULE2];
        r2.on = lvl >= 2 && !drop;
        // clear of the title: under it, or above it, or at the foot of a turned title
        if (T[E_TITLE].rot) r2.v[1] = BB + 0.5f;
        else if (titleBottom - 2.5f >= BB) r2.v[1] = std::floor(titleBottom - 2.5f);
        else if (T[E_TITLE].v[1] + 1.f <= TT + 0.5f) r2.v[1] = std::ceil(T[E_TITLE].v[1] + 0.7f);
        else r2.on = false;
        r2.v[0] = -Wi;
        r2.v[2] = Wi;
    }

    for (int i = 0; i < E_COUNT; i++) retarget(i, T[i], dur, instant);
    for (int i = E_RULE0; i <= E_RULE2; i++) el_[i].ruleBase = T[i].v[1];
}

namespace {
bool sameTarget(const SwissUniverse* /*unused*/, const float a[4], const float b[4]) {
    for (int i = 0; i < 4; i++)
        if (std::fabs(a[i] - b[i]) > 1e-3f) return false;
    return true;
}
} // namespace

void SwissUniverse::retarget(int slot, const Target& t, float dur, bool instant) {
    Element& e = el_[slot];
    if (instant) {
        e.cur = e.from = e.to = t;
        e.alpha = e.alphaFrom = t.on ? 1.f : 0.f;
        e.t = 1.f;
        e.twoPhase = false;
        return;
    }
    if (e.to.on == t.on && (!t.on || (sameTarget(this, e.to.v, t.v) && e.to.text == t.text && e.to.color == t.color &&
                                      e.to.rot == t.rot)))
        return;
    const bool text = slot == E_TITLE || slot == E_ARTIST || slot == E_SECTION || slot == E_INFO;
    e.from = e.cur;
    e.alphaFrom = e.alpha;
    e.to = t;
    e.t = 0.f;
    e.dur = std::max(0.05f, dur);
    e.swapped = false;
    const bool changed = e.cur.text != t.text || e.cur.rot != t.rot || e.cur.color != t.color;
    float far;
    if (text) far = std::max(std::fabs(e.cur.v[0] - t.v[0]), std::fabs(e.cur.v[1] - t.v[1]));
    else far = std::fabs((e.cur.v[0] + e.cur.v[2]) - (t.v[0] + t.v[2])) * 0.5f;
    e.twoPhase = e.cur.on && t.on && e.alpha > 0.01f && (changed || far > (text ? 9.f : 12.f));
}

namespace {
// A shape's hidden form: a field wipes shut along its longer side (toward its left or bottom edge), a disc shrinks
// to its center, a rule to its start.
void collapse(int slot, const float in[4], float out[4], int fieldSlots0, int disc, int rule0, int rule2) {
    for (int i = 0; i < 4; i++) out[i] = in[i];
    if (slot >= fieldSlots0 && slot < disc) {
        if (in[2] - in[0] > in[3] - in[1]) out[2] = in[0];
        else out[3] = in[1];
    } else if (slot == disc) out[2] = 0.f;
    else if (slot >= rule0 && slot <= rule2) out[2] = in[0];
}
} // namespace

void SwissUniverse::updateElements(float dt) {
    for (int i = 0; i < E_COUNT; i++) {
        Element& e = el_[i];
        const bool text = i == E_TITLE || i == E_ARTIST || i == E_SECTION || i == E_INFO;
        const bool bullet = i == E_BULLET;
        if (e.t >= 1.f) {
            e.cur = e.to;
            e.alpha = e.to.on ? 1.f : 0.f;
            continue;
        }
        e.t = std::min(1.f, e.t + dt / e.dur);
        float hidF[4], hidT[4];
        collapse(i, e.from.v, hidF, E_FIELD0, E_DISC, E_RULE0, E_RULE2);
        collapse(i, e.to.v, hidT, E_FIELD0, E_DISC, E_RULE0, E_RULE2);
        auto lerp4 = [&](const float* a, const float* b, float s, float* o) {
            for (int k = 0; k < 4; k++) o[k] = a[k] + (b[k] - a[k]) * s;
        };
        if (e.twoPhase) {
            if (e.t < 0.5f) {
                const float s = ease(e.t * 2.f);
                if (text || bullet) e.alpha = e.alphaFrom * (1.f - s);
                else { lerp4(e.from.v, hidF, s, e.cur.v); e.alpha = 1.f; }
            } else {
                if (!e.swapped) {
                    e.swapped = true;
                    e.cur = e.to;
                }
                const float s = ease(e.t * 2.f - 1.f);
                if (text || bullet) { e.alpha = s; std::memcpy(e.cur.v, e.to.v, sizeof(e.cur.v)); }
                else { lerp4(hidT, e.to.v, s, e.cur.v); e.alpha = 1.f; }
            }
            continue;
        }
        const float s = ease(e.t);
        if (e.to.on && e.from.on) {
            const std::string keepText = e.to.text;
            lerp4(e.from.v, e.to.v, s, e.cur.v);
            e.cur.text = keepText;
            e.cur.color = e.to.color;
            e.cur.rot = e.to.rot;
            e.cur.on = true;
            e.alpha = e.alphaFrom + (1.f - e.alphaFrom) * s;
        } else if (e.to.on) {
            const float v0[4] = {e.cur.v[0], e.cur.v[1], e.cur.v[2], e.cur.v[3]};
            (void)v0;
            e.cur.on = true;
            e.cur.text = e.to.text;
            e.cur.color = e.to.color;
            e.cur.rot = e.to.rot;
            if (text || bullet) { std::memcpy(e.cur.v, e.to.v, sizeof(e.cur.v)); e.alpha = e.alphaFrom + (1.f - e.alphaFrom) * s; }
            else { lerp4(hidT, e.to.v, s, e.cur.v); e.alpha = 1.f; }
        } else {
            if (text || bullet) e.alpha = e.alphaFrom * (1.f - s);
            else { lerp4(e.from.v, hidF, s, e.cur.v); e.alpha = e.from.on ? 1.f : 0.f; }
            if (e.t >= 1.f) e.cur.on = false;
        }
    }
}

// The poster itself (into posterT_): fields, the disc, the words, the rules.
void SwissUniverse::drawPoster() {
    auto col = [&](int c) { return palShown_[std::clamp(c, 0, 3)]; };
    // fields and the disc
    for (int i = E_FIELD0; i <= E_FIELD2; i++) {
        const Element& e = el_[i];
        if (!e.cur.on && e.alpha <= 0.f) continue;
        rectB(e.cur.v[0], e.cur.v[1], e.cur.v[2], e.cur.v[3], col(e.cur.color), e.alpha);
    }
    {
        const Element& e = el_[E_DISC];
        if (e.cur.on || e.alpha > 0.f) discB(e.cur.v[0], e.cur.v[1], e.cur.v[2], col(e.cur.color), e.alpha);
    }
    flushShapes();
    // the words
    std::vector<float> q;
    for (int i : {E_SECTION, E_TITLE, E_ARTIST, E_INFO}) {
        const Element& e = el_[i];
        if (e.alpha <= 0.002f || e.cur.text.empty()) continue;
        const Font& f = (i == E_ARTIST || i == E_INFO) ? reg_ : bold_;
        const float track = i == E_INFO ? 0.06f : i == E_ARTIST ? 0.f : -0.02f;
        const float capPx = e.cur.v[2] * unitPx_;
        q.clear();
        const std::vector<std::string> lines = splitLines(e.cur.text);
        for (size_t l = 0; l < lines.size(); l++) {
            if (e.cur.rot == 1) {
                const vec2 a = px(e.cur.v[0] + l * e.cur.v[2] * e.cur.v[3], e.cur.v[1]);
                textQuads(f, lines[l], a.x, a.y, capPx, track, 1, q);
            } else {
                const float base = e.cur.v[1] - e.cur.v[2] - l * e.cur.v[2] * e.cur.v[3];
                const vec2 a = px(e.cur.v[0], base);
                textQuads(f, lines[l], a.x, a.y, capPx, track, 0, q);
            }
        }
        drawText(f, q, col(e.cur.color), e.alpha);
    }
    // the rules and the small square
    const float th = std::max(1.f, height_ / 1080.f * (2.f + 2.5f * kickShown_));
    for (int i = E_RULE0; i <= E_RULE2; i++) {
        const Element& e = el_[i];
        if (!e.cur.on && e.alpha <= 0.f) continue;
        const vec2 a = px(e.cur.v[0], e.cur.v[1]), b = px(e.cur.v[2], e.cur.v[1]);
        rectPx(a.x, a.y - th * 0.5f, b.x, a.y + th * 0.5f, col(0), e.alpha);
    }
    {
        const Element& e = el_[E_BULLET];
        if (e.alpha > 0.f) rectB(e.cur.v[0], e.cur.v[1], e.cur.v[2], e.cur.v[3], col(e.cur.color), e.alpha);
    }
    flushShapes();
}

// =====================================================================================================
// The board
// =====================================================================================================

// The seven types within a palette: its three colors, a shade of the first, a tint of the second, the ink and a
// grey.
vec3 SwissUniverse::pieceColor(int type, int pal) const {
    const VariantDef& V = VARIANTS[variant_];
    const unsigned* p = V.pal[std::clamp(pal, 0, 4)];
    const vec3 c0 = hex(p[0]), c1 = hex(p[1]), c2 = hex(p[2]);
    switch (std::clamp(type, 0, 6)) {
    case I: return c0;
    case O: return ink_;
    case T: return c1;
    case S: return c2;
    // a shade of the first color (of the second when the first is too light to shade cleanly: a yellow goes olive)
    case Z: return ugl::luma(c0) > 0.55f ? lerp(c1, hex(V.ink), 0.45f) : lerp(c0, hex(V.ink), 0.4f);
    // a tint of the second toward white (pastel on the dark and grey grounds)
    case J: return lerp(c1, hex(0xf6f6f4), 0.45f);
    default: return lerp(ink_, ground_, 0.5f);
    }
}

// One cell of a piece: a flat square, inset by the gutter on the sides it does not share with its piece, its corner
// cut where two shared sides meet around a missing diagonal (so the piece's outline stays clean). Edges snapped to
// pixels so the cells of a piece meet without a seam. mask bits: 1 left, 2 right, 4 below, 8 above, 16 below-left,
// 32 below-right, 64 above-left, 128 above-right.
void SwissUniverse::drawCell(float x, float y, float half, int m, const vec3& c, float a, bool snap) {
    const float g = GAP * half * 2.f;
    const float x0 = x - half + ((m & 1) ? 0.f : g), x1 = x + half - ((m & 2) ? 0.f : g);
    const float yb = y - half + ((m & 4) ? 0.f : g), yt = y + half - ((m & 8) ? 0.f : g);
    vec2 p0 = px(x0, yt), p1 = px(x1, yb);
    vec2 gi = px(x0 + g, yt - g), go = px(x1 - g, yb + g);
    if (snap) {
        p0 = vec2{std::round(p0.x), std::round(p0.y)};
        p1 = vec2{std::round(p1.x), std::round(p1.y)};
        gi = vec2{std::round(gi.x), std::round(gi.y)};
        go = vec2{std::round(go.x), std::round(go.y)};
    }
    const bool cutBL = (m & 1) && (m & 4) && !(m & 16), cutBR = (m & 2) && (m & 4) && !(m & 32);
    const bool cutTL = (m & 1) && (m & 8) && !(m & 64), cutTR = (m & 2) && (m & 8) && !(m & 128);
    if (!cutBL && !cutBR && !cutTL && !cutTR) { rectPx(p0.x, p0.y, p1.x, p1.y, c, a); return; }
    // three slabs: top (y p0..gi), middle, bottom (go..p1)
    rectPx(cutTL ? gi.x : p0.x, p0.y, cutTR ? go.x : p1.x, gi.y, c, a);
    rectPx(p0.x, gi.y, p1.x, go.y, c, a);
    rectPx(cutBL ? gi.x : p0.x, go.y, cutBR ? go.x : p1.x, p1.y, c, a);
}

void SwissUniverse::drawBoard(const Game& g) {
    const float br = saturate(bridge_.amount);
    const vec3 bInk = ugl::toSrgb(bridge_.ink);
    auto tone = [&](const vec3& c) { return lerp(c, bInk, smoothstepf(0.f, 1.f, br)); };
    auto mask8 = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };

    // the well's frame: two rules at its sides, the baseline under it across the poster's game band
    {
        const float th = std::max(1.f, height_ / 1080.f * 1.5f);
        const vec3 c = tone(ink_);
        const vec2 a = px(-5.f, 10.f), b = px(5.f, -10.f);
        const float al = 0.55f * (1.f - 0.6f * br);
        rectPx(std::round(a.x) - 2.f * th, a.y, std::round(a.x) - th, b.y + th, c, al);
        rectPx(std::round(b.x) + th, a.y, std::round(b.x) + 2.f * th, b.y + th, c, al);
        rectPx(std::round(a.x) - 2.f * th, std::round(b.y) + th, std::round(b.x) + 2.f * th, std::round(b.y) + 2.f * th, c, al);
    }

    // the ghost: the piece's outline, a fine rule along the edge of the piece's flat shape (inset by the gutter like
    // a locked piece), one closed line: a side runs to the inset corner where the piece turns outward, straight on
    // into its neighbor's side where the piece continues, and across its neighbor's rule where the piece turns
    // inward (a concave corner), so every corner closes.
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto in = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            const float th = std::max(1.f, std::round(height_ / 1080.f * 2.f));
            const vec3 c = tone(lerp(ground_, ink_, 0.45f));
            auto X = [&](float bx) { return std::round(px(bx, 0.f).x); };
            auto Y = [&](float by) { return std::round(px(0.f, by).y); };
            const float gg = GAP;
            for (auto& cc : gc) {
                if (cc[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)cc[0], (float)cc[1]);
                auto has = [&](int dx, int dy) { return in(cc[0] + dx, cc[1] + dy); };
                const float xl = w.x - 0.5f, xr = w.x + 0.5f, yb = w.y - 0.5f, yt = w.y + 0.5f;
                // the inset outline (pixels, y down)
                const float L = X(xl + gg), R = X(xr - gg), T = Y(yt - gg), B = Y(yb + gg);
                // a side's ends: the inset corner (convex), the cell's edge (straight on), or across the next
                // side's rule (concave). Game y grows downward: dy -1 is above.
                auto topEnd = [&](int ox) { return !has(0, -1) ? T : has(ox, -1) ? Y(yt + gg) - th : Y(yt); };
                auto botEnd = [&](int ox) { return !has(0, 1) ? B : has(ox, 1) ? Y(yb - gg) + th : Y(yb); };
                auto leftEnd = [&](int oy) { return !has(-1, 0) ? L : has(-1, oy) ? X(xl - gg) - th : X(xl); };
                auto rightEnd = [&](int oy) { return !has(1, 0) ? R : has(1, oy) ? X(xr + gg) + th : X(xr); };
                if (!has(-1, 0)) rectPx(L, topEnd(-1), L + th, botEnd(-1), c, 1.f);
                if (!has(1, 0)) rectPx(R - th, topEnd(1), R, botEnd(1), c, 1.f);
                if (!has(0, -1)) rectPx(leftEnd(-1), T, rightEnd(-1), T + th, c, 1.f);
                if (!has(0, 1)) rectPx(leftEnd(1), B - th, rightEnd(1), B, c, 1.f);
            }
        }
    }

    // locked pieces
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            auto same = [&](int nx, int ny) {
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H) return false;
                if (std::fabs(g.rowOffset(ny) - off) > 0.02f) return false;
                if (ids_[y][x] && ids_[ny][nx]) return ids_[ny][nx] == ids_[y][x];
                return false;
            };
            const vec3 p = layout::cellPos((float)x, y - off);
            drawCell(p.x, p.y, 0.5f, mask8(x, y, same), tone(pieceColor(c.type, ids_[y][x] ? pal_[y][x] : palIdx_)), 1.f);
        }

    // cleared rows: a flat bar sliding out sideways, clipped at the well's edges
    {
        const vec2 a = px(-5.f, 10.f), b = px(5.f, -10.f);
        const float clip[4] = {std::round(a.x), -1e5f, std::round(b.x), 1e5f};
        for (const Wipe& w : wipes_) {
            if (w.t < w.delay) {
                for (int x = 0; x < Game::W; x++) {
                    if (w.type[x] < 0) continue;
                    const vec3 p = layout::cellPos((float)x, 0.f);
                    drawCell(p.x, w.y, 0.5f, w.mask[x], tone(pieceColor(w.type[x], w.pal[x])), 1.f);
                }
                continue;
            }
            const float k = saturate((w.t - w.delay) / WIPE_S);
            const float dx = w.dir * (k * k) * 10.5f;
            for (int x = 0; x < Game::W; x++) {
                if (w.type[x] < 0) continue;
                const vec3 p = layout::cellPos((float)x, 0.f);
                vec2 p0 = px(p.x - 0.5f + dx, w.y + 0.5f - GAP), p1 = px(p.x + 0.5f + dx, w.y - 0.5f + GAP);
                rectPxClip(std::round(p0.x), std::round(p0.y), std::round(p1.x), std::round(p1.y),
                           tone(pieceColor(w.type[x], w.pal[x])), 1.f, clip);
            }
        }
    }

    // the falling piece: exactly a locked piece, in the current palette
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto in = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            drawCell(w.x, w.y, 0.5f, mask8(c[0], c[1], in), tone(pieceColor(p.type, palIdx_)), 1.f);
        }
    }

    // the hard drop's rule at the contact
    if (lockLine_ > 0.f) {
        const float k = 1.f - lockLine_;
        const float cx = 0.5f * (lockX0_ + lockX1_), hw = (0.5f * (lockX1_ - lockX0_) + 0.4f) * (1.f - ease(k));
        const float th = std::max(1.f, height_ / 1080.f * 2.f);
        const vec2 a = px(cx - hw, lockY_), b = px(cx + hw, lockY_);
        rectPx(a.x, a.y + th, b.x, a.y + 2.f * th, tone(ink_), 0.9f);
    }

    // previews: small flat pieces in the HUD columns
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
        auto in = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        for (auto& c : cells) {
            const float x = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, y = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            drawCell(x, y, 0.5f * sc, mask8(c[0], c[1], in), tone(pieceColor(type, palIdx_)), alpha);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
    flushShapes();
}

// =====================================================================================================
// The HUD
// =====================================================================================================

// Inter on the poster's ground: labels small and spaced in capitals, values bold, the section in the signature
// color; the bonus gauge a fine ring whose charge is drawn in the signature color.
void SwissUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    const vec3 ink = ink_, ground = ground_, accent = palShown_[1];
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : ground;
            rectPx(r.x, r.y, r.x + r.w, r.y + r.h, c, std::min(1.f, r.alpha));
        }
        flushShapes();
    }

    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const float th = std::max(1.f, 1.5f * s), th2 = std::max(2.f, 5.f * s);
        ringPx(c.x, c.y, r - th * 0.5f, r + th * 0.5f, 1.f, ink, (g.bonusReady() ? 0.7f : 0.3f) * vis);
        ringPx(c.x, c.y, r - th2 * 0.5f, r + th2 * 0.5f, fill, accent, 0.95f * vis);
        flushShapes();
    }

    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha) * vis;
        q.clear();
        // cap: cap height in pixels, dy: shift of the top of the caps from the anchor
        auto place = [&](const Font& f, const std::string& text, float cap, float track, float dy = 0.f) {
            const float w = capWidth(f, text, track) * cap;
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy + cap, cap, track, 0, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(reg_, upper(h.text), base * 0.5f, 0.08f, base * 0.1f);
            drawText(reg_, q, ink, saturate(0.35f + h.alpha * 1.4f) * vis);
            continue;
        case HUD_SECTION:
            place(bold_, upper(h.text), base * 0.55f, 0.06f, base * 0.05f);
            drawText(bold_, q, accent, saturate(0.4f + h.alpha * 1.5f) * vis);
            continue;
        case HUD_TITLE:
            place(bold_, h.text, base * 0.62f, -0.01f, base * 0.02f);
            drawText(bold_, q, ink, saturate(h.alpha * 1.6f) * vis);
            continue;
        case HUD_INFO:
            place(reg_, h.text, base * 0.48f, 0.06f, base * 0.1f);
            drawText(reg_, q, ink, saturate(alpha * 1.3f) * 0.8f);
            continue;
        case HUD_LABEL:
            place(reg_, upper(h.text), base * 0.42f, 0.14f, base * 0.12f);
            drawText(reg_, q, ink, saturate(alpha * 1.3f) * 0.75f);
            continue;
        case HUD_VALUE:
            place(bold_, h.text, base * 0.62f, -0.01f, base * 0.02f);
            drawText(bold_, q, ink, alpha);
            continue;
        case HUD_NOTICE:
            place(bold_, h.text, base * 0.55f, 0.02f);
            drawText(bold_, q, accent, alpha);
            continue;
        case HUD_KEY:
            place(bold_, h.text, base * 0.55f, 0.f);
            drawText(bold_, q, ink, alpha);
            continue;
        case HUD_TOAST:
            place(bold_, lower(h.text), base * 0.72f, -0.02f, -base * 0.05f);
            drawText(bold_, q, ink, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(reg_, h.text, base * 0.5f, 0.04f);
            drawText(reg_, q, accent, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(bold_, h.text, base * 0.62f, -0.01f);
                drawText(bold_, q, ink, alpha);
            } else {
                place(reg_, h.text, base * 0.5f, 0.03f);
                drawText(reg_, q, h.accent ? accent : ink, alpha);
            }
            continue;
        default:
            place(reg_, h.text, base * 0.5f, 0.02f);
            drawText(reg_, q, h.accent ? accent : ink, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void SwissUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const VariantDef& V = VARIANTS[variant_];
    bare_ = f.bare;
    if (!bare_) syncIds(game); // the world alone: the pieces' ids kept for the whole frame
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) {
            overT_ = -1.f;
            wipes_.erase(std::remove_if(wipes_.begin(), wipes_.end(), [](const Wipe& w) { return w.delay > 0.f; }), wipes_.end());
        } else overT_ += dt;
    }
    time_ += dt;
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }

    // ---- What the song says: its title (and artist), its information, its section.
    for (const HudText& h : f.hud) {
        if (h.role == HUD_TITLE) {
            std::string t = ascii(h.text), a;
            // brackets go, "artist - title" splits
            std::string clean;
            int depth = 0;
            for (char c : t) {
                if (c == '(' || c == '[') depth++;
                else if (c == ')' || c == ']') depth = std::max(0, depth - 1);
                else if (!depth) clean += (c == '_' ? ' ' : c);
            }
            const size_t dash = clean.find(" - ");
            if (dash != std::string::npos) {
                a = clean.substr(0, dash);
                clean = clean.substr(dash + 3);
            }
            auto trim = [](std::string x) {
                while (!x.empty() && x.front() == ' ') x.erase(x.begin());
                while (!x.empty() && x.back() == ' ') x.pop_back();
                return x;
            };
            title_ = lower(trim(clean));
            artist_ = trim(a);
        } else if (h.role == HUD_INFO) {
            const std::string t = ascii(h.text);
            const size_t sp = t.find("   ");
            info1_ = upper(sp == std::string::npos ? t : t.substr(0, sp));
            info2_ = sp == std::string::npos ? std::string() : upper(t.substr(sp + 3));
            while (!info2_.empty() && info2_.front() == ' ') info2_.erase(info2_.begin());
        } else if (h.role == HUD_SECTION) {
            section_ = upper(ascii(h.text));
        }
    }

    // ---- The palette: the section's (the drop's in a drop), the poster easing to it.
    {
        int p = sectionPalette(section_);
        if (p < 0) p = level_ == 0 ? 0 : level_ == 1 ? 1 : 3;
        if (dropMode_) p = 4;
        palIdx_ = p;
        const vec3 g0 = hex(V.ground), i0 = hex(V.ink);
        // the negative print: slow motion eases into it; the drop cuts to it on its downbeat, holds it while it
        // lasts and eases back after (both at once print positive again)
        dropNeg_ = dropMode_ ? 1.f : approach(dropNeg_, 0.f, 1.25f, dt);
        const float sn = smoothstepf(0.f, 1.f, slow_), dn = smoothstepf(0.f, 1.f, dropNeg_);
        const float neg = sn + dn - 2.f * sn * dn;
        const vec3 gT = lerp(g0, i0, neg), iT = lerp(i0, g0, neg);
        const vec3 targ[4] = {iT, hex(V.pal[p][0]), hex(V.pal[p][1]), hex(V.pal[p][2])};
        if (!palInit_ || dropPending_) { // the drop cuts to its palette
            for (int i = 0; i < 4; i++) palShown_[i] = targ[i];
            palInit_ = true;
        }
        const float k = 1.f - std::exp(-dt * (dropMode_ ? 3.f : 1.6f));
        for (int i = 0; i < 4; i++) palShown_[i] = lerp(palShown_[i], targ[i], k);
        ground_ = gT;
        ink_ = iT;
        palShown_[0] = iT;
    }

    // ---- The bar grid: a new layout on every bar, a step every two beats.
    {
        const double beat = music.hasSong ? music.beat : time_ * 2.0;
        const long bar = (long)std::floor(beat / 4.0);
        const long step = (long)std::floor(beat / 2.0);
        const float barS = 4.f * 60.f / (music.hasSong ? std::max(40.f, music.bpm) : 120.f);
        if (bar_ <= -1000) {
            bar_ = bar;
            compose(bar, 0.f, true);
        } else if (dropPending_) {
            dropPending_ = false;
            bar_ = bar;
            compose(bar, 0.f, true); // a hard cut on the downbeat
        } else if (bar != bar_ && overT_ < 0.f) {
            bar_ = bar;
            compose(bar, std::clamp(barS * 0.75f, 1.2f, 2.4f), false);
        }
        if (step != step_) {
            step_ = step;
            // a rule steps along the grid, the small square along its rule
            if (lv_ > 0.5f && !f.paused) {
                const int r = E_RULE0 + (int)(((step % 3) + 3) % 3);
                Element& e = el_[r];
                if (e.to.on && e.t >= 1.f) {
                    Target t = e.to;
                    t.v[1] = e.ruleBase + (((step / 3) & 1) ? -1.f : 0.f) * (r == E_RULE0 ? 0.f : 1.f);
                    if (r == E_RULE0) { t.v[2] = t.v[0] + std::max(2.f, (t.v[2] - t.v[0]) + (((step / 3) & 1) ? -2.f : 2.f)); }
                    retarget(r, t, 0.4f, false);
                }
                Element& b = el_[E_BULLET];
                if (b.to.on && b.t >= 1.f) {
                    Target t = b.to;
                    const float x0 = el_[E_RULE0].to.v[0];
                    const float k = (float)(((step % 4) + 4) % 4);
                    t.v[0] = x0 + k;
                    t.v[2] = t.v[0] + 0.7f;
                    retarget(E_BULLET, t, 0.35f, false);
                }
            }
        }
    }
    updateElements(dt);

    // ---- Kicks (peak only): the rules thicken for an instant.
    {
        const float kick = std::max(music.kick, kickIn_);
        kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
        const float wp = smoothstepf(1.2f, 2.f, lv_);
        kickShown_ = std::max(kickShown_ * std::exp(-dt * 7.f), kick * wp);
    }
    lockLine_ = std::max(0.f, lockLine_ - dt / 0.28f);
    for (Wipe& w : wipes_) w.t += dt;
    wipes_.erase(std::remove_if(wipes_.begin(), wipes_.end(), [](const Wipe& w) { return w.t > w.delay + WIPE_S; }), wipes_.end());
    if (!bare_) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // ---- The poster.
    posterT_.bind();
    glClearColor(ground_.x, ground_.y, ground_.z, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawPoster();

    // ---- On screen: the poster, faint behind the well and the HUD's boxes.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_BLEND);
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, posterT_.tex);
    set1i(progComp_, "uPoster", 0);
    set2f(progComp_, "uRes", (float)width_, (float)height_);
    set3f(progComp_, "uGround", ground_);
    {
        std::vector<float> rects, soft;
        auto add = [&](float x0, float y0, float x1, float y1, float feather, float k) {
            if (rects.size() / 4 >= 24) return;
            rects.insert(rects.end(), {x0, y0, x1, y1});
            soft.insert(soft.end(), {feather, k});
        };
        // The veil covers only what is played or read: the well, and a crisp box behind each HUD element
        // (the hold and next previews, the gauge, each group of lettering), so the poster runs at full strength
        // everywhere else. Boxes that touch are merged, so a column of lines reads as one block.
        const float pad = 12.f * height_ / 1080.f;
        std::vector<vec4> boxes;
        auto boardBox = [&](float x0, float y0, float x1, float y1) {
            const vec2 p0 = px(x0, y1), p1 = px(x1, y0);
            boxes.push_back({p0.x, p0.y, p1.x, p1.y});
        };
        const vec2 a = px(-5.45f, 10.f), b = px(5.45f, -10.45f);
        add(a.x, a.y, b.x, b.y, 1.f, VEIL); // the well, from its top edge to just under its baseline
        boardBox(layout::HOLD_CENTER.x - 2.f, layout::HOLD_CENTER.y - 1.3f, layout::HOLD_CENTER.x + 2.f, layout::HOLD_LABEL.y + 0.7f);
        boardBox(layout::nextCenter(0).x - 2.f, layout::nextCenter(2).y - 1.1f, layout::nextCenter(0).x + 2.f, layout::NEXT_LABEL.y + 0.7f);
        boardBox(layout::GAUGE_CENTER.x - 2.f, layout::GAUGE_CENTER.y - 1.9f, layout::GAUGE_CENTER.x + 2.f, layout::GAUGE_CENTER.y + 1.3f);
        for (const HudText& h : f.hud) {
            if (h.alpha <= 0.02f || h.text.empty()) continue;
            const float base = h.scale * 12.f;
            const float w = capWidth(reg_, ascii(h.text), 0.06f) * base * 0.6f;
            const float x0 = ugl::hudLeft(h, w, width_);
            const vec4 r{x0 - pad, h.y - pad, x0 + w + pad, h.y + base * 0.8f + pad};
            if (r.z > a.x && r.x < b.x) continue; // over the well, veiled already
            boxes.push_back(r);
        }
        // merge boxes that touch or overlap (until stable)
        for (bool merged = true; merged;) {
            merged = false;
            for (size_t i = 0; i < boxes.size() && !merged; i++)
                for (size_t j = i + 1; j < boxes.size() && !merged; j++) {
                    vec4 &p = boxes[i], &q = boxes[j];
                    if (p.x <= q.z + 1.f && q.x <= p.z + 1.f && p.y <= q.w + 1.f && q.y <= p.w + 1.f) {
                        p = {std::min(p.x, q.x), std::min(p.y, q.y), std::max(p.z, q.z), std::max(p.w, q.w)};
                        boxes.erase(boxes.begin() + j);
                        merged = true;
                    }
                }
        }
        for (const vec4& r : boxes) add(r.x, r.y, r.z, r.w, 1.f, VEIL);
        const int n = bare_ ? 0 : (int)rects.size() / 4; // the world alone: the poster unveiled
        set1i(progComp_, "uCount", n);
        if (n) {
            glUniform4fv(ugl::U(progComp_, "uRects"), n, rects.data());
            glUniform2fv(ugl::U(progComp_, "uSoft"), n, soft.data());
        }
    }
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // ---- The bridge: the world gives way to the bridge's color (the stack turns to its ink in drawBoard).
    if (bridge_.amount > 0.001f) {
        rectPx(0, 0, (float)width_, (float)height_, ugl::toSrgb(bridge_.color), smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
        flushShapes();
    }

    // ---- The game.
    if (!bare_) drawBoard(game);

    // ---- Dim (pause, game over screen) and the startup fade.
    {
        const float d = 0.45f * dimT_;
        if (d > 0.002f) rectPx(0, 0, (float)width_, (float)height_, ground_, d);
        if (f.fade < 0.999f) rectPx(0, 0, (float)width_, (float)height_, vec3(0.f), 1.f - saturate(f.fade));
        flushShapes();
    }
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_swiss() {
    static const UniverseEntry e{
        "SWISS POSTER", "a Swiss typographic poster: the song's words and flat color fields recompose on the bar grid, the pieces flat blocks in each section's palette",
        [] { return std::unique_ptr<Universe>(new SwissUniverse()); },
        // Graphic and clean: it suits most songs, steady, rhythmic ones best.
        [](const SongLook& l) { return 0.8f + 0.2f * l.density + 0.1f * (1.f - l.dynamics); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
