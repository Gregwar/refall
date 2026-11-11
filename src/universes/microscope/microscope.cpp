// MICROSCOPE: a Petri dish seen through the eyepiece of a microscope, filled with life by the cleared rows.
//
// One world: the round field of view of the eyepiece on black, its edge soft with a faint colored fringe. Bright-field
// variants light the field evenly in a pale, neutral tone and show the life stained and absorbing; dark-field ones
// leave the field black and show only the light scattered by the edges of what is in it (glowing walls, cilia,
// glass). Out-of-focus debris floats faintly in the medium; along the well run the eyepiece's reticle (two fine etched
// lines, a floor and a scale of ticks, one per row).
//
// The pieces are plant tissue: each block one calm cell, a rounded cube of evenly stained cytoplasm in its glassy wall,
// nothing else inside. The seven types are seven dyes of the variant's stain. The falling piece is exactly a locked one, the ghost
// a faint wall, the previews the same cells.
//
// About one cell in five has a microorganism living inside it, shown large in a clear pool as one bold shape: the one
// that cell frees (the other cells are empty and still). The dish starts empty at the start of a song. Each cleared row bursts (its cells round off,
// their walls break open and their content disperses, gone in a third of a second) and frees its organisms (a row without one frees nothing): each
// swims out of its own cell to the nearer side, growing on the way, and lives in the dish for the rest of the song: paramecia (a slipper fringed with beating
// cilia), amoebas (a slow blob reaching out with pseudopods), pennate and centric diatoms (glass boats and discs of
// fine striae), rotifers (wheels of cilia on a telescoping body), and volvox colonies (rolling spheres of tiny cells,
// daughter colonies inside; a Tetris releases a big one). They roam the sides of the dish, steering clear of the well
// and the HUD, at their own depth: the focal plane drifts, so some of them are soft, out of focus. Beyond a cap the
// oldest fade away. A new song rinses the dish.
//
// The song: calm parts are a still, dimmer dish with soft focus and slow life; mid wakes it up, the cilia beating on
// the beat grid (every two beats); a peak brings brighter light, crisp focus and lively swimmers, the kicks flicking
// the cilia and the bars turning the swimmers. The drop changes the stain: a new staining palette (the variant's
// second dye) soaks unevenly into the whole dish, the tissue and the life, with one swell of light; it is rinsed out
// slowly after the peak. Slow motion switches the illumination to a silver dark-field, time stretched. Game over
// kills the tissue: from the top, each cell's protoplast shrinks away from its wall (plasmolysis), the stain drains,
// and it fades. Exit and entrance: the dish goes out of focus and fades to the bridge.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/microscope/microscope.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/microscope/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Lato (labels, numbers) and EB Garamond Italic (the song, like a species' name), SIL Open Font License, see
// OFL-Lato.txt and OFL-EBGaramond.txt.
const unsigned char FONT_SANS[] = {
#include "universes/microscope/Lato-Regular.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/microscope/EBGaramond12-Italic.ttf.inc"
};

namespace shaders = microscope_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// A stain (sRGB): the field (center, edge), the seven piece types (I O T S Z J L), the organisms (body, rim,
// organelles).
struct StainDef {
    unsigned field[2];
    unsigned piece[7];
    unsigned org[3];
};
// A variant: its illumination, its own stain and the one the drop washes in, the HUD's accent.
struct VariantDef {
    const char* name;
    bool dark;
    StainDef own, drop;
    unsigned accent;
};
const VariantDef VARIANTS[] = {
    // light scattered on black: cool pastel cells, silver life; the drop: a fluorescent label
    {"DARK FIELD", true,
     {{0x040609, 0x010202}, {0x7fd8f0, 0xf0d070, 0xb69af2, 0x7fe0a6, 0xf08a7e, 0x7f9ef4, 0xf2ac66}, {0x9fb4c4, 0xdfe9f2, 0xf2e6c8}},
     {{0x030604, 0x010201}, {0x3fe0ff, 0xffe24a, 0xd070ff, 0x4cf07c, 0xff5a70, 0x5a7aff, 0xff9a3a}, {0x3aa860, 0x6cff9a, 0x7a96ff}},
     0xa8dcef},
    // fluorophores on black: GFP, DAPI, mCherry...; the drop: a second channel, rhodamine
    {"FLUORESCENCE", true,
     {{0x030308, 0x010102}, {0x52c8ff, 0xf2e650, 0xc85af0, 0x4ef08a, 0xff4e6e, 0x5c6cff, 0xff8a3c}, {0x2c9a5a, 0x5cf08e, 0x7a8cff}},
     {{0x060305, 0x020101}, {0x6ac8ff, 0xffd46a, 0xff6ad8, 0xc8f05a, 0xff5a4a, 0x8a6aff, 0xffa04a}, {0xa83a5a, 0xff7a9a, 0x7a96ff}},
     0x6cf09a},
    // crossed polarizers: interference colors on black; the drop: the next order of colors
    {"POLARIZED", true,
     {{0x040405, 0x010101}, {0x5ab0f0, 0xf2d24a, 0xd05ac8, 0x52d8a0, 0xf27a4a, 0x8a7af0, 0xe8e0cc}, {0x8a8478, 0xe8e2d2, 0xf0c870}},
     {{0x040306, 0x010101}, {0x40d0e0, 0xf2a040, 0xf05a8a, 0x9ae060, 0xff6a50, 0xb06af0, 0xf0e070}, {0x8a6a90, 0xf0d2f0, 0x9ae0c0}},
     0xf0c870},
    // bright-field, methylene blue with a safranin counterstain; the drop: eosin
    {"METHYLENE BLUE", false,
     {{0xeef1f0, 0xd6dde1}, {0x2f7fd0, 0xd8a23a, 0x6448b8, 0x1f9e96, 0xc8405a, 0x23367e, 0xe0703a}, {0x3c6ea8, 0x1f3d70, 0x14284e}},
     {{0xf2f0f0, 0xdedadc}, {0xd84a7a, 0xf09a5a, 0x8a3aa0, 0x3a9a8a, 0xc02a3a, 0x4a3a8a, 0xe8b040}, {0xc8507a, 0x8a2050, 0x4a2a6a}},
     0x2f6fc0},
    // bright-field, hematoxylin and eosin; the drop: a Gram stain (crystal violet, iodine, safranin)
    {"H&E", false,
     {{0xf0f1f0, 0xdcdedf}, {0x6a4ab0, 0xe86a8a, 0xa03a8a, 0x5a8ad0, 0xd04a4a, 0x3a2a6a, 0xf0a070}, {0xb05a8a, 0x6a2a5a, 0x3a1a4a}},
     {{0xf0f0f2, 0xdadae2}, {0x5a3ac8, 0xe0a040, 0xd84a8a, 0x3a8ab0, 0xc83a3a, 0x2a2a7a, 0xe87a5a}, {0x6a4ab8, 0x3a2a7a, 0x2a1a4a}},
     0xb04a7a},
    // bright-field, malachite green and safranin; the drop: methylene blue
    {"MALACHITE", false,
     {{0xeef2ee, 0xd8dfda}, {0x2a9a6a, 0xe0b040, 0x7a4aa8, 0x7ac04a, 0xd04a5a, 0x1f5a6a, 0xe8804a}, {0x3a8a5a, 0x1f4a3a, 0x8a2a3a}},
     {{0xeef0f2, 0xd6dce2}, {0x2f7fd0, 0xe0b040, 0x6448b8, 0x1f9e96, 0xc8405a, 0x23367e, 0xe0703a}, {0x3c6ea8, 0x1f3d70, 0x14284e}},
     0x2a8a5a},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float LYSE_T = 0.32f;      // a cleared cell is gone in this long (real time)
constexpr float DIE_T = 2.2f;        // game over: a cell's plasmolysis
constexpr int MAX_ORGS = 30;         // the dish's population; beyond it, the oldest fade away
constexpr float ORG_FADE = 4.f;
constexpr float LIFE_SHARE = 0.22f;   // the share of the cells with an organism living inside
constexpr float DWELL_SIZE = 0.82f;   // an organism's size inside its cell (one bold shape, in a clear pool)

// The kinds of life: paramecium, amoeba, pennate diatom, centric diatom, rotifer, volvox.
enum { K_PARA, K_AMOEBA, K_PENNATE, K_CENTRIC, K_ROTIFER, K_VOLVOX, K_COUNT };
const float KIND_SIZE[K_COUNT][2] = {{2.3f, 2.9f}, {2.3f, 2.9f}, {2.0f, 2.7f}, {1.7f, 2.2f}, {2.5f, 3.1f}, {2.0f, 2.5f}};
const float KIND_SPEED[K_COUNT] = {0.55f, 0.07f, 0.14f, 0.05f, 0.22f, 0.16f};
const float KIND_TURN[K_COUNT] = {0.5f, 0.25f, 0.06f, 0.15f, 0.35f, 0.25f};

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
int glyphIndex(unsigned char ch) {
    if (ch < 32 || ch >= 128) ch = '?';
    return ch - 32;
}
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }
float wrapAngle(float a) {
    while (a > PI) a -= TAU;
    while (a < -PI) a += TAU;
    return a;
}
float sdBox(vec2 p, vec2 c, vec2 h) {
    const float dx = std::fabs(p.x - c.x) - h.x, dy = std::fabs(p.y - c.y) - h.y;
    const float ox = std::max(dx, 0.f), oy = std::max(dy, 0.f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.f);
}

// A VAO over the unit quad with `n` per-instance vec4 attributes (locations 1..n) of `stride` bytes.
void instVao(GLuint quad, GLuint& vao, GLuint& vbo, int n, size_t stride) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < n; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool MicroscopeUniverse::init(int width, int height) {
    using shaders::withCommon;
    for (auto& l : fallLook_) l = lookOf(0, 0);
    const std::string inkText = withCommon(shaders::TEXT_FS, shaders::INK_GLSL);
    const std::string inkShape = withCommon(shaders::SHAPE_FS, shaders::INK_GLSL);
    progField_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::FIELD_FS).c_str(), "microscope field");
    progOrg_ = ugl::program(shaders::ORG_VS, withCommon(shaders::ORG_FS).c_str(), "microscope life");
    progPiece_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::PIECE_FS).c_str(), "microscope cells");
    progSpeck_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::SPECK_FS).c_str(), "microscope specks");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "microscope down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "microscope blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "microscope final");
    progShape_ = ugl::program(shaders::SHAPE_VS, inkShape.c_str(), "microscope shape");
    progText_ = ugl::program(shaders::TEXT_VS, inkText.c_str(), "microscope text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    instVao(quadVbo_, pieceVao_, pieceVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, orgVao_, orgVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, speckVao_, speckVbo_, 4, sizeof(Inst));
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

    buildFont(fontSans_, FONT_SANS);
    buildFont(fontIt_, FONT_IT);
    resize(width, height);
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void MicroscopeUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] microscope font init failed\n");
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

void MicroscopeUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    // the eyepiece's field: as wide as the screen allows, always around the HUD's columns
    diskR_ = std::max(hw * 0.95f, 15.6f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
}

MicroscopeUniverse::~MicroscopeUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progField_, progOrg_, progPiece_, progSpeck_, progDown_, progBlur_, progFinal_, progShape_, progText_})
        glDeleteProgram(p);
    for (GLuint v : {pieceVao_, orgVao_, speckVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, pieceVbo_, orgVbo_, speckVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontSans_, &fontIt_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void MicroscopeUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        // darker songs look in dark-field (light on black), brighter ones in bright-field (stains on a pale field)
        const bool dark = !t.hasSong || t.brightness < 0.55f || (t.minor && t.brightness < 0.62f);
        const int pick = (int)(h * 3.f) % 3;
        if (dark) v = t.hasSong && t.dynamics > 0.62f ? 1 : t.hasSong && t.airWeight > 0.6f ? 2 : pick;
        else v = 3 + (t.minor ? (pick == 0 ? 1 : pick) : pick);
        v %= VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
    }
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    auto fill = [&](Stain& s, const StainDef& d) {
        for (int i = 0; i < 2; i++) s.field[i] = lin(d.field[i]);
        for (int i = 0; i < 7; i++) s.piece[i] = lin(d.piece[i]);
        for (int i = 0; i < 3; i++) s.org[i] = lin(d.org[i]);
    };
    fill(baseT_, V.own);
    fill(dropT_, V.drop);
    darkT_ = V.dark ? 1.f : 0.f;
    const bool instant = !colorsSet_ || seconds <= 0.1f;
    if (instant) {
        base_ = baseT_;
        dropS_ = dropT_;
        dark_ = darkT_;
        colorsSet_ = true;
    }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    rng_ = Rng(seed_ ^ 0x3C1C0Bull);
    // a new song: the dish is rinsed, empty again
    if (instant) orgs_.clear();
    else
        for (Org& o : orgs_)
            if (o.fade < 0.f) o.fade = 0.f;
    pending_.clear();
    stain_ = 0.f;
    dropMode_ = false;
    focus_ = focusT_ = 0.f;
}

void MicroscopeUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    (void)energy;
    (void)impact;
    if (level < 2) dropMode_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    if (level != level_) focusT_ = rng_.range(-0.5f, 0.5f);   // a new section: the observer refocuses
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

// The drop: the variant's second stain soaks into the dish, with one swell of light and a refocus.
void MicroscopeUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    focusT_ = rng_.range(-0.3f, 0.3f);
}

void MicroscopeUniverse::refresh(float strength) {
    (void)strength;
    focusT_ = rng_.range(-0.6f, 0.6f);
}

vec2 MicroscopeUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints MicroscopeUniverse::hints() const {
    TransitionHints h;
    const bool dark = dark_ > 0.5f;
    h.light = !dark;
    h.background = dark ? lerp(base_.field[0], base_.field[1], 0.5f) : base_.field[0];
    vec3 ink(0.f);
    for (int i = 0; i < 7; i++) ink = ink + base_.piece[i] * (1.f / 7.f);
    h.ink = dark ? ink : ink * 0.7f;
    h.accent = lin(VARIANTS[variant_].accent);
    h.styles = TS_LUMA | TS_INK | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

MicroscopeUniverse::CellLook MicroscopeUniverse::lookOf(int x, int y) {
    (void)x;
    (void)y;
    CellLook l;
    Rng& r = rng_;
    const float a = r.range(0.f, TAU), d = r.range(0.05f, 0.15f);
    l.nx = std::cos(a) * d;
    l.ny = std::sin(a) * d;
    l.seed = r.uniform();
    if (r.chance(LIFE_SHARE)) l.life = pickKind();
    return l;
}

// Keeps the looks in step with the board (a new board, a stack inherited from another universe).
void MicroscopeUniverse::syncLooks(const Game& g) {
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const bool on = g.cell(x, y).type >= 0;
            if (on && !has_[y][x]) look_[y][x] = lookOf(x, y);
            has_[y][x] = on;
        }
}

int MicroscopeUniverse::pickKind() {
    const float w[K_COUNT] = {0.27f, 0.15f, 0.2f, 0.14f, 0.16f, 0.08f};
    int k = rng_.weighted(w);
    // not the same kind three times in a row
    if (orgs_.size() >= 2 && orgs_[orgs_.size() - 1].kind == k && orgs_[orgs_.size() - 2].kind == k) k = (k + 1 + rng_.irange(0, 3)) % K_COUNT;
    return k;
}

// A cell's organism as it is now: small, turning slowly about the middle of the cell (the same on every frame for a
// given look and time, so that it leaves the cell exactly as it was in it).
MicroscopeUniverse::Org MicroscopeUniverse::dweller(const CellLook& l, float x, float y, int type) const {
    Org o;
    const int kind = std::clamp(l.life, 0, (int)K_COUNT - 1);
    const float t = worldTime_ + l.seed * 50.f, s = l.seed * 37.f;
    o.kind = kind;
    o.seed = l.seed;
    o.size = DWELL_SIZE;
    o.p = vec2{x + 0.07f * std::sin(t * 0.31f + s), y + 0.07f * std::cos(t * 0.27f + s * 1.3f)};
    o.ang = s + t * (l.seed < 0.5f ? 0.23f : -0.23f) + 0.6f * std::sin(t * 0.17f + s);
    o.type = std::clamp(type, 0, 6);
    o.depth = 0.f;
    o.turnPhase = s;
    o.tint = 0.1f;   // its own colors, not the cell's dye (it would vanish in it)
    o.spin = s;
    return o;
}

// The organism of a cleared cell leaves it: it darts out of the well toward the nearer side, growing to its full size
// on the way, and makes its home in the dish.
void MicroscopeUniverse::releaseFrom(const CellLook& l, float x, float y, int type, float delay, bool big) {
    Org o = dweller(l, x, y, type);
    const int kind = o.kind;
    o.size = big ? 3.3f : rng_.range(KIND_SIZE[kind][0], KIND_SIZE[kind][1]);
    o.grow0 = DWELL_SIZE / o.size;
    o.fromCell = true;
    const int side = x < 0.f ? -1 : 1;
    const float out = side > 0 ? 0.f : PI;
    if (kind != K_CENTRIC && kind != K_VOLVOX) {
        // it turns toward the way out (a pennate diatom may just back up)
        if (kind == K_PENNATE && std::cos(o.ang - out) < 0.f) o.dir = -1.f;
        o.ang = out + rng_.range(-0.4f, 0.4f) + (o.dir < 0.f ? PI : 0.f);
    } else o.ang = out + rng_.range(-0.4f, 0.4f);
    o.speed = KIND_SPEED[kind] * rng_.range(0.8f, 1.25f);
    o.depth = rng_.range(-0.6f, 0.6f);
    o.burst = 1.f;
    o.home = pickHome(side);
    pending_.push_back({delay, o});
}

// A place for an organism to live on one side of the dish: mostly the wide band beyond the HUD's column, sometimes
// the corner under it; the least crowded of a few tries.
vec2 MicroscopeUniverse::pickHome(int side) {
    vec2 best{side * 15.f, 0.f};
    float bestScore = -1e9f;
    for (int tryN = 0; tryN < 6; tryN++) {
        const float y = rng_.range(-9.f, 9.f);
        const float dy = y - viewC_.y;
        const float xmax = std::sqrt(std::max(0.f, (diskR_ - 2.2f) * (diskR_ - 2.2f) - dy * dy));
        const float x0 = 13.2f;
        if (xmax < x0 + 0.5f) continue;
        const vec2 h{side * rng_.range(x0, xmax), y};
        if (zoneDist(h, nullptr) < 1.2f) continue;
        float nearest = 99.f;
        for (const Org& o : orgs_) {
            const float dx = o.home.x - h.x, dy = o.home.y - h.y;
            nearest = std::min(nearest, std::sqrt(dx * dx + dy * dy));
        }
        if (nearest > bestScore) { bestScore = nearest; best = h; }
    }
    return best;
}

void MicroscopeUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        for (size_t i = 0; i < ev.cells.size() && i < 4; i++) {
            const auto& c = ev.cells[i];
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            look_[c.y][c.x] = fallLook_[i];
            has_[c.y][c.x] = true;
        }
        for (auto& l : fallLook_) l = lookOf(0, 0);
        break;
    }
    case GameEvent::HardDrop: {
        // fine debris pushed out from under the piece, at the contact
        if (!game.hasPiece()) break;
        Piece gp = game.piece();
        gp.y = game.ghostY();
        int cells[4][2];
        game.pieceCells(gp, cells);
        int low = -1, minX = 99, maxX = -1;
        for (auto& c : cells) low = std::max(low, c[1]);
        for (auto& c : cells)
            if (c[1] == low) { minX = std::min(minX, c[0]); maxX = std::max(maxX, c[0]); }
        if (low < Game::HIDDEN) break;
        const vec3 a = layout::cellPos((float)minX, (float)low), b = layout::cellPos((float)maxX, (float)low);
        for (int i = 0; i < 12; i++) {
            const float s = i % 2 ? 1.f : -1.f;
            Speck k;
            k.p = vec2{(s < 0 ? a.x - 0.45f : b.x + 0.45f) + rng_.range(-0.1f, 0.1f), a.y - 0.42f + rng_.range(0.f, 0.2f)};
            k.v = vec2{s * rng_.range(0.5f, 1.4f), rng_.range(0.05f, 0.5f)};
            k.t = 0.f;
            k.life = rng_.range(0.45f, 0.8f);
            k.r = rng_.range(0.05f, 0.1f);
            specks_.push_back(k);
        }
        break;
    }
    case GameEvent::Clear: {
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= Game::HIDDEN && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        std::sort(rows.begin(), rows.end());
        const bool tetris = ev.count >= 4;
        // the cells burst
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const CellLook l = has_[c.y][c.x] ? look_[c.y][c.x] : lookOf(c.x, c.y);
            lyses_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), l, 0.f});
        }
        // the organisms living in the cleared cells swim out (a row with none frees nothing: only the life shown in
        // the cells is ever released; a Tetris, a big one among them)
        for (size_t k = 0; k < rows.size(); k++) {
            std::vector<const GameEvent::CellInfo*> row, lives;
            for (auto& c : ev.cells)
                if (c.y == rows[k] && c.x >= 0 && c.x < Game::W) {
                    row.push_back(&c);
                    if (has_[c.y][c.x] && look_[c.y][c.x].life >= 0) lives.push_back(&c);
                }
            if (lives.empty()) continue;
            const size_t bigAt = tetris && k == 1 ? (size_t)rng_.irange(0, (int)lives.size() - 1) : lives.size();
            for (size_t i = 0; i < lives.size(); i++) {
                const auto* c = lives[i];
                CellLook l = look_[c->y][c->x];
                const vec3 p = layout::cellPos((float)c->x, (float)c->y);
                releaseFrom(l, p.x, p.y, c->type, 0.f, i == bigAt);
            }
        }
        if (tetris) swell_ = std::max(swell_, 0.55f);
        // the looks of the rows above step down
        {
            bool cleared[Game::H] = {};
            for (int y : rows) cleared[y] = true;
            for (auto& c : ev.cells)
                if (c.y >= 0 && c.y < Game::H) cleared[c.y] = true;
            CellLook nl[Game::H][Game::W];
            bool nh[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                for (int x = 0; x < Game::W; x++) { nl[dst][x] = look_[y][x]; nh[dst][x] = has_[y][x]; }
                dst--;
            }
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) { look_[y][x] = nl[y][x]; has_[y][x] = nh[y][x]; }
        }
        break;
    }
    case GameEvent::TopOut: {
        // the tissue dies from the top: plasmolysis, the stain drained
        overT_ = 0.f;
        dying_.clear();
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const CellLook l = has_[c.y][c.x] ? look_[c.y][c.x] : lookOf(c.x, c.y);
            dying_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), l, 0.25f + 0.07f * (c.y - top) + 0.1f * hashf((uint64_t)(c.x * 31 + c.y))});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The dish's life
// =====================================================================================================

// Distance from p to the places the life keeps away from (the well, the HUD's columns and corners, the field's
// edge): positive outside them, negative inside; its gradient (pointing out).
float MicroscopeUniverse::zoneDist(const vec2& p, vec2* grad) const {
    auto f = [&](const vec2& q) {
        float d = sdBox(q, vec2{0.f, 0.4f}, vec2{5.9f, 13.f});
        d = std::min(d, sdBox(q, vec2{-8.6f, 1.8f}, vec2{3.1f, 9.1f}));
        d = std::min(d, sdBox(q, vec2{8.6f, 2.5f}, vec2{3.1f, 8.4f}));
        const float hx = viewH_.x + 2.f;
        d = std::min(d, sdBox(q, vec2{-hx + (hx - 11.3f) * 0.5f, -8.8f}, vec2{(hx - 11.3f) * 0.5f, 1.9f}));
        d = std::min(d, sdBox(q, vec2{-hx + (hx - 11.3f) * 0.5f, 11.f}, vec2{(hx - 11.3f) * 0.5f, 1.9f}));
        // the field's edge and the screen's
        const float dy = q.y - viewC_.y;
        d = std::min(d, diskR_ - 0.6f - std::sqrt(q.x * q.x + dy * dy));
        d = std::min(d, viewH_.y - 0.6f - std::fabs(dy));
        return d;
    };
    const float d = f(p);
    if (grad) {
        const float e = 0.15f;
        vec2 g{f(vec2{p.x + e, p.y}) - f(vec2{p.x - e, p.y}), f(vec2{p.x, p.y + e}) - f(vec2{p.x, p.y - e})};
        const float l = std::sqrt(g.x * g.x + g.y * g.y);
        *grad = l > 1e-5f ? vec2{g.x / l, g.y / l} : vec2{p.x < 0 ? -1.f : 1.f, 0.f};
    }
    return d;
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void MicroscopeUniverse::addCell(float x, float y, float half, int type, const CellLook& l, int kind, float prog, float alpha,
                                 float flash) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)kind}, {(float)type, l.seed, prog, alpha}, {l.nx, l.ny, flash, l.life >= 0 ? 1.f : 0.f}, {x, y, 0.f, 0.f}});
}

void MicroscopeUniverse::buildBoard(const Game& g) {
    // the ghost: the wall alone, where the piece will land
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            CellLook l;
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                addCell(w.x, w.y, 0.5f, p.type, l, 1, 0.f, 1.f, 0.f);
            }
        }
    }
    // the stack (gone into the dying cells at a game over)
    if (overT_ < 0.f)
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Cell& c = g.cell(x, y);
                if (c.type < 0) continue;
                const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
                addCell(p.x, p.y, 0.5f, c.type, look_[y][x], 0, 0.f, 1.f, std::pow(saturate(c.flash), 14.f));
            }
    // the falling piece: exactly a locked one at its place (its cells keep their looks when it locks)
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            if (cells[i][1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
            const float a = saturate((10.6f - w.y) / 0.6f);
            addCell(w.x, w.y, 0.5f, p.type, fallLook_[i], 0, 0.f, a, 0.f);
        }
    }
    // the previews
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
        for (int i = 0; i < 4; i++) {
            const float ox = cells[i][0] - (minx + maxx) * 0.5f, oy = cells[i][1] - (miny + maxy) * 0.5f;
            CellLook l;
            const float an = (float)(i * 2.1 + type);
            l.nx = std::cos(an) * 0.07f;
            l.ny = std::sin(an) * 0.07f;
            l.seed = hashf((uint64_t)(type * 7 + i));
            addCell(center.x + ox * sc, center.y - oy * sc, 0.5f * sc, type, l, 0, 0.f, alpha, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.45f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void MicroscopeUniverse::drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count) {
    if (!count) return;
    glUseProgram(prog);
    setMat(prog, "uVP", vp_);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
}

void MicroscopeUniverse::setStain(GLuint prog) {
    auto arr = [&](const char* name, const vec3* v, int n) {
        float f[21];
        for (int i = 0; i < n; i++) { f[i * 3] = v[i].x; f[i * 3 + 1] = v[i].y; f[i * 3 + 2] = v[i].z; }
        glUniform3fv(ugl::U(prog, name), n, f);
    };
    arr("uFieldA", base_.field, 2);
    arr("uFieldB", dropS_.field, 2);
    arr("uPieceA", base_.piece, 7);
    arr("uPieceB", dropS_.piece, 7);
    arr("uOrgA", base_.org, 3);
    arr("uOrgB", dropS_.org, 3);
    set1f(prog, "uStain", stain_);
    set1f(prog, "uStainSeed", seedF_);
    set1f(prog, "uDark", lerpf(dark_, 1.f, slow_));
    set1f(prog, "uSlow", slow_);
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float MicroscopeUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) w += f.g[glyphIndex((unsigned char)s[i])].adv * k + track;
    return w - track;
}

void MicroscopeUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                                   std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = size / f.px;
    const float baseline = y + f.capH * k;
    float pen = x;
    for (size_t i = 0; i < s.size(); i++) {
        const Glyph& g = f.g[glyphIndex((unsigned char)s[i])];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) out.insert(out.end(), q, q + 4);
        }
        pen += g.adv * k + track;
    }
}

void MicroscopeUniverse::drawText(const Font& f, const std::vector<float>& q, bool accent, float alpha) {
    if (q.empty() || !f.ok || alpha <= 0.003f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const vec2 c = project(vec3(0.f, viewC_.y, 0.f));
    glUniform3f(ugl::U(progText_, "uDiskPx"), c.x, c.y, diskR_ * height_ / (2.f * viewH_.y));
    set3f(progText_, "uInkIn", accent ? accentIn_ : inkIn_);
    set3f(progText_, "uInkOut", accent ? accentOut_ : inkOut_);
    set3f(progText_, "uHaloIn", haloIn_);
    set3f(progText_, "uHaloOut", haloOut_);
    set1f(progText_, "uHaloA", 0.35f);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void MicroscopeUniverse::flushShapes(int mode) {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    set1i(progShape_, "uMode", mode);
    const vec2 c = project(vec3(0.f, viewC_.y, 0.f));
    glUniform3f(ugl::U(progShape_, "uDiskPx"), c.x, c.y, diskR_ * height_ / (2.f * viewH_.y));
    set3f(progShape_, "uInkIn", mode == 2 ? accentIn_ : inkIn_);
    set3f(progShape_, "uInkOut", mode == 2 ? accentOut_ : inkOut_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void MicroscopeUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    // the ink: dark on a pale field, pale on dark-field and on the black around the field
    const float dk = smoothstepf(0.35f, 0.65f, lerpf(dark_, 1.f, slow_));
    const vec3 pale = hex(0xc6ccd4), darkInk = hex(0x262a33);
    inkIn_ = lerp(darkInk, pale, dk);
    inkOut_ = pale;
    const vec3 acc = hex(VARIANTS[variant_].accent);
    accentIn_ = lerp(acc, lerp(acc, vec3(1.f), 0.15f), dk);
    accentOut_ = lerp(acc, vec3(1.f), dk > 0.5f ? 0.15f : 0.45f);
    haloIn_ = lerp(ugl::toSrgb(base_.field[0]), vec3(0.f), dk);
    haloOut_ = vec3(0.f);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? acc : lerp(ugl::toSrgb(base_.field[0]), vec3(0.02f), dk);
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes(0);
    }
    // the bonus gauge: a fine ring like a field stop, its charge in the accent
    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 120;
        const float th = std::max(1.f, 1.3f * s), th2 = std::max(2.f, 3.5f * s);
        auto seg = [&](float a0, float a1, float w, float al) {
            const float s0 = std::sin(a0), c0 = std::cos(a0), s1 = std::sin(a1), c1 = std::cos(a1);
            const float ri = r - w * 0.5f, ro = r + w * 0.5f;
            const float pts[4][2] = {{c.x + s0 * ri, c.y - c0 * ri}, {c.x + s0 * ro, c.y - c0 * ro},
                                     {c.x + s1 * ro, c.y - c1 * ro}, {c.x + s1 * ri, c.y - c1 * ri}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], 0.f, 0.f, 0.f, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        };
        const float dimk = vis * (1.f - 0.5f * dimT_);
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, th, (g.bonusReady() ? 0.55f : 0.25f) * dimk);
        // a scale of ticks inside the ring, every 30 degrees
        for (int i = 0; i < 12; i++) {
            const float a = TAU * i / 12.f;
            const float sn = std::sin(a), cs = std::cos(a), w = th * 0.5f;
            const float r0 = r * 0.8f, r1 = r * 0.9f;
            const float pts[4][2] = {{c.x + sn * r0 - cs * w, c.y - cs * r0 - sn * w}, {c.x + sn * r1 - cs * w, c.y - cs * r1 - sn * w},
                                     {c.x + sn * r1 + cs * w, c.y - cs * r1 + sn * w}, {c.x + sn * r0 + cs * w, c.y - cs * r0 + sn * w}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], 0.f, 0.f, 0.f, 0.25f * dimk};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        }
        flushShapes(1);
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, th2, 0.95f * dimk);
        }
        flushShapes(2);
    }
    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha) * vis;
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        auto upper = [](std::string t) {
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(fontSans_, upper(h.text), base * 0.66f, base * 0.3f, base * 0.08f);
            drawText(fontSans_, q, false, saturate(0.3f + h.alpha * 1.5f) * 0.75f * vis);
            continue;
        case HUD_SECTION:
            place(fontSans_, upper(h.text), base * 0.72f, base * 0.34f);
            drawText(fontSans_, q, true, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(fontIt_, h.text, base * 1.12f, base * 0.01f, -base * 0.12f);
            drawText(fontIt_, q, false, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(fontSans_, upper(h.text), base * 0.6f, base * 0.24f);
            drawText(fontSans_, q, false, saturate(alpha * 1.2f) * 0.72f);
            continue;
        case HUD_LABEL:
            place(fontSans_, upper(h.text), base * 0.62f, base * 0.4f);
            drawText(fontSans_, q, false, saturate(alpha * 1.2f) * 0.62f);
            continue;
        case HUD_VALUE:
            place(fontSans_, h.text, base * 1.0f, base * 0.06f, -base * 0.04f);
            drawText(fontSans_, q, false, alpha * 0.95f);
            continue;
        case HUD_NOTICE:
            place(fontIt_, h.text, base * 1.1f, base * 0.02f);
            drawText(fontIt_, q, true, alpha);
            continue;
        case HUD_KEY:
            place(fontSans_, h.text, base * 0.85f, base * 0.1f);
            drawText(fontSans_, q, false, alpha);
            continue;
        case HUD_TOAST:
            place(fontSans_, upper(h.text), base * 0.95f, base * 0.42f, -base * 0.1f);
            drawText(fontSans_, q, false, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontIt_, h.text, base * 1.0f, base * 0.02f);
            drawText(fontIt_, q, true, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontSans_, upper(h.text), base * 0.85f, base * 0.3f);
                drawText(fontSans_, q, false, alpha);
            } else {
                place(fontIt_, h.text, base * 0.95f, base * 0.02f);
                drawText(fontIt_, q, h.accent, alpha);
            }
            continue;
        default:
            place(fontSans_, h.text, base * 0.75f, base * 0.06f);
            drawText(fontSans_, q, h.accent, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MicroscopeUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    if (overT_ < 0.f && !bare_) syncLooks(game); // the world alone: the cells' looks kept for the whole frame
    // a new variant eases in
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        auto ease = [&](Stain& s, const Stain& t) {
            for (int i = 0; i < 2; i++) s.field[i] = lerp(s.field[i], t.field[i], k);
            for (int i = 0; i < 7; i++) s.piece[i] = lerp(s.piece[i], t.piece[i], k);
            for (int i = 0; i < 3; i++) s.org[i] = lerp(s.org[i], t.org[i], k);
        };
        ease(base_, baseT_);
        ease(dropS_, dropT_);
        dark_ = lerpf(dark_, darkT_, k);
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) { overT_ = -1.f; dying_.clear(); }
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    time_ += dt;
    worldTime_ += wdt;
    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const double beat = music.hasSong ? music.beat : time_ * 2.0;
    // the cilia beat on the beat grid, every two beats (not in calm parts)
    const float half = (float)(beat * 0.5 - std::floor(beat * 0.5));
    const float pulse = std::exp(-half * 5.f) * (0.35f * wm + 0.65f * wp) * (1.f - 0.7f * slow_);
    kickEnv_ = std::max(kickEnv_ * std::exp(-dt * 5.f), std::max(kickIn_, music.kick) * wp);
    kickIn_ = 0.f;
    stain_ = approach(stain_, dropMode_ ? 1.f : 0.f, dropMode_ ? 0.42f : 0.12f, dt);
    swell_ = approach(swell_, 0.f, 0.22f, dt);
    focus_ += (focusT_ - focus_) * (1.f - std::exp(-dt * 1.3f));
    // a bar at a peak: a few swimmers turn, a diatom reverses
    const double bar = std::floor(beat / 4.0);
    const bool newBar = bar != lastBar_;
    lastBar_ = bar;
    if (newBar && wp > 0.5f && !orgs_.empty()) {
        for (int n = 0; n < 2; n++) {
            Org& o = orgs_[rng_.irange(0, (int)orgs_.size() - 1)];
            if (o.kind == K_PENNATE) o.dir = -o.dir;
            else o.ang += rng_.range(-0.7f, 0.7f);
        }
    }

    // ---- The life (world time: slow motion stretches it).
    for (auto& pr : pending_) pr.first -= dt;
    for (size_t i = 0; i < pending_.size();) {
        if (pending_[i].first <= 0.f) {
            orgs_.push_back(pending_[i].second);
            pending_.erase(pending_.begin() + (long)i);
            // beyond the cap, the oldest fade away
            int alive = 0;
            for (const Org& o : orgs_) alive += o.fade < 0.f;
            for (Org& o : orgs_) {
                if (alive <= MAX_ORGS) break;
                if (o.fade < 0.f) { o.fade = 0.f; alive--; }
            }
        } else i++;
    }
    const float act = (0.45f + 0.35f * wm + 0.4f * wp);
    for (Org& o : orgs_) {
        o.age += wdt;
        if (o.fade >= 0.f) o.fade += dt / ORG_FADE;
        o.burst = std::max(0.f, o.burst - wdt * 0.9f);
        // wander around its home, apart from the others, steered clear of the well and the HUD
        vec2 g;
        const float zd = zoneDist(o.p, &g);
        const float margin = 1.2f + o.size * 0.45f;
        const float turn = KIND_TURN[o.kind] * (std::sin(o.turnPhase + worldTime_ * 0.23f) * 0.7f + std::sin(o.turnPhase * 1.7f + worldTime_ * 0.11f) * 0.5f);
        o.ang += turn * wdt * act;
        if (rng_.chance(wdt / 45.f)) o.home = pickHome(o.home.x < 0.f ? -1 : 1);   // now and then it moves on
        const vec2 th{o.home.x - o.p.x, o.home.y - o.p.y};
        const float hd2 = std::sqrt(th.x * th.x + th.y * th.y);
        const vec2 hn = hd2 > 1e-4f ? vec2{th.x / hd2, th.y / hd2} : vec2{0.f, 0.f};
        vec2 sep{0.f, 0.f};
        for (const Org& b : orgs_) {
            if (&b == &o) continue;
            const float dx = o.p.x - b.p.x, dy = o.p.y - b.p.y, R = (o.size + b.size) * 0.62f;
            const float dd = std::sqrt(dx * dx + dy * dy);
            if (dd < R && dd > 1e-4f) { sep.x += dx / dd * (1.f - dd / R); sep.y += dy / dd * (1.f - dd / R); }
        }
        const float wz = saturate((margin - zd) / margin), wh = smoothstepf(1.5f, 6.f, hd2);
        const vec2 want{hn.x * wh * 0.8f + sep.x * 1.2f + g.x * wz * 2.5f, hn.y * wh * 0.8f + sep.y * 1.2f + g.y * wz * 2.5f};
        const float wl = std::sqrt(want.x * want.x + want.y * want.y);
        if (wl > 0.05f) {
            const float target = std::atan2(want.y, want.x);
            const float hd = o.kind == K_PENNATE && o.dir < 0.f ? o.ang + PI : o.ang;
            const float diff = wrapAngle(target - hd);
            if (o.kind == K_PENNATE && std::fabs(diff) > 0.7f * PI && wz > 0.3f) o.dir = -o.dir;   // a diatom just backs up
            else o.ang += (diff > 0 ? 1.f : -1.f) * std::min(std::fabs(diff), (0.15f + 0.4f * std::min(wl, 1.f) + 1.4f * wz) * wdt);
        }
        float sp = o.speed * act * (1.f + 0.6f * pulse * (o.kind == K_PARA)) + 2.2f * o.burst * o.burst;
        if (o.kind == K_PARA) sp *= 1.f + 0.8f * kickEnv_;
        const float dirSign = o.kind == K_PENNATE ? o.dir : 1.f;
        o.p.x += std::cos(o.ang) * sp * dirSign * wdt;
        o.p.y += std::sin(o.ang) * sp * dirSign * wdt;
        // carried out to its home by the medium at first, nudged apart from the others
        const float carry = 2.2f * (1.f - smoothstepf(1.f, 6.f, o.age)) * smoothstepf(0.3f, 2.f, hd2);
        o.p.x += (hn.x * carry + sep.x * 0.3f) * wdt;
        o.p.y += (hn.y * carry + sep.y * 0.3f) * wdt;
        // inside a forbidden place (past its birth): pushed out
        if (zd < 0.f && o.age > 1.5f) {
            // (out toward the sides: the forbidden places are the middle columns and the corners' text)
            const float out = std::fabs(o.p.x) < 12.f ? (o.p.x < 0.f ? -0.8f : 0.8f) : 0.f;
            o.p.x += (g.x * 0.9f + out) * wdt;
            o.p.y += g.y * 0.9f * wdt;
        }
        o.cilia = 0.15f + 0.3f * wm + 0.9f * pulse + 0.6f * kickEnv_;
        o.spin += wdt * (o.kind == K_CENTRIC ? 0.08f : 0.f);
    }
    orgs_.erase(std::remove_if(orgs_.begin(), orgs_.end(), [](const Org& o) { return o.fade >= 1.f; }), orgs_.end());
    for (Lyse& l : lyses_) l.t += dt;
    lyses_.erase(std::remove_if(lyses_.begin(), lyses_.end(), [](const Lyse& l) { return l.t > LYSE_T; }), lyses_.end());
    for (Speck& k : specks_) {
        k.t += dt;
        k.p.x += k.v.x * dt;
        k.p.y += k.v.y * dt;
        k.v.x *= std::exp(-dt * 3.f);
        k.v.y *= std::exp(-dt * 3.f);
    }
    specks_.erase(std::remove_if(specks_.begin(), specks_.end(), [](const Speck& k) { return k.t > k.life; }), specks_.end());
    if (!bare_) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Instances.
    const float bridgeK = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    cells_.clear();
    orgInst_.clear();
    const float spread = lerpf(1.1f, 0.55f, wp) * (1.f + 0.3f * slow_);
    const float calmBlur = 0.25f * (1.f - wm);
    std::vector<Inst> dwellInst;   // drawn over the tissue: the cells' organisms, and those just out of them
    for (const Org& o : orgs_) {
        vec2 g;
        const float zd = zoneDist(o.p, &g);
        const float young = 1.f - smoothstepf(1.5f, 3.f, o.age);
        // (one out of a cleared cell is in plain view as it leaves the well; one from elsewhere fades in)
        const float minA = o.fromCell ? lerpf(0.05f, 1.f, 1.f - smoothstepf(2.5f, 4.f, o.age)) : lerpf(0.05f, 0.4f, young);
        float a = lerpf(minA, 1.f, smoothstepf(-0.2f, 1.0f, zd));
        if (!o.fromCell) a *= smoothstepf(0.f, 0.6f, o.age);
        if (o.fade >= 0.f) a *= 1.f - smoothstepf(0.f, 1.f, o.fade);
        a *= 1.f - bridgeK;
        if (a <= 0.003f) continue;
        const float grow = lerpf(o.grow0, 1.f, smoothstepf(0.f, o.fromCell ? 2.2f : 1.8f, o.age));
        const float focusIn = o.fromCell ? smoothstepf(0.5f, 3.f, o.age) : 1.f;   // it drifts to its depth
        float blur = saturate(std::fabs(o.depth * spread - focus_) * 0.55f - 0.12f + calmBlur) * focusIn +
                     (o.fromCell ? 0.f : 0.5f * young) + bridgeK;
        blur = saturate(blur);
        const float ang = o.kind == K_CENTRIC || o.kind == K_VOLVOX ? 0.f : o.ang;
        const float anim = worldTime_ * (o.kind == K_AMOEBA ? 1.f : 0.8f + 0.5f * act) + o.seed * 100.f;
        (o.fromCell && o.age < 1.5f ? dwellInst : orgInst_)
            .push_back({{o.p.x, o.p.y, o.size * grow, ang}, {(float)o.kind, o.seed, anim, a},
                        {blur, o.cilia, o.tint, (float)o.type}, {o.spin, o.fromCell ? 1.f - smoothstepf(0.8f, 2.6f, o.age) : 0.f, 0.f, 0.f}});
    }
    if (!bare_) { // the world alone: no tissue, nothing bursting or dying
        for (const Lyse& l : lyses_) addCell(l.x, l.y, 0.5f, l.type, l.l, 2, saturate(l.t / LYSE_T), 1.f, 0.f);
        if (overT_ >= 0.f)
            for (const Dying& d : dying_) {
                const float t = saturate((overT_ - d.delay) / DIE_T);
                if (t < 1.f) addCell(d.x, d.y, 0.5f, d.type, d.l, 3, t, 1.f, 0.f);
            }
        buildBoard(game);
    }
    // the organisms living inside cells of the stack and the falling piece
    if (!bare_) {
        auto dwell = [&](const CellLook& l, float x, float y, int type, float alpha) {
            if (l.life < 0 || alpha <= 0.003f) return;
            const Org o = dweller(l, x, y, type);
            const float ang = o.kind == K_CENTRIC || o.kind == K_VOLVOX ? 0.f : o.ang;
            const float anim = worldTime_ * (o.kind == K_AMOEBA ? 1.f : 0.8f + 0.5f * act) + o.seed * 100.f;
            dwellInst.push_back({{o.p.x, o.p.y, o.size, ang}, {(float)o.kind, o.seed, anim, alpha * (1.f - bridgeK)},
                                 {0.f, 0.15f + 0.3f * wm + 0.6f * kickEnv_, o.tint, (float)o.type}, {o.spin, 1.f, 0.f, 0.f}});
        };
        if (overT_ < 0.f)
            for (int y = Game::HIDDEN; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) {
                    const Cell& c = game.cell(x, y);
                    if (c.type < 0 || !has_[y][x]) continue;
                    const vec3 p = layout::cellPos((float)x, y - game.rowOffset(y));
                    dwell(look_[y][x], p.x, p.y, c.type, 1.f);
                }
        if (game.hasPiece() && !game.over()) {
            const Piece& p = game.piece();
            int pc[4][2];
            game.pieceCells(p, pc);
            const float dx = game.pieceVisualX() - p.x, dy = game.pieceVisualY() - p.y;
            for (int i = 0; i < 4; i++) {
                if (pc[i][1] < Game::HIDDEN - 1) continue;
                const vec3 w = layout::cellPos(pc[i][0] + dx, pc[i][1] + dy);
                dwell(fallLook_[i], w.x, w.y, p.type, saturate((10.6f - w.y) / 0.6f));
            }
        }
    }
    std::vector<Inst> speckInst;
    for (const Speck& k : specks_) {
        if (bare_) break; // (pushed out from under a dropped piece)
        const float s = k.t / k.life;
        const float a = smoothstepf(0.f, 0.1f, s) * (1.f - smoothstepf(0.4f, 1.f, s)) * 0.5f;
        speckInst.push_back({{k.p.x, k.p.y, k.r, 0.f}, {0.f, 0.f, 0.f, a}, {0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}});
    }

    // ---- Draw.
    const float dk = lerpf(dark_, 1.f, slow_);
    const float aaW = 2.f * viewH_.y / (float)height_;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progField_);
    setStain(progField_);
    set4f(progField_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progField_, "uDisk", viewC_.y, diskR_);
    set1f(progField_, "uTime", worldTime_);
    set1f(progField_, "uIllum", 0.93f + 0.045f * wm + 0.03f * wp);
    set1f(progField_, "uSwell", swell_ * (1.f - 0.6f * slow_));
    set1f(progField_, "uSeed", seedF_);
    set1f(progField_, "uAA", aaW);
    set1f(progField_, "uReticle", bare_ ? 0.f : 1.f - bridgeK);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // the scene's alpha is the stack's (for the bridge)
    if (!orgInst_.empty()) {
        glUseProgram(progOrg_);
        setStain(progOrg_);
        set1f(progOrg_, "uGlow", (0.85f + 0.12f * wm + 0.15f * wp + 0.2f * swell_) * (1.f - 0.2f * slow_));
        drawInst(progOrg_, orgVao_, orgVbo_, orgInst_.data(), orgInst_.size() * sizeof(Inst), orgInst_.size());
    }
    if (!speckInst.empty()) {
        glUseProgram(progSpeck_);
        setStain(progSpeck_);
        drawInst(progSpeck_, speckVao_, speckVbo_, speckInst.data(), speckInst.size() * sizeof(Inst), speckInst.size());
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(progPiece_);
    setStain(progPiece_);
    set1f(progPiece_, "uTime", worldTime_);
    set1f(progPiece_, "uLife", (0.6f + 0.4f * wm) * (1.f - 0.5f * slow_));
    set1f(progPiece_, "uGlow", (0.95f + 0.05f * wm + 0.08f * wp + 0.1f * swell_) * (1.f - 0.1f * slow_));
    drawInst(progPiece_, pieceVao_, pieceVbo_, cells_.data(), cells_.size() * sizeof(Inst), cells_.size());
    if (!dwellInst.empty()) {
        glUseProgram(progOrg_);
        setStain(progOrg_);
        set1f(progOrg_, "uGlow", (0.85f + 0.12f * wm + 0.15f * wp + 0.2f * swell_) * (1.f - 0.2f * slow_));
        drawInst(progOrg_, orgVao_, orgVbo_, dwellInst.data(), dwellInst.size() * sizeof(Inst), dwellInst.size());
    }

    // ---- The glow (dark-field).
    glDisable(GL_BLEND);
    const float bloomK = dk * (0.22f + 0.1f * wp + 0.2f * swell_);
    if (bloomK > 0.01f) {
        bloomA_.bind();
        glUseProgram(progDown_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
        set1i(progDown_, "uSrc", 0);
        set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
        ugl::drawFullscreen();
        glUseProgram(progBlur_);
        set1i(progBlur_, "uSrc", 0);
        for (int i = 0; i < 2; i++) {
            const float st = 1.f + i;
            bloomB_.bind();
            glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
            set2f(progBlur_, "uDir", st / bloomA_.w, 0.f);
            ugl::drawFullscreen();
            bloomA_.bind();
            glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
            set2f(progBlur_, "uDir", 0.f, st / bloomA_.h);
            ugl::drawFullscreen();
        }
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
    set4f(progFinal_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progFinal_, "uDisk", viewC_.y, diskR_);
    set1f(progFinal_, "uBloomK", bloomK);
    set1f(progFinal_, "uDarkF", dk);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", bridgeK);
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

const UniverseEntry& universe_microscope() {
    static const UniverseEntry e{
        "MICROSCOPE", "a Petri dish through the eyepiece: stained plant cells; each cleared row releases life that swims for the rest of the song",
        [] { return std::unique_ptr<Universe>(new MicroscopeUniverse()); },
        // A quiet, precise world: fits most songs, a little more the calm and airy ones.
        [](const SongLook& l) { return 0.8f + 0.15f * l.airWeight + 0.15f * (1.f - l.density); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
