// TOPOGRAPHY: a contour map whose terrain is the stack.
//
// One world: a survey sheet of fine contour lines (an index contour every fifth line) on white or black paper.
// The stack is the land: every locked cell is elevation, so the stack stands as a plateau with its foothills, and
// the isolines hug its skyline, spread up the empty well and run out over the margins, redrawn on every lock. The
// pieces are small tinted plateaus (each type its own tint, a crisp outline and two form lines inside, the cells of
// one piece merged), the falling piece hovers over the map with a soft shadow, the ghost is a dashed border.
//
// The song is the weather. Calm parts drizzle: a few faint streams. Mid parts rain: the water runs downhill on the
// actual terrain (a drainage network on a coarser grid, hollows filling into ponds), gathers into rivers that flow, and carves the land and the stack's flanks
// (the erosion is kept for the whole song: the map keeps a record). The drop is a flood: the water rises, lakes fill
// the valleys and ring the stack, lines parallel to the shores ripple outward with the kicks, the layer tints come
// in, and the level swells slowly with the phrases. A cleared row sinks into the map (the cells shrink and fade; the
// contours relax as the land slumps) and leaves its debris as fans against the well's walls, a Tetris is a flash
// flood, slow motion is a low dusk sun raking the relief, game over is the sea rising over everything.
//
// Cost: the terrain is a CPU grid of four samples per cell (~20k), three box blurs a frame, the drainage network a few times a second; the
// map is one full-screen pass sampling it with a B-spline.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane (like SUMI).
#include "universes/topography/topography.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <queue>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/topography/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Barlow (Italic for names, Medium for labels and numbers), SIL Open Font License, see OFL-Barlow.txt.
const unsigned char FONT_ITALIC[] = {
#include "universes/topography/Barlow-Italic.ttf.inc"
};
const unsigned char FONT_ROMAN[] = {
#include "universes/topography/Barlow-Medium.ttf.inc"
};

namespace shaders = topography_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// The maps (sRGB): paper, contour, index contour, water ink, lake, three layer tints (low, mid, high), woodland,
// HUD ink, accent; the seven piece tints (I O T S Z J L); dark paper, contour opacity, woodland, relief shading.
struct VariantDef {
    const char* name;
    unsigned paper, line, index, water, lake, tint0, tint1, tint2, wood, ink, accent;
    unsigned piece[7];
    float dark, lineAlpha, woodAmt, shade;
};
const VariantDef VARIANTS[] = {
    // A Swiss sheet: white paper, brown contours, blue water; green lowlands, pale rock at the peak.
    {"SWISS", 0xfafaf8, 0xc0946a, 0x8c5f36, 0x2f7fc1, 0xd3e6f4, 0xd3e8cc, 0xe9efe4, 0xffffff, 0xe2eedc, 0x3a3530, 0x2f7fc1,
     {0x2a9d8f, 0xe3a72f, 0x8a5fbf, 0x5fa84a, 0xd64933, 0x3a5fbf, 0xe8833a}, 0.f, 0.85f, 0.f, 0.22f},
    // Night survey: pale lines on near-black, cyan water, pastel plateaus.
    {"NIGHT SURVEY", 0x0b0e11, 0x56636f, 0x9aa8b5, 0x4fc0d8, 0x0d2630, 0x0f1b1b, 0x191a28, 0x2b2536, 0x0d1510, 0xd8dee4, 0x4fc0d8,
     {0x86e0d0, 0xf3d38a, 0xc5a3ef, 0xb8dc7c, 0xf29a9a, 0x8fa8f5, 0xf5b47e}, 1.f, 0.8f, 0.f, 0.3f},
    // A quadrangle: brown contours, woodland green, deep survey colors.
    {"QUADRANGLE", 0xfcfcfa, 0xb8825a, 0x8a5530, 0x2b77cf, 0xc2def5, 0xc8e2b6, 0xe2ecd6, 0xf6f8f2, 0xd9ecce, 0x2c2a26, 0xc8372d,
     {0x0f8ea8, 0xd9a400, 0xa24db8, 0x2e8b57, 0xc8372d, 0x34489a, 0xe57b1f}, 0.f, 0.9f, 1.f, 0.18f},
    // Graphite: grey lines, black water lines, muted inks for the pieces.
    {"GRAPHITE", 0xf5f5f4, 0x969696, 0x3a3a3a, 0x2a2a2a, 0xd6d8da, 0xf2f2f1, 0xe8e8e7, 0xfbfbfb, 0xe9e9e9, 0x222222, 0x222222,
     {0x5b8fa8, 0xd0a53c, 0x8c6fa8, 0x6e9a50, 0xc65f52, 0x4b5b8f, 0xd28545}, 0.f, 0.9f, 0.f, 0.28f},
    // Glacier: blue contours on white like the ice on alpine sheets, warm plateaus.
    {"GLACIER", 0xf8fbfd, 0x86afd6, 0x3f74a8, 0x1f5fa8, 0xb4d2ef, 0xf1f6fa, 0xf6f9fc, 0xffffff, 0xe2eef7, 0x23364a, 0x1f5fa8,
     {0x2fa39a, 0xf0b43c, 0xc0569a, 0x6aa84a, 0xe2553f, 0x34418f, 0xf08a3c}, 0.f, 0.85f, 0.f, 0.22f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The drop's colors, per variant: the pieces take another set of seven tints (sRGB, I O T S Z J L) and the
// contours turn toward the water's ink.
const unsigned DROP_PIECES[][7] = {
    {0xe8505b, 0x2ab7a9, 0xf2b134, 0x6c5ce7, 0x3fa7d6, 0xe36397, 0x8cc152}, // SWISS
    {0xffd27f, 0x7fd8ff, 0xff9ec7, 0xb7a6ff, 0x9ef0b0, 0xffa07a, 0xf5f5a0}, // NIGHT SURVEY
    {0xd23c77, 0x1f9e89, 0xf2a900, 0x4b3fa0, 0x2e86de, 0xe8603c, 0x6aa121}, // QUADRANGLE
    {0xe4572e, 0x29a19c, 0xf3a712, 0x5f4bb6, 0x3c91e6, 0xc33c8f, 0x8bbf3f}, // GRAPHITE
    {0x5b6ee1, 0xff7a59, 0x2ec4b6, 0xf6b93b, 0xb5179e, 0x26a269, 0xe8505b}, // GLACIER
};
static_assert(sizeof(DROP_PIECES) / sizeof(DROP_PIECES[0]) == VARIANT_COUNT, "a drop set per variant");

using Palette = TopographyUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.paper = L(d.paper); p.line = L(d.line); p.index = L(d.index); p.water = L(d.water); p.lake = L(d.lake);
    p.tint0 = L(d.tint0); p.tint1 = L(d.tint1); p.tint2 = L(d.tint2); p.wood = L(d.wood); p.ink = L(d.ink);
    p.accent = L(d.accent);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.dark = d.dark; p.lineAlpha = d.lineAlpha; p.woodAmt = d.woodAmt; p.shade = d.shade;
    return p;
}

Palette dropPaletteOf(int v) {
    v = ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT;
    Palette p = paletteOf(v);
    for (int i = 0; i < 7; i++) p.piece[i] = ugl::toLinear(hex(DROP_PIECES[v][i]));
    p.line = lerp(p.line, p.water, 0.55f);
    p.index = lerp(p.index, p.water, 0.45f);
    return p;
}

void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

// Value noise for the ground (seeded per song).
float hashf(int x, int y, uint64_t s) {
    const uint64_t h = splitmix64((uint64_t)(uint32_t)x * 73856093ull ^ (uint64_t)(uint32_t)y * 19349663ull ^ s);
    return (h >> 40) * (1.0f / 16777216.0f);
}
float vnoise(float x, float y, uint64_t s) {
    const int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    const float a = hashf(xi, yi, s), b = hashf(xi + 1, yi, s), c = hashf(xi, yi + 1, s), d = hashf(xi + 1, yi + 1, s);
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
float fbm(float x, float y, uint64_t s) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < 3; i++) {
        sum += amp * vnoise(x, y, s + (uint64_t)i * 977);
        norm += amp;
        const float nx = 0.8f * x - 0.6f * y, ny = 0.6f * x + 0.8f * y;
        x = nx * 2.03f + 1.7f;
        y = ny * 2.03f - 0.9f;
        amp *= 0.5f;
    }
    return sum / norm;
}

// The same fbm, alive: a slow warp whose own noise drifts, and each octave drifting its own way, so the hills
// swell, part and merge rather than slide.
float fbmT(float x, float y, uint64_t s, float t) {
    const float wx = vnoise(x * 0.45f + t * 0.021f, y * 0.45f - t * 0.013f, s + 501) - 0.5f;
    const float wy = vnoise(x * 0.45f - t * 0.017f + 7.3f, y * 0.45f + t * 0.019f, s + 503) - 0.5f;
    x += 1.1f * wx;
    y += 1.1f * wy;
    static const float DX[3] = {0.019f, -0.034f, 0.041f}, DY[3] = {-0.012f, -0.026f, 0.052f};
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < 3; i++) {
        sum += amp * vnoise(x + DX[i] * t, y + DY[i] * t, s + (uint64_t)i * 977);
        norm += amp;
        const float nx = 0.8f * x - 0.6f * y, ny = 0.6f * x + 0.8f * y;
        x = nx * 2.03f + 1.7f;
        y = ny * 2.03f - 0.9f;
        amp *= 0.5f;
    }
    return sum / norm;
}

// A separable box blur (zero outside), `passes` times: three passes are close to a gaussian.
void boxBlur(std::vector<float>& f, std::vector<float>& tmp, int w, int h, int r, int passes) {
    tmp.resize(f.size());
    const float inv = 1.f / (2 * r + 1);
    for (int p = 0; p < passes; p++) {
        for (int y = 0; y < h; y++) {
            const float* src = &f[(size_t)y * w];
            float* dst = &tmp[(size_t)y * w];
            float acc = 0;
            for (int x = 0; x <= r && x < w; x++) acc += src[x];
            for (int x = 0; x < w; x++) {
                dst[x] = acc * inv;
                if (x + r + 1 < w) acc += src[x + r + 1];
                if (x - r >= 0) acc -= src[x - r];
            }
        }
        for (int x = 0; x < w; x++) {
            float acc = 0;
            for (int y = 0; y <= r && y < h; y++) acc += tmp[(size_t)y * w + x];
            for (int y = 0; y < h; y++) {
                f[(size_t)y * w + x] = acc * inv;
                if (y + r + 1 < h) acc += tmp[(size_t)(y + r + 1) * w + x];
                if (y - r >= 0) acc -= tmp[(size_t)(y - r) * w + x];
            }
        }
    }
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

constexpr float STACK_H = 5.5f;   // the plateau's height (contour intervals)
constexpr float FOOTHILLS = 13.f;
constexpr float WELL_FLOOR = 0.4f; // the well's terrace
constexpr float POND_MIN = 0.35f;   // the depth of a hollow that holds a pond in the rain
constexpr float RIVER_THR = 55.f; // the rain gathered (in nodes of drizzle) where a river shows // the foothills' height for a full blur of stack

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool TopographyUniverse::init(int width, int height) {
    progMap_ = ugl::program(ugl::FULLSCREEN_VS, shaders::MAP_FS, "topography map");
    progCell_ = ugl::program(shaders::CELL_VS, shaders::CELL_FS, "topography cell");
    progRiver_ = ugl::program(shaders::RIVER_VS, shaders::RIVER_FS, "topography river");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "topography shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "topography text");
    progPost_ = ugl::program(ugl::FULLSCREEN_VS, shaders::POST_FS, "topography post");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &cellVao_);
    glGenBuffers(1, &cellVbo_);
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(CellInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glGenVertexArrays(1, &riverVao_);
    glGenBuffers(1, &riverVbo_);
    glBindVertexArray(riverVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, riverVbo_);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(RiverInst), (void*)(sizeof(float) * 4 * i));
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

    glGenTextures(1, &fieldTex_);
    glBindTexture(GL_TEXTURE_2D, fieldTex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    buildFont(fontItalic_, FONT_ITALIC);
    buildFont(fontRoman_, FONT_ROMAN);
    pal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void TopographyUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] topography font init failed\n");
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

void TopographyUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    riverT_.create(width_, height_, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
    buildGrid();
}

TopographyUniverse::~TopographyUniverse() {
    sceneT_.destroy();
    riverT_.destroy();
    for (GLuint p : {progMap_, progRiver_, progCell_, progShape_, progText_, progPost_}) glDeleteProgram(p);
    for (GLuint v : {riverVao_, cellVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, riverVbo_, cellVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    if (fieldTex_) glDeleteTextures(1, &fieldTex_);
    for (Font* f : {&fontItalic_, &fontRoman_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The terrain grid
// =====================================================================================================

void TopographyUniverse::buildGrid() {
    gx0_ = viewC_.x - viewH_.x - 1.f;
    gy0_ = viewC_.y - viewH_.y - 1.f;
    gw_ = (int)std::ceil(2.f * (viewH_.x + 1.f) / gs_) + 1;
    gh_ = (int)std::ceil(2.f * (viewH_.y + 1.f) / gs_) + 1;
    const size_t n = (size_t)gw_ * gh_;
    for (auto* v : {&base_, &wood_, &pondL_, &pondT_, &eros_, &sed_, &disp_, &target_, &mask_, &tmp_, &tmp2_, &big_}) v->assign(n, 0.f);
    cw_ = (gw_ + 1) / 2;
    ch_ = (gh_ + 1) / 2;
    const size_t cn = (size_t)cw_ * ch_;
    for (auto* v : {&nh_, &acc_, &rainF_, &rvis_}) v->assign(cn, 0.f);
    for (auto* v : {&down_, &up_, &order_}) v->assign(cn, -1);
    netT_ = 9.f;
    upload_.assign(n * 4, 0.f);
    fresh_ = true;
    buildGround();
}

// The ground of the sheet, from the song's seed: rolling hills in the outer margins, gentler beside the well and
// under the HUD columns, the well a shallow valley; woodland patches on the low, gentle ground.
void TopographyUniverse::buildGround() {
    if (gw_ <= 0) return;
    nC_.clear(); // everything afresh
    evolveGround();
    // spot heights: the tops of the highest hills in the outer margins
    peaks_.clear();
    for (int i = 0; i < 6; i++) {
        Peak p{0, 0, i % 3, 1.f, 1.f};
        if (placePeak(p)) peaks_.push_back(p);
    }
    spots_.clear();
    for (int tries = 0; tries < 3; tries++) {
        float best = -1e9f;
        int bi = -1, bj = -1;
        for (int j = 4; j < gh_ - 4; j++)
            for (int i = 4; i < gw_ - 4; i++) {
                const float x = gx0_ + i * gs_, y = gy0_ + j * gs_;
                if (!spotFree(x, y)) continue;
                bool far = true;
                for (const Spot& sp : spots_)
                    if ((sp.x - x) * (sp.x - x) + (sp.y - y) * (sp.y - y) < 36.f) far = false;
                if (!far) continue;
                const float v = base_[(size_t)j * gw_ + i];
                if (v > best) { best = v; bi = i; bj = j; }
            }
        if (bi < 0 || best < 1.5f) break;
        spots_.push_back({gx0_ + bi * gs_, gy0_ + bj * gs_, (int)std::lround(400.f + best * 25.f * 4.f)});
    }
}

// Where a spot height may stand: the outer margins, clear of the HUD's corners.
bool TopographyUniverse::spotFree(float x, float y) const {
    if (std::fabs(x) < 13.f || std::fabs(x) > viewH_.x - 1.5f || y < -8.f || y > viewC_.y + viewH_.y - 3.f) return false;
    return !(x < -9.f && (y > viewC_.y + 5.5f || y < -6.f));
}

void TopographyUniverse::evolveGround() {
    if (gw_ <= 0) return;
    const uint64_t s = seed_ * 31 + 7;
    const float t = evo_;
    // the noise on the coarse grid (a quarter of the samples), bilinear in between
    // (the woodland, slower still, a quarter of its rows a frame)
    const bool all = nC_.size() != (size_t)cw_ * ch_;
    nC_.resize((size_t)cw_ * ch_);
    wC_.resize((size_t)cw_ * ch_);
    woodRow_ = (woodRow_ + 1) & 3;
    for (int j = 0; j < ch_; j++)
        for (int i = 0; i < cw_; i++) {
            const float x = gx0_ + 2 * i * gs_, y = gy0_ + 2 * j * gs_;
            nC_[(size_t)j * cw_ + i] = fbmT(x / 8.5f + 3.1f, y / 8.5f - 1.7f, s, t);
            if (all || (j & 3) == woodRow_) wC_[(size_t)j * cw_ + i] = fbmT(x / 3.2f - 11.f, y / 3.2f + 5.f, s + 99, t * 0.6f);
        }
    for (int j = 0; j < gh_; j++) {
        const int cj = std::min(j / 2, ch_ - 2);
        const float fy = (j - 2 * cj) * 0.5f;
        const float y = gy0_ + j * gs_;
        for (int i = 0; i < gw_; i++) {
            const int ci = std::min(i / 2, cw_ - 2);
            const float fx = (i - 2 * ci) * 0.5f;
            const size_t c = (size_t)cj * cw_ + ci;
            auto bil = [&](const std::vector<float>& v) {
                return lerpf(lerpf(v[c], v[c + 1], fx), lerpf(v[c + cw_], v[c + cw_ + 1], fx), fy);
            };
            const float n = bil(nC_), wn = bil(wC_);
            const float x = gx0_ + i * gs_;
            const float ax = std::fabs(x);
            const float amp = lerpf(0.3f, 1.f, smoothstepf(9.5f, 14.5f, ax));
            const float wellX = 1.f - smoothstepf(4.f, 8.f, ax), wellY = 1.f - smoothstepf(9.f, 13.f, std::fabs(y));
            // the HUD's corners on the left (the song's title, the section) stay dry ground
            const float corner = smoothstepf(-10.f, -13.f, x) * smoothstepf(7.f, 9.5f, std::fabs(y - viewC_.y));
            // the well: a level terrace (no hollows behind the pieces), a little above the valleys around
            const float well = wellX * wellY;
            const float b = lerpf(amp * 16.f * (n - 0.47f), WELL_FLOOR + 1.2f * (n - 0.5f), well) + 3.f * corner;
            base_[(size_t)j * gw_ + i] = b;
            const float away = smoothstepf(12.f, 13.5f, ax) + (1.f - smoothstepf(9.f, 11.f, ax)) * smoothstepf(10.6f, 11.8f, std::fabs(y));
            wood_[(size_t)j * gw_ + i] = saturate((wn - 0.52f) * 5.f + 0.5f) * saturate(away) * smoothstepf(2.2f, 1.4f, b);
        }
    }
}

void TopographyUniverse::splat(std::vector<float>& f, float x, float y, float v) {
    const int i = (int)std::floor(x), j = (int)std::floor(y);
    if (i < 0 || j < 0 || i >= gw_ - 1 || j >= gh_ - 1) return;
    const float fx = x - i, fy = y - j;
    float* r0 = &f[(size_t)j * gw_ + i];
    float* r1 = r0 + gw_;
    r0[0] += v * (1 - fx) * (1 - fy);
    r0[1] += v * fx * (1 - fy);
    r1[0] += v * (1 - fx) * fy;
    r1[1] += v * fx * fy;
}

void TopographyUniverse::shower(float x, float y, float r, float amt, float life) {
    if (showers_.size() > 24) showers_.erase(showers_.begin());
    showers_.push_back({x, y, r, 0.f, life, amt});
}

// The drainage network, from the terrain shown: every node drains to its steepest lower neighbour, the rain
// (a drizzle everywhere, more under the showers) gathers downstream; the rivers carve their beds (stream power,
// kept for the whole song, gentler inside the well).
void TopographyUniverse::drainage(float interval) {
    const int n = cw_ * ch_;
    if (n <= 0) return;
    const float cs = gs_ * 2.f;
    for (int j = 0; j < ch_; j++)
        for (int i = 0; i < cw_; i++) {
            const int fi = std::min(2 * i, gw_ - 1), fj = std::min(2 * j, gh_ - 1);
            const int k = j * cw_ + i;
            nh_[k] = disp_[(size_t)fj * gw_ + fi];
            const float x = gx0_ + fi * gs_, y = gy0_ + fj * gs_;
            float r = drizzle_;
            for (const Shower& sh : showers_) {
                const float env = smoothstepf(0.f, 0.8f, sh.t) * (1.f - smoothstepf(sh.life * 0.5f, sh.life, sh.t));
                const float dx = x - sh.x, dy = y - sh.y;
                r += sh.amt * env * std::exp(-(dx * dx + dy * dy) / (sh.r * sh.r));
            }
            rainF_[k] = r;
        }
    // Priority flood from the sheet's edges: the hollows fill up to their spill point, so every node drains
    // somewhere and the rivers run long and join (instead of dying in every small hollow).
    const int di[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dj[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    fill_.assign(n, 0.f);
    std::vector<char> seen(n, 0);
    using QE = std::pair<float, int>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
    for (int j = 0; j < ch_; j++)
        for (int i = 0; i < cw_; i++)
            if (i == 0 || j == 0 || i == cw_ - 1 || j == ch_ - 1) {
                const int k = j * cw_ + i;
                seen[k] = 1;
                fill_[k] = nh_[k];
                down_[k] = -1;
                pq.push({nh_[k], k});
            }
    while (!pq.empty()) {
        const QE e = pq.top();
        pq.pop();
        const int i = e.second % cw_, j = e.second / cw_;
        for (int d = 0; d < 8; d++) {
            const int ni = i + di[d], nj = j + dj[d];
            if (ni < 0 || nj < 0 || ni >= cw_ || nj >= ch_) continue;
            const int nk = nj * cw_ + ni;
            if (seen[nk]) continue;
            seen[nk] = 1;
            fill_[nk] = std::max(nh_[nk], e.first + 1e-3f);
            down_[nk] = e.second;
            pq.push({fill_[nk], nk});
        }
    }
    // then the steepest way down on the filled terrain (the flood's way across the flats)
    for (int j = 1; j < ch_ - 1; j++)
        for (int i = 1; i < cw_ - 1; i++) {
            const int k = j * cw_ + i;
            float best = 0.f;
            int bk = -1;
            for (int d = 0; d < 8; d++) {
                const int nk = (j + dj[d]) * cw_ + i + di[d];
                const float s = (fill_[k] - fill_[nk]) / (d < 4 ? 1.f : 1.41421f);
                if (s > best) { best = s; bk = nk; }
            }
            if (bk >= 0 && fill_[k] - nh_[k] < 1e-4f) down_[k] = bk;
        }
    // the hollows' depths on the fine grid (how deep a pond would be: the filled terrain minus the ground)
    for (int j = 0; j < gh_; j++)
        for (int i = 0; i < gw_; i++) {
            const int ci = std::min(i / 2, cw_ - 1), cj = std::min(j / 2, ch_ - 1);
            const int ci1 = std::min(ci + 1, cw_ - 1), cj1 = std::min(cj + 1, ch_ - 1);
            const float fx = (i & 1) ? 0.5f : 0.f, fy = (j & 1) ? 0.5f : 0.f;
            auto D = [&](int q) { return fill_[q] - nh_[q]; };
            const float a = lerpf(D(cj * cw_ + ci), D(cj * cw_ + ci1), fx);
            const float b = lerpf(D(cj1 * cw_ + ci), D(cj1 * cw_ + ci1), fx);
            pondT_[(size_t)j * gw_ + i] = lerpf(a, b, fy);
        }
    for (int k = 0; k < n; k++) order_[k] = k;
    std::sort(order_.begin(), order_.end(), [&](int a, int b) { return fill_[a] > fill_[b]; });
    for (int k = 0; k < n; k++) { acc_[k] = rainF_[k]; up_[k] = -1; }
    for (int k : order_)
        if (down_[k] >= 0) acc_[down_[k]] += acc_[k];
    for (int k = 0; k < n; k++) {
        const int d = down_[k];
        if (d >= 0 && (up_[d] < 0 || acc_[k] > acc_[up_[d]])) up_[d] = k;
    }
    // stream power erosion
    for (int k = 0; k < n; k++) {
        const int d = down_[k];
        if (d < 0 || acc_[k] < 6.f) continue;
        const float slope = std::clamp((nh_[k] - nh_[d]) / cs, 0.f, 3.f);
        const int i = k % cw_, j = k / cw_;
        const float x = gx0_ + 2 * i * gs_, y = gy0_ + 2 * j * gs_;
        const bool inWell = std::fabs(x) < 5.4f && std::fabs(y) < 10.4f;
        const float e = 0.0045f * std::sqrt(acc_[k]) * slope * interval * (inWell ? 0.2f : 1.f);
        splat(eros_, 2.f * i + 0.5f, 2.f * j + 0.5f, e);
    }
}

// The rivers as smooth curves through the network (a quadratic from the middle of the main upstream link, through
// the node, to the middle of the downstream link), widening downstream; drawn into a coverage target.
void TopographyUniverse::buildRivers() {
    rivers_.clear();
    const float thr = RIVER_THR;
    auto pos =[&](int k) { return vec2{gx0_ + 2 * (k % cw_) * gs_, gy0_ + 2 * (k / cw_) * gs_}; };
    const float ws = std::max(0.75f, height_ / 1080.f * 1.25f);
    auto width = [&](int k) { return ws * std::min(3.f, 0.8f + 0.6f * std::log2(std::max(1.f, acc_[k] / thr))); };
    for (int k = 0; k < cw_ * ch_; k++) {
        const float a = rvis_[k];
        const int d = down_[k];
        if (a < 0.01f || d < 0) continue;
        const int u = up_[k];
        const bool src = u < 0 || rvis_[u] < 0.01f;
        const vec2 c = pos(k), dn = pos(d);
        const vec2 p0 = src ? c : vec2{(pos(u).x + c.x) * 0.5f, (pos(u).y + c.y) * 0.5f};
        const vec2 p2{(c.x + dn.x) * 0.5f, (c.y + dn.y) * 0.5f};
        const float w0 = src ? 0.25f * width(k) : 0.5f * (width(u) + width(k)), w2 = 0.5f * (width(k) + width(d));
        const float a2 = std::min(a, std::max(rvis_[d], 0.f) + 0.3f);
        vec2 prev = p0;
        float pw = w0;
        for (int s = 1; s <= 4; s++) {
            const float t = s / 4.f, it = 1.f - t;
            const vec2 q{it * it * p0.x + 2 * t * it * c.x + t * t * p2.x, it * it * p0.y + 2 * t * it * c.y + t * t * p2.y};
            const float w = lerpf(w0, w2, t);
            rivers_.push_back({{prev.x, prev.y, q.x, q.y}, {pw, w, lerpf(a, a2, t), 0.f}});
            prev = q;
            pw = w;
        }
    }
}

// A place for a living peak: in the outer margins, clear of the HUD's corners and of the other peaks.
bool TopographyUniverse::placePeak(Peak& p) {
    for (int tries = 0; tries < 30; tries++) {
        const float side = rng_.chance(0.5f) ? -1.f : 1.f;
        const float x = side * rng_.range(13.5f, std::max(13.6f, viewH_.x - 2.f));
        const float y = rng_.range(viewC_.y - viewH_.y + 2.5f, viewC_.y + viewH_.y - 2.5f);
        if (x < -11.f && std::fabs(y - viewC_.y) > 7.f) continue; // the title, the section
        if (x > 11.f && y < viewC_.y - 7.5f) continue;             // the scale bar
        bool ok = true;
        for (const Peak& q : peaks_)
            if (q.wT > 0.f && (q.x - x) * (q.x - x) + (q.y - y) * (q.y - y) < 30.f) ok = false;
        if (!ok) continue;
        p.x = x;
        p.y = y;
        return true;
    }
    return false;
}

// The HUD's text and the previews, as boxes in pixels (padded): the map fades behind them.
void TopographyUniverse::buildHudRects(const std::vector<HudText>& hud) {
    hudRects_.clear();
    auto add = [&](float x0, float y0, float x1, float y1) {
        if (hudRects_.size() / 4 >= 40) return;
        hudRects_.insert(hudRects_.end(), {x0, y0, x1, y1});
    };
    for (const HudText& h : hud) {
        if (h.alpha <= 0.05f || h.text.empty() || h.role == HUD_DIALOG || h.role == HUD_HELP) continue;
        const float base = h.scale * 12.f;
        const bool spaced = h.role == HUD_LABEL || h.role == HUD_SECTION || h.role == HUD_TOAST;
        const float size = base * (h.role == HUD_VALUE ? 1.05f : h.role == HUD_TITLE || h.role == HUD_UNIVERSE ? 1.25f : 0.95f);
        float w = textWidth(fontRoman_, h.text, size, spaced ? base * 0.45f : base * 0.05f);
        if (h.role == HUD_UNIVERSE) w *= 1.25f;
        const float x0 = ugl::hudLeft(h, w, width_), pad = size * 0.35f;
        add(x0 - pad, h.y - pad, x0 + w + pad, h.y + size * 1.1f + pad);
    }
    auto piece = [&](const vec3& c, float sc) {
        const vec2 a = project(vec3(c.x - 2.1f * sc, c.y + 1.4f * sc, 0.f)), b = project(vec3(c.x + 2.1f * sc, c.y - 1.4f * sc, 0.f));
        add(a.x, a.y, b.x, b.y);
    };
    piece(layout::HOLD_CENTER, layout::previewScale(0));
    for (int i = 0; i < 3; i++) piece(layout::nextCenter(i), layout::previewScale(i));
}

float TopographyUniverse::waterPercentile(float p) {
    if (sorted_.empty()) return -99.f;
    if (p <= 0.f) return sorted_.front() - 1.f + p * 10.f;
    if (p >= 1.f) return sorted_.back() + 1.5f;
    const float f = p * (sorted_.size() - 1);
    const size_t i = (size_t)f;
    return lerpf(sorted_[i], sorted_[std::min(i + 1, sorted_.size() - 1)], f - i);
}

void TopographyUniverse::updateTerrain(const Game& game, float dt, float wdt) {
    // The ground drifts on, slowly at a break, a little faster in a chorus and in a drop.
    {
        const float sp = 0.7f + 0.35f * smoothstepf(0.2f, 1.f, lv_) + 0.35f * smoothstepf(1.1f, 2.f, lv_) + 0.6f * dropAmt_;
        evoSpeed_ = approach(evoSpeed_, sp, 0.3f, dt);
        evo_ += wdt * evoSpeed_;
        evolveGround();
        // the rivers' record heals slowly as the land moves under it
        const float heal = std::exp(-dt * 0.012f);
        for (size_t k = 0; k < eros_.size(); k++) { eros_[k] *= heal; sed_[k] *= heal; }
        // the spot heights follow their hilltops
        for (Spot& sp2 : spots_) {
            int i = (int)std::lround((sp2.x - gx0_) / gs_), j = (int)std::lround((sp2.y - gy0_) / gs_);
            if (i < 1 || j < 1 || i >= gw_ - 1 || j >= gh_ - 1) continue;
            int bi = i, bj = j;
            for (int dj = -1; dj <= 1; dj++)
                for (int di = -1; di <= 1; di++)
                    if (base_[(size_t)(j + dj) * gw_ + i + di] > base_[(size_t)bj * gw_ + bi]) { bi = i + di; bj = j + dj; }
            const float tx = gx0_ + bi * gs_, ty = gy0_ + bj * gs_;
            const float k = 1.f - std::exp(-dt * 0.8f);
            if (spotFree(tx, ty)) { sp2.x += (tx - sp2.x) * k; sp2.y += (ty - sp2.y) * k; }
            const int v = (int)std::lround((400.f + base_[(size_t)bj * gw_ + bi] * 100.f) / 5.f) * 5;
            if (std::abs(v - sp2.value) >= 10) sp2.value = v;
        }
    }
    // The stack's mass: each locked cell's coverage of the grid (cells sit higher the deeper they are in the stack,
    // so a tall stack is a mountain and its flanks rise unevenly).
    std::fill(mask_.begin(), mask_.end(), 0.f);
    const float keep = 1.f - over_;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = game.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - game.rowOffset(y));
            const float wgt = (1.f + 0.03f * (Game::H - 1 - y)) * keep;
            const float xa = p.x - 0.5f, xb = p.x + 0.5f, ya = p.y - 0.5f, yb = p.y + 0.5f;
            const int i0 = std::max(0, (int)std::floor((xa - gx0_) / gs_)), i1 = std::min(gw_ - 1, (int)std::ceil((xb - gx0_) / gs_));
            const int j0 = std::max(0, (int)std::floor((ya - gy0_) / gs_)), j1 = std::min(gh_ - 1, (int)std::ceil((yb - gy0_) / gs_));
            for (int j = j0; j <= j1; j++) {
                const float sy = gy0_ + j * gs_;
                const float oy = std::max(0.f, std::min(yb, sy + gs_ * 0.5f) - std::max(ya, sy - gs_ * 0.5f)) / gs_;
                if (oy <= 0.f) continue;
                for (int i = i0; i <= i1; i++) {
                    const float sx = gx0_ + i * gs_;
                    const float ox = std::max(0.f, std::min(xb, sx + gs_ * 0.5f) - std::max(xa, sx - gs_ * 0.5f)) / gs_;
                    if (ox > 0.f) mask_[(size_t)j * gw_ + i] += ox * oy * wgt;
                }
            }
        }
    big_ = mask_;
    boxBlur(mask_, tmp_, gw_, gh_, 2, 3);   // the plateau's flanks
    boxBlur(big_, tmp2_, gw_, gh_, 10, 3);  // the foothills

    // Impacts of hard drops: a small swell of the ground under the piece, settling.
    for (auto& im : impacts_) im.t += dt;
    impacts_.erase(std::remove_if(impacts_.begin(), impacts_.end(), [](const Impact& i) { return i.t > 1.2f; }), impacts_.end());

    const size_t n = target_.size();
    for (size_t k = 0; k < n; k++)
        target_[k] = base_[k] + STACK_H * mask_[k] + (FOOTHILLS + 9.f * heave_) * big_[k] + sed_[k] - eros_[k];
    // the drop's living relief: peaks in the margins following the bands
    for (const Peak& pk : peaks_) {
        const float amp = pk.w * relief_ * (0.8f + 5.f * bandLv_[pk.band]);
        if (amp < 0.01f) continue;
        const int ci = (int)((pk.x - gx0_) / gs_), cj = (int)((pk.y - gy0_) / gs_);
        for (int j = std::max(0, cj - 30); j <= std::min(gh_ - 1, cj + 30); j++)
            for (int i = std::max(0, ci - 30); i <= std::min(gw_ - 1, ci + 30); i++) {
                const float dx = gx0_ + i * gs_ - pk.x, dy = gy0_ + j * gs_ - pk.y;
                target_[(size_t)j * gw_ + i] += amp * std::exp(-(dx * dx + dy * dy) / 6.5f);
            }
    }
    for (const Impact& im : impacts_) {
        const float env = smoothstepf(0.f, 0.06f, im.t) * (1.f - smoothstepf(0.1f, 0.8f, im.t)) * im.amp;
        const int ci = (int)((im.x - gx0_) / gs_), cj = (int)((im.y - gy0_) / gs_);
        for (int j = std::max(0, cj - 10); j <= std::min(gh_ - 1, cj + 10); j++)
            for (int i = std::max(0, ci - 10); i <= std::min(gw_ - 1, ci + 10); i++) {
                const float dx = gx0_ + i * gs_ - im.x, dy = gy0_ + j * gs_ - im.y;
                target_[(size_t)j * gw_ + i] += env * std::exp(-(dx * dx + dy * dy) / 1.6f);
            }
    }

    // The map shown eases toward the terrain (the land rises quickly under a lock, slumps more slowly after a
    // clear): the contours always glide.
    if (fresh_) {
        disp_ = target_;
        fresh_ = false;
    } else {
        const float up = 1.f - std::exp(-dt * 7.f), down = 1.f - std::exp(-dt * 3.2f);
        for (size_t k = 0; k < n; k++) {
            const float d = target_[k] - disp_[k];
            disp_[k] += d * (d > 0.f ? up : down);
        }
    }

    // Water: the network is redrawn a few times a second (the rivers fade in and out where they change), the
    // erosion stays (lightly smoothed so the valleys are round).
    for (auto& sh : showers_) sh.t += wdt;
    showers_.erase(std::remove_if(showers_.begin(), showers_.end(), [](const Shower& x) { return x.t > x.life; }), showers_.end());
    netT_ += wdt;
    if (netT_ > 0.25f) {
        drainage(std::min(netT_, 0.5f));
        netT_ = 0.f;
    }
    {
        const float k = 1.f - std::exp(-dt * 1.3f);
        for (int c = 0; c < cw_ * ch_; c++) {
            const float x = gx0_ + 2 * (c % cw_) * gs_, y = gy0_ + 2 * (c / cw_) * gs_;
            // no river runs behind the pieces: the well stays calm
            const float inWell = std::fabs(x) < 5.4f && std::fabs(y) < 10.4f ? 0.f : 1.f;
            const float t = smoothstepf(RIVER_THR * 0.75f, RIVER_THR * 1.4f, acc_[c]) * inWell * (nh_[c] > water_level_ + 0.1f && fill_[c] - nh_[c] < POND_MIN * 0.5f ? 1.f : 0.f);
            rvis_[c] += (t - rvis_[c]) * k;
        }
    }
    for (size_t k = 0; k < n; k++) eros_[k] = std::min(eros_[k], 2.2f);
    {
        const float k = 1.f - std::exp(-dt * 3.f);
        for (size_t q = 0; q < n; q++) pondL_[q] += (pondT_[q] - pondL_[q]) * k;
    }
    sortT_ += dt;
    if (sortT_ > 0.3f) {
        sortT_ = 0.f;
        tmp_ = eros_;
        boxBlur(tmp_, tmp2_, gw_, gh_, 1, 1);
        for (size_t k = 0; k < n; k++) eros_[k] = eros_[k] * 0.8f + tmp_[k] * 0.2f;
        // the lakes' levels are read from the terrain's own heights (a share of the map under water)
        sorted_.clear();
        for (size_t k = 0; k < n; k += 3) sorted_.push_back(disp_[k]);
        std::sort(sorted_.begin(), sorted_.end());
    }

    // Upload: elevation, water, woodland.
    for (size_t k = 0; k < n; k++) {
        upload_[k * 4 + 0] = disp_[k];
        upload_[k * 4 + 1] = pondL_[k];
        upload_[k * 4 + 2] = wood_[k];
        upload_[k * 4 + 3] = 0.f;
    }
    glBindTexture(GL_TEXTURE_2D, fieldTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, gw_, gh_, 0, GL_RGBA, GL_FLOAT, upload_.data());
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void TopographyUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 1;
        else if (t.hasSong && !t.minor && t.brightness > 0.55f) v = h < 0.5f ? 0 : 4;
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const uint64_t old = seed_;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x70B0ull);
    if (seconds <= 0.f) palSet_ = false;
    if (old != seed_) {
        // a new sheet: new ground, the old rivers' record washed out (the contours glide to it)
        buildGround();
        std::fill(eros_.begin(), eros_.end(), 0.f);
        std::fill(sed_.begin(), sed_.end(), 0.f);
        if (seconds <= 0.f) fresh_ = true;
    }
}

void TopographyUniverse::setLevel(int level, float energy, float seconds, float impact) {
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
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.f, 6.f);
    (void)impact;
}

// The drop: the flood. The water surges up the valleys and the layer tints come in.
void TopographyUniverse::drop(float strength) {
    dropMode_ = true;
    surge_ = std::max(surge_, 0.6f + 0.4f * strength);
    heave_ = std::max(heave_, 0.6f + 0.4f * strength); // the land heaves around the stack
    for (auto& pk : peaks_) pk.w = std::max(pk.w, 0.6f);
}

void TopographyUniverse::refresh(float strength) {
    // a shower over one side of the sheet
    const float side = rng_.chance(0.5f) ? -1.f : 1.f;
    shower(side * rng_.range(13.f, 18.f), rng_.range(-6.f, 8.f), 4.f + 2.f * strength, 1.5f + 2.f * strength);
}

void TopographyUniverse::levelUp() { shower(rng_.chance(0.5f) ? -15.f : 15.f, rng_.range(-5.f, 7.f), 5.f, 2.5f); }

vec2 TopographyUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints TopographyUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = p.paper;
    h.light = p.dark < 0.5f;
    h.ink = p.index;
    h.accent = p.water;
    h.styles = TS_LUMA | TS_RIPPLE | TS_INK;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps one id per locked piece (so two pieces of the same type side by side stay two shapes); when the board was
// changed behind our back (a new universe mid-game), the ids are rebuilt from runs of the same type.
void TopographyUniverse::syncIds(const Game& g) {
    bool ok = true;
    for (int y = 0; y < Game::H && ok; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) { ok = false; break; }
    if (ok) return;
    std::memset(ids_, 0, sizeof(ids_));
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0 || ids_[y][x]) continue;
            // flood the same-type run, at most four cells (a piece)
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

void TopographyUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::HardDrop: {
        if (ev.cells.empty()) break;
        float cx = 0, minY = 99.f;
        for (auto& q : ev.cells) {
            const vec3 p = layout::cellPos((float)q.x, (float)q.y);
            cx += p.x;
            minY = std::min(minY, p.y);
        }
        cx /= ev.cells.size();
        impacts_.push_back({cx, minY - 0.3f, 0.f, 0.5f + 0.5f * std::min(1.f, ev.count / 12.f)});
        break;
    }
    case GameEvent::Clear: {
        // the rows sink into the map: each cell shrinks and fades (merged with its piece's cells in the row)
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
                if (c.y < Game::H - 1 && rows[c.y + 1] && ids_[c.y + 1][c.x] == id) m |= 4;
                if (c.y > 0 && rows[c.y - 1] && ids_[c.y - 1][c.x] == id) m |= 8;
            }
            fading_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f});
        }
        // the debris: fans of sediment against both walls, at the rows' height (kept for the song)
        const bool tetris = ev.count >= 4;
        for (int y = 0; y < Game::H; y++) {
            if (!rows[y]) continue;
            const float wy = layout::cellPos(0.f, (float)y).y;
            for (int s = -1; s <= 1; s += 2) {
                const float fx = s * (6.3f + rng_.range(0.f, 1.2f));
                const int ci = (int)((fx - gx0_) / gs_), cj = (int)((wy - gy0_) / gs_);
                for (int j = std::max(0, cj - 9); j <= std::min(gh_ - 1, cj + 9); j++)
                    for (int i = std::max(0, ci - 9); i <= std::min(gw_ - 1, ci + 9); i++) {
                        const float dx = gx0_ + i * gs_ - fx, dy = gy0_ + j * gs_ - wy;
                        float& sd = sed_[(size_t)j * gw_ + i];
                        sd = std::min(2.f, sd + (tetris ? 0.4f : 0.28f) * std::exp(-(dx * dx * 0.7f + dy * dy) / 1.4f));
                    }
            }
        }
        // compact our ids like the game did
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
        if (tetris) {
            tetrisFlood_ = 1.f;
        }
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
// Geometry
// =====================================================================================================

void TopographyUniverse::cell(float x, float y, float half, int mask, const vec3& fill, float alpha, int kind, float inset,
                              float formA, float shrink, const vec3& line, float extra) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {fill.x, fill.y, fill.z, alpha}, {(float)kind, inset, formA, shrink},
                      {line.x, line.y, line.z, extra}});
}

void TopographyUniverse::buildBoard(const Game& g) {
    auto lineOf = [&](const vec3& c) { return pal_.dark > 0.5f ? c * 0.42f : c * 0.5f; };
    auto pieceCol = [&](int t) { return pal_.piece[std::clamp(t, 0, 6)]; };
    // the mask of a cell's same-piece neighbors (bits: left right down up, down-left down-right up-left up-right;
    // board up = game y - 1)
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    const float form = 0.15f;
    // Locked cells: plateaus. A new piece's form lines spring out from its middle as it settles.
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
            const float age = c.flash > 0.f ? (1.f - c.flash) * Game::LOCK_FX_SECONDS : 9.f;
            const float k = smoothstepf(0.f, 0.45f, age);
            const float inset = lerpf(0.42f, form, k);
            float alpha = 1.f;
            if (overT_ >= 0.f) {
                const float rowFrac = (Game::H - 1 - y) / (float)(Game::H - Game::HIDDEN);
                alpha = 1.f - smoothstepf(rowFrac * 0.8f, rowFrac * 0.8f + 0.2f, over_);
            }
            cell(p.x, p.y, 0.5f, maskOf(x, y, same), pieceCol(c.type), alpha, 0, inset, 1.f, 0.f, lineOf(pieceCol(c.type)));
        }
    // Cleared cells sinking into the map (real time, under half a second).
    for (const Fading& f : fading_) {
        const float a = 1.f - smoothstepf(0.12f, 0.36f, f.t);
        const float sh = 0.3f * smoothstepf(0.f, 0.36f, f.t);
        cell(f.x, f.y, 0.5f, f.mask, pieceCol(f.type), a, 0, form + sh * 0.5f, 1.f - smoothstepf(0.f, 0.2f, f.t), sh,
             lineOf(pieceCol(f.type)));
    }
    // The ghost (a dashed border), the hovering piece's shadow and the piece.
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
                cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), lerp(pieceCol(p.type), pal_.index, 0.25f), 0.85f, 1, 0.f, 0.f, 0.f, vec3(0.f));
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        const vec3 shadowCol = pal_.dark > 0.5f ? vec3(0.f) : pal_.index * 0.3f;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x + 0.14f, w.y - 0.2f, 0.5f, maskOf(c[0], c[1], inPiece), shadowCol, pal_.dark > 0.5f ? 0.55f : 0.16f, 2, 0.f, 0.f, 0.f, vec3(0.f));
        }
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), pieceCol(p.type), 1.f, 0, form, 1.f, 0.f, lineOf(pieceCol(p.type)));
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
            cell(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), pieceCol(type), alpha, 0, form, 1.f, 0.f, lineOf(pieceCol(type)));
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
    // The bonus gauge: a round tarn filling up.
    {
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        cell(gc.x, gc.y, layout::GAUGE_R, 0, pal_.lake, g.bonusReady() ? 1.f : 0.f, 3, 0.f, 1.f, 0.f, pal_.index, fill);
    }
}

// =====================================================================================================
// Text
// =====================================================================================================

float TopographyUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void TopographyUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                                   std::vector<float>& out) const {
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

void TopographyUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    set3f(progText_, "uInk", lerp(ink, bridge_.ink, b));
    set3f(progText_, "uHalo", lerp(pal_.paper, bridge_.color, b));
    set1f(progText_, "uHaloA", 0.85f);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD in TOPOGRAPHY's lettering: a map's. Barlow italic for names (the song, the sheet, like the names of
// waters), small spaced capitals for labels and the section (like the names of regions), the numbers upright.
void TopographyUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        std::vector<float> v;
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal_.accent : pal_.paper;
            const float a = std::min(1.f, r.alpha);
            const float q[6][6] = {{r.x, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y + r.h, c.x, c.y, c.z, a},
                                   {r.x, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y + r.h, c.x, c.y, c.z, a}, {r.x, r.y + r.h, c.x, c.y, c.z, a}};
            for (auto& p : q) v.insert(v.end(), p, p + 6);
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
            name = titleCase(name);
            const float sz = base * 1.25f, vsz = base * 0.62f, vtr = vsz * 0.32f;
            const float wName = textWidth(fontItalic_, name, sz, 0.f);
            const float wVar = var.empty() ? 0.f : textWidth(fontRoman_, var, vsz, vtr);
            const float gap = base * 0.7f;
            const float x = ugl::hudLeft(h, wName + (var.empty() ? 0.f : gap + wVar), width_);
            textQuads(fontItalic_, name, x, h.y - base * 0.1f, sz, 0.f, q);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            drawText(fontItalic_, q, ink, a);
            if (!var.empty()) {
                q.clear();
                textQuads(fontRoman_, var, x + wName + gap, h.y + base * 0.2f, vsz, vtr, q);
                drawText(fontRoman_, q, ink, a * 0.8f);
            }
            continue;
        }
        case HUD_SECTION:
            place(fontRoman_, h.text, base * 0.95f, base * 0.45f);
            drawText(fontRoman_, q, ink, saturate(0.2f + h.alpha * 1.9f));
            continue;
        case HUD_TITLE:
            place(fontItalic_, h.text, base * 1.15f, 0.f, -base * 0.1f);
            drawText(fontItalic_, q, ink, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontRoman_, h.text, base * 0.75f, base * 0.12f);
            drawText(fontRoman_, q, ink, saturate(alpha * 1.2f) * 0.75f);
            continue;
        case HUD_LABEL:
            place(fontRoman_, h.text, base * 0.7f, base * 0.38f);
            drawText(fontRoman_, q, ink, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_VALUE:
            place(fontRoman_, h.text, base * 1.05f, base * 0.05f);
            drawText(fontRoman_, q, ink, alpha);
            continue;
        case HUD_NOTICE:
            place(fontItalic_, h.text, base * 1.1f, 0.f);
            drawText(fontItalic_, q, accent, alpha);
            continue;
        case HUD_KEY:
            place(fontRoman_, h.text, base * 0.85f, base * 0.08f);
            drawText(fontRoman_, q, ink, alpha);
            continue;
        case HUD_TOAST:
            place(fontRoman_, h.text, base * 1.3f, base * 0.5f, -base * 0.15f);
            drawText(fontRoman_, q, ink, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontItalic_, h.text, base * 0.9f, 0.f);
            drawText(fontItalic_, q, accent, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontItalic_, h.text, base * 1.25f, 0.f, -base * 0.1f);
                drawText(fontItalic_, q, ink, alpha);
            } else {
                place(fontRoman_, h.text, base * 0.85f, base * 0.1f);
                drawText(fontRoman_, q, h.accent ? accent : ink, alpha);
            }
            continue;
        default:
            place(fontRoman_, h.text, base * 0.8f, base * 0.06f);
            drawText(fontRoman_, q, h.accent ? accent : ink, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void TopographyUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
    }
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 0.9f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 3.f) : 0.f, overT_ >= 0.f ? 10.f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    worldTime_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    dropAmt_ = approach(dropAmt_, dropMode_ ? 1.f : 0.f, dropMode_ ? 3.f : 0.4f, dt);
    {
        // the drop's colors come in with the flood (and leave slowly after it)
        // (switched quickly on the downbeat, so the pieces never pass through muddy in-between tints)
        colAmt_ = approach(colAmt_, dropMode_ ? wp : 0.f, dropMode_ ? 7.f : 0.5f, dt);
        pal_ = basePal_;
        lerpPalette(pal_, dropPaletteOf(variant_), smoothstepf(0.f, 1.f, colAmt_));
    }
    heave_ = approach(heave_, 0.f, 0.45f, dt);
    half_ = approach(half_, dropAmt_ * wp, 1.5f, dt);
    // the relief that lives with the bands: strong in a drop, a breath in a chorus
    relief_ = approach(relief_, wp * (0.22f + 0.78f * dropAmt_), 1.5f, dt);
    {
        const int lo[3] = {0, 4, 10}, hi[3] = {4, 10, NUM_BANDS};
        for (int g = 0; g < 3; g++) {
            float v = 0.f;
            for (int b = lo[g]; b < hi[g]; b++) v += music.bandsFast[b];
            v /= (float)(hi[g] - lo[g]);
            bandMax_[g] = std::max(bandMax_[g] * std::exp(-dt * 0.05f), std::max(v, 0.02f));
            const float t = saturate(v / bandMax_[g]);
            bandLv_[g] = approach(bandLv_[g], t, t > bandLv_[g] ? 6.f : 2.2f, dt);
        }
    }
    for (auto& pk : peaks_) pk.w = approach(pk.w, pk.wT, 0.8f, dt);
    peaks_.erase(std::remove_if(peaks_.begin(), peaks_.end(), [](const Peak& p) { return p.wT <= 0.f && p.w < 0.01f; }), peaks_.end());
    surge_ = approach(surge_, 0.f, 0.22f, dt);
    tetrisFlood_ = approach(tetrisFlood_, 0.f, 0.2f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (!f.paused) {
        // rain: a drizzle when calm, steady rain in mid parts (showers on the bars), less at the peak
        drizzle_ = (0.45f + 0.9f * wm * (0.6f + 0.6f * energy_)) * (1.f - 0.4f * wp) * (bridge_.amount < 0.5f && overT_ < 0.f ? 1.f : 0.3f);
        if (music.hasSong) {
            const double beat = std::floor(music.beat + 1e-4);
            if (lastBeat_ < 0.0 || beat < lastBeat_ - 8.0) lastBeat_ = beat;
            if (beat > lastBeat_) {
                lastBeat_ = beat;
                beatCount_++;
                const bool bar = std::fmod(beat, 4.0) < 0.5;
                const int bar4 = (int)std::floor(beat / 16.0);
                if (std::fmod(beat, 16.0) < 0.5 && bar4 != lastBar4_ && dropAmt_ > 0.5f) {
                    lastBar4_ = bar4;
                    // the oldest active peak sinks, a new one pushes up elsewhere
                    for (auto& pk : peaks_)
                        if (pk.wT > 0.f) { pk.wT = 0.f; break; }
                    Peak np{0, 0, rng_.irange(0, 2), 0.f, 1.f};
                    if (placePeak(np)) peaks_.push_back(np);
                }
                if (bar && wm > 0.5f && wp < 0.5f && overT_ < 0.f) {
                    const float side = rng_.chance(0.5f) ? -1.f : 1.f;
                    shower(side * rng_.range(12.5f, viewH_.x - 2.f), rng_.range(-8.f, 9.f), rng_.range(2.5f, 4.f), rng_.range(1.f, 2.f));
                }
            }
        }
        phase_ += wdt * (0.1f + 0.2f * wp + (1.2f + 1.6f * dropAmt_) * kick * wp * (0.4f + 0.6f * dropAmt_));
        flow_ += wdt * (0.3f + 0.4f * wm) * (1.f + 2.f * dropAmt_) * std::clamp(music.bpm / 120.f, 0.6f, 1.6f);
    }
    // the lakes: a share of the sheet under water (none when calm, pools in mid parts, the flood at the peak,
    // swelling slowly over two bars)
    {
        float p;
        if (lv_ <= 1.f) p = lerpf(-0.05f, 0.025f, lv_);
        else p = lerpf(0.025f, 0.16f + 0.12f * dropAmt_, lv_ - 1.f);
        p += 0.12f * surge_ + 0.16f * tetrisFlood_;
        if (overT_ >= 0.f) p = lerpf(p, 1.1f, over_);
        float W = waterPercentile(p);
        // the drop floods past the well's terrace: the stack stands as an island
        W = std::max(W, lerpf(-99.f, WELL_FLOOR + 0.6f, dropAmt_ * wp));
        W = std::max(W, lerpf(-99.f, WELL_FLOOR + 0.7f, tetrisFlood_));
        if (music.hasSong) W += 0.3f * wp * (0.5f - 0.5f * std::cos((float)(TAU * std::fmod(music.beat, 8.0) / 8.0)));
        if (water_level_ < -90.f) water_level_ = W;
        const float rate = W > water_level_ ? 2.2f : 0.9f;
        water_level_ += (W - water_level_) * (1.f - std::exp(-dt * rate));
    }
    tint_ = approach(tint_, 0.1f * wm + wp * (0.3f + 0.35f * dropAmt_), 1.2f + 2.f * dropAmt_, dt);
    riverVis_ = approach(riverVis_, 0.6f + 0.4f * wm, 1.f, dt);
    pond_ = approach(pond_, smoothstepf(0.35f, 1.f, lv_) * (overT_ < 0.f ? 1.f : 0.f), 0.25f, dt); // the hollows fill slowly
    for (auto& fd : fading_) fd.t += dt; // a clear runs in real time, even in slow motion
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > 0.4f; }), fading_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    updateTerrain(game, f.paused ? 0.f : dt, wdt);

    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float s = height_ / 1080.f;

    // ---- The rivers' coverage.
    buildRivers();
    riverT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!rivers_.empty()) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_MAX);
        glUseProgram(progRiver_);
        setMat(progRiver_, "uVP", vp_);
        set1f(progRiver_, "uPxW", 2.f * viewH_.y / height_);
        glBindVertexArray(riverVao_);
        glBindBuffer(GL_ARRAY_BUFFER, riverVbo_);
        glBufferData(GL_ARRAY_BUFFER, rivers_.size() * sizeof(RiverInst), rivers_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)rivers_.size());
        glBlendEquation(GL_FUNC_ADD);
        glDisable(GL_BLEND);
    }

    // ---- The map.
    sceneT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(progMap_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fieldTex_);
    set1i(progMap_, "uField", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, riverT_.tex);
    set1i(progMap_, "uRivers", 1);
    glActiveTexture(GL_TEXTURE0);
    set4f(progMap_, "uGrid", gx0_, gy0_, 1.f / gs_, 0.f);
    set4f(progMap_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progMap_, "uRes", (float)width_, (float)height_);
    set1f(progMap_, "uLineW", 1.05f * s);
    set3f(progMap_, "uPaper", pal_.paper);
    set3f(progMap_, "uLine", pal_.line);
    set3f(progMap_, "uIndex", pal_.index);
    set3f(progMap_, "uWaterLine", pal_.water);
    set3f(progMap_, "uLake", pal_.lake);
    set3f(progMap_, "uTint0", pal_.tint0);
    set3f(progMap_, "uTint1", pal_.tint1);
    set3f(progMap_, "uTint2", pal_.tint2);
    set3f(progMap_, "uWood", pal_.wood);
    set1f(progMap_, "uLineAlpha", pal_.lineAlpha);
    set1f(progMap_, "uTintAmt", tint_);
    set1f(progMap_, "uShade", pal_.shade * (0.35f + 0.4f * wm + 0.25f * wp) + 0.3f * slow_);
    set1f(progMap_, "uWoodAmt", pal_.woodAmt * (0.6f + 0.4f * wm) * (1.f - 0.6f * tint_));
    set1f(progMap_, "uHMin", -3.f);
    set1f(progMap_, "uHMax", 9.f);
    set1f(progMap_, "uWater", water_level_);
    set1f(progMap_, "uWaterOn", 1.f);
    set1f(progMap_, "uPond", pond_);
    set1f(progMap_, "uPondMin", POND_MIN);
    set1f(progMap_, "uPhase", phase_);
    set1f(progMap_, "uRiver", riverVis_ * (1.f - over_));
    set1f(progMap_, "uFlow", flow_);
    set1f(progMap_, "uWellCalm", 0.6f);
    set1f(progMap_, "uSlow", slow_);
    set1f(progMap_, "uDark", pal_.dark);
    set1f(progMap_, "uFurn", 1.f - over_ * 0.7f);
    set1f(progMap_, "uBridge", bridge);
    set3f(progMap_, "uBridgeCol", bridge_.color);
    buildHudRects(f.hud);
    set1i(progMap_, "uHudN", (int)(hudRects_.size() / 4));
    if (!hudRects_.empty()) glUniform4fv(ugl::U(progMap_, "uHud"), (GLsizei)(hudRects_.size() / 4), hudRects_.data());
    set1f(progMap_, "uHalf", half_);
    set1f(progMap_, "uDropFx", dropAmt_ * wp);
    set1f(progMap_, "uShoreKick", std::max(music.kick, kickIn_) * dropAmt_ * wp);
    set4f(progMap_, "uLeftCol", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
    set4f(progMap_, "uRightCol", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    ugl::drawFullscreen();

    // ---- Pieces, ghost, previews, gauge.
    cells_.clear();
    buildBoard(game);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!cells_.empty()) {
        // shadows first, then the rest in order
        std::stable_sort(cells_.begin(), cells_.end(), [](const CellInst& a, const CellInst& b) {
            return (a.c[0] == 2.f ? 0 : 1) < (b.c[0] == 2.f ? 0 : 1);
        });
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uLineW", std::max(0.9f, 1.15f * s));
        set3f(progCell_, "uLake", pal_.lake);
        set3f(progCell_, "uWaterLine", pal_.water);
        set3f(progCell_, "uRing", pal_.index);
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    // ---- Spot heights and the HUD.
    {
        const float a = 0.75f * (1.f - saturate(bridge_.amount * 2.f)) * (1.f - over_);
        if (a > 0.01f) {
            std::vector<float> dots, nums;
            for (const Spot& sp : spots_) {
                if (!spotFree(sp.x, sp.y)) continue;
                const vec2 p = project(vec3(sp.x, sp.y, 0.f));
                const float ts = std::max(9.f, 17.f * s);
                textQuads(fontRoman_, ".", p.x - ts * 0.15f, p.y - ts * 1.25f, ts * 1.8f, 0.f, dots);
                textQuads(fontItalic_, std::to_string(sp.value), p.x + ts * 0.45f, p.y - ts * 0.3f, ts, 0.f, nums);
            }
            drawText(fontRoman_, dots, pal_.index, a);
            drawText(fontItalic_, nums, pal_.index, a);
        }
    }
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

const UniverseEntry& universe_topography() {
    static const UniverseEntry e{
        "TOPOGRAPHY", "a contour map whose terrain is the stack, rivers in the rain, a flood on the drop",
        [] { return std::unique_ptr<Universe>(new TopographyUniverse()); },
        // Songs with a build and a drop suit the flood best; it fits most songs.
        [](const SongLook& l) { return 0.75f + 0.3f * l.dynamics + 0.15f * (1.f - l.airWeight); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
