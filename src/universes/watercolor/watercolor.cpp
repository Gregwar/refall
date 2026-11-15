// WATERCOLOR: wet watercolor on cold-press paper.
//
// One world: a sheet of cold-press paper with a landscape painted on it in a few loose, transparent washes (a
// graded wet-in-wet sky, a far range fading into mist, a crisp middle range with dried darker rims, water, fields or
// trees in front), the painting vignetted on the white sheet with ragged edges. The well is masked out (clean paper,
// a pencil frame drawn lightly around it); the pieces are transparent pigment washes: the cells of one color merge
// into one wash, they granulate in the paper's valleys, land wet (softer, darker, no rim) and dry with a dark rim,
// and two colors overlapping glaze darker. The ghost is the pencil underdrawing of where the wash will go.
//
// The song paints: calm is a quick sketch (a pale sky band and a far range on a mostly white sheet), mid fills the
// painting in (the middle range, the foreground and the trees come in as washes spreading, the sky reaches higher)
// and the beats drop pigment into the wet sky where it blooms; peak glazes everything deeper and more saturated,
// the sky up to the top of the sheet, bigger drops, and the kicks flick spatter. A cleared row's pigment dissolves
// back into the paper in a moment (nothing painted over the row, nothing left behind), a quad floods the sheet with
// water (a great backrun blooming out from the well) and a fresh glaze, slow motion is a night glaze of indigo
// with the moon lifted out of it, game over tilts the wet sheet (the stack runs down in drips and the painting is
// washed back to paper). A change of universe washes the sheet back to plain paper from the well outward (and
// paints it in from the edges when entering).
//
// Around a song: in the setup one loaded brush lays the washes down the white sheet from the top, a wet bead at its
// front; in the teardown the sheet tilts like at a game over, the washes running down and lifted back to paper.
//
// Cost: the landscape is drawn at a reduced resolution (at most 540 lines), the wet layer is a small pigment and
// water simulation at most 360 lines high, a few steps a frame; the paper is rendered once per size.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane (like SUMI).
#include "universes/watercolor/watercolor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/watercolor/shaders.hpp"

namespace {

// Yrsa (Italic for the titles, Medium for labels and numbers), SIL Open Font License, see OFL-Yrsa.txt.
const unsigned char FONT_ITALIC[] = {
#include "universes/watercolor/Yrsa-Italic.ttf.inc"
};
const unsigned char FONT_ROMAN[] = {
#include "universes/watercolor/Yrsa-Medium.ttf.inc"
};

namespace shaders = watercolor_shaders;
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

constexpr float WELL_X = 5.3f, WELL_Y = 10.3f; // the masked well (half extents, board)

vec2 v2(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// The variants: each a painting with its own pigments (sRGB: the color of a full wash on white paper). Colors: the
// paper, the HUD's ink, the sky's top and low washes, the sun (painted), the far range, the middle range, the
// foreground, the trees, the accent (drops, quad); the seven piece pigments (I O T S Z J L). Numbers: the horizon
// (board y), the sky's reach above it at the peak, the sun (x, y, radius, lifted out 0..1, painted 0..1), the far
// range (height, roughness), the middle range (height, roughness), the foreground (0 meadow, 1 water, 2 rows), the
// trees (0 none, 1 conifers, 2 round, 3 blossom), granulation, wetness (soft edges), body color (gouache on toned
// paper).
struct VariantDef {
    const char* name;
    unsigned paper, ink, skyTop, skyLow, sun, far, mid, near, tree, accent;
    unsigned piece[7];
    float horizon, reach, sunX, sunY, sunR, sunLift, sunPaint, farH, farRough, midH, midRough, nearType, treeType,
        gran, wet, body;
};
const VariantDef VARIANTS[] = {
    // A Turner sunset over an estuary: Indian yellow and vermilion, a lifted sun, violet-grey hills, still water.
    {"TURNER SUNSET", 0xf7f0e2, 0x4a3426, 0xe8b45c, 0xe3784e, 0xf2c25a, 0x9a86a6, 0x7d5a58, 0x7f8aa0, 0x5a4a50, 0xd8552e,
     {0x5b8fb0, 0xe9a52a, 0xb4506a, 0x9a9a3a, 0xd8402a, 0x6a5a8a, 0xe8823a},
     -4.6f, 16.5f, -15.0f, -2.6f, 1.7f, 0.65f, 0.45f, 3.6f, 0.3f, 1.7f, 0.4f, 1.f, 0.f, 0.35f, 0.7f, 0.f},
    // A Nordic fjord: cerulean and Payne's grey, steep walls mirrored in cold water, a pale low sun.
    {"NORDIC FJORD", 0xf3f4f2, 0x26303c, 0x6b9cc8, 0xc9d5dc, 0xffffff, 0x6d7f98, 0x3a5062, 0x56788a, 0x2e4450, 0x2a7f8e,
     {0x4aa8c8, 0xc9b47a, 0x7a6aa0, 0x3e8a7a, 0xa84a5a, 0x2e4a7a, 0xb88a6a},
     -4.4f, 15.5f, 14.0f, 1.6f, 1.1f, 1.0f, 0.f, 6.4f, 0.85f, 2.6f, 0.75f, 1.f, 1.f, 0.55f, 0.35f, 0.f},
    // Provence: cobalt sky over Naples yellow, a painted sun, lilac hills and rows of lavender.
    {"PROVENCE LAVENDER", 0xf8f3e8, 0x3e3452, 0x86a9dc, 0xf0d6a0, 0xf3c24c, 0xa89cc6, 0x8c9a5c, 0x8462b8, 0x55603a, 0x9a62c8,
     {0x7aa8e0, 0xf2d060, 0x9c6ad0, 0x9ab060, 0xe07a8a, 0x5a6ac0, 0xe89a58},
     -4.8f, 16.5f, 15.5f, 4.2f, 1.25f, 0.3f, 0.85f, 2.4f, 0.1f, 1.5f, 0.2f, 2.f, 0.f, 0.3f, 0.6f, 0.f},
    // Monsoon: a wet grey-green sky, misty layered hills in Hooker's green, terraces and round trees.
    {"MONSOON GREENS", 0xf2f2ea, 0x22342c, 0x93a8a0, 0xdde2d0, 0xffffff, 0x8aa498, 0x4c7a5c, 0x6e9440, 0x2f5a3c, 0x2f9070,
     {0x2fa0a0, 0xd8b030, 0x6a7aa0, 0x4a9a40, 0xa8503a, 0x2a5a6a, 0xb88a30},
     -4.2f, 16.f, 0.f, 0.f, 0.01f, 0.f, 0.f, 3.4f, 0.2f, 2.4f, 0.35f, 0.f, 2.f, 0.3f, 1.0f, 0.f},
    // A Payne's grey storm over the sea: heavy wet clouds, a band of lifted light, a dark coast.
    {"STORM GREY", 0xf1f0ec, 0x22252c, 0x48546a, 0xaab2b8, 0xffffff, 0x58647a, 0x343c48, 0x5e707e, 0x262c34, 0xd2a640,
     {0x6f9ab4, 0xc8a450, 0x8a7898, 0x7a9070, 0xa85048, 0x4a5670, 0xb07a50},
     -4.0f, 16.5f, -14.0f, -2.0f, 2.6f, 1.0f, 0.f, 1.6f, 0.5f, 1.4f, 0.3f, 1.f, 0.f, 0.6f, 0.9f, 0.f},
    // Cherry spring: soft cerulean and blush, pale green hills, blossoming trees.
    {"CHERRY SPRING", 0xf9f5f1, 0x5a3848, 0xa2c4e4, 0xf4d4dc, 0xffffff, 0xb2c6ae, 0x92b474, 0x9cbc78, 0x6a4a4a, 0xe2789c,
     {0x80c0e0, 0xf2d870, 0xc898d8, 0x98cc78, 0xec7ca0, 0x7890d8, 0xf4a888},
     -4.6f, 15.5f, 15.0f, 4.8f, 1.2f, 0.7f, 0.f, 2.2f, 0.15f, 1.3f, 0.3f, 0.f, 3.f, 0.25f, 0.75f, 0.f},
    // Autumn earth: raw and burnt sienna hills under a low painted sun, ochre fields, rust trees.
    {"AUTUMN EARTH", 0xf5ecdc, 0x3c2a1e, 0x9fb2c2, 0xeecc96, 0xe6a43e, 0xa6988a, 0xa8602e, 0x9a7a30, 0x9a3e22, 0xcc5626,
     {0x5a8090, 0xd8a030, 0x7a3a4a, 0x6a7a2a, 0xb83a20, 0x3a3a50, 0xd06a20},
     -4.5f, 15.5f, -15.5f, -1.2f, 1.5f, 0.35f, 0.75f, 2.8f, 0.3f, 2.0f, 0.4f, 0.f, 2.f, 0.45f, 0.6f, 0.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = WatercolorUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.paper = L(d.paper); p.ink = L(d.ink); p.skyTop = L(d.skyTop); p.skyLow = L(d.skyLow); p.sun = L(d.sun);
    p.far = L(d.far); p.mid = L(d.mid); p.near = L(d.near); p.tree = L(d.tree); p.accent = L(d.accent);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    p.horizon = d.horizon; p.reach = d.reach; p.sunX = d.sunX; p.sunY = d.sunY; p.sunR = d.sunR; p.sunLift = d.sunLift;
    p.sunPaint = d.sunPaint; p.farH = d.farH; p.farRough = d.farRough; p.midH = d.midH; p.midRough = d.midRough;
    p.nearType = d.nearType; p.treeType = d.treeType; p.gran = d.gran; p.wet = d.wet; p.body = d.body;
    return p;
}

void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

// A pigment's absorbance from the color of its full wash on white paper (relative to the paper).
vec3 absorb(const vec3& c, const vec3& paper) {
    auto k = [](float a, float p) { return -std::log(std::clamp(a / std::max(p, 1e-3f), 0.004f, 1.f)); };
    return {k(c.x, paper.x), k(c.y, paper.y), k(c.z, paper.z)};
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool WatercolorUniverse::init(int width, int height) {
    progPaper_ = compile(ugl::FULLSCREEN_VS, shaders::PAPER_FS, "watercolor paper");
    progLand_ = compile(ugl::FULLSCREEN_VS, shaders::LAND_FS, "watercolor land");
    progBloom_ = compile(shaders::BLOOM_VS, shaders::BLOOM_FS, "watercolor bloom");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMPOSITE_FS, "watercolor composite");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "watercolor cell");
    progPencil_ = compile(shaders::PENCIL_VS, shaders::PENCIL_FS, "watercolor pencil");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "watercolor shape");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "watercolor post");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, int vec4s, size_t stride) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < vec4s; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(cellVao_, cellVbo_, 4, sizeof(CellInst));
    instanced(lineVao_, lineVbo_, 2, sizeof(LineInst));
    instanced(bloomVao_, bloomVbo_, 3, sizeof(DropInst));

    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)(sizeof(float) * 2));

    glBindVertexArray(0);

    // the HUD's ink: a transparent pigment, absorbed in linear light as the washes are (on the final picture: the
    // ink to the power of its strength), a touch darker at the edges where it dried; white gouache on the toned
    // paper (uP0.x)
    ink_.glsl = R"(
vec4 letter(Letter L) {
    float a = L.std.a * (0.86 + 0.14 * (1.0 - smoothstep(0.0, 3.0, L.dist)));
    if (uP0.x > 0.5) return vec4(L.ink.rgb, 1.0) * pow(a, 0.7); // (light over the darker paper, in linear light)
    vec3 F = pow(max(L.ink.rgb, vec3(0.003)), vec3(a)); // what the paper keeps
    float k = max(a, 1.0 - min(F.r, min(F.g, F.b)));      // (multiplied in as F - 1 + k + (1 - k))
    return vec4(F - 1.0 + k, k);
}
)";

    buildNoise();
    pal_ = paletteOf(0);
    for (auto& b : backruns_) b = vec4(0.f, 0.f, 1.f, 0.f);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

// A tileable 256x256 noise texture: r, g, b gradient noise (periods 8, 16, 32 across it), a the cold-press
// relief (round domes of mixed sizes on a jittered grid).
void WatercolorUniverse::buildNoise() {
    const int N = 256;
    std::vector<unsigned char> px(N * N * 4);
    auto hash2 = [](int x, int y, int s) {
        uint64_t h = splitmix64((uint64_t)(uint32_t)x * 73856093ull ^ (uint64_t)(uint32_t)y * 19349663ull ^ (uint64_t)s * 83492791ull);
        return (h >> 40) * (1.0f / 16777216.0f);
    };
    auto perlin = [&](float x, float y, int period, int seed) {
        const int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
        const float fx = x - x0, fy = y - y0;
        auto grad = [&](int ix, int iy, float dx, float dy) {
            const float a = hash2(((ix % period) + period) % period, ((iy % period) + period) % period, seed) * TAU;
            return std::cos(a) * dx + std::sin(a) * dy;
        };
        auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
        const float n00 = grad(x0, y0, fx, fy), n10 = grad(x0 + 1, y0, fx - 1, fy);
        const float n01 = grad(x0, y0 + 1, fx, fy - 1), n11 = grad(x0 + 1, y0 + 1, fx - 1, fy - 1);
        const float u = fade(fx), v = fade(fy);
        return lerpf(lerpf(n00, n10, u), lerpf(n01, n11, u), v);
    };
    auto domes = [&](float x, float y, int cells, int seed) {
        const int cx = (int)std::floor(x), cy = (int)std::floor(y);
        float best = 0.f;
        for (int j = -2; j <= 2; j++)
            for (int i = -2; i <= 2; i++) {
                const int ix = cx + i, iy = cy + j;
                const int wx = ((ix % cells) + cells) % cells, wy = ((iy % cells) + cells) % cells;
                const float qx = ix + 0.1f + 0.8f * hash2(wx, wy, seed), qy = iy + 0.1f + 0.8f * hash2(wx, wy, seed + 1);
                const float R = 0.6f + 0.7f * hash2(wx, wy, seed + 2);
                const float d2 = ((x - qx) * (x - qx) * 1.3f + (y - qy) * (y - qy)) / (R * R);
                if (d2 < 1.f) best = std::max(best, std::sqrt(1.f - d2) * (0.7f + 0.3f * R));
            }
        return best;
    };
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = (x + 0.5f) / N, v = (y + 0.5f) / N;
            const float r = 0.5f + 0.5f * (perlin(u * 8, v * 8, 8, 11) * 0.75f + perlin(u * 16, v * 16, 16, 12) * 0.3f);
            const float g = 0.5f + 0.6f * perlin(u * 16, v * 16, 16, 21);
            const float b = 0.5f + 0.6f * perlin(u * 32, v * 32, 32, 31);
            const float a = domes(u * 32, v * 32, 32, 41) * 0.7f + domes(u * 64, v * 64, 64, 51) * 0.3f;
            unsigned char* p = &px[(y * N + x) * 4];
            p[0] = (unsigned char)std::clamp((int)(r * 255.f + 0.5f), 0, 255);
            p[1] = (unsigned char)std::clamp((int)(g * 255.f + 0.5f), 0, 255);
            p[2] = (unsigned char)std::clamp((int)(b * 255.f + 0.5f), 0, 255);
            p[3] = (unsigned char)std::clamp((int)(a * 255.f + 0.5f), 0, 255);
        }
    glGenTextures(1, &noiseTex_);
    glBindTexture(GL_TEXTURE_2D, noiseTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void WatercolorUniverse::bindNoise(GLuint prog, int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, noiseTex_);
    set1i(prog, "uNoise", unit);
}

void WatercolorUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = v2(hw, hh);
    viewC_ = v2(cam.target.x, cam.target.y);
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    px_ = 2.f * hh / height_;
    createTargets();
}

void WatercolorUniverse::createTargets() {
    paperT_.create(width_, height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    const int lh = std::max(90, std::min(540, height_ / 2));
    const int lw = std::max(160, (int)std::lround((float)lh * width_ / height_));
    landT_.create(lw, lh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int wh = std::max(90, std::min(540, height_ / 2));
    const int ww = std::max(160, (int)std::lround((float)wh * width_ / height_));
    wetT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    // the paper, once
    paperT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progPaper_);
    bindNoise(progPaper_, 0);
    set1f(progPaper_, "uScale", px_);
    ugl::drawFullscreen();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

WatercolorUniverse::~WatercolorUniverse() {
    paperT_.destroy();
    landT_.destroy();
    wetT_.destroy();
    sceneT_.destroy();
    for (GLuint p : {progPaper_, progLand_, progBloom_, progComp_, progCell_, progPencil_, progShape_, progPost_})
        glDeleteProgram(p);
    for (GLuint v : {cellVao_, lineVao_, bloomVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, lineVbo_, bloomVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
    if (noiseTex_) glDeleteTextures(1, &noiseTex_);
}

// =====================================================================================================
// Song, level, bonus
// =====================================================================================================

// The painting from the song: dark minor songs paint the cold fjord (or the storm when they are dynamic), bright
// major ones a sunny day; the key's hue picks among the rest.
int WatercolorUniverse::pickVariant(const SongLook& t) const {
    float h = std::fmod(t.hue / TAU, 1.f);
    if (h < 0) h += 1.f;
    const bool night = t.hasSong && t.minor && t.brightness < 0.42f;
    const bool bright = t.hasSong && !t.minor && t.brightness > 0.55f;
    int v;
    if (night) v = t.dynamics > 0.6f ? 4 : 1;
    else if (bright) v = h < 0.33f ? 0 : (h < 0.66f ? 2 : 5);
    else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
    if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    return v;
}

void WatercolorUniverse::setSong(const SongLook& look, float seconds) {
    variant_ = look.variant >= 0 ? look.variant % VARIANT_COUNT : pickVariant(look);
    seed_ = look.seed;
    seedF_ = (float)(look.seed % 1000) * 0.137f;
    rng_ = Rng(look.seed ^ 0xAC0Bull);
    // two or three backruns in the sky, where they show (outside the well and the columns)
    for (int i = 0; i < 3; i++) {
        const float side = (i & 1) ? 1.f : -1.f;
        backruns_[i] = vec4(side * rng_.range(12.5f, 19.f), rng_.range(-1.f, 8.f), rng_.range(0.9f, 1.8f), i < 2 ? 1.f : (rng_.chance(0.5f) ? 0.8f : 0.f));
    }
    if (seconds <= 0.f) palSet_ = false;
    else {
        // a new song in the same painting: a few fresh drops of the new pigments
        for (int i = 0; i < 4; i++) drop(skySpot(true), rng_.range(1.f, 1.6f), 14.f, dropColor(0.5f), 0.5f);
    }
}

void WatercolorUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
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
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 2.5f, 6.f);
    if (rising && impact > 0.3f) {
        // into a drop: a splash of water and pigment on both sides
        for (int i = 0; i < 3; i++) drop(skySpot(true), rng_.range(1.2f, 2.0f) * (0.7f + 0.5f * impact), 16.f, dropColor(1.f), 0.7f);
    }
}

void WatercolorUniverse::refresh(float strength) {
    const int n = 1 + (int)(strength * 2.5f);
    for (int i = 0; i < n; i++) drop(skySpot(false), rng_.range(0.7f, 1.2f), 12.f, dropColor(0.3f), 0.35f * strength + 0.1f);
}

void WatercolorUniverse::setBonus(bool on) { slowOn_ = on; }

void WatercolorUniverse::levelUp() {
    for (int i = 0; i < 2; i++) drop(skySpot(true), rng_.range(1.0f, 1.5f), 14.f, absorb(pal_.accent, pal_.paper), 0.6f);
}

vec2 WatercolorUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints WatercolorUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    // the sheet seen from afar: mostly paper, a touch of the sky's wash
    h.background = lerp(p.paper, p.paper * p.skyLow, 0.35f);
    h.light = ugl::luma(h.background) > 0.28f;
    h.ink = p.ink;
    h.accent = p.accent;
    h.styles = TS_INK | TS_LUMA | TS_RIPPLE; // a wet front, the darks first, water
    return h;
}

vec3 WatercolorUniverse::pieceK(int type) const { return absorb(pal_.piece[std::clamp(type, 0, 6)], pal_.paper); }

// A drop's pigment: the sky's own colors when calm, more of the accent and the sun as it rises.
vec3 WatercolorUniverse::dropColor(float wp) {
    const float r = rng_.uniform();
    vec3 c;
    if (r < 0.35f - 0.15f * wp) c = pal_.skyTop;
    else if (r < 0.6f - 0.15f * wp) c = pal_.skyLow;
    else if (r < 0.75f) c = lerp(pal_.paper, pal_.accent, 0.75f);
    else c = pal_.sunPaint > 0.3f ? pal_.sun : pal_.far;
    return absorb(c, pal_.paper);
}

// Somewhere in the painted sky, away from the well and the HUD columns: beside the columns, or above them.
vec2 WatercolorUniverse::skySpot(bool big) {
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const float top = std::min(viewC_.y + viewH_.y - 1.f, pal_.horizon + pal_.reach * (0.42f + 0.33f * wm + 0.25f * wp) - 1.f);
    const float edge = viewH_.x - (big ? 2.2f : 1.6f);
    for (int tries = 0; tries < 8; tries++) {
        const float side = rng_.chance(0.5f) ? -1.f : 1.f;
        vec2 p;
        if (rng_.chance(0.8f) || top < 11.2f) p = v2(side * rng_.range(12.2f, std::max(12.4f, edge)), rng_.range(pal_.horizon + 1.5f, std::max(pal_.horizon + 2.f, top)));
        else p = v2(rng_.range(-edge, edge), rng_.range(11.f, std::max(11.2f, top)));
        if (std::fabs(p.x) < WELL_X + 1.f && p.y < WELL_Y + 0.8f) continue;
        if (std::fabs(p.x) < 11.6f && std::fabs(p.x) > 5.6f && p.y < 10.8f) continue;
        return p;
    }
    return v2((rng_.chance(0.5f) ? -1.f : 1.f) * 15.f, pal_.horizon + 4.f);
}

void WatercolorUniverse::drop(vec2 p, float r, float life, const vec3& K, float strength, int kind) {
    if (blooms_.size() > 160) blooms_.erase(blooms_.begin());
    blooms_.push_back({p, r, 0.f, life, K, strength, rng_.range(0.f, 50.f), kind});
}

// =====================================================================================================
// Game events
// =====================================================================================================

void WatercolorUniverse::onEvent(const GameEvent& ev, const Game&) {
    switch (ev.type) {
    case GameEvent::HardDrop: {
        if (ev.count < 4) break;
        // a hard landing flicks a few drops of the piece's pigment
        int type = ev.cells.empty() ? 0 : ev.cells[0].type;
        float minY = 99.f, cx = 0.f;
        for (auto& q : ev.cells) { minY = std::min(minY, layout::cellPos((float)q.x, (float)q.y).y); cx += layout::cellPos((float)q.x, (float)q.y).x; }
        cx /= std::max<size_t>(1, ev.cells.size());
        const int n = std::min(2 + ev.count / 4, 5);
        for (int i = 0; i < n; i++) {
            Dot d;
            const float a = rng_.range(0.15f, PI - 0.15f);
            const float dist = rng_.range(0.6f, 1.3f);
            d.p = v2(cx + std::cos(a) * dist * 1.4f, minY - 0.5f + std::sin(a) * dist * 0.4f);
            if (std::fabs(d.p.x) > 5.f || d.p.y < -10.f) continue;
            d.r = rng_.range(0.05f, 0.09f); d.t = 0.f; d.life = rng_.range(0.6f, 0.9f); d.K = pieceK(type);
            dots_.push_back(d);
        }
        break;
    }
    case GameEvent::Clear: {
        const bool quadClear = ev.count >= 4;
        // each cleared cell's pigment dissolves back into the paper, in a moment; nothing is painted over the row
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            Lifted l;
            l.x = p.x; l.y = p.y; l.t = 0.f; l.type = std::clamp(c.type, 0, 6);
            l.delay = rng_.range(0.f, 0.06f);
            lifted_.push_back(l);
        }
        if (quadClear) {
            // water flooded onto the sheet: a great backrun out from the well, and a fresh glaze of the accent
            floodT_ = 0.f;
            floodK_ = absorb(pal_.accent, pal_.paper);
            for (int s = -1; s <= 1; s += 2) {
                drop(v2(s * 14.f, pal_.horizon + 3.5f), 7.5f, 11.f, floodK_, 0.5f, 1);
                for (int i = 0; i < 3; i++)
                    drop(v2(s * rng_.range(12.5f, 18.f), rng_.range(pal_.horizon + 1.f, pal_.horizon + 9.f)), rng_.range(1.0f, 1.6f), 16.f,
                         i == 0 ? floodK_ : dropColor(1.f), 0.8f);
            }
        }
        break;
    }
    case GameEvent::TopOut: {
        // the wet sheet tilted: the stack runs down in drips and is washed away
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            running_.push_back({p.x, p.y, 0.f, (Game::H - 1 - c.y) * 0.02f + rng_.range(0.f, 0.15f), std::clamp(c.type, 0, 6), rng_.uniform()});
        }
        overT_ = 0.f;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Simulation
// =====================================================================================================

void WatercolorUniverse::simulate(const Game& game, const MusicState& music, float dt, float wdt) {
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);

    // The beats drop pigment into the wet sky: a pale drop on each bar when calm, every beat from mid, bigger and
    // more saturated at the peak.
    if (music.hasSong) {
        const double beat = std::floor(music.beat + 1e-4);
        if (lastBeat_ < 0.0 || beat < lastBeat_ - 8.0) lastBeat_ = beat;
        if (beat > lastBeat_) {
            lastBeat_ = beat;
            beatCount_++;
            const bool down = std::fmod(beat, 4.0) < 0.5;
            // calm: a drop on some bars; mid: every other beat; peak: every beat
            const bool fire = wm < 0.3f ? (down && rng_.chance(0.6f)) : (wp < 0.4f ? (beatCount_ % 2 == 0) : (beatCount_ % 4 != 3));
            if (fire && bridge_.amount < 0.5f && overT_ < 0.f) {
                const float r = (0.9f + 0.6f * wm + 0.6f * wp) * rng_.range(0.75f, 1.3f) * (down ? 1.15f : 1.f);
                const float s = 0.16f + 0.08f * wm + 0.1f * wp;
                drop(skySpot(r > 1.4f), r, 8.f + 4.f * wp, dropColor(wp), s);
            }
        }
    }
    // Kicks at the peak flick spatter into the painting.
    sinceKick_ += dt;
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (kick > 0.55f && prevKick_ <= 0.55f && sinceKick_ > 0.25f && wp > 0.35f && bridge_.amount < 0.5f && overT_ < 0.f) {
        sinceKick_ = 0.f;
        const vec2 c = skySpot(false);
        const vec3 k = dropColor(0.f);
        const float ang = rng_.range(0.f, TAU);
        const int n = rng_.irange(3, 6);
        for (int i = 0; i < n; i++) {
            const float t = rng_.range(-1.f, 1.f);
            const vec2 p = v2(c.x + std::cos(ang) * t * 2.2f + rng_.range(-0.4f, 0.4f), c.y + std::sin(ang) * t * 1.2f + rng_.range(-0.4f, 0.4f));
            if (std::fabs(p.x) < 11.8f && p.y < 10.8f) continue;
            drop(p, rng_.range(0.08f, 0.2f) * (1.f - 0.4f * std::fabs(t)), 10.f, k, 0.55f, 2);
        }
    }
    prevKick_ = kick;

    // Effects age.
    // a line clear runs at the real speed, even in slow motion (the row must not hide the stack for long)
    for (auto& l : lifted_) l.t += dt;
    lifted_.erase(std::remove_if(lifted_.begin(), lifted_.end(), [](const Lifted& l) { return l.t > l.delay + 0.45f; }), lifted_.end());
    for (auto& r : running_) r.t += dt;
    running_.erase(std::remove_if(running_.begin(), running_.end(), [](const Running& r) { return r.t > r.delay + 3.2f; }), running_.end());
    for (auto& b : blooms_) b.t += wdt;
    blooms_.erase(std::remove_if(blooms_.begin(), blooms_.end(), [](const Bloom& b) { return b.t > b.life; }), blooms_.end());
    for (auto& d : dots_) d.t += wdt;
    dots_.erase(std::remove_if(dots_.begin(), dots_.end(), [](const Dot& d) { return d.t > d.life; }), dots_.end());
    floodT_ += wdt;
    flood_ = floodT_ < 0.6f ? smoothstepf(0.f, 0.6f, floodT_) : 1.f - smoothstepf(2.f, 7.f, floodT_);

}

// =====================================================================================================
// Geometry
// =====================================================================================================

void WatercolorUniverse::cell(float x, float y, float half, int mask, const vec3& K, float alpha, int kind, float wet,
                              float seed, float extra, const vec3& body, float bodyAmt) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {K.x, K.y, K.z, alpha}, {(float)kind, wet, seed, extra},
                      {body.x, body.y, body.z, bodyAmt}});
}

void WatercolorUniverse::pencilLine(vec2 a, vec2 b, float width, float alpha, float seed) {
    if (alpha <= 0.003f) return;
    pencil_.push_back({{a.x, a.y, b.x, b.y}, {width, alpha, seed, 0.f}});
}

void WatercolorUniverse::buildBoard(const Game& g) {
    const float body = pal_.body;
    auto bodyCol = [&](int type) { return pal_.piece[std::clamp(type, 0, 6)]; };
    // Same-color neighbors merge into one wash: an 8-bit mask of them (L R D U, LD RD LU RU).
    auto maskAt = [&](int x, int y, auto same) {
        int m = 0;
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1}; // game y grows down
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    // Locked cells (a cell lands wet and dries in about two seconds).
    std::vector<int> liftedAt(Game::W * Game::H, 0);
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            auto same = [&](int nx, int ny) {
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H) return false;
                return g.cell(nx, ny).type == c.type && std::fabs(g.rowOffset(ny) - off) < 0.02f;
            };
            const vec3 p = layout::cellPos((float)x, y - off);
            const float age = c.flash > 0.f ? (1.f - c.flash) * Game::LOCK_FX_SECONDS : 9.f;
            const float wet = 1.f - smoothstepf(0.1f, 2.2f, age);
            cell(p.x, p.y, 0.5f, maskAt(x, y, same), pieceK(c.type), 1.f, 0, wet, (float)c.type * 1.7f, 0.f, bodyCol(c.type), body);
        }
    // Cleared cells dissolving back into the paper (each cell alone: the row's wash is broken).
    for (const Lifted& l : lifted_) {
        const float t = l.t - l.delay;
        const float k = saturate(t / 0.4f);
        const float a = 1.f - smoothstepf(0.25f, 0.45f, t);
        cell(l.x, l.y, 0.5f, 0, pieceK(l.type), a, 1, 0.f, (float)l.type * 1.7f, k, bodyCol(l.type), body);
    }
    // Game over: the stack running down the tilted sheet.
    for (const Running& r : running_) {
        const float t = std::max(0.f, r.t - r.delay);
        const float run = smoothstepf(0.f, 2.2f, t);
        const float a = 1.f - smoothstepf(1.4f, 3.0f, t);
        cell(r.x, r.y - run * 1.2f, 0.5f, 0, pieceK(r.type), a, 2, 0.6f * (1.f - run), r.seed * 9.f, run, bodyCol(r.type), body);
    }
    // Spatter.
    for (const Dot& d : dots_) {
        const float a = smoothstepf(0.f, 0.05f, d.t) * (1.f - smoothstepf(d.life * 0.6f, d.life, d.t));
        cell(d.p.x, d.p.y, d.r, 0, d.K, a, 3, 0.f, d.p.x * 3.1f + d.p.y);
    }
    // The falling piece (wet, being painted) and its ghost (the pencil underdrawing).
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
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
                const float s = (float)(c[0] * 7 + c[1] * 13);
                const float j = 0.015f;
                if (!inGhost(c[0] - 1, c[1])) pencilLine(v2(w.x - 0.47f, w.y - 0.5f - j), v2(w.x - 0.47f, w.y + 0.5f + j), 0.02f, 0.42f, s);
                if (!inGhost(c[0] + 1, c[1])) pencilLine(v2(w.x + 0.47f, w.y + 0.5f + j), v2(w.x + 0.47f, w.y - 0.5f - j), 0.02f, 0.42f, s + 1.f);
                if (!inGhost(c[0], c[1] - 1)) pencilLine(v2(w.x - 0.5f - j, w.y + 0.47f), v2(w.x + 0.5f + j, w.y + 0.47f), 0.02f, 0.42f, s + 2.f);
                if (!inGhost(c[0], c[1] + 1)) pencilLine(v2(w.x + 0.5f + j, w.y - 0.47f), v2(w.x - 0.5f - j, w.y - 0.47f), 0.02f, 0.42f, s + 3.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, maskAt(c[0], c[1], inPiece), pieceK(p.type), 1.f, 0, 0.55f, (float)p.type * 1.7f, 0.f, bodyCol(p.type), body);
        }
    }
    // Previews: small dry washes.
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
            const vec2 pos = v2(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc);
            cell(pos.x, pos.y, 0.5f * sc, maskAt(c[0], c[1], inPiece), pieceK(type), alpha, 0, 0.f, (float)type * 1.7f, 0.f, bodyCol(type), body);
        }
    };
    preview(g.holdType(), v2(layout::HOLD_CENTER.x, layout::HOLD_CENTER.y), layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), v2(c.x, c.y), layout::previewScale(i), 1.f);
    }
    // The well's pencil frame: four light strokes overshooting the corners.
    {
        const float X = 5.2f, Y = 10.2f, o = 0.35f;
        const float a = 0.5f * (1.f - saturate(run_ * 1.5f));
        pencilLine(v2(-X - 0.03f, -Y - o), v2(-X + 0.02f, Y + o * 0.6f), 0.026f, a, 1.f);
        pencilLine(v2(X + 0.01f, Y + o), v2(X - 0.02f, -Y - o * 0.7f), 0.026f, a, 2.f);
        pencilLine(v2(-X - o * 0.8f, -Y + 0.02f), v2(X + o, -Y - 0.02f), 0.026f, a, 3.f);
        pencilLine(v2(X + o * 0.5f, Y - 0.01f), v2(-X - o, Y + 0.03f), 0.026f, a * 0.8f, 4.f);
    }
}

// =====================================================================================================
// Text
// =====================================================================================================

namespace {
vec3 toSrgb(const vec3& c) {
    return {std::pow(std::max(c.x, 0.f), 1.f / 2.2f), std::pow(std::max(c.y, 0.f), 1.f / 2.2f), std::pow(std::max(c.z, 0.f), 1.f / 2.2f)};
}
} // namespace

// The HUD in WATERCOLOR's typography: a book italic for the song, the painting's name, the section and the
// celebrations, like a title penciled under a painting; small spaced capitals for labels; the numbers in the
// roman. All in the painting's own ink (a sepia, an indigo...), transparent like the washes (multiplied in, a touch
// darker at the edges where it dried: ink_), white gouache on the toned paper.
void WatercolorUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* italic = text::font(FONT_ITALIC);
    text::Font* roman = text::font(FONT_ROMAN);
    const float s = height_ / 1080.f;
    const float base = h.scale * 12.f;
    const float alpha = saturate(h.alpha);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    const vec3 ink = toSrgb(lerp(pal_.body > 0.3f ? ugl::toLinear(hex(0xf0ece0)) : pal_.ink, bridge_.ink, b));
    const vec3 accent = toSrgb(lerp(lerp(pal_.accent, pal_.ink, 0.25f), bridge_.ink, b));
    float weight = 0.0075f; // (the roman a touch bolder, the italic a little more)
    // a run: its size and letter spacing in pixels
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
        if (f == italic) weight = 0.01f;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        // "Watercolor" in the italic, the painting's name in small spaced capitals after it.
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        const float sz = base * 1.5f, vsz = base * 0.68f, a = saturate(0.35f + h.alpha * 1.6f);
        put(italic, text::titleCase(name), sz, 0.f, ink, a, -base * 0.15f);
        if (!var.empty()) {
            text::Span& v = out.add(var, roman, vsz, ink, 0.28f, a * 0.85f);
            v.gap = base * 0.8f / vsz;
            // its cap line 0.37 of the base under the name's
            v.dy = base * 0.37f + text::capHeight(roman) * vsz - text::capHeight(italic) * sz;
        }
        break;
    }
    case HUD_SECTION: put(italic, text::titleCase(h.text), base * 1.4f, 0.f, ink, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(italic, h.text, base * 1.3f, 0.f, ink, h.alpha * 1.3f, -base * 0.15f); break;
    case HUD_INFO: put(roman, h.text, base * 0.8f, base * 0.12f, ink, saturate(alpha * 1.2f) * 0.8f); break;
    case HUD_LABEL: put(roman, h.text, base * 0.78f, base * 0.3f, ink, saturate(alpha * 1.2f) * 0.75f); break;
    case HUD_VALUE: put(roman, h.text, base * 1.12f, base * 0.04f, ink, alpha); break;
    case HUD_NOTICE: put(italic, h.text, base * 1.2f, 0.f, accent, alpha); break;
    case HUD_KEY: put(roman, h.text, base * 0.9f, base * 0.05f, ink, alpha); break;
    case HUD_TOAST: put(italic, text::titleCase(h.text), base * 1.55f, 0.f, ink, alpha, -base * 0.2f); weight = 0.0125f; break;
    case HUD_TOAST_SUB: put(roman, h.text, base * 0.85f, base * 0.2f, accent, alpha * 0.9f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) {
            put(italic, text::titleCase(h.text), base * 1.35f, 0.f, ink, alpha, -base * 0.12f);
            weight = 0.0125f;
        } else {
            put(roman, h.text, base * 0.88f, base * 0.1f, h.accent ? accent : ink, alpha);
        }
        break;
    default: put(roman, h.text, base * 0.85f, base * 0.06f, h.accent ? accent : ink, alpha); break;
    }
    out.look.weight = weight;
    const bool gouache = pal_.body > 0.3f && b < 0.5f;
    out.blend = gouache ? text::BLEND_OVER : text::BLEND_MULTIPLY;
    ink_.p0 = vec4{gouache ? 1.f : 0.f, 0.f, 0.f, 0.f};
    out.lettering = &ink_;
}

// The menus' panels: plain paint.
void WatercolorUniverse::drawPanels(const std::vector<HudRect>& panels) {
    if (panels.empty()) return;
    std::vector<Vert> v;
    auto quad = [&](float x0, float y0, float x1, float y1, const vec3& c, float a) {
        const Vert q[6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                           {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
        v.insert(v.end(), q, q + 6);
    };
    for (const HudRect& r : panels) {
        const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal_.accent : pal_.paper;
        quad(r.x, r.y, r.x + r.w, r.y + r.h, c, std::min(1.f, r.alpha * (r.accent || r.custom ? 1.f : 1.1f)));
    }
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vert), v.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)v.size());
}

// =====================================================================================================
// Frame
// =====================================================================================================

void WatercolorUniverse::drawLand() {
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const Palette& P = pal_;
    const vec3 paper = P.paper;
    auto K = [&](const vec3& c) { return absorb(c, paper); };
    landT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progLand_);
    bindNoise(progLand_, 0);
    set4f(progLand_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progLand_, "uSeed", seedF_);
    set1f(progLand_, "uHorizon", P.horizon);
    set1f(progLand_, "uReach", P.reach * (0.42f + 0.33f * wm + 0.25f * wp));
    set1f(progLand_, "uSpread", 0.3f + 0.4f * wm + 0.3f * wp);
    set1f(progLand_, "uSkyDens", 0.32f + 0.1f * wm + 0.08f * wp);
    set1f(progLand_, "uGlaze", wp);
    set1f(progLand_, "uWet", P.wet);
    set1f(progLand_, "uGran", P.gran);
    set3f(progLand_, "uSkyTopK", K(P.skyTop));
    set3f(progLand_, "uSkyLowK", K(P.skyLow));
    set3f(progLand_, "uSunK", K(P.sun));
    set3f(progLand_, "uFarK", K(P.far));
    set3f(progLand_, "uMidK", K(P.mid));
    set3f(progLand_, "uNearK", K(P.near));
    set3f(progLand_, "uTreeK", K(P.tree));
    set3f(progLand_, "uAccentK", K(P.accent));
    set4f(progLand_, "uSun", P.sunX, P.sunY, P.sunR * (1.f + 0.15f * wp), P.sunLift);
    set1f(progLand_, "uSunPaint", P.sunPaint * (0.5f + 0.25f * wm + 0.5f * wp));
    set4f(progLand_, "uFar", P.farH, P.farRough, 0.42f + 0.1f * wm + 0.15f * wp, 1.3f);
    set4f(progLand_, "uMid", P.midH, P.midRough, 0.55f + 0.15f * wp, 0.15f + 1.1f * wm);
    set4f(progLand_, "uNear", P.nearType, 0.45f + 0.15f * wp, 0.25f + 1.1f * wm, 0.f);
    set4f(progLand_, "uTrees", P.treeType, 0.7f + 0.2f * wp, smoothstepf(0.3f, 1.f, wm) * 1.3f, 0.f);
    glUniform4fv(ugl::U(progLand_, "uBloom"), 3, &backruns_[0].x);
    set1f(progLand_, "uLpx", 2.f * viewH_.y / landT_.h);
    set1f(progLand_, "uNight", slow_);
    set3f(progLand_, "uNightK", absorb(ugl::toLinear(hex(0x3a4466)), vec3(1.f)) * 0.8f);
    set3f(progLand_, "uMoon", vec3(15.f, 6.f, 1.1f));
    set1f(progLand_, "uFlood", flood_);
    set3f(progLand_, "uFloodK", floodK_);
    ugl::drawFullscreen();
}

void WatercolorUniverse::drawBlooms() {
    wetT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    drops_.clear();
    for (const Bloom& b : blooms_)
        drops_.push_back({{b.p.x, b.p.y, b.r, b.t}, {b.K.x, b.K.y, b.K.z, b.strength}, {b.seed, (float)b.kind, b.life, 0.f}});
    if (drops_.empty()) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(progBloom_);
    bindNoise(progBloom_, 0);
    set4f(progBloom_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    glBindVertexArray(bloomVao_);
    glBindBuffer(GL_ARRAY_BUFFER, bloomVbo_);
    glBufferData(GL_ARRAY_BUFFER, drops_.size() * sizeof(DropInst), drops_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)drops_.size());
    glDisable(GL_BLEND);
}

void WatercolorUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    bare_ = f.bare;
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { pal_ = target; palSet_ = true; }
        else lerpPalette(pal_, target, 1.f - std::exp(-dt * 1.0f));
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 0.9f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    run_ = approach(run_, overT_ >= 0.f ? saturate(overT_ / 3.5f) : 0.f, overT_ >= 0.f ? 10.f : 0.6f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    if (!f.paused) simulate(game, music, dt, wdt);
    const float wp = smoothstepf(1.1f, 2.f, lv_);
    const float bridge = saturate(bridge_.amount);

    // ---- The painting (reduced resolution), the wet layer.
    drawLand();
    drawBlooms();

    // ---- The sheet at full resolution.
    sceneT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    {
        glUseProgram(progComp_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, paperT_.tex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, landT_.tex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, wetT_.tex);
        bindNoise(progComp_, 3);
        set1i(progComp_, "uPaper", 0);
        set1i(progComp_, "uLand", 1);
        set1i(progComp_, "uWet", 2);
        set4f(progComp_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set2f(progComp_, "uRes", (float)width_, (float)height_);
        set3f(progComp_, "uPaperCol", pal_.paper);
        set1f(progComp_, "uPx", px_);
        // (a break: no well masked out of the painting, which runs on through its place)
        set4f(progComp_, "uWell", WELL_X, WELL_Y, bare_ ? 1.f : 0.f, bare_ ? 0.f : 0.15f);
        set1f(progComp_, "uWetGain", 1.f - smoothstepf(0.f, 0.6f, run_));
        // the teardown: the sheet tilted, its washes running down and lifted back to paper (the game over's run)
        set1f(progComp_, "uRun", std::max(run_, smoothstepf(0.05f, 0.9f, f.teardown)));
        set1f(progComp_, "uPaint", smoothstepf(0.1f, 0.85f, f.setup));
        set1f(progComp_, "uBridge", bridge);
        set3f(progComp_, "uBridgeCol", bridge_.color);
        set1f(progComp_, "uLift", 1.f);
        set1f(progComp_, "uSeed", seedF_);
        ugl::drawFullscreen();
        glActiveTexture(GL_TEXTURE0);
    }

    // ---- Glazed over the sheet: pencil, pieces, the HUD (dst * mul + add).
    cells_.clear();
    pencil_.clear();
    if (!bare_) buildBoard(game);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC1_COLOR);
    if (!pencil_.empty()) {
        glUseProgram(progPencil_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, paperT_.tex);
        set1i(progPencil_, "uPaper", 0);
        bindNoise(progPencil_, 1);
        setMat(progPencil_, "uVP", vp_);
        set2f(progPencil_, "uRes", (float)width_, (float)height_);
        set1f(progPencil_, "uPx", px_);
        set3f(progPencil_, "uGraphite", lerp(vec3(0.16f, 0.16f, 0.17f), bridge_.ink, smoothstepf(0.3f, 0.9f, bridge)));
        set1f(progPencil_, "uBridge", bridge);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, pencil_.size() * sizeof(LineInst), pencil_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)pencil_.size());
    }
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, paperT_.tex);
        set1i(progCell_, "uPaper", 0);
        bindNoise(progCell_, 1);
        setMat(progCell_, "uVP", vp_);
        set2f(progCell_, "uRes", (float)width_, (float)height_);
        set1f(progCell_, "uPx", px_);
        set1f(progCell_, "uGran", 0.22f + 0.2f * pal_.gran);
        const float bb = smoothstepf(0.2f, 0.9f, bridge);
        set1f(progCell_, "uBridge", bb);
        set3f(progCell_, "uBridgeK", absorb(bridge_.ink, vec3(std::max(0.05f, ugl::luma(bridge_.color) * 1.4f))) * 0.6f);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    glActiveTexture(GL_TEXTURE0);
    drawPanels(f.panels);
    glDisable(GL_BLEND);
    (void)wp;

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

const UniverseEntry& universe_watercolor() {
    static const UniverseEntry e{
        "WATERCOLOR", "wet watercolor on cold-press paper", [] { return std::unique_ptr<Universe>(new WatercolorUniverse()); },
        // Gentle, acoustic, airy songs suit the paper best (slow to mid tempo, soft dynamics, room in the mix).
        [](const SongLook& l) {
            return 0.6f + 0.35f * (1.f - smoothstepf(115.f, 155.f, l.bpm)) + 0.25f * l.airWeight + 0.25f * (1.f - l.density) +
                   0.15f * (1.f - l.bassWeight);
        },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
