// ARCHITECT: a modern villa built over one song, on an empty green field, by two tower cranes.
//
// One world, an architectural rendering in real 3D seen through its own camera (a slow orbit around the site, the
// well drawn over it through the layout's camera): soft sun with a shadow map, sky and ground ambient, screen-space
// ambient occlusion, real materials (white render, board-formed concrete, wood cladding, travertine, glass with the
// rooms seen behind it, pool water with caustics), aerial perspective, a filmic tone map.
//
// The construction follows the song (paced lines, 50 at scene completion; site.cpp): the empty field with the
// surveyor's stakes, the site levelled and the slabs poured, the cranes climbing up, the rooms lowered one by one
// (the ground floor, the cantilevered slab, upstairs, the roofs), the glazing, the cladding, the canopy, the roof
// terrace and its pergola, the pool dug, tiled and filled, the garden (decks, lawn, trees, loungers, lamps), the
// cranes leaving, and the finished villa at dusk: the rooms light up one after the other, the pool glows turquoise.
// The light follows the same arc, morning to dusk. Every cleared row is a delivery: its cells slide back into the
// board and a crane picks the next element up behind the well (hidden by it), hoists it, slews and trolleys out
// with the load swinging on its cables, and lowers it softly into place. Between deliveries the cranes keep
// moving, slowly.
//
// The song: calm is a still site; mid and peak bring the cement mixer's drum turning, soft welding sparks on the
// beat grid (every two beats), and at a peak the cranes' beacons blink; the drop switches the site's floodlights on
// (a warm swell, settling over the bars). A Tetris lights every crane lamp and sends a shower of sparks. Slow
// motion is a hushed, misty blue hour. Game over: the pieces grey out from the top; the next game starts from the
// field again (the villa sinks back).
//
// The pieces are pigmented architectural concrete panels, one per cell: a chamfered block, a cloudy mottle, fine
// pores and the four tie holes of its formwork (the architect's concrete), one strong pigment per piece type, a
// crisp darker rim, a soft contact shadow on the deep backing of the well.
#include "universes/architect/architect.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/architect/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Josefin Sans (Regular and Light), SIL Open Font License, see OFL-JosefinSans.txt.
const unsigned char FONT_REG[] = {
#include "universes/architect/JosefinSans-Regular.ttf.inc"
};
const unsigned char FONT_LIGHT[] = {
#include "universes/architect/JosefinSans-Light.ttf.inc"
};

namespace shaders = architect_shaders;
namespace as = arch_site;
using as::Q;
using as::qAxis;
using as::qMul;
using as::qRot;
using as::qYaw;
using ugl::set1f;
using ugl::set1i;
using ugl::set3f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
vec3 mixDisplay(const vec3& a, const vec3& b, float t) { return ugl::toLinear(lerp(ugl::toSrgb(a), ugl::toSrgb(b), t)); }

float smoother(float t) {
    t = saturate(t);
    return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
}
float easeOutBack(float t, float c1 = 1.4f) {
    t = saturate(t);
    const float c3 = c1 + 1.f, u = t - 1.f;
    return 1.f + c3 * u * u * u + c1 * u * u;
}
float hashf(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (x & 0xFFFFFF) / 16777216.f;
}
float wrapPi(float a) {
    while (a > PI) a -= TAU;
    while (a < -PI) a += TAU;
    return a;
}

// Test hooks: ZEN_ARCHITECT_LINES=N adds N real lines to the construction (stills of a stage), ZEN_GROW_RATE=R adds
// R lines a second (each one a delivery, like a cleared row), ZEN_ARCH_CAM="dist,pitch,fov,yaw" moves the camera.
float hookLines() {
    static const float n = std::getenv("ZEN_ARCHITECT_LINES") ? (float)std::atof(std::getenv("ZEN_ARCHITECT_LINES")) : 0.f;
    return n;
}
float hookRate() {
    static const float r = std::getenv("ZEN_GROW_RATE") ? (float)std::atof(std::getenv("ZEN_GROW_RATE")) : 0.f;
    return r;
}

// A mat4 inverse (general).
mat4 inverse(const mat4& mm) {
    const float* a = mm.m;
    float inv[16];
    inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
    float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    mat4 r{};
    if (std::fabs(det) < 1e-12f) return mat4::identity();
    det = 1.f / det;
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] * det;
    return r;
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

// ---------------------------------------------------------------- the variants
struct VariantDef {
    const char* name;
    // the land: the meadow (two greens, its dry tufts), the levelled soil, the new lawn, the far hills
    unsigned grassA, grassB, dry, soil, lawn, hill, hill2;
    float hillK, cloudK;
    bool sea;
    unsigned seaCol;
    // the villa
    unsigned render, render2, stone, wood, deck, frame, concrete, tile, water, waterDeep, waterGlow;
    unsigned leaf, leaf2, bark, fabric, crane;
    int wallMat, upperMat, flora;
    // the well: its backing, the seven pieces (I O T S Z J L), the accent
    unsigned backing;
    unsigned piece[7];
    unsigned accent;
};
const VariantDef VARIANTS[] = {
    // a white villa on the Riviera: travertine, olive trees and cypresses, the sea behind
    {"RIVIERA", 0x5f7d36, 0x7c8e42, 0xae9e6a, 0xbaa27e, 0x6e9a3c, 0x8a9a92, 0x74855e, 0.8f, 0.55f, true, 0x2c6f96,
     0xf1eee7, 0xf7f5f0, 0xdccaa6, 0xa3744c, 0x9c7858, 0x34312e, 0xb9b5ad, 0xdcf0f2, 0x3cc4d6, 0x1a86ac, 0x2ee0ec,
     0x8b9c6a, 0x5c7040, 0x6a5a4a, 0xf0ebe2, 0xf0bf2c,
     as::RENDER, as::STONE, 0,
     0x1b2029, {0x5aa8d8, 0xeab84a, 0x9c78c6, 0x84aa48, 0xd65e3c, 0x3a62b4, 0xea8a38}, 0xd65e3c},
    // a modernist forest house: board-formed concrete and larch, firs and birches, misty hills
    {"PINEWOOD", 0x4e7a3a, 0x6a8c3e, 0x8c8a54, 0x7c6852, 0x548a3a, 0x557a6a, 0x3c5c40, 1.2f, 0.7f, false, 0,
     0xe8e6e1, 0xebe9e4, 0x8f8c86, 0xb67c46, 0x8c6a4c, 0x262626, 0xb4b2ac, 0xc8e4e6, 0x2ea8ba, 0x14708c, 0x30d8e2,
     0x6a9a4a, 0x2e5a32, 0x5a4636, 0xd9d2c4, 0xd2463a,
     as::BOARDFORM, as::WOOD, 1,
     0x161d1a, {0x5cb4b2, 0xdeb244, 0x9060a2, 0x70a04a, 0xc64c3a, 0x4a6cae, 0xd67a40}, 0xd67a40},
    // a tropical villa: white render and teak, palms, the lagoon behind
    {"TROPICS", 0x4c8638, 0x62963e, 0x8ea05a, 0xb4947a, 0x4a8e38, 0x3f7a5a, 0x2e6a46, 0.9f, 0.6f, true, 0x1c8aa6,
     0xf4f2ed, 0xfaf8f4, 0xcbbb9c, 0x8c5a36, 0x8a6044, 0x2e2a26, 0xbab6ae, 0xc2ecf2, 0x22d2da, 0x0c92b8, 0x20f2f2,
     0x4ea040, 0x2e7a34, 0x8a7a66, 0xf4efe6, 0xeeeeea,
     as::RENDER, as::WOOD, 2,
     0x10222a, {0x34c2cc, 0xf4bc38, 0xc464b2, 0x4cb058, 0xee6450, 0x3a7ad4, 0xf48c3e}, 0xee6450},
    // a desert villa: warm render and rammed earth, cacti and agaves, red mesas
    {"MESA", 0xbc9e6e, 0xcaa97a, 0xa88a5c, 0xc49c6e, 0xd6c6a6, 0xb8735a, 0x9c5e46, 1.1f, 0.35f, false, 0,
     0xece2d0, 0xf3ece2, 0xc2885c, 0x9c6c46, 0x8c6c52, 0x2a2622, 0xc4b8a6, 0xd2ecec, 0x30c8cc, 0x12909e, 0x30eae2,
     0x6a9a5a, 0x7a8a50, 0x7a6650, 0xe8dccb, 0xe8822e,
     as::RENDER, as::STONE, 3,
     0x231b18, {0x4cb2bc, 0xe2b24e, 0xa278ac, 0x78aa5c, 0xca563a, 0x4a5cac, 0xe4884a}, 0xe4884a},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// ---------------------------------------------------------------- the hour: morning to dusk over the song
struct Hour {
    float t;
    float sx, sy, sz;       // toward the sun
    unsigned sun;           // the sun's color
    float sunK;             // its strength
    unsigned top, hor, low; // the sky
};
const Hour HOURS[] = {
    {0.00f, -0.62f, 0.34f, 0.71f, 0xffe2c0, 2.2f, 0x4a86d0, 0xbcd4e6, 0xb0c4c8},
    {0.30f, -0.30f, 0.80f, 0.52f, 0xfff2e2, 2.5f, 0x2f6fcf, 0xaacbe8, 0xa8bcc4},
    {0.62f, 0.42f, 0.58f, 0.70f, 0xffe8cc, 2.4f, 0x3a74c6, 0xc2d6e2, 0xb4c0c0},
    {0.82f, 0.80f, 0.24f, 0.55f, 0xffc080, 2.3f, 0x4c6eae, 0xf0cc98, 0xc8b8a0},
    {0.93f, 0.62f, 0.05f, -0.78f, 0xff9a5a, 1.7f, 0x3e5088, 0xf0a070, 0x9a8090},
    {1.00f, 0.55f, -0.07f, -0.83f, 0xd07a5a, 0.35f, 0x18224a, 0x6e5a80, 0x3a3a58},
};
constexpr int HOUR_COUNT = (int)(sizeof(HOURS) / sizeof(HOURS[0]));

struct Sky {
    vec3 sunDir, sunCol, top, hor, low;
};
Sky skyAt(float t) {
    t = saturate(t);
    int i = 0;
    while (i + 1 < HOUR_COUNT - 1 && HOURS[i + 1].t < t) i++;
    const Hour& a = HOURS[i];
    const Hour& b = HOURS[i + 1];
    const float u = smoothstepf(0.f, 1.f, saturate((t - a.t) / std::max(1e-4f, b.t - a.t)));
    Sky s;
    s.sunDir = normalize(lerp(vec3(a.sx, a.sy, a.sz), vec3(b.sx, b.sy, b.sz), u));
    s.sunCol = lerp(lin(a.sun) * a.sunK, lin(b.sun) * b.sunK, u);
    s.top = mixDisplay(lin(a.top), lin(b.top), u);
    s.hor = mixDisplay(lin(a.hor), lin(b.hor), u);
    s.low = mixDisplay(lin(a.low), lin(b.low), u);
    return s;
}

constexpr float SLING = 2.4f;     // the hook block above a load's top
constexpr float BACK_Z = -0.62f;  // the well's backing (board space)

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool ArchitectUniverse::init(int width, int height) {
    using shaders::with;
    const std::string common = std::string(shaders::COMMON) + shaders::SHADOW_GLSL + shaders::LIGHTS_GLSL;
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, with(shaders::WORLD_BG_FS, common.c_str(), shaders::PIT_GLSL).c_str(), "architect world");
    progInst_ = ugl::program(shaders::INST_VS, with(shaders::INST_FS, common.c_str(), shaders::INST_COMMON).c_str(), "architect solids");
    progShadow_ = ugl::program(shaders::INST_VS, with(shaders::SHADOW_FS, shaders::INST_COMMON).c_str(), "architect shadow");
    progPre_ = ugl::program(shaders::INST_VS, with(shaders::PRE_FS, shaders::INST_COMMON).c_str(), "architect prepass");
    progPreGround_ = ugl::program(ugl::FULLSCREEN_VS, with(shaders::PRE_GROUND_FS, shaders::PIT_GLSL).c_str(), "architect prepass ground");
    progSsao_ = ugl::program(ugl::FULLSCREEN_VS, shaders::SSAO_FS, "architect ssao");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "architect blur");
    progGlow_ = ugl::program(shaders::GLOW_VS, shaders::GLOW_FS, "architect glow");
    progBlock_ = ugl::program(shaders::INST_VS, with(shaders::BLOCK_FS, shaders::INST_COMMON).c_str(), "architect blocks");
    progFlat_ = ugl::program(shaders::FLAT_VS, shaders::FLAT_FS, "architect flat");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "architect final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "architect shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "architect text");

    // ---- the meshes: pos, normal (object space, unit sized)
    std::vector<float> mesh[as::MESH_COUNT];
    auto V = [&](std::vector<float>& m, const vec3& p, const vec3& n) {
        const float v[6] = {p.x, p.y, p.z, n.x, n.y, n.z};
        m.insert(m.end(), v, v + 6);
    };
    {   // box
        const float N[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (auto& n : N) {
            const vec3 nv(n[0], n[1], n[2]);
            const vec3 u = std::fabs(nv.y) > 0.5f ? vec3(1, 0, 0) : normalize(cross(vec3(0, 1, 0), nv));
            const vec3 v = cross(nv, u);
            const float c[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
            for (auto& k : c) V(mesh[as::BOX], nv * 0.5f + u * (0.5f * k[0]) + v * (0.5f * k[1]), nv);
        }
    }
    {   // sphere (radius 0.5)
        const int SU = 32, SV = 20;
        auto P = [&](int i, int j) {
            const float th = PI * j / SV, ph = TAU * i / SU;
            return vec3(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
        };
        for (int j = 0; j < SV; j++)
            for (int i = 0; i < SU; i++) {
                const vec3 a = P(i, j), b = P(i + 1, j), c = P(i + 1, j + 1), d = P(i, j + 1);
                for (const vec3& p : {a, c, b, a, d, c}) V(mesh[as::SPHERE], p * 0.5f, p);
            }
    }
    {   // cylinder and cone (radius 0.5, y -0.5..0.5)
        const int S = 20;
        for (int cone = 0; cone < 2; cone++) {
            std::vector<float>& m = mesh[cone ? as::CONE : as::CYL];
            const float rt = cone ? 0.f : 0.5f;
            const float slope = cone ? 0.5f : 0.f;
            for (int i = 0; i < S; i++) {
                const float a0 = TAU * i / S, a1 = TAU * (i + 1) / S;
                const vec3 d0(std::cos(a0), 0, std::sin(a0)), d1(std::cos(a1), 0, std::sin(a1));
                const vec3 n0 = normalize(d0 + vec3(0, slope, 0)), n1 = normalize(d1 + vec3(0, slope, 0));
                const vec3 b0 = d0 * 0.5f + vec3(0, -0.5f, 0), b1 = d1 * 0.5f + vec3(0, -0.5f, 0);
                const vec3 t0 = d0 * rt + vec3(0, 0.5f, 0), t1 = d1 * rt + vec3(0, 0.5f, 0);
                V(m, b0, n0); V(m, t1, n1); V(m, b1, n1);
                V(m, b0, n0); V(m, t0, n0); V(m, t1, n1);
                V(m, vec3(0, -0.5f, 0), vec3(0, -1, 0)); V(m, b1, vec3(0, -1, 0)); V(m, b0, vec3(0, -1, 0));
                if (!cone) { V(m, vec3(0, 0.5f, 0), vec3(0, 1, 0)); V(m, t0, vec3(0, 1, 0)); V(m, t1, vec3(0, 1, 0)); }
            }
        }
    }
    auto instAttribs = [&](GLuint vbo) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < 5; i++) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(Inst), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(2 + i, 1);
        }
    };
    for (int m = 0; m < as::MESH_COUNT; m++) {
        glGenVertexArrays(1, &meshVao_[m]);
        glGenBuffers(1, &meshVbo_[m]);
        glGenBuffers(1, &instVbo_[m]);
        glBindVertexArray(meshVao_[m]);
        glBindBuffer(GL_ARRAY_BUFFER, meshVbo_[m]);
        glBufferData(GL_ARRAY_BUFFER, mesh[m].size() * sizeof(float), mesh[m].data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
        instAttribs(instVbo_[m]);
        meshCount_[m] = (int)(mesh[m].size() / 6);
    }
    glGenVertexArrays(1, &blockVao_);
    glGenBuffers(1, &blockVbo_);
    glBindVertexArray(blockVao_);
    glBindBuffer(GL_ARRAY_BUFFER, meshVbo_[as::BOX]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    instAttribs(blockVbo_);

    // the glows: a quad per sprite
    glGenVertexArrays(1, &glowVao_);
    glGenBuffers(1, &glowQuad_);
    glGenBuffers(1, &glowVbo_);
    glBindVertexArray(glowVao_);
    glBindBuffer(GL_ARRAY_BUFFER, glowQuad_);
    const float quadV[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadV), quadV, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, nullptr);
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 4));
    glVertexAttribDivisor(2, 1);

    glGenVertexArrays(1, &flatVao_);
    glGenBuffers(1, &flatVbo_);
    glBindVertexArray(flatVao_);
    glBindBuffer(GL_ARRAY_BUFFER, flatVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 11, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 11, (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 11, (void*)(sizeof(float) * 7));
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

    // the sun's shadow map
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 4096, 4096, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &shadowFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLint maxS = 4;
    glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    samples_ = std::max(1, std::min(4, (int)maxS));

    buildFont(fontReg_, FONT_REG);
    buildFont(fontLight_, FONT_LIGHT);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void ArchitectUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] architect font init failed\n");
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

void ArchitectUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera ref = layout::camera(width_, height_);
    bvp_ = ref.vp;
    beye_ = ref.eye;
    updateCamera();

    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
    aoW_ = std::max(1, width_ / 2);
    aoH_ = std::max(1, height_ / 2);
    for (auto& t : aoT_) t.create(aoW_, aoH_, GL_R8, GL_RED, GL_UNSIGNED_BYTE, false);
    if (msFbo_) {
        glDeleteFramebuffers(1, &msFbo_);
        glDeleteRenderbuffers(1, &msColor_);
        glDeleteRenderbuffers(1, &msDepth_);
    }
    glGenFramebuffers(1, &msFbo_);
    glGenRenderbuffers(1, &msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGBA16F, width_, height_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);

    // the prepass (half resolution): view-space positions and normals for the occlusion
    if (preFbo_) {
        glDeleteFramebuffers(1, &preFbo_);
        glDeleteTextures(1, &prePos_);
        glDeleteTextures(1, &preNrm_);
        glDeleteRenderbuffers(1, &preDepth_);
    }
    glGenFramebuffers(1, &preFbo_);
    glGenTextures(1, &prePos_);
    glGenTextures(1, &preNrm_);
    glGenRenderbuffers(1, &preDepth_);
    auto tex = [&](GLuint t, GLenum internal, GLenum type) {
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, internal, aoW_, aoH_, 0, GL_RGBA, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    tex(prePos_, GL_RGBA32F, GL_FLOAT);
    tex(preNrm_, GL_RGBA16F, GL_HALF_FLOAT);
    glBindRenderbuffer(GL_RENDERBUFFER, preDepth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, aoW_, aoH_);
    glBindFramebuffer(GL_FRAMEBUFFER, preFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, prePos_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, preNrm_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, preDepth_);
    const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

ArchitectUniverse::~ArchitectUniverse() {
    sceneT_.destroy();
    for (auto& t : aoT_) t.destroy();
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (preFbo_) glDeleteFramebuffers(1, &preFbo_);
    if (preDepth_) glDeleteRenderbuffers(1, &preDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    for (GLuint t : {prePos_, preNrm_, shadowTex_})
        if (t) glDeleteTextures(1, &t);
    for (GLuint p : {progBg_, progInst_, progShadow_, progPre_, progPreGround_, progSsao_, progBlur_, progGlow_, progBlock_,
                     progFlat_, progFinal_, progShape_, progText_})
        glDeleteProgram(p);
    for (int m = 0; m < as::MESH_COUNT; m++) {
        glDeleteVertexArrays(1, &meshVao_[m]);
        glDeleteBuffers(1, &meshVbo_[m]);
        glDeleteBuffers(1, &instVbo_[m]);
    }
    for (GLuint v : {blockVao_, glowVao_, flatVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {blockVbo_, glowQuad_, glowVbo_, flatVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontReg_, &fontLight_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// The world's camera: a slow orbit around the site, raised a little, looking down on it.
void ArchitectUniverse::updateCamera() {
    float dist = 50.f, pitch = 7.5f, fov = 42.f, yaw0 = 0.f;
    if (const char* c = std::getenv("ZEN_ARCH_CAM")) std::sscanf(c, "%f,%f,%f,%f", &dist, &pitch, &fov, &yaw0);
    const vec3 target(-2.f, 4.5f, -1.f);
    const float yaw = camYaw_ + yaw0 * PI / 180.f, pr = pitch * PI / 180.f;
    eye_ = target + vec3(std::sin(yaw) * std::cos(pr), std::sin(pr), std::cos(yaw) * std::cos(pr)) * dist;
    view_ = mat4::lookAt(eye_, target, vec3(0, 1, 0));
    proj_ = mat4::perspective(fov * PI / 180.f, (float)width_ / height_, 0.5f, 3000.f);
    vp_ = proj_ * view_;
    invVp_ = inverse(vp_);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 ArchitectUniverse::backdrop() const {
    const Sky s = skyAt(saturate(p_ / as::L_DONE));
    return mixDisplay(s.hor, lin(VARIANTS[variant_].grassA), 0.4f);
}

vec3 ArchitectUniverse::pieceCol(int t) const { return lin(VARIANTS[variant_].piece[std::clamp(t, 0, 6)]); }

void ArchitectUniverse::setSong(const SongLook& t, float seconds) {
    (void)seconds;
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (!t.hasSong) v = 0;
        else if (t.minor && t.brightness < 0.45f) v = 1;                          // pinewood
        else if (t.brightness > 0.58f && t.airWeight > 0.45f) v = 0;              // riviera
        else if (t.bpm > 112.f && t.bassWeight > 0.55f) v = 2;                    // tropics
        else if (t.bassWeight > 0.6f && t.airWeight < 0.4f) v = 3;                // mesa
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
}

void ArchitectUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)impact;
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
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

// The drop: the site's floodlights come on, a warm swell.
void ArchitectUniverse::drop(float strength) {
    dropMode_ = true;
    dropRelease_ = 1.f;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void ArchitectUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.2f * strength); }

vec2 ArchitectUniverse::project(const vec3& p) const { return layout::project(bvp_, p, width_, height_); }

TransitionHints ArchitectUniverse::hints() const {
    TransitionHints h;
    const VariantDef& V = VARIANTS[variant_];
    h.background = backdrop();
    h.light = ugl::luma(h.background) > 0.2f;
    h.ink = h.light ? lin(0x23262b) : lin(0xf2ece0);
    h.accent = lin(V.accent);
    h.styles = TS_REBUILD | TS_CURTAIN | TS_LUMA;
    return h;
}

// =====================================================================================================
// The site
// =====================================================================================================

void ArchitectUniverse::buildSite() {
    const VariantDef& V = VARIANTS[variant_];
    as::Palette& P = pal_;
    P.render = lin(V.render);
    P.render2 = lin(V.render2);
    P.stone = lin(V.stone);
    P.wood = lin(V.wood);
    P.deck = lin(V.deck);
    P.frame = lin(V.frame);
    P.concrete = lin(V.concrete);
    P.glass = vec3(1.f, 1.f, 1.f);
    P.tile = lin(V.tile);
    P.water = lin(V.water);
    P.soil = lin(V.soil);
    P.leaf = lin(V.leaf);
    P.leaf2 = lin(V.leaf2);
    P.bark = lin(V.bark);
    P.fabric = lin(V.fabric);
    P.crane = lin(V.crane);
    P.wallMat = V.wallMat;
    P.upperMat = V.upperMat;
    P.flora = V.flora;
    prog_ = as::program(P, seed_);
    scenery_ = as::scenery(P, seed_);
    els_.assign(prog_.size(), ElState{});
    for (auto& c : cranes_) c.queue.clear(), c.job = -1, c.phase = 0;
    builtVariant_ = variant_;
    pInit_ = false;
}

float ArchitectUniverse::elementK(const as::Element& e) const {
    float k = saturate((p_ - e.due) / std::max(0.05f, e.dur));
    if (p_ > e.gone) k *= 1.f - saturate((p_ - e.gone) / std::max(0.05f, e.goneDur));
    return k;
}
float ArchitectUniverse::tilesK() const { return smoother((p_ - (as::L_POOL + 3.5f)) / 1.2f); }
float ArchitectUniverse::lightK(int room) const {
    return smoother((p_ - (as::L_LIGHTS + 0.75f * room)) / 1.2f) * houseK_;
}
float ArchitectUniverse::poolLightK() const { return smoother((p_ - 47.5f) / 1.5f); }
float ArchitectUniverse::lampK() const { return smoother((p_ - 48.5f) / 1.2f); }

// A still: every crane element already due stands in place at once (the first frame, a test hook's jump).
void ArchitectUniverse::placeDueNow() {
    for (size_t i = 0; i < prog_.size(); i++) {
        if (prog_[i].kind != as::CRANE) continue;
        ElState& s = els_[i];
        if (p_ >= prog_[i].due && s.state < 2) {
            s.state = 3;
            s.t = 9.f;
            s.vis = 1.f;
        }
    }
    for (auto& c : cranes_) {
        c.queue.clear();
        if (c.job >= 0 && els_[c.job].state == 2) { els_[c.job].state = 3; els_[c.job].t = 9.f; }
        c.job = -1;
        c.phase = 0;
    }
}

void ArchitectUniverse::push(const Part& p, float glow, float alt) {
    inst_[p.mesh].push_back({{p.c.x, p.c.y, p.c.z, hashf((uint32_t)(p.c.x * 131.f + p.c.z * 977.f + p.c.y * 37.f)) + 0.001f},
                             {p.s.x, p.s.y, p.s.z, (float)p.mat},
                             {p.col.x, p.col.y, p.col.z, p.param},
                             {p.q.x, p.q.y, p.q.z, p.q.w},
                             {glow, p.depth, alt, 0.f}});
}
void ArchitectUniverse::pushRaw(int mesh, const vec3& c, const vec3& s, const Q& q, int mat, const vec3& col, float param,
                                float glow, float depth, float alt) {
    inst_[mesh].push_back({{c.x, c.y, c.z, hashf((uint32_t)(c.x * 131.f + c.z * 977.f)) + 0.001f},
                           {s.x, s.y, s.z, (float)mat},
                           {col.x, col.y, col.z, param},
                           {q.x, q.y, q.z, q.w},
                           {glow, depth, alt, 0.f}});
}

void ArchitectUniverse::drawElements() {
    for (const Part& p : scenery_) push(p);
    const float tk = tilesK();
    for (size_t i = 0; i < prog_.size(); i++) {
        const as::Element& e = prog_[i];
        const ElState& st = els_[i];
        auto lit = [&](const Part& p) {
            if (p.light == as::GARDEN_LAMP) return lampK() * 3.f + flood_ * 0.f;
            if (p.light >= 0) return lightK(p.light);
            return 0.f;
        };
        if (e.kind == as::CRANE) {
            if (st.state == 2) {
                // on the hook: the load hangs under it, swaying, turning a little
                int ci = e.crane;
                const Crane& c = cranes_[ci];
                const vec3 hk = hookPos(ci);
                const vec3 at = hk - vec3(0.f, SLING, 0.f);
                const Q yq = qYaw(c.wob);
                for (const Part& p0 : e.parts) {
                    Part p = p0;
                    p.c = at + qRot(yq, p0.c - e.anchor);
                    p.q = qMul(yq, p0.q);
                    push(p, lit(p0), p0.tiles ? tk : 0.f);
                }
                continue;
            }
            if (st.state != 3 && st.vis <= 0.001f) continue;
            if (st.state != 3 && st.vis > 0.001f) {
                // taken down (a new game): it sinks back into the ground
                for (const Part& p0 : e.parts) {
                    Part p = p0;
                    p.c.y = p0.c.y * st.vis - (1.f - st.vis) * 2.f;
                    p.s.y = p0.s.y * std::max(0.02f, st.vis);
                    push(p, lit(p0));
                }
                continue;
            }
            // set down without its crane (a backlog caught up): it settles softly from a few meters above
            const float lift = st.direct ? 4.f * (1.f - smoother(st.t / 1.6f)) : 0.f;
            for (const Part& p0 : e.parts) {
                Part p = p0;
                p.c.y += lift;
                push(p, lit(p0), p0.tiles ? tk : 0.f);
            }
            continue;
        }
        const float k = elementK(e);
        if (k <= 0.001f) continue;
        for (const Part& p0 : e.parts) {
            Part p = p0;
            switch (e.kind) {
            case as::GROW: {
                const float kk = smoother(k);
                const float bottom = p0.c.y - p0.s.y * 0.5f;
                p.s.y = p0.s.y * kk;
                p.c.y = bottom + p.s.y * 0.5f;
                break;
            }
            case as::TREE: {
                const float kk = e.gone < 1e8f ? smoother(k) : easeOutBack(k, 0.8f);
                const vec3 base((e.lo.x + e.hi.x) * 0.5f, 0.f, (e.lo.z + e.hi.z) * 0.5f);
                p.c = base + (p0.c - base) * kk;
                p.s = p0.s * kk;
                break;
            }
            case as::POP: {
                const float kk = p_ > e.gone ? smoother(k) : easeOutBack(k, 1.2f);
                const vec3 base((e.lo.x + e.hi.x) * 0.5f, e.lo.y, (e.lo.z + e.hi.z) * 0.5f);
                p.c = base + (p0.c - base) * kk;
                p.s = p0.s * std::max(0.f, kk);
                break;
            }
            case as::DRIVE: {
                // it drives in from the right, and away again
                const float u = 1.f - smoother(k);
                p.c.x += 60.f * u * (p_ > e.gone ? 1.f : 1.f);
                if (p0.param > 8.5f) p.q = qMul(p0.q, qYaw(drum_));
                break;
            }
            default: break;
            }
            push(p, lit(p0), p0.tiles ? tk : 0.f);
        }
    }
}

// =====================================================================================================
// The cranes
// =====================================================================================================

vec3 ArchitectUniverse::trolleyPos(int i) const {
    const Crane& c = cranes_[i];
    return vec3(as::CRANE_X[i] + std::cos(c.a) * c.r, mastH(i) + 0.2f, as::CRANE_Z[i] - std::sin(c.a) * c.r);
}
vec3 ArchitectUniverse::hookPos(int i) const {
    const Crane& c = cranes_[i];
    const vec3 t = trolleyPos(i);
    return vec3(t.x + c.sx, c.h, t.z + c.sz);
}
float ArchitectUniverse::clearance(int i) const {
    float top = 6.f;
    for (size_t k = 0; k < prog_.size(); k++)
        if (els_[k].state == 3) top = std::max(top, prog_[k].hi.y);
    return std::min(mastH(i) - 2.f, top + 3.5f + SLING + 3.5f);
}
// Where a crane picks its load up: on the ground behind the well, hidden by it.
vec3 ArchitectUniverse::hiddenYard(int i) const {
    // the ray through the screen's middle, a little under it, down to the ground
    const vec4 a = invVp_ * vec4(0.f, -0.15f, -1.f, 1.f), b = invVp_ * vec4(0.f, -0.15f, 1.f, 1.f);
    const vec3 ro(a.x / a.w, a.y / a.w, a.z / a.w), rd = normalize(vec3(b.x / b.w, b.y / b.w, b.z / b.w) - ro);
    vec3 p = rd.y < -1e-3f ? ro + rd * (-ro.y / rd.y) : vec3(0.f, 0.f, -18.f);
    p.z = std::min(p.z, -14.f);
    p.x += i ? 1.6f : -1.6f;
    // within reach
    const vec3 base(as::CRANE_X[i], 0.f, as::CRANE_Z[i]);
    vec3 d = p - base;
    d.y = 0.f;
    const float l = length(d);
    if (l > as::JIB_L - 2.f) p = base + d * ((as::JIB_L - 2.f) / l);
    p.y = 0.2f;
    return p;
}

// Every element the paced lines have earned stands in place (or is on its way down, on a hook).
bool ArchitectUniverse::built() const {
    for (size_t i = 0; i < prog_.size(); i++)
        if (prog_[i].kind == as::CRANE && p_ >= prog_[i].due && els_[i].state != 3) return false;
    return true;
}

void ArchitectUniverse::setDown(int i) {
    ElState& s = els_[i];
    s.state = 3;
    s.t = 0.f;
    s.vis = 1.f;
    s.direct = true;
    for (auto& c : cranes_) {
        c.queue.erase(std::remove(c.queue.begin(), c.queue.end(), i), c.queue.end());
        if (c.job == i) { c.job = -1; c.phase = 0; }
    }
}

// The house's state comes from the paced lines (like the pool and the garden): an element is earned when the lines
// reach its due mark, and the cranes only animate the deliveries. They work faster with a backlog; an element that
// waits too long for its crane (or has no crane standing to lower it) is set down directly, settling from above, so
// a structural element is never left out. The cranes leave, and the lights come on, only once the house is complete.
void ArchitectUniverse::updateCranes(float dt, float workDt) {
    const bool complete = built();
    houseK_ = complete ? std::min(1.f, houseK_ + dt / 2.f) : 0.f;
    // the erection and the departure, from the progress
    for (int i = 0; i < 2; i++) {
        Crane& c = cranes_[i];
        const float up = smoother((p_ - as::L_CRANES - 0.6f * i) / 3.f);
        c.erect = up;
        const bool idle = c.job < 0 && c.queue.empty() && complete;
        const float dTarget = smoother((p_ - as::L_LEAVE - 0.9f * i) / 2.2f);
        if (dTarget < c.depart) c.depart = dTarget;
        else if (idle) c.depart = std::min(dTarget, c.depart + dt * 0.25f);
    }
    // the earned elements join their crane's queue
    for (size_t i = 0; i < prog_.size(); i++) {
        const as::Element& e = prog_[i];
        if (e.kind != as::CRANE) continue;
        ElState& s = els_[i];
        if (s.state == 0 && p_ >= e.due && pTarget_ >= e.due) {
            s.state = 1;
            s.wait = 0.f;
            cranes_[e.crane].queue.push_back((int)i);
        }
    }
    // the catch-up: an element left waiting is set down without its crane (the oldest first)
    for (size_t i = 0; i < prog_.size(); i++) {
        ElState& s = els_[i];
        if (prog_[i].kind != as::CRANE || s.state != 1) continue;
        const Crane& c = cranes_[prog_[i].crane];
        const bool craneUp = c.erect > 0.999f && c.depart < 0.001f;
        s.wait += workDt;
        const int ahead = (int)(std::find(c.queue.begin(), c.queue.end(), (int)i) - c.queue.begin());
        if ((!craneUp && s.wait > 1.f) || s.wait > 14.f || ahead >= 4) setDown((int)i);
    }
    // (a paused frame moves nothing: no step to divide by)
    if (workDt <= 1e-6f) return;
    const float g = 9.81f;
    const int sub = std::max(1, (int)std::ceil(workDt / (1.f / 120.f)));
    const float h = workDt / sub;
    for (int i = 0; i < 2; i++) {
        Crane& c = cranes_[i];
        const vec3 base(as::CRANE_X[i], 0.f, as::CRANE_Z[i]);
        const float rest = std::atan2(-(-2.f - base.z), -4.f - base.x);
        if (!c.primed) {
            c.a = c.ta = rest;
            c.r = c.tr = 13.f;
            c.h = c.th = mastH(i) - 6.f;
            c.primed = true;
            c.lastT = trolleyPos(i);
        }
        const bool ready = c.erect > 0.999f && c.depart < 0.001f;
        const int qn = (int)c.queue.size();
        const float speed = std::min(3.f, 1.f + 0.4f * qn) * (ready ? 1.f : 0.f);
        // ---- the job: to the yard, down, hook on, up, across, down softly, let go, up
        auto angleTo = [&](const vec3& p) { return c.a + wrapPi(std::atan2(-(p.z - base.z), p.x - base.x) - c.a); };
        auto radiusTo = [&](const vec3& p) { return std::clamp(std::sqrt((p.x - base.x) * (p.x - base.x) + (p.z - base.z) * (p.z - base.z)), 3.5f, as::JIB_L - 1.f); };
        const float hiY = clearance(i);
        if (ready && c.job < 0 && !c.queue.empty()) {
            c.job = c.queue.front();
            c.queue.erase(c.queue.begin());
            c.phase = 1;
            c.jobT = 0.f;
            c.yard = hiddenYard(i);
        }
        if (c.job >= 0 && (els_[c.job].state == 0 || els_[c.job].state == 3)) { c.job = -1; c.phase = 0; }
        // a watchdog: a delivery that takes far too long ends with its element set down
        if (c.job >= 0) {
            c.jobT += workDt;
            if (c.jobT > 30.f) setDown(c.job);
        }
        const bool angOk = std::fabs(c.a - c.ta) < 0.012f && std::fabs(c.av) < 0.03f;
        const bool radOk = std::fabs(c.r - c.tr) < 0.15f && std::fabs(c.rv) < 0.2f;
        const bool hOk = std::fabs(c.h - c.th) < 0.06f && std::fabs(c.hv) < 0.08f;
        if (c.job >= 0) {
            const as::Element& e = prog_[c.job];
            ElState& s = els_[c.job];
            const float loadH = e.hi.y - e.lo.y;
            switch (c.phase) {
            case 1: // over the yard, high
                c.ta = angleTo(c.yard);
                c.tr = radiusTo(c.yard);
                c.th = std::max(c.h, std::min(hiY, c.h + 2.f));
                if (angOk && radOk) c.phase = 2;
                break;
            case 2: // down to the load
                c.th = c.yard.y + loadH + SLING;
                if (hOk) { c.phase = 3; c.wait = 0.f; }
                break;
            case 3: // hooked on
                c.wait += workDt * speed;
                if (c.wait > 0.5f) { s.state = 2; c.phase = 4; }
                break;
            case 4: // up
                c.th = hiY;
                if (c.h > hiY - 1.2f) c.phase = 5;
                break;
            case 5: // across, the load swinging
                c.ta = angleTo(e.anchor);
                c.tr = radiusTo(e.anchor);
                c.th = hiY;
                if (angOk && radOk && std::fabs(c.sx) + std::fabs(c.sz) < 0.6f) c.phase = 6;
                break;
            case 6: // down, softly
                c.th = e.anchor.y + SLING;
                c.tr = radiusTo(e.anchor);
                if (hOk) {
                    s.state = 3;
                    s.t = 0.f;
                    s.vis = 1.f;
                    c.phase = 7;
                    c.wait = 0.f;
                }
                break;
            case 7: // let go
                c.wait += workDt * speed;
                if (c.wait > 0.5f) c.phase = 8;
                break;
            case 8: // up, free
                c.th = std::min(mastH(i) - 4.f, e.hi.y + SLING + 4.f);
                if (c.h > c.th - 0.8f) { c.job = -1; c.phase = 0; }
                break;
            default: break;
            }
        } else {
            // between jobs: a slow wander over the site, never still
            const float t = worldTime_ + 37.f * i;
            const float act = 0.6f + 0.4f * smoothstepf(0.3f, 1.5f, lv_);
            c.ta = c.a + wrapPi(rest + (0.28f * std::sin(t * 0.045f) + 0.12f * std::sin(t * 0.11f + 1.f)) * act - c.a);
            c.tr = 13.f + 6.f * std::sin(t * 0.037f + 2.f * i) * act;
            c.th = (c.erect > 0.999f ? mastH(i) - 7.f : mastH(i) - 3.f) + 2.f * std::sin(t * 0.06f + i);
            if (!ready) c.ta = rest, c.tr = 8.f;
        }
        // ---- the motion: critically damped springs, capped speeds (eased starts and stops), then the sway
        const float vS = 0.16f * std::max(speed, 0.5f), vR = 2.2f * std::max(speed, 0.5f), vH = 2.0f * std::max(speed, 0.5f);
        const float idleK = c.job < 0 ? 0.5f : 1.f;
        for (int k = 0; k < sub; k++) {
            auto spring = [&](float& x, float& v, float target, float w, float vmax) {
                const float acc = w * w * (target - x) - 2.f * w * v;
                v += acc * h;
                v = std::clamp(v, -vmax, vmax);
                x += v * h;
            };
            spring(c.a, c.av, c.ta, 0.9f, vS * idleK);
            spring(c.r, c.rv, c.tr, 1.1f, vR * idleK);
            // the last meters of a lowering are slower: a soft landing
            const float landK = c.phase == 6 ? 0.25f + 0.75f * smoothstepf(0.3f, 3.f, c.h - c.th) : 1.f;
            spring(c.h, c.hv, c.th, 1.2f, vH * idleK * landK);
            const vec3 tp = trolleyPos(i);
            const vec3 v = (tp - c.lastT) / h;
            const vec3 acc = (v - c.lastV) / h;
            c.lastT = tp;
            c.lastV = v;
            const float L = std::max(3.f, mastH(i) - c.h);
            const float w2 = g / L;
            c.svx += (-w2 * c.sx - 0.45f * c.svx - std::clamp(acc.x, -4.f, 4.f) * 0.9f) * h;
            c.svz += (-w2 * c.sz - 0.45f * c.svz - std::clamp(acc.z, -4.f, 4.f) * 0.9f) * h;
            c.sx = std::clamp(c.sx + c.svx * h, -1.6f, 1.6f);
            c.sz = std::clamp(c.sz + c.svz * h, -1.6f, 1.6f);
            // the load turning a little on its slings
            c.wobV += (-0.6f * c.wob - 0.3f * c.wobV + c.av * 0.8f) * h;
            c.wob = std::clamp(c.wob + c.wobV * h, -0.12f, 0.12f);
        }
        // a landing at an angle never shows: the swing is gone by the time it touches
        if (c.phase == 6) {
            const float near = 1.f - smoothstepf(0.5f, 3.f, c.h - c.th);
            c.sx *= 1.f - near * 0.08f;
            c.sz *= 1.f - near * 0.08f;
            c.wob *= 1.f - near * 0.08f;
        }
        // (never let a bad step poison the motion for good)
        if (!std::isfinite(c.a + c.av + c.r + c.rv + c.h + c.hv + c.sx + c.sz + c.svx + c.svz + c.wob + c.wobV)) {
            c.a = c.ta = rest;
            c.r = c.tr = 13.f;
            c.h = c.th = mastH(i) - 6.f;
            c.av = c.rv = c.hv = c.sx = c.sz = c.svx = c.svz = c.wob = c.wobV = 0.f;
            c.lastT = trolleyPos(i);
            c.lastV = vec3(0, 0, 0);
            if (c.job >= 0) setDown(c.job);
        }
        c.beacon = approach(c.beacon, 0.f, 3.f, dt);
    }
}

void ArchitectUniverse::drawCranes() {
    const as::Palette& P = pal_;
    const vec3 paint = P.crane, dark(0.05f, 0.05f, 0.055f), steel(0.35f, 0.36f, 0.38f);
    for (int i = 0; i < 2; i++) {
        const Crane& c = cranes_[i];
        const float e = c.erect * (1.f - c.depart);
        if (e <= 0.001f) continue;
        const vec3 base(as::CRANE_X[i], 0.f, as::CRANE_Z[i]);
        const float H = mastH(i);
        // the pad, the mast climbing (it rises first), then the head and the jib swinging out
        pushRaw(as::BOX, base + vec3(0, 0.3f, 0), vec3(4.4f, 0.6f, 4.4f), Q{}, as::CONCRETE, P.concrete * 0.95f);
        const float mk = smoother(e / 0.6f);
        const float mh = H * mk;
        pushRaw(as::BOX, base + vec3(0, 0.6f + mh * 0.5f, 0), vec3(1.7f, mh, 1.7f), Q{}, as::LATTICE, paint);
        const float jk = smoother((e - 0.55f) / 0.45f);
        if (jk <= 0.001f) continue;
        const Q q = qYaw(c.a);
        const vec3 dir(std::cos(c.a), 0.f, -std::sin(c.a));
        const vec3 top = base + vec3(0, 0.6f + mh, 0);
        // the slewing ring and the cab
        pushRaw(as::BOX, top + vec3(0, 0.45f, 0), vec3(2.3f, 0.9f, 2.3f), q, as::STEEL, paint * 0.85f);
        pushRaw(as::BOX, top + vec3(0, 1.3f, 0) + qRot(q, vec3(0.9f, 0.f, 1.5f)), vec3(1.5f, 1.8f, 1.4f), q, as::STEEL, vec3(0.85f, 0.85f, 0.82f));
        pushRaw(as::BOX, top + vec3(0, 1.5f, 0) + qRot(q, vec3(1.66f, 0.f, 1.5f)), vec3(0.04f, 1.0f, 1.2f), q, as::FRAME, vec3(0.04f, 0.06f, 0.08f));
        // the tower head
        const float headH = 4.f * jk;
        pushRaw(as::BOX, top + vec3(0, 0.9f + headH * 0.5f, 0), vec3(1.1f, headH, 1.1f), q, as::LATTICE, paint);
        // the jib and the counter-jib
        const float J = as::JIB_L * jk, C = as::COUNTER_L * jk;
        const vec3 jy = top + vec3(0, 1.45f, 0);
        pushRaw(as::BOX, jy + dir * (J * 0.5f + 0.6f), vec3(J, 1.1f, 1.1f), q, as::LATTICE, paint);
        pushRaw(as::BOX, jy - dir * (C * 0.5f + 0.6f), vec3(C, 0.9f, 1.4f), q, as::LATTICE, paint);
        pushRaw(as::BOX, jy - dir * (C * 0.5f + 0.6f) + vec3(0, 0.5f, 0), vec3(C, 0.1f, 1.5f), q, as::STEEL, steel);
        for (int k = 0; k < 3; k++)
            pushRaw(as::BOX, jy - dir * (C - 0.6f - k * 1.05f) + vec3(0, -0.9f, 0), vec3(1.0f, 1.9f, 1.8f), q, as::CONCRETE, vec3(0.32f, 0.32f, 0.31f));
        // the tie rods from the head's top
        const vec3 apex = top + vec3(0, 0.9f + headH, 0);
        auto rodTo = [&](const vec3& a, const vec3& b, float w, int mat, const vec3& col) {
            const vec3 d = b - a;
            pushRaw(as::CYL, (a + b) * 0.5f, vec3(w, length(d), w), as::qFromY(d), mat, col);
        };
        rodTo(apex, jy + dir * (J * 0.62f) + vec3(0, 0.5f, 0), 0.08f, as::STEEL, steel);
        rodTo(apex, jy - dir * (C + 0.4f) + vec3(0, 0.5f, 0), 0.08f, as::STEEL, steel);
        // the beacons: at the tip and the apex (red, blinking at a peak)
        const float bl = 0.35f + 0.65f * c.beacon;
        glow(apex + vec3(0, 0.25f, 0), 0.9f + 0.6f * c.beacon, vec3(1.f, 0.12f, 0.06f) * (0.8f + 2.5f * bl) * (0.4f + 0.6f * night_ + 0.3f * c.beacon), 0.4f);
        glow(jy + dir * (J + 0.6f), 0.8f + 0.5f * c.beacon, vec3(1.f, 0.12f, 0.06f) * (0.8f + 2.5f * bl) * (0.4f + 0.6f * night_ + 0.3f * c.beacon), 0.4f);
        if (jk < 0.999f) continue;
        // the trolley, the cables, the hook block, the slings to the load
        const vec3 tp = trolleyPos(i);
        pushRaw(as::BOX, tp + vec3(0, 0.f, 0), vec3(1.2f, 0.5f, 1.3f), q, as::STEEL, steel);
        const vec3 hk = hookPos(i);
        for (float o : {-0.12f, 0.12f}) {
            const vec3 side = qRot(q, vec3(0, 0, o));
            rodTo(tp + side - vec3(0, 0.2f, 0), hk + side * 0.5f + vec3(0, 0.45f, 0), 0.035f, as::CABLE, dark);
        }
        pushRaw(as::BOX, hk + vec3(0, 0.15f, 0), vec3(0.55f, 0.75f, 0.35f), qMul(q, qYaw(c.wob)), as::STEEL, lin(0xf2c200));
        if (c.job >= 0 && els_[c.job].state == 2) {
            const as::Element& e2 = prog_[c.job];
            const vec3 at = hk - vec3(0.f, SLING, 0.f);
            const float hx = std::min(2.2f, (e2.hi.x - e2.lo.x) * 0.45f), hz = std::min(2.2f, (e2.hi.z - e2.lo.z) * 0.45f);
            const Q yq = qYaw(c.wob);
            for (int k = 0; k < 4; k++) {
                const vec3 corner = at + qRot(yq, vec3(k % 2 ? hx : -hx, 0.f, k < 2 ? hz : -hz));
                rodTo(hk - vec3(0, 0.2f, 0), corner, 0.03f, as::CABLE, dark);
            }
        }
    }
    // the site's light towers (with the cranes): their lamps come on at the drop
    const float tw = std::min(cranes_[0].erect * (1.f - cranes_[0].depart), 1.f);
    if (tw > 0.01f) {
        const vec3 pos[2] = {vec3(-34.f, 0.f, 4.f), vec3(30.f, 0.f, 2.f)};
        for (int i = 0; i < 2; i++) {
            const float th = 10.f * smoother(tw);
            pushRaw(as::BOX, pos[i] + vec3(0, 0.35f, 0), vec3(2.2f, 0.7f, 1.4f), Q{}, as::STEEL, P.crane * 0.9f);
            pushRaw(as::CYL, pos[i] + vec3(0, th * 0.5f, 0), vec3(0.22f, th, 0.22f), Q{}, as::STEEL, steel);
            const vec3 head = pos[i] + vec3(0, th, 0);
            const Q aim = qYaw(i ? 0.9f : -0.9f);
            pushRaw(as::BOX, head, vec3(2.0f, 0.35f, 0.2f), aim, as::STEEL, dark);
            for (int k = 0; k < 4; k++) {
                const vec3 lp = head + qRot(aim, vec3(-0.75f + 0.5f * k, 0.38f, 0.12f));
                pushRaw(as::BOX, lp, vec3(0.42f, 0.42f, 0.12f), aim, as::LAMP, vec3(1.f, 0.92f, 0.78f), 0.f, 4.f * flood_);
                if (flood_ > 0.02f) glow(lp + qRot(aim, vec3(0, 0, 0.3f)), 1.6f, vec3(1.f, 0.9f, 0.75f) * (1.5f * flood_), 0.2f);
            }
        }
    }
}

void ArchitectUniverse::glow(const vec3& p, float size, const vec3& col, float soft) {
    const float v[8] = {p.x, p.y, p.z, size, col.x, col.y, col.z, soft};
    glows_.insert(glows_.end(), v, v + 8);
}

// =====================================================================================================
// The well
// =====================================================================================================

void ArchitectUniverse::syncCells(const Game& g) {
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const bool full = g.cell(x, y).type >= 0;
            if (full && seeds_[y][x] == 0.f) seeds_[y][x] = 0.001f + hashf((uint32_t)(x * 131 + y * 977 + (int)(time_ * 10.f)));
            if (!full) seeds_[y][x] = 0.f;
        }
}

void ArchitectUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        // a small puff of concrete dust from under the piece, at its contact
        int maxY = -1;
        for (auto& c : ev.cells) maxY = std::max(maxY, c.y);
        float x0 = 99.f, x1 = -99.f;
        for (auto& c : ev.cells)
            if (c.y == maxY) { x0 = std::min(x0, (float)c.x); x1 = std::max(x1, (float)c.x); }
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            seeds_[c.y][c.x] = 0.001f + hashf((uint32_t)(c.x * 31 + c.y * 131 + (int)(time_ * 100.f)));
        }
        if (maxY >= Game::HIDDEN && hardPending_) {
            const vec3 l = layout::cellPos(x0, (float)maxY), r = layout::cellPos(x1, (float)maxY);
            for (int k = 0; k < 6; k++) {
                const bool right = k % 2;
                Dust d;
                d.p = (right ? r + vec3(0.45f, -0.45f, 0.1f) : l + vec3(-0.45f, -0.45f, 0.1f));
                d.v = vec3((right ? 1.f : -1.f) * rng_.range(0.4f, 1.1f), rng_.range(0.05f, 0.35f), 0.f);
                d.t = 0.f;
                d.life = rng_.range(0.45f, 0.7f);
                d.size = rng_.range(0.18f, 0.3f);
                dust_.push_back(d);
            }
        }
        hardPending_ = false;
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        int rowK = 0;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (!rows[y]) continue;
            for (auto& c : ev.cells) {
                if (c.y != y) continue;
                clearing_.push_back({c.x, y, std::clamp(c.type, 0, 6), seeds_[y][c.x] > 0.f ? seeds_[y][c.x] : 0.5f, 0.f,
                                     0.02f * std::fabs(c.x - 4.5f) + 0.03f * rowK});
            }
            rowK++;
        }
        float ns[Game::H][Game::W] = {};
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (rows[y]) continue;
            std::memcpy(ns[dst], seeds_[y], sizeof(seeds_[y]));
            dst--;
        }
        std::memcpy(seeds_, ns, sizeof(seeds_));
        if (ev.count >= 4) {
            // a Tetris: every crane lamp lit, a shower of sparks over the villa
            for (auto& c : cranes_) c.beacon = 1.f;
            swell_ = std::max(swell_, 0.5f);
            for (int k = 0; k < 40; k++) {
                Spark s;
                s.p = vec3(rng_.range(-22.f, 18.f), rng_.range(5.f, 9.f), rng_.range(-5.f, 4.f));
                s.v = vec3(rng_.range(-1.f, 1.f), rng_.range(0.5f, 3.f), rng_.range(-0.5f, 1.f));
                s.t = -rng_.range(0.f, 0.8f);
                s.life = rng_.range(0.8f, 1.5f);
                s.size = rng_.range(0.2f, 0.4f);
                sparks_.push_back(s);
            }
        }
        break;
    }
    case GameEvent::HardDrop: hardPending_ = true; break;
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

void ArchitectUniverse::quad(const vec3& c, float hw, float hh, const vec3& col, float a, float sx, float sy) {
    const float q[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
    for (auto& k : q) {
        const float v[11] = {c.x + k[0] * hw, c.y + k[1] * hh, c.z, col.x, col.y, col.z, a, k[0], k[1], sx, sy};
        flat_.insert(flat_.end(), v, v + 11);
    }
}
void ArchitectUniverse::line(float x0, float y0, float x1, float y1, float z, float w, const vec3& col, float a) {
    const float ex = x1 - x0, ey = y1 - y0, l = std::sqrt(ex * ex + ey * ey);
    if (l < 1e-5f) return;
    const float nx = -ey / l * w * 0.5f, ny = ex / l * w * 0.5f;
    const float v[6][2] = {{x0 - nx, y0 - ny}, {x1 - nx, y1 - ny}, {x1 + nx, y1 + ny}, {x0 - nx, y0 - ny}, {x1 + nx, y1 + ny}, {x0 + nx, y0 + ny}};
    for (auto& p : v) {
        const float vv[11] = {p[0], p[1], z, col.x, col.y, col.z, a, 0.f, 0.f, 0.f, 0.f};
        flat_.insert(flat_.end(), vv, vv + 11);
    }
}

void ArchitectUniverse::buildBoard(const Game& g) {
    blocks_.clear();
    flat_.clear();
    if (bare_) return;
    const VariantDef& V = VARIANTS[variant_];
    const vec3 back = lin(V.backing);
    // the backing: a deep even panel behind the well, a soft shade around it, a fine pale border
    quad(vec3(0.f, 0.f, BACK_Z - 0.01f), 5.9f, 10.9f, vec3(0.f), 0.28f, 0.14f, 0.075f);
    quad(vec3(0.f, 0.f, BACK_Z), 5.12f, 10.12f, back, 1.f, 0.f, 0.f);
    const vec3 rule = mixDisplay(back, lin(0xf4efe6), 0.35f);
    const float bx = 5.12f, by = 10.12f, zr = BACK_Z + 0.002f;
    line(-bx, -by, bx, -by, zr, 0.03f, rule, 0.55f);
    line(-bx, by, bx, by, zr, 0.03f, rule, 0.55f);
    line(-bx, -by, -bx, by, zr, 0.03f, rule, 0.55f);
    line(bx, -by, bx, by, zr, 0.03f, rule, 0.55f);

    const float CS = 0.93f, CD = 0.5f;
    auto block = [&](const vec3& c, float sc, int type, float seed, float dull, float lift, float shadowA) {
        const vec3 col = pieceCol(type);
        blocks_.push_back({{c.x, c.y, c.z - CD * 0.5f * sc, seed}, {CS * sc, CS * sc, CD * sc, 0.f},
                           {col.x, col.y, col.z, 0.f}, {0.f, 0.f, 0.f, 1.f}, {dull, lift, 0.f, 0.f}});
        if (shadowA > 0.f) quad(vec3(c.x + 0.07f * sc, c.y - 0.1f * sc, BACK_Z + 0.004f), 0.56f * sc, 0.56f * sc, vec3(0.f), shadowA, 0.45f, 0.45f);
    };
    // the dull of a game over, from the top down
    auto dullAt = [&](int y) {
        if (overT_ < 0.f) return 0.f;
        return smoothstepf(0.f, 0.5f, overT_ - 0.05f * (float)(y - Game::HIDDEN));
    };
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            block(p, 1.f, c.type, seeds_[y][x] > 0.f ? seeds_[y][x] : 0.3f, dullAt(y), 0.f, 0.4f);
        }
    // the cleared rows: the panels slide back into the board, under half a second
    for (const ClearCell& cc : clearing_) {
        const float u = saturate((cc.t - cc.delay) / 0.34f);
        const vec3 p = layout::cellPos((float)cc.x, (float)cc.y) + vec3(0.f, 0.f, -1.3f * u * u);
        block(p, 1.f - 0.15f * u, cc.type, cc.seed, 0.f, 0.25f * (1.f - u) * smoothstepf(0.f, 0.2f, cc.t - cc.delay + 0.1f), 0.f);
    }
    // the falling piece and its ghost
    if (g.hasPiece() && !g.over()) {
        const Piece& pc = g.piece();
        int cells[4][2];
        g.pieceCells(pc, cells);
        const float dx = g.pieceVisualX() - pc.x, dy = g.pieceVisualY() - pc.y;
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            if (c[1] < Game::HIDDEN - 1) continue;
            block(layout::cellPos(c[0] + dx, c[1] + dy), 1.f, pc.type, 0.17f + 0.13f * i, 0.f, 0.f, 0.35f);
        }
        Piece gp = pc;
        gp.y = g.ghostY();
        if (gp.y > pc.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto inG = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            const vec3 ink = mixDisplay(back, lin(0xf4efe6), 0.45f);
            const float in = 0.08f, w = 0.05f, zz = BACK_Z + 0.006f;
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 p = layout::cellPos((float)c[0], (float)c[1]);
                const float l = p.x - 0.5f + in, r = p.x + 0.5f - in, t = p.y + 0.5f - in, b = p.y - 0.5f + in;
                const bool L = inG(c[0] - 1, c[1]), R = inG(c[0] + 1, c[1]), U = inG(c[0], c[1] - 1), D = inG(c[0], c[1] + 1);
                const float l2 = L ? p.x - 0.5f : l, r2 = R ? p.x + 0.5f : r, t2 = U ? p.y + 0.5f : t, b2 = D ? p.y - 0.5f : b;
                if (!L) line(l, b2, l, t2, zz, w, ink, 0.8f);
                if (!R) line(r, b2, r, t2, zz, w, ink, 0.8f);
                if (!U) line(l2, t, r2, t, zz, w, ink, 0.8f);
                if (!D) line(l2, b, r2, b, zz, w, ink, 0.8f);
            }
        }
    }
    // the dust of a hard drop
    for (const Dust& d : dust_) {
        const float u = saturate(d.t / d.life);
        const vec3 p = d.p + d.v * (d.t * (1.f - 0.4f * u));
        quad(p, d.size * (1.f + 1.6f * u), d.size * (1.f + 1.6f * u), mixDisplay(back, lin(0xd8d2c8), 0.7f), 0.3f * (1.f - u) * smoothstepf(0.f, 0.08f, d.t), 1.f, 1.f);
    }
    // the previews
    auto preview = [&](int type, vec2 center, float sc, float dim) {
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
            const auto& c = cells[i];
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            block(vec3(px, py, 0.f), sc, type, 0.11f + 0.17f * i + 0.05f * type, dim, 0.f, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.6f : 0.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 0.f);
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float ArchitectUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
    }
    return w - track;
}

void ArchitectUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                                  std::vector<float>& out) const {
    if (!f.ok) return;
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
    }
}

void ArchitectUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    ugl::set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", haloA);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void ArchitectUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    ugl::set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

const char* ArchitectUniverse::stageName() const {
    const float p = p_;
    if (p < as::L_LEVEL) return "the field";
    if (p < as::L_SLAB + 2.f) return "groundworks";
    if (p < as::L_WALLS) return "the cranes";
    if (p < as::L_GLASS) return "the structure";
    if (p < as::L_POOL) return "glazing";
    if (p < as::L_GARDEN) return "the pool";
    if (p < as::L_LEAVE) return "the garden";
    if (p < as::L_DONE - 0.5f) return "finishing";
    return "completed";
}

// The lettering in a dark graphite by day, a warm white at dusk (with a halo of the other); the bonus gauge a
// ring of twelve fine strokes, like an architect's scale, the charged ones in the accent.
void ArchitectUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    const VariantDef& V = VARIANTS[variant_];
    const float dark = night_;
    const vec3 inkL = lerp(lin(0x22252a), lin(0xf4eee2), dark), haloL = lerp(lin(0xf6f3ee), lin(0x0c0e14), dark);
    const vec3 ink = ugl::toSrgb(inkL), halo = ugl::toSrgb(haloL);
    const vec3 accent = ugl::toSrgb(dark > 0.5f ? lin(V.accent) : mixDisplay(lin(V.accent), lin(0x000000), 0.15f));
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                   {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : q) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }

    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = Game::BONUS_LINES;
        auto stroke = [&](float cx, float cy, float ang, float len, float wid, const vec3& col, float al) {
            const float ux = std::sin(ang), uy = -std::cos(ang), vx = -uy, vy = ux;
            const float hl = len * 0.5f, hw = wid * 0.5f;
            const float px[4] = {cx - ux * hl - vx * hw, cx + ux * hl - vx * hw, cx + ux * hl + vx * hw, cx - ux * hl + vx * hw};
            const float py[4] = {cy - uy * hl - vy * hw, cy + uy * hl - vy * hw, cy + uy * hl + vy * hw, cy - uy * hl + vy * hw};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {px[k], py[k], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        };
        for (int i = 0; i < N; i++) {
            const float a = TAU * i / N;
            const float cx = c.x + std::sin(a) * r * 0.78f, cy = c.y - std::cos(a) * r * 0.78f;
            const float part = saturate(fill * N - i);
            const float al = (g.bonusReady() ? 0.85f : 0.5f) * vis * (1.f - 0.5f * dimT_);
            stroke(cx, cy, a, r * 0.42f, r * 0.07f, halo, al * 0.5f);
            stroke(cx, cy, a, r * 0.38f, r * 0.04f, ink, al * (1.f - part) * 0.7f);
            stroke(cx, cy, a, r * 0.42f, r * 0.075f, accent, part * 0.95f * vis * (1.f - 0.5f * dimT_));
        }
        flushShapes();
    }

    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
            return w;
        };
        auto upper = [&](std::string t) {
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            const std::string u = upper(h.text);
            const float w = place(fontReg_, u, base * 0.7f, base * 0.22f, base * 0.1f);
            drawText(fontReg_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f));
            // the site's stage after it
            if (h.x >= 0 && !h.center) {
                q.clear();
                textQuads(fontLight_, std::string("  ") + stageName(), h.x + w + base * 0.3f, h.y + base * 0.1f, base * 0.74f, base * 0.06f, q);
                drawText(fontLight_, q, accent, halo, saturate(0.3f + h.alpha * 1.5f) * 0.95f);
            }
            continue;
        }
        case HUD_SECTION:
            place(fontReg_, upper(h.text), base * 0.86f, base * 0.2f);
            drawText(fontReg_, q, accent, halo, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(fontLight_, h.text, base * 1.12f, base * 0.02f, -base * 0.1f);
            drawText(fontLight_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontLight_, h.text, base * 0.72f, base * 0.08f);
            drawText(fontLight_, q, ink, halo, saturate(alpha * 1.2f) * 0.8f);
            continue;
        case HUD_LABEL:
            place(fontReg_, upper(h.text), base * 0.6f, base * 0.3f, base * 0.06f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.8f);
            continue;
        case HUD_VALUE:
            place(fontLight_, h.text, base * 1.04f, base * 0.04f);
            drawText(fontLight_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontReg_, h.text, base * 0.95f, base * 0.06f);
            drawText(fontReg_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontReg_, h.text, base * 0.85f, 0.f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontReg_, upper(h.text), base * 1.1f, base * 0.2f, -base * 0.1f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontLight_, h.text, base * 0.95f, base * 0.05f);
            drawText(fontLight_, q, accent, halo, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontReg_, h.text, base * 1.05f, base * 0.08f, -base * 0.08f);
                drawText(fontReg_, q, ink, halo, alpha);
            } else {
                place(fontLight_, h.text, base * 0.82f, base * 0.04f);
                drawText(fontLight_, q, h.accent ? accent : ink, halo, alpha);
            }
            continue;
        default:
            place(fontLight_, h.text, base * 0.8f, base * 0.03f);
            drawText(fontLight_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void ArchitectUniverse::drawInstances(GLuint prog, bool shadowPass) {
    glUseProgram(prog);
    for (int m = 0; m < as::MESH_COUNT; m++) {
        if (inst_[m].empty()) continue;
        set1i(prog, "uMesh", m);
        glBindVertexArray(meshVao_[m]);
        glDrawArraysInstanced(GL_TRIANGLES, 0, meshCount_[m], (GLsizei)inst_[m].size());
    }
    (void)shadowPass;
}

void ArchitectUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    // ZEN_GPUTIME: the GPU time of a frame, printed every 120 frames
    static const bool gpuTime = std::getenv("ZEN_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    if (builtVariant_ != variant_) buildSite();
    if (!bare_) syncCells(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += f.paused ? 0.f : dt;
    worldTime_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    swell_ = approach(swell_, 0.f, 0.3f, dt);
    if (!dropMode_) dropRelease_ = std::max(0.f, dropRelease_ - dt / 8.f);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    // the floodlights: on with the drop, held through the peak, off slowly after it
    const float floodT = overT_ >= 0.f ? 0.f : std::max(dropRelease_, dropMode_ ? 1.f : 0.f);
    flood_ = floodT > flood_ ? std::min(floodT, flood_ + dt / 1.4f) : approach(flood_, floodT, 0.5f, dt);
    // the beat grid (every two beats): sparks at the work, the beacons at a peak
    bool tick = false;
    if (!f.paused) {
        if (music.hasSong) {
            const double hb = std::floor(music.beat / 2.0 + 1e-4);
            if (lastHalf_ < 0.0 || hb < lastHalf_ - 8.0) lastHalf_ = hb;
            if (hb > lastHalf_) { lastHalf_ = hb; tick = true; }
        } else {
            beatClock_ += wdt;
            if (beatClock_ > 1.f) { beatClock_ = 0.f; tick = true; }
        }
    }
    if (tick && overT_ < 0.f) {
        if (wp > 0.3f)
            for (auto& c : cranes_) c.beacon = std::max(c.beacon, 0.6f + 0.4f * wp);
        // a welding spark at the work (the last element placed), soft
        if (wm > 0.3f && p_ > as::L_WALLS && p_ < as::L_LEAVE) {
            int last = -1;
            float best = 1e9f;
            for (size_t i = 0; i < prog_.size(); i++)
                if (prog_[i].kind == as::CRANE && els_[i].state == 3 && els_[i].t < best) { best = els_[i].t; last = (int)i; }
            if (last >= 0) {
                const as::Element& e = prog_[last];
                const vec3 at(rng_.chance(0.5f) ? e.lo.x + 0.2f : e.hi.x - 0.2f, e.hi.y - rng_.range(0.f, 1.2f), e.hi.z + 0.1f);
                const int n = 3 + (int)(3.f * wp);
                for (int k = 0; k < n; k++) {
                    Spark s;
                    s.p = at;
                    s.v = vec3(rng_.range(-1.2f, 1.2f), rng_.range(0.2f, 1.8f), rng_.range(0.f, 1.2f));
                    s.t = 0.f;
                    s.life = rng_.range(0.35f, 0.7f);
                    s.size = rng_.range(0.12f, 0.22f);
                    sparks_.push_back(s);
                }
            }
        }
    }
    drum_ += wdt * (0.4f + 1.2f * wm + 1.2f * wp + 1.5f * kickIn_);

    // ---- The progress: the paced lines (and the test hooks), shown with a capped speed.
    if (!bare_) {
        const float hr = hookRate();
        const float extra = hookLines() + (hr > 0.f ? hr * time_ : 0.f);
        pTarget_ = paceLines(game, extra);
        const int lines = linesHere(game);
        if (lastLines_ >= 0 && lines < lastLines_ && !game.over()) {
            // a new game: the villa sinks back, the field again
            for (size_t i = 0; i < els_.size(); i++)
                if (els_[i].state > 0) { els_[i].state = 0; els_[i].direct = false; }
            for (auto& c : cranes_) { c.queue.clear(); c.job = -1; c.phase = 0; }
            overT_ = -1.f;
        }
        lastLines_ = lines;
    }
    if (!pInit_) {
        p_ = pTarget_;
        pInit_ = true;
        placeDueNow();
        for (auto& c : cranes_) { c.erect = smoother((p_ - as::L_CRANES) / 3.f); }
    } else if (pTarget_ >= p_) {
        const float rate = 0.5f * std::max(1.f, paceScale());
        p_ = std::min(pTarget_, p_ + rate * wdt);
    } else {
        p_ = std::max(pTarget_, p_ - 12.f * dt);
    }
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    for (size_t i = 0; i < els_.size(); i++) {
        ElState& s = els_[i];
        if (s.state == 3) { s.t += wdt; s.vis = 1.f; }
        else if (s.state != 2) s.vis = std::max(0.f, s.vis - dt / 1.2f);
    }

    // ---- The cranes, the sparks, the clears, the dust.
    updateCranes(dt, wdt);
    for (Spark& s : sparks_) {
        s.t += wdt;
        if (s.t > 0.f) { s.v.y -= 6.f * wdt; s.p = s.p + s.v * wdt; }
    }
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& s) { return s.t > s.life; }), sparks_.end());
    for (ClearCell& c : clearing_) c.t += dt;
    clearing_.erase(std::remove_if(clearing_.begin(), clearing_.end(), [](const ClearCell& c) { return c.t > c.delay + 0.36f; }), clearing_.end());
    for (Dust& d : dust_) d.t += dt;
    dust_.erase(std::remove_if(dust_.begin(), dust_.end(), [](const Dust& d) { return d.t > d.life; }), dust_.end());
    if (!bare_) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The hour.
    const float tod = saturate(p_ / as::L_DONE);
    Sky sky = skyAt(tod);
    night_ = smoothstepf(0.9f, 1.f, tod);
    // slow motion: a hushed, misty blue hour
    sky.sunCol = lerp(sky.sunCol, sky.sunCol * vec3(0.55f, 0.62f, 0.85f) * 0.7f, slow_);
    sky.hor = lerp(sky.hor, mixDisplay(sky.hor, lin(0x8a9ab8), 0.6f), slow_);
    sky.top = lerp(sky.top, mixDisplay(sky.top, lin(0x4a5a88), 0.5f), slow_);
    const VariantDef& V = VARIANTS[variant_];
    const vec3 skyAmb = mixDisplay(sky.top, sky.hor, 0.45f);
    const vec3 ambSky = mixDisplay(skyAmb, vec3(ugl::luma(skyAmb)) * vec3(1.05f, 1.f, 0.92f), 0.45f) * (0.42f + 0.1f * swell_);
    const vec3 ambGround = lin(V.grassA) * (0.06f + 0.18f * saturate(sky.sunDir.y + 0.1f)) * (0.5f + 0.5f * sky.sunCol.x / 3.f);
    const vec3 fogCol = lerp(sky.hor, mixDisplay(sky.hor, lin(0xc8ccd4), 0.4f), slow_);
    const float fogDist = lerpf(450.f, 150.f, slow_) * (V.flora == 1 ? 0.75f : 1.f);

    // ---- The camera: a slow orbit (a few degrees each way), the yard's place follows it.
    camYaw_ = 0.045f * std::sin(worldTime_ * TAU / 140.f) + 0.02f * std::sin(worldTime_ * TAU / 61.f);
    updateCamera();

    // ---- Geometry.
    for (auto& v : inst_) v.clear();
    glows_.clear();
    drawElements();
    drawCranes();
    for (const Spark& s : sparks_) {
        if (s.t < 0.f) continue;
        const float u = saturate(s.t / s.life);
        glow(s.p, s.size * (1.f - 0.5f * u), vec3(1.f, 0.72f, 0.4f) * (2.2f * (1.f - u)), 0.15f);
    }
    // the lamps' glows at dusk, the rooms' and the pool's light on the site
    lights_.clear();
    {
        const float gl = lampK();
        for (size_t i = 0; i < prog_.size(); i++) {
            const as::Element& e = prog_[i];
            const bool shown = e.kind == as::CRANE ? els_[i].state == 3 : elementK(e) > 0.5f;
            if (!shown) continue;
            for (const Part& p : e.parts) {
                if (p.light == as::GARDEN_LAMP && gl > 0.01f) {
                    glow(p.c + vec3(0, 0.05f, 0), 0.5f, pal_.lampCol * (1.4f * gl), 0.3f);
                    if (lights_.size() < 20) lights_.push_back({p.c + vec3(0, 0.3f, 0), 3.f, pal_.lampCol * (0.3f * gl)});
                } else if (p.mat == as::GLASS && p.light >= 0) {
                    const float k = lightK(p.light);
                    if (k < 0.01f) continue;
                    // the room's light out through the glass onto the deck and the lawn
                    vec3 out(0, 0, 1.f);
                    if (p.s.x < p.s.z) out = vec3(1.f, 0, 0);
                    const vec3 c = p.c + out * 2.2f - vec3(0, p.s.y * 0.25f, 0);
                    if (lights_.size() < 20) lights_.push_back({c, 8.f + 0.25f * std::max(p.s.x, p.s.z), vec3(1.f, 0.68f, 0.38f) * (0.4f * k)});
                }
            }
        }
        const float pl = poolLightK();
        if (pl > 0.01f) {
            const vec3 wg = lin(V.waterGlow);
            for (int k = 0; k < 3; k++)
                lights_.push_back({vec3(lerpf(as::POOL[0], as::POOL[2], (k + 0.5f) / 3.f), 0.2f, (as::POOL[1] + as::POOL[3]) * 0.5f), 7.f, wg * (0.7f * pl)});
        }
        if (flood_ > 0.01f) {
            lights_.push_back({vec3(-26.f, 9.f, 2.f), 40.f, vec3(1.f, 0.9f, 0.75f) * (1.6f * flood_)});
            lights_.push_back({vec3(22.f, 9.f, 0.f), 40.f, vec3(1.f, 0.9f, 0.75f) * (1.6f * flood_)});
        }
    }

    // ---- Shadows: the sun over the site.
    const vec3 L = sky.sunDir.y > 0.02f ? sky.sunDir : normalize(vec3(sky.sunDir.x, 0.02f, sky.sunDir.z));
    {
        const vec3 center(-2.f, 6.f, -4.f);
        const mat4 lv = mat4::lookAt(center + L * 150.f, center, std::fabs(L.y) > 0.98f ? vec3(0, 0, 1) : vec3(0, 1, 0));
        const float xs[2] = {-48.f, 44.f}, ys[2] = {-2.f, 34.f}, zs[2] = {-34.f, 22.f};
        float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++)
                for (int c = 0; c < 2; c++) {
                    const vec4 p = lv * vec4(xs[a], ys[b], zs[c], 1.f);
                    const float cc[3] = {p.x, p.y, p.z};
                    for (int i = 0; i < 3; i++) { mn[i] = std::min(mn[i], cc[i]); mx[i] = std::max(mx[i], cc[i]); }
                }
        lightVp_ = mat4::ortho(mn[0], mx[0], mn[1], mx[1], -mx[2] - 60.f, -mn[2] + 10.f) * lv;
    }
    for (int m = 0; m < as::MESH_COUNT; m++) {
        glBindBuffer(GL_ARRAY_BUFFER, instVbo_[m]);
        glBufferData(GL_ARRAY_BUFFER, inst_[m].size() * sizeof(Inst), inst_[m].data(), GL_STREAM_DRAW);
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, 4096, 4096);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 2.f);
    glUseProgram(progShadow_);
    setMat(progShadow_, "uVP", lightVp_);
    setMat(progShadow_, "uView", view_);
    set1f(progShadow_, "uWind", worldTime_);
    drawInstances(progShadow_, true);
    glDisable(GL_POLYGON_OFFSET_FILL);

    const float seedU = (float)(seed_ % 1000) * 0.137f;
    auto common = [&](GLuint p) {
        set1f(p, "uSeed", seedU);
        set1f(p, "uTime", worldTime_);
        set3f(p, "uSunDir", sky.sunDir);
        set3f(p, "uSunCol", sky.sunCol * (1.f + 0.08f * swell_));
        set3f(p, "uSkyTop", sky.top);
        set3f(p, "uSkyHor", sky.hor);
        set3f(p, "uSkyLow", sky.low);
        set3f(p, "uFogCol", fogCol);
        set3f(p, "uAmbSky", ambSky);
        set3f(p, "uAmbGround", ambGround);
        set3f(p, "uEye", eye_);
        ugl::set2f(p, "uInvRes", 1.f / width_, 1.f / height_);
        set1f(p, "uFogDist", fogDist);
        set1f(p, "uNight", night_);
        set1f(p, "uSea", V.sea ? 1.f : 0.f);
        set1f(p, "uSeaZ", -230.f);
        set3f(p, "uSeaCol", lin(V.seaCol) * (0.4f + 0.6f * saturate(sky.sunCol.x / 2.5f)));
        setMat(p, "uLightVP", lightVp_);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        set1i(p, "uShadow", 1);
        set1f(p, "uSoft", 2.6f / 4096.f);
        const int n = (int)std::min<size_t>(lights_.size(), 24);
        set1i(p, "uNumLights", n);
        if (n > 0) {
            float pos[24 * 4], col[24 * 3];
            for (int i = 0; i < n; i++) {
                pos[i * 4] = lights_[i].p.x; pos[i * 4 + 1] = lights_[i].p.y; pos[i * 4 + 2] = lights_[i].p.z; pos[i * 4 + 3] = lights_[i].r;
                col[i * 3] = lights_[i].col.x; col[i * 3 + 1] = lights_[i].col.y; col[i * 3 + 2] = lights_[i].col.z;
            }
            glUniform4fv(ugl::U(p, "uLightPos"), n, pos);
            glUniform3fv(ugl::U(p, "uLightCol"), n, col);
        }
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, aoT_[0].tex);
        set1i(p, "uAO", 3);
        glActiveTexture(GL_TEXTURE0);
    };
    // the pool's excavation: marked out, dug down with sloped earth sides (eased), the margin backfilled round the shell
    const float pitD = (as::POOL_DEPTH + 0.2f) * smoother((p_ - as::L_POOL) / 2.5f);
    const float pitM = 1.5f * (1.f - smoother((p_ - (as::L_POOL + 3.8f)) / 0.8f));
    const float shell = p_ >= as::L_POOL + 3.5f ? 1.f : 0.f; // (the shell element rises from L_POOL + 2.5 over a line)
    const float pitMark = smoother((p_ - (as::L_POOL - 1.5f)) / 0.6f) * (1.f - smoother((p_ - (as::L_POOL + 0.4f)) / 0.6f));
    const float level = smoother((p_ - as::L_LEVEL) / 1.8f);
    const float lawn = smoother((p_ - as::L_GARDEN) / 4.f);

    // ---- The prepass (half resolution) and the occlusion.
    glBindFramebuffer(GL_FRAMEBUFFER, preFbo_);
    glViewport(0, 0, aoW_, aoH_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(progPreGround_);
    setMat(progPreGround_, "uInvVP", invVp_);
    setMat(progPreGround_, "uView", view_);
    setMat(progPreGround_, "uVP", vp_);
    ugl::set4f(progPreGround_, "uPool", as::POOL[0], as::POOL[1], as::POOL[2], as::POOL[3]);
    set1f(progPreGround_, "uPitD", pitD);
    set1f(progPreGround_, "uPitM", pitM);
    set1f(progPreGround_, "uShell", shell);
    ugl::drawFullscreen();
    glUseProgram(progPre_);
    setMat(progPre_, "uVP", vp_);
    setMat(progPre_, "uView", view_);
    set1f(progPre_, "uWind", worldTime_);
    drawInstances(progPre_, false);
    glDisable(GL_DEPTH_TEST);
    aoT_[0].bind();
    glUseProgram(progSsao_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prePos_);
    set1i(progSsao_, "uPos", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, preNrm_);
    set1i(progSsao_, "uNrm", 1);
    setMat(progSsao_, "uProj", proj_);
    set1f(progSsao_, "uRadius", 1.1f);
    set1f(progSsao_, "uStrength", 1.15f);
    ugl::drawFullscreen();
    for (int pass = 0; pass < 2; pass++) {
        aoT_[1 - pass].bind();
        glUseProgram(progBlur_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, aoT_[pass].tex);
        set1i(progBlur_, "uAO", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, prePos_);
        set1i(progBlur_, "uPos", 1);
        ugl::set2f(progBlur_, "uDir", pass == 0 ? 1.f / aoW_ : 0.f, pass == 0 ? 0.f : 1.f / aoH_);
        ugl::drawFullscreen();
    }
    // (the result is in aoT_[0]; the main pass reads it at half resolution)
    glBindTexture(GL_TEXTURE_2D, aoT_[0].tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // ---- The world (multisampled): the sky and the ground, the solids, the glows.
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(progBg_);
    common(progBg_);
    setMat(progBg_, "uInvVP", invVp_);
    setMat(progBg_, "uVP", vp_);
    set3f(progBg_, "uGrassA", lin(V.grassA));
    set3f(progBg_, "uGrassB", lin(V.grassB));
    set3f(progBg_, "uDry", lin(V.dry));
    set3f(progBg_, "uSoil", lin(V.soil));
    set3f(progBg_, "uLawn", lin(V.lawn));
    set3f(progBg_, "uHill", lin(V.hill));
    set3f(progBg_, "uHill2", lin(V.hill2));
    ugl::set4f(progBg_, "uSite", as::SITE[0], as::SITE[1], as::SITE[2], as::SITE[3]);
    ugl::set4f(progBg_, "uPool", as::POOL[0], as::POOL[1], as::POOL[2], as::POOL[3]);
    set1f(progBg_, "uLevel", level);
    set1f(progBg_, "uFlowers", V.flora == 3 ? 0.f : 1.f);
    set1f(progBg_, "uLawnK", lawn);
    set1f(progBg_, "uPitD", pitD);
    set1f(progBg_, "uPitM", pitM);
    set1f(progBg_, "uShell", shell);
    set1f(progBg_, "uPitMark", pitMark);
    set1f(progBg_, "uHillK", V.hillK);
    set1f(progBg_, "uCloudK", V.cloudK * (1.f - 0.4f * night_));
    set1f(progBg_, "uStarK", night_);
    {
        float fp[8 * 4];
        for (int i = 0; i < as::FOOTPRINT_COUNT; i++)
            for (int k = 0; k < 4; k++) fp[i * 4 + k] = as::FOOTPRINTS[i][k];
        glUniform4fv(ugl::U(progBg_, "uFoot"), as::FOOTPRINT_COUNT, fp);
        // (the footprints keep the lawn off only once their buildings stand: before, the site is soil anyway)
        set1i(progBg_, "uNumFoot", as::FOOTPRINT_COUNT);
    }
    ugl::drawFullscreen();
    glDepthFunc(GL_LEQUAL);
    glUseProgram(progInst_);
    common(progInst_);
    setMat(progInst_, "uVP", vp_);
    setMat(progInst_, "uView", view_);
    set1f(progInst_, "uWind", worldTime_);
    set3f(progInst_, "uRoomDay", ambSky * 0.9f + sky.sunCol * 0.05f);
    set3f(progInst_, "uRoomNight", vec3(1.f, 0.62f, 0.32f) * 1.6f);
    set3f(progInst_, "uWater", lin(V.water));
    set3f(progInst_, "uWaterDeep", lin(V.waterDeep));
    set3f(progInst_, "uWaterGlow", lin(V.waterGlow) * 0.9f);
    set1f(progInst_, "uPoolLight", poolLightK());
    drawInstances(progInst_, false);
    if (!glows_.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthMask(GL_FALSE);
        glUseProgram(progGlow_);
        setMat(progGlow_, "uVP", vp_);
        const vec3 fwd = normalize(vec3(-2.f, 3.2f, -1.f) - eye_);
        const vec3 right = normalize(cross(fwd, vec3(0, 1, 0))), up = cross(right, fwd);
        set3f(progGlow_, "uRight", right);
        set3f(progGlow_, "uUp", up);
        glBindVertexArray(glowVao_);
        glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
        glBufferData(GL_ARRAY_BUFFER, glows_.size() * sizeof(float), glows_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)(glows_.size() / 8));
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    // ---- The well, through the layout's camera.
    buildBoard(game);
    if (!bare_) {
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
        glUseProgram(progFlat_);
        setMat(progFlat_, "uVP", bvp_);
        glBindVertexArray(flatVao_);
        glBindBuffer(GL_ARRAY_BUFFER, flatVbo_);
        glBufferData(GL_ARRAY_BUFFER, flat_.size() * sizeof(float), flat_.data(), GL_STREAM_DRAW);
        // the backing writes its depth (the cleared panels slide back behind it); the rest only blends
        glDepthMask(GL_TRUE);
        glDrawArrays(GL_TRIANGLES, 0, 12);
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_TRIANGLES, 12, (GLsizei)(flat_.size() / 11) - 12);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        if (!blocks_.empty()) {
            glUseProgram(progBlock_);
            setMat(progBlock_, "uVP", bvp_);
            set1i(progBlock_, "uMesh", 0);
            set3f(progBlock_, "uKeyDir", normalize(vec3(-0.45f, 0.62f, 0.64f)));
            const vec3 key = lerp(vec3(1.f, 0.97f, 0.92f), vec3(1.f, 0.86f, 0.72f), smoothstepf(0.7f, 0.95f, tod));
            set3f(progBlock_, "uKeyCol", key * 0.95f);
            set3f(progBlock_, "uAmbTop", vec3(0.5f, 0.53f, 0.58f));
            set3f(progBlock_, "uAmbBot", vec3(0.24f, 0.24f, 0.25f));
            set3f(progBlock_, "uEyeB", beye_);
            set3f(progBlock_, "uRimCol", vec3(0.5f));
            set1f(progBlock_, "uDull", 1.f);
            glBindBuffer(GL_ARRAY_BUFFER, blockVbo_);
            glBufferData(GL_ARRAY_BUFFER, blocks_.size() * sizeof(Inst), blocks_.data(), GL_STREAM_DRAW);
            glBindVertexArray(blockVao_);
            glDrawArraysInstanced(GL_TRIANGLES, 0, meshCount_[as::BOX], (GLsizei)blocks_.size());
        }
    }
    glDisable(GL_DEPTH_TEST);

    // ---- Resolve, then the frame.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    set1f(progFinal_, "uExposure", lerpf(0.82f, 1.4f, night_) * (1.f + 0.06f * swell_));
    set1f(progFinal_, "uSlow", slow_ * 0.5f);
    set3f(progFinal_, "uSlowTint", vec3(0.82f, 0.9f, 1.08f));
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
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
        if (++n == 120) {
            std::fprintf(stderr, "[architect] gpu %.2f ms/frame (progress %.1f)\n", acc / n, p_);
            acc = 0.0;
            n = 0;
        }
    }
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_architect() {
    static const UniverseEntry e{
        "ARCHITECT", "a modern villa built over the song by two tower cranes, from an empty field to its lights at dusk",
        [] { return std::unique_ptr<Universe>(new ArchitectUniverse()); },
        // Calm, warm, steady songs suit the slow construction best; it fits most songs.
        [](const SongLook& l) { return 0.85f + 0.2f * (1.f - l.dynamics) + 0.1f * l.brightness; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
