// STAR CHART: an antique engraved celestial map that fills with constellations over the song.
//
// One world: a celestial atlas plate, gold engraved on deep navy (or silver on black, brown ink on parchment, gold on
// red lacquer like a Chinese star chart, brass on a teal planisphere). The sheet has a graduated neatline with
// alternate bars; the margins hold two hemispheres, each a planisphere on its pole: a fixed rim graduated in degrees
// and hours, and inside it the turning sky with its declination circles, its hour lines, a graduated ecliptic, the
// Milky Way as a soft band of fine stippling and a field of engraved stars of six magnitudes. Above and below them, a
// cartouche, a table of the magnitudes and two compass roses. The well is a calm, darker panel in a double rule,
// graduated with a tick per row.
//
// The pieces are enamelled plaques: each piece one field of its enamel (seven sober jewel tones that read on the
// ground) inside a single gold rim, a fine inner rule following it and a small engraved star on each cell; the gold
// catches a slow sheen. The ghost is the rim alone; the previews are the same plaques.
//
// **The sky fills up over the song**: a cleared row's gold leaf flakes off (gone in about a third of a second, at the
// real speed even in slow motion) and each of its cells flies out as a star, on a gentle arc, to one of the
// hemispheres, where they light up as a new constellation (its brightest stars on the figure's joints, the others
// scattered around it as faint stars). Its lines are then engraved one by one on the bars (calm: one a bar; mid: every
// two beats; peak: every beat), and its name is lettered in italics once it is complete. A Tetris brings a great
// constellation, sends a comet across its hemisphere and leaves a nebula there. By the end of a long song both skies
// are full of constellations.
//
// The song: calm is a still plate (steady stars, the pen slow); mid twinkles a few stars every two beats; a peak
// twinkles more on every beat, draws faster and adds detail (the half-hour lines, the ecliptic's degrees), the kicks
// flaring a bright star or two. **The drop brings the figures to life**: the line-art figures around the
// constellations (a serpent, a fish, a swan, a laurel crown, a lyre, a bow and arrow, scales, a ship, a scorpion:
// figures.cpp) trace themselves in gold and start to move (the serpent winds, the fish beats its tail...), both skies
// turn slowly on their poles and the stars shine brighter, all eased in over half a second; after the drop the figures
// stay faintly engraved. Slow motion lays a silver armillary sphere over each hemisphere, turning slowly, and the
// gold turns to silver light. Game over drains the stack's enamel from the top, leaving the gold rims, and the sky
// dims.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane (like BLUEPRINT).
#include "universes/starchart/starchart.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/starchart/figures.hpp"
#include "universes/starchart/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Marcellus (engraved roman capitals) and Cormorant Garamond Italic (the song, the constellations' names), SIL Open
// Font License, see OFL-Marcellus.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/starchart/Marcellus-Regular.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/starchart/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = starchart_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;
using Stroke = StarChartUniverse::Stroke;

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 sub(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
float len(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
vec2 polar(vec2 c, float r, float a) { return {c.x + r * std::cos(a), c.y + r * std::sin(a)}; }
vec2 rot(vec2 v, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return {c * v.x - s * v.y, s * v.x + c * v.y};
}
vec2 lerp2(vec2 a, vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A plate (sRGB): the ground, its mottle, the well's panel, the engraved lines, the faint lines, the stars, their
// glow, the accent, the HUD's ink, the pieces' rim, the slow motion's silver; the seven enamels (I O T S Z J L);
// a pale sheet, how much it glows, the stars' style, the enamel's opacity.
struct ChartDef {
    const char* name;
    unsigned sky, sky2, well, line, faint, star, glow, accent, ink, rim, silver;
    unsigned piece[7];
    float light, glowA, starStyle, enamel;
};
const ChartDef VARIANTS[] = {
    // Gold engraved on deep navy, the classic atlas plate.
    {"AURUM", 0x0f1e3f, 0x14264d, 0x08122a, 0xdcb662, 0x7f6c43, 0xffe4a8, 0xffc566, 0xffd47a, 0xeedcac, 0xd2a446, 0xdce6f4,
     {0x2fa3b2, 0xdca63e, 0x8a5cba, 0x3c9e66, 0xbc4250, 0x4a72d4, 0xd27236}, 0.f, 1.f, 0.f, 0.96f},
    // Silver on black.
    {"ARGENT", 0x0a0c12, 0x10131b, 0x05060a, 0xcdd5e0, 0x596370, 0xf4f7ff, 0xb4c8ff, 0xe8eefc, 0xe2e7f0, 0xc6ced9, 0xffe2a8,
     {0x48bccb, 0xe2c25e, 0xa07cdc, 0x58bc88, 0xd65a6a, 0x5f88e6, 0xe48c50}, 0.f, 0.9f, 0.f, 0.96f},
    // Brown ink on parchment, hand-colored.
    {"SEPIA", 0xefe7d6, 0xe5dac4, 0xf7f2e8, 0x5c3c1e, 0xa58a63, 0x3c2614, 0x9a6a24, 0xa8321e, 0x3e2814, 0x4c3218, 0x34486c,
     {0x2a8296, 0xc98a1a, 0x7a4598, 0x4a8a40, 0xb23a2a, 0x2e4c9a, 0xc8641e}, 1.f, 0.f, 0.f, 0.92f},
    // Gold on red lacquer, a Chinese star chart: open circles for the stars.
    {"VERMILION", 0x2c0c0d, 0x371112, 0x1d0607, 0xe2ba62, 0x8a5c33, 0xffe8b4, 0xffae5c, 0xff7050, 0xf2dcae, 0xdcac4a, 0xe8eef8,
     {0x4fbcaa, 0xe8c672, 0xa282d4, 0x86bc62, 0xece2d0, 0x4c8cdc, 0xec8e4c}, 0.f, 1.f, 1.f, 0.96f},
    // Brass on a teal planisphere, verdigris accents.
    {"PLANISPHERE", 0x0b2127, 0x0f2a31, 0x06161a, 0xdcc084, 0x5f8379, 0xfff2d4, 0xa6f0de, 0x80e2ca, 0xeadfc4, 0xcca464, 0xe4ecf6,
     {0x5acbdb, 0xf2ca62, 0xb28ae2, 0x72d29a, 0xea6a72, 0x6a92f2, 0xf29a5a}, 0.f, 1.f, 0.f, 0.96f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = StarChartUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const ChartDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.sky = L(d.sky); p.sky2 = L(d.sky2); p.well = L(d.well); p.line = L(d.line); p.faint = L(d.faint);
    p.star = L(d.star); p.glow = L(d.glow); p.accent = L(d.accent); p.ink = L(d.ink); p.rim = L(d.rim);
    p.silver = L(d.silver);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.light = d.light; p.glowA = d.glowA; p.starStyle = d.starStyle; p.enamel = d.enamel;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

std::vector<vec2> circlePts(vec2 c, float r, int n, float a0 = 0.f) {
    std::vector<vec2> p;
    p.reserve(n + 1);
    for (int i = 0; i <= n; i++) p.push_back(polar(c, r, a0 + TAU * i / n));
    return p;
}
std::vector<vec2> arcPts(vec2 c, float r, float a0, float a1, int n) {
    std::vector<vec2> p;
    for (int i = 0; i <= n; i++) p.push_back(polar(c, r, a0 + (a1 - a0) * i / n));
    return p;
}

const char* ROMAN[] = {"XII", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI"};
const char* SUFFIX[] = {"", " Minor", " Australis", " Borealis", " Nova", " Altera"};

constexpr float CLEAR_T = 0.34f;   // a cleared row's gold leaf is gone in this long
constexpr int MAX_NEBULAE = 8;
constexpr int DISC_CONSTELLATIONS = 6; // lettered constellations a hemisphere holds (the oldest then retires)
constexpr int DISC_FAINT = 90;         // faint stars the clears may add to a hemisphere

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool StarChartUniverse::init(int width, int height) {
    progSky_ = ugl::program(ugl::FULLSCREEN_VS, shaders::SKY_FS, "starchart sky");
    progLine_ = ugl::program(shaders::LINE_VS, shaders::LINE_FS, "starchart line");
    progLayer_ = ugl::program(ugl::FULLSCREEN_VS, shaders::LAYER_FS, "starchart layer");
    progCell_ = ugl::program(shaders::CELL_VS, shaders::CELL_FS, "starchart cell");
    progStar_ = ugl::program(shaders::STAR_VS, shaders::STAR_FS, "starchart star");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "starchart shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "starchart text");
    progPost_ = ugl::program(ugl::FULLSCREEN_VS, shaders::POST_FS, "starchart post");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, int attribs, int stride) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < attribs; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(lineVao_, lineVbo_, 3, sizeof(LineInst));
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));
    instanced(starVao_, starVbo_, 3, sizeof(StarInst));
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

    buildFont(caps_, FONT_CAPS);
    buildFont(ital_, FONT_ITAL);
    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    seedField();
    return glGetError() == GL_NO_ERROR;
}

void StarChartUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] starchart font init failed\n");
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

void StarChartUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    lineT_.create(width_, height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    layoutDiscs();
}

StarChartUniverse::~StarChartUniverse() {
    sceneT_.destroy();
    lineT_.destroy();
    for (GLuint p : {progSky_, progLine_, progLayer_, progCell_, progStar_, progShape_, progText_, progPost_}) glDeleteProgram(p);
    for (GLuint v : {lineVao_, cellVao_, starVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, lineVbo_, cellVbo_, starVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&caps_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Engraved lines
// =====================================================================================================

float StarChartUniverse::fineW() const { return std::max(0.8f, 1.15f * (height_ / 1080.f)); }

vec3 StarChartUniverse::roleColor(int role) const {
    switch (role) {
    case 1: return pal_.faint;
    case 2: return pal_.accent;
    case 3: return pal_.silver;
    case 4: return pal_.star;
    default: return pal_.line;
    }
}

void StarChartUniverse::segment(vec2 a, vec2 b, const vec3& c, float alpha, float w, float glow) {
    lines_.push_back({{a.x, a.y, b.x, b.y}, {c.x, c.y, c.z, alpha}, {w, glow * pal_.glowA, 0.f, 0.f}});
}

// Emits the stroke's segments up to `reveal` of its length (the burin's progress).
void StarChartUniverse::emit(const Stroke& s, float reveal, float alpha) {
    const float a = alpha * s.alpha;
    if (a <= 0.004f || reveal <= 0.f || s.pts.size() < 2) return;
    float total = 0.f;
    for (size_t i = 1; i < s.pts.size(); i++) total += len(sub(s.pts[i], s.pts[i - 1]));
    const float stop = total * std::min(1.f, reveal);
    const vec3 c = roleColor(s.role);
    const float w = fineW() * s.w;
    float d = 0.f;
    for (size_t i = 1; i < s.pts.size() && d < stop; i++) {
        vec2 p0 = s.pts[i - 1], p1 = s.pts[i];
        const float L = len(sub(p1, p0));
        if (L < 1e-6f) continue;
        if (d + L > stop) p1 = add(p0, mul(sub(p1, p0), (stop - d) / L));
        segment(p0, p1, c, a, w, s.glow);
        d += L;
    }
}

void StarChartUniverse::emitAll(const std::vector<Stroke>& ss, float reveal, float alpha) {
    if (reveal <= 0.f || alpha <= 0.004f) return;
    if (reveal >= 1.f) {
        for (const Stroke& s : ss) emit(s, 1.f, alpha);
        return;
    }
    // the burin goes around the strokes in order
    std::vector<float> L(ss.size(), 0.f);
    float total = 0.f;
    for (size_t k = 0; k < ss.size(); k++) {
        for (size_t i = 1; i < ss[k].pts.size(); i++) L[k] += len(sub(ss[k].pts[i], ss[k].pts[i - 1]));
        total += L[k];
    }
    const float target = total * reveal;
    float acc = 0.f;
    for (size_t k = 0; k < ss.size() && acc < target; k++) {
        emit(ss[k], L[k] > 0.f ? (target - acc) / L[k] : 1.f, alpha);
        acc += L[k];
    }
}

// =====================================================================================================
// The plate: the hemispheres, the neatline, the well, the ornaments
// =====================================================================================================

void StarChartUniverse::layoutDiscs() {
    const float hw = viewH_.x, hh = viewH_.y;
    const float outer = viewC_.x + hw - 0.85f, inner = 10.75f;
    const float rt = std::clamp((outer - inner) * 0.5f, 2.2f, 6.4f);
    const float cx = (outer + inner) * 0.5f;
    const float cy = viewC_.y - 0.1f;
    for (int d = 0; d < 2; d++) {
        disc_[d].c = V(d == 0 ? -cx : cx, cy);
        disc_[d].R = rt - 0.62f;
        disc_[d].dir = d == 0 ? 1.f : -1.f;
    }
    // the ornaments above and below the hemispheres (the title above on the left, the section under it)
    const float top = viewC_.y + hh, bot = viewC_.y - hh;
    const float aboveY0 = cy + rt + 0.55f, belowY1 = cy - rt - 0.35f;
    ornaments_ = (top - aboveY0) > 2.6f;
    cartW_ = std::min(2.f * rt, 8.6f);
    cartH_ = std::clamp(top - 0.9f - aboveY0, 1.2f, 2.6f);
    cartC_ = V(cx, aboveY0 + cartH_ * 0.5f + 0.15f);
    rose2R_ = std::clamp((top - 2.6f - aboveY0) * 0.5f, 0.9f, 1.7f);
    rose2C_ = V(-cx, aboveY0 + rose2R_ + 0.2f);
    roseR_ = std::clamp((belowY1 - (bot + 0.9f)) * 0.5f, 1.f, 2.3f);
    roseC_ = V(cx, belowY1 - roseR_ - 0.1f);
    legendW_ = std::min(2.f * rt, 8.2f);
    legendH_ = std::clamp(belowY1 - (bot + 2.9f), 1.1f, 2.1f);
    legendC_ = V(-cx, belowY1 - legendH_ * 0.5f - 0.1f);
}

vec2 StarChartUniverse::toWorld(int d, vec2 local) const {
    const Disc& k = disc_[d];
    return add(k.c, rot(mul(local, k.R), k.rot));
}

// A hemisphere: the fixed rim (a double rule, ticks every 5 and 15 degrees, the hours in Roman numerals) and the
// turning sky (the pole, declination circles, hour lines, the ecliptic as a graduated double band).
void StarChartUniverse::buildDisc(int d, std::vector<Stroke>& out, float detail) const {
    const Disc& k = disc_[d];
    const vec2 c = k.c;
    const float R = k.R;
    // the rim
    out.push_back(Stroke{circlePts(c, R, 160), 1.3f, 0.95f, 0});
    out.push_back(Stroke{circlePts(c, R + 0.07f, 160), 0.8f, 0.8f, 0});
    out.push_back(Stroke{circlePts(c, R + 0.5f, 170), 1.5f, 0.95f, 0});
    out.push_back(Stroke{circlePts(c, R + 0.56f, 170), 0.7f, 0.7f, 0});
    out.push_back(Stroke{circlePts(c, R + 0.24f, 160), 0.6f, 0.55f, 1});
    for (int i = 0; i < 72; i++) {
        const float a = PI * 0.5f - TAU * i / 72.f;
        const float l = (i % 6 == 0) ? 0.17f : (i % 3 == 0 ? 0.13f : 0.09f);
        out.push_back(Stroke{{polar(c, R + 0.07f, a), polar(c, R + 0.07f + l, a)}, i % 6 == 0 ? 1.f : 0.7f, 0.85f, 0});
    }
    // the turning sky
    auto P = [&](vec2 l) { return toWorld(d, l); };
    const float rk[3] = {0.25f, 0.5f, 0.75f};
    for (float r : rk) {
        Stroke s{{}, 0.7f, 0.5f, 1};
        for (int i = 0; i <= 120; i++) s.pts.push_back(P(V(r * std::cos(TAU * i / 120.f), r * std::sin(TAU * i / 120.f))));
        out.push_back(s);
    }
    for (int i = 0; i < 24; i++) {
        const bool hour = i % 2 == 0;
        if (!hour && detail < 0.02f) continue;
        const float a = TAU * i / 24.f;
        const float r0 = hour ? (i % 6 == 0 ? 0.04f : 0.25f) : 0.5f;
        out.push_back(Stroke{{P(V(r0 * std::cos(a), r0 * std::sin(a))), P(V(std::cos(a), std::sin(a)))}, 0.65f,
                             hour ? 0.5f : 0.35f * detail, 1});
    }
    // the pole
    out.push_back(Stroke{{P(V(-0.05f, 0.f)), P(V(0.05f, 0.f))}, 0.9f, 0.9f, 0});
    out.push_back(Stroke{{P(V(0.f, -0.05f)), P(V(0.f, 0.05f))}, 0.9f, 0.9f, 0});
    {
        Stroke s{{}, 0.8f, 0.8f, 0};
        for (int i = 0; i <= 24; i++) s.pts.push_back(P(V(0.022f * std::cos(TAU * i / 24.f), 0.022f * std::sin(TAU * i / 24.f))));
        out.push_back(s);
    }
    // the ecliptic: an off-center circle, doubled, graduated every ten degrees (every two at a peak)
    const vec2 ec = V(0.f, d == 0 ? 0.2f : -0.2f);
    const float er = 0.74f, eb = 0.035f;
    for (int j = 0; j < 2; j++) {
        Stroke s{{}, j == 0 ? 1.f : 0.7f, 0.85f, 0};
        for (int i = 0; i <= 140; i++) {
            const float a = TAU * i / 140.f;
            s.pts.push_back(P(add(ec, V((er + j * eb) * std::cos(a), (er + j * eb) * std::sin(a)))));
        }
        out.push_back(s);
    }
    for (int i = 0; i < 180; i++) {
        const bool ten = i % 5 == 0;
        if (!ten && detail < 0.02f) continue;
        const float a = TAU * i / 180.f;
        const vec2 u = V(std::cos(a), std::sin(a));
        out.push_back(Stroke{{P(add(ec, mul(u, er))), P(add(ec, mul(u, er + eb * (ten ? 1.f : 0.6f))))}, 0.6f,
                             ten ? 0.8f : 0.6f * detail, 0});
    }
}

// The neatline: a heavy and a fine rule around the sheet, alternate bars between them every unit (the degrees).
void StarChartUniverse::buildBorder(std::vector<Stroke>& out) const {
    const float x0 = viewC_.x - viewH_.x + 0.28f, x1 = viewC_.x + viewH_.x - 0.28f;
    const float y0 = viewC_.y - viewH_.y + 0.28f, y1 = viewC_.y + viewH_.y - 0.28f;
    const float g = 0.17f;
    out.push_back(Stroke{{V(x0, y0), V(x1, y0), V(x1, y1), V(x0, y1), V(x0, y0)}, 1.7f, 0.9f, 0});
    out.push_back(Stroke{{V(x0 + g, y0 + g), V(x1 - g, y0 + g), V(x1 - g, y1 - g), V(x0 + g, y1 - g), V(x0 + g, y0 + g)}, 0.8f, 0.8f, 0});
    const float bw = g * pxPerUnit_ / fineW() * 0.55f; // a bar fills the band
    auto bars = [&](vec2 a, vec2 b) {
        const float L = len(sub(b, a));
        const int n = (int)std::floor(L);
        const float off = (L - n) * 0.5f;
        for (int i = 0; i < n; i += 2) {
            const float t0 = (off + i) / L, t1 = (off + i + 1) / L;
            out.push_back(Stroke{{lerp2(a, b, t0), lerp2(a, b, t1)}, bw, 0.55f, 0});
        }
    };
    const float m = g * 0.5f;
    bars(V(x0 + g, y0 + m), V(x1 - g, y0 + m));
    bars(V(x0 + g, y1 - m), V(x1 - g, y1 - m));
    bars(V(x0 + m, y0 + g), V(x0 + m, y1 - g));
    bars(V(x1 - m, y0 + g), V(x1 - m, y1 - g));
}

// The well: a double rule, a tick per row on both sides (longer every five).
void StarChartUniverse::buildWell(std::vector<Stroke>& out) const {
    const float a = 5.1f, b = 10.1f, e = 0.16f;
    out.push_back(Stroke{{V(-a, b), V(a, b), V(a, -b), V(-a, -b), V(-a, b)}, 1.2f, 0.9f, 0});
    out.push_back(Stroke{{V(-a - e, b + e), V(a + e, b + e), V(a + e, -b - e), V(-a - e, -b - e), V(-a - e, b + e)}, 0.7f, 0.7f, 0});
    for (int i = 0; i <= 20; i++) {
        const float y = -10.f + i;
        const float l = i % 5 == 0 ? 0.32f : 0.14f;
        out.push_back(Stroke{{V(-a - e, y), V(-a - e - l, y)}, 0.8f, 0.75f, 0});
        out.push_back(Stroke{{V(a + e, y), V(a + e + l, y)}, 0.8f, 0.75f, 0});
    }
}

// A compass rose: sixteen points, the cardinal ones long and half shaded with fine hatching, a ring of degrees.
void StarChartUniverse::buildRose(vec2 c, float r, std::vector<Stroke>& out) const {
    out.push_back(Stroke{circlePts(c, r * 0.62f, 90), 0.8f, 0.8f, 0});
    out.push_back(Stroke{circlePts(c, r * 0.68f, 90), 0.6f, 0.6f, 1});
    for (int i = 0; i < 64; i++) {
        const float a = TAU * i / 64.f;
        out.push_back(Stroke{{polar(c, r * 0.62f, a), polar(c, r * (i % 4 == 0 ? 0.68f : 0.65f), a)}, 0.5f, 0.6f, 1});
    }
    for (int i = 0; i < 16; i++) {
        const float a = PI * 0.5f - TAU * i / 16.f;
        const float L = i % 4 == 0 ? r : (i % 2 == 0 ? r * 0.7f : r * 0.48f);
        const float wd = i % 4 == 0 ? 0.13f * r : (i % 2 == 0 ? 0.1f * r : 0.07f * r);
        const vec2 tip = polar(c, L, a), l = polar(c, wd, a + PI * 0.5f), rr = polar(c, wd, a - PI * 0.5f);
        out.push_back(Stroke{{l, tip, rr}, i % 4 == 0 ? 1.f : 0.8f, i % 4 == 0 ? 0.95f : 0.75f, 0});
        out.push_back(Stroke{{c, tip}, 0.6f, 0.7f, 0});
        // the shaded half: hatching parallel to the point's axis
        if (i % 2 == 0) {
            const int n = i % 4 == 0 ? 7 : 4;
            for (int h = 1; h < n; h++) {
                const float u = (float)h / n;
                const vec2 p0 = lerp2(c, l, u), p1 = lerp2(tip, l, u);
                out.push_back(Stroke{{p0, lerp2(p0, p1, 0.92f)}, 0.45f, 0.55f, 0});
            }
        }
    }
    out.push_back(Stroke{circlePts(c, r * 0.06f, 20), 0.8f, 0.9f, 0});
}

// A cartouche: a scroll of parchment drawn in line, its two ends rolled, a fine inner rule.
void StarChartUniverse::buildCartouche(vec2 c, float w, float h, std::vector<Stroke>& out) const {
    const float x0 = c.x - w * 0.5f, x1 = c.x + w * 0.5f, y0 = c.y - h * 0.5f, y1 = c.y + h * 0.5f;
    const float rr = h * 0.18f;
    // the body with a gentle wave on its long sides
    Stroke top{{}, 1.f, 0.9f, 0}, bot{{}, 1.f, 0.9f, 0};
    for (int i = 0; i <= 40; i++) {
        const float u = i / 40.f, x = lerpf(x0 + rr, x1 - rr, u);
        top.pts.push_back(V(x, y1 - 0.06f * h * std::sin(u * TAU)));
        bot.pts.push_back(V(x, y0 - 0.06f * h * std::sin(u * TAU)));
    }
    out.push_back(top);
    out.push_back(bot);
    // the rolled ends: a spiral each
    for (int s = -1; s <= 1; s += 2) {
        const float ex = s < 0 ? x0 + rr : x1 - rr;
        Stroke sp{{}, 0.9f, 0.85f, 0};
        for (int i = 0; i <= 40; i++) {
            const float u = i / 40.f, a = (s < 0 ? -1.f : 1.f) * (PI * 0.5f + u * TAU * 1.15f);
            const float r = (h * 0.5f) * (1.f - 0.72f * u);
            sp.pts.push_back(V(ex + s * 0.f + r * std::cos(a) * 0.55f, c.y + r * std::sin(a)));
        }
        out.push_back(sp);
    }
    // the inner rule
    const float ix = rr * 1.9f, iy = h * 0.2f;
    out.push_back(Stroke{{V(x0 + ix, y0 + iy), V(x1 - ix, y0 + iy), V(x1 - ix, y1 - iy), V(x0 + ix, y1 - iy), V(x0 + ix, y0 + iy)}, 0.55f, 0.55f, 1});
}

// The table of magnitudes: a framed strip with a star of each of the six magnitudes (the stars are sprites).
void StarChartUniverse::buildLegend(vec2 c, float w, float h, std::vector<Stroke>& out) const {
    const float x0 = c.x - w * 0.5f, x1 = c.x + w * 0.5f, y0 = c.y - h * 0.5f, y1 = c.y + h * 0.5f;
    out.push_back(Stroke{{V(x0, y0), V(x1, y0), V(x1, y1), V(x0, y1), V(x0, y0)}, 0.9f, 0.85f, 0});
    const float e = 0.1f;
    out.push_back(Stroke{{V(x0 + e, y0 + e), V(x1 - e, y0 + e), V(x1 - e, y1 - e), V(x0 + e, y1 - e), V(x0 + e, y0 + e)}, 0.55f, 0.6f, 1});
    out.push_back(Stroke{{V(x0 + e, c.y + h * 0.14f), V(x1 - e, c.y + h * 0.14f)}, 0.5f, 0.5f, 1});
    for (int i = 1; i < 6; i++) {
        const float x = lerpf(x0, x1, i / 6.f);
        out.push_back(Stroke{{V(x, y0 + e), V(x, c.y + h * 0.14f)}, 0.45f, 0.45f, 1});
    }
}

// An armillary sphere (slow motion): the meridian ring, the horizon, the equator and the tilted ecliptic band,
// the colures, all turning slowly about the tilted axis, drawn as ellipses in a light projection.
void StarChartUniverse::buildArmillary(vec2 c, float r, float t, std::vector<Stroke>& out) const {
    const float tilt = 0.41f, spin = t * 0.25f, view = 0.32f;
    // a ring: a great circle of normal n (3D), drawn through a projection looking slightly from above
    auto ring = [&](float ax, float ay, float az, float rad, float w, float a) {
        float nx = ax, ny = ay, nz = az;
        const float l = std::sqrt(nx * nx + ny * ny + nz * nz);
        nx /= l; ny /= l; nz /= l;
        // two axes of the circle's plane
        float ux = -ny, uy = nx, uz = 0.f;
        if (std::fabs(nz) > 0.9f) { ux = 1.f; uy = 0.f; uz = 0.f; }
        float ul = std::sqrt(ux * ux + uy * uy + uz * uz);
        ux /= ul; uy /= ul; uz /= ul;
        const float vx = ny * uz - nz * uy, vy = nz * ux - nx * uz, vz = nx * uy - ny * ux;
        Stroke s{{}, w, a, 3, 0.6f};
        for (int i = 0; i <= 96; i++) {
            const float q = TAU * i / 96.f;
            const float px = (ux * std::cos(q) + vx * std::sin(q)) * rad, py = (uy * std::cos(q) + vy * std::sin(q)) * rad,
                        pz = (uz * std::cos(q) + vz * std::sin(q)) * rad;
            // look from slightly above: y and z mixed
            const float sx = px, sy = py * std::cos(view) - pz * std::sin(view);
            s.pts.push_back(V(c.x + sx * r, c.y + sy * r));
        }
        out.push_back(s);
    };
    // the axis tilted toward us, the sphere turning about it
    auto axisRot = [&](float x, float y, float z, float& ox, float& oy, float& oz) {
        // spin about y, then tilt about x
        const float cs = std::cos(spin), sn = std::sin(spin);
        const float x1 = cs * x + sn * z, z1 = -sn * x + cs * z;
        const float ct = std::cos(tilt), st = std::sin(tilt);
        ox = x1;
        oy = ct * y - st * z1;
        oz = st * y + ct * z1;
    };
    float ax, ay, az;
    ring(0.f, 0.f, 1.f, 1.f, 1.3f, 0.9f);            // the meridian (fixed, facing us)
    ring(0.f, 1.f, 0.f, 1.f, 1.1f, 0.75f);           // the horizon
    axisRot(0.f, 1.f, 0.f, ax, ay, az);
    ring(ax, ay, az, 0.94f, 1.f, 0.8f);              // the equator
    axisRot(0.f, std::cos(0.41f), std::sin(0.41f), ax, ay, az);
    ring(ax, ay, az, 0.9f, 1.f, 0.8f);               // the ecliptic
    ring(ax, ay, az, 0.86f, 0.6f, 0.6f);
    axisRot(1.f, 0.f, 0.f, ax, ay, az);
    ring(ax, ay, az, 0.94f, 0.7f, 0.6f);             // the colures
    axisRot(0.f, 0.f, 1.f, ax, ay, az);
    ring(ax, ay, az, 0.94f, 0.7f, 0.6f);
    axisRot(0.f, 1.f, 0.f, ax, ay, az);
    const float sy0 = ay * std::cos(view) - az * std::sin(view);
    out.push_back(Stroke{{V(c.x - ax * r * 1.15f, c.y - sy0 * r * 1.15f), V(c.x + ax * r * 1.15f, c.y + sy0 * r * 1.15f)}, 1.f, 0.8f, 3, 0.6f});
    out.push_back(Stroke{circlePts(c, r * 0.12f, 24), 1.f, 0.9f, 3, 0.8f});
}

// =====================================================================================================
// Stars and constellations
// =====================================================================================================

// The fixed field of each hemisphere: a few hundred stars, most of them faint, denser along the Milky Way.
void StarChartUniverse::seedField() {
    stars_.clear();
    cons_.clear();
    flights_.clear();
    nebulae_.clear();
    comets_.clear();
    disc_[0].count = disc_[1].count = 0;
    Rng r(seed_ ^ 0x5747ull);
    for (int d = 0; d < 2; d++) {
        for (int i = 0; i < 60; i++) {
            float x, y;
            do { x = r.range(-1.f, 1.f); y = r.range(-1.f, 1.f); } while (x * x + y * y > 0.93f * 0.93f);
            Star s;
            s.disc = d;
            s.p = V(x, y);
            const float u = r.uniform();
            s.mag = u < 0.03f ? r.range(1.8f, 2.6f) : u < 0.16f ? r.range(2.9f, 4.f) : r.range(4.5f, 6.f);
            s.born = 1.f;
            s.phase = r.range(0.f, TAU);
            stars_.push_back(s);
        }
    }
    figureBagN_ = 0;
}

int StarChartUniverse::nextFigure() {
    // a shuffled bag of the ordinary figures (the scorpion is kept for a Tetris)
    if (figureBagN_ <= 0) {
        const int n = starchart::figureCount() - 1;
        for (int i = 0; i < n; i++) figureBag_[i] = i;
        for (int i = n - 1; i > 0; i--) std::swap(figureBag_[i], figureBag_[rng_.irange(0, i)]);
        figureBagN_ = n;
    }
    return figureBag_[--figureBagN_];
}

vec2 StarChartUniverse::consPoint(const Constellation& k, vec2 p) const {
    if (k.mirror) p.x = -p.x;
    return add(k.c, mul(rot(p, k.ang), k.scale));
}

// A clear: a new constellation in the emptier hemisphere, at its emptiest spot; the cells fly there as its stars.
void StarChartUniverse::addConstellation(const std::vector<vec2>& from, bool big) {
    if (from.empty()) return;
    const int d = disc_[0].count == disc_[1].count ? rng_.irange(0, 1) : disc_[0].count < disc_[1].count ? 0 : 1;
    Disc& D = disc_[d];
    // a full hemisphere: its oldest constellation retires (its lines and name fade, its stars join the field)
    if (D.count >= DISC_CONSTELLATIONS)
        for (Constellation& o : cons_)
            if (o.disc == d && !o.retired) {
                o.retired = true;
                for (int si : o.stars) {
                    stars_[si].cons = -1;
                    stars_[si].mag = std::max(4.3f, stars_[si].mag + 1.5f);
                    stars_[si].extra = true;
                }
                D.count--;
                break;
            }
    // and the oldest faint stars give way to the new ones
    {
        int faint = 0;
        for (const Star& st : stars_)
            if (st.disc == d && st.extra && st.fade > 0.f) faint++;
        const int over = faint + (int)from.size() - DISC_FAINT;
        int n = 0;
        for (Star& st : stars_)
            if (n < over && st.disc == d && st.extra && st.fade >= 1.f) { st.fade = 0.999f; n++; }
    }
    Constellation k;
    k.disc = d;
    k.figure = big ? starchart::figureCount() - 1 : nextFigure();
    k.big = big ? 1.f : 0.f;
    // smaller once the sky gets crowded
    k.scale = big ? 0.46f : rng_.range(0.3f, 0.34f);
    k.ang = rng_.range(-0.45f, 0.45f);
    k.mirror = rng_.chance(0.5f);
    // the emptiest spot: the candidate farthest from the other constellations (and from the rim)
    float best = -1e9f;
    for (int tries = 0; tries < 40; tries++) {
        const float a = rng_.range(0.f, TAU), rr = std::sqrt(rng_.uniform()) * std::max(0.f, 0.9f - k.scale);
        const vec2 p = V(rr * std::cos(a), rr * std::sin(a));
        float m = 9.f;
        for (const Constellation& o : cons_)
            if (o.disc == d && !o.retired) m = std::min(m, len(sub(o.c, p)) - (o.scale + k.scale) * 0.9f);
        m = std::min(m, (0.95f - k.scale - len(p)) * 2.f);
        m += rng_.range(0.f, 0.03f);
        if (m > best) { best = m; k.c = p; }
    }
    const starchart::FigureDef& fig = starchart::figure(k.figure);
    // the figure's stars first (the brightest on the first joints), then faint ones scattered around it
    const int n = (int)from.size();
    for (int i = 0; i < n; i++) {
        Star s;
        s.disc = d;
        s.born = 0.f;
        s.phase = rng_.range(0.f, TAU);
        if (i < (int)fig.anchors.size()) {
            s.p = consPoint(k, fig.anchors[i]);
            s.mag = i < 2 ? rng_.range(1.3f, 2.2f) : rng_.range(2.3f, 3.6f);
            if (big) s.mag -= 0.4f;
            s.cons = (int)cons_.size();
            k.stars.push_back((int)stars_.size());
        } else {
            const float a = rng_.range(0.f, TAU), rr = k.scale * rng_.range(0.4f, 1.5f);
            s.p = add(k.c, V(rr * std::cos(a), rr * std::sin(a)));
            if (len(s.p) > 0.95f) s.p = mul(s.p, 0.95f / len(s.p));
            s.mag = rng_.range(4.4f, 6.f);
            s.extra = true;
        }
        Flight fl;
        fl.from = from[i];
        fl.star = (int)stars_.size();
        fl.delay = 0.12f + 0.022f * i + rng_.range(0.f, 0.05f);
        fl.dur = rng_.range(1.25f, 1.75f) * (big ? 1.15f : 1.f);
        fl.bend = rng_.range(0.6f, 1.2f);
        flights_.push_back(fl);
        stars_.push_back(s);
    }
    // anchors left without a star (a clear always brings at least ten): none in practice
    for (const auto& e : fig.edges)
        if (e.first < (int)k.stars.size() && e.second < (int)k.stars.size()) k.edges.push_back(e);
    k.edgeReveal.assign(k.edges.size(), 0.f);
    k.ready = -1.f;
    // its name: the figure's, with an epithet when it comes back
    int same = 0;
    for (const Constellation& o : cons_)
        if (o.figure == k.figure && !o.retired) same++;
    k.name = std::string(fig.name) + SUFFIX[std::min(same, 5)];
    D.count++;
    cons_.push_back(k);
}

// The burin engraves the next lines: the oldest unfinished constellation whose stars are all lit.
void StarChartUniverse::penStep(float speed) {
    int steps = 1;
    int backlog = 0;
    for (const Constellation& k : cons_)
        if (!k.retired) backlog += (int)k.edges.size() - k.nextEdge;
    if (backlog > 10) steps = 2;
    if (backlog > 24) steps = 3;
    for (int s = 0; s < steps; s++)
        for (Constellation& k : cons_) {
            if (k.retired || k.ready < 0.f || k.nextEdge >= (int)k.edges.size()) continue;
            k.edgeReveal[k.nextEdge] = std::max(k.edgeReveal[k.nextEdge], 0.0001f);
            k.nextEdge++;
            break;
        }
    (void)speed;
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void StarChartUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && !t.minor && t.brightness > 0.62f) v = 2;            // parchment for bright, major songs
        else if (t.hasSong && t.minor && t.brightness < 0.4f) v = 1;        // silver on black for dark ones
        else {
            static const int BY_HUE[] = {0, 3, 0, 4, 0, 4};                  // gold on navy the most often
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const uint64_t old = seed_;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x57A2ull);
    if (seconds <= 0.f) palSet_ = false;
    char buf[24];
    std::snprintf(buf, sizeof(buf), "TAB. %s", ROMAN[(seed_ >> 8) % 12]);
    chartNo_ = buf;
    // a new song: a new sky
    if (old != seed_) seedField();
}

void StarChartUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropMode_ = false;
    const bool instant = !hasLevel_ || seconds < 0.1f;
    (void)impact;
    if (instant) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        detail_ = level == 2 ? 1.f : 0.f;
        return;
    }
    const bool rising = level > level_;
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.f, 6.f);
}

// The drop: the figures come alive and the skies turn (only on a real drop: see render).
void StarChartUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
}

void StarChartUniverse::refresh(float strength) {
    if (strength > 0.3f) penStep(1.f);
}

vec2 StarChartUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints StarChartUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = p.sky;
    h.light = p.light > 0.5f;
    h.ink = p.line;
    h.accent = p.accent;
    h.styles = TS_INK | TS_LUMA | TS_REBUILD;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void StarChartUniverse::syncIds(const Game& g) {
    bool ok = true;
    for (int y = 0; y < Game::H && ok; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) { ok = false; break; }
    if (ok) return;
    std::memset(ids_, 0, sizeof(ids_));
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
                const int dd[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& e : dd) {
                    const int nx = cx + e[0], ny = cy + e[1];
                    if (nx < 0 || ny < 0 || nx >= Game::W || ny >= Game::H || ids_[ny][nx]) continue;
                    if (g.cell(nx, ny).type != g.cell(x, y).type || sp >= 8 || count + sp >= 4) continue;
                    ids_[ny][nx] = id;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

void StarChartUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock:
    case GameEvent::HardDrop: {
        if (ev.type == GameEvent::Lock) {
            const int id = nextId_++;
            for (auto& c : ev.cells)
                if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        }
        if (ev.cells.empty()) break;
        // a small glint where the piece meets what is under it (its lowest cells' middle)
        float yLo = 99.f, xs = 0.f;
        int n = 0;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y < yLo + 0.01f) { xs += p.x; n++; }
        }
        if (ev.type == GameEvent::HardDrop || glints_.empty() || glints_.back().t > 0.1f)
            glints_.push_back({V(xs / std::max(1, n), yLo - 0.5f), 0.f, ev.type == GameEvent::HardDrop ? 1.f : 0.6f});
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        std::vector<vec2> from;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
                for (int i = 0; i < 8; i++) {
                    const int nx = c.x + dx[i], ny = c.y + dy[i];
                    if (nx < 0 || nx >= Game::W || ny < 0 || ny >= Game::H || !rows[ny]) continue;
                    if (ids_[ny][nx] == id) m |= 1 << i;
                }
            }
            fading_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f});
            from.push_back(V(p.x, p.y));
        }
        {
            int nb[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
        }
        // the cells fly out as the stars of a new constellation, in a shuffled order
        for (int i = (int)from.size() - 1; i > 0; i--) std::swap(from[i], from[rng_.irange(0, i)]);
        const bool tetris = ev.count >= 4;
        const size_t first = cons_.size();
        addConstellation(from, tetris);
        if (tetris && cons_.size() > first) {
            // a comet across that hemisphere, and a nebula where it passes
            const Constellation& k = cons_.back();
            const int d = k.disc;
            const float a0 = rng_.range(0.f, TAU);
            Comet cm;
            cm.disc = d;
            cm.a = V(0.98f * std::cos(a0), 0.98f * std::sin(a0));
            cm.b = V(-0.98f * std::cos(a0 + 0.5f), -0.98f * std::sin(a0 + 0.5f));
            cm.ctl = mul(add(cm.a, cm.b), 0.5f);
            cm.ctl = add(cm.ctl, mul(V(-(cm.b.y - cm.a.y), cm.b.x - cm.a.x), 0.25f));
            cm.t = 0.f;
            cm.dur = 3.2f;
            comets_.push_back(cm);
            Nebula nb;
            nb.disc = d;
            nb.p = lerp2(lerp2(cm.a, cm.ctl, 0.55f), lerp2(cm.ctl, cm.b, 0.55f), 0.55f);
            nb.p = lerp2(nb.p, k.c, 0.35f);
            nb.r = rng_.range(0.26f, 0.34f);
            nb.ang = rng_.range(0.f, PI);
            nb.born = -1.2f;
            nebulae_.push_back(nb);
            if ((int)nebulae_.size() > MAX_NEBULAE) nebulae_.erase(nebulae_.begin());
            tetrisGlow_ = 1.f;
        }
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
        // keep the stack: its enamel drains from the top down (see buildBoard)
        overCells_.clear();
        bool full[Game::H][Game::W] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) full[c.y][c.x] = true;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.x < 0 || c.x >= Game::W || c.y >= Game::H) continue;
            const int id = ids_[c.y][c.x];
            int m = 0;
            const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
            for (int i = 0; i < 8; i++) {
                const int nx = c.x + dx[i], ny = c.y + dy[i];
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H || !full[ny][nx]) continue;
                if (id && ids_[ny][nx] == id) m |= 1 << i;
            }
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            overCells_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, (float)c.y});
        }
        break;
    }
    default: break;
    }
    (void)game;
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void StarChartUniverse::cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase,
                             float shine) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, shine, 0.f}});
}

void StarChartUniverse::buildBoard(const Game& g) {
    auto tintOf = [&](int t) { return pal_.piece[std::clamp(t, 0, 6)]; };
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
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
            cell(p.x, p.y, 0.5f, maskOf(x, y, same), tintOf(c.type), 1.f, 0, 0.f, 0.f);
        }
    // game over: the stack's enamel drains from the top down, then the plaques fade to their faint gold rims
    if (overT_ >= 0.f)
        for (const Fading& o : overCells_) {
            const float rowFrac = (o.t - Game::HIDDEN) / (float)(Game::H - Game::HIDDEN);
            const float k = smoothstepf(rowFrac * 0.6f, rowFrac * 0.6f + 0.3f, over_);
            cell(o.x, o.y, 0.5f, o.mask, tintOf(o.type), 1.f - 0.45f * smoothstepf(0.7f, 1.f, over_), 0, 0.f, 0.f);
            cells_.back().c[3] = k;
        }
    // rows flaking away (real time)
    for (const Fading& f : fading_) cell(f.x, f.y, 0.5f, f.mask, tintOf(f.type), 1.f, 0, saturate(f.t / CLEAR_T), 0.f);
    // the ghost (the rim alone) and the falling piece
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
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
                cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), tintOf(p.type), 0.42f, 1, 0.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), tintOf(p.type), 1.f, 0, 0.f, 0.f);
        }
    }
    // previews
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
            cell(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), tintOf(type), alpha, 0, 0.f, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void StarChartUniverse::sprite(vec2 p, float size, const vec3& c, float alpha, float rays, float glow, float spin) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, spin}, {c.x, c.y, c.z, alpha}, {rays, glow * pal_.glowA, pal_.starStyle, 0.f}});
}

// =====================================================================================================
// Text
// =====================================================================================================

float StarChartUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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
            w += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
    return w - track;
}

void StarChartUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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
            pen += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
}

void StarChartUniverse::arcQuads(const Font& f, const std::string& s, vec2 c, float r, float a, float size, float track,
                                 bool under, std::vector<float>& out) const {
    if (!f.ok || r <= 1.f) return;
    const float k = size / f.px;
    const float baseline = f.capH * k;
    const float W = textWidth(f, s, size, track);
    float pen = 0.f;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        const float mid = pen + g.adv * k * 0.5f;
        const float th = under ? a - (W * 0.5f - mid) / r : a + (W * 0.5f - mid) / r;
        // pixels: the point on the circle, the glyph's right and up directions
        const vec2 P = V(c.x + r * std::cos(th), c.y - r * std::sin(th));
        const vec2 right = under ? V(-std::sin(th), -std::cos(th)) : V(std::sin(th), std::cos(th));
        const vec2 up = under ? V(-std::cos(th), std::sin(th)) : V(std::cos(th), -std::sin(th));
        if (g.w > 0) {
            auto place = [&](float u, float v, float tu, float tv) {
                const float du = u - mid, dv = baseline - v;
                out.push_back(P.x + right.x * du + up.x * dv);
                out.push_back(P.y + right.y * du + up.y * dv);
                out.push_back(tu); out.push_back(tv);
            };
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            place(x0, y0, g.x0, g.y0); place(x1, y0, g.x1, g.y0); place(x1, y1, g.x1, g.y1);
            place(x0, y0, g.x0, g.y0); place(x1, y1, g.x1, g.y1); place(x0, y1, g.x0, g.y1);
        }
        pen += g.adv * k + track;
    }
}

void StarChartUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float halo) {
    if (q.empty() || !f.ok || alpha <= 0.004f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    set3f(progText_, "uInk", lerp(ink, bridge_.ink, b));
    set3f(progText_, "uHalo", lerp(pal_.sky, bridge_.color, b));
    set1f(progText_, "uHaloA", halo);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The plate's lettering: the hemispheres' titles along their rims, the hours, the cartouche, the constellations.
void StarChartUniverse::drawLabels(float world) {
    std::vector<float> q;
    for (const Label& l : labels_) {
        if (l.alpha <= 0.004f || l.text.empty()) continue;
        q.clear();
        const Font& f = l.italic ? ital_ : caps_;
        const vec2 p = project(vec3(l.pos.x, l.pos.y, 0.f));
        const float track = l.italic ? l.size * 0.02f : l.size * 0.18f;
        const float w = textWidth(f, l.text, l.size, track);
        const float capH = f.capH * l.size / f.px;
        textQuads(f, l.text, p.x - w * 0.5f, p.y - capH * 0.5f, l.size, track, q);
        drawText(f, q, roleColor(l.role), l.alpha * world, l.italic ? 0.8f : 0.f);
    }
    // along the rims
    for (int d = 0; d < 2; d++) {
        const Disc& D = disc_[d];
        const vec2 c = project(vec3(D.c.x, D.c.y, 0.f));
        const float s = pxPerUnit_;
        q.clear();
        const float tsz = std::min(0.36f, D.R * 0.085f) * s;
        arcQuads(caps_, d == 0 ? "HEMISPHAERIVM BOREALE" : "HEMISPHAERIVM AVSTRALE", c, (D.R + 0.72f) * s, PI * 0.5f, tsz,
                 tsz * 0.32f, false, q);
        drawText(caps_, q, pal_.line, 0.9f * world);
        // the hours in the rim's band
        q.clear();
        const float hsz = 0.17f * s;
        const float capH = caps_.capH * hsz / caps_.px;
        for (int i = 0; i < 12; i++) {
            const float a = PI * 0.5f - TAU * i / 12.f;
            const bool under = std::sin(a) < -0.2f;
            arcQuads(caps_, ROMAN[i], c, (D.R + 0.37f) * s - (under ? -capH * 0.5f : capH * 0.5f), a, hsz, hsz * 0.1f, under, q);
        }
        drawText(caps_, q, pal_.line, 0.85f * world);
    }
}

// The HUD in engraved lettering: Marcellus capitals, widely spaced, the song in Cormorant italics.
void StarChartUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        std::vector<float> v;
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal_.accent : pal_.sky;
            const float a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{r.x, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y + r.h, c.x, c.y, c.z, a},
                                    {r.x, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y + r.h, c.x, c.y, c.z, a}, {r.x, r.y + r.h, c.x, c.y, c.z, a}};
            for (auto& p : qd) v.insert(v.end(), p, p + 6);
        }
        glUseProgram(progShape_);
        set2f(progShape_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(shapeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6));
    }
    const vec3 ink = pal_.ink, accent = pal_.accent;
    std::vector<float> q;
    const float halo = 0.7f;  // a thin halo of the ground where the lettering crosses an engraved line
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string name = h.text, var;
            const size_t dot = name.find("\xC2\xB7");
            if (dot != std::string::npos) { var = name.substr(dot + 2); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            const float sz = base * 1.05f, vsz = base * 0.95f, tr = sz * 0.24f;
            const float wName = textWidth(caps_, name, sz, tr);
            std::string vt = var;
            for (size_t i = 1; i < vt.size(); i++) vt[i] = (char)std::tolower((unsigned char)vt[i]);
            const float wVar = vt.empty() ? 0.f : textWidth(ital_, vt, vsz * 1.25f, 0.f);
            const float gap = base * 0.7f;
            const float x = ugl::hudLeft(h, wName + (vt.empty() ? 0.f : gap + wVar), width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(caps_, name, x, h.y, sz, tr, q);
            drawText(caps_, q, ink, a, halo);
            if (!vt.empty()) {
                q.clear();
                textQuads(ital_, vt, x + wName + gap, h.y - sz * 0.12f, vsz * 1.25f, 0.f, q);
                drawText(ital_, q, ink, a * 0.8f, halo);
            }
            continue;
        }
        case HUD_SECTION:
            place(caps_, h.text, base * 0.9f, base * 0.5f);
            drawText(caps_, q, ink, saturate(0.2f + h.alpha * 1.9f), halo);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.45f, base * 0.02f, -base * 0.2f);
            drawText(ital_, q, ink, saturate(h.alpha * 1.3f), halo);
            continue;
        case HUD_INFO:
            place(caps_, h.text, base * 0.7f, base * 0.22f);
            drawText(caps_, q, ink, saturate(alpha * 1.3f) * 0.9f, halo);
            continue;
        case HUD_LABEL:
            place(caps_, h.text, base * 0.72f, base * 0.42f);
            drawText(caps_, q, pal_.line, saturate(alpha * 1.2f) * 0.85f, halo);
            continue;
        case HUD_VALUE:
            place(caps_, h.text, base * 1.1f, base * 0.08f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_NOTICE:
            place(ital_, h.text, base * 1.35f, base * 0.02f);
            drawText(ital_, q, accent, alpha, halo);
            continue;
        case HUD_KEY:
            place(caps_, h.text, base * 0.8f, base * 0.1f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_TOAST:
            place(caps_, h.text, base * 1.25f, base * 0.5f, -base * 0.15f);
            drawText(caps_, q, ink, alpha, 0.9f);
            continue;
        case HUD_TOAST_SUB:
            place(ital_, h.text, base * 1.2f, base * 0.02f);
            drawText(ital_, q, accent, alpha * 0.9f, 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(caps_, h.text, base * 1.15f, base * 0.2f, -base * 0.1f);
                drawText(caps_, q, ink, alpha, 0.9f);
            } else {
                place(caps_, h.text, base * 0.8f, base * 0.12f);
                drawText(caps_, q, h.accent ? accent : ink, alpha, 0.9f);
            }
            continue;
        default:
            place(caps_, h.text, base * 0.78f, base * 0.1f);
            drawText(caps_, q, h.accent ? accent : ink, alpha, halo);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void StarChartUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 3.f) : 0.f, overT_ >= 0.f ? 10.f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    time_ += wdt;
    armT_ += f.paused ? 0.f : dt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    // a real drop, not a chorus (both rise into a peak): from the HUD's section, else from the music's pace
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && overT_ < 0.f && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    // eased in over about half a second, out over a few
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 5.f : 0.6f, dt);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        // slow motion: the gold turns to silver light, the ground cools
        if (slow_ > 0.01f) {
            const float k = slow_ * 0.85f;
            pal_.line = lerp(pal_.line, pal_.silver, k);
            pal_.star = lerp(pal_.star, lerp(pal_.silver, vec3(1.f), 0.4f), k);
            pal_.glow = lerp(pal_.glow, pal_.silver, k);
            pal_.rim = lerp(pal_.rim, pal_.silver * 0.85f, k * 0.6f);
            pal_.faint = lerp(pal_.faint, pal_.silver * 0.45f, k);
            if (pal_.light < 0.5f) {
                pal_.sky = lerp(pal_.sky, pal_.sky * vec3(0.7f, 0.8f, 1.f) * 0.75f, slow_);
                pal_.sky2 = lerp(pal_.sky2, pal_.sky2 * vec3(0.7f, 0.8f, 1.f) * 0.75f, slow_);
            }
        }
    }
    swell_ = approach(swell_, 0.f, 0.45f, dt);
    tetrisGlow_ = approach(tetrisGlow_, 0.f, 0.5f, dt);
    detail_ = approach(detail_, wp > 0.5f ? 1.f : 0.f, 0.8f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    // the skies turn on their poles in a drop (eased), and stay where they turned to
    discW_ = approach(discW_, isDrop ? 0.05f : 0.f, isDrop ? 1.5f : 0.7f, dt);
    for (Disc& d : disc_) d.rot += d.dir * discW_ * wdt;

    // ---- The clock: the burin engraves on bars (calm), every two beats (mid), every beat (peak); stars twinkle.
    if (!f.paused) {
        auto twinkle = [&](float frac, float amount) {
            for (Star& s : stars_)
                if (s.born > 0.9f && rng_.chance(frac)) s.tw = std::max(s.tw, amount * rng_.range(0.5f, 1.f));
        };
        auto onBeat = [&](int beat) {
            const bool bar = beat % 4 == 0;
            if (overT_ >= 0.f) return;
            if (level_ >= 2 || isDrop) penStep(1.f);
            else if (level_ == 1 ? beat % 2 == 0 : bar) penStep(1.f);
            if (level_ >= 2) twinkle(isDrop ? 0.2f : 0.12f, 1.f);
            else if (level_ == 1 && beat % 2 == 0) twinkle(0.07f, 0.8f);
        };
        if (music.hasSong) {
            const double b = std::floor(music.beat + 1e-4);
            if (lastBar_ < -0.5 || b < lastBar_ - 8.0 || b > lastBar_ + 8.0) lastBar_ = b;
            while (b > lastBar_) {
                lastBar_ += 1.0;
                onBeat((int)lastBar_ & 0x7fffffff);
            }
        } else {
            beatClock_ += dt;
            if (beatClock_ > 0.5f) {
                beatClock_ -= 0.5f;
                barClock_ += 1.f;
                onBeat((int)barClock_);
            }
        }
        // a kick in a peak flares a bright star or two
        if (kick > 0.6f && wp > 0.5f && rng_.chance(std::min(1.f, dt * 8.f))) {
            for (int tries = 0; tries < 6; tries++) {
                Star& s = stars_[rng_.irange(0, (int)stars_.size() - 1)];
                if (s.mag < 3.f && s.born > 0.9f) { s.tw = 1.5f; break; }
            }
        }
    }
    for (Star& s : stars_) {
        if (s.fade < 1.f) s.fade = std::max(0.f, s.fade - dt * 0.5f);
        s.tw = approach(s.tw, 0.f, 2.6f, dt);
        if (s.born > 0.f && s.born < 1.f) s.born = std::min(1.f, s.born + dt * 2.5f);
    }
    // flights (world time), arriving stars light up and flare
    for (Flight& fl : flights_) {
        if (fl.delay > 0.f) { fl.delay -= wdt; continue; }
        fl.t += wdt / fl.dur;
        if (fl.t >= 1.f && fl.star >= 0) {
            Star& s = stars_[fl.star];
            s.born = std::max(s.born, 0.02f);
            s.tw = 1.6f;
        }
    }
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& x) { return x.t >= 1.f; }), flights_.end());
    for (size_t i = 0; i < cons_.size(); i++) {
        Constellation& k = cons_[i];
        k.age += wdt;
        k.shown = approach(k.shown, k.retired ? 0.f : 1.f, 0.5f, dt);
        if (k.ready < 0.f) {
            bool all = true;
            for (int si : k.stars)
                if (stars_[si].born <= 0.f) all = false;
            if (all) k.ready = 0.f;
        } else k.ready += wdt;
        for (float& e : k.edgeReveal)
            if (e > 0.f && e < 1.f) e = std::min(1.f, e + wdt / lerpf(0.7f, 0.35f, wp));
        const bool done = k.nextEdge >= (int)k.edges.size() && (k.edgeReveal.empty() || k.edgeReveal.back() >= 1.f);
        k.nameA = approach(k.nameA, done ? 1.f : 0.f, 1.2f, dt);
        // the figure traces itself in a drop (one after another), and stays faintly engraved
        if (life > 0.3f && k.ready >= 0.f) {
            const float wait = 0.18f * (float)(i % 8);
            k.figure01 = std::min(1.f, k.figure01 + wdt * (k.figure01 > 0.f || dropAmt_ > 0.95f - wait * 0.2f ? 0.55f : 0.f));
            k.figureInk = std::max(k.figureInk, k.figure01);
        }
    }
    for (Comet& c : comets_) c.t += wdt / c.dur;
    comets_.erase(std::remove_if(comets_.begin(), comets_.end(), [](const Comet& c) { return c.t >= 1.f; }), comets_.end());
    for (Nebula& n : nebulae_) n.born += wdt / 3.f;
    for (Glint& g : glints_) g.t += dt;
    glints_.erase(std::remove_if(glints_.begin(), glints_.end(), [](const Glint& g) { return g.t > 0.5f; }), glints_.end());
    for (auto& fd : fading_) fd.t += dt; // a clear runs in real time, even in slow motion
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The engraving.
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    const float skyA = world * (1.f - 0.45f * over_);
    lines_.clear();
    labels_.clear();
    sprites_.clear();
    std::vector<Stroke> ss;
    buildBorder(ss);
    buildWell(ss);
    for (const Stroke& s : ss) emit(s, 1.f, world);
    ss.clear();
    for (int d = 0; d < 2; d++) buildDisc(d, ss, detail_);
    for (const Stroke& s : ss) emit(s, 1.f, skyA);
    if (ornaments_) {
        ss.clear();
        buildRose(roseC_, roseR_, ss);
        buildRose(rose2C_, rose2R_, ss);
        buildCartouche(cartC_, cartW_, cartH_, ss);
        buildLegend(legendC_, legendW_, legendH_, ss);
        for (const Stroke& s : ss) emit(s, 1.f, world * 0.9f);
        const float sz = std::min(std::min(0.5f, cartH_ * 0.24f), cartW_ * 0.052f) * pxPerUnit_;
        labels_.push_back({"TABVLA STELLARVM", V(cartC_.x, cartC_.y + cartH_ * 0.08f), sz, 0.95f, 0, false});
        labels_.push_back({chartNo_, V(cartC_.x, cartC_.y - cartH_ * 0.22f), sz * 0.55f, 0.8f, 0, false});
        labels_.push_back({"MAGNITVDINES", V(legendC_.x, legendC_.y + legendH_ * 0.32f), legendH_ * 0.16f * pxPerUnit_, 0.85f, 0, false});
        for (int i = 0; i < 6; i++) {
            const float x = legendC_.x + legendW_ * ((i + 0.5f) / 6.f - 0.5f);
            const float sy = legendC_.y - legendH_ * 0.15f;
            const float mag = 1.f + i;
            const float size = (0.34f - 0.05f * mag) * std::min(1.f, legendH_ / 1.6f);
            sprite(V(x, sy), size, pal_.star, world * 0.95f, mag < 2.5f ? 2.f : mag < 4.5f ? 1.f : 0.f, mag < 3.f ? 0.5f : 0.15f, 0.f);
        }
        labels_.push_back({"N", V(roseC_.x, roseC_.y + roseR_ + 0.32f), 0.34f * pxPerUnit_, 0.9f, 0, false});
    }
    // the constellations' lines
    const float twFrame = 0.7f + 0.3f * wm + 0.25f * wp + 0.45f * life + 0.35f * tetrisGlow_;
    for (size_t i = 0; i < cons_.size(); i++) {
        const Constellation& k = cons_[i];
        if (k.shown < 0.005f) continue;
        const vec3 c = roleColor(0);
        const float lineA = skyA * (0.8f + 0.2f * life) * k.shown;
        for (size_t e = 0; e < k.edges.size(); e++) {
            const float rv = k.edgeReveal[e];
            if (rv <= 0.f) continue;
            vec2 a = toWorld(k.disc, stars_[k.stars[k.edges[e].first]].p), b = toWorld(k.disc, stars_[k.stars[k.edges[e].second]].p);
            // the lines stop short of the stars
            const vec2 dd = sub(b, a);
            const float L = len(dd);
            if (L < 0.3f) continue;
            const float gap = 0.13f / L;
            const vec2 a1 = lerp2(a, b, gap), b1 = lerp2(a, b, 1.f - gap);
            segment(a1, lerp2(a1, b1, smoothstepf(0.f, 1.f, rv)), c, lineA, fineW() * 1.3f, 0.6f + 0.6f * life);
        }
        // the figure: traced in a drop, alive while it lasts; a faint engraving after
        const float figA = std::max(k.figureInk * 0.24f, k.figure01 * life) * skyA * k.shown;
        if (figA > 0.01f) {
            std::vector<std::vector<vec2>> fl;
            std::vector<float> fw;
            starchart::figureLines(k.figure, life, time_ + i * 1.7f, fl, fw);
            std::vector<Stroke> fs;
            for (size_t j = 0; j < fl.size(); j++) {
                Stroke s{{}, 0.7f + 0.5f * fw[j], 0.4f + 0.6f * fw[j], 0, 0.3f * life};
                for (const vec2& p : fl[j]) s.pts.push_back(toWorld(k.disc, consPoint(k, p)));
                fs.push_back(s);
            }
            emitAll(fs, k.figure01 > 0.f ? smoothstepf(0.f, 1.f, k.figure01) : 1.f, figA);
        }
        if (k.nameA > 0.01f) {
            // lettered under the figure (its whole extent, whichever way it turned)
            const vec2 c = toWorld(k.disc, k.c);
            const float r = k.scale * disc_[k.disc].R;
            labels_.push_back({k.name, V(c.x, c.y - r * 0.98f - 0.12f), (k.big > 0.5f ? 0.4f : 0.32f) * pxPerUnit_, k.nameA * 0.85f * skyA * k.shown, 0, true});
        }
    }
    // the stars
    for (const Star& s : stars_) {
        if (s.born <= 0.f || s.fade <= 0.f) continue;
        const vec2 p = toWorld(s.disc, s.p);
        const float m = std::clamp(s.mag, 1.f, 6.f);
        const float base = lerpf(0.3f, 0.07f, (m - 1.f) / 5.f);
        const float tw = s.tw + 0.06f * std::sin(time_ * 1.3f + s.phase) * (m < 4.f ? 1.f : 0.f);
        const float flare = 1.f + 0.6f * tw;
        const float rays = m < 2.6f ? 2.f : m < 4.1f ? 1.f : 0.f;
        const float bornK = smoothstepf(0.f, 1.f, s.born);
        const float size = base * (1.f + 0.35f * tw) * (s.born < 1.f ? 1.f + 0.8f * (1.f - s.born) : 1.f) * (1.f + 0.12f * life);
        const float glow = (m < 3.5f ? 0.55f : 0.25f) * flare * twFrame;
        sprite(p, size, pal_.star, std::min(1.f, (m < 5.f ? 1.f : 0.7f) * bornK * (0.75f + 0.25f * flare)) * skyA * s.fade, rays * std::min(1.f, 0.6f + tw), glow, 0.f);
    }
    // flights: a star on its arc, a short fading train behind it
    for (const Flight& fl : flights_) {
        if (fl.delay > 0.f || fl.star < 0) continue;
        const Star& st = stars_[fl.star];
        const vec2 to = toWorld(st.disc, st.p);
        // out through the well's side and under the HUD's column, then up into the hemisphere
        const float yLow = viewC_.y - viewH_.y + 3.2f - 0.6f * fl.bend;
        const vec2 c1 = V(lerpf(fl.from.x, to.x, 0.35f), yLow), c2 = V(lerpf(fl.from.x, to.x, 0.8f), yLow - 0.5f);
        auto at = [&](float t) {
            t = saturate(t);
            const float e = t * t * (3.f - 2.f * t), u = 1.f - e;
            return add(add(mul(fl.from, u * u * u), mul(c1, 3.f * u * u * e)), add(mul(c2, 3.f * u * e * e), mul(to, e * e * e)));
        };
        // faint while it crosses the HUD's lettering
        auto clear = [&](vec2 p) {
            auto in = [&](const float (&r)[4]) {
                return smoothstepf(0.6f, 0.f, std::max(std::max(r[0] - p.x, p.x - r[2]), std::max(r[1] - p.y, p.y - r[3])));
            };
            return 1.f - 0.8f * std::max(in(layout::LEFT_COLUMN), in(layout::RIGHT_COLUMN));
        };
        const vec2 head = at(fl.t);
        for (int k = 1; k <= 6; k++) {
            const vec2 a = at(fl.t - 0.012f * (k - 1)), b = at(fl.t - 0.012f * k);
            segment(a, b, pal_.star, world * 0.5f * (1.f - k / 7.f) * smoothstepf(0.f, 0.08f, fl.t) * clear(a), fineW() * (1.6f - k * 0.18f), 0.8f);
        }
        sprite(head, 0.17f, pal_.star, world * clear(head), 1.f, 0.9f, fl.t * 3.f);
    }
    // comets: the head and a tail of fine diverging lines, engraved
    for (const Comet& c : comets_) {
        auto at = [&](float t) {
            const vec2 p = lerp2(lerp2(c.a, c.ctl, t), lerp2(c.ctl, c.b, t), t);
            return toWorld(c.disc, p);
        };
        const float t = smoothstepf(0.f, 1.f, c.t);
        const float fade = smoothstepf(0.f, 0.12f, c.t) * (1.f - smoothstepf(0.8f, 1.f, c.t));
        const vec2 head = at(t), back = at(std::max(0.f, t - 0.04f));
        vec2 dir = sub(head, back);
        const float dl = len(dir);
        dir = dl > 1e-4f ? mul(dir, 1.f / dl) : V(1, 0);
        const vec2 nrm = V(-dir.y, dir.x);
        for (int i = -3; i <= 3; i++) {
            const float spread = i * 0.06f, L = 2.4f - std::fabs((float)i) * 0.3f;
            Stroke s{{}, 0.8f, 0.85f - std::fabs((float)i) * 0.12f, 4, 0.8f};
            for (int k = 0; k <= 10; k++) {
                const float u = k / 10.f;
                s.pts.push_back(sub(head, add(mul(dir, L * u), mul(nrm, spread * u * L * (1.f + 0.4f * u)))));
            }
            emit(s, 1.f, fade * world);
        }
        sprite(head, 0.38f, pal_.star, fade * world, 2.f, 1.2f, c.t * 2.f);
    }
    // the armillary spheres (slow motion)
    if (slow_ > 0.01f) {
        ss.clear();
        for (int d = 0; d < 2; d++) buildArmillary(disc_[d].c, disc_[d].R * 0.92f, armT_ * (d == 0 ? 1.f : -1.f), ss);
        for (const Stroke& s : ss) emit(s, 1.f, smoothstepf(0.f, 1.f, slow_) * world * 0.75f);
    }
    // the lock's glints
    for (const Glint& g : glints_) {
        const float k = g.t / 0.5f;
        sprite(g.p, (0.22f + 0.2f * k) * g.size, pal_.star, (1.f - k) * (1.f - k) * 0.9f, 1.f, 0.6f, 0.4f);
    }
    // the bonus gauge: an engraved ring with its degrees, the charge a gold arc
    {
        const vec3 gc = layout::GAUGE_CENTER;
        const vec2 c = V(gc.x, gc.y);
        const float R = layout::GAUGE_R;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        ss.clear();
        ss.push_back(Stroke{circlePts(c, R, 90), 0.9f, 0.85f, 0});
        ss.push_back(Stroke{circlePts(c, R * 0.86f, 90), 0.6f, 0.6f, 1});
        for (int i = 0; i < 36; i++) {
            const float a = TAU * i / 36.f;
            ss.push_back(Stroke{{polar(c, R * 0.86f, a), polar(c, R * (i % 3 == 0 ? 0.76f : 0.81f), a)}, 0.6f, 0.6f, 0});
        }
        if (fill > 0.002f) ss.push_back(Stroke{arcPts(c, R * 0.93f, PI * 0.5f, PI * 0.5f - TAU * fill, std::max(4, (int)(90 * fill))),
                                                3.f, on ? 1.f : 0.75f, on ? 2 : 0, on ? 1.f : 0.4f});
        for (const Stroke& s : ss) emit(s, 1.f, 1.f);
    }

    cells_.clear();
    buildBoard(game);

    // ---- Lines into their layer (MAX: joints and crossings never double).
    lineT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!lines_.empty()) {
        if (bridge > 0.001f)
            for (LineInst& li : lines_) {
                const vec3 c = lerp(vec3(li.b[0], li.b[1], li.b[2]), bridge_.ink, smoothstepf(0.2f, 0.9f, bridge));
                li.b[0] = c.x; li.b[1] = c.y; li.b[2] = c.z;
            }
        glEnable(GL_BLEND);
        glBlendEquation(GL_MAX);
        glUseProgram(progLine_);
        setMat(progLine_, "uVP", vp_);
        set1f(progLine_, "uPxPerUnit", pxPerUnit_);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, lines_.size() * sizeof(LineInst), lines_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)lines_.size());
        glBlendEquation(GL_FUNC_ADD);
        glDisable(GL_BLEND);
    }

    // ---- The sheet and its skies.
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progSky_);
    set3f(progSky_, "uSky", pal_.sky);
    set3f(progSky_, "uSky2", pal_.sky2);
    set3f(progSky_, "uWell", pal_.well);
    set3f(progSky_, "uGlow", pal_.glow * (1.f + 0.5f * life + 0.4f * tetrisGlow_));
    set3f(progSky_, "uInk", pal_.faint);
    set4f(progSky_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set4f(progSky_, "uDisc0", disc_[0].c.x, disc_[0].c.y, disc_[0].R, disc_[0].rot);
    set4f(progSky_, "uDisc1", disc_[1].c.x, disc_[1].c.y, disc_[1].R, disc_[1].rot);
    set1f(progSky_, "uLight", pal_.light);
    set1f(progSky_, "uSeed", (float)(seed_ % 89));
    set1f(progSky_, "uMilky", (0.45f + 0.4f * wm + 0.15f * wp) * skyA);
    set1f(progSky_, "uPx", pxPerUnit_);
    set1f(progSky_, "uSwell", swell_ * 0.5f + tetrisGlow_ * 0.5f);
    {
        float nb[MAX_NEBULAE * 4] = {}, na[MAX_NEBULAE * 2] = {};
        int n = 0;
        for (const Nebula& q : nebulae_) {
            if (n >= MAX_NEBULAE) break;
            const vec2 p = toWorld(q.disc, q.p);
            nb[n * 4 + 0] = p.x; nb[n * 4 + 1] = p.y;
            nb[n * 4 + 2] = q.r * disc_[q.disc].R;
            nb[n * 4 + 3] = smoothstepf(0.f, 1.f, q.born) * skyA;
            na[n * 2 + 0] = q.ang + disc_[q.disc].rot;
            na[n * 2 + 1] = (float)(n * 7 % 13);
            n++;
        }
        glUniform4fv(ugl::U(progSky_, "uNeb"), MAX_NEBULAE, nb);
        glUniform2fv(ugl::U(progSky_, "uNebAng"), MAX_NEBULAE, na);
        set1i(progSky_, "uNebN", n);
    }
    set1f(progSky_, "uBridge", bridge);
    set3f(progSky_, "uBridgeCol", bridge_.color);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progLayer_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lineT_.tex);
    set1i(progLayer_, "uLayer", 0);
    ugl::drawFullscreen();

    // ---- Stars.
    if (!sprites_.empty()) {
        if (bridge > 0.001f)
            for (StarInst& si : sprites_) {
                const vec3 c = lerp(vec3(si.b[0], si.b[1], si.b[2]), bridge_.ink, smoothstepf(0.2f, 0.9f, bridge));
                si.b[0] = c.x; si.b[1] = c.y; si.b[2] = c.z;
                si.b[3] *= world;
            }
        glUseProgram(progStar_);
        setMat(progStar_, "uVP", vp_);
        set1f(progStar_, "uPxPerUnit", pxPerUnit_);
        set3f(progStar_, "uGlowCol", pal_.glow);
        glBindVertexArray(starVao_);
        glBindBuffer(GL_ARRAY_BUFFER, starVbo_);
        glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(StarInst), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }

    // ---- Pieces, previews.
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uRimW", std::max(1.6f, 3.f * (height_ / 1080.f)));
        set1f(progCell_, "uLight", pal_.light);
        set1f(progCell_, "uEnamel", pal_.enamel);
        set1f(progCell_, "uTime", time_);
        set1f(progCell_, "uPxPerUnit", pxPerUnit_);
        set3f(progCell_, "uRim", pal_.rim);
        set3f(progCell_, "uRimHi", pal_.light > 0.5f ? pal_.rim : lerp(pal_.rim, vec3(1.f, 0.95f, 0.8f), 0.55f));
        set3f(progCell_, "uInk", pal_.ink);
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    // ---- Lettering and the HUD.
    drawLabels(world);
    drawHud(f.hud, f.panels);
    glDisable(GL_BLEND);

    // ---- Post.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progPost_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progPost_, "uScene", 0);
    set1f(progPost_, "uFade", saturate(f.fade));
    set1f(progPost_, "uDim", dimT_);
    ugl::drawFullscreen();
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_starchart() {
    static const UniverseEntry e{
        "STAR CHART", "an antique engraved celestial map: enamelled pieces, cleared rows fly out as stars into new constellations",
        [] { return std::unique_ptr<Universe>(new StarChartUniverse()); },
        // Fits most songs; the quieter, more spacious ones a little better.
        [](const SongLook& l) { return 0.85f + 0.2f * (1.f - l.density) + 0.1f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const ChartDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
