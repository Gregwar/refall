// BLUEPRINT: a technical drawing whose parts are the pieces.
//
// One world: a drafting sheet (a Prussian-blue cyanotype, white drafting film, a whiteprint, a CAD screen) with its
// border, zone marks and title block. The well is a container drawn in section (hatched walls, an open top, center
// line stubs). The pieces are drafted parts: a crisp outline, a light wash of their tint and the section hatching of
// their type (45 degrees, cross-hatch, stipple, horizontal, 135 degrees, vertical, steel), so the seven types read
// apart at a glance; the ghost is a hidden line (dashed outline). Every placed piece is dimensioned (extension lines,
// arrows, its width lettered above it, fading after a few seconds); the stack's overall height is dimensioned beside
// the well and glides when it changes; the well's width is dimensioned below it.
//
// The drafter draws: every line comes in as a pen stroke. The song is the drawing. A break is sparse paper with a
// few construction lines (an earlier machine left as a faint ghost); verse and mid parts draw a machine elevation in
// the margins, a part every four bars (gears, a cam and its follower, a four-bar linkage, a crank and piston...);
// a chorus draws faster and adds center lines, section hatching and the linkage's coupler curve. The DROP brings the
// machine to life: the gears turn at tempo fractions with an eased ratchet every two beats, the piston strokes once
// a bar and fires on the kicks, the cam lifts its follower, the linkage rocks; the print changes (the blueprint turns
// into a white redline print, the film is exposed into a blueprint...) and the pieces' fills with it; every four bars
// a new part is drawn. A cleared row is rubbed out (the lines lift in eraser streaks, nothing is drawn over the row);
// each clear is a revision in the title block (a pen-drawn delta and the next letter), a Tetris a revision cloud.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane (like SUMI).
#include "universes/blueprint/blueprint.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/blueprint/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Share Tech (a technical, DIN-like lettering), SIL Open Font License, see OFL-ShareTech.txt.
const unsigned char FONT[] = {
#include "universes/blueprint/ShareTech-Regular.ttf.inc"
};

namespace shaders = blueprint_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

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

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A sheet (sRGB): paper, its mottle, object lines, construction lines, dimensions, accent, HUD ink, the machine's
// lines; the seven piece tints (I O T S Z J L) and their outline; dark paper, wash, hatching, outline in the tint,
// mottle.
struct SheetDef {
    unsigned paper, paper2, line, cons, dim, accent, ink, mach;
    unsigned piece[7];
    unsigned outline;
    float dark, fillA, hatchA, outlineTint, mottle;
};
struct VariantDef {
    const char* name;
    SheetDef base, drop;
};
const VariantDef VARIANTS[] = {
    // Cyanotype: white lines on Prussian blue; the drop turns it into a white redline print.
    {"CYANOTYPE",
     {0x1c4377, 0x15355f, 0xf3f7fc, 0x86a9d6, 0xdce8f6, 0xffd36e, 0xeef3fa, 0xe3ecf8,
      {0x9fe6f5, 0xfff0a6, 0xe0c2ff, 0xc0f5b0, 0xffb9b0, 0xb3c7ff, 0xffd2a0}, 0xffffff, 1.f, 0.08f, 0.95f, 0.f, 0.7f},
     {0xf3f5f8, 0xe2e8ef, 0x1c4377, 0x9db7d9, 0x1c4377, 0xd9372c, 0x1c4377, 0xd9372c,
      {0x0f8fc0, 0xd99e00, 0x8a4fd0, 0x2e9a4a, 0xd9372c, 0x2f55c8, 0xe8742a}, 0x1c4377, 0.f, 0.24f, 1.f, 0.f, 0.5f}},
    // Vellum: graphite and non-photo blue on white drafting film; the drop exposes it into a blueprint.
    {"VELLUM",
     {0xf6f8f9, 0xe9edf0, 0x2a2e33, 0x7ea9e0, 0x4a5058, 0xd0412f, 0x2a2e33, 0x33383e,
      {0x1b8fb0, 0xc49410, 0x7b4fb8, 0x3f8f45, 0xc8443a, 0x2f4f9f, 0xd07228}, 0x2a2e33, 0.f, 0.2f, 1.f, 0.f, 0.5f},
     {0x1f4f8a, 0x173f70, 0xf5f8fc, 0x8fb2dc, 0xe2ecf8, 0xffb35c, 0xf0f4fa, 0xffc274,
      {0x8ff0ff, 0xfff38a, 0xf0b8ff, 0xb2ff9e, 0xff9f94, 0xa8beff, 0xffc68a}, 0xffffff, 1.f, 0.09f, 1.f, 0.f, 0.6f}},
    // CAD: thin colored lines on a black screen, each piece on its own layer; the drop runs the simulation.
    {"CAD",
     {0x0c0f13, 0x10151b, 0xd9dee4, 0x2e3946, 0x44c4d8, 0xf2d34a, 0xd0d6dc, 0xb8c0c9,
      {0x35d0e0, 0xf0d84a, 0xd65ad8, 0x58d96a, 0xff5b4f, 0x6a86ff, 0xff9f3d}, 0xd9dee4, 1.f, 0.1f, 0.7f, 1.f, 0.15f},
     {0x0c0f13, 0x15110c, 0xf0f2f4, 0x4a3b22, 0xffb02e, 0xff4f7a, 0xf0f2f4, 0xffb02e,
      {0xffe14d, 0xff7a3d, 0xff4f9a, 0xc6ff4d, 0xff3d3d, 0x4dd9ff, 0xffa64d}, 0xf0f2f4, 1.f, 0.26f, 0.9f, 1.f, 0.15f}},
    // Diazo: a whiteprint, blue-violet lines on white; the drop turns it to a deep violet print with warm lines.
    {"DIAZO",
     {0xf7f7fb, 0xe9e8f3, 0x2e2b86, 0xa29fd8, 0x4f4c9e, 0xc2338a, 0x2a2780, 0x34318e,
      {0x2e6fd0, 0xb88a10, 0x8a3fc0, 0x2f8f6a, 0xc23a5a, 0x37348f, 0xd0602a}, 0x2c2a80, 0.f, 0.2f, 1.f, 0.f, 0.5f},
     {0x262470, 0x1c1a5a, 0xf3f2ff, 0x8f8cd8, 0xe0defa, 0xffa05a, 0xf0effc, 0xffa05a,
      {0x9fd0ff, 0xffe39a, 0xe2b8ff, 0xa8f0cf, 0xffb0c4, 0xc8c6ff, 0xffc49a}, 0xffffff, 1.f, 0.09f, 1.f, 0.f, 0.6f}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = BlueprintUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

Palette paletteOf(const SheetDef& d) {
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.paper = L(d.paper); p.paper2 = L(d.paper2); p.line = L(d.line); p.cons = L(d.cons); p.dim = L(d.dim);
    p.accent = L(d.accent); p.ink = L(d.ink); p.mach = L(d.mach);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.outline = L(d.outline);
    p.dark = d.dark; p.fillA = d.fillA; p.hatchA = d.hatchA; p.outlineTint = d.outlineTint; p.mottle = d.mottle;
    return p;
}
int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette basePalette(int v) { return paletteOf(VARIANTS[wrapV(v)].base); }
Palette dropPalette(int v) { return paletteOf(VARIANTS[wrapV(v)].drop); }

void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

std::string titleCase(const std::string& s) {
    std::string o = s;
    bool start = true;
    for (char& c : o) {
        if (std::isalpha((unsigned char)c)) {
            c = start ? (char)std::toupper((unsigned char)c) : (char)std::tolower((unsigned char)c);
            start = false;
        } else start = c == ' ' || c == '-';
    }
    return o;
}

std::string revLetter(int n) {
    // A, B, ... Z, AA, AB ... (I and O are skipped, as on drawings)
    static const char* L = "ABCDEFGHJKLMNPQRSTUVWXYZ";
    const int k = 24;
    std::string s;
    n = std::max(0, n);
    do {
        s.insert(s.begin(), L[n % k]);
        n = n / k - 1;
    } while (n >= 0);
    return s;
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
std::vector<vec2> rectPts(float x0, float y0, float x1, float y1) {
    return {V(x0, y0), V(x1, y0), V(x1, y1), V(x0, y1), V(x0, y0)};
}

// The machine's parts, in the order they are drawn.
enum PartKind { K_GEAR_A, K_CAM, K_GEAR_B, K_LINKAGE, K_PISTON, K_GEAR_C, K_YOKE, K_SPRING, K_COUPLER, K_COUNT };
constexpr int TEETH_A = 24, TEETH_B = 12, TEETH_C = 12;

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool BlueprintUniverse::init(int width, int height) {
    progPaper_ = ugl::program(ugl::FULLSCREEN_VS, shaders::PAPER_FS, "blueprint paper");
    progLine_ = ugl::program(shaders::LINE_VS, shaders::LINE_FS, "blueprint line");
    progLayer_ = ugl::program(ugl::FULLSCREEN_VS, shaders::LAYER_FS, "blueprint layer");
    progCell_ = ugl::program(shaders::CELL_VS, shaders::CELL_FS, "blueprint cell");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "blueprint shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "blueprint text");
    progPost_ = ugl::program(ugl::FULLSCREEN_VS, shaders::POST_FS, "blueprint post");

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
    instanced(cellVao_, cellVbo_, 4, sizeof(CellInst));
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

    buildFont(font_, FONT);
    pal_ = basePal_ = basePalette(0);
    parts_.assign(K_COUNT, Part{});
    for (int i = 0; i < K_COUNT; i++) parts_[i].kind = i;
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void BlueprintUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] blueprint font init failed\n");
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

void BlueprintUniverse::resize(int width, int height) {
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
    layoutMachine();
}

BlueprintUniverse::~BlueprintUniverse() {
    sceneT_.destroy();
    lineT_.destroy();
    for (GLuint p : {progPaper_, progLine_, progLayer_, progCell_, progShape_, progText_, progPost_}) glDeleteProgram(p);
    for (GLuint v : {lineVao_, cellVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, lineVbo_, cellVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    if (font_.tex) glDeleteTextures(1, &font_.tex);
    delete (stbtt_fontinfo*)font_.info;
}

// =====================================================================================================
// Pen strokes
// =====================================================================================================

vec3 BlueprintUniverse::roleColor(Role r) const {
    switch (r) {
    case R_CONS: return pal_.cons;
    case R_DIM: return pal_.dim;
    case R_ACCENT: return pal_.accent;
    case R_MACH: return pal_.mach;
    default: return pal_.line;
    }
}

float BlueprintUniverse::weightPx(Weight w) const {
    const float s = height_ / 1080.f;
    switch (w) {
    case W_THIN: return std::max(0.8f, 1.15f * s);
    case W_THICK: return std::max(1.6f, 2.9f * s);
    default: return std::max(1.1f, 1.8f * s);
    }
}

float BlueprintUniverse::strokeLen(const Stroke& s) const {
    float L = 0.f;
    for (size_t i = 1; i < s.pts.size(); i++) L += len(sub(s.pts[i], s.pts[i - 1]));
    return L;
}

// Emits the stroke's segments up to `reveal` of its length (the pen's progress).
void BlueprintUniverse::emit(const Stroke& s, float reveal, float alpha, float scale) {
    const float a = alpha * s.alpha;
    if (a <= 0.004f || reveal <= 0.f || s.pts.size() < 2) return;
    const float total = strokeLen(s);
    const float stop = total * std::min(1.f, reveal);
    const vec3 c = roleColor(s.role);
    const float w = weightPx(s.w) * scale;
    const float dash = s.pat == P_DOT ? 0.16f : s.pat == P_CENTER ? 0.42f : 0.3f;
    float d = 0.f;
    for (size_t i = 1; i < s.pts.size() && d < stop; i++) {
        vec2 p0 = s.pts[i - 1], p1 = s.pts[i];
        const float L = len(sub(p1, p0));
        if (L < 1e-6f) continue;
        if (d + L > stop) p1 = add(p0, mul(sub(p1, p0), (stop - d) / L));
        LineInst li{{p0.x, p0.y, p1.x, p1.y}, {c.x, c.y, c.z, a}, {w, (float)s.pat, d, dash}};
        lines_.push_back(li);
        d += L;
    }
}

void BlueprintUniverse::emitAll(const std::vector<Stroke>& ss, float reveal, float alpha) {
    if (reveal <= 0.f || alpha <= 0.004f) return;
    if (reveal >= 1.f) {
        for (const Stroke& s : ss) emit(s, 1.f, alpha);
        return;
    }
    float total = 0.f;
    for (const Stroke& s : ss) total += strokeLen(s);
    const float target = total * reveal;
    float acc = 0.f;
    for (const Stroke& s : ss) {
        const float L = strokeLen(s);
        if (acc >= target) break;
        emit(s, L > 0.f ? (target - acc) / L : 1.f, alpha);
        acc += L;
    }
}

// =====================================================================================================
// The machine
// =====================================================================================================

void BlueprintUniverse::layoutMachine() {
    const float hw = viewH_.x;
    const float x0 = 11.9f, x1 = hw - 0.9f;
    const float W = x1 - x0;
    machOn_ = W > 3.5f;
    const float s = machScale_ = std::clamp(W / 7.9f, 0.5f, 1.1f);
    const float xR = (x0 + x1) * 0.5f, xL = -xR;
    rA_ = 2.3f * s;
    rB_ = rA_ * TEETH_B / TEETH_A;
    rC_ = rA_ * TEETH_C / TEETH_A;
    gA_ = V(xR - 0.9f * s, 6.4f);
    gB_ = polar(gA_, rA_ + rB_, -62.f * PI / 180.f);
    gC_ = polar(gA_, rA_ + rC_, 32.f * PI / 180.f);
    cam_ = V(xL + 0.3f * s, 2.6f);
    link_ = V(xL - 2.2f * s, -4.7f);
    yoke_ = V(xR + 0.2f * s, -4.9f);
}

// A spur gear: its pitch circle (a center line), the teeth, the web and its lightening holes, the hub and the bore
// with its keyway; `ang` turns it.
void BlueprintUniverse::gear(std::vector<Stroke>& out, vec2 c, float r, int N, float ang, int holes, bool detail) const {
    const float m = 2.f * r / N, ra = r + m, rd = r - 1.25f * m;
    const float p = TAU / N;
    Stroke pitch{circlePts(c, r, 72), W_THIN, P_CENTER, R_CONS, 0.9f};
    out.push_back(pitch);
    Stroke teeth;
    teeth.w = W_MED;
    teeth.role = R_MACH;
    for (int i = 0; i < N; i++) {
        const float t = ang + i * p;
        teeth.pts.push_back(polar(c, rd, t - 0.29f * p));
        teeth.pts.push_back(polar(c, r, t - 0.23f * p));
        teeth.pts.push_back(polar(c, ra, t - 0.14f * p));
        teeth.pts.push_back(polar(c, ra, t + 0.14f * p));
        teeth.pts.push_back(polar(c, r, t + 0.23f * p));
        teeth.pts.push_back(polar(c, rd, t + 0.29f * p));
        teeth.pts.push_back(polar(c, rd, t + 0.5f * p));
    }
    teeth.pts.push_back(teeth.pts.front());
    out.push_back(teeth);
    const float rim = rd - 1.3f * m, hub = 0.3f * r, bore = 0.13f * r;
    out.push_back(Stroke{circlePts(c, rim, 64, ang), W_THIN, P_SOLID, R_MACH, 0.9f});
    out.push_back(Stroke{circlePts(c, hub, 40, ang), W_MED, P_SOLID, R_MACH, 1.f});
    {
        // the bore and its keyway
        Stroke b;
        b.w = W_MED;
        b.role = R_MACH;
        const float kw = 0.32f * bore, kd = 0.35f * bore;
        const float a0 = std::asin(kw / bore);
        const int n = 28;
        for (int i = 0; i <= n; i++) b.pts.push_back(polar(c, bore, ang + PI / 2 + a0 + (TAU - 2 * a0) * i / n));
        const vec2 u = rot(V(0, 1), ang), v = rot(V(1, 0), ang);
        const vec2 k0 = add(c, add(mul(u, std::sqrt(bore * bore - kw * kw)), mul(v, -kw)));
        b.pts.push_back(add(k0, mul(u, kd)));
        b.pts.push_back(add(add(k0, mul(u, kd)), mul(v, 2 * kw)));
        b.pts.push_back(add(k0, mul(v, 2 * kw)));
        out.push_back(b);
    }
    if (holes > 0 && rim > hub * 1.6f) {
        const float rr = (rim + hub) * 0.5f, hr = (rim - hub) * 0.3f;
        for (int i = 0; i < holes; i++) out.push_back(Stroke{circlePts(polar(c, rr, ang + TAU * i / holes + p * 0.5f), hr, 28), W_THIN, P_SOLID, R_MACH, 1.f});
    }
    // center mark: a small cross, and with the details its center lines
    const float k = 0.22f * machScale_;
    out.push_back(Stroke{{V(c.x - k, c.y), V(c.x + k, c.y)}, W_THIN, P_SOLID, R_MACH, 1.f});
    out.push_back(Stroke{{V(c.x, c.y - k), V(c.x, c.y + k)}, W_THIN, P_SOLID, R_MACH, 1.f});
    if (detail && detail_ > 0.01f) {
        const float e = ra + 0.4f * machScale_;
        out.push_back(Stroke{{V(c.x - e, c.y), V(c.x + e, c.y)}, W_THIN, P_CENTER, R_CONS, detail_});
        out.push_back(Stroke{{V(c.x, c.y - e), V(c.x, c.y + e)}, W_THIN, P_CENTER, R_CONS, detail_});
    }
}

void BlueprintUniverse::buildPart(int kind, std::vector<Stroke>& out) const {
    const float s = machScale_;
    // the drive: A turns with angle_, B and C mesh with it, the cam and the linkage's crank turn once a bar
    const float phA = angle_;
    auto mesh = [&](vec2 cA, vec2 cX, int nX) {
        const float beta = std::atan2(cX.y - cA.y, cX.x - cA.x);
        return beta + PI - PI / nX - ((float)TEETH_A / nX) * (phA - beta);
    };
    const float phB = mesh(gA_, gB_, TEETH_B);
    const float phC = mesh(gA_, gC_, TEETH_C);
    const float phCam = -2.f * angle_ + 0.6f;
    const float phCrank = 2.f * angle_ + 1.3f;
    auto hatchRect = [&](float x0, float y0, float x1, float y1, float alpha) {
        // section hatching (45 degrees) clipped to a rectangle
        const float sp = 0.22f * s;
        for (float k = -(y1 - y0); k < (x1 - x0); k += sp) {
            float ax = x0 + k, ay = y0, bx = x0 + k + (y1 - y0), by = y1;
            if (ax < x0) { ay += x0 - ax; ax = x0; }
            if (bx > x1) { by -= bx - x1; bx = x1; }
            if (ay >= by) continue;
            out.push_back(Stroke{{V(ax, ay), V(bx, by)}, W_THIN, P_SOLID, R_MACH, alpha * 0.8f});
        }
    };
    auto slot = [&](vec2 a, vec2 b, float w, Role role) {
        // a link: two parallel lines with round ends, and its pivots
        const vec2 d = sub(b, a);
        const float L = std::max(1e-4f, len(d));
        const float ang = std::atan2(d.y, d.x);
        Stroke st;
        st.w = W_MED;
        st.role = role;
        auto add2 = [&](const std::vector<vec2>& v) { st.pts.insert(st.pts.end(), v.begin(), v.end()); };
        add2(arcPts(b, w, ang - PI / 2, ang + PI / 2, 10));
        add2(arcPts(a, w, ang + PI / 2, ang + 3 * PI / 2, 10));
        st.pts.push_back(st.pts.front());
        out.push_back(st);
        out.push_back(Stroke{circlePts(a, w * 0.42f, 16), W_THIN, P_SOLID, role, 1.f});
        out.push_back(Stroke{circlePts(b, w * 0.42f, 16), W_THIN, P_SOLID, role, 1.f});
        (void)L;
    };
    switch (kind) {
    case K_GEAR_A: gear(out, gA_, rA_, TEETH_A, phA, 5, true); break;
    case K_GEAR_B: {
        gear(out, gB_, rB_, TEETH_B, phB, 0, true);
        break;
    }
    case K_GEAR_C: gear(out, gC_, rC_, TEETH_C, phC, 0, true); break;
    case K_PISTON: {
        // the crank on B, the rod, the piston in its cylinder (drawn in section)
        const float rc = 0.72f * s, L = 2.9f * s;
        const vec2 pin = polar(gB_, rc, phB);
        const float qy = pin.y - std::sqrt(std::max(0.01f, L * L - (pin.x - gB_.x) * (pin.x - gB_.x)));
        const vec2 q = V(gB_.x, qy);
        const float pw = 0.6f * s, ph0 = -0.62f * s, ph1 = 0.42f * s;
        const float yTop = gB_.y - L + rc + ph1 + 0.18f * s;
        const float yBot = gB_.y - L - rc + ph0 - 0.3f * s;
        const float wi = pw + 0.04f * s, wo = wi + 0.3f * s, head = 0.32f * s;
        // cylinder: inner and outer walls, the head
        out.push_back(Stroke{{V(gB_.x - wi, yTop), V(gB_.x - wi, yBot), V(gB_.x + wi, yBot), V(gB_.x + wi, yTop)}, W_MED, P_SOLID, R_MACH, 1.f});
        out.push_back(Stroke{{V(gB_.x - wo, yTop), V(gB_.x - wo, yBot - head), V(gB_.x + wo, yBot - head), V(gB_.x + wo, yTop)}, W_MED, P_SOLID, R_MACH, 1.f});
        out.push_back(Stroke{{V(gB_.x - wo, yTop), V(gB_.x - wi, yTop)}, W_MED, P_SOLID, R_MACH, 1.f});
        out.push_back(Stroke{{V(gB_.x + wi, yTop), V(gB_.x + wo, yTop)}, W_MED, P_SOLID, R_MACH, 1.f});
        // the cylinder's axis
        out.push_back(Stroke{{V(gB_.x, yBot - head - 0.4f * s), V(gB_.x, gB_.y - rB_ - 0.25f * s)}, W_THIN, P_CENTER, R_CONS, 0.8f});
        // the piston and its rings
        out.push_back(Stroke{rectPts(q.x - pw, q.y + ph0, q.x + pw, q.y + ph1), W_MED, P_SOLID, R_MACH, 1.f});
        for (int i = 1; i <= 2; i++) {
            const float ry = q.y + ph1 - 0.14f * s * i;
            out.push_back(Stroke{{V(q.x - pw, ry), V(q.x + pw, ry)}, W_THIN, P_SOLID, R_MACH, 1.f});
        }
        // the rod and the crank
        slot(q, pin, 0.16f * s, R_MACH);
        out.push_back(Stroke{{gB_, pin}, W_THIN, P_SOLID, R_MACH, 1.f});
        // the walls in section, with the details
        if (detail_ > 0.01f) {
            hatchRect(gB_.x - wo, yBot - head, gB_.x - wi, yTop, detail_);
            hatchRect(gB_.x + wi, yBot - head, gB_.x + wo, yTop, detail_);
            hatchRect(gB_.x - wi, yBot - head, gB_.x + wi, yBot, detail_);
        }
        // firing on the kick: the chamber under the piston lights in the accent
        if (fire_ > 0.02f) {
            const float c0 = yBot + 0.06f * s, c1 = q.y + ph0 - 0.06f * s;
            for (float y = c0; y < c1; y += 0.13f * s) {
                const float k = (y - c0) / std::max(0.01f, c1 - c0);
                out.push_back(Stroke{{V(gB_.x - wi + 0.08f * s, y), V(gB_.x + wi - 0.08f * s, y)}, W_THIN, P_SOLID, R_ACCENT, fire_ * (1.f - 0.6f * k)});
            }
        }
        break;
    }
    case K_CAM:
    case K_SPRING: {
        // a pear cam turning under a flat-faced follower in its guide; the spring keeps the follower down
        const float r0 = 0.95f * s, h = 0.8f * s;
        std::vector<vec2> prof;
        float top = -1e9f;
        const int n = 72;
        for (int i = 0; i <= n; i++) {
            const float t = TAU * i / n;
            const float k = 0.5f - 0.5f * std::cos(t);
            const float r = r0 + h * k * k;
            const vec2 pt = polar(cam_, r, t + phCam);
            prof.push_back(pt);
            top = std::max(top, pt.y);
        }
        const float foot = top, plate = 0.16f * s, sw = 0.15f * s;
        const float guide0 = cam_.y + r0 + h + 1.6f * s, guide1 = guide0 + 0.7f * s;
        const float stemTop = std::min(8.4f, guide1 + 0.9f * s + (foot - cam_.y - r0));
        const float collar = foot + plate + 0.75f * s;
        if (kind == K_CAM) {
            out.push_back(Stroke{prof, W_MED, P_SOLID, R_MACH, 1.f});
            out.push_back(Stroke{circlePts(cam_, 0.3f * s, 24), W_MED, P_SOLID, R_MACH, 1.f});
            out.push_back(Stroke{circlePts(cam_, 0.12f * s, 16), W_THIN, P_SOLID, R_MACH, 1.f});
            out.push_back(Stroke{circlePts(cam_, r0, 64), W_THIN, P_CENTER, R_CONS, 0.8f}); // base circle
            out.push_back(Stroke{rectPts(cam_.x - 0.8f * s, foot, cam_.x + 0.8f * s, foot + plate), W_MED, P_SOLID, R_MACH, 1.f});
            out.push_back(Stroke{{V(cam_.x - sw, foot + plate), V(cam_.x - sw, stemTop), V(cam_.x + sw, stemTop), V(cam_.x + sw, foot + plate)}, W_MED, P_SOLID, R_MACH, 1.f});
            out.push_back(Stroke{rectPts(cam_.x - 0.42f * s, collar, cam_.x + 0.42f * s, collar + 0.14f * s), W_MED, P_SOLID, R_MACH, 1.f});
            for (int side = -1; side <= 1; side += 2) {
                const float gx0 = cam_.x + side * (sw + 0.03f * s), gx1 = cam_.x + side * (sw + 0.75f * s);
                out.push_back(Stroke{rectPts(std::min(gx0, gx1), guide0, std::max(gx0, gx1), guide1), W_MED, P_SOLID, R_MACH, 1.f});
                if (detail_ > 0.01f) hatchRect(std::min(gx0, gx1), guide0, std::max(gx0, gx1), guide1, detail_);
            }
            const float e = r0 + h + 0.45f * s;
            out.push_back(Stroke{{V(cam_.x - e, cam_.y), V(cam_.x + e, cam_.y)}, W_THIN, P_CENTER, R_CONS, 0.5f + 0.5f * detail_});
            out.push_back(Stroke{{V(cam_.x, cam_.y - e), V(cam_.x, stemTop + 0.3f * s)}, W_THIN, P_CENTER, R_CONS, 0.5f + 0.5f * detail_});
        } else {
            Stroke sp;
            sp.w = W_THIN;
            sp.role = R_MACH;
            const float y0 = collar + 0.14f * s, y1 = guide0;
            const int coils = 9;
            sp.pts.push_back(V(cam_.x - 0.36f * s, y0));
            for (int i = 0; i < coils; i++) {
                const float y = y0 + (y1 - y0) * (i + 0.5f) / coils;
                sp.pts.push_back(V(cam_.x + (i % 2 ? -1.f : 1.f) * 0.36f * s, y));
            }
            sp.pts.push_back(V(cam_.x + (coils % 2 ? -1.f : 1.f) * 0.36f * s, y1));
            out.push_back(sp);
        }
        break;
    }
    case K_LINKAGE:
    case K_COUPLER: {
        // a crank-rocker four-bar linkage on two grounded pivots, its coupler a triangular plate
        const float g = 4.2f * s, a = 0.95f * s, b = 3.9f * s, c = 2.7f * s;
        const vec2 O2 = link_, O4 = add(link_, V(g, 0.f));
        auto solve = [&](float th, vec2& A, vec2& B, vec2& P) {
            A = polar(O2, a, th);
            const vec2 d = sub(O4, A);
            const float D = std::max(1e-4f, len(d));
            const float x = (b * b - c * c + D * D) / (2 * D);
            const float y = std::sqrt(std::max(0.f, b * b - x * x));
            const vec2 u = mul(d, 1.f / D), nrm = V(-u.y, u.x);
            B = add(A, add(mul(u, x), mul(nrm, y)));
            const vec2 ab = sub(B, A);
            const vec2 nn = mul(V(-ab.y, ab.x), 1.f / std::max(1e-4f, len(ab)));
            P = add(add(A, mul(ab, 0.45f)), mul(nn, 1.05f * s));
        };
        if (kind == K_COUPLER) {
            Stroke tr;
            tr.w = W_THIN;
            tr.pat = P_DOT;
            tr.role = R_CONS;
            for (int i = 0; i <= 96; i++) {
                vec2 A, B, P;
                solve(TAU * i / 96, A, B, P);
                tr.pts.push_back(P);
            }
            out.push_back(tr);
            break;
        }
        vec2 A, B, P;
        solve(phCrank, A, B, P);
        // ground pivots
        for (const vec2& O : {O2, O4}) {
            const float k = 0.36f * s, hgt = 0.48f * s;
            out.push_back(Stroke{{O, V(O.x - k, O.y - hgt), V(O.x + k, O.y - hgt), O}, W_MED, P_SOLID, R_MACH, 1.f});
            out.push_back(Stroke{{V(O.x - k * 1.6f, O.y - hgt), V(O.x + k * 1.6f, O.y - hgt)}, W_MED, P_SOLID, R_MACH, 1.f});
            for (int i = 0; i < 6; i++) {
                const float x = O.x - k * 1.6f + (i + 0.5f) * k * 3.2f / 6.f;
                out.push_back(Stroke{{V(x, O.y - hgt), V(x - 0.18f * s, O.y - hgt - 0.18f * s)}, W_THIN, P_SOLID, R_MACH, 1.f});
            }
            out.push_back(Stroke{circlePts(O, 0.11f * s, 16), W_MED, P_SOLID, R_MACH, 1.f});
        }
        slot(O2, A, 0.17f * s, R_MACH);
        slot(O4, B, 0.17f * s, R_MACH);
        // the coupler plate A-B-P
        {
            Stroke pl;
            pl.w = W_MED;
            pl.role = R_MACH;
            pl.pts = {A, P, B, A};
            out.push_back(pl);
            out.push_back(Stroke{circlePts(P, 0.1f * s, 14), W_THIN, P_SOLID, R_MACH, 1.f});
        }
        // the frame line between the pivots
        out.push_back(Stroke{{O2, O4}, W_THIN, P_CENTER, R_CONS, 0.7f});
        break;
    }
    case K_YOKE: {
        // a Scotch yoke: a crank pin in the slot of a yoke that slides between two guides
        const float r = 0.95f * s;
        const float th = -2.f * angle_ + 2.1f;
        const vec2 pin = polar(yoke_, r, th);
        out.push_back(Stroke{circlePts(yoke_, r + 0.32f * s, 56), W_MED, P_SOLID, R_MACH, 1.f});      // the crank disc
        out.push_back(Stroke{circlePts(yoke_, r, 56), W_THIN, P_CENTER, R_CONS, 0.8f});               // the pin's path
        out.push_back(Stroke{circlePts(yoke_, 0.22f * s, 20), W_MED, P_SOLID, R_MACH, 1.f});
        out.push_back(Stroke{circlePts(pin, 0.13f * s, 16), W_MED, P_SOLID, R_MACH, 1.f});
        const float sw = 0.16f * s, sh = r + 0.5f * s, fw = 0.2f * s;
        // the yoke: a frame around the slot, its rods to both sides
        out.push_back(Stroke{rectPts(pin.x - sw, yoke_.y - sh, pin.x + sw, yoke_.y + sh), W_MED, P_SOLID, R_MACH, 1.f});
        out.push_back(Stroke{rectPts(pin.x - sw - fw, yoke_.y - sh - fw, pin.x + sw + fw, yoke_.y + sh + fw), W_MED, P_SOLID, R_MACH, 1.f});
        const float g0 = yoke_.x - 3.1f * s, g1 = yoke_.x + 3.1f * s, rw = 0.1f * s;
        out.push_back(Stroke{{V(g0 - 0.5f * s, yoke_.y + rw), V(pin.x - sw - fw, yoke_.y + rw)}, W_MED, P_SOLID, R_MACH, 1.f});
        out.push_back(Stroke{{V(g0 - 0.5f * s, yoke_.y - rw), V(pin.x - sw - fw, yoke_.y - rw)}, W_MED, P_SOLID, R_MACH, 1.f});
        out.push_back(Stroke{{V(pin.x + sw + fw, yoke_.y + rw), V(g1 + 0.5f * s, yoke_.y + rw)}, W_MED, P_SOLID, R_MACH, 1.f});
        out.push_back(Stroke{{V(pin.x + sw + fw, yoke_.y - rw), V(g1 + 0.5f * s, yoke_.y - rw)}, W_MED, P_SOLID, R_MACH, 1.f});
        for (float gx : {g0, g1}) {
            const float bw = 0.3f * s, bh = 0.32f * s;
            out.push_back(Stroke{rectPts(gx - bw, yoke_.y + rw + 0.02f * s, gx + bw, yoke_.y + bh), W_MED, P_SOLID, R_MACH, 1.f});
            out.push_back(Stroke{rectPts(gx - bw, yoke_.y - bh, gx + bw, yoke_.y - rw - 0.02f * s), W_MED, P_SOLID, R_MACH, 1.f});
            if (detail_ > 0.01f) {
                hatchRect(gx - bw, yoke_.y + rw + 0.02f * s, gx + bw, yoke_.y + bh, detail_);
                hatchRect(gx - bw, yoke_.y - bh, gx + bw, yoke_.y - rw - 0.02f * s, detail_);
            }
        }
        out.push_back(Stroke{{V(g0 - 0.8f * s, yoke_.y), V(g1 + 0.8f * s, yoke_.y)}, W_THIN, P_CENTER, R_CONS, 0.6f + 0.4f * detail_});
        break;
    }
    default: break;
    }
}

void BlueprintUniverse::addPart(float speed, float delay) {
    if (partsWanted_ >= K_COUNT) return;
    Part& p = parts_[partsWanted_++];
    p.started = true;
    p.reveal = 0.f;
    p.ink = 1.f;
    p.speed = speed;
    p.delay = delay;
}

// =====================================================================================================
// Dimensions and the sheet
// =====================================================================================================

void BlueprintUniverse::arrow(vec2 tip, vec2 dir, Role role, float alpha, std::vector<Stroke>& out) const {
    // a closed, narrow arrowhead (drawn as two strokes and a short base)
    const float L = 0.32f, W = 0.09f;
    const vec2 n = V(-dir.y, dir.x);
    const vec2 b = sub(tip, mul(dir, L));
    Stroke s;
    s.w = W_THIN;
    s.role = role;
    s.alpha = alpha;
    s.pts = {add(b, mul(n, W)), tip, sub(b, mul(n, W)), add(b, mul(n, W))};
    out.push_back(s);
    // fill it with two inner strokes
    s.pts = {tip, b};
    out.push_back(s);
    s.pts = {tip, add(b, mul(n, W * 0.5f))};
    out.push_back(s);
    s.pts = {tip, sub(b, mul(n, W * 0.5f))};
    out.push_back(s);
}

// A horizontal dimension from x0 to x1 at height y, its extension lines rising from yRef (the part's edge).
void BlueprintUniverse::dimH(float x0, float x1, float y, float yRef0, float yRef1, float reveal, float alpha, Role role,
                             std::vector<Stroke>& out) const {
    if (reveal <= 0.f || alpha <= 0.004f) return;
    const float gap = 0.12f, over = 0.2f;
    const float sg = y > yRef0 ? 1.f : -1.f;
    const float e = saturate(reveal / 0.45f);
    out.push_back(Stroke{{V(x0, yRef0 + gap * sg), V(x0, yRef0 + gap * sg + (y + over * sg - yRef0 - gap * sg) * e)}, W_THIN, P_SOLID, role, alpha});
    out.push_back(Stroke{{V(x1, yRef1 + gap * sg), V(x1, yRef1 + gap * sg + (y + over * sg - yRef1 - gap * sg) * e)}, W_THIN, P_SOLID, role, alpha});
    const float d = saturate((reveal - 0.3f) / 0.5f);
    if (d > 0.f) {
        const float xm = (x0 + x1) * 0.5f, hl = (x1 - x0) * 0.5f * d;
        out.push_back(Stroke{{V(xm, y), V(xm - hl, y)}, W_THIN, P_SOLID, role, alpha});
        out.push_back(Stroke{{V(xm, y), V(xm + hl, y)}, W_THIN, P_SOLID, role, alpha});
    }
    if (reveal >= 0.8f) {
        arrow(V(x0, y), V(-1, 0), role, alpha, out);
        arrow(V(x1, y), V(1, 0), role, alpha, out);
    }
}

// A vertical dimension from y0 to y1 at x, its extension lines from xRef.
void BlueprintUniverse::dimV(float y0, float y1, float x, float xRef, float reveal, float alpha, Role role,
                             std::vector<Stroke>& out) const {
    if (reveal <= 0.f || alpha <= 0.004f) return;
    const float gap = 0.1f, over = 0.2f;
    out.push_back(Stroke{{V(xRef + gap, y0), V(x + over, y0)}, W_THIN, P_SOLID, role, alpha});
    out.push_back(Stroke{{V(xRef + gap, y1), V(x + over, y1)}, W_THIN, P_SOLID, role, alpha});
    out.push_back(Stroke{{V(x, y0), V(x, y1)}, W_THIN, P_SOLID, role, alpha});
    if (y1 - y0 > 0.7f) {
        arrow(V(x, y0), V(0, -1), role, alpha, out);
        arrow(V(x, y1), V(0, 1), role, alpha, out);
    }
}

void BlueprintUniverse::buildSheet(std::vector<Stroke>& out, float lvA) {
    const float hw = viewH_.x, hh = viewH_.y, cx = viewC_.x, cy = viewC_.y;
    const float b = 0.3f;
    const float X0 = cx - hw + b, X1 = cx + hw - b, Y0 = cy - hh + b, Y1 = cy + hh - b;
    // the border and its zone ticks
    out.push_back(Stroke{rectPts(X0, Y0, X1, Y1), W_MED, P_SOLID, R_LINE, 0.9f});
    {
        const int nx = 8, ny = 4;
        for (int i = 1; i < nx; i++) {
            const float x = X0 + (X1 - X0) * i / nx;
            out.push_back(Stroke{{V(x, Y1), V(x, Y1 - 0.28f)}, W_THIN, P_SOLID, R_LINE, 0.8f});
            out.push_back(Stroke{{V(x, Y0), V(x, Y0 + 0.28f)}, W_THIN, P_SOLID, R_LINE, 0.8f});
        }
        for (int i = 1; i < ny; i++) {
            const float y = Y0 + (Y1 - Y0) * i / ny;
            out.push_back(Stroke{{V(X0, y), V(X0 + 0.28f, y)}, W_THIN, P_SOLID, R_LINE, 0.8f});
            out.push_back(Stroke{{V(X1, y), V(X1 - 0.28f, y)}, W_THIN, P_SOLID, R_LINE, 0.8f});
        }
        const float ts = 0.34f * pxPerUnit_;
        for (int i = 0; i < nx; i++) {
            const float x = X0 + (X1 - X0) * (i + 0.5f) / nx;
            if (std::fabs(x) > 5.6f && x < -11.f) continue; // keep clear of the song's title
            labels_.push_back({std::to_string(nx - i), V(x, Y1 - 0.13f), ts, 0.55f, R_LINE, false, true});
        }
        for (int i = 0; i < ny; i++) {
            const float y = Y0 + (Y1 - Y0) * (i + 0.5f) / ny;
            labels_.push_back({std::string(1, (char)('A' + i)), V(X1 - 0.17f, y + 0.17f), ts, 0.55f, R_LINE, false, true});
        }
    }
    // the well, drawn in section: inner and outer walls, the floor, hatched between; center line stubs
    {
        const float t = 0.3f, top = 10.25f;
        out.push_back(Stroke{{V(-5.f, top), V(-5.f, -10.f), V(5.f, -10.f), V(5.f, top)}, W_THICK, P_SOLID, R_LINE, 1.f});
        out.push_back(Stroke{{V(-5.f - t, top), V(-5.f - t, -10.f - t), V(5.f + t, -10.f - t), V(5.f + t, top)}, W_MED, P_SOLID, R_LINE, 0.9f});
        out.push_back(Stroke{{V(-5.f - t, top), V(-5.f, top)}, W_MED, P_SOLID, R_LINE, 0.9f});
        out.push_back(Stroke{{V(5.f, top), V(5.f + t, top)}, W_MED, P_SOLID, R_LINE, 0.9f});
        const float sp = 0.26f;
        // walls: 45 degree hatching between the lines
        for (int side = -1; side <= 1; side += 2) {
            const float xa = side < 0 ? -5.f - t : 5.f, xb = side < 0 ? -5.f : 5.f + t;
            for (float y = -10.f - t; y < top - t; y += sp)
                out.push_back(Stroke{{V(xa, y), V(xb, y + t)}, W_THIN, P_SOLID, R_LINE, 0.55f});
        }
        for (float x = -5.f - t; x < 5.f; x += sp) out.push_back(Stroke{{V(x, -10.f - t), V(x + t, -10.f)}, W_THIN, P_SOLID, R_LINE, 0.55f});
        out.push_back(Stroke{{V(0.f, top - 0.1f), V(0.f, top + 0.9f)}, W_THIN, P_CENTER, R_CONS, 0.8f * lvA});
        // the well's width, below it
        const float yd = -10.95f;
        out.push_back(Stroke{{V(-5.f - t, -10.f - t - 0.1f), V(-5.f - t, yd - 0.18f)}, W_THIN, P_SOLID, R_DIM, 0.7f});
        out.push_back(Stroke{{V(5.f + t, -10.f - t - 0.1f), V(5.f + t, yd - 0.18f)}, W_THIN, P_SOLID, R_DIM, 0.7f});
        out.push_back(Stroke{{V(-5.f - t, yd), V(-0.75f, yd)}, W_THIN, P_SOLID, R_DIM, 0.7f});
        out.push_back(Stroke{{V(0.75f, yd), V(5.f + t, yd)}, W_THIN, P_SOLID, R_DIM, 0.7f});
        arrow(V(-5.f - t, yd), V(-1, 0), R_DIM, 0.7f, out);
        arrow(V(5.f + t, yd), V(1, 0), R_DIM, 0.7f, out);
        labels_.push_back({"106", V(0.f, yd + 0.2f), 0.42f * pxPerUnit_, 0.75f, R_DIM, false, true});
    }
    // the title block, bottom right
    {
        const float w = 7.4f, hgt = 2.55f;
        const float x0 = X1 - w, y0 = Y0, x1 = X1, y1 = Y0 + hgt;
        out.push_back(Stroke{{V(x0, y0), V(x0, y1), V(x1, y1)}, W_MED, P_SOLID, R_LINE, 0.95f});
        const float r1 = y1 - 1.05f, r2 = r1 - 0.75f;
        out.push_back(Stroke{{V(x0, r1), V(x1, r1)}, W_THIN, P_SOLID, R_LINE, 0.9f});
        out.push_back(Stroke{{V(x0, r2), V(x1, r2)}, W_THIN, P_SOLID, R_LINE, 0.9f});
        const float c1 = x0 + 3.0f, c2 = x0 + 5.0f;
        out.push_back(Stroke{{V(c1, y0), V(c1, r1)}, W_THIN, P_SOLID, R_LINE, 0.9f});
        out.push_back(Stroke{{V(c2, y0), V(c2, r1)}, W_THIN, P_SOLID, R_LINE, 0.9f});
        const float cap = 0.24f * pxPerUnit_, val = 0.42f * pxPerUnit_;
        labels_.push_back({"TETRIS UNIVERSE", V(x0 + 0.2f, y1 - 0.2f), 0.5f * pxPerUnit_, 0.95f, R_LINE, false, false});
        labels_.push_back({"GENERAL ASSEMBLY  -  MECHANISM", V(x0 + 0.2f, y1 - 0.78f), cap, 0.7f, R_LINE, false, false});
        auto field = [&](float x, float yTop, const std::string& k, const std::string& v, float a) {
            labels_.push_back({k, V(x + 0.12f, yTop - 0.1f), cap, 0.6f, R_LINE, false, false});
            labels_.push_back({v, V(x + 0.12f, yTop - 0.32f), val, a, R_LINE, false, false});
        };
        field(x0, r1, "DWG NO.", dwgNo_, 0.95f);
        field(c1, r1, "SCALE", "1 : 1", 0.95f);
        field(c2, r1, "SHEET", "1 / 1", 0.95f);
        field(x0, r2, "DRAWN", "T. UNIVERSE", 0.95f);
        field(c1, r2, "UNITS", "MM", 0.95f);
        // the revision: the letter, a delta drawn around it on each clear (in the accent, settling to ink)
        labels_.push_back({"REV", V(c2 + 0.12f, r2 - 0.1f), cap, 0.6f, R_LINE, false, false});
        const std::string letter = rev_ > 0 ? revLetter(rev_ - 1) : "-";
        const float fresh = 1.f - smoothstepf(0.6f, 2.4f, revT_);
        const vec2 tc = V(c2 + 1.25f, r2 - 0.5f);
        labels_.push_back({letter, V(tc.x, tc.y + 0.18f), val, 0.95f, fresh > 0.3f ? R_ACCENT : R_LINE, false, true});
        if (rev_ > 0) {
            const float tr = 0.42f;
            Stroke tri{{polar(tc, tr, PI / 2), polar(tc, tr, PI / 2 + TAU / 3), polar(tc, tr, PI / 2 + 2 * TAU / 3), polar(tc, tr, PI / 2)},
                       W_THIN, P_SOLID, fresh > 0.3f ? R_ACCENT : R_LINE, 0.9f};
            for (auto& p : tri.pts) p.y -= 0.02f;
            emit(tri, saturate(revT_ / 0.45f), 1.f);
        }
        // a Tetris: a revision cloud around the block, drawn by the pen, lifting after a few seconds
        if (cloudT_ < 4.5f) {
            const float a = 1.f - smoothstepf(3.2f, 4.5f, cloudT_);
            Stroke cl;
            cl.w = W_THIN;
            cl.role = R_ACCENT;
            const float m = 0.2f;
            const vec2 c[4] = {V(x0 - m, y0 + 0.05f), V(x1 + 0.02f, y0 + 0.05f), V(x1 + 0.02f, y1 + m), V(x0 - m, y1 + m)};
            for (int e = 0; e < 4; e++) {
                const vec2 p0 = c[e], p1 = c[(e + 1) % 4];
                const float L = len(sub(p1, p0));
                const int n = std::max(1, (int)std::round(L / 0.55f));
                const vec2 d = mul(sub(p1, p0), 1.f / n);
                const float ang = std::atan2(d.y, d.x);
                for (int i = 0; i < n; i++) {
                    const vec2 mid = add(p0, mul(d, i + 0.5f));
                    const auto arc = arcPts(mid, len(d) * 0.5f, ang + PI, ang, 8);
                    cl.pts.insert(cl.pts.end(), arc.begin(), arc.end());
                }
            }
            emit(cl, saturate(cloudT_ / 1.1f), a);
        }
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void BlueprintUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 2;            // CAD
        else if (t.hasSong && !t.minor && t.brightness > 0.58f) v = h < 0.5f ? 1 : 3; // the white sheets
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const uint64_t old = seed_;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0xB1E5ull);
    if (seconds <= 0.f) palSet_ = false;
    if (old != seed_) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "BP-%04X", (unsigned)(seed_ & 0xFFFF));
        dwgNo_ = buf;
        // a new drawing: the old machine stays as a ghost the pen draws over again
        for (Part& p : parts_) {
            if (p.reveal > 0.f) p.ever = true;
            p.started = false;
            p.reveal = 0.f;
        }
        partsWanted_ = 0;
        lastPartBar_ = barCount_ - 2;
    }
}

void BlueprintUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropMode_ = false;
    const bool instant = !hasLevel_ || seconds < 0.1f;
    if (instant) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        // the drawing as it would be by then
        const int want = level == 0 ? 0 : level == 1 ? 3 : 5;
        while (partsWanted_ < want) {
            addPart(1.f, 0.f);
            parts_[partsWanted_ - 1].reveal = 1.f;
        }
        detail_ = level == 2 ? 1.f : 0.f;
        consA_ = level == 0 ? 0.5f : 0.8f;
        return;
    }
    const bool rising = level > level_;
    if (level_ == 0 && level >= 1) {
        // back from a break: the pen inks the ghosts again, one after another
        float delay = 0.f;
        for (Part& p : parts_)
            if (p.started && p.ink < 0.6f) {
                p.reveal = 0.f;
                p.ink = 1.f;
                p.delay = delay;
                p.speed = 0.5f;
                delay += 0.5f;
            }
    }
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.f, 6.f);
    (void)impact;
}

// The drop: the machine comes alive and the print changes. At least five parts are on the sheet at once.
void BlueprintUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    float delay = 0.f;
    for (Part& p : parts_)
        if (p.started && p.reveal < 1.f) p.speed = std::max(p.speed, 1.2f);
    while (partsWanted_ < 5) {
        addPart(1.1f, delay);
        delay += 0.2f;
    }
    lastPartBar_ = barCount_;
}

void BlueprintUniverse::refresh(float strength) {
    if (level_ >= 1 && strength > 0.3f) addPart(0.6f, 0.f);
}

void BlueprintUniverse::levelUp() {}

vec2 BlueprintUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints BlueprintUniverse::hints() const {
    const Palette p = basePalette(variant_);
    TransitionHints h;
    h.background = p.paper;
    h.light = p.dark < 0.5f;
    h.ink = p.line;
    h.accent = p.accent;
    h.styles = TS_INK | TS_LUMA | TS_REBUILD;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void BlueprintUniverse::syncIds(const Game& g) {
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

void BlueprintUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        float x0 = 99.f, x1 = -99.f, yLo = 99.f, yHi = -99.f;
        for (auto& c : ev.cells) {
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            x0 = std::min(x0, p.x - 0.5f); x1 = std::max(x1, p.x + 0.5f);
            yLo = std::min(yLo, p.y); yHi = std::max(yHi, p.y);
        }
        if (!ev.cells.empty()) {
            lockFx_.push_back({id, yLo, yHi, 0.f});
            // the part is dimensioned above it
            const float top = yHi + 0.5f;
            if (top + 1.4f < 10.f) pieceDim_ = Dim{x0, x1, top, 0.f, 0.f, true, (int)std::lround((x1 - x0) * 10.f)};
            else pieceDim_.alive = false;
        }
        break;
    }
    case GameEvent::Clear: {
        // the rows are rubbed out (each cell keeps its piece's outline while it lifts)
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
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
        if (pieceDim_.alive) pieceDim_.life = std::max(pieceDim_.life, 3.5f); // lifted with the row
        rev_++;
        revT_ = 0.f;
        if (ev.count >= 4) cloudT_ = 0.f;
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        break;
    default: break;
    }
    (void)game;
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void BlueprintUniverse::cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int kind, int style,
                             float hatchA, float erase, const vec3& line, float front) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {tint.x, tint.y, tint.z, alpha}, {(float)kind, (float)style, hatchA, erase},
                      {line.x, line.y, line.z, front}});
}

void BlueprintUniverse::buildBoard(const Game& g) {
    auto tintOf = [&](int t) { return pal_.piece[std::clamp(t, 0, 6)]; };
    auto lineOf = [&](int t) { return lerp(pal_.outline, tintOf(t), pal_.outlineTint); };
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    const float hatch = pal_.hatchA;
    // locked parts (their hatching drawn up from the impact when they land)
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
            float front = 1e4f;
            for (const LockFx& l : lockFx_)
                if (l.id == ids_[y][x]) front = lerpf(l.minY - 0.55f, l.maxY + 0.6f, smoothstepf(0.f, 0.26f, l.t));
            float alpha = 1.f;
            float h = hatch;
            if (overT_ >= 0.f) {
                // game over: the parts are struck out from the bottom, leaving their outlines as hidden lines
                const float rowFrac = (Game::H - 1 - y) / (float)(Game::H - Game::HIDDEN);
                const float k = smoothstepf(rowFrac * 0.8f, rowFrac * 0.8f + 0.2f, over_);
                h *= 1.f - k;
                alpha = 1.f - 0.55f * k;
            }
            cell(p.x, p.y, 0.5f, maskOf(x, y, same), tintOf(c.type), alpha, 0, c.type, h, 0.f, lineOf(c.type), front);
        }
    // rows being rubbed out (real time)
    for (const Fading& f : fading_)
        cell(f.x, f.y, 0.5f, f.mask, tintOf(f.type), 1.f, 0, f.type, hatch, saturate(f.t / 0.36f), lineOf(f.type), 1e4f);
    // the ghost (a hidden line) and the falling part (its hatching lighter until it lands)
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
                cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), tintOf(p.type), 0.8f, 1, p.type, 0.f, 0.f, lineOf(p.type), 1e4f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), tintOf(p.type), 1.f, 0, p.type, hatch * 0.5f, 0.f, lineOf(p.type), 1e4f);
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
            cell(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), tintOf(type), alpha, 0, type, hatch, 0.f, lineOf(type), 1e4f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
    // the bonus gauge: a drafted circle hatched as it charges
    {
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        cell(gc.x, gc.y, layout::GAUGE_R, 0, pal_.accent, g.bonusReady() || g.bonusActive() ? 1.f : 0.f, 3, 0, 1.f, 0.f, pal_.line, fill);
    }
}

// =====================================================================================================
// Text
// =====================================================================================================

float BlueprintUniverse::textWidth(const std::string& s, float size, float track) const {
    const Font& f = font_;
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

void BlueprintUniverse::textQuads(const std::string& s, float x, float y, float size, float track, std::vector<float>& out,
                                  float slant, bool vertical) const {
    const Font& f = font_;
    if (!f.ok) return;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    const float baseline = f.capH * k; // local: x along the line, y down from the top
    float pen = 0.f;
    auto place = [&](float u, float v, float tu, float tv) {
        // slant: the top leans forward
        u += slant * (baseline - v);
        float px, py;
        if (vertical) { px = x + v; py = y - u; } // reading upward, the letters' tops to the left
        else { px = x + u; py = y + v; }
        out.push_back(px); out.push_back(py); out.push_back(tu); out.push_back(tv);
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

void BlueprintUniverse::drawText(const std::vector<float>& q, const vec3& ink, float alpha, float halo) {
    if (q.empty() || !font_.ok || alpha <= 0.004f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    set3f(progText_, "uInk", lerp(ink, bridge_.ink, b));
    set3f(progText_, "uHalo", lerp(pal_.paper, bridge_.color, b));
    set1f(progText_, "uHaloA", std::min(halo, hudHalo_));
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// World lettering (dimensions, the title block, zone marks): slanted like hand lettering on a drawing.
void BlueprintUniverse::drawLabels() {
    hudHalo_ = 1.f;
    const float world = 1.f - smoothstepf(0.f, 0.6f, bridge_.amount);
    std::vector<float> q;
    for (const Label& l : labels_) {
        if (l.alpha <= 0.004f || l.text.empty()) continue;
        q.clear();
        const vec2 p = project(vec3(l.pos.x, l.pos.y, 0.f));
        const float track = l.size * 0.06f;
        const float w = textWidth(l.text, l.size, track);
        const float capH = font_.capH * l.size / font_.px;
        if (l.vertical) textQuads(l.text, p.x - capH * 0.5f, p.y + (l.center ? w * 0.5f : 0.f), l.size, track, q, 0.18f, true);
        else textQuads(l.text, p.x - (l.center ? w * 0.5f : 0.f), p.y, l.size, track, q, 0.18f, false);
        drawText(q, roleColor(l.role), l.alpha * world, 0.9f);
    }
}

// The HUD in BLUEPRINT's lettering: condensed technical capitals, labels spaced like the captions of a title block.
void BlueprintUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        std::vector<float> v;
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal_.accent : pal_.paper;
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
    hudHalo_ = 0.f;
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const std::string& text, float size, float track, float dy = 0.f, float slant = 0.f) {
            const float w = textWidth(text, size, track);
            textQuads(text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q, slant);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string name = h.text, var;
            const size_t dot = name.find("\xC2\xB7");
            if (dot != std::string::npos) { var = name.substr(dot + 2); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            const float sz = base * 1.1f, vsz = base * 0.66f, tr = sz * 0.16f, vtr = vsz * 0.3f;
            const float wName = textWidth(name, sz, tr);
            const float wVar = var.empty() ? 0.f : textWidth(var, vsz, vtr);
            const float gap = base * 0.8f;
            const float x = ugl::hudLeft(h, wName + (var.empty() ? 0.f : gap + wVar), width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(name, x, h.y, sz, tr, q);
            drawText(q, ink, a);
            if (!var.empty()) {
                q.clear();
                textQuads(var, x + wName + gap, h.y + (sz - vsz) * 0.72f, vsz, vtr, q);
                drawText(q, ink, a * 0.75f);
            }
            continue;
        }
        case HUD_SECTION:
            place(h.text, base * 0.95f, base * 0.42f);
            drawText(q, ink, saturate(0.2f + h.alpha * 1.9f));
            continue;
        case HUD_TITLE:
            place(h.text, base * 1.2f, base * 0.04f, -base * 0.1f, 0.12f);
            drawText(q, ink, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(h.text, base * 0.75f, base * 0.14f);
            drawText(q, ink, saturate(alpha * 1.2f) * 0.75f);
            continue;
        case HUD_LABEL:
            place(h.text, base * 0.72f, base * 0.36f);
            drawText(q, ink, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_VALUE:
            place(h.text, base * 1.1f, base * 0.06f);
            drawText(q, ink, alpha);
            continue;
        case HUD_NOTICE:
            place(h.text, base * 1.05f, base * 0.1f, 0.f, 0.15f);
            drawText(q, accent, alpha);
            continue;
        case HUD_KEY:
            place(h.text, base * 0.85f, base * 0.1f);
            drawText(q, ink, alpha);
            continue;
        case HUD_TOAST:
            place(h.text, base * 1.3f, base * 0.45f, -base * 0.15f);
            drawText(q, ink, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(h.text, base * 0.9f, base * 0.1f, 0.f, 0.15f);
            drawText(q, accent, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(h.text, base * 1.2f, base * 0.1f, -base * 0.1f);
                drawText(q, ink, alpha);
            } else {
                place(h.text, base * 0.85f, base * 0.1f);
                drawText(q, h.accent ? accent : ink, alpha);
            }
            continue;
        default:
            place(h.text, base * 0.8f, base * 0.08f);
            drawText(q, h.accent ? accent : ink, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void BlueprintUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 0.9f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 3.f) : 0.f, overT_ >= 0.f ? 10.f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    dropAmt_ = approach(dropAmt_, dropMode_ ? 1.f : 0.f, dropMode_ ? 4.f : 0.5f, dt);
    // the print changes on the drop's downbeat (quickly, so the pieces never sit long in between), back slowly
    colAmt_ = approach(colAmt_, dropMode_ ? 1.f : 0.f, dropMode_ ? 10.f : 0.6f, dt);
    {
        const Palette target = basePalette(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        Palette dp = dropPalette(variant_);
        lerpPalette(pal_, dp, smoothstepf(0.f, 1.f, colAmt_));
        // slow motion: the sheet under a dimmer lamp
        if (slow_ > 0.01f) {
            const float k = 0.22f * slow_;
            pal_.paper = pal_.paper * (1.f - k);
            pal_.paper2 = pal_.paper2 * (1.f - k);
        }
    }
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    detail_ = approach(detail_, wp > 0.5f ? 1.f : 0.f, 0.8f, dt);
    consA_ = approach(consA_, 0.5f + 0.3f * wm, 1.f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    fire_ = std::max(approach(fire_, 0.f, 7.f, dt), kick * dropAmt_ * wp);

    // ---- The clock: bars draw new parts; in a drop, the machine runs on the beat.
    if (!f.paused) {
        auto onBar = [&]() {
            barCount_++;
            if (level_ < 1 || overT_ >= 0.f) return;
            const int interval = dropMode_ ? 4 : level_ >= 2 ? 2 : 4;
            if (barCount_ - lastPartBar_ >= interval) {
                lastPartBar_ = barCount_;
                addPart(dropMode_ ? 0.9f : level_ >= 2 ? 0.55f : 0.32f, 0.f);
            }
        };
        if (music.hasSong) {
            // bars counted from the beat grid (robust to long frames: every bar crossed counts, a seek does not)
            const double bar = std::floor(music.beat / 4.0 + 1e-4);
            if (lastBeat_ < -0.5 || bar < lastBeat_ - 2.0 || bar > lastBeat_ + 4.0) lastBeat_ = bar;
            while (bar > lastBeat_) {
                lastBeat_ += 1.0;
                onBar();
            }
            // the drive: a ratchet every two beats (half eased, half steady), one turn of A every eight beats
            const double u = music.beat / 2.0;
            const double fu = u - std::floor(u);
            const double e = 1.0 - std::pow(1.0 - std::min(1.0, fu / 0.45), 3.0);
            const double pos = std::floor(u) + 0.45 * fu + 0.55 * e; // in half-time units
            double d = pos - driveBeat_;
            driveBeat_ = pos;
            if (d < 0.0 || d > 0.5) d = 0.0; // a seek
            motion_ = approach(motion_, dropMode_ && wp > 0.5f && overT_ < 0.f ? 1.f : 0.f, dropMode_ ? 5.f : 1.2f, dt);
            angle_ -= (float)(d * (TAU / 4.0)) * motion_ * lerpf(1.f, 0.35f, slow_);
        } else {
            barClock_ += dt;
            if (barClock_ > 2.f) { barClock_ -= 2.f; onBar(); }
        }
        // the pen
        for (Part& p : parts_) {
            if (!p.started) continue;
            if (p.delay > 0.f) { p.delay -= dt; continue; }
            p.reveal = std::min(1.f, p.reveal + dt * p.speed * (1.f + swell_));
            if (p.reveal >= 1.f) p.ever = true;
            // a break leaves the machine as a faint ghost
            p.ink = level_ == 0 && lv_ < 0.6f ? approach(p.ink, 0.f, 0.5f, dt) : p.ink;
        }
    }
    for (auto& l : lockFx_) l.t += dt;
    lockFx_.erase(std::remove_if(lockFx_.begin(), lockFx_.end(), [](const LockFx& l) { return l.t > 0.5f; }), lockFx_.end());
    for (auto& fd : fading_) fd.t += dt; // a clear runs in real time, even in slow motion
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > 0.36f; }), fading_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);
    revT_ += dt;
    cloudT_ += dt;
    if (pieceDim_.alive) {
        pieceDim_.t += dt;
        // the space above the part must stay free (a clear or a new part there lifts the dimension)
        for (int y = Game::HIDDEN; y < Game::H; y++) {
            const float cy = layout::cellPos(0.f, (float)y).y;
            if (cy < pieceDim_.y || cy > pieceDim_.y + 1.5f) continue;
            for (int x = 0; x < Game::W; x++) {
                const float cx = layout::cellPos((float)x, 0.f).x;
                if (cx > pieceDim_.x0 && cx < pieceDim_.x1 && game.cell(x, y).type >= 0) pieceDim_.life = std::max(pieceDim_.life, 3.4f);
            }
        }
        if (pieceDim_.t > 4.2f + pieceDim_.life) pieceDim_.alive = false;
    }
    {
        const int rows = std::min(20, game.stackHeight());
        if (rows != stackRows_) { stackRows_ = rows; stackDimT_ = 0.f; }
        stackDimT_ += dt;
        stackShown_ = approach(stackShown_, (float)stackRows_, 5.f, dt);
    }

    // ---- The drawing.
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    lines_.clear();
    labels_.clear();
    std::vector<Stroke> ss;
    buildSheet(ss, 1.f);
    for (const Stroke& s : ss) emit(s, 1.f, world);
    // construction lines through the machine's centers (a break shows only these)
    if (machOn_) {
        ss.clear();
        const float xr0 = 11.6f, xr1 = viewH_.x - 0.3f;
        auto hline = [&](float y, float x0, float x1) { ss.push_back(Stroke{{V(x0, y), V(x1, y)}, W_THIN, P_SOLID, R_CONS, 0.55f}); };
        auto vline = [&](float x, float y0, float y1) { ss.push_back(Stroke{{V(x, y0), V(x, y1)}, W_THIN, P_SOLID, R_CONS, 0.55f}); };
        hline(gA_.y, xr0, xr1);
        hline(gB_.y, xr0 + 1.f, xr1);
        vline(gA_.x, -7.6f, viewH_.y - 0.3f);
        vline(gB_.x, -7.6f, gA_.y + 1.f);
        hline(yoke_.y, xr0, xr1);
        hline(cam_.y, -xr1, -xr0);
        hline(link_.y, -xr1, -xr0);
        vline(cam_.x, -7.2f, 8.6f);
        vline(link_.x, -7.2f, cam_.y - 1.f);
        ss.push_back(Stroke{arcPts(gA_, rA_ + 1.2f * machScale_, 3.5f, 4.6f, 24), W_THIN, P_SOLID, R_CONS, 0.55f});
        ss.push_back(Stroke{arcPts(cam_, 2.4f * machScale_, 0.2f, 1.2f, 24), W_THIN, P_SOLID, R_CONS, 0.55f});
        const float reveal = saturate(time_ / 4.f);
        emitAll(ss, reveal, consA_ * world * (1.f - over_ * 0.6f));
        // the parts
        for (const Part& p : parts_) {
            if (!p.started && !p.ever) continue;
            ss.clear();
            buildPart(p.kind, ss);
            const float ghost = 0.16f;
            if (p.ever && p.reveal < 1.f) emitAll(ss, 1.f, ghost * world);
            if (p.started) emitAll(ss, p.reveal, (ghost + (1.f - ghost) * p.ink) * world);
        }
    }
    // dimensions: the last part, the stack's height
    ss.clear();
    if (pieceDim_.alive && overT_ < 0.f) {
        const Dim& d = pieceDim_;
        // half the ink of the other dimensions: it lies inside the well
        const float a = 0.5f * (1.f - smoothstepf(3.0f, 4.0f, d.t + d.life)) * world;
        const float y = d.y + 0.72f;
        dimH(d.x0, d.x1, y, d.y, d.y, saturate(d.t / 0.4f), a, R_DIM, ss);
        if (d.t > 0.25f) labels_.push_back({std::to_string(d.value), V((d.x0 + d.x1) * 0.5f, y + 0.5f), 0.46f * pxPerUnit_, a * saturate((d.t - 0.25f) / 0.2f), R_DIM, false, true});
    }
    if (stackShown_ > 0.05f && overT_ < 0.f) {
        const float x = 5.62f;
        const float y1 = -10.f + stackShown_;
        dimV(-10.f, y1, x, 5.f + 0.3f, 1.f, 0.85f * world, R_DIM, ss);
        const float ta = 0.85f * world * smoothstepf(0.f, 0.5f, stackDimT_);
        if (stackShown_ > 1.2f)
            labels_.push_back({std::to_string(stackRows_ * 10), V(x + 0.1f, -10.f + stackShown_ * 0.5f), 0.4f * pxPerUnit_, ta, R_DIM, true, true});
    }
    for (const Stroke& s : ss) emit(s, 1.f, 1.f);

    // ---- Pen lines into their layer (MAX: joints and crossings of one ink never darken).
    lineT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!lines_.empty()) {
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

    // ---- The sheet.
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progPaper_);
    set3f(progPaper_, "uPaper", pal_.paper);
    set3f(progPaper_, "uPaper2", pal_.paper2);
    set4f(progPaper_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progPaper_, "uMottle", pal_.mottle);
    set1f(progPaper_, "uGrain", pal_.dark > 0.5f ? 0.035f : 0.018f);
    set1f(progPaper_, "uVignette", 0.12f);
    set1f(progPaper_, "uSeed", (float)(seed_ % 97));
    set1f(progPaper_, "uBridge", bridge);
    set3f(progPaper_, "uBridgeCol", bridge_.color);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progLayer_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lineT_.tex);
    set1i(progLayer_, "uLayer", 0);
    ugl::drawFullscreen();

    // ---- Pieces, ghost, previews, gauge.
    cells_.clear();
    buildBoard(game);
    if (!cells_.empty()) {
        const float s = height_ / 1080.f;
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uLineW", std::max(1.2f, 2.2f * s));
        set1f(progCell_, "uHatchW", std::max(0.8f, 1.25f * s));
        set1f(progCell_, "uSpacing", 0.2f);
        set1f(progCell_, "uPxPerUnit", pxPerUnit_);
        set1f(progCell_, "uFillA", pal_.fillA);
        set1f(progCell_, "uHatchShade", lerpf(0.72f, 1.f, pal_.dark)); // darker hatching on light paper
        set3f(progCell_, "uPaper", pal_.paper);
        set3f(progCell_, "uRing", pal_.line);
        set3f(progCell_, "uAccent", pal_.accent);
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    // ---- Lettering and the HUD.
    if (overT_ >= 0.f) {
        const float a = smoothstepf(0.5f, 1.5f, overT_) * world;
        labels_.push_back({"VOID", V(viewC_.x + viewH_.x - 0.3f - 3.7f, viewC_.y - viewH_.y + 0.3f + 2.2f), 1.1f * pxPerUnit_, a * 0.9f, R_ACCENT, false, true});
    }
    drawLabels();
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
    (void)wm;
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_blueprint() {
    static const UniverseEntry e{
        "BLUEPRINT", "a technical drawing: drafted, dimensioned parts, a machine drawn in the margins that runs on the drop",
        [] { return std::unique_ptr<Universe>(new BlueprintUniverse()); },
        // Driving, structured songs suit the machine best; it fits most songs.
        [](const SongLook& l) { return 0.8f + 0.25f * l.dynamics + 0.15f * l.density; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
