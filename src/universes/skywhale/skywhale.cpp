// SKY WHALE: a little koi swims in a lily pond at dusk, behind the well. Every cleared line makes it grow, and it
// never stops: it outgrows the pond and slips into the river, which widens to the sea; as a whale it breaches in
// the open sea by a lighthouse; then it leaves the water and swims up through the clouds, seabirds and hot-air
// balloons in its wake, on through the stratosphere into orbit, where it glides around the Earth with aurora
// trailing from its fins, until its arc spans the Moon's orbit.
//
// The growth is G = ln(the creature's length in meters): 30 cm at the first line, 1 m (leaving the pond) at ~6
// lines, a 15 m whale reaching the sea at ~24, 150 m leaving the water at ~48, the clouds (2 km) at ~64, orbit at
// ~100, around the Earth at ~140, past the Moon's orbit at ~200, then slower and slower. One mesh for its whole life
// (a grid per part: body, tail fin, dorsal, pectorals, pelvics) placed by the vertex shader against the morph (koi ->
// whale: the slender patterned body thickens, the forked vertical tail turns into horizontal flukes, the pectorals
// grow into a humpback's long flippers, the markings become soft lights), the swimming wave (side to side for the
// koi, up and down for the whale) and the spine's bend (around the planet in orbit). Each clear is a slow, powerful
// stroke of the tail: the growth springs to its new target (a critically damped spring), the creature surges ahead
// along its path and the camera follows a moment later. A Tetris is a huge one, with a burst of light and spray.
//
// The world is GIANT TREE's, adapted: a ray-traced planet (the pond, the river and the sea at local scale, with
// meadows, pebbles and caustics, lily pads; continents and oceans farther) under an exponential atmosphere, a cloud
// layer and a shell of clouds, the moon, the stars, a galaxy. Toon look: crisp bands of light, cool tinted shades,
// warm rims, scalloped toon trees on the banks, the creature glossy and painterly.
//
// The music: calm is a slow glide; mid and peak swim livelier with glints around it. The drop (a real one only, not
// a chorus) is its song: it surges, its lights run from its head to its tail and soft rings of light ripple out of
// it through the sky on the beat (every two beats), then it settles. Game over: it sinks into the deep, fading; the
// next game hatches a new koi in the pond (the camera dives back). The pieces are koi-scale enamel.
//
// Layout contract: the board is drawn flat (orthographic at the reference camera's scale); only the world behind it
// has its own 3D camera.
#include "universes/skywhale/skywhale.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/skywhale/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Quicksand Medium (the labels and numbers, round and soft) and Fraunces Soft Italic (the song, the variant, the
// notices), SIL Open Font License, see OFL-Quicksand.txt and OFL-Fraunces.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/skywhale/Quicksand-Medium.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/skywhale/Fraunces-SoftItalic.ttf.inc"
};

namespace shaders = skywhale_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, shaders::COMMON_GLSL);
    std::string v(vs);
    v.insert(v.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(v.c_str(), f.c_str(), name);
}
// A world shader: the light in both stages (and the creature's shape for its own).
GLuint compileWorld(const char* vs, const char* fs, const char* name, bool whale) {
    const std::string pre = std::string(shaders::COMMON_GLSL) + shaders::LIGHT_GLSL + (whale ? shaders::WHALE_GLSL : "");
    std::string f(fs);
    f.insert(f.find('\n') + 1, pre);
    std::string v(vs);
    v.insert(v.find('\n') + 1, pre);
    return ugl::program(v.c_str(), f.c_str(), name);
}

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 lerp2(vec2 a, vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB), in the Palette's order: the sky, space, the sun and the toon light; the land, the shore, the lily
// pads, the water, the trees; the creature (the koi's white and its two patch colors, the whale's back and belly, the
// fins, its lights, its eye); the aurora, the lanterns, the clouds; the HUD's ink, accent, veil; the seven pieces;
// the sun's place (azimuth from the view: 0 ahead, negative to the left; elevation), its size, night, stars, the koi's ink spots, the sky's aurora,
// bloom, how much the creature glows.
struct VariantDef {
    const char* name;
    unsigned c[40];
    unsigned piece[7];
    float sunAz, sunEl, sunSize, night, stars, patch2, auroraSky, bloom, glowK;
};
const VariantDef VARIANTS[] = {
    // Dusk: an indigo sky over a peach horizon, the sun setting ahead, warm lanterns; a kohaku koi (red on white).
    {"DUSK",
     {0x2c3a8c, 0xff9e72, 0x05061a, 0x6a3cd8, 0x2ab0ff, 0xffe2b0, 0xff8a4a, 0xffd6b0, 0x5a4ea0, 0xffa888, 0xffc27a,
      0x8ab84e, 0x4f8a44, 0x1f5aa0, 0x5aa048, 0xe0c070, 0xff7ab8, 0xecd4b0, 0x3c9a4a, 0xff8ac0, 0x9ae0d8, 0x123a62,
      0x4caa48, 0x245a50, 0xfff6ee, 0xff4a1e, 0x1c1a26, 0x34467e, 0xf2e2d4, 0xfff0e4, 0xffcf6a, 0xffb030, 0x5affb0,
      0xb070ff, 0xffb050, 0xffd2b8, 0x8a6ab0, 0xfff4ea, 0xffb27a, 0x161636},
     {0xff5a3c, 0xffc23a, 0x5ad66a, 0x4ab4ff, 0xb48aff, 0xff86b8, 0xfff0dc}, -1.9f, 0.2f, 0.035f, 0.f, 0.f, 0.35f, 0.f, 0.45f, 0.35f},
    // Moonlight: a deep blue night, cool light, the koi platinum and red, its lights cyan; the stars.
    {"MOONLIGHT",
     {0x0a1648, 0x2a4e96, 0x02040e, 0x8a3cff, 0x18b0ff, 0xcfe0ff, 0x5a7cff, 0xa8c4ff, 0x262a70, 0x2a4aa0, 0x9ad8ff,
      0x2a7a6a, 0x15485a, 0x0c3a8a, 0x1e6a60, 0x50609a, 0xffa0e0, 0x6a7aa0, 0x1e6a5a, 0xffa0e0, 0x6ab0d0, 0x061838,
      0x1e6a6a, 0x102a50, 0xf0f0ff, 0xff5a3a, 0x20284a, 0x22386a, 0xc8d6f0, 0xe8f0ff, 0x7affe8, 0x8affff, 0x5affc0,
      0xa070ff, 0xffb860, 0xa8bcf0, 0x34347a, 0xeef4ff, 0x8affea, 0x050a26},
     {0x5ae8ff, 0xffe06a, 0xd080ff, 0x5effa8, 0xff6ab0, 0x6a8cff, 0xffa060}, 2.2f, 0.45f, 0.03f, 1.f, 1.f, 0.3f, 0.f, 0.6f, 1.f},
    // Dawn: pastel pink and blue, cherry trees on the banks, a golden ogon koi, white lotus.
    {"DAWN",
     {0x5e86d8, 0xffc6c8, 0x0a0820, 0xff6ad0, 0x7ae0ff, 0xfff6e6, 0xffb0a0, 0xfff0e0, 0x8a80d0, 0xffd0d8, 0xfff0b0,
      0x9ce0a0, 0x5cb890, 0x3a8ad8, 0x8ad08a, 0xffe0a0, 0xffffff, 0xf0dcc8, 0x5ab878, 0xffffff, 0xb0f0f0, 0x2a5a9a,
      0xff9ac8, 0xb060a8, 0xffe27a, 0xff7a1a, 0x3a2a40, 0x5a6aa8, 0xfff0ec, 0xfff4d8, 0xff9ad8, 0x6a4a20, 0x7affd0,
      0xff8ae0, 0xffc070, 0xffffff, 0xc0a8f0, 0xffffff, 0xfff0a0, 0x2a2058},
     {0x6ae0ff, 0xffd84a, 0xd8a0ff, 0x7aee9a, 0xff7a8a, 0x7a96ff, 0xffaa60}, 2.0f, 0.16f, 0.032f, 0.f, 0.f, 0.2f, 0.f, 0.35f, 0.25f},
    // Aurora: a polar night, green curtains over the sky, snowy banks and pines, a showa koi (red, white, ink).
    {"AURORA",
     {0x061830, 0x1a5470, 0x020610, 0x2aff9a, 0x8a4aff, 0xd0f0ff, 0x4affb0, 0xa0e0e8, 0x1a3060, 0x1a4a6a, 0x8affd0,
      0xd0e4f0, 0x8aa8c4, 0x0a3060, 0x4a7a7a, 0xc8d0d8, 0xffffff, 0x8a9aa8, 0x2a7a6a, 0xc8a0ff, 0x70c0c8, 0x041a30,
      0x1a5a50, 0x0c2a3a, 0xffffff, 0xff3a2a, 0x101820, 0x1c3448, 0xc0e0e4, 0xe8fff8, 0x6affb4, 0x6affb4, 0x5affa0,
      0xc06aff, 0xffa860, 0x9ad8e0, 0x2a3a6a, 0xeafff6, 0x7affc4, 0x041422},
     {0x5af0ff, 0xffe066, 0xd884ff, 0x6affa0, 0xff6a8a, 0x5a8cff, 0xffa05a}, 2.5f, 0.3f, 0.03f, 1.f, 1.f, 0.7f, 1.f, 0.6f, 0.9f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = SkyWhaleUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    Palette p;
    vec3* c = reinterpret_cast<vec3*>(&p);
    for (int i = 0; i < 40; i++) c[i] = ugl::toLinear(hex(d.c[i]));
    for (int i = 0; i < 7; i++) p.piece[i] = ugl::toLinear(hex(d.piece[i]));
    p.sunAz = d.sunAz; p.sunEl = d.sunEl; p.sunSize = d.sunSize; p.night = d.night; p.stars = d.stars;
    p.patch2 = d.patch2; p.auroraSky = d.auroraSky; p.bloom = d.bloom; p.glowK = d.glowK;
    return p;
}
static_assert(offsetof(Palette, piece) == sizeof(vec3) * 40, "40 colors before the pieces");
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

constexpr float CLEAR_T = 0.4f;      // a cleared row melts in this long (real time)
constexpr float FLOW_T = 1.25f;      // its light flows to the creature in this long
constexpr int MAX_MOTES = 1600;
constexpr int MAX_SPARKS = 1800;
constexpr float CLOUD_Y = 1900.f;    // the cloud layer (meters)
constexpr double R_EARTH = 6.371e6;

// The local geography (the same functions as in SKY_FS): the pond at the origin, the river leaving it east (+x),
// widening to the sea; a headland with the lighthouse on the far side of the mouth.
namespace swgeo {
constexpr float COAST_X = 120.f, HEAD_Z = 70.f;
inline float riverZ(float x) {
    return 0.16f * x * std::sin(0.95f * std::log(x + 4.f) + 0.6f) * (1.f - smoothstepf(COAST_X * 0.6f, COAST_X * 1.6f, x));
}
inline float riverHW(float x) { return 0.55f + 0.17f * x; }
inline float coastX(float z) {
    return COAST_X + 14.f * std::sin(z / 37.f) + 6.f * std::sin(z / 11.f + 1.f) + 70.f * std::exp(-(z - HEAD_Z) * (z - HEAD_Z) / 900.f);
}
inline float pondR(float x, float z) {
    const float a = std::atan2(z, x);
    return 2.4f * (1.f + 0.07f * std::sin(3.f * a + 0.5f) + 0.04f * std::sin(5.f * a + 2.f));
}
} // namespace swgeo

// The journey: lines -> the creature's length (meters), interpolated in log space; beyond the table, slower and slower.
struct Stage { float lines, length; };
const Stage STAGES[] = {
    {0, 0.3f}, {3, 0.5f}, {6, 0.95f}, {11, 2.5f}, {16, 6.f}, {21, 12.f}, {26, 20.f}, {32, 38.f}, {40, 80.f},
    {48, 160.f}, {56, 500.f}, {64, 1400.f}, {72, 4000.f}, {82, 1.4e4f}, {92, 6.e4f}, {104, 3.e5f}, {118, 1.4e6f},
    {134, 7.e6f}, {150, 2.e7f}, {170, 3.5e7f}, {195, 5.e7f}, {220, 7.e7f},
};
constexpr int STAGE_COUNT = (int)(sizeof(STAGES) / sizeof(STAGES[0]));
constexpr float G_MAX = 18.8f;

// The Moon (meters, around the Earth's center): out where the creature swims at the end of the journey, to circle it.
constexpr double R_MOON = 3.2e6, D_MOON = 3.6e8, PHI_MOON = 2.1;
vec3 moonPos() { return vec3((float)(D_MOON * std::sin(PHI_MOON)), (float)(-R_EARTH + D_MOON * std::cos(PHI_MOON)), 0.f); }
// How far it has gone from circling the Earth to circling the Moon.
float moonK(float G) { return smoothstepf(16.9f, 17.5f, G); }
// The body it circles (the Earth's center, then the Moon's).
vec3 orbitCenter(float G) { return lerp(vec3(0.f, -(float)R_EARTH, 0.f), moonPos(), moonK(G)); }

// Test hooks: ZEN_SKYWHALE_LINES=N adds N lines to the growth, ZEN_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("ZEN_SKYWHALE_LINES") ? std::atoi(std::getenv("ZEN_SKYWHALE_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

// A value keyed on the growth (piecewise smooth between keys).
float keyed(float G, const float* keys, const float* vals, int n) {
    if (G <= keys[0]) return vals[0];
    for (int i = 0; i + 1 < n; i++)
        if (G < keys[i + 1]) return lerpf(vals[i], vals[i + 1], smoothstepf(keys[i], keys[i + 1], G));
    return vals[n - 1];
}

// The creature's morph: 0 a koi .. 1 a whale.
float morphOf(float G) { return smoothstepf(0.2f, 2.7f, G); }

// The breaching cycle at sea: the center's height (body lengths) at phase p (0..1): cruising, a dive, a leap.
float breachY(float p) {
    p -= std::floor(p);
    if (p < 0.6f) return 0.05f * std::sin(p / 0.6f * TAU * 2.f) - 0.02f;   // surfacing to breathe, twice
    if (p < 0.68f) return -0.13f * std::sin((p - 0.6f) / 0.08f * PI);
    if (p < 0.94f) { const float s = std::sin((p - 0.68f) / 0.26f * PI); return 0.48f * s * std::sqrt(s); }
    return -0.06f * std::sin((p - 0.94f) / 0.06f * PI);
}

// The creature's body in its own space (CPU mirror of WHALE_GLSL's bend, for the aurora's ribbons).
vec3 bendBody(vec3 p, float k) {
    if (std::fabs(k) < 1e-3f) return p;
    const float a = k * p.x;
    return vec3(std::sin(a) / k + std::sin(a) * p.y, (std::cos(a) - 1.f) / k + std::cos(a) * p.y, p.z);
}

// A mesh turned on a lathe (meters): a profile of (radius, height, material) points; `segs` around; `flat`: faceted.
struct ProfilePt { float r, y; int mat; };
void lathe(const std::vector<ProfilePt>& prof, int segs, bool flat, float rock, std::vector<float>& out) {
    float yMax = 0.f;
    for (auto& p : prof) yMax = std::max(yMax, p.y);
    auto rad = [&](const ProfilePt& p, float a) {
        float r = p.r;
        if (p.mat == 5) r *= 1.f + rock * (0.14f * std::sin(3.f * a + 0.7f) + 0.09f * std::sin(7.f * a + 2.f) + 0.05f * std::sin(13.f * a));
        return r;
    };
    auto put = [&](const vec3& p, const vec3& n, int mat) {
        const float v[8] = {p.x, p.y, p.z, (float)mat, n.x, n.y, n.z, yMax > 0.f ? p.y / yMax : 0.f};
        out.insert(out.end(), v, v + 8);
    };
    for (size_t i = 0; i + 1 < prof.size(); i++) {
        const ProfilePt &A = prof[i], &B = prof[i + 1];
        if (std::fabs(A.r - B.r) < 1e-6f && std::fabs(A.y - B.y) < 1e-6f) continue;
        const int mat = B.mat;
        for (int s = 0; s < segs; s++) {
            const float a0 = TAU * s / segs, a1 = TAU * (s + 1) / segs;
            const vec3 p00(std::cos(a0) * rad(A, a0), A.y, std::sin(a0) * rad(A, a0));
            const vec3 p01(std::cos(a1) * rad(A, a1), A.y, std::sin(a1) * rad(A, a1));
            const vec3 p10(std::cos(a0) * rad(B, a0), B.y, std::sin(a0) * rad(B, a0));
            const vec3 p11(std::cos(a1) * rad(B, a1), B.y, std::sin(a1) * rad(B, a1));
            vec3 fn = normalize(cross(p10 - p00, p01 - p00));
            if (dot(fn, vec3(std::cos(a0), 0.f, std::sin(a0))) < 0.f) fn = fn * -1.f;
            auto sn = [&](float a) {
                const float dr = B.r - A.r, dy = B.y - A.y;
                const float l = std::sqrt(dr * dr + dy * dy) + 1e-9f;
                return normalize(vec3(std::cos(a) * dy / l, -dr / l, std::sin(a) * dy / l));
            };
            const vec3 n0 = flat ? fn : sn(a0), n1 = flat ? fn : sn(a1);
            put(p00, n0, mat); put(p10, n0, mat); put(p11, n1, mat);
            put(p00, n0, mat); put(p11, n1, mat); put(p01, n1, mat);
        }
    }
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool SkyWhaleUniverse::init(int width, int height) {
    progSky_ = compileWorld(ugl::FULLSCREEN_VS, shaders::SKY_FS, "skywhale sky", false);
    progWhale_ = compileWorld(shaders::WHALE_VS, shaders::WHALE_FS, "skywhale whale", true);
    progProp_ = compileWorld(shaders::PROP_VS, shaders::PROP_FS, "skywhale prop", false);
    progPuff_ = compileWorld(shaders::PUFF_VS, shaders::PUFF_FS, "skywhale puff", false);
    progCloud_ = compileWorld(shaders::CLOUD_VS, shaders::CLOUD_FS, "skywhale cloud", false);
    progSea_ = compileWorld(ugl::FULLSCREEN_VS, shaders::SEA_FS, "skywhale sea", false);
    progGlow_ = compile(shaders::GLOW_VS, shaders::GLOW_FS, "skywhale glow");
    progRibbon_ = compile(shaders::RIBBON_VS, shaders::RIBBON_FS, "skywhale ribbon");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "skywhale comp");
    progBright_ = compile(ugl::FULLSCREEN_VS, shaders::BRIGHT_FS, "skywhale bright");
    progBlur_ = compile(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "skywhale blur");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "skywhale sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "skywhale veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "skywhale cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "skywhale shape");
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "skywhale text");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "skywhale post");
    progUp_ = compile(ugl::FULLSCREEN_VS, shaders::UPSAMPLE_FS, "skywhale upsample");

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
    instanced(puffVao_, puffVbo_, 4, sizeof(float) * 16);
    instanced(glowVao_, glowVbo_, 4, sizeof(float) * 16);
    instanced(cloudVao_, cloudVbo_, 2, sizeof(float) * 8);
    instanced(spriteVao_, spriteVbo_, 3, sizeof(SpriteInst));
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));
    // the aurora's ribbons: two vec4 a vertex
    glGenVertexArrays(1, &ribbonVao_);
    glGenBuffers(1, &ribbonVbo_);
    glBindVertexArray(ribbonVao_);
    glBindBuffer(GL_ARRAY_BUFFER, ribbonVbo_);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 4 * i));
    }
    // shapes and text (pixels)
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
    buildWhale();
    buildProps();
    buildWorld();
    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void SkyWhaleUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] skywhale font init failed\n");
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

// The creature's mesh: a grid per part (part, a, b, side), placed by WHALE_VS.
void SkyWhaleUniverse::buildWhale() {
    std::vector<float> v;
    std::vector<uint32_t> idx;
    auto grid = [&](int part, int na, int nb, float a0, float a1, float b0, float b1, float side, bool bodyU) {
        const uint32_t base = (uint32_t)(v.size() / 4);
        for (int i = 0; i < na; i++) {
            float t = (float)i / (na - 1);
            float a = bodyU ? std::max(0.0006f, std::pow(t, 1.7f)) : lerpf(a0, a1, t);
            for (int j = 0; j < nb; j++) {
                const float b = lerpf(b0, b1, (float)j / (nb - 1));
                const float q[4] = {(float)part, a, b, side};
                v.insert(v.end(), q, q + 4);
            }
        }
        for (int i = 0; i + 1 < na; i++)
            for (int j = 0; j + 1 < nb; j++) {
                const uint32_t p0 = base + i * nb + j, p1 = p0 + 1, p2 = p0 + nb, p3 = p2 + 1;
                const uint32_t q[6] = {p0, p2, p1, p1, p2, p3};
                idx.insert(idx.end(), q, q + 6);
            }
    };
    grid(0, 120, 49, 0.f, 1.f, 0.f, TAU, 0.f, true);            // the body (u from the snout, theta around)
    grid(1, 20, 27, 0.f, 1.f, -1.f, 1.f, 0.f, false);            // the tail fin
    grid(2, 22, 7, 0.f, 1.f, 0.f, 1.f, 0.f, false);              // the dorsal fin
    grid(3, 18, 8, 0.f, 1.f, -0.5f, 0.5f, 1.f, false);           // the pectoral fins
    grid(3, 18, 8, 0.f, 1.f, -0.5f, 0.5f, -1.f, false);
    grid(4, 8, 5, 0.f, 1.f, -0.5f, 0.5f, 1.f, false);            // the koi's pelvic fins
    grid(4, 8, 5, 0.f, 1.f, -0.5f, 0.5f, -1.f, false);
    glGenVertexArrays(1, &whaleVao_);
    glGenBuffers(1, &whaleVbo_);
    glGenBuffers(1, &whaleIbo_);
    glBindVertexArray(whaleVao_);
    glBindBuffer(GL_ARRAY_BUFFER, whaleVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, whaleIbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);
    whaleCount_ = (GLsizei)idx.size();
    glBindVertexArray(0);
}

// The lighthouse on its rock (meters, 35 m tall) and a stone lantern (a toro, 0.9 m, hexagonal).
void SkyWhaleUniverse::buildProps() {
    auto upload = [&](PropMesh& m, const std::vector<float>& data) {
        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);
        glBindVertexArray(m.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
        for (int i = 0; i < 2; i++) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 4 * i));
        }
        m.count = (GLsizei)(data.size() / 8);
        glBindVertexArray(0);
    };
    {
        std::vector<ProfilePt> p = {{16.f, -3.f, 5}, {15.f, 0.5f, 5}, {12.f, 3.f, 5}, {8.f, 5.f, 5}, {4.6f, 6.f, 5}, {3.6f, 6.f, 0}, {3.5f, 7.f, 0}};
        // the tower: red and white bands, tapering
        const float y0 = 7.f, y1 = 27.f;
        auto r = [&](float y) { return lerpf(3.3f, 2.3f, (y - y0) / (y1 - y0)); };
        int mat = 1;
        for (float y = y0; y < y1 - 0.01f; y += 5.f) {
            p.push_back({r(y), y, mat});
            p.push_back({r(y + 5.f), y + 5.f, mat});
            mat = mat == 1 ? 2 : 1;
        }
        const std::vector<ProfilePt> top = {{2.3f, 27.f, 4}, {3.3f, 27.f, 4}, {3.3f, 28.f, 4}, {1.9f, 28.f, 4}, {1.9f, 28.f, 3},
                                            {1.9f, 31.5f, 3}, {1.9f, 31.5f, 4}, {2.4f, 31.5f, 4}, {1.0f, 33.5f, 4}, {0.25f, 34.6f, 4}, {0.f, 35.f, 4}};
        p.insert(p.end(), top.begin(), top.end());
        std::vector<float> data;
        lathe(p, 36, false, 1.f, data);
        upload(lighthouse_, data);
    }
    {
        const std::vector<ProfilePt> p = {{0.24f, 0.f, 0}, {0.24f, 0.08f, 0}, {0.16f, 0.1f, 0}, {0.075f, 0.1f, 0}, {0.06f, 0.42f, 0},
                                          {0.19f, 0.42f, 0}, {0.19f, 0.48f, 0}, {0.13f, 0.48f, 0}, {0.13f, 0.48f, 6}, {0.13f, 0.66f, 6},
                                          {0.13f, 0.66f, 0}, {0.32f, 0.67f, 0}, {0.3f, 0.71f, 0}, {0.09f, 0.8f, 0}, {0.05f, 0.86f, 0},
                                          {0.06f, 0.88f, 0}, {0.f, 0.93f, 0}};
        std::vector<float> data;
        lathe(p, 6, true, 0.f, data);
        upload(toro_, data);
    }
}

// The banks: toon trees and bushes around the pond and along the river, reeds and grass at the water's edge.
void SkyWhaleUniverse::buildWorld() {
    Rng r(0x5EA1ull);
    trees_.clear();
    auto puff = [&](const vec3& p, float rad, float tint, int kind, float h) {
        const float q[16] = {p.x, p.y, p.z, rad, tint, 0.f, 0.f, r.uniform(), 0.f, 0.f, 0.f, (float)kind, h, 0.f, 0.f, 0.f};
        trees_.insert(trees_.end(), q, q + 16);
    };
    auto tree = [&](const vec3& foot, float h) {
        const float tint = r.range(-1.f, 1.f);
        puff(foot, h * 0.035f, tint, 1, h * 0.75f);
        const vec3 top = foot + vec3(0.f, h * 0.68f, 0.f);
        const int n = r.irange(5, 8);
        for (int i = 0; i < n; i++) {
            const float a = r.range(0.f, TAU), d = h * r.range(0.05f, 0.2f);
            puff(top + vec3(std::cos(a) * d, r.range(-0.08f, 0.14f) * h, std::sin(a) * d), h * r.range(0.16f, 0.24f), tint, 0, 0.f);
        }
    };
    auto bush = [&](const vec3& foot, float s) {
        const float tint = r.range(-1.f, 1.f);
        const int n = r.irange(2, 4);
        for (int i = 0; i < n; i++)
            puff(foot + vec3(r.range(-0.4f, 0.4f) * s, s * r.range(0.35f, 0.6f), r.range(-0.4f, 0.4f) * s), s * r.range(0.4f, 0.6f), tint, 0, 0.f);
    };
    // around the pond: bushes on the banks, three trees beyond (the far side, the camera looks across the pond)
    for (int i = 0; i < 26; i++) {
        const float a = r.range(0.f, TAU);
        if (std::fabs(std::remainder(a - 0.f, TAU)) < 0.5f) continue; // the river's outlet
        const float d = swgeo::pondR(std::cos(a), std::sin(a)) + r.range(0.35f, 1.2f);
        bush(vec3(std::cos(a) * d, 0.f, std::sin(a) * d), r.range(0.25f, 0.5f));
    }
    const float ta[4] = {1.7f, 2.4f, 1.15f, 3.3f};
    for (float a : ta) {
        const float d = r.range(4.5f, 7.f);
        tree(vec3(std::cos(a) * d, 0.f, std::sin(a) * d), r.range(4.f, 6.5f));
    }
    // along the river: trees of a real size on both banks, thinning out toward the sea
    for (int i = 0; i < 90; i++) {
        const float x = std::exp(r.range(std::log(4.f), std::log(swgeo::COAST_X * 1.1f)));
        const float side = r.chance(0.5f) ? 1.f : -1.f;
        const float hw = swgeo::riverHW(x);
        const float z = swgeo::riverZ(x) + side * (hw + r.range(1.5f, 3.f + 0.6f * x));
        if (x > swgeo::coastX(z) - 8.f) continue;
        if (r.chance(0.65f)) tree(vec3(x, 0.f, z), r.range(5.f, 11.f));
        else bush(vec3(x, 0.f, z), r.range(0.6f, 1.6f));
    }
    // bushes on the headland around the lighthouse
    for (int i = 0; i < 10; i++) {
        const float z = swgeo::HEAD_Z + r.range(-45.f, 45.f);
        const float x = swgeo::coastX(z) - r.range(30.f, 90.f);
        bush(vec3(x, 0.f, z), r.range(1.5f, 3.f));
    }
    // reeds and grass at the pond's edge, and along the river's start
    tufts_.clear();
    for (int i = 0; i < 900; i++) {
        Tuft t;
        const float a = r.range(0.f, TAU);
        const float pr = swgeo::pondR(std::cos(a), std::sin(a));
        const bool reed = r.chance(0.3f);
        const float d = pr + (reed ? r.range(-0.15f, 0.25f) : r.range(0.15f, 2.2f));
        if (std::fabs(std::remainder(a, TAU)) < 0.35f && d < pr + 0.3f) continue;
        t.p = vec3(std::cos(a) * d, 0.f, std::sin(a) * d);
        t.h = reed ? r.range(0.12f, 0.22f) : r.range(0.04f, 0.08f);
        t.seed = r.uniform();
        t.kind = reed ? 4 : 3;
        tufts_.push_back(t);
    }
    for (int i = 0; i < 0; i++) {
        Tuft t;
        const float x = r.range(2.f, 14.f);
        const float side = r.chance(0.5f) ? 1.f : -1.f;
        const float z = swgeo::riverZ(x) + side * (swgeo::riverHW(x) + r.range(-0.1f, 0.6f));
        t.p = vec3(x, 0.f, z);
        t.h = r.range(0.25f, 0.5f) * (0.8f + 0.05f * x);
        t.seed = r.uniform();
        t.kind = r.chance(0.5f) ? 4 : 3;
        tufts_.push_back(t);
    }
    // the clouds: toon cumulus over the country, around where the creature crosses their layer (~12 km east)
    Rng c(0xC10Dull);
    clouds_.clear();
    for (int k = 0; k < 200; k++) {
        const int band = k < 50 ? 0 : k < 150 ? 1 : 2;
        const float a = c.range(0.f, TAU);
        const float d = band == 0 ? c.range(500.f, 5000.f) : band == 1 ? c.range(5000.f, 18000.f) : c.range(18000.f, 45000.f);
        const float big = band == 0 ? c.range(90.f, 240.f) : band == 1 ? c.range(180.f, 420.f) : c.range(400.f, 900.f);
        const vec3 ctr(9000.f + std::cos(a) * d, CLOUD_Y + c.range(-250.f, 350.f), std::sin(a) * d);
        const int n = c.irange(5, 8);
        const float ca = c.range(0.f, TAU);
        const vec3 ax(std::cos(ca), 0.f, std::sin(ca)), az(-std::sin(ca), 0.f, std::cos(ca));
        for (int i = 0; i < n; i++) {
            const float u = (i - (n - 1) * 0.5f) / std::max(1.f, (n - 1) * 0.5f);
            Puff p;
            p.r = big * c.range(0.55f, 0.85f) * (1.f - 0.45f * std::fabs(u));
            p.p = ctr + ax * (u * big * 1.5f) + az * (c.range(-0.4f, 0.4f) * big) + vec3(0.f, c.range(0.f, 0.5f) * big * (1.f - std::fabs(u)) + p.r * 0.3f, 0.f);
            p.seed = c.uniform();
            clouds_.push_back(p);
        }
    }
}

void SkyWhaleUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    aspect_ = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the ray-traced sky at a reduced resolution (it is soft), with its depth in a texture
    const int sh = std::max(180, std::min(720, (int)(height_ * 0.66f)));
    const int sw = std::max(320, (int)std::lround((float)sh * width_ / height_));
    skyT_.create(sw, sh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
    if (!skyDepthTex_) glGenTextures(1, &skyDepthTex_);
    glBindTexture(GL_TEXTURE_2D, skyDepthTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, sw, sh, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, skyT_.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, skyDepthTex_, 0);
    // the geometry: multisampled (4x), at the full resolution up to 1440 lines
    const int wh = std::max(180, std::min(1440, height_));
    const int ww = std::max(320, (int)std::lround((float)wh * width_ / height_));
    {
        GLint maxS = 1;
        glGetIntegerv(GL_MAX_SAMPLES, &maxS);
        msSamples_ = std::max(1, std::min(4, (int)maxS));
        msW_ = ww;
        msH_ = wh;
        if (!msFbo_) { glGenFramebuffers(1, &msFbo_); glGenRenderbuffers(1, &msColor_); glGenRenderbuffers(1, &msDepth_); }
        glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, msSamples_, GL_RGBA16F, ww, wh);
        glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, msSamples_, GL_DEPTH_COMPONENT32F, ww, wh);
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::fprintf(stderr, "[skywhale] multisampled framebuffer incomplete (%dx%d x%d)\n", ww, wh, msSamples_);
    }
    worldT_.create(ww, wh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
    bloomA_.create(std::max(16, sw / 4), std::max(16, sh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(std::max(16, sw / 4), std::max(16, sh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

SkyWhaleUniverse::~SkyWhaleUniverse() {
    for (ugl::Target* t : {&skyT_, &worldT_, &bloomA_, &bloomB_, &sceneT_}) t->destroy();
    if (skyDepthTex_) glDeleteTextures(1, &skyDepthTex_);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    for (GLuint r : {msColor_, msDepth_})
        if (r) glDeleteRenderbuffers(1, &r);
    for (GLuint p : {progSky_, progWhale_, progProp_, progPuff_, progGlow_, progRibbon_, progCloud_, progSea_, progComp_,
                     progBright_, progBlur_, progSprite_, progVeil_, progCell_, progShape_, progText_, progPost_, progUp_})
        glDeleteProgram(p);
    for (GLuint v : {whaleVao_, lighthouse_.vao, toro_.vao, puffVao_, glowVao_, cloudVao_, ribbonVao_, spriteVao_, cellVao_,
                     shapeVao_, textVao_})
        glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, whaleVbo_, whaleIbo_, lighthouse_.vbo, toro_.vbo, puffVbo_, glowVbo_, cloudVbo_, ribbonVbo_,
                     spriteVbo_, cellVbo_, shapeVbo_, textVbo_})
        glDeleteBuffers(1, &b);
    for (Font* f : {&caps_, &ital_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The creature
// =====================================================================================================

// Lines -> G: the journey's table, then a slow climb on, up to G_MAX.
float SkyWhaleUniverse::growthOf(int lines) const {
    const float l = (float)std::max(0, lines);
    float h;
    if (l >= STAGES[STAGE_COUNT - 1].lines) {
        h = std::log(STAGES[STAGE_COUNT - 1].length) + (l - STAGES[STAGE_COUNT - 1].lines) * 0.012f;
    } else {
        int i = 0;
        while (i + 2 < STAGE_COUNT && STAGES[i + 1].lines <= l) i++;
        const float t = (l - STAGES[i].lines) / (STAGES[i + 1].lines - STAGES[i].lines);
        h = lerpf(std::log(STAGES[i].length), std::log(STAGES[i + 1].length), t);
    }
    return std::min(h, G_MAX);
}

// The cruise's geography: round the pond on a circle, down the river (held short of the sea until it is a whale),
// then on around the body it circles.
namespace {
constexpr float POND_CX = -0.3f, POND_CZ = 0.1f;
float pondCircleR(float L) { return 0.8f + 0.5f * smoothstepf(0.3f, 0.9f, L); }
float kPondPath(float L) { return 1.f - smoothstepf(0.6f, 0.95f, L); }
// the river's place (meters down it) from the length alone
double riverS(double L) { return L < 0.9 ? -0.5 + 2.9 * smoothstepf(0.3f, 0.9f, (float)L) : 2.4 + 9.0 * (L - 0.9); }
// how far ahead of that it may cruise (it stays in the river until it has grown into a whale)
float riverCap(float L) { return 0.6f * std::max(0.f, swgeo::COAST_X - 14.f - (float)riverS(L)); }
// out at sea and beyond: it cruises on around the planet
float kOpenOf(float G) { return smoothstepf(std::log(11.f), std::log(18.f), G); }
// p turned about c in the orbit's plane (z axis) so its angle (from +y toward +x) grows by a
vec3 turnAbout(const vec3& p, const vec3& c, double a) {
    const double dx = (double)p.x - c.x, dy = (double)p.y - c.y, ca = std::cos(a), sa = std::sin(a);
    return vec3((float)(c.x + dx * ca + dy * sa), (float)(c.y - dx * sa + dy * ca), p.z);
}
// polar coordinates about c in the orbit's plane: angle from +y toward +x, radius
void polar(const vec3& p, const vec3& c, double& a, double& r) {
    const double dx = (double)p.x - c.x, dy = (double)p.y - c.y;
    a = std::atan2(dx, dy);
    r = std::sqrt(dx * dx + dy * dy);
}
vec3 fromPolar(const vec3& c, double a, double r, float z) {
    return vec3((float)(c.x + r * std::sin(a)), (float)(c.y + r * std::cos(a)), z);
}
double wrapPi(double a) { return a - TAU * std::floor((a + PI) / TAU); }
} // namespace

// The path's center at G (meters), at a cruise: round the pond, down the river (its centerline) to the sea, then up
// into the sky (its altitude growing with the length) and around the planet (the growth's angle saturating, the
// cruise turning it on around), then over to the Moon, which it circles.
vec3 SkyWhaleUniverse::pathPosC(float G, const Cruise& cr) const {
    const double L = std::exp((double)G);
    const double s = riverS(L) + std::min(cr.riverC, riverCap((float)L));
    const double z = s > 0.0 ? swgeo::riverZ((float)s) * smoothstepf(0.5f, 2.5f, (float)s) : 0.0;
    const float hK = smoothstepf(std::log(150.f), std::log(700.f), G);
    const double h = L * lerpf(1.5f, 0.6f, smoothstepf(13.f, 18.f, G)) * hK;
    const double a = 0.35;
    const double phi = a * std::log1p(std::max(s, -0.9 * a * R_EARTH) / (a * R_EARTH));
    const double rr = R_EARTH + h;
    // y relative to the sea level at the origin, without cancellation
    const double sn = std::sin(phi);
    const double cm1 = -2.0 * std::sin(phi * 0.5) * std::sin(phi * 0.5); // cos(phi) - 1
    const double y = h + rr * cm1;
    vec3 p((float)(rr * sn), (float)y, (float)z);
    // the koi circles the pond
    const float kP = kPondPath((float)L);
    if (kP > 0.f) {
        const float R = pondCircleR((float)L);
        p = lerp(p, vec3(POND_CX + R * std::cos(cr.pondA), p.y, POND_CZ + R * std::sin(cr.pondA)), kP);
    }
    // the cruise around the planet
    const vec3 ce(0.f, -(float)R_EARTH, 0.f);
    if (cr.orbitA != 0.0) p = turnAbout(p, ce, cr.orbitA);
    // the end: it swims out to the Moon and circles it (around the Earth, then around the Moon, never through them)
    const float kM = moonK(G);
    if (kM > 0.f) {
        const double rhoM = R_MOON + 0.3 * L;
        const double ph = PHI_MOON + PI - 0.8 * (G - 16.9) + cr.orbitA;
        const vec3 mc = moonPos();
        const vec3 m = mc + vec3((float)(rhoM * std::sin(ph)), (float)(rhoM * std::cos(ph)), 0.f);
        const vec3 mid = lerp(ce, mc, 0.5f);
        if (kM < 0.5f) {
            const double k = smoothstepf(0.f, 0.5f, kM);
            double a0, r0, a1, r1;
            polar(p, ce, a0, r0);
            polar(mid, ce, a1, r1);
            p = fromPolar(ce, a0 + wrapPi(a1 - a0) * k, std::exp(lerpf((float)std::log(r0), (float)std::log(r1), (float)k)), lerpf(p.z, 0.f, (float)k));
        } else {
            const double k = smoothstepf(0.5f, 1.f, kM);
            double a0, r0, a1, r1;
            polar(mid, mc, a0, r0);
            polar(m, mc, a1, r1);
            p = fromPolar(mc, a0 + wrapPi(a1 - a0) * k, std::exp(lerpf((float)std::log(r0), (float)std::log(r1), (float)k)), 0.f);
        }
    }
    return p;
}

// The cruise's direction at G: its path's tangent along the cruise (round the pond, down the river, around).
vec3 SkyWhaleUniverse::travelDir(float G) const {
    const float L = std::exp(G);
    const vec3 ctr = orbitCenter(G);
    const vec3 c = pathPos(G);
    const double rho = std::max(1.0, (double)length(c - ctr));
    Cruise a = cr_, b = cr_;
    a.pondA += 0.02f; b.pondA -= 0.02f;
    a.riverC -= 0.02f * L; b.riverC += 0.02f * L;
    a.orbitA -= 0.02 * L / rho; b.orbitA += 0.02 * L / rho;
    vec3 d = pathPosC(G, b) - pathPosC(G, a);
    const vec3 up = normalize(c - ctr);
    // where the cruise is held (the river's cap) and in orbit: along the orbit (east)
    const vec3 along = normalize(cross(up, vec3(0.f, 0.f, 1.f)));
    d = d - up * dot(d, up);
    const float dl = length(d);
    if (dl < 1e-4f * L) return along;
    return normalize(lerp(along, d * (1.f / dl), smoothstepf(0.f, 0.01f * L, dl)));
}

// Advances the cruise by dt (its speed, speedL_, in body lengths a second).
void SkyWhaleUniverse::advanceCruise(float G, float dt) {
    const float L = std::exp(G);
    const float d = speedL_ * L * dt; // meters
    const float kP = kPondPath(L), kO = kOpenOf(G);
    cr_.pondA -= d * kP / pondCircleR(L);
    // down the river, slowing as it nears what it may reach before it is grown
    const float cap = riverCap(L);
    if (cr_.riverC > cap) cr_.riverC = cap;
    else if (cap > 0.f) cr_.riverC += d * (1.f - kP) * (1.f - kO) * std::max(0.08f, 1.f - cr_.riverC / cap);
    // around the planet (or the Moon)
    const double rho = std::max(1.0, (double)length(pathPos(G) - orbitCenter(G)));
    cr_.orbitA += (double)d * kO / rho;
}

// The wandering around the cruise's line (body lengths: along it, up, to its side), and its roll: gentle weaves and
// turns toward and away from the camera, rises and dives, in the sky a barrel roll now and then, in space a loop.
vec3 SkyWhaleUniverse::wander(float G, float t, float* roll) const {
    const float kPond = 1.f - smoothstepf(std::log(0.7f), std::log(1.2f), G);
    const float kWater = 1.f - smoothstepf(std::log(150.f), std::log(400.f), G);
    const float kSpace = smoothstepf(11.f, 13.f, G);
    const float kAir = (1.f - kWater) * (1.f - kSpace);
    vec3 w(0.f);
    // along: easing ahead and falling back a little
    w.x = 0.07f * std::sin(t * 0.23f + 0.4f) + 0.03f * std::sin(t * 0.51f);
    // to the side (toward and away from the camera): long weaves
    const float side = 0.22f * std::sin(t * 0.21f) + 0.08f * std::sin(t * 0.47f + 1.3f);
    w.z = side * lerpf(1.f, 0.8f, kPond) * (1.f + 0.5f * kAir);
    // up and down: just under the surface in the water, rises and dives in the air and in space
    w.y = 0.015f * std::sin(t * 0.37f) * kWater + (0.16f * std::sin(t * 0.17f + 2.f) + 0.05f * std::sin(t * 0.43f)) * (1.f - kWater);
    float r = 0.f;
    // a maneuver every ~26 units of its clock: a barrel roll in the air, a loop in space
    const float M = 26.f, dur = 7.f;
    const float ph = std::fmod(t + 13.f, M);
    if (kWater < 1.f && ph < dur) {
        const float e = smoothstepf(0.f, 1.f, ph / dur);
        const float k = 1.f - kWater;
        r += TAU * e * k * (1.f - kSpace);
        const float a = TAU * e;
        const float rad = 0.3f;
        w.x += kSpace * k * (rad * std::sin(a) * 1.2f + 0.3f * std::sin(PI * e));
        w.y += kSpace * k * rad * (1.f - std::cos(a));
    }
    if (roll) *roll = r;
    return w;
}

// The creature's pose at G: its path and cruise, its heading along them, and its own motion: circling the pond,
// swimming down the river, cruising and breaching at sea, flying in the air, bent around the planet in orbit; always
// weaving, rising, diving, banking into its turns.
SkyWhaleUniverse::Pose SkyWhaleUniverse::poseAt(float G) const {
    Pose P;
    const float L = std::exp(G);
    P.L = L;
    const vec3 c = pathPos(G);
    const vec3 ctr = orbitCenter(G);
    const vec3 up0 = normalize(c - ctr);
    vec3 h = travelDir(G);
    h = normalize(h - up0 * dot(h, up0));
    const vec3 side0 = normalize(cross(h, up0));
    const float kWater = 1.f - smoothstepf(std::log(150.f), std::log(400.f), G);
    const float kSea = smoothstepf(std::log(10.f), std::log(18.f), G) * kWater;
    const float kSpace = smoothstepf(11.f, 13.f, G);
    const float kPond = 1.f - smoothstepf(std::log(0.7f), std::log(1.2f), G);
    // the wandering, and its velocity and turn (for the heading and the banking)
    const float rate = (0.7f + 0.6f * pace_) * (1.f + 0.5f * surge_);
    const float e = 0.08f;
    float roll = 0.f, rollA = 0.f, rollB = 0.f;
    const vec3 w0 = wander(G, swimT_, &roll);
    const vec3 wa = wander(G, swimT_ - e, &rollA), wb = wander(G, swimT_ + e, &rollB);
    auto toWorld = [&](const vec3& w) { return h * w.x + up0 * w.y + side0 * w.z; };
    vec3 vel = h * speedL_ + toWorld(wb - wa) * (rate / (2.f * e));          // body lengths a second
    const vec3 acc = toWorld(wa + wb - w0 * 2.f) * (rate * rate / (e * e));
    vec3 offs = toWorld(w0) * L;
    P.wo = offs;
    // the depth: just under the surface in the pond and the river, the back out of the water at sea
    offs = offs + up0 * (L * lerpf(lerpf(-0.1f, -0.055f, smoothstepf(-0.5f, 1.5f, G)), 0.06f, kSea) * kWater);
    if (kSea > 0.f) {
        const float b = breachY(breachPh_);
        const float eb = 0.01f;
        const float db = (breachY(breachPh_ + eb) - breachY(breachPh_ - eb)) / (2.f * eb) / 11.f; // body lengths a second
        offs = offs + up0 * (L * b * kSea);
        vel = vel + up0 * (db * kSea);
        // a leap carries it forward
        const float p = breachPh_ - std::floor(breachPh_);
        offs = offs + h * (L * 0.1f * kSea * smoothstepf(0.66f, 0.94f, p));
    }
    // the game over: sinking into the deep
    offs = offs - up0 * (L * 0.9f * sink_ * (0.4f + 0.6f * kWater));
    P.c = c + offs;
    // the axes: along its velocity (a loop turns it over), banking into its turns, rolling now and then
    const vec3 f = normalize(vel + h * 1e-4f);
    vec3 side = side0 - f * dot(f, side0);
    side = length(side) > 1e-4f ? normalize(side) : normalize(cross(f, up0));
    const vec3 upv = cross(side, f);
    const float spd = std::max(length(vel), 0.05f);
    float bank = std::clamp(-dot(acc, side) / (spd * 2.2f), -0.7f, 0.7f) * (1.f - 0.5f * kSea);
    bank += 0.22f * kPond; // round the pond's circle
    const float rl = bank + roll;
    const vec3 side2 = side * std::cos(rl) + upv * std::sin(rl);
    P.fx = f;
    P.fz = normalize(side2);
    P.fy = normalize(cross(P.fz, P.fx));
    const double rho = length(c - ctr);
    P.bend = kSpace * (float)std::min(2.6, (double)L / rho);
    return P;
}

// =====================================================================================================
// Camera
// =====================================================================================================

// Rides with the creature, a moment behind it: close over the pond looking down through the water at the koi, low
// over the river, at the sea's level for the breaches with the lighthouse behind, alongside it up through the
// clouds, looking down at the curving horizon, then at the planet a ball under it. The world is drawn mirrored (the
// final composite flips it), so the creature, kept right of the well here, swims left to right on the left of the
// well. Only this camera moves: the board is drawn on its own. The world is drawn relative to the eye.
void SkyWhaleUniverse::updateCamera(float dt) {
    if (gc_ < -50.f) { gc_ = g_; gcVel_ = 0.f; }
    {
        const float w = 3.6f;
        gcVel_ += (w * w * (g_ - gc_) + 2.f * w * (0.85f * gVel_ - gcVel_)) * dt;
        gc_ += gcVel_ * dt;
    }
    const float G = gc_;
    const float L = std::exp(G);
    static const float K[] = {-1.2f, 0.f, 1.6f, 2.7f, 3.9f, 5.0f, 6.5f, 7.6f, 10.f, 12.5f, 14.5f, 16.5f, 18.5f, 21.f};
    static const float PITCH[] = {-50.f, -44.f, -30.f, -14.f, -11.f, -9.f, 2.f, 0.f, -14.f, -22.f, -24.f, -20.f, -14.f, -12.f};
    static const float FR[] = {1.05f, 0.95f, 0.8f, 0.72f, 0.78f, 0.78f, 0.72f, 0.72f, 0.72f, 0.72f, 0.75f, 0.78f, 0.8f, 0.8f};
    static const float PSI[] = {0.05f, -0.15f, -0.3f, -0.5f, -0.55f, -0.55f, -0.5f, -0.45f, -0.35f, 0.1f, 0.12f, 0.1f, 0.1f, 0.1f};
    static const float SY[] = {-0.04f, 0.f, 0.f, 0.06f, 0.08f, 0.1f, 0.12f, 0.15f, 0.2f, 0.22f, 0.2f, 0.12f, 0.05f, 0.f};
    const int n = (int)(sizeof(K) / sizeof(K[0]));
    const float pitch = keyed(G, K, PITCH, n) * PI / 180.f;
    const float psi = keyed(G, K, PSI, n) + 0.03f * std::sin(time_ * 0.021f);
    // (the world is drawn mirrored: the creature sits in the free band left of the HUD's left column)
    const float sy = keyed(G, K, SY, n);
    tanH_ = std::tan(27.f * PI / 180.f);
    const float colEdge = -layout::LEFT_COLUMN[0]; // board units from the center
    const float sx = std::clamp(0.5f * (colEdge / std::max(viewH_.x, 1.f) + 1.f) - 0.06f, 0.55f, 0.75f);
    // the anchor: the path and its cruise (not the breaches: the creature leaps out of the frame and back), and a
    // part of its wandering, a moment behind
    vec3 anchor = pathPos(G);
    const float kWater = 1.f - smoothstepf(std::log(150.f), std::log(400.f), G);
    const float kSea = smoothstepf(std::log(10.f), std::log(18.f), G) * kWater;
    {
        const vec3 up = normalize(anchor - orbitCenter(G) + vec3(0.f, 1e-3f, 0.f));
        anchor = anchor + up * (L * lerpf(-0.08f, 0.08f, kSea) * kWater);
        camWo_ = lerp(camWo_, pose_.wo * 0.45f, 1.f - std::exp(-dt * 1.2f));
        anchor = anchor + camWo_;
    }
    // pulled back: the creature smaller in a wider scene
    float F = keyed(G, K, FR, n) * L * lerpf(1.6f, 1.85f, smoothstepf(9.f, 12.f, G));
    // far out: the planet comes into the frame (and the moon's orbit at the end)
    {
        const float kE = smoothstepf(15.f, 17.f, G);
        if (kE > 0.f) {
            const vec3 ce = orbitCenter(G);
            const float rho = length(anchor - ce);
            anchor = lerp(anchor, ce + (anchor - ce) * 0.4f, kE * 0.8f);
            F = lerpf(F, std::max(F, rho * 1.0f), kE);
        }
    }
    // the local frame at the anchor (up: away from the planet's center; ahead: the cruise's bearing, smoothed), so
    // the camera keeps its bearings round the pond, down the river and all the way around the planet
    const vec3 upL = normalize(anchor - orbitCenter(G) + vec3(0.f, 1e-3f, 0.f));
    {
        vec3 h = travelDir(G);
        h = h - upL * dot(h, upL);
        if (length(h) < 1e-4f) h = normalize(cross(upL, vec3(0.f, 0.f, 1.f)));
        h = normalize(h);
        if (!camHSet_) { camH_ = h; camHSet_ = true; }
        camH_ = camH_ - upL * dot(camH_, upL);
        camH_ = normalize(lerp(length(camH_) > 1e-4f ? normalize(camH_) : h, h, 1.f - std::exp(-dt * 1.5f)));
    }
    const vec3 northL = normalize(cross(camH_, upL));
    const vec3 eastL = cross(upL, northL);
    const vec3 lookH = eastL * std::sin(psi) + northL * std::cos(psi);
    vec3 fwd = normalize(lookH * std::cos(pitch) + upL * std::sin(pitch));
    vec3 right = normalize(cross(fwd, upL));
    vec3 upv = cross(right, fwd);
    camUp_ = upL;
    const vec3 dirA = normalize(fwd + right * (sx * tanH_ * aspect_) + upv * (sy * tanH_));
    const float D = F / tanH_;
    vec3 eye = anchor - dirA * D;
    // above the water and the ground (the planet's curve, locally)
    if (L < 2000.f) {
        const float curve = (eye.x * eye.x + eye.z * eye.z) / (2.f * (float)R_EARTH);
        eye.y = std::max(eye.y, 0.06f + 0.12f * std::min(L, 300.f) * kWater - curve);
    }
    eye_ = eye;
    look_ = fwd;
    camR_ = right;
    camU_ = upv;
    {
        const vec3 oc(eye.x, eye.y + (float)R_EARTH, eye.z);
        alt_ = std::max(0.f, length(oc) - (float)R_EARTH);
        if (length(oc) < 1e7f) alt_ = std::max(0.f, eye.y + (eye.x * eye.x + eye.z * eye.z) / (2.f * (float)R_EARTH));
    }
    const float zn = std::max(0.01f, D * 0.02f), zf = std::max({1.6e5f, D * 80.f, L * 6.f});
    const mat4 view3 = mat4::lookAt(vec3(0.f), fwd, upL);
    proj3_ = mat4::perspective(2.f * std::atan(tanH_), aspect_, zn, zf);
    vp3_ = proj3_ * view3;
}

vec2 SkyWhaleUniverse::toBoard(const vec3& w) const {
    const vec4 c = vp3_ * vec4(w - eye_, 1.f);
    // (the world is drawn mirrored)
    if (c.w <= 1e-4f) return V(viewC_.x - viewH_.x, viewC_.y);
    return V(viewC_.x - c.x / c.w * viewH_.x, viewC_.y + c.y / c.w * viewH_.y);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void SkyWhaleUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && !t.minor && t.brightness > 0.6f) v = 2;            // dawn for bright, major songs
        else if (t.hasSong && t.minor && t.brightness < 0.42f) v = 1;       // moonlight for dark ones
        else if (t.hasSong && t.airWeight > 0.62f && t.density < 0.45f) v = 3; // the aurora for airy, sparse ones
        else {
            static const int BY_HUE[] = {0, 2, 1, 0, 3, 0};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x77EEull);
    if (koiSeed_ == 1) { koiSeed_ = seed_; koiSeedF_ = (float)(splitmix64(koiSeed_) % 1000) / 1000.f; }
    if (seconds <= 0.f) palSet_ = false;
}

void SkyWhaleUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropMode_ = false;
    (void)impact;
    const bool instant = !hasLevel_ || seconds < 0.1f;
    if (instant) {
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
}

// The drop: the song (only on a real drop: see render).
void SkyWhaleUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void SkyWhaleUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

void SkyWhaleUniverse::levelUp() { levelGlow_ = 1.f; }

vec2 SkyWhaleUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints SkyWhaleUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = lerp(p.zenith, p.horizon, 0.45f) * (p.night > 0.5f ? 1.f : 0.75f);
    h.light = false;
    h.ink = p.ink;
    h.accent = p.glow;
    h.styles = TS_RIPPLE | TS_INK | TS_LUMA;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void SkyWhaleUniverse::syncIds(const Game& g) {
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

void SkyWhaleUniverse::addMote(const Mote& m) {
    if ((int)motes_.size() >= MAX_MOTES) return;
    motes_.push_back(m);
}

void SkyWhaleUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::HardDrop: {
        // a few bubbles pushed out from under the piece, rising
        float yLo = 99.f;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f) continue;
            for (int i = 0; i < 2; i++) {
                Mote m;
                m.p = V(p.x + rng_.range(-0.45f, 0.45f), p.y - 0.45f);
                m.v = V(rng_.range(-0.5f, 0.5f), rng_.range(0.4f, 1.1f));
                m.life = rng_.range(0.7f, 1.2f);
                m.size = rng_.range(0.07f, 0.13f);
                m.col = lerp(pal_.piece[std::clamp(c.type, 0, 6)], vec3(1.f), 0.5f);
                m.kind = 3;
                m.seed = rng_.uniform();
                addMote(m);
            }
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        const int lines = std::max(1, ev.count);
        // the cells melt into light that flows out of the well to the creature's head
        vec2 to = toBoard(pose_.c + pose_.fx * (pose_.L * 0.25f));
        to.x = std::clamp(to.x, -viewH_.x + 1.f, viewH_.x - 1.f);
        to.y = std::clamp(to.y, viewC_.y - viewH_.y + 1.f, viewC_.y + viewH_.y - 1.f);
        const float side = to.x >= 0.f ? 1.f : -1.f;
        for (auto& cl : ev.cells) {
            if (cl.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)cl.x, (float)cl.y);
            const int id = ids_[cl.y][cl.x];
            int m = 0;
            if (id) {
                const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
                for (int i = 0; i < 8; i++) {
                    const int nx = cl.x + dx[i], ny = cl.y + dy[i];
                    if (nx < 0 || nx >= Game::W || ny < 0 || ny >= Game::H || !rows[ny]) continue;
                    if (ids_[ny][nx] == id) m |= 1 << i;
                }
            }
            fading_.push_back({p.x, p.y, std::clamp(cl.type, 0, 6), m, 0.f});
            for (int i = 0; i < 2; i++) {
                Mote mt;
                mt.kind = 1;
                mt.from = V(p.x + rng_.range(-0.3f, 0.3f), p.y + rng_.range(-0.3f, 0.3f));
                mt.to = add(to, V(rng_.range(-1.5f, 1.5f), rng_.range(-1.2f, 1.2f)));
                mt.ctl = V(side * rng_.range(6.f, 8.f), std::max(mt.from.y, mt.to.y) + rng_.range(1.f, 4.f));
                mt.p = mt.from;
                mt.life = FLOW_T * rng_.range(0.8f, 1.1f);
                mt.age = -rng_.range(0.f, 0.25f) - (float)(side > 0 ? Game::W - 1 - cl.x : cl.x) * 0.015f;
                mt.size = rng_.range(0.1f, 0.17f);
                mt.col = lerp(pal_.piece[std::clamp(cl.type, 0, 6)], pal_.glow, 0.5f);
                mt.bright = 1.2f;
                mt.seed = rng_.uniform();
                addMote(mt);
            }
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
        // a stroke of the tail: the creature surges, glints scatter from it
        stroke_ = std::max(stroke_, 0.7f + 0.15f * lines);
        surgeIn_ = std::min(2.f, surgeIn_ + 0.45f + 0.2f * lines);
        sparkAcc_ += 6.f * lines;
        if (lines >= 4) {
            burst_ = 1.f;
            stroke_ = 1.6f;
            // at sea: it breaches at once
            const float p = breachPh_ - std::floor(breachPh_);
            if (p < 0.55f || p > 0.96f) breachKick_ = 1.f;
            // a ring of light, sparkles
            Ring rg;
            rg.c = pose_.c + pose_.fx * (pose_.L * 0.2f);
            rg.n = normalize(lerp(pose_.fx, normalize(eye_ - rg.c), 0.6f));
            rg.age = 0.f; rg.life = 3.f; rg.r0 = pose_.L * 0.2f; rg.r1 = pose_.L * 0.75f; rg.hue = rng_.uniform();
            rings_.push_back(rg);
            sparkAcc_ += 60.f;
        }
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
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
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void SkyWhaleUniverse::cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase,
                            float shine) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, shine, 0.f}});
}

void SkyWhaleUniverse::buildBoard(const Game& g) {
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
    // game over: the stack's color drains from the top down
    if (overT_ >= 0.f)
        for (const Fading& o : overCells_) {
            const float rowFrac = (o.t - Game::HIDDEN) / (float)(Game::H - Game::HIDDEN);
            const float k = smoothstepf(rowFrac * 0.6f, rowFrac * 0.6f + 0.3f, over_);
            cell(o.x, o.y, 0.5f, o.mask, tintOf(o.type), 1.f - 0.5f * smoothstepf(0.7f, 1.f, over_), 0, 0.f, 0.f);
            cells_.back().c[3] = k;
        }
    for (const Fading& f : fading_) cell(f.x, f.y, 0.5f, f.mask, tintOf(f.type), 1.f, 0, saturate(f.t / CLEAR_T), 0.f);
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
                cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), tintOf(p.type), 0.5f, 1, 0.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), tintOf(p.type), 1.f, 0, 0.f, 0.f);
        }
    }
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

void SkyWhaleUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param, float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

// =====================================================================================================
// Text
// =====================================================================================================

float SkyWhaleUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void SkyWhaleUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void SkyWhaleUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float halo) {
    if (q.empty() || !f.ok || alpha <= 0.004f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    set3f(progText_, "uInk", lerp(ink, bridge_.ink, b));
    set3f(progText_, "uHalo", lerp(pal_.veil * 0.6f, bridge_.color, b));
    set1f(progText_, "uHaloA", halo);
    set1f(progText_, "uGlowA", 0.45f * (1.f - b));
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD: Quicksand capitals, softly spaced; the song and the notices in Fraunces' soft italics.
void SkyWhaleUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        std::vector<float> v;
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal_.accent : pal_.veil;
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
    const float halo = pal_.night > 0.5f ? 0.6f : 0.8f;
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
            const float sz = base * 0.92f, tr = sz * 0.3f;
            const float wName = textWidth(caps_, name, sz, tr);
            std::string vt = var;
            for (size_t i = 1; i < vt.size(); i++) vt[i] = (char)std::tolower((unsigned char)vt[i]);
            const float vsz = base * 1.15f;
            const float wVar = vt.empty() ? 0.f : textWidth(ital_, vt, vsz, 0.f);
            const float gap = base * 0.7f;
            const float x = ugl::hudLeft(h, wName + (vt.empty() ? 0.f : gap + wVar), width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            textQuads(caps_, name, x, h.y, sz, tr, q);
            drawText(caps_, q, ink, a, halo);
            if (!vt.empty()) {
                q.clear();
                textQuads(ital_, vt, x + wName + gap, h.y - sz * 0.12f, vsz, 0.f, q);
                drawText(ital_, q, accent, a * 0.9f, halo);
            }
            continue;
        }
        case HUD_SECTION:
            place(caps_, h.text, base * 0.82f, base * 0.48f);
            drawText(caps_, q, ink, saturate(0.2f + h.alpha * 1.9f), halo);
            continue;
        case HUD_TITLE:
            place(ital_, h.text, base * 1.35f, base * 0.01f, -base * 0.2f);
            drawText(ital_, q, ink, saturate(h.alpha * 1.3f), halo);
            continue;
        case HUD_INFO:
            place(caps_, h.text, base * 0.66f, base * 0.22f);
            drawText(caps_, q, ink, saturate(alpha * 1.3f) * 0.85f, halo);
            continue;
        case HUD_LABEL:
            place(caps_, h.text, base * 0.68f, base * 0.4f);
            drawText(caps_, q, lerp(ink, accent, 0.45f), saturate(alpha * 1.2f) * 0.92f, halo);
            continue;
        case HUD_VALUE:
            place(caps_, h.text, base * 1.05f, base * 0.06f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_NOTICE:
            place(ital_, h.text, base * 1.3f, base * 0.01f);
            drawText(ital_, q, accent, alpha, halo);
            continue;
        case HUD_KEY:
            place(caps_, h.text, base * 0.76f, base * 0.1f);
            drawText(caps_, q, ink, alpha, halo);
            continue;
        case HUD_TOAST:
            place(caps_, h.text, base * 1.15f, base * 0.48f, -base * 0.15f);
            drawText(caps_, q, ink, alpha, 0.85f);
            continue;
        case HUD_TOAST_SUB:
            place(ital_, h.text, base * 1.15f, base * 0.01f);
            drawText(ital_, q, accent, alpha * 0.9f, 0.85f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(caps_, h.text, base * 1.05f, base * 0.2f, -base * 0.1f);
                drawText(caps_, q, ink, alpha, 0.85f);
            } else {
                place(caps_, h.text, base * 0.76f, base * 0.12f);
                drawText(caps_, q, h.accent ? accent : ink, alpha, 0.85f);
            }
            continue;
        default:
            place(caps_, h.text, base * 0.76f, base * 0.1f);
            drawText(caps_, q, h.accent ? accent : ink, alpha, halo);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void SkyWhaleUniverse::render(const Frame& f, GLuint fbo) {
    // ZEN_GPUTIME: the GPU time of a frame, printed every 240 frames
    static const bool gpuTime = std::getenv("ZEN_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    if (!f.bare) syncIds(game); // (a break's blank board keeps the stack's ids)
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) { // (a break's blank board is never over)
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 3.f) : 0.f, overT_ >= 0.f ? 10.f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    time_ += wdt;

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

    // ---- The game's lines -> the growth (a new game hatches a new koi).
    {
        if (!f.bare) here_ = linesHere(game);
        const int lines = here_ + extraLines(time_); // (a break: the growth as it was)
        if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
            koiSeed_ = splitmix64(koiSeed_ + 0x9E37ull);
            koiSeedF_ = (float)(koiSeed_ % 1000) / 1000.f;
            gBonus_ = 0.f;
            g_ = growthOf(lines);
            gVel_ = 0.f;
            hatchT_ = 0.f;
            cr_ = Cruise{};
            surge_ = surgeIn_ = 0.f;
            sparks_.clear();
            rings_.clear();
        }
        lines_ = lines;
        if (isDrop && !wasDrop_) {
            // the drop: a great stroke, a surge of growth
            gBonus_ += 0.3f;
            stroke_ = std::max(stroke_, 1.5f);
        }
        gTarget_ = std::min(G_MAX, growthOf(lines) + gBonus_);
        if (linesShown_ < 0) { g_ = gTarget_; gVel_ = 0.f; }
        linesShown_ = lines;
        const float w = 3.8f;
        gVel_ += (w * w * (gTarget_ - g_) - 2.f * w * gVel_) * wdt;
        g_ += gVel_ * wdt;
        if (g_ > gTarget_ + 0.001f && gVel_ > 0.f) { g_ = gTarget_; gVel_ = 0.f; }
        hatchT_ += dt;
    }
    sink_ = approach(sink_, overT_ >= 0.f ? 1.f : 0.f, overT_ >= 0.f ? 0.25f : 2.f, dt);
    if (overT_ < 0.f && hatchT_ < 0.1f) sink_ = 0.f;
    stroke_ = approach(stroke_, 0.f, 0.8f, wdt);
    burst_ = approach(burst_, 0.f, 0.6f, dt);
    levelGlow_ = approach(levelGlow_, 0.f, 0.5f, dt);
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.5f : 0.35f, dt);
    const float life = smoothstepf(0.f, 1.f, dropAmt_);
    sing_ = approach(sing_, isDrop ? 1.f : 0.f, isDrop ? 1.2f : 0.3f, dt);
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
        if (slow_ > 0.01f) {
            // slow motion: a hush, the light softer and bluer, the creature's lights brighter
            const float k = slow_;
            pal_.sunGlow = lerp(pal_.sunGlow, pal_.sunGlow * vec3(0.75f, 0.9f, 1.25f), k);
            pal_.haze = lerp(pal_.haze, pal_.haze * vec3(0.8f, 0.9f, 1.15f), k);
            pal_.lit = lerp(pal_.lit, pal_.lit * vec3(0.85f, 0.95f, 1.15f), k);
            pal_.glowK += 0.5f * k;
        }
    }
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);

    // ---- The swim: a traveling wave, livelier with the music, a powerful stroke on each clear.
    const float G = g_;
    const float L = std::exp(G);
    const float m = morphOf(G);
    const float kWater = 1.f - smoothstepf(std::log(150.f), std::log(400.f), G);
    const float kSea = smoothstepf(std::log(10.f), std::log(18.f), G) * kWater;
    const float kSpace = smoothstepf(11.f, 13.f, G);
    {
        const float omega = lerpf(3.2f, 1.5f, m) * (0.75f + 0.25f * wm + 0.4f * wp + 0.2f * life) * (1.f - 0.6f * sink_);
        swimPh_ += wdt * (omega + 2.6f * stroke_);
        finPh_ += wdt * lerpf(2.2f, 1.1f, m) * (0.8f + 0.2f * wm + 0.3f * wp);
        singPh_ += wdt * 0.45f;
        // the breaching cycle at sea (a Tetris starts a leap at once)
        if (kSea > 0.01f) {
            const float p0 = breachPh_ - std::floor(breachPh_);
            breachPh_ += wdt / 11.f;
            if (breachKick_ > 0.f) { breachPh_ = std::floor(breachPh_) + 0.6f; breachKick_ = 0.f; }
            const float p1 = breachPh_ - std::floor(breachPh_);
            // a splash and spray as it bursts out (and falls back)
            auto splash = [&](float n) {
                splashC_ = pose_.c;
                splashAge_ = 0.f;
                for (int i = 0; i < (int)n && (int)sparks_.size() < MAX_SPARKS; i++) {
                    Spark s;
                    const float a = rng_.range(0.f, TAU), d = rng_.range(0.f, 0.12f) * L;
                    s.p = vec3(pose_.c.x + std::cos(a) * d + pose_.fx.x * rng_.range(-0.4f, 0.4f) * L, 0.f,
                               pose_.c.z + std::sin(a) * d + pose_.fx.z * rng_.range(-0.4f, 0.4f) * L);
                    s.v = vec3(std::cos(a) * rng_.range(0.05f, 0.3f), rng_.range(0.3f, 0.75f), std::sin(a) * rng_.range(0.05f, 0.3f)) * L;
                    s.age = -rng_.range(0.f, 0.2f);
                    s.life = rng_.range(1.0f, 1.8f);
                    s.size = L * rng_.range(0.008f, 0.02f);
                    s.seed = rng_.uniform();
                    s.spin = 0.f;
                    s.kind = 13;
                    s.col = lerp(vec3(1.f), pal_.water, 0.2f);
                    sparks_.push_back(s);
                }
            };
            if (p0 < 0.69f && p1 >= 0.69f) splash(90.f * kSea);
            if (p0 < 0.93f && p1 >= 0.93f) splash(140.f * kSea);
        }
        splashAge_ += wdt;
    }
    // ---- The cruise: it always swims on, its pace from the music's energy, a clear's surge on top (smooth both ways).
    {
        const float en = saturate(music.hasSong ? music.energy : energy_);
        pace_ += (en - pace_) * (1.f - std::exp(-dt * 0.6f));
        surge_ += (std::min(surgeIn_, 2.f) - surge_) * (1.f - std::exp(-wdt * 3.f));
        surgeIn_ *= std::exp(-wdt * 0.8f);
        const float kPondS = 1.f - smoothstepf(std::log(0.7f), std::log(1.2f), G);
        const float base = lerpf(lerpf(lerpf(0.3f, 0.34f, kSea), 0.42f, 1.f - kWater), 0.32f, kSpace) * lerpf(1.f, 0.9f, kPondS);
        speedL_ = base * (0.6f + 0.7f * pace_ + 0.1f * wm + 0.15f * wp + 0.2f * life) * (1.f + 1.5f * surge_) * (1.f - 0.85f * sink_);
        advanceCruise(G, wdt);
        swimT_ += wdt * (0.7f + 0.6f * pace_) * (1.f + 0.5f * surge_);
        swimPh_ += wdt * 2.2f * surge_;
    }
    pose_ = poseAt(G);
    updateCamera(dt);
    const float spaceK = smoothstepf(1.2e4f, 9.e4f, alt_);

    // ---- Particles in the world: glints around it, spray, streaks, rings of song.
    {
        auto bodyPoint = [&](float spread) {
            const float u = rng_.range(-0.45f, 0.45f);
            return pose_.c + pose_.fx * (u * L) + pose_.fy * (rng_.range(-1.f, 1.f) * spread * L) + pose_.fz * (rng_.range(-1.f, 1.f) * spread * L);
        };
        const float glintRate = (0.6f + 3.f * wm + 10.f * wp + 14.f * life) * (0.4f + 0.6f * std::max(pal_.night, std::max(spaceK, 1.f - kWater)));
        if (!f.paused) {
            sparkAcc_ += glintRate * wdt;
            streakAcc_ += std::min(std::max(0.f, gVel_ - 0.08f) * 60.f, 40.f) * wdt;
        }
        sparkAcc_ = std::min(sparkAcc_, 160.f);
        while (sparkAcc_ >= 1.f && (int)sparks_.size() < MAX_SPARKS) {
            sparkAcc_ -= 1.f;
            Spark s;
            s.p = bodyPoint(0.25f + 0.3f * burst_);
            s.v = (pose_.fy * rng_.range(0.0f, 0.05f) - pose_.fx * rng_.range(0.05f, 0.2f)) * L + (burst_ > 0.2f ? normalize(s.p - pose_.c) * (L * 0.4f * burst_) : vec3(0.f));
            s.age = 0.f;
            s.life = rng_.range(1.2f, 2.6f);
            s.size = L * rng_.range(0.012f, 0.03f);
            s.seed = rng_.uniform();
            s.spin = 0.f;
            s.kind = rng_.chance(0.35f) ? 9 : 2;
            s.col = lerp(pal_.glow, vec3(1.f), rng_.range(0.1f, 0.5f)) * 1.3f;
            // in the water: glints are bubbles of light rising
            sparks_.push_back(s);
        }
        // streaks rushing past a surge
        streakAcc_ = std::min(streakAcc_, 30.f);
        while (streakAcc_ >= 1.f && (int)sparks_.size() < MAX_SPARKS) {
            streakAcc_ -= 1.f;
            Spark s;
            s.p = bodyPoint(1.2f) + pose_.fx * (L * rng_.range(-0.2f, 0.8f));
            s.v = pose_.fx * (-L * 1.6f);
            s.age = 0.f;
            s.life = rng_.range(0.35f, 0.7f);
            s.size = L * rng_.range(0.01f, 0.02f);
            s.seed = rng_.uniform();
            s.spin = 0.f;
            s.kind = 8;
            s.col = lerp(vec3(1.f), pal_.glow, 0.3f);
            sparks_.push_back(s);
        }
        for (Spark& s : sparks_) {
            s.age += wdt;
            if (s.age < 0.f) continue;
            s.p += s.v * wdt;
            if (s.kind == 13) {
                s.v.y -= 1.4f * L * wdt;     // spray falls back
                if (s.p.y < 0.f && s.v.y < 0.f) s.life = std::min(s.life, s.age);
            } else if (s.kind != 8) s.v = s.v * std::exp(-0.8f * wdt);
        }
        sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& s) { return s.age >= s.life; }), sparks_.end());
        // the song: a ring of light from its head every two beats (on the beat grid), slow and soft
        if (isDrop && !f.paused) {
            bool fire = false;
            if (music.hasSong) {
                const double half = std::floor(music.beat / 2.0);
                if (half != lastRingBeat_) { fire = true; lastRingBeat_ = half; }
            } else {
                ringClock_ += dt;
                if (ringClock_ > 1.f) { ringClock_ -= 1.f; fire = true; }
            }
            if (fire && rings_.size() < 24) {
                Ring rg;
                rg.c = pose_.c + pose_.fx * (L * 0.3f);
                rg.n = normalize(lerp(pose_.fx, normalize(eye_ - rg.c), 0.55f));
                rg.age = 0.f;
                rg.life = 3.6f;
                rg.r0 = L * 0.12f;
                rg.r1 = L * lerpf(0.62f, 0.5f, kSpace);
                rg.hue = std::fmod((float)rings_.size() * 0.13f + time_ * 0.05f, 1.f);
                rings_.push_back(rg);
            }
        }
        if (levelGlow_ > 0.99f) {
            Ring rg;
            rg.c = pose_.c;
            rg.n = normalize(eye_ - rg.c);
            rg.age = 0.f; rg.life = 3.f; rg.r0 = L * 0.25f; rg.r1 = L * 0.7f; rg.hue = 0.3f;
            rings_.push_back(rg);
            levelGlow_ = 0.98f;
        }
        for (Ring& r : rings_) r.age += wdt;
        rings_.erase(std::remove_if(rings_.begin(), rings_.end(), [](const Ring& r) { return r.age >= r.life; }), rings_.end());
    }

    // ---- 2D motes over the board.
    for (Mote& mo : motes_) {
        mo.age += dt;
        if (mo.kind == 1) {
            const float t = smoothstepf(0.f, 1.f, saturate(mo.age / mo.life));
            const vec2 a = lerp2(mo.from, mo.ctl, t), b = lerp2(mo.ctl, mo.to, t);
            mo.p = lerp2(a, b, t);
        } else {
            mo.p = add(mo.p, mul(mo.v, dt));
            if (mo.kind == 3) mo.v = mul(mo.v, std::exp(-1.5f * dt));
        }
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& x) { return x.age >= x.life; }), motes_.end());
    for (auto& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    sprites_.clear();
    for (const Mote& mo : motes_) {
        if (mo.age < 0.f || f.bare) continue; // (the clears' motes: none in a break)
        const float t = mo.age / mo.life;
        float a = smoothstepf(0.f, 0.12f, t) * (1.f - smoothstepf(0.6f, 1.f, t)) * mo.bright;
        if (mo.kind == 1) a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.85f, 1.f, t)) * mo.bright;
        sprite(mo.p, mo.size * (mo.kind == 1 ? 2.1f : 1.6f), mo.col, a * world, mo.kind == 3 ? 3 : 0);
    }
    if (!f.bare) {
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const bool on = game.bonusReady() || game.bonusActive();
        sprite(V(gc.x, gc.y), layout::GAUGE_R * 1.25f, on ? pal_.glow : pal_.accent, 1.f, 1, fill, on ? 1.f : 0.f);
    }
    cells_.clear();
    if (!f.bare) buildBoard(game); // (a break: the world alone)

    // =========================================================== the world (the sky reduced, the geometry multisampled)
    // the sun's azimuth is given from the camera's view (so each variant keeps its light whatever the stage)
    // the sun's azimuth is given from the view (0: straight ahead, behind the creature; negative: to the left), so each
    // variant keeps its light at every stage
    vec3 sunDir;
    {
        const vec3 lh = normalize(look_ - camUp_ * dot(look_, camUp_));
        const vec3 rh = normalize(cross(lh, camUp_));
        const float az = lerpf(pal_.sunAz, pal_.sunAz < 0.f ? -2.1f : 2.1f, smoothstepf(10.f, 13.f, g_));
        const float el = lerpf(pal_.sunEl, 0.35f, smoothstepf(10.f, 13.f, g_));
        sunDir = normalize((lh * std::cos(az) + rh * std::sin(az)) * std::cos(el) + camUp_ * std::sin(el));
    }
    const float glowK = (pal_.glowK * (0.6f + 0.4f * m) + 0.6f * spaceK + 0.15f * wm + 0.4f * wp + 1.1f * sing_ + 0.8f * burst_ + 0.4f * levelGlow_) * (1.f - 0.7f * sink_);
    const vec3 head = pose_.c + pose_.fx * (0.5f * L), tail = pose_.c - pose_.fx * (0.5f * L);
    auto setLight = [&](GLuint p) {
        set3f(p, "uEye", eye_);
        set3f(p, "uSunDir", sunDir);
        set3f(p, "uSunC", lerp(pal_.lit, vec3(1.f, 0.97f, 0.93f) * 1.05f, 0.6f * spaceK) * (1.f + 0.1f * wp + 0.15f * life));
        set3f(p, "uShadowC", lerp(pal_.shadow, vec3(0.05f, 0.07f, 0.2f), 0.5f * spaceK));
        set3f(p, "uSunGlow", pal_.sunGlow * (1.f + 0.15f * wp + 0.25f * life));
        set3f(p, "uHazeC", pal_.haze);
        set3f(p, "uRimC", pal_.rim);
        set1f(p, "uFogK", 2.2e-5f);
        set1f(p, "uAlt", alt_);
        set1f(p, "uNight", pal_.night);
        set1f(p, "uTime", time_);
        set1f(p, "uBridge", bridge);
        set3f(p, "uBridgeCol", bridge_.color);
        set3f(p, "uZenithC", pal_.zenith);
        set3f(p, "uHorizonC", pal_.horizon);
        set3f(p, "uWaterC", pal_.water);
        set3f(p, "uDeepC", pal_.deep);
        set1f(p, "uWaterK", 1.2f / std::max(L * 0.5f, 0.6f));
        set1f(p, "uRippleS", std::max(0.05f, 0.2f * std::min(L, 400.f)));
        set1f(p, "uRippleA", lerpf(0.12f, 0.45f, kSea) * (1.f + 0.3f * wp));
        set1f(p, "uFlowT", time_ * (0.25f + 0.1f * wm));
        set1f(p, "uReflK", lerpf(0.65f, 1.f, kSea));
    };
    const float minPx = 2.f * tanH_ / msH_ * 0.8f;
    // the floating lanterns on the pond (drifting slowly)
    vec4 lant[6];
    {
        const float on = (1.f - smoothstepf(0.5f, 2.5f, G)) * world;
        for (int i = 0; i < 6; i++) {
            const float a = i * 1.047f + 0.4f + time_ * 0.012f * (i % 2 ? 1.f : -0.8f);
            const float d = 0.42f + 0.22f * std::sin(i * 2.3f + 0.7f) + 0.04f * std::sin(time_ * 0.1f + i);
            lant[i] = vec4(-0.45f + std::cos(a) * d, 0.035f, 0.1f + std::sin(a) * d, i < 5 ? on * (0.75f + 0.25f * std::sin(time_ * 1.3f + i * 2.f)) : 0.f);
        }
    }

    skyT_.bind();
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    // the sky, the planet, the water, space (it writes the depth of the land, the pads, the water's floor)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(progSky_);
    setLight(progSky_);
    setMat(progSky_, "uVP", vp3_);
    set3f(progSky_, "uLook", look_);
    set3f(progSky_, "uCamR", camR_);
    set3f(progSky_, "uCamU", camU_);
    set2f(progSky_, "uTan", tanH_ * aspect_, tanH_);
    set1f(progSky_, "uPix", 2.f * tanH_ / skyT_.h);
    set1f(progSky_, "uSunSize", pal_.sunSize);
    set1f(progSky_, "uStars", pal_.stars);
    set1f(progSky_, "uHills", 1.f - smoothstepf(150.f, 2500.f, alt_));
    set1f(progSky_, "uDeep", smoothstepf(16.f, 18.f, gc_));
    set1f(progSky_, "uGlow", 0.3f * wp + 0.5f * life + 0.3f * burst_ + 0.2f * swell_);
    set1f(progSky_, "uSpaceK", spaceK);
    set1f(progSky_, "uAurora", pal_.auroraSky * (1.f - spaceK * 0.5f) * (0.8f + 0.2f * wm + 0.4f * wp + 0.4f * life));
    set3f(progSky_, "uSpaceC", pal_.space);
    set3f(progSky_, "uNebulaC", pal_.nebula);
    set3f(progSky_, "uNebula2C", pal_.nebula2);
    set3f(progSky_, "uAuroraC", pal_.aurora);
    set3f(progSky_, "uAurora2C", pal_.aurora2);
    set3f(progSky_, "uGroundA", pal_.groundA);
    set3f(progSky_, "uGroundB", pal_.groundB);
    set3f(progSky_, "uOceanC", pal_.ocean);
    set3f(progSky_, "uLandC", pal_.land);
    set3f(progSky_, "uDesertC", pal_.desert);
    set3f(progSky_, "uFlowerC", pal_.flower);
    set3f(progSky_, "uSandC", pal_.sand);
    set3f(progSky_, "uPadC", pal_.pad);
    set3f(progSky_, "uLotusC", pal_.lotus);
    set1f(progSky_, "uCoastX", swgeo::COAST_X);
    set1f(progSky_, "uHeadZ", swgeo::HEAD_Z);
    set3f(progSky_, "uWhaleA", head);
    set3f(progSky_, "uWhaleB", tail);
    set1f(progSky_, "uWhaleW", 0.085f * L);
    {
        const float yc = pose_.c.y;
        const float nearSurf = smoothstepf(-0.3f * L, -0.06f * L, yc) * (1.f - smoothstepf(0.05f * L, 0.25f * L, yc));
        set1f(progSky_, "uWakeK", nearSurf * kSea * world);
        set1f(progSky_, "uShadowK", (1.f - smoothstepf(1.5f, 3.f, G)) * world);
        set1f(progSky_, "uFoamBody", smoothstepf(2.3f, 3.f, G));
    }
    set4f(progSky_, "uSplash", splashC_.x, splashC_.y, splashC_.z, kSea > 0.01f ? splashAge_ : 99.f);
    for (int i = 0; i < 6; i++) {
        const std::string n = "uLant[" + std::to_string(i) + "]";
        set4f(progSky_, n.c_str(), lant[i].x, lant[i].y, lant[i].z, lant[i].w);
    }
    set3f(progSky_, "uLantC", pal_.lantern);
    {
        // the moon, out where the creature's arc reaches it at the end of the journey
        const vec3 moon = moonPos();
        set4f(progSky_, "uMoon", moon.x, moon.y, moon.z, (float)R_MOON);
    }
    set3f(progSky_, "uGalaxyDir", normalize(vec3(-0.6f, 0.25f, 0.75f)));
    ugl::drawFullscreen();

    // ---- The geometry, multisampled at the full resolution, over the sky upsampled with its depth.
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, msW_, msH_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(progUp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyT_.tex);
    set1i(progUp_, "uSky", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, skyDepthTex_);
    set1i(progUp_, "uDepth", 1);
    glActiveTexture(GL_TEXTURE0);
    ugl::drawFullscreen();

    glDepthFunc(GL_LESS);
    // the lighthouse and the stone lanterns
    {
        glUseProgram(progProp_);
        setLight(progProp_);
        setMat(progProp_, "uVP", vp3_);
        const vec3 mats[7] = {pal_.sand * vec3(0.75f, 0.75f, 0.8f), vec3(0.95f, 0.93f, 0.9f), ugl::toLinear(hex(0xd8343a)),
                              pal_.lantern, ugl::toLinear(hex(0x3a3048)), pal_.sand * vec3(0.6f, 0.55f, 0.55f), pal_.lantern};
        for (int i = 0; i < 7; i++) {
            const std::string n = "uMat[" + std::to_string(i) + "]";
            set3f(progProp_, n.c_str(), mats[i]);
        }
        set3f(progProp_, "uLampC", pal_.lantern);
        auto draw = [&](const PropMesh& pm, const vec3& at, float scale, float lamp) {
            const vec3 rel = at - eye_;
            if (length(rel) > 6.e4f * scale) return;
            set3f(progProp_, "uRel", rel);
            set1f(progProp_, "uScale", scale);
            set1f(progProp_, "uLamp", lamp);
            glBindVertexArray(pm.vao);
            glDrawArrays(GL_TRIANGLES, 0, pm.count);
        };
        const float dusk = 0.55f + 0.45f * pal_.night;
        draw(lighthouse_, vec3(swgeo::coastX(swgeo::HEAD_Z) - 22.f, 0.f, swgeo::HEAD_Z), 1.f, dusk);
        draw(toro_, vec3(-1.6f, 0.f, 2.85f), 1.f, dusk * 0.9f);
        draw(toro_, vec3(1.5f, 0.f, 3.1f), 0.85f, dusk * 0.9f);
    }
    // the trees and bushes on the banks
    if (!trees_.empty() && alt_ < 2.e4f) {
        std::vector<float> inst = trees_;
        for (size_t i = 0; i < inst.size(); i += 16) {
            const float tint = inst[i + 4];
            const bool trunk = inst[i + 11] > 0.5f;
            const vec3 lit = trunk ? vec3(0.f) : lerp(pal_.tree, pal_.tree * vec3(1.15f, 1.05f, 0.8f), 0.5f + 0.5f * tint);
            const vec3 shade = trunk ? lerp(pal_.sand * 0.45f, pal_.treeShade, 0.4f) : pal_.treeShade * (0.9f + 0.15f * tint);
            inst[i + 4] = lit.x; inst[i + 5] = lit.y; inst[i + 6] = lit.z;
            inst[i + 8] = shade.x; inst[i + 9] = shade.y; inst[i + 10] = shade.z;
        }
        glUseProgram(progPuff_);
        setLight(progPuff_);
        setMat(progPuff_, "uVP", vp3_);
        set3f(progPuff_, "uCamR", camR_);
        set3f(progPuff_, "uCamU", camU_);
        set1f(progPuff_, "uMinPx", minPx);
        set1f(progPuff_, "uWindPhase", time_);
        set1f(progPuff_, "uWind", 0.4f + 0.4f * wm + 0.6f * wp);
        glBindVertexArray(puffVao_);
        glBindBuffer(GL_ARRAY_BUFFER, puffVbo_);
        glBufferData(GL_ARRAY_BUFFER, inst.size() * sizeof(float), inst.data(), GL_STREAM_DRAW);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(inst.size() / 16));
        glDisable(GL_BLEND);
    }
    // the creature
    {
        glUseProgram(progWhale_);
        setLight(progWhale_);
        setMat(progWhale_, "uVP", vp3_);
        set1f(progWhale_, "uM", m);
        set1f(progWhale_, "uTailR", smoothstepf(0.35f, 0.7f, m));
        set1f(progWhale_, "uSwimPh", swimPh_);
        set1f(progWhale_, "uSwimA", lerpf(0.06f, 0.045f, m) * (0.8f + 0.15f * wm + 0.25f * wp + 0.9f * stroke_) * (1.f - 0.5f * sink_));
        set1f(progWhale_, "uBend", pose_.bend);
        set1f(progWhale_, "uFinPh", finPh_);
        set1f(progWhale_, "uFinA", lerpf(0.4f, 0.22f, m) * (0.8f + 0.3f * wp + 0.4f * stroke_));
        set1f(progWhale_, "uL", L);
        set3f(progWhale_, "uRel", pose_.c - eye_);
        const float rot[9] = {pose_.fx.x, pose_.fx.y, pose_.fx.z, pose_.fy.x, pose_.fy.y, pose_.fy.z, pose_.fz.x, pose_.fz.y, pose_.fz.z};
        glUniformMatrix3fv(ugl::U(progWhale_, "uRot"), 1, GL_FALSE, rot);
        set3f(progWhale_, "uKoiC", pal_.koi);
        set3f(progWhale_, "uKoiPatch", pal_.koiPatch);
        set3f(progWhale_, "uKoiPatch2", pal_.koiPatch2);
        set3f(progWhale_, "uBackC", pal_.back);
        set3f(progWhale_, "uBellyC", pal_.belly);
        set3f(progWhale_, "uFinC", pal_.fin);
        set3f(progWhale_, "uGlowC", pal_.glow);
        set3f(progWhale_, "uEyeC", pal_.eye);
        set1f(progWhale_, "uPatch2", pal_.patch2);
        set1f(progWhale_, "uSeed", koiSeedF_);
        set1f(progWhale_, "uGlowK", glowK);
        set1f(progWhale_, "uSing", sing_ * (0.7f + 0.3f * (music.hasSong ? music.beatPulse : 0.f)) + 0.6f * burst_);
        set1f(progWhale_, "uSingPh", singPh_);
        set1f(progWhale_, "uSink", sink_ * 0.8f);
        set1f(progWhale_, "uFoam", kWater * world);
        set1f(progWhale_, "uGloss", lerpf(0.75f, 0.35f, m));
        set1f(progWhale_, "uDark", std::max(std::max(pal_.night, spaceK), std::max(sing_, burst_)));
        glBindVertexArray(whaleVao_);
        glDrawElements(GL_TRIANGLES, whaleCount_, GL_UNSIGNED_INT, nullptr);
    }

    // ---- Transparent: the clouds (far to near), the sea of clouds, the lights in the air, the aurora.
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    if (alt_ < 2.5e5f) {
        std::vector<std::pair<float, int>> order;
        order.reserve(clouds_.size());
        const float drift = time_ * (6.f + 8.f * wm + 8.f * wp);
        const float pxK = 1.f / (minPx / 0.8f);
        // (the field tiles around the eye: the creature cruises on and on through it)
        constexpr float CP = 100000.f;
        auto cloudAt = [&](const Puff& pf) {
            vec3 p = pf.p + vec3(drift, 0.f, 0.f);
            p.x = eye_.x + std::remainder(p.x - eye_.x, CP);
            p.z = eye_.z + std::remainder(p.z - eye_.z, CP);
            // on the planet's curve
            p.y -= ((p.x * p.x + p.z * p.z) - (pf.p.x * pf.p.x + pf.p.z * pf.p.z)) / (2.f * (float)R_EARTH);
            return p;
        };
        for (size_t i = 0; i < clouds_.size(); i++) {
            const vec3 p = cloudAt(clouds_[i]);
            const vec3 d = p - eye_;
            const float d2 = dot(d, d);
            if (clouds_[i].r * pxK < 0.6f * std::sqrt(d2)) continue;
            order.push_back({d2, (int)i});
        }
        std::sort(order.begin(), order.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
        cinst_.clear();
        for (auto& o : order) {
            const Puff& pf = clouds_[o.second];
            const vec3 p = cloudAt(pf);
            const float dEye = std::sqrt(o.first);
            const float a = smoothstepf(pf.r * 0.8f, pf.r * 2.5f, dEye) * (1.f - 0.3f * pal_.night);
            const float q[8] = {p.x, p.y, p.z, pf.r, pf.seed, a, 0.f, 0.f};
            cinst_.insert(cinst_.end(), q, q + 8);
        }
        if (!cinst_.empty()) {
            glUseProgram(progCloud_);
            setLight(progCloud_);
            setMat(progCloud_, "uVP", vp3_);
            set3f(progCloud_, "uCamR", camR_);
            set3f(progCloud_, "uCamU", camU_);
            set3f(progCloud_, "uCloudLit", pal_.cloudLit * (1.f + 0.15f * life));
            set3f(progCloud_, "uCloudShade", pal_.cloudShade);
            glBindVertexArray(cloudVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cloudVbo_);
            glBufferData(GL_ARRAY_BUFFER, cinst_.size() * sizeof(float), cinst_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(cinst_.size() / 8));
        }
    }
    if (eye_.y > CLOUD_Y + 200.f) {
        glUseProgram(progSea_);
        setLight(progSea_);
        setMat(progSea_, "uVP", vp3_);
        set3f(progSea_, "uLook", look_);
        set3f(progSea_, "uCamR", camR_);
        set3f(progSea_, "uCamU", camU_);
        set2f(progSea_, "uTan", tanH_ * aspect_, tanH_);
        set1f(progSea_, "uPix", 2.f * tanH_ / msH_);
        set1f(progSea_, "uCloudY", CLOUD_Y);
        set1f(progSea_, "uDrift", time_ * (8.f + 10.f * wm + 10.f * wp));
        set1f(progSea_, "uFade", 0.92f * smoothstepf(CLOUD_Y + 200.f, CLOUD_Y + 1800.f, eye_.y));
        set3f(progSea_, "uCloudLit", pal_.cloudLit * (1.f + 0.15f * life));
        set3f(progSea_, "uCloudShade", pal_.cloudShade);
        ugl::drawFullscreen();
    }
    glDepthMask(GL_FALSE);
    {
        ginst_.clear();
        auto glow = [&](const vec3& p, float size, const vec3& c, float a, int mode, float seed, float spin, const vec3& axis = vec3(0.f), float param = 0.f) {
            if (a <= 0.004f) return;
            const float q[16] = {p.x, p.y, p.z, size, c.x, c.y, c.z, a, (float)mode, seed, spin, 0.f, axis.x, axis.y, axis.z, param};
            ginst_.insert(ginst_.end(), q, q + 16);
        };
        // grass and reeds by the pond (gone once the camera is far)
        if (gc_ < 3.f) {
            const vec3 gcol = lerp(pal_.groundB, pal_.pad, 0.5f) * (pal_.night > 0.5f ? 0.85f : 1.f);
            const float wind = 0.35f + 0.45f * wm + 0.7f * wp;
            for (const Tuft& t : tufts_)
                if (length(t.p - eye_) > t.h * 6.f) glow(t.p, t.h, t.kind == 4 ? gcol * 0.85f : gcol * (0.8f + 0.4f * t.seed), world, t.kind, t.seed,
                     std::sin(time_ * 1.3f + t.p.x * 2.f + t.seed * 6.f) * (0.1f + 0.2f * wind));
        }
        // the floating lanterns
        for (int i = 0; i < 6; i++)
            if (lant[i].w > 0.01f) glow(vec3(lant[i].x, lant[i].y, lant[i].z), 0.045f, pal_.lantern * (1.4f - 0.4f * pal_.night), lant[i].w, 11, (float)i, 0.f);
        // the lighthouse's lamp and its beam sweeping the dusk
        {
            const vec3 lamp(swgeo::coastX(swgeo::HEAD_Z) - 22.f, 29.7f, swgeo::HEAD_Z);
            const float dusk = (0.45f + 0.55f * pal_.night) * world * (1.f - smoothstepf(6.f, 8.f, G));
            const float ba = time_ * 0.5f;
            const vec3 dir = normalize(vec3(std::cos(ba), -0.04f, std::sin(ba)));
            glow(lamp, 2.5f, pal_.lantern * 2.f, dusk, 0, 0.f, 0.f);
            glow(lamp, 14.f, pal_.lantern * 0.7f, dusk * 0.6f, 12, 0.f, 0.f, dir, 260.f);
            glow(lamp, 14.f, pal_.lantern * 0.7f, dusk * 0.6f, 12, 0.f, 0.f, dir * -1.f, 260.f);
        }
        // seabirds riding the creature's wake (at sea and low in the sky)
        if (G > 2.2f && G < 8.f && overT_ < 0.f) {
            const float sp = std::max(L * 0.8f, 8.f);
            for (int fl = 0; fl < 2; fl++) {
                const int n = fl == 0 ? 7 : 5;
                for (int i = 0; i < n; i++) {
                    const float k = (float)((i + 1) / 2) * (i % 2 ? 1.f : -1.f);
                    const float ph = time_ * 0.25f + fl * 2.f + i * 0.4f;
                    const vec3 p = tail - pose_.fx * (sp * (0.3f + 0.25f * fl + 0.08f * std::fabs(k))) + pose_.fz * (k * sp * 0.1f + (fl ? 0.4f : -0.3f) * sp) +
                                   vec3(0.f, std::max(0.f, pose_.c.y) + sp * (0.15f + 0.1f * fl) + std::sin(ph) * sp * 0.04f, 0.f);
                    const vec3 col = pal_.night > 0.5f ? lerp(pal_.glow, vec3(1.f), 0.4f) : vec3(1.f, 0.98f, 0.95f);
                    glow(p, 0.7f, col, world * 0.95f, 5, (float)i * 0.37f + fl, time_ * 7.f + i * 1.3f);
                }
            }
        }
        // hot-air balloons drifting in its wake as it climbs (their burners lit)
        if (G > 4.5f && G < 9.5f) {
            static const unsigned BAL[6] = {0xff3b4a, 0xffc61a, 0x2a8cff, 0xa64cff, 0xff7a1a, 0x18c8a8};
            Rng br(0xBA11ull);
            for (int i = 0; i < 7; i++) {
                const vec3 off = pose_.fx * (-L * br.range(0.6f, 2.2f)) + pose_.fz * (L * br.range(-1.f, 1.f)) + vec3(0.f, L * br.range(-0.5f, 0.3f), 0.f);
                vec3 p = pose_.c + off + vec3(std::sin(time_ * 0.2f + i) * 4.f, std::sin(time_ * 0.3f + i) * 6.f, 0.f);
                p.y = std::max(p.y, 60.f);
                glow(p, br.range(12.f, 18.f), ugl::toLinear(hex(BAL[i % 6])), world, 7, br.uniform(), 0.4f + 0.6f * pal_.night);
            }
        }
        // satellites in orbit
        if (alt_ > 3.e4f) {
            Rng sr(0x5A7Eull);
            for (int i = 0; i < 7; i++) {
                const float h = sr.range(3.5e5f, 1.4e6f);
                const float tilt = sr.range(-0.5f, 0.5f), az = sr.range(0.f, TAU);
                const vec3 u = normalize(vec3(std::sin(tilt) * std::cos(az), std::cos(tilt), std::sin(tilt) * std::sin(az)));
                const vec3 v = normalize(cross(u, vec3(std::cos(az + 1.f), 0.f, std::sin(az + 1.f))));
                const float th = sr.range(-0.4f, 0.4f) + time_ * sr.range(0.004f, 0.012f) * (sr.chance(0.5f) ? 1.f : -1.f);
                const float rr = (float)R_EARTH + h;
                const vec3 p = vec3(0.f, -(float)R_EARTH, 0.f) + (u * std::cos(th) + v * std::sin(th)) * rr;
                glow(p, 2200.f * std::sqrt(h / 4.e5f), vec3(1.f), world * smoothstepf(3.e4f, 1.5e5f, alt_), 6, sr.uniform(), time_ + i);
            }
        }
        for (const Spark& s : sparks_) {
            if (s.age < 0.f) continue;
            const float t = s.age / s.life;
            const float a = smoothstepf(0.f, 0.08f, t) * (1.f - smoothstepf(0.6f, 1.f, t));
            const vec3 col = s.kind == 13 ? s.col * lerp(pal_.lit, vec3(1.f), 0.4f) : s.col;
            glow(s.p, s.size, col, a * world * (s.kind == 13 ? 1.f : 0.85f), s.kind, s.seed, 0.f, s.kind == 8 ? s.v : vec3(0.f));
        }
        for (const Ring& r : rings_) {
            const float t = r.age / r.life;
            const float rad = lerpf(r.r0, r.r1, 1.f - (1.f - t) * (1.f - t));
            const float a = smoothstepf(0.f, 0.15f, t) * (1.f - smoothstepf(0.25f, 1.f, t)) * world * 0.45f;
            glow(r.c + pose_.fx * (L * 0.2f * t), rad, lerp(pal_.glow, vec3(1.f), 0.2f), a, 10, r.hue, time_ * 0.6f, r.n, 0.025f + 0.02f * t);
        }
        if (!ginst_.empty()) {
            glUseProgram(progGlow_);
            setMat(progGlow_, "uVP", vp3_);
            set3f(progGlow_, "uEye", eye_);
            set3f(progGlow_, "uCamR", camR_);
            set3f(progGlow_, "uCamU", camU_);
            set1f(progGlow_, "uMinPx", minPx);
            glBindVertexArray(glowVao_);
            glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
            glBufferData(GL_ARRAY_BUFFER, ginst_.size() * sizeof(float), ginst_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(ginst_.size() / 16));
        }
        // the aurora trailing from its fins in orbit (and in the polar night's sky, faintly, from the start of the climb)
        const float auroraK = std::max(kSpace, pal_.auroraSky * smoothstepf(5.f, 7.f, G) * 0.6f) * world * (1.f - sink_);
        if (auroraK > 0.01f) {
            ribbon_.clear();
            // the fin tips in the body's frame (the whale's: the pectoral flippers, the flukes' tips)
            const float tips[4][3] = {{0.16f, -0.17f, 0.32f}, {0.16f, -0.17f, -0.32f}, {-0.41f, 0.f, 0.17f}, {-0.41f, 0.f, -0.17f}};
            const int N = 48;
            for (int r = 0; r < 4; r++) {
                const float seed = r * 1.7f;
                auto at = [&](float k, float hgt) {
                    const float x = tips[r][0] - k * 1.1f;
                    const float y = tips[r][1] + 0.05f * std::sin(k * 7.f - time_ * 1.1f + seed) + hgt;
                    const float z = tips[r][2] * (1.f + 0.6f * k) + 0.06f * std::sin(k * 5.f - time_ * 0.8f + seed * 2.f);
                    const vec3 b = bendBody(vec3(x, y, z), pose_.bend);
                    return pose_.c + (pose_.fx * b.x + pose_.fy * b.y + pose_.fz * b.z) * L - eye_;
                };
                for (int i = 0; i < N; i++) {
                    const float k0 = (float)i / N, k1 = (float)(i + 1) / N;
                    const float h0 = 0.1f * (0.3f + 0.7f * k0), h1 = 0.1f * (0.3f + 0.7f * k1);
                    const vec3 a0 = at(k0, 0.f), a1 = at(k1, 0.f), b0 = at(k0, h0), b1 = at(k1, h1);
                    const float al = auroraK * (r < 2 ? 1.f : 0.7f) * (0.8f + 0.4f * sing_);
                    auto put = [&](const vec3& p, float u, float v) {
                        const float q[8] = {p.x, p.y, p.z, u, v, al, seed, 0.f};
                        ribbon_.insert(ribbon_.end(), q, q + 8);
                    };
                    put(a0, k0, 0.f); put(b0, k0, 1.f); put(b1, k1, 1.f);
                    put(a0, k0, 0.f); put(b1, k1, 1.f); put(a1, k1, 0.f);
                }
            }
            glBlendFunc(GL_ONE, GL_ONE);
            glUseProgram(progRibbon_);
            setMat(progRibbon_, "uVP", vp3_);
            set3f(progRibbon_, "uAuroraC", pal_.aurora);
            set3f(progRibbon_, "uAurora2C", pal_.aurora2);
            set1f(progRibbon_, "uTime", time_);
            glBindVertexArray(ribbonVao_);
            glBindBuffer(GL_ARRAY_BUFFER, ribbonVbo_);
            glBufferData(GL_ARRAY_BUFFER, ribbon_.size() * sizeof(float), ribbon_.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(ribbon_.size() / 8));
            glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    // resolve the samples
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, worldT_.fbo);
    glBlitFramebuffer(0, 0, msW_, msH_, 0, 0, worldT_.w, worldT_.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---- The bloom (from the sky's resolution / 4).
    bloomA_.bind();
    glUseProgram(progBright_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progBright_, "uSrc", 0);
    set2f(progBright_, "uTexel", 0.4f / bloomA_.w, 0.4f / bloomA_.h);
    set1f(progBright_, "uThreshold", pal_.night > 0.5f ? 0.45f : 0.85f);
    ugl::drawFullscreen();
    for (int pass = 0; pass < 2; pass++) {
        bloomB_.bind();
        glUseProgram(progBlur_);
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        set1i(progBlur_, "uSrc", 0);
        set2f(progBlur_, "uDir", 1.f / bloomA_.w, 0.f);
        ugl::drawFullscreen();
        bloomA_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        set2f(progBlur_, "uDir", 0.f, 1.f / bloomA_.h);
        ugl::drawFullscreen();
    }

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progComp_, "uWorld", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set1i(progComp_, "uBloomT", 1);
    glActiveTexture(GL_TEXTURE0);
    set3f(progComp_, "uLook", look_);
    set3f(progComp_, "uCamR", camR_);
    set3f(progComp_, "uCamU", camU_);
    set2f(progComp_, "uTan", tanH_ * aspect_, tanH_);
    set2f(progComp_, "uRes", (float)width_, (float)height_);
    set1f(progComp_, "uBloomK", pal_.bloom * (1.f + 0.3f * wp + 0.35f * life + 0.4f * burst_) * world);
    set1f(progComp_, "uTwinkle", 0.25f + 0.2f * wm + 0.25f * wp);
    set1f(progComp_, "uTime", time_);
    set1f(progComp_, "uSat", 1.1f);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    // ---- The veil behind the well and the HUD's columns (none in a break).
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(pal_.glow * 0.5f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", pal_.night > 0.5f ? 0.8f : 0.72f);
        set1f(progVeil_, "uColA", (pal_.night > 0.5f ? 0.4f : 0.62f + 0.08f * wp) * world);
        set1f(progVeil_, "uRailA", 0.5f);
        set1f(progVeil_, "uPx", pxPerUnit_);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.f, layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }

    // ---- Pieces, previews, the motes over them, the HUD.
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uTime", time_);
        set3f(progCell_, "uLightC", lerp(pal_.glow, vec3(1.f), 0.4f));
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    if (!sprites_.empty()) {
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        set1f(progSprite_, "uPxPerUnit", pxPerUnit_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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
    if (gpuQ) {
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
        static double acc = 0.0;
        static int n = 0;
        acc += ns * 1e-6;
        if (++n == 240) {
            std::fprintf(stderr, "[skywhale] gpu %.2f ms/frame (%dx%d, G %.2f)\n", acc / n, width_, height_, g_);
            acc = 0.0;
            n = 0;
        }
    }
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_skywhale() {
    static const UniverseEntry e{
        "SKY WHALE", "a koi in a lily pond growing with every cleared line into a whale that swims up through the clouds and around the Earth",
        [] { return std::unique_ptr<Universe>(new SkyWhaleUniverse()); },
        // Fits most songs; the calm, airy ones a little better.
        [](const SongLook& l) { return 0.85f + 0.2f * (1.f - l.density) + 0.15f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
