// MICROSCOPE: a picture-book Petri dish seen through a toy microscope, filled with cute creatures by the cleared rows.
//
// One world, drawn like a children's book or a toon cartoon: the round dish of the eyepiece in a thick, glossy toy
// rim, on the variant's table color; inside, saturated water, a little deeper toward the rim, soft patches of light
// drifting in it and a few big bubbles rising slowly at the sides. The well is a calm rounded pane of a quieter tone
// (deeper on the dark waters, paler on the light ones), its edge one soft line, so the stack always reads.
//
// The pieces are picture-book living cells: one soft rounded square per block, one clean color per type, a pale
// translucent membrane, a bright cytoplasm and one big soft nucleus, a bold outline in a deep shade of its own color. The falling piece is exactly a
// locked one, the ghost a faint outline, the previews the same cells.
//
// The dish starts empty at the start of a song and fills with life as the rows clear: each cleared row pops its cells
// (they puff into balls and vanish, in 0.3 s) and frees a creature, which swims out of the well behind the stack,
// growing to its size on the way, and lives at a home at the side of the dish for the rest of the song: beans
// (paramecia, a rippling fringe of cilia), blobs (amoebas reaching out a lobe), tadpoles (flagellates lashing their
// tails), daisies (centric diatoms, seven petals turning slowly), rotifers (two spinning wheels on their heads) and
// volvox balls (babies rolling inside; a Tetris frees a big one). Every creature is toon shaded with big shiny eyes,
// rosy cheeks and a small smile, its face always upright, looking where it swims, blinking now and then; they swim
// with a stroke (squash and stretch), never rigidly. The dish fills at the song's pace (paceLines: full at scene
// completion, ~16 creatures, a third of them further back in the water); a row cleared while the dish is as full as the
// pace allows frees bubbles instead. They keep clear of the well, the HUD's columns and the corners' lettering.
//
// The song: calm is a still dish, slow swimmers; mid wakes them up, a few creatures hopping (a squash and a bounce)
// on the beat grid every two beats, in turn; a peak livens them up, the kicks make the swimmers dart and each bar
// turns a few. The drop pours the variant's second palette into the water and the creatures (soaking in unevenly),
// with one soft swell of light, and every creature closes its eyes happily for a moment. Slow motion is the dish by
// moonlight, the creatures sleepy. Game over deflates the stack from the top, the cells sagging and graying out. Exit
// and entrance: the dish fades to the bridge.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/microscope/microscope.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/microscope/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// M PLUS Rounded 1c Bold and Medium, SIL Open Font License, see OFL-MPLUSRounded1c.txt.
const unsigned char FONT_BOLD[] = {
#include "universes/microscope/MPLUSRounded1c-Bold.ttf.inc"
};
const unsigned char FONT_MEDIUM[] = {
#include "universes/microscope/MPLUSRounded1c-Medium.ttf.inc"
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

// A variant (sRGB): the water (center, edge, the well's pane) and the drop's water (center, edge), the table around
// the dish and its rim, the seven piece types (I O T S Z J L), five creature colors and the drop's, the HUD's inks
// (in the dish, on the table) and accent.
struct VariantDef {
    const char* name;
    bool light;
    unsigned water[3], dropWater[2];
    unsigned table, rim;
    unsigned piece[7];
    unsigned life[5], dropLife[5];
    unsigned inkIn, inkOut, accent;
};
const VariantDef VARIANTS[] = {
    // a turquoise lagoon, a sunny yellow rim; candy pieces on a deep teal pane; the drop: a green lagoon
    {"LAGOON", false, {0x1499bd, 0x0b6f9a, 0x08405f}, {0x15a88a, 0x0b7a6a}, 0x0b2f4a, 0xffd23f,
     {0x5ee4ff, 0xffd93b, 0xc985ff, 0x8de85a, 0xff5a6e, 0x6a8cff, 0xff9a3c},
     {0xff8fc0, 0xc6f25a, 0xffd84a, 0xffa25a, 0xc0a8ff}, {0xff6fae, 0xfff27a, 0x7af5ff, 0xff8a5a, 0xe08bff},
     0xf2fbff, 0xf2fbff, 0xffd23f},
    // a bubblegum night: plum water, a pink rim; the drop: raspberry
    {"BUBBLEGUM", false, {0x7a38a8, 0x58237f, 0x2a0e48}, {0xb02f78, 0x821c58}, 0x1c0832, 0xff7ac0,
     {0x5ef2ff, 0xffe45c, 0xff8ad8, 0x8cf26b, 0xff5c5c, 0x8a9cff, 0xffa04d},
     {0xffe45c, 0x7af7d0, 0xff9ad1, 0xb6ff7a, 0x9fd4ff}, {0x7af7ff, 0xffb45c, 0xfff07a, 0x9fffb0, 0xffd0f0},
     0xfff0fa, 0xfff0fa, 0xff7ac0},
    // a starry blue: navy water, a sky-blue rim; neon candy pieces; the drop: violet
    {"MIDNIGHT", false, {0x2348a8, 0x173584, 0x0a1642}, {0x6430b0, 0x46208a}, 0x070b24, 0x6ad5ff,
     {0x4ff0ff, 0xffe14d, 0xff7ae0, 0x6cf58a, 0xff5d6a, 0x93a2ff, 0xffa24a},
     {0xffd84d, 0xff8fc8, 0x7af0c4, 0xa8f060, 0xffa86a}, {0x7af7ff, 0xff7ae0, 0xfff07a, 0x9cff8a, 0xffb05a},
     0xeef4ff, 0xeef4ff, 0x6ad5ff},
    // lemonade: pale yellow water, a tomato rim on a teal table; the drop: strawberry
    {"LEMONADE", true, {0xfff0a0, 0xffd45e, 0xfffbe4}, {0xffc0d4, 0xff95b8}, 0x1fa89c, 0xff6b4a,
     {0x1fb3e0, 0xf5b400, 0x9b4dde, 0x3dbb4a, 0xee3d55, 0x3360e0, 0xff7a1a},
     {0xff6fa5, 0x3cc7c0, 0x8f6bff, 0x7ed957, 0xff9445}, {0x3cb4ff, 0xff5a7a, 0x2ec4a0, 0xb06bff, 0xffb020},
     0x2a2440, 0xffffff, 0xe8452a},
    // mint: pale mint water, a yellow rim on a blue table; the drop: a sky blue
    {"MINT", true, {0xc0f5df, 0x7ee3c4, 0xefffff}, {0xc4e0ff, 0x94c4ff}, 0x2f5fd0, 0xffd23f,
     {0x19a7ff, 0xffc400, 0xa14ee8, 0x22b04a, 0xff4a5f, 0x2f54d9, 0xff8a1f},
     {0xff7eb3, 0xffb23d, 0x5b8cff, 0xb072ff, 0xff6b6b}, {0xff5fa0, 0x2ec4ff, 0xffd23f, 0x8f6bff, 0x3cd08a},
     0x15304a, 0xffffff, 0x2f6fe0},
    // berry: pink water, a golden rim on a purple table; the drop: lilac
    {"BERRY", true, {0xffd0e4, 0xffa6c9, 0xfff3f8}, {0xd8c8ff, 0xb6a0ff}, 0x6a2a8a, 0xffc93c,
     {0x14a8e0, 0xf2b000, 0x8a4fe0, 0x2fb45a, 0xe8304a, 0x2e5ae6, 0xff7a1a},
     {0x4fc3ff, 0x8a6bff, 0x3cc78f, 0xffb020, 0xff5a8a}, {0xff5a8a, 0x2ec4ff, 0xffd23f, 0x3cd08a, 0xb072ff},
     0x3a1838, 0xffffff, 0xe8457a},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float POP_T = 0.3f;     // a cleared cell is gone in this long (real time)
constexpr float DIE_T = 1.6f;     // game over: a cell's deflating
constexpr float FULL = 12.f;      // the dish's creatures at scene completion (at most: full_, the room there is)
constexpr float ORG_FADE = 1.8f;
constexpr float RIM_W = 0.6f;     // the dish's rim (board units)

// The kinds of life: bean (paramecium), blob (amoeba), tadpole (flagellate), daisy (centric diatom), rotifer, volvox.
enum { K_BEAN, K_BLOB, K_TADPOLE, K_DAISY, K_ROTIFER, K_VOLVOX, K_COUNT, K_BUBBLE = 6 };
const float KIND_SIZE[K_COUNT][2] = {{4.0f, 4.6f}, {3.8f, 4.3f}, {3.8f, 4.4f}, {3.6f, 4.1f}, {3.9f, 4.4f}, {3.8f, 4.3f}};
const float KIND_SPEED[K_COUNT] = {0.55f, 0.12f, 0.5f, 0.12f, 0.28f, 0.2f};
const float KIND_TURN[K_COUNT] = {0.5f, 0.25f, 0.55f, 0.15f, 0.35f, 0.25f};
const float KIND_STROKE[K_COUNT] = {1.6f, 0.5f, 2.2f, 0.4f, 1.2f, 0.6f};   // swim strokes a second
const float KIND_STRETCH[K_COUNT] = {0.07f, 0.04f, 0.06f, 0.02f, 0.06f, 0.025f};

// Test hooks: ZEN_MICROSCOPE_LINES=N adds N lines to the dish, ZEN_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("ZEN_MICROSCOPE_LINES") ? std::atoi(std::getenv("ZEN_MICROSCOPE_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

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
// A hop: a squash and a bounce back, eased (0 at its start and its end).
float hopCurve(float t) { return t >= 1.2f ? 0.f : -std::sin(t * TAU / 0.55f) * std::exp(-t * 3.5f) * smoothstepf(0.f, 0.06f, t); }

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
    const std::string inkText = withCommon(shaders::TEXT_FS, shaders::INK_GLSL);
    const std::string inkShape = withCommon(shaders::SHAPE_FS, shaders::INK_GLSL);
    progField_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::FIELD_FS).c_str(), "microscope water");
    progLife_ = ugl::program(shaders::LIFE_VS, withCommon(shaders::LIFE_FS).c_str(), "microscope life");
    progPiece_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::PIECE_FS).c_str(), "microscope cells");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "microscope final");
    progShape_ = ugl::program(shaders::SHAPE_VS, inkShape.c_str(), "microscope shape");
    progText_ = ugl::program(shaders::TEXT_VS, inkText.c_str(), "microscope text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    instVao(quadVbo_, pieceVao_, pieceVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, lifeVao_, lifeVbo_, 5, sizeof(LInst));
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

    buildFont(fontB_, FONT_BOLD);
    buildFont(fontM_, FONT_MEDIUM);
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
    // the dish: as wide as the screen allows, always around the HUD's columns
    diskR_ = std::max(hw * 0.95f, 15.6f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    // how many creatures the dish holds at its fullest: the room left for them around the well and the HUD
    int room = 0;
    for (float y = viewC_.y - hh; y < viewC_.y + hh; y += 0.5f)
        for (float x = -hw; x < hw; x += 0.5f) room += zoneDist(vec2{x, y}, nullptr) >= 2.f;
    full_ = std::clamp(std::round(room * 0.25f / 8.5f), 4.f, FULL);
}

MicroscopeUniverse::~MicroscopeUniverse() {
    sceneT_.destroy();
    for (GLuint p : {progField_, progLife_, progPiece_, progFinal_, progShape_, progText_}) glDeleteProgram(p);
    for (GLuint v : {pieceVao_, lifeVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, pieceVbo_, lifeVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontB_, &fontM_}) {
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
        // darker songs in the deep waters, brighter ones in the light ones
        const bool dark = !t.hasSong || t.brightness < 0.55f || (t.minor && t.brightness < 0.62f);
        const int pick = (int)(h * 3.f) % 3;
        if (dark) v = t.hasSong && t.dynamics > 0.62f ? 1 : t.hasSong && t.airWeight > 0.6f ? 2 : pick;
        else v = 3 + (t.minor ? (pick == 0 ? 1 : pick) : pick);
        v %= VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
    }
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    Palette& p = palT_;
    for (int i = 0; i < 3; i++) p.water[i] = lin(V.water[i]);
    for (int i = 0; i < 2; i++) p.dropWater[i] = lin(V.dropWater[i]);
    for (int i = 0; i < 7; i++) p.piece[i] = lin(V.piece[i]);
    for (int i = 0; i < 5; i++) { p.life[i] = lin(V.life[i]); p.dropLife[i] = lin(V.dropLife[i]); }
    p.table = lin(V.table);
    p.rim = lin(V.rim);
    p.rimInk = V.light ? lin(0x2a2440) * 0.8f : lin(0x0a0a1e);
    p.line = V.light ? lerp(p.water[2], lin(V.inkIn), 0.35f) : lerp(p.water[0], vec3(1.f), 0.25f);
    p.eyeInk = lin(0x1a1028);
    p.blush = lin(0xff6f8e);
    p.night = V.light ? vec3(0.26f, 0.36f, 0.95f) : vec3(0.3f, 0.52f, 1.8f);
    const bool instant = !colorsSet_ || seconds <= 0.1f;
    if (instant) {
        pal_ = palT_;
        colorsSet_ = true;
    }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    rng_ = Rng(seed_ ^ 0x3C1C0Bull);
    // (the dish keeps its life into the next song in this universe: it fills at the pace of the lines, paceLines)
    if (instant) { orgs_.clear(); bubbles_.clear(); }
    clears_.clear();
    stain_ = 0.f;
    dropMode_ = false;
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
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

// The drop: the variant's second palette soaks into the dish, one swell of light, every creature happy for a moment.
void MicroscopeUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    for (Org& o : orgs_) {
        o.happy = 2.2f + o.seed * 0.8f;
        if (o.hop > 1.2f) o.hop = -o.seed * 0.5f;
    }
}

void MicroscopeUniverse::refresh(float strength) {
    (void)strength;
    // a new section: a few creatures turn
    for (Org& o : orgs_)
        if (rng_.chance(0.4f)) o.ang += rng_.range(-0.8f, 0.8f);
}

vec2 MicroscopeUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints MicroscopeUniverse::hints() const {
    TransitionHints h;
    const bool light = VARIANTS[variant_].light;
    h.light = light;
    h.background = pal_.water[0];
    vec3 ink(0.f);
    for (int i = 0; i < 7; i++) ink = ink + pal_.piece[i] * (1.f / 7.f);
    h.ink = light ? ink * 0.7f : ink;
    h.accent = lin(VARIANTS[variant_].accent);
    h.styles = TS_RIPPLE | TS_INK;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void MicroscopeUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::HardDrop: {
        // a few bubbles pushed out from under the piece, at the contact (behind the stack)
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
        for (int i = 0; i < 4; i++) {
            const float s = i % 2 ? 1.f : -1.f;
            Bubble k;
            k.p = vec2{(s < 0 ? a.x - 0.45f : b.x + 0.45f) + rng_.range(-0.1f, 0.1f), a.y - 0.35f + rng_.range(0.f, 0.2f)};
            k.v = vec2{s * rng_.range(0.4f, 1.0f), rng_.range(0.3f, 0.7f)};
            k.t = 0.f;
            k.life = rng_.range(0.6f, 0.9f);
            k.r = rng_.range(0.13f, 0.22f);
            k.deco = false;
            bubbles_.push_back(k);
        }
        break;
    }
    case GameEvent::Clear: {
        // the cells pop; the life in the rows is freed on the next frame (at the song's pace, paceLines)
        ClearAt ca;
        std::vector<int> rows;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            pops_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), 0.f});
            ca.cells.push_back(vec2{p.x, p.y});
            if (std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        }
        ca.rows = std::max(1, std::max(ev.count, (int)rows.size()));
        if (!ca.cells.empty()) clears_.push_back(ca);
        if (ev.count >= 4) swell_ = std::max(swell_, 0.5f);
        break;
    }
    case GameEvent::TopOut: {
        // the stack deflates from the top
        overT_ = 0.f;
        dying_.clear();
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            dying_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), 0.2f + 0.06f * (c.y - top) + 0.08f * rng_.uniform()});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The dish's life
// =====================================================================================================

int MicroscopeUniverse::alive() const {
    int n = 0;
    for (const Org& o : orgs_) n += o.fade < 0.f;
    return n;
}

int MicroscopeUniverse::sideCount(int side) const {
    int n = 0;
    for (const Org& o : orgs_) n += o.fade < 0.f && (o.home.x < 0.f) == (side < 0);
    return n;
}

int MicroscopeUniverse::pickKind() {
    const float w[K_COUNT] = {0.22f, 0.17f, 0.19f, 0.15f, 0.15f, 0.12f};
    int k = rng_.weighted(w);
    // not the same kind as one of the last two
    for (int tries = 0; tries < 4; tries++) {
        bool seen = false;
        for (size_t i = orgs_.size() >= 2 ? orgs_.size() - 2 : 0; i < orgs_.size(); i++) seen |= orgs_[i].kind == k;
        if (!seen) break;
        k = rng_.weighted(w);
    }
    return k;
}

int MicroscopeUniverse::pickColor() {
    int c = rng_.irange(0, 4);
    for (int tries = 0; tries < 4; tries++) {
        bool seen = false;
        for (size_t i = orgs_.size() >= 2 ? orgs_.size() - 2 : 0; i < orgs_.size(); i++) seen |= orgs_[i].col == c;
        if (!seen) break;
        c = rng_.irange(0, 4);
    }
    return c;
}

// A place for a creature to live on one side of the dish: the wide band beyond the HUD's column, or the corner under
// it; the least crowded of a few tries.
vec2 MicroscopeUniverse::pickHome(int side, float size) {
    vec2 best{side * 15.f, 0.f};
    float bestScore = -1e9f;
    const float r = size * 0.4f;
    for (int tryN = 0; tryN < 10; tryN++) {
        const float y = rng_.range(-viewH_.y + 1.5f, viewH_.y - 1.5f) + viewC_.y;
        const float dy = y - viewC_.y;
        const float xmax = std::sqrt(std::max(0.f, (diskR_ - RIM_W - r - 0.6f) * (diskR_ - RIM_W - r - 0.6f) - dy * dy));
        const float x0 = 7.f;
        if (xmax < x0 + 0.5f) continue;
        const vec2 h{side * rng_.range(x0, xmax), y};
        const float zd = zoneDist(h, nullptr);
        if (zd < r + 0.4f) continue;
        float nearest = 99.f;
        for (const Org& o : orgs_) {
            if (o.fade >= 0.f) continue;
            const float dx = o.home.x - h.x, dy2 = o.home.y - h.y;
            nearest = std::min(nearest, std::sqrt(dx * dx + dy2 * dy2) - (o.size + size) * 0.45f);
        }
        if (nearest > bestScore) { bestScore = nearest; best = h; }
    }
    return best;
}

// A new creature: out of a cleared row (it swims out of the well behind the stack, growing on the way), or simply
// fading in at its home.
void MicroscopeUniverse::spawn(vec2 at, int side, bool fromCell, bool big) {
    Org o;
    o.kind = big ? K_VOLVOX : pickKind();
    o.col = pickColor();
    o.seed = rng_.uniform();
    o.depth = 0.f;
    o.size = big ? 5.8f : rng_.range(KIND_SIZE[o.kind][0], KIND_SIZE[o.kind][1]);
    o.speed = KIND_SPEED[o.kind] * rng_.range(0.8f, 1.2f);
    o.phase = rng_.range(0.f, TAU);
    o.spin = rng_.range(0.f, TAU);
    o.nextBlink = rng_.range(1.5f, 6.f);
    o.hop = 9.f;
    o.home = pickHome(side, o.size * (o.depth > 0.5f ? 0.72f : 1.f));
    o.fromCell = fromCell;
    if (fromCell) {
        o.p = at;
        o.grow0 = std::min(0.6f, 1.1f / o.size);
        o.burst = 1.f;
        o.ang = (side > 0 ? 0.f : PI) + rng_.range(-0.4f, 0.4f);
    } else {
        o.p = o.home;
        o.ang = rng_.range(0.f, TAU);
        o.age = 0.f;
        o.grow0 = 0.7f;
    }
    o.look = o.ang;
    orgs_.push_back(o);
}

// A cleared row frees its life: a creature (a Tetris: a big volvox among them), as many as the song's pace allows in
// the dish so far (full at scene completion); a row cleared while the dish is as full as it may be yet frees a few
// bubbles instead.
void MicroscopeUniverse::release(const ClearAt& c, float pace) {
    const int cap = (int)std::ceil(full_ * saturate(pace / SCENE_REF_LINES) - 0.01f) + 1;
    const int al = alive();
    const bool tetris = c.rows >= 4;
    int n = std::min(cap - al, tetris ? 3 : c.rows >= 3 ? 2 : 1);
    if (tetris) n = std::max(n, 1);
    for (int i = 0; i < n; i++) {
        const vec2 at = c.cells[(size_t)rng_.irange(0, (int)c.cells.size() - 1)];
        int side = std::fabs(at.x) < 0.3f ? (rng_.chance(0.5f) ? 1 : -1) : at.x < 0.f ? -1 : 1;
        if (sideCount(side) >= sideCount(-side) + 2) side = -side;   // (the two sides stay balanced)
        spawn(at, side, true, tetris && i == 0);
    }
    // beyond the full dish (a Tetris' big one), the oldest swims away, fading
    int over = alive() - (int)full_;
    for (Org& o : orgs_) {
        if (over <= 0) break;
        if (o.fade < 0.f && !o.fromCell) { o.fade = 0.f; over--; }
        else if (o.fade < 0.f && o.age > 5.f) { o.fade = 0.f; over--; }
    }
    if (n <= 0)
        for (int i = 0; i < 5; i++) {
            const vec2 at = c.cells[(size_t)rng_.irange(0, (int)c.cells.size() - 1)];
            Bubble k;
            k.p = vec2{at.x + rng_.range(-0.3f, 0.3f), at.y};
            k.v = vec2{(at.x < 0.f ? -1.f : 1.f) * rng_.range(0.6f, 1.6f), rng_.range(0.6f, 1.2f)};
            k.t = -0.05f * i;
            k.life = rng_.range(1.2f, 1.8f);
            k.r = rng_.range(0.2f, 0.35f);
            k.deco = false;
            bubbles_.push_back(k);
        }
    // the dish cheers: a few creatures hop
    for (Org& o : orgs_)
        if (o.fade < 0.f && !o.fromCell && o.hop > 1.2f && rng_.chance(tetris ? 1.f : 0.3f)) o.hop = -rng_.range(0.f, 0.3f);
}

// Distance from p to the places the life keeps away from (the well, the HUD's columns and corners, the dish's rim):
// positive outside them, negative inside; its gradient (pointing out).
float MicroscopeUniverse::zoneDist(const vec2& p, vec2* grad) const {
    auto f = [&](const vec2& q) {
        float d = sdBox(q, vec2{0.f, 0.4f}, vec2{5.9f, 13.f});
        d = std::min(d, sdBox(q, vec2{-8.6f, 1.8f}, vec2{3.1f, 9.1f}));
        d = std::min(d, sdBox(q, vec2{8.6f, 2.5f}, vec2{3.1f, 8.4f}));
        const float hx = viewH_.x + 2.f;
        d = std::min(d, sdBox(q, vec2{-hx + (hx - 11.3f) * 0.5f, -8.8f}, vec2{(hx - 11.3f) * 0.5f, 1.9f}));
        d = std::min(d, sdBox(q, vec2{-hx + (hx - 11.3f) * 0.5f, 11.f}, vec2{(hx - 11.3f) * 0.5f, 1.9f}));
        // the dish's rim and the screen's edge
        const float dy = q.y - viewC_.y;
        d = std::min(d, diskR_ - RIM_W - 0.6f - std::sqrt(q.x * q.x + dy * dy));
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

void MicroscopeUniverse::addCell(float x, float y, float half, int type, int kind, float prog, float alpha, float flash) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)kind}, {(float)type, 0.f, prog, alpha}, {flash, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}});
}

void MicroscopeUniverse::addLife(std::vector<LInst>& out, const Org& o, float alpha, float sizeK) {
    if (alpha <= 0.003f) return;
    // the swim stroke: squash and stretch along its heading, and a hop now and then
    const float st = KIND_STRETCH[o.kind] * std::sin(o.phase) + 0.16f * hopCurve(o.hop);
    const bool round = o.kind == K_DAISY || o.kind == K_VOLVOX;
    const float rot = round ? 0.f : o.ang;
    // the eyes: a blink now and then, sleepy in slow motion, happy on the drop
    const float blink = o.blink > 0.f ? std::sin(saturate(o.blink / 0.2f) * PI) : 0.f;
    const float lid = std::max(blink, 0.55f * slow_);
    const float happy = smoothstepf(0.f, 0.3f, o.happy) * (1.f - slow_);
    const vec3 c = pal_.life[o.col], cd = pal_.dropLife[o.col];
    out.push_back({{o.p.x, o.p.y, o.size * sizeK, rot}, {(float)o.kind, o.seed, worldTime_ + o.seed * 50.f, alpha},
                   {c.x, c.y, c.z, round ? std::fabs(st) * 0.6f : st}, {cd.x, cd.y, cd.z, lid}, {happy, o.depth, o.spin, o.look}});
}

void MicroscopeUniverse::buildBoard(const Game& g) {
    // the ghost: a faint outline where the piece will land
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                addCell(w.x, w.y, 0.5f, p.type, 1, 0.f, 1.f, 0.f);
            }
        }
    }
    // the stack (gone into the deflating cells at a game over)
    if (overT_ < 0.f)
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Cell& c = g.cell(x, y);
                if (c.type < 0) continue;
                const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
                addCell(p.x, p.y, 0.5f, c.type, 0, 0.f, 1.f, std::pow(saturate(c.flash), 8.f));
            }
    // the falling piece: exactly a locked one at its place
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            if (cells[i][1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
            addCell(w.x, w.y, 0.5f, p.type, 0, 0.f, saturate((10.6f - w.y) / 0.6f), 0.f);
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
            addCell(center.x + ox * sc, center.y - oy * sc, 0.5f * sc, type, 0, 0.f, alpha, 0.f);
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

void MicroscopeUniverse::setPalette(GLuint prog) {
    auto arr = [&](const char* name, const vec3* v, int n) {
        float f[21];
        for (int i = 0; i < n; i++) { f[i * 3] = v[i].x; f[i * 3 + 1] = v[i].y; f[i * 3 + 2] = v[i].z; }
        glUniform3fv(ugl::U(prog, name), n, f);
    };
    arr("uWaterA", pal_.water, 3);
    arr("uWaterB", pal_.dropWater, 2);
    arr("uPiece", pal_.piece, 7);
    set3f(prog, "uNight", pal_.night);
    set3f(prog, "uEyeInk", pal_.eyeInk);
    set3f(prog, "uBlush", pal_.blush);
    set1f(prog, "uStain", stain_);
    set1f(prog, "uStainSeed", seedF_);
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
    const VariantDef& V = VARIANTS[variant_];
    // the ink: the variant's own in the dish, another on the table; by moonlight (slow motion) a pale one
    const vec3 pale = hex(0xf2f4ff);
    inkIn_ = lerp(hex(V.inkIn), pale, slow_);
    inkOut_ = hex(V.inkOut);
    const vec3 acc = hex(V.accent);
    accentIn_ = lerp(acc, lerp(acc, pale, 0.4f), slow_);
    accentOut_ = lerp(acc, vec3(1.f), 0.2f);
    haloIn_ = lerp(ugl::toSrgb(pal_.water[0]), ugl::toSrgb(pal_.water[0] * pal_.night), slow_);
    haloOut_ = ugl::toSrgb(pal_.table);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? acc : ugl::toSrgb(pal_.water[2]);
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes(0);
    }
    // the bonus gauge: a chunky ring, its charge in the accent
    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 120;
        const float th = std::max(3.f, 7.f * s);
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
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, th, (g.bonusReady() ? 0.5f : 0.25f) * dimk);
        flushShapes(1);
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, th, 0.95f * dimk);
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
            place(fontM_, upper(h.text), base * 0.66f, base * 0.2f, base * 0.08f);
            drawText(fontM_, q, false, saturate(0.3f + h.alpha * 1.5f) * 0.8f * vis);
            continue;
        case HUD_SECTION:
            place(fontB_, upper(h.text), base * 0.74f, base * 0.24f);
            drawText(fontB_, q, true, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(fontB_, h.text, base * 1.05f, base * 0.02f, -base * 0.1f);
            drawText(fontB_, q, false, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(fontM_, upper(h.text), base * 0.6f, base * 0.18f);
            drawText(fontM_, q, false, saturate(alpha * 1.2f) * 0.78f);
            continue;
        case HUD_LABEL:
            place(fontB_, upper(h.text), base * 0.62f, base * 0.3f);
            drawText(fontB_, q, false, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_VALUE:
            place(fontB_, h.text, base * 1.0f, base * 0.04f, -base * 0.04f);
            drawText(fontB_, q, false, alpha);
            continue;
        case HUD_NOTICE:
            place(fontB_, h.text, base * 1.0f, base * 0.04f);
            drawText(fontB_, q, true, alpha);
            continue;
        case HUD_KEY:
            place(fontB_, h.text, base * 0.85f, base * 0.08f);
            drawText(fontB_, q, false, alpha);
            continue;
        case HUD_TOAST:
            place(fontB_, upper(h.text), base * 0.95f, base * 0.3f, -base * 0.1f);
            drawText(fontB_, q, false, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontM_, h.text, base * 0.95f, base * 0.04f);
            drawText(fontM_, q, true, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontB_, upper(h.text), base * 0.85f, base * 0.24f);
                drawText(fontB_, q, false, alpha);
            } else {
                place(fontM_, h.text, base * 0.9f, base * 0.03f);
                drawText(fontM_, q, h.accent, alpha);
            }
            continue;
        default:
            place(fontM_, h.text, base * 0.75f, base * 0.05f);
            drawText(fontM_, q, h.accent, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MicroscopeUniverse::render(const Frame& f, GLuint fbo) {
    // ZEN_GPUTIME: the GPU time of a frame, printed every 240 frames
    static const bool gpuTime = std::getenv("ZEN_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    // a new variant eases in
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        Palette& p = pal_;
        const Palette& t = palT_;
        for (int i = 0; i < 3; i++) p.water[i] = lerp(p.water[i], t.water[i], k);
        for (int i = 0; i < 2; i++) p.dropWater[i] = lerp(p.dropWater[i], t.dropWater[i], k);
        for (int i = 0; i < 7; i++) p.piece[i] = lerp(p.piece[i], t.piece[i], k);
        for (int i = 0; i < 5; i++) { p.life[i] = lerp(p.life[i], t.life[i], k); p.dropLife[i] = lerp(p.dropLife[i], t.dropLife[i], k); }
        p.table = lerp(p.table, t.table, k);
        p.rim = lerp(p.rim, t.rim, k);
        p.rimInk = lerp(p.rimInk, t.rimInk, k);
        p.line = lerp(p.line, t.line, k);
        p.night = lerp(p.night, t.night, k);
        p.eyeInk = t.eyeInk;
        p.blush = t.blush;
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) { overT_ = -1.f; dying_.clear(); }
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
    const double beat = music.hasSong ? music.beat : time_ * 2.0;
    kickEnv_ = std::max(kickEnv_ * std::exp(-dt * 5.f), std::max(kickIn_, music.kick) * wp);
    kickIn_ = 0.f;
    stain_ = approach(stain_, dropMode_ ? 1.f : 0.f, dropMode_ ? 0.42f : 0.12f, dt);
    swell_ = approach(swell_, 0.f, 0.22f, dt);
    // every two beats (mid and peak), a third of the creatures hop, in turn
    const double half = std::floor(beat * 0.5);
    if (half != lastHalf_) {
        if (lastHalf_ >= 0.0 && wm > 0.5f && slow_ < 0.5f && !f.paused) {
            beatCount_++;
            int i = 0;
            for (Org& o : orgs_)
                if (o.fade < 0.f && (i++ % 3) == beatCount_ % 3 && o.hop > 1.2f) o.hop = -0.1f * o.seed;
        }
        lastHalf_ = half;
    }
    // a bar at a peak: a few swimmers turn
    const double bar = std::floor(beat / 4.0);
    if (bar != lastBar_) {
        lastBar_ = bar;
        if (wp > 0.5f && !orgs_.empty())
            for (int n = 0; n < 2; n++) orgs_[(size_t)rng_.irange(0, (int)orgs_.size() - 1)].ang += rng_.range(-0.9f, 0.9f);
    }

    // ---- The game's lines -> the dish's life (on the 50-line scale: the song's pace, paceLines; a break: as it was).
    if (!bare_) {
        const float pl = paceLines(game, (float)extraLines(time_));
        if (pl + 0.5f < pace_) // a new game: the dish is rinsed
            for (Org& o : orgs_)
                if (o.fade < 0.f) o.fade = 0.f;
        pace_ = pl;
    }
    for (const ClearAt& c : clears_) release(c, pace_);
    clears_.clear();
    {
        // (a stage reached without clears, the test hook: the dish fills in, one creature at a time)
        fillT_ -= dt;
        const int want = (int)std::floor(full_ * saturate(pace_ / SCENE_REF_LINES) + 0.01f);
        if (fillT_ <= 0.f && alive() < want) {
            const int l = sideCount(-1), r = sideCount(1);
            spawn(vec2{0.f, 0.f}, l == r ? (rng_.chance(0.5f) ? 1 : -1) : l < r ? -1 : 1, false, false);
            fillT_ = 0.05f;
        }
    }
    // the grown-ups: the creatures grow up a little as the dish fills
    paceShown_ = approach(paceShown_, pace_, 1.2f, dt);
    const float growK = lerpf(0.78f, 1.f, smoothstepf(0.f, SCENE_REF_LINES, paceShown_));

    // ---- The life (world time: slow motion stretches it).
    const float act = (0.45f + 0.35f * wm + 0.4f * wp) * (1.f - 0.4f * slow_);
    for (Org& o : orgs_) {
        o.age += wdt;
        if (o.fade >= 0.f) o.fade += dt / ORG_FADE;
        o.burst = std::max(0.f, o.burst - wdt * 0.7f);
        o.hop += wdt;
        o.happy -= dt;
        // blinking now and then
        o.nextBlink -= wdt;
        if (o.blink > 0.f) o.blink += wdt;
        if (o.blink > 0.2f) o.blink = 0.f;
        if (o.nextBlink <= 0.f) { o.blink = 1e-4f; o.nextBlink = rng_.range(2.5f, 7.f); }
        // wander around its home, apart from the others, steered clear of the well and the HUD
        vec2 g;
        const float zd = zoneDist(o.p, &g);
        const float margin = 0.8f + o.size * 0.45f;
        const float turn = KIND_TURN[o.kind] * (std::sin(o.seed * 40.f + worldTime_ * 0.23f) * 0.7f + std::sin(o.seed * 71.f + worldTime_ * 0.11f) * 0.5f);
        o.ang += turn * wdt * act;
        if (rng_.chance(wdt / 50.f)) o.home = pickHome(o.home.x < 0.f ? -1 : 1, o.size);   // now and then it moves on
        const vec2 th{o.home.x - o.p.x, o.home.y - o.p.y};
        const float hd2 = std::sqrt(th.x * th.x + th.y * th.y);
        const vec2 hn = hd2 > 1e-4f ? vec2{th.x / hd2, th.y / hd2} : vec2{0.f, 0.f};
        vec2 sep{0.f, 0.f};
        for (const Org& b : orgs_) {
            if (&b == &o || b.fade >= 0.f) continue;
            const float dx = o.p.x - b.p.x, dy = o.p.y - b.p.y, R = (o.size + b.size) * 0.48f;
            const float dd = std::sqrt(dx * dx + dy * dy);
            if (dd < R && dd > 1e-4f) { sep.x += dx / dd * (1.f - dd / R); sep.y += dy / dd * (1.f - dd / R); }
        }
        // (the way out of the well and the HUD's columns is sideways: their union's gradient may point back in)
        if (std::fabs(o.p.x) < 11.8f && zd < margin) g = vec2{o.p.x < 0.f ? -1.f : 1.f, 0.f};
        const float wz = saturate((margin - zd) / margin), wh = smoothstepf(0.5f, 3.5f, hd2);
        const vec2 want{hn.x * wh * 0.9f + sep.x * 2.f + g.x * wz * 2.5f, hn.y * wh * 0.9f + sep.y * 2.f + g.y * wz * 2.5f};
        const float wl = std::sqrt(want.x * want.x + want.y * want.y);
        if (wl > 0.05f) {
            const float diff = wrapAngle(std::atan2(want.y, want.x) - o.ang);
            o.ang += (diff > 0 ? 1.f : -1.f) * std::min(std::fabs(diff), (0.15f + 0.35f * std::min(wl, 1.f) + 1.2f * wz) * wdt);
        }
        // the stroke: a push and a glide (its speed and its squash and stretch on the same cycle)
        o.phase += wdt * KIND_STROKE[o.kind] * TAU * (0.6f + 0.5f * act);
        const float push = 0.55f + 0.45f * std::sin(o.phase + 1.2f);
        float sp = o.speed * act * push * (1.f + 0.7f * kickEnv_ * (o.kind == K_BEAN || o.kind == K_TADPOLE));
        o.p.x += std::cos(o.ang) * sp * wdt;
        o.p.y += std::sin(o.ang) * sp * wdt;
        // carried out to its home at first (out of the well, behind the stack), eased, nudged apart from the others
        const float carry = (o.fromCell ? 3.2f : 0.f) * (1.f - smoothstepf(1.5f, 5.f, o.age)) * smoothstepf(0.2f, 2.5f, hd2) *
                            smoothstepf(0.f, 0.35f, o.age);
        o.p.x += (hn.x * carry + sep.x * 1.2f) * wdt;
        o.p.y += (hn.y * carry + sep.y * 1.2f) * wdt;
        // inside a forbidden place (past its birth): eased out
        const float overlap = o.size * 0.35f - zd;
        if (overlap > 0.f && o.age > 2.f) {
            const float k = 1.5f * std::min(overlap, 1.f);
            o.p.x += g.x * k * wdt;
            o.p.y += g.y * k * wdt;
        }
        if (o.age > 6.f) o.fromCell = false;
        // the eyes look where it swims (eased), the wheels and petals turn, the babies roll
        o.look += wrapAngle(o.ang - o.look) * (1.f - std::exp(-wdt * 2.f));
        o.spin += wdt * (o.kind == K_ROTIFER ? 2.2f + 2.f * act : o.kind == K_DAISY ? 0.12f : o.kind == K_VOLVOX ? 0.35f : 0.f);
    }
    orgs_.erase(std::remove_if(orgs_.begin(), orgs_.end(), [](const Org& o) { return o.fade >= 1.f; }), orgs_.end());
    // a few big bubbles rising slowly at the sides
    {
        int deco = 0;
        for (const Bubble& b : bubbles_) deco += b.deco;
        if (deco < 7 && rng_.chance(wdt * 0.5f)) {
            Bubble b;
            const int side = rng_.chance(0.5f) ? 1 : -1;
            const float x = side * rng_.range(12.5f, std::max(13.f, diskR_ - 3.f));
            b.p = vec2{x, viewC_.y - viewH_.y - 1.f};
            b.v = vec2{0.f, rng_.range(0.25f, 0.5f)};
            b.t = 0.f;
            b.life = 2.f * viewH_.y / b.v.y + 4.f;
            b.r = rng_.range(0.35f, 0.75f);
            b.deco = true;
            bubbles_.push_back(b);
        }
    }
    for (Bubble& b : bubbles_) {
        const float bt = b.deco ? wdt : dt;
        b.t += bt;
        if (b.t < 0.f) continue;
        b.p.x += (b.v.x + (b.deco ? 0.15f * std::sin(b.t * 0.8f + b.r * 10.f) : 0.f)) * bt;
        b.p.y += b.v.y * bt;
        if (!b.deco) { b.v.x *= std::exp(-bt * 2.5f); b.v.y = std::max(b.v.y, 0.3f); }
    }
    bubbles_.erase(std::remove_if(bubbles_.begin(), bubbles_.end(), [](const Bubble& b) { return b.t > b.life; }), bubbles_.end());
    for (Pop& p : pops_) p.t += dt;
    pops_.erase(std::remove_if(pops_.begin(), pops_.end(), [](const Pop& p) { return p.t > POP_T; }), pops_.end());
    if (!bare_) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Instances.
    const float bridgeK = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    cells_.clear();
    lifeBack_.clear();
    lifeFront_.clear();
    for (const Bubble& b : bubbles_) {
        if (b.t < 0.f || (bare_ && !b.deco)) continue;
        const float s = b.t / b.life;
        float a = b.deco ? smoothstepf(0.f, 0.05f, s) * (1.f - smoothstepf(0.9f, 1.f, s)) * 0.55f * smoothstepf(-1.f, 1.f, zoneDist(b.p, nullptr))
                         : smoothstepf(0.f, 0.1f, s) * (1.f - smoothstepf(0.4f, 1.f, s)) * 0.8f;
        a *= 1.f - bridgeK;
        if (a <= 0.003f) continue;
        const vec3 c = lerp(pal_.water[0], vec3(1.f), 0.75f);
        lifeBack_.push_back({{b.p.x, b.p.y, b.r * 2.5f, 0.f}, {(float)K_BUBBLE, 0.f, 0.f, a}, {c.x, c.y, c.z, 0.f},
                             {c.x, c.y, c.z, 0.f}, {0.f, 0.f, 0.f, 0.f}});
    }
    for (int layer = 1; layer >= 0; layer--)
        for (const Org& o : orgs_) {
            if ((o.depth > 0.5f) != (layer == 1)) continue;
            vec2 g;
            const float zd = zoneDist(o.p, &g);
            // calm behind the well and the HUD: a newborn crossing it shows faintly (behind the stack), then keeps out
            // (by its body's edge: the zones are generous around the text)
            const float settled = lerpf(0.2f, 1.f, smoothstepf(-0.8f, 0.3f, zd - o.size * 0.4f * 0.7f));
            const float newborn = o.fromCell ? 1.f - smoothstepf(1.5f, 3.5f, o.age) : 0.f;
            const bool inWell = std::fabs(o.p.x) < 5.6f;
            float a = lerpf(settled, std::max(settled, inWell ? 0.75f : 0.2f), newborn);
            if (!o.fromCell) a *= smoothstepf(0.f, 0.8f, o.age);
            float fadeK = 1.f;
            if (o.fade >= 0.f) { a *= 1.f - smoothstepf(0.3f, 1.f, o.fade); fadeK = 1.f - 0.6f * smoothstepf(0.f, 1.f, o.fade); }
            a *= 1.f - bridgeK;
            const float grow = lerpf(o.grow0, 1.f, smoothstepf(0.f, o.fromCell ? 2.4f : 1.5f, o.age));
            addLife(lifeBack_, o, a, grow * growK * fadeK * (o.depth > 0.5f ? 0.72f : 1.f));
        }
    if (!bare_) { // the world alone: no stack, nothing popping or deflating
        for (const Pop& p : pops_) addCell(p.x, p.y, 0.5f, p.type, 2, saturate(p.t / POP_T), 1.f, 0.f);
        if (overT_ >= 0.f)
            for (const Dying& d : dying_) {
                const float t = saturate((overT_ - d.delay) / DIE_T);
                if (t < 1.f) addCell(d.x, d.y, 0.5f, d.type, 3, t, 1.f, 0.f);
            }
        buildBoard(game);
    }

    // ---- Draw.
    const float aaW = 2.f * viewH_.y / (float)height_;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progField_);
    setPalette(progField_);
    set4f(progField_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progField_, "uDisk", viewC_.y, diskR_);
    set1f(progField_, "uTime", worldTime_);
    set1f(progField_, "uSwell", swell_ * (1.f - 0.6f * slow_));
    set1f(progField_, "uSeed", seedF_);
    set1f(progField_, "uAA", aaW);
    set1f(progField_, "uWell", bare_ ? 0.f : 1.f - bridgeK);
    set3f(progField_, "uLine", pal_.line);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // the scene's alpha is the stack's (for the bridge)
    if (!lifeBack_.empty()) {
        glUseProgram(progLife_);
        setPalette(progLife_);
        set1f(progLife_, "uGlow", 1.f + 0.06f * wp + 0.12f * swell_);
        set1f(progLife_, "uPane", bare_ ? 0.f : 1.f - bridgeK);
        drawInst(progLife_, lifeVao_, lifeVbo_, lifeBack_.data(), lifeBack_.size() * sizeof(LInst), lifeBack_.size());
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(progPiece_);
    setPalette(progPiece_);
    drawInst(progPiece_, pieceVao_, pieceVbo_, cells_.data(), cells_.size() * sizeof(Inst), cells_.size());

    // ---- The frame.
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    set4f(progFinal_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progFinal_, "uDisk", viewC_.y, diskR_);
    set1f(progFinal_, "uRimW", RIM_W);
    set1f(progFinal_, "uSlow", slow_);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", bridgeK);
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    set3f(progFinal_, "uTable", pal_.table);
    set3f(progFinal_, "uRim", pal_.rim);
    set3f(progFinal_, "uRimInk", pal_.rimInk);
    set3f(progFinal_, "uNight", pal_.night);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
    if (gpuQ) {
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
        static double acc = 0.0;
        static int n = 0;
        acc += ns * 1e-6;
        if (++n == 240) {
            std::fprintf(stderr, "[microscope] gpu %.2f ms/frame (%dx%d, %d creatures)\n", acc / n, width_, height_, alive());
            acc = 0.0;
            n = 0;
        }
    }
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_microscope() {
    static const UniverseEntry e{
        "MICROSCOPE", "a picture-book Petri dish: jelly cells; each cleared row frees a cute creature that swims in the dish for the rest of the song",
        [] { return std::unique_ptr<Universe>(new MicroscopeUniverse()); },
        // A playful world: fits most songs, a little more the light and lively ones.
        [](const SongLook& l) { return 0.8f + 0.15f * l.brightness + 0.15f * (1.f - l.density); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
