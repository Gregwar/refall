// ISLANDS: one continuous flight over an endless tropical archipelago, behind the well, like drone footage.
//
// The world (world.cpp) is procedural and endless: a route steered through the sea, hero islands placed along it
// (a white villa with its pool, its deck and its jetty, little sandy cays with a few palms, atolls with their
// lagoon and their reef, a rocky islet with its lighthouse, a village with its bungalows on stilts, big green
// volcanic islands with cliffs and a waterfall, a gap between two of them), filler islands all around for the
// vistas, each island one terrain mesh at its level of detail (built on a worker thread) and its instanced content.
// Life: sailboats, catamarans, speedboats and kayaks with their wakes, a seaplane, flocks of seabirds, clouds.
//
// The camera flies it like a drone: one continuous flight along the route, its altitude, speed, heading, pitch
// and bank each on a critically damped spring (smooth to the second order), its altitude in log space (a climb
// to the clouds and a dive back to the waves are each one smooth move). It turns its view to keep the island it
// passes in the band beside the well, banks into the route's turns, and follows the terrain and what stands on it
// ahead (it rises over a hill, a palm, a roof well before it). The music: calm parts are high, wide, slow shots
// (and now and then a climb above the clouds, the sea curving away); mid is cruising, dipping low past the islands;
// a peak skims the waves a few meters up; a build climbs; the drop is one big dive from up there down to the waves.
// The day passes over the song: midday, the afternoon, the golden hour, the sunset, dusk (the villas light up).
// Before a song the drone's camera tilts down from the sky onto the sea and its islands; after it the camera tilts
// back up into the sky.
// Every cleared row releases a flock of seabirds beside the well (at dusk, it lights lanterns along the shore too).
//
// The look is flattened low poly: the terrain a jittered grid of flat facets (one material and one flat color each),
// faceted trees and rocks, the sea in flat bands of color by its depth (a foam line at the shore, the turquoise
// shallows, a teal ledge, the deep blue) with a few white angular glints and crisp white wakes, a plain gradient
// sky, faceted clouds.
// The pieces are low poly cubes: one chamfered cube per cell in its piece's color, seen in perspective, flat shaded by
// the scene's sun, its corners nudged by its seed, running on into the cells of the same piece (one block a piece).
#include "universes/islands/islands.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>

#include "layout.hpp"
#include "universes/islands/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Outfit (Medium, Light) and Playfair Display Italic, SIL Open Font License, see OFL-Outfit.txt, OFL-PlayfairDisplay.txt.
const unsigned char FONT_MED[] = {
#include "universes/islands/Outfit-Medium.ttf.inc"
};
const unsigned char FONT_LIGHT[] = {
#include "universes/islands/Outfit-Light.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/islands/PlayfairDisplay-Italic.ttf.inc"
};

namespace sh = islands_shaders;
using isl::Q;
using isl::qAxis;
using isl::qMul;
using isl::qRot;
using isl::qYaw;
using ugl::set1f;
using ugl::set1i;
using ugl::set3f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
vec3 mixDisplay(const vec3& a, const vec3& b, float t) { return ugl::toLinear(lerp(ugl::toSrgb(a), ugl::toSrgb(b), t)); }
float wrapPi(float a) {
    while (a > PI) a -= TAU;
    while (a < -PI) a += TAU;
    return a;
}

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
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[11] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
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
    if (std::fabs(det) < 1e-20f) return mat4::identity();
    det = 1.f / det;
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] * det;
    return r;
}

// ---------------------------------------------------------------- the variants
struct VariantDef {
    const char* name;
    // the land and the seabed
    unsigned sand, sandWet, grass, jungle, rock, cliff, seabed, reefA, reefB, dry;
    float dryK;
    // the water: its scattering in the shallows and the deep, its absorption (per meter), the foam
    unsigned shallow, deep;
    float sigma[3], scatterK, wave;
    // the flora
    unsigned leaf, leaf2, trunk, fol1, fol2;
    float cloudP;
    int hours;                 // 0: midday to dusk, 1: the golden hour to nightfall
    // the well: its backing, the seven pieces (I O T S Z J L), the accent
    unsigned backing;
    unsigned piece[7];
    unsigned accent;
};
const VariantDef VARIANTS[] = {
    // white sand cays and villas over turquoise shallows, deep blue offshore
    {"CARIBBEAN", 0xf2dfa8, 0xd8c08a, 0x6cc040, 0x3e9a34, 0x7a5e48, 0x5a4234, 0xe6dcc0, 0xc4806c, 0x6e9048, 0xa4a052, 0.12f,
     0x3ccad2, 0x2366b6, {0.85f, 0.16f, 0.1f}, 9.f, 1.f,
     0x4f9a2e, 0xc2b848, 0x8c7658, 0x3e8c30, 0x2a6a28, 0.42f, 0,
     0x0a1e32, {0x22d0d6, 0xf7c332, 0xa760e8, 0x3fcf6e, 0xf0475e, 0x5c90ff, 0xff8c2e}, 0xff7a5a},
    // jagged jungle peaks in emerald lagoons, overwater bungalows of thatch
    {"POLYNESIA", 0xf0dca8, 0xd4bc88, 0x58b838, 0x2a8a30, 0x6a5444, 0x4e3a2e, 0xe4dcc2, 0xcc8670, 0x80689c, 0x8aa648, 0.05f,
     0x36d2c2, 0x1d5cac, {0.8f, 0.13f, 0.09f}, 10.f, 1.1f,
     0x3e902a, 0xb4c040, 0x806c50, 0x2e8228, 0x1c5a22, 0.5f, 0,
     0x081e2a, {0x2ae6c4, 0xffd23c, 0xd25ad8, 0x6ad13c, 0xff5646, 0x5288fa, 0xff9a36}, 0xff6a4a},
    // white villages, olive trees and cypresses on pale limestone, a deep blue sea
    {"MEDITERRANEAN", 0xecdcb0, 0xccb48a, 0x9aac46, 0x5a8034, 0xb8946c, 0x86624a, 0xdcd6c4, 0x8c8c62, 0x5c7046, 0xc8a456, 0.45f,
     0x34b2da, 0x1c56ae, {0.75f, 0.12f, 0.075f}, 7.f, 0.9f,
     0x6a8a3a, 0xa8a050, 0x7a6450, 0x6a7a44, 0x3c4e2a, 0.3f, 0,
     0x0a1836, {0x2ec8ec, 0xf4cc46, 0x9070e4, 0x5cc04c, 0xec5044, 0x4c86f6, 0xf28c3a}, 0x2a5ae0},
    // the whole flight in the golden hour, into the sunset and the night, the lanterns lit along the shores
    {"SUNSET", 0xf0d8a8, 0xd0b084, 0x66b23c, 0x348a30, 0x76604c, 0x58443a, 0xe6d8c0, 0xc47e6a, 0x6e844a, 0xa4a052, 0.1f,
     0x34c0ca, 0x1f5eac, {0.85f, 0.16f, 0.1f}, 9.f, 0.85f,
     0x4c922e, 0xc8b048, 0x86704e, 0x3a8030, 0x26622a, 0.45f, 1,
     0x121432, {0x2cd8e2, 0xffd84a, 0xb86cf2, 0x48d87c, 0xff4f6e, 0x6e9cff, 0xff9a3c}, 0xffae4a},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

isl::Style styleFor(int v) {
    isl::Style s{};
    using namespace isl;
    const float base[6] = {0.26f, 0.26f, 0.16f, 0.14f, 0.09f, 0.09f};
    for (int i = 0; i < 6; i++) s.w[i] = base[i];
    const int bagCar[8] = {T_VILLA, T_CAY, 100 + T_ISLET, T_VOLCANO, T_ATOLL, 200, T_VILLAGE, 100 + T_VILLA};
    const int bagPoly[8] = {T_ATOLL, T_VILLAGE, 100 + T_CAY, T_VOLCANO, 200, T_VILLA, 100 + T_ATOLL, T_ISLET};
    const int bagMed[8] = {T_VILLA, T_ISLET, 100 + T_VILLAGE, T_VOLCANO, 200, T_VILLAGE, 100 + T_ISLET, T_VILLA};
    const int* bag = v == 1 ? bagPoly : v == 2 ? bagMed : bagCar;
    for (int i = 0; i < 8; i++) s.heroBag[i] = bag[i];
    s.fillP = 0.72f;
    s.dry = v == 2;
    s.thatch = v == 1;
    s.villageWalls[0] = lin(0xf2c6c0);
    s.villageWalls[1] = lin(0xf4dc90);
    s.villageWalls[2] = lin(0xa8dcc8);
    s.villageWalls[3] = lin(0x9cc4ec);
    s.roof = lin(0xb84a34);
    s.hull[0] = lin(0xf6f4ee);
    s.hull[1] = lin(0x1c2c4c);
    s.hull[2] = lin(0xb03028);
    s.sailCol = lin(0xf6f2e8);
    s.canvas[0] = lin(0xe8483c);
    s.canvas[1] = lin(0x2a8ac8);
    s.canvas[2] = lin(0xf2b830);
    s.accentWood = lin(0xa87650);
    if (v == 1) { // Polynesia: volcanoes and atolls, few cays
        const float w[6] = {0.3f, 0.12f, 0.3f, 0.08f, 0.12f, 0.08f};
        for (int i = 0; i < 6; i++) s.w[i] = w[i];
        s.accentWood = lin(0x9a6a44);
    }
    if (v == 2) { // the Mediterranean: rocky islands, villages, no atolls
        const float w[6] = {0.42f, 0.06f, 0.f, 0.26f, 0.16f, 0.1f};
        for (int i = 0; i < 6; i++) s.w[i] = w[i];
        s.fillP = 0.66f;
        s.canvas[0] = lin(0x1e50c0);
        s.canvas[1] = lin(0xf4f2ee);
        s.canvas[2] = lin(0x2a8ac8);
        s.accentWood = lin(0x9c7a5a);
    }
    return s;
}

// ---------------------------------------------------------------- the hours: the day passes over the song
struct Hour {
    float t, elev;              // song progress, the sun's elevation (degrees)
    unsigned sun;
    float sunK;
    unsigned top, hor, low, fog;
    float haze;                 // per meter at sea level
};
const Hour DAY[] = {
    {0.00f, 64.f, 0xfff6e8, 2.15f, 0x1e6ed6, 0x9ecff0, 0x8ec4e2, 0xb0d8f0, 1.f / 16000.f},
    {0.42f, 46.f, 0xfff2dc, 2.15f, 0x2672d4, 0xa8d2ee, 0x94c6e2, 0xb6d8ee, 1.f / 15000.f},
    {0.66f, 24.f, 0xffe2b4, 2.1f, 0x3474c8, 0xcadcde, 0xa8c4d0, 0xd2dad8, 1.f / 13000.f},
    {0.80f, 9.f, 0xffb46c, 2.1f, 0x45689e, 0xf2c48e, 0xc8b0a0, 0xe8c09c, 1.f / 11000.f},
    {0.90f, 2.5f, 0xff8448, 1.9f, 0x2e4486, 0xfa9a5c, 0xa47a7c, 0xe0946e, 1.f / 9000.f},
    {0.955f, -1.f, 0xff6638, 1.2f, 0x1e2c6c, 0xf27848, 0x7a5466, 0xb46a5e, 1.f / 9000.f},
    {1.00f, -5.f, 0xc04a40, 0.4f, 0x0e1844, 0x8a4a5c, 0x2a2a4c, 0x3e3456, 1.f / 9000.f},
};
const Hour EVENING[] = {
    {0.00f, 17.f, 0xffd298, 2.2f, 0x2a66c4, 0xf0c27c, 0xa4b0b8, 0xc8b49a, 1.f / 18000.f},
    {0.34f, 7.f, 0xffa860, 2.1f, 0x30559e, 0xf6a866, 0xa89090, 0xd4a084, 1.f / 15000.f},
    {0.60f, 1.5f, 0xff7a42, 1.9f, 0x2c3e82, 0xfa8a56, 0x9a7078, 0xdc8a6e, 1.f / 9500.f},
    {0.74f, -1.f, 0xff6236, 1.1f, 0x1c2a68, 0xee7046, 0x744e64, 0xae6458, 1.f / 9000.f},
    {0.87f, -4.f, 0xc04a40, 0.45f, 0x10184a, 0x7e4660, 0x2c2a4e, 0x3c3458, 1.f / 9000.f},
    {1.00f, -9.f, 0x704060, 0.12f, 0x060c2c, 0x262850, 0x121632, 0x1c2040, 1.f / 9000.f},
};

// Test hooks: REFALL_ISLANDS_TOD=T holds the hour (0 midday .. 1 dusk), REFALL_ISLANDS_ALT=A holds the altitude (m),
// REFALL_ISLANDS_S=S starts the flight S meters along the route.
float hookTod() {
    static const float v = std::getenv("REFALL_ISLANDS_TOD") ? (float)std::atof(std::getenv("REFALL_ISLANDS_TOD")) : -1.f;
    return v;
}
float hookAlt() {
    static const float v = std::getenv("REFALL_ISLANDS_ALT") ? (float)std::atof(std::getenv("REFALL_ISLANDS_ALT")) : -1.f;
    return v;
}
float hookS() {
    static const float v = std::getenv("REFALL_ISLANDS_S") ? (float)std::atof(std::getenv("REFALL_ISLANDS_S")) : 0.f;
    return v;
}

constexpr float BACK_Z = -0.3f;

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool IslandsUniverse::init(int width, int height) {
    using sh::with;
    const std::string common = std::string(sh::COMMON) + sh::SHADOW_GLSL;
    progSky_ = ugl::program(ugl::FULLSCREEN_VS, with(sh::SKY_FS, sh::COMMON).c_str(), "islands sky");
    progTerrain_ = ugl::program(sh::TERRAIN_VS, with(sh::TERRAIN_FS, common.c_str()).c_str(), "islands terrain");
    progObj_ = ugl::program(sh::OBJ_VS, with(sh::OBJ_FS, common.c_str()).c_str(), "islands objects");
    progWater_ = ugl::program(ugl::FULLSCREEN_VS, with(sh::WATER_FS, sh::COMMON).c_str(), "islands water");
    progCloud_ = ugl::program(sh::CLOUD_VS, with(sh::CLOUD_FS, sh::COMMON).c_str(), "islands clouds");
    progGlow_ = ugl::program(sh::GLOW_VS, sh::GLOW_FS, "islands glow");
    progFlat_ = ugl::program(sh::FLAT_VS, sh::FLAT_FS, "islands flat");
    progGlass_ = ugl::program(sh::GLASS_VS, sh::GLASS_FS, "islands glass");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, sh::FINAL_FS, "islands final");
    progShape_ = ugl::program(sh::SHAPE_VS, sh::SHAPE_FS, "islands shape");

    // ---- the object meshes (pos, normal, aux) and their instance buffers
    for (int m = 0; m < isl::M_COUNT; m++) {
        const std::vector<float> v = isl::buildMesh(m);
        glGenVertexArrays(1, &meshVao_[m]);
        glGenBuffers(1, &meshVbo_[m]);
        glGenBuffers(1, &instVbo_[m]);
        glBindVertexArray(meshVao_[m]);
        glBindBuffer(GL_ARRAY_BUFFER, meshVbo_[m]);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 3));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 6));
        glBindBuffer(GL_ARRAY_BUFFER, instVbo_[m]);
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(3 + i);
            glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(Inst), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(3 + i, 1);
        }
        meshCount_[m] = (int)(v.size() / 8);
    }
    // ---- the terrain grids' indices, one per level of detail
    for (int l = 0; l < isl::LOD_COUNT; l++) {
        const int n = isl::LOD_N[l];
        std::vector<unsigned> idx;
        idx.reserve((size_t)(n - 1) * (n - 1) * 6);
        for (int j = 0; j + 1 < n; j++)
            for (int i = 0; i + 1 < n; i++) {
                const unsigned a = (unsigned)(j * n + i), b = a + 1, c = a + (unsigned)n, d = c + 1;
                idx.insert(idx.end(), {a, c, b, b, c, d});
            }
        glGenBuffers(1, &idx_[l]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx_[l]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
        idxCount_[l] = (int)idx.size();
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // ---- sprites: glows, cloud puffs (one quad, instanced)
    const float quadV[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadV), quadV, GL_STATIC_DRAW);
    auto sprite = [&](GLuint& vao, GLuint& vbo, int vec4s) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < vec4s; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4 * vec4s, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    sprite(glowVao_, glowVbo_, 2);
    sprite(cloudVao_, cloudVbo_, 2);
    // ---- the pieces' cubes: one chamfered cube, instanced. Each vertex its corner's signs and the axes the chamfer
    // pulls it in along: every corner of the box gives three (one on each face meeting there); the faces, the edges'
    // chamfers and the corners' triangles join them.
    {
        std::vector<float> cv;
        auto V = [&](int sx, int sy, int sz, int axis) {
            const float v[6] = {(float)sx, (float)sy, (float)sz, axis == 0 ? 0.f : 1.f, axis == 1 ? 0.f : 1.f, axis == 2 ? 0.f : 1.f};
            cv.insert(cv.end(), v, v + 6);
        };
        auto corner = [](int a, int sa, int b, int sb, int c, int sc, int* s) { s[a] = sa; s[b] = sb; s[c] = sc; };
        int q[4][3];
        for (int a = 0; a < 3; a++) {
            const int b = (a + 1) % 3, c = (a + 2) % 3;
            for (int sa = -1; sa <= 1; sa += 2) {
                // the face
                const int uv[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
                for (int k = 0; k < 4; k++) corner(a, sa, b, uv[k][0], c, uv[k][1], q[k]);
                for (int k : {0, 1, 2, 0, 2, 3}) V(q[k][0], q[k][1], q[k][2], a);
                // the chamfer along the edge with the next axis' faces (along the third axis)
                for (int sb = -1; sb <= 1; sb += 2) {
                    int e0[3], e1[3];
                    corner(a, sa, b, sb, c, -1, e0);
                    corner(a, sa, b, sb, c, 1, e1);
                    V(e0[0], e0[1], e0[2], a); V(e1[0], e1[1], e1[2], a); V(e1[0], e1[1], e1[2], b);
                    V(e0[0], e0[1], e0[2], a); V(e1[0], e1[1], e1[2], b); V(e0[0], e0[1], e0[2], b);
                }
            }
        }
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                    for (int a = 0; a < 3; a++) V(sx, sy, sz, a);
        cubeCount_ = (int)(cv.size() / 6);
        glGenVertexArrays(1, &glassVao_);
        glGenBuffers(1, &cubeVbo_);
        glGenBuffers(1, &glassVbo_);
        glBindVertexArray(glassVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
        glBufferData(GL_ARRAY_BUFFER, cv.size() * sizeof(float), cv.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
        glBindBuffer(GL_ARRAY_BUFFER, glassVbo_);
        for (int i = 0; i < 3; i++) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 12, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(2 + i, 1);
        }
    }

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
    glBindVertexArray(0);

    // ---- the sun's shadow map
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenTextures(1, &shadowDummy_);
    glBindTexture(GL_TEXTURE_2D, shadowDummy_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &shadowFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLint maxS = 4;
    glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    samples_ = std::max(1, std::min(4, (int)maxS));

    world_.reset(new isl::World());
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void IslandsUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera ref = layout::camera(width_, height_);
    bvp_ = ref.vp;
    beye_ = ref.eye;
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
    halfW_ = std::max(1, width_ / 2);
    halfH_ = std::max(1, height_ / 2);
    seabedT_.create(halfW_, halfH_, GL_RGBA32F, GL_RGBA, GL_FLOAT, false);
    // the half-resolution pass's depth buffer
    if (halfDepth_) glDeleteRenderbuffers(1, &halfDepth_);
    glGenRenderbuffers(1, &halfDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, halfDepth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, halfW_, halfH_);
    glBindFramebuffer(GL_FRAMEBUFFER, seabedT_.fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, halfDepth_);
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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

IslandsUniverse::~IslandsUniverse() {
    world_.reset();
    std::vector<unsigned> vaos, vbos;
    isl::takeGraveyard(vaos, vbos);
    for (unsigned v : vaos) glDeleteVertexArrays(1, &v);
    for (unsigned b : vbos) glDeleteBuffers(1, &b);
    sceneT_.destroy();
    seabedT_.destroy();
    if (halfDepth_) glDeleteRenderbuffers(1, &halfDepth_);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
    if (shadowDummy_) glDeleteTextures(1, &shadowDummy_);
    for (GLuint p : {progSky_, progTerrain_, progObj_, progWater_, progCloud_, progGlow_, progFlat_, progGlass_, progFinal_, progShape_})
        glDeleteProgram(p);
    for (int m = 0; m < isl::M_COUNT; m++) {
        glDeleteVertexArrays(1, &meshVao_[m]);
        glDeleteBuffers(1, &meshVbo_[m]);
        glDeleteBuffers(1, &instVbo_[m]);
    }
    for (int l = 0; l < isl::LOD_COUNT; l++) glDeleteBuffers(1, &idx_[l]);
    for (GLuint v : {glowVao_, cloudVao_, glassVao_, flatVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, glowVbo_, cloudVbo_, glassVbo_, cubeVbo_, flatVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 IslandsUniverse::backdrop() const { return mixDisplay(sky_.hor, lin(VARIANTS[variant_].shallow), 0.35f); }
vec3 IslandsUniverse::pieceCol(int t) const { return lin(VARIANTS[variant_].piece[std::clamp(t, 0, 6)]); }

void IslandsUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (!t.hasSong) v = 0;
        else if ((t.minor && t.brightness < 0.48f) || t.bpm < 88.f) v = 3;          // sunset
        else if (t.bassWeight > 0.58f && t.density > 0.5f) v = 1;                  // polynesia
        else if (t.airWeight > 0.55f && t.brightness > 0.5f && !t.minor) v = 2;    // the mediterranean
        else if (t.brightness > 0.55f) v = 0;                                      // caribbean
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    const bool changed = v != variant_ || builtVariant_ < 0;
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    songLen_ = t.duration > 20.f ? t.duration : 200.f;
    const float remaining = t.remaining > 0.f ? t.remaining : songLen_;
    songT_ = std::max(0.f, songLen_ - remaining);
    if (seconds <= 0.f) todInit_ = false;
    if (changed && seconds > 0.f) builtVariant_ = -2; // rebuilt on the next frame (a new archipelago)
}

void IslandsUniverse::setLevel(int level, float energy, float seconds, float impact) {
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
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.f, 7.f);
}

// The drop: one big dive, from wherever the camera is down to the waves.
void IslandsUniverse::drop(float strength) {
    dropMode_ = true;
    diveT_ = 0.f;
    swell_ = std::max(swell_, 0.4f + 0.4f * strength);
}

void IslandsUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.15f * strength); }

vec2 IslandsUniverse::project(const vec3& p) const { return layout::project(bvp_, p, width_, height_); }

TransitionHints IslandsUniverse::hints() const {
    TransitionHints h;
    const VariantDef& V = VARIANTS[variant_];
    h.background = backdrop();
    h.light = ugl::luma(h.background) > 0.2f;
    h.ink = h.light ? lin(0x0e2238) : lin(0xf4efe4);
    h.accent = lin(V.accent);
    h.styles = TS_RIPPLE | TS_CURTAIN | TS_LUMA;
    return h;
}

// =====================================================================================================
// The world
// =====================================================================================================

void IslandsUniverse::buildWorld() {
    isl::Style st = styleFor(variant_);
    world_->reset(seed_, st);
    boats_.clear();
    flocks_.clear();
    lanterns_.clear();
    plane_ = Plane{};
    planeWait_ = 12.f;
    s_ = hookS();
    camInit_ = false;
    builtVariant_ = variant_;
    // the world near the start, at once (later islands stream in)
    world_->update(vec3(0, 60, 0), s_, 9000.f);
    fleet_.clear();
    pods_.clear();
    sparks_.clear();
    // Test hook: REFALL_ISLANDS_FLEET=N starts with a fleet of N boats (as if N rows had been cleared)
    if (const char* fl = std::getenv("REFALL_ISLANDS_FLEET")) {
        const int n = std::atoi(fl);
        for (int i = 0; i < n; i++) {
            std::vector<int> row;
            for (int k = 0; k < 10; k++) row.push_back(rng_.irange(0, 6));
            launchBoat(row, i % 12 == 11);
            Boat& b = fleet_.back();
            b.sb = s_ + rng_.range(-60.f, 1100.f);
            b.latN = (rng_.chance(0.5f) ? 1.f : -1.f) * rng_.range(60.f, 260.f);
            b.launchT = 99.f;
            b.unfurl = 1.f;
        }
        flocks_.clear();
    }
    if (std::getenv("REFALL_ISLANDS_DEBUG"))
        for (const isl::Hero& h : world_->heroes)
            if (h.sc < 12000.0)
                std::fprintf(stderr, "[islands] hero at s %.0f: type %d%s%s\n", h.sc, h.isl ? h.isl->type : -1, h.orbit ? " (orbit)" : "", h.gap ? " (gap)" : "");
}

void IslandsUniverse::uploadMeshes() {
    std::vector<std::shared_ptr<isl::Island>> ready;
    world_->collectReady(ready);
    for (auto& I : ready) {
        if (I->ready.empty()) continue;
        if (!I->vao) {
            glGenVertexArrays(1, &I->vao);
            glGenBuffers(1, &I->vbo);
        }
        glBindVertexArray(I->vao);
        glBindBuffer(GL_ARRAY_BUFFER, I->vbo);
        glBufferData(GL_ARRAY_BUFFER, I->ready.size() * sizeof(float), I->ready.data(), GL_STATIC_DRAW);
        const GLsizei st = sizeof(float) * isl::TERRAIN_FLOATS;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, st, (void*)(sizeof(float) * 3));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, st, (void*)(sizeof(float) * 6));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, st, (void*)(sizeof(float) * 10));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx_[I->readyLod]);
        glBindVertexArray(0);
        I->lod = I->readyLod;
        I->ready.clear();
        I->ready.shrink_to_fit();
    }
    std::vector<unsigned> vaos, vbos;
    isl::takeGraveyard(vaos, vbos);
    for (unsigned v : vaos) glDeleteVertexArrays(1, &v);
    for (unsigned b : vbos) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// The flight
// =====================================================================================================

// A critically damped spring toward the target (implicit, stable at any step).
void IslandsUniverse::Spring::step(float target, float omega, float dt) {
    if (dt <= 0.f) return;
    const float f = 1.f + 2.f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx;
    v = nv;
}

// The altitude the route ahead needs: over every hill, palm and roof the camera must pass, with a margin, and an
// easy climb toward it (it rises well before).
float IslandsUniverse::floorAhead(const vec3& from, float yaw, float v) const {
    (void)from; (void)yaw;
    float need = 3.2f;
    const float reach = std::max(500.f, v * 14.f);
    for (int i = 0; i <= 70; i++) {
        const float d = i < 40 ? i * 6.f : 240.f + (i - 40) * (reach - 240.f) / 30.f;
        const isl::RoutePt r = world_->route(s_ + d);
        const float h = world_->obstacle(r.x, r.z);
        if (h <= 0.3f) continue;
        need = std::max(need, h + 5.f - d * 0.26f);
    }
    return need;
}

void IslandsUniverse::fly(float dt, float wdt, const std::string& section) {
    (void)dt;
    const float wc = 1.f - smoothstepf(0.f, 1.f, lv_), wp = smoothstepf(1.f, 2.f, lv_);
    const float wm = std::max(0.f, 1.f - wc - wp);
    calmPhase_ += wdt * wc / 100.f;
    cruisePhase_ += wdt / 30.f;
    const isl::Hero* hero = world_->heroNear(s_, 1100.0, 200.0);

    // ---- the altitude wanted: calm wide (lower, and only now and then a climb above the clouds), mid cruising with
    // quick climbs and dives over the water, a peak skimming low
    const float cyc = 0.5f - 0.5f * std::cos(calmPhase_ * TAU);
    float aCalm = std::exp(lerpf(std::log(90.f), std::log(1700.f), smoothstepf(0.55f, 0.95f, cyc)) + 0.6f * std::sin(cruisePhase_ * TAU * 1.3f));
    float aMid = 20.f * std::exp(1.1f * std::sin(cruisePhase_ * TAU) + 0.5f * std::sin(cruisePhase_ * TAU * 2.7f + 1.f));
    float aPeak = 4.2f + 1.4f * std::sin(cruisePhase_ * TAU * 3.1f);
    if (hero && !hero->gap) {
        // a pass: down to the island, close over the water beside its beach, its deck, its palms (or an orbit)
        const float ds = (float)(hero->sc - s_);
        float k = smoothstepf(900.f, 260.f, ds) * smoothstepf(-260.f, -70.f, ds);
        if (hero->orbit) k = smoothstepf((float)(hero->s0), (float)(hero->s0 + 300.0), (float)s_) * smoothstepf((float)hero->s1, (float)(hero->s1 - 200.0), (float)s_);
        aMid = std::exp(lerpf(std::log(aMid), std::log(hero->orbit ? 28.f : 7.f), k * 0.92f));
        aCalm = std::exp(lerpf(std::log(aCalm), std::log(hero->orbit ? 90.f : 25.f), k * 0.8f));
    }
    float target = wc * std::log(aCalm) + wm * std::log(aMid) + wp * std::log(aPeak);
    float omega = 0.42f;
    buildK_ = approach(buildK_, section == "BUILD" ? 1.f : 0.f, section == "BUILD" ? 0.35f : 0.6f, wdt);
    target = lerpf(target, std::max(target, std::log(620.f)), buildK_);
    if (diveT_ < 9.f) {
        diveT_ += wdt;
        target = std::log(4.5f);
        omega = 0.52f;
    }
    if (hookAlt() > 0.f) target = std::log(hookAlt());
    // ahead: the terrain and what stands on it
    const float fl = floorAhead(eye_, yaw_, vel_);
    if (std::log(fl) > target) {
        target = std::log(fl);
        omega = std::max(omega, 0.75f);
    }
    if (!camInit_) {
        logAlt_.x = target;
        logAlt_.v = 0.f;
    }
    logAlt_.step(target, omega, wdt);
    alt_ = std::exp(logAlt_.x);
    // never into a hill
    const float here = world_->obstacle(eye_.x, eye_.z) + 2.2f;
    if (alt_ < here) {
        alt_ = here;
        logAlt_.x = std::log(alt_);
        logAlt_.v = std::max(logAlt_.v, 0.f);
    }

    // ---- the speed: from the altitude (the same sense of speed high and low) and the music
    float vT = (9.f + 3.f * std::sqrt(alt_)) * (0.62f + 0.42f * lv_ + 0.25f * energy_ + 0.2f * dropK_) * (hero && hero->orbit ? 0.85f : 1.f) * lerpf(1.f, 0.35f, slow_);
    {
        // never a fast turn: the speed is capped by the route's curvature a little ahead (a turn rate under ~7 deg/s)
        float k = 0.f;
        for (int i = 0; i <= 8; i++) k = std::max(k, std::fabs(world_->route(s_ + i * std::max(25.f, vel_ * 0.8f)).curv));
        vT = std::min(vT, 0.19f / std::max(k, 1e-5f));
    }
    if (!camInit_) { speed_.x = vT; speed_.v = 0.f; }
    speed_.step(vT, 0.6f, wdt);
    vel_ = std::max(0.f, speed_.x);
    s_ += (double)(vel_ * wdt);

    // ---- the position and the heading (the route a moment ahead)
    const isl::RoutePt rp = world_->route(s_);
    const isl::RoutePt ra = world_->route(s_ + std::max(30.f, vel_ * 1.6f));
    eye_ = vec3(rp.x, alt_, rp.z);
    const float travel = 0.5f * (rp.head + ra.head);

    // ---- the view: the island passed kept in the band beside the well
    float offT = 0.f, pitchF = 0.f, wF = 0.f;
    if (hero && !hero->gap && hero->isl) {
        const vec3 F = hero->isl->focus;
        const float dx = F.x - eye_.x, dz = F.z - eye_.z;
        const float hd = std::sqrt(dx * dx + dz * dz);
        const float rel = wrapPi(std::atan2(dz, dx) - travel);
        const float side = rel >= 0.f ? 1.f : -1.f;
        const float want = rel - side * 0.52f;
        wF = smoothstepf(2600.f, 1100.f, hd) * (1.f - smoothstepf(1.7f, 2.35f, std::fabs(rel)));
        offT = std::clamp(want, -1.05f, 1.05f) * wF;
        pitchF = std::atan2(F.y - alt_, std::max(hd, 1.f));
    }
    heroW_ = wF;
    // the target itself moves slowly (a pan never whips, even passing close by)
    if (!camInit_) offTgt_ = offT;
    offTgt_ += std::clamp(offT - offTgt_, -0.24f * wdt, 0.24f * wdt);
    if (!camInit_) { yawOff_.x = offTgt_; yawOff_.v = 0.f; }
    yawOff_.step(offTgt_, 0.75f, wdt);
    yaw_ = travel + yawOff_.x;
    // the pitch: near level skimming, looking down from high, the horizon kept in view above the clouds
    const float la = std::log(alt_);
    float p = -lerpf(0.06f, 0.5f, smoothstepf(std::log(10.f), std::log(1000.f), la));
    p = lerpf(p, -0.36f, smoothstepf(std::log(1300.f), std::log(2200.f), la));
    if (wF > 0.f) p = lerpf(p, std::clamp(pitchF * 0.5f + p * 0.5f, -1.f, 0.f), wF * smoothstepf(std::log(400.f), std::log(30.f), la) * 0.6f);
    if (!camInit_) { pitch_.x = p; pitch_.v = 0.f; }
    pitch_.step(p, 0.55f, wdt);
    // the bank: into the route's turns, a coordinated turn's angle, gentle
    const float bankT = std::clamp(std::atan(vel_ * vel_ * rp.curv / 9.81f) * 0.9f, -0.26f, 0.26f);
    if (!camInit_) { bank_.x = bankT; bank_.v = 0.f; }
    bank_.step(bankT, 1.f, wdt);
    camInit_ = true;
    static const bool dbg = std::getenv("REFALL_ISLANDS_DEBUG") != nullptr;
    static float dbgT = 0.f;
    dbgT += dt;
    if (dbg && dbgT > 1.f) {
        dbgT = 0.f;
        std::fprintf(stderr, "[islands] s %.0f alt %.1f (target %.1f, floor %.1f) v %.1f pitch %.2f yawOff %.2f lv %.2f sec %s dive %.1f hero %d islands %zu\n",
                     s_, alt_, std::exp(target), fl, vel_, pitch_.x, yawOff_.x, lv_, section.c_str(), diveT_, hero ? (hero->orbit ? 2 : 1) : 0,
                     world_->islands.size());
    }

    const float cp = std::cos(pitch_.x);
    fwd_ = normalize(vec3(std::cos(yaw_) * cp, std::sin(pitch_.x), std::sin(yaw_) * cp));
    const vec3 rightOfTravel(-std::sin(travel), 0.f, std::cos(travel));
    const vec3 upW = normalize(vec3(0, 1, 0) + rightOfTravel * std::tan(bank_.x));
    right_ = normalize(cross(fwd_, upW));
    up_ = cross(right_, fwd_);
    // Test hook: REFALL_ISLANDS_TRACE=FILE writes the camera of every frame (smoothness checks)
    static FILE* trace = std::getenv("REFALL_ISLANDS_TRACE") ? std::fopen(std::getenv("REFALL_ISLANDS_TRACE"), "w") : nullptr;
    if (trace && wdt > 0.f)
        std::fprintf(trace, "%.4f %.3f %.3f %.3f %.5f %.5f %.5f %.3f %.2f\n", wdt, eye_.x, eye_.y, eye_.z, yaw_, pitch_.x, bank_.x, vel_, lv_);
}

// =====================================================================================================
// Life: boats, the seaplane, birds, lanterns, clouds
// =====================================================================================================

void IslandsUniverse::spawnBoat(bool initial) {
    for (int tries = 0; tries < 16; tries++) {
        Boat b;
        const float r = rng_.uniform();
        vec3 p;
        bool shallow = rng_.chance(0.35f);
        if (shallow) {
            // near an island ahead: kayaks off its beach, a catamaran anchored in its shallows
            std::vector<std::shared_ptr<isl::Island>> near;
            for (const auto& I : world_->islands) {
                const float dx = I->x - eye_.x, dz = I->z - eye_.z;
                const float d = std::sqrt(dx * dx + dz * dz);
                if (d > 2600.f || d < 100.f) continue;
                if ((dx * fwd_.x + dz * fwd_.z) < 0.f && !initial) continue;
                if (I->type == isl::T_ISLET) continue;
                near.push_back(I);
            }
            if (near.empty()) continue;
            const auto& I = near[(size_t)(rng_.uniform() * near.size()) % near.size()];
            const float a = rng_.range(0.f, TAU);
            const float d = I->R * rng_.range(1.05f, I->type == isl::T_CAY ? 2.6f : 1.5f);
            p = vec3(I->x + std::cos(a) * d, 0.f, I->z + std::sin(a) * d);
            const float h = world_->terrain(p.x, p.z);
            if (h > -0.9f || h < -6.f) continue;
            b.kind = rng_.chance(0.55f) ? 3 : 1;
            b.moored = b.kind == 1 || rng_.chance(0.3f);
            b.head = a + PI * 0.5f + rng_.range(-0.4f, 0.4f);
        } else {
            const float d = initial ? rng_.range(150.f, 2600.f) : rng_.range(1300.f, 3000.f);
            const float a = yaw_ + rng_.range(-1.f, 1.f);
            p = vec3(eye_.x + std::cos(a) * d, 0.f, eye_.z + std::sin(a) * d);
            if (world_->terrain(p.x, p.z) > -3.f) continue;
            b.kind = r < 0.45f ? 0 : r < 0.62f ? 1 : 2;
            b.head = rng_.range(0.f, TAU);
        }
        if (!initial) {
            // never born inside the view unless far in the haze (and even then it fades in)
            const vec3 d(p.x - eye_.x, 0.f, p.z - eye_.z);
            const float dl = length(d);
            const vec3 fh = normalize(vec3(fwd_.x, 0, fwd_.z));
            if (dl < 2600.f && dot(d / std::max(dl, 1.f), fh) > 0.55f) continue;
            b.appear = 0.f;
        }
        b.scale = b.appear; // (born invisible: its first frame must not show it at full size)
        b.p = p;
        b.phase = rng_.range(0.f, 50.f);
        switch (b.kind) {
        case 0: b.len = rng_.range(9.f, 13.f); b.beam = b.len * 0.32f; b.cruise = rng_.range(3.f, 5.f); break;
        case 1: b.len = rng_.range(11.f, 14.f); b.beam = b.len * 0.5f; b.cruise = rng_.range(3.5f, 5.5f); break;
        case 2: b.len = rng_.range(7.f, 10.f); b.beam = b.len * 0.34f; b.cruise = rng_.range(11.f, 17.f); break;
        default: b.len = 4.f; b.beam = 0.65f; b.cruise = 1.2f; break;
        }
        if (b.moored) b.cruise = 0.f;
        b.speed = b.cruise;
        const isl::Style st = styleFor(variant_);
        const float c = rng_.uniform();
        if (b.kind == 3) {
            const unsigned kc[4] = {0xf2b21e, 0xe8402e, 0x2aa8d8, 0x44c060};
            b.col = lin(kc[rng_.irange(0, 3)]);
        } else b.col = c < 0.6f ? st.hull[0] : c < 0.85f ? st.hull[1] : st.hull[2];
        b.stripe = b.kind == 2 ? rng_.irange(1, 3) : (b.col.x > 0.5f ? rng_.irange(0, 2) : 0);
        boats_.push_back(b);
        return;
    }
}

void IslandsUniverse::updateBoats(float dt) {
    for (Boat& b : boats_) {
        b.phase += dt;
        b.age += dt;
        if (b.cruise > 0.f) {
            const vec3 d(std::cos(b.head), 0, std::sin(b.head));
            const float look = b.len * 2.f + b.speed * 8.f;
            const float ahead = world_->terrain(b.p.x + d.x * look, b.p.z + d.z * look);
            float tr = 0.f;
            if (ahead > -2.5f) {
                // land ahead: turn toward the deeper side
                const float l = world_->terrain(b.p.x + std::cos(b.head - 0.7f) * look, b.p.z + std::sin(b.head - 0.7f) * look);
                const float r = world_->terrain(b.p.x + std::cos(b.head + 0.7f) * look, b.p.z + std::sin(b.head + 0.7f) * look);
                tr = l < r ? -0.25f : 0.25f;
            } else tr = 0.03f * std::sin(b.phase * 0.07f + b.len);
            b.turn = approach(b.turn, tr, 1.5f, dt);
            b.head += b.turn * dt * std::min(1.f, 4.f / std::max(1.f, b.speed * 0.3f));
            b.speed = approach(b.speed, ahead > -2.5f ? b.cruise * 0.5f : b.cruise, 0.5f, dt);
            b.p = b.p + vec3(std::cos(b.head), 0, std::sin(b.head)) * (b.speed * dt);
        }
    }
    // they fade in when born and fade out before they go (never a pop at a distance threshold)
    for (Boat& b : boats_) {
        const float dx = b.p.x - eye_.x, dz = b.p.z - eye_.z;
        const float d2 = dx * dx + dz * dz;
        const bool behind = dx * fwd_.x + dz * fwd_.z < -900.f;
        if (b.sink <= 0.f && (d2 > 4200.f * 4200.f || behind)) b.sink = 0.001f;
        if (b.sink > 0.f) b.sink += dt;
        b.appear = std::min(1.f, b.appear + dt / 2.f);
        b.scale = smoothstepf(0.f, 1.f, b.appear) * (1.f - smoothstepf(0.f, 2.f, b.sink));
    }
    boats_.erase(std::remove_if(boats_.begin(), boats_.end(), [&](const Boat& b) { return b.sink > 2.f; }), boats_.end());
    const int want = 22;
    int live = 0;
    for (const Boat& b : boats_) live += b.sink <= 0.f;
    for (int k = 0; k < 2 && live + k < want; k++) spawnBoat(false);
}

void IslandsUniverse::boatParts(const Boat& b) {
    using namespace isl;
    const float bob = 0.07f * std::sin(b.phase * 1.3f);
    const float pitch = 0.025f * std::sin(b.phase * 1.1f + 1.f) + (b.kind == 2 ? 0.05f * saturate(b.speed / 12.f) : 0.f);
    const float heel = (b.kind == 0 ? 0.13f : 0.02f) * (b.cruise > 0.f ? 1.f : 0.f) + 0.025f * std::sin(b.phase * 0.9f);
    const Q q = qMul(qMul(qYaw(-b.head), qAxis(vec3(0, 0, 1), pitch)), qAxis(vec3(1, 0, 0), heel));
    const float K = b.scale;
    auto at = [&](float x, float y, float z) { return b.p + vec3(0, bob * K, 0) + qRot(q, vec3(x, y, z) * K); };
    auto add = [&](int mesh, int mat, const vec3& p, const vec3& s, const Q& qq, const vec3& col, float param = 0.f) {
        inst_[mesh].push_back(isl::inst(mat, p, s * K, qq, col, param, b.len * 0.37f));
    };
    const vec3 white(0.95f, 0.95f, 0.93f), metal(0.75f);
    // the sails: white, or (the fleet) in bands of the colors of the row that launched it, unfurling
    const vec3 sail = b.sailCode > 0.f ? b.sailCol : lin(0xf6f2e8), jib = b.sailCode > 0.f ? b.jibCol : lin(0xf6f2e8);
    const float uf = b.unfurl, ufx = 0.25f + 0.75f * b.unfurl;
    const float trim = 0.32f * (std::sin(b.head * 3.f) > 0.f ? 1.f : -1.f);
    switch (b.kind) {
    case 0: { // a sailboat
        const float L = b.len, B = b.beam;
        add(M_HULL, MAT_HULL, at(0, 0.8f, 0), vec3(L, 1.6f, B), q, b.col, (float)b.stripe);
        add(M_BOX, MAT_RENDER, at(-0.05f * L, 1.15f, 0), vec3(0.3f * L, 0.7f, 0.6f * B), q, white);
        add(M_BOX, MAT_GLASS, at(-0.05f * L, 1.25f, 0), vec3(0.22f * L, 0.2f, 0.62f * B), q, vec3(1.f), 0.f);
        const float mx = 0.08f * L, mh = 1.35f * L;
        add(M_CYL, MAT_METAL, at(mx, 0.8f + mh * 0.5f, 0), vec3(0.13f, mh, 0.13f), q, metal);
        const Q qs = qMul(q, qYaw(trim));
        const vec3 piv = at(mx, 2.f, 0);
        add(M_CYL, MAT_METAL, piv + qRot(qs, vec3(-0.21f * L, 0, 0)), vec3(0.1f, 0.42f * L, 0.1f), qMul(qs, qAxis(vec3(0, 0, 1), PI * 0.5f)), metal);
        add(M_SAIL, MAT_SAIL, piv, vec3(0.42f * L * ufx, (mh - 1.6f) * uf, 1.f), qs, sail, b.sailCode);
        add(M_JIB, MAT_SAIL, at(0.46f * L, 1.2f, 0), vec3(0.9f * L * ufx, (mh - 1.f) * uf, 1.f), qMul(q, qYaw(trim * 0.8f)), jib);
        break;
    }
    case 1: { // a catamaran
        const float L = b.len, B = b.beam;
        for (int s = -1; s <= 1; s += 2) add(M_HULL, MAT_HULL, at(0, 0.75f, s * B * 0.4f), vec3(L, 1.3f, 0.19f * B), q, white, 0.f);
        add(M_BOX, MAT_RENDER, at(-0.08f * L, 1.12f, 0), vec3(0.5f * L, 0.4f, 0.85f * B), q, white);
        add(M_BOX, MAT_RENDER, at(-0.1f * L, 1.75f, 0), vec3(0.3f * L, 0.95f, 0.65f * B), q, white);
        add(M_BOX, MAT_GLASS, at(0.05f * L, 1.85f, 0), vec3(0.04f * L, 0.5f, 0.6f * B), q, vec3(1.f), 0.f);
        add(M_BOX, MAT_PAINT, at(0.27f * L, 1.0f, 0), vec3(0.3f * L, 0.05f, 0.75f * B), q, vec3(0.18f));
        if (b.cruise > 0.f || b.kind == 1) {
            const float mx = 0.06f * L, mh = 1.3f * L;
            add(M_CYL, MAT_METAL, at(mx, 1.2f + mh * 0.5f, 0), vec3(0.15f, mh, 0.15f), q, metal);
            if (b.cruise > 0.f) {
                const Q qs = qMul(q, qYaw(trim * 0.7f));
                add(M_SAIL, MAT_SAIL, at(mx, 2.2f, 0), vec3(0.42f * L * ufx, (mh - 1.8f) * uf, 1.f), qs, sail, b.sailCode);
                add(M_JIB, MAT_SAIL, at(0.45f * L, 1.3f, 0), vec3(0.75f * L * ufx, (mh - 1.4f) * uf, 1.f), qMul(q, qYaw(trim * 0.6f)), jib);
            }
        }
        break;
    }
    case 2: { // a speedboat
        const float L = b.len, B = b.beam;
        add(M_HULL, MAT_HULL, at(0, 0.7f, 0), vec3(L, 1.25f, B), q, b.col, (float)b.stripe);
        add(M_BOX, MAT_GLASS, at(0.08f * L, 1.15f, 0), vec3(0.06f * L, 0.55f, 0.75f * B), qMul(q, qAxis(vec3(0, 0, 1), 0.55f)), vec3(1.f), 0.f);
        add(M_BOX, MAT_RENDER, at(0.0f, 0.98f, 0), vec3(0.14f * L, 0.6f, 0.4f * B), q, white);
        add(M_BOX, MAT_PAINT, at(-0.22f * L, 0.92f, 0), vec3(0.22f * L, 0.42f, 0.75f * B), q, lin(0xe8e0d0));
        break;
    }
    default: { // a kayak and its paddler
        add(M_HULL, MAT_HULL, at(0, 0.15f, 0), vec3(4.f, 0.32f, 0.62f), q, b.col, 0.f);
        add(M_CYL, MAT_PAINT, at(0, 0.45f, 0), vec3(0.36f, 0.6f, 0.3f), q, lin(0xf4f0e6));
        add(M_SPHERE, MAT_PAINT, at(0, 0.88f, 0), vec3(0.24f), q, lin(0x8a5a3c));
        const float sw = std::sin(b.phase * 2.6f) * 0.6f;
        add(M_CYL, MAT_METAL, at(0.2f, 0.62f, 0), vec3(0.04f, 2.3f, 0.04f), qMul(q, qMul(qAxis(vec3(1, 0, 0), PI * 0.5f + sw), qAxis(vec3(0, 1, 0), 0.f))), vec3(0.2f));
        break;
    }
    }
}

void IslandsUniverse::updatePlane(float dt) {
    using namespace isl;
    if (!plane_.on) {
        planeWait_ -= dt;
        if (planeWait_ > 0.f || alt_ > 1500.f) return;
        // across the view ahead, from one side to the other, at a few hundred meters
        const float side = rng_.chance(0.5f) ? 1.f : -1.f;
        const float dist = std::clamp(alt_ * 2.f + 500.f, 500.f, 1600.f);
        const vec3 fh = normalize(vec3(fwd_.x, 0, fwd_.z));
        const vec3 rh = normalize(vec3(right_.x, 0, right_.z));
        const vec3 c = eye_ + fh * dist;
        plane_.on = true;
        plane_.p = c - rh * (side * dist * 1.6f); // (beside, outside the view: it flies into it)
        plane_.p.y = std::clamp(alt_ * 0.6f + 60.f, 80.f, 420.f);
        plane_.v = normalize(rh * side + fh * 0.25f) * 46.f;
        plane_.t = 0.f;
        plane_.bank = 0.f;
        return;
    }
    plane_.t += dt;
    // a slow turn and a gentle bank
    const float turn = 0.05f * std::sin(plane_.t * 0.15f);
    const float c = std::cos(turn * dt), s = std::sin(turn * dt);
    plane_.v = vec3(plane_.v.x * c - plane_.v.z * s, 0.f, plane_.v.x * s + plane_.v.z * c);
    plane_.bank = approach(plane_.bank, -turn * 4.f, 0.5f, dt);
    plane_.p = plane_.p + plane_.v * dt;
    const float dx = plane_.p.x - eye_.x, dz = plane_.p.z - eye_.z;
    if (plane_.t > 72.f || dx * dx + dz * dz > 5200.f * 5200.f) {
        plane_.on = false;
        planeWait_ = rng_.range(35.f, 60.f);
    }
}

// A flock of seabirds rises beside the well (the sides alternate), flying along with the camera, drifting outward
// and up, and leaves after a few seconds.
void IslandsUniverse::releaseFlock(int n, bool big) {
    Flock f;
    const int side = flockSide_;
    flockSide_ = -flockSide_;
    const float D = std::clamp(vel_ * 1.4f + 26.f, 34.f, 150.f);
    // from the island nearest that band if there is one close, else over the water
    const vec3 band = normalize(fwd_ + right_ * (0.58f * side));
    vec3 c = eye_ + band * D;
    for (const auto& I : world_->islands)
        for (const vec3& bs : I->birdSpots) {
            const vec3 d = bs - eye_;
            const float l = length(d);
            if (l < 40.f || l > D * 1.6f) continue;
            const vec3 dn = d / l;
            if (dot(dn, fwd_) < 0.55f) continue;
            const float sideK = dot(dn, right_) * side;
            if (sideK < 0.36f || sideK > 0.6f) continue;
            c = bs;
            break;
        }
    c.y = std::max(c.y, std::max(3.f, eye_.y - D * 0.35f));
    f.c = c;
    const vec3 rh = normalize(vec3(right_.x, 0, right_.z));
    const vec3 fh = normalize(vec3(fwd_.x, 0, fwd_.z));
    f.v = fh * (vel_ * 0.9f) + rh * (side * (2.5f + 0.02f * vel_)) + vec3(0, 2.f, 0);
    f.life = big ? 13.f : 9.f;
    f.scale = std::max(1.4f, 0.026f * D);
    f.frigate = variant_ != 2 && rng_.chance(0.25f);
    f.follow = true;
    for (int i = 0; i < n; i++) {
        Bird b;
        const float r = std::sqrt(rng_.uniform()) * (6.f + n * 0.5f) * f.scale;
        const float a = rng_.range(0.f, TAU);
        b.off = vec3(std::cos(a) * r, rng_.range(-2.f, 2.f) * f.scale, std::sin(a) * r);
        b.phase = rng_.range(0.f, 100.f);
        b.glide = rng_.uniform();
        b.size = rng_.range(0.85f, 1.15f) * (f.frigate ? 1.8f : 1.2f) * f.scale;
        f.birds.push_back(b);
    }
    flocks_.push_back(f);
}

// A burst of seabirds rising from a point on the sea (a boat's launch), flying with the camera, outward and up.
void IslandsUniverse::releaseFlockAt(const vec3& c0, int n) {
    Flock f;
    const vec3 d = c0 - eye_;
    const float D = std::max(30.f, length(d));
    const float side = dot(d, right_) >= 0.f ? 1.f : -1.f;
    f.c = c0 + vec3(0, std::max(4.f, 0.04f * D), 0);
    const vec3 rh = normalize(vec3(right_.x, 0, right_.z));
    const vec3 fh = normalize(vec3(fwd_.x, 0, fwd_.z));
    f.v = fh * (vel_ * 0.9f) + rh * (side * (3.f + 0.02f * vel_)) + vec3(0, 4.f, 0);
    f.life = 8.f;
    f.scale = std::max(1.6f, 0.03f * D);
    f.frigate = false;
    f.follow = true;
    for (int i = 0; i < n; i++) {
        Bird b;
        const float r = std::sqrt(rng_.uniform()) * (5.f + n * 0.4f) * f.scale;
        const float a = rng_.range(0.f, TAU);
        b.off = vec3(std::cos(a) * r, rng_.range(0.f, 3.f) * f.scale, std::sin(a) * r);
        b.phase = rng_.range(0.f, 100.f);
        b.glide = rng_.uniform();
        b.size = rng_.range(0.85f, 1.15f) * 1.3f * f.scale;
        f.birds.push_back(b);
    }
    flocks_.push_back(f);
}

// A point on the sea in the band beside the well (side -1 left, +1 right), about `dist` away (or where the band's
// ray meets the sea, from high up).
vec3 IslandsUniverse::seaPointInBand(int side, float dist) const {
    const vec3 dir = normalize(fwd_ + right_ * (0.62f * side) - up_ * 0.12f);
    if (dir.y < -0.03f) {
        const float t = eye_.y / -dir.y;
        if (t < std::max(dist * 3.f, 9000.f)) return vec3(eye_.x + dir.x * t, 0.f, eye_.z + dir.z * t);
    }
    const vec3 dh = normalize(vec3(dir.x, 0.f, dir.z));
    return vec3(eye_.x + dh.x * dist, 0.f, eye_.z + dh.z * dist);
}

// A cleared row launches a sailboat: from the shore nearest the camera in the band beside the well (or from the
// open sea there), its sails unfurling in bands of the row's piece colors, a burst of seabirds; then it joins the
// fleet sailing along the route with the camera. The fleet grows over the song.
void IslandsUniverse::launchBoat(const std::vector<int>& types, bool big) {
    const int side = flockSide_;
    flockSide_ = -flockSide_;
    const float dist = std::clamp(vel_ * 2.2f + 70.f, 80.f, 320.f);
    vec3 p = seaPointInBand(side, dist);
    // from a beach near that point, out into the water
    float bd = 260.f * spread_;
    vec3 shore = p;
    bool found = false;
    for (const auto& I : world_->islands)
        for (const vec3& sp : I->shore) {
            const float dd = length(vec3(sp.x - p.x, 0, sp.z - p.z));
            if (dd < bd) { bd = dd; shore = sp; found = true; }
        }
    if (found) {
        std::shared_ptr<isl::Island> I = world_->nearestIsland(shore, 50.f);
        const vec3 out = I ? normalize(vec3(shore.x - I->x, 0, shore.z - I->z)) : normalize(vec3(p.x - eye_.x, 0, p.z - eye_.z));
        p = vec3(shore.x, 0, shore.z);
        for (int k = 0; k < 40 && world_->terrain(p.x, p.z) > -1.6f; k++) p = p + out * 5.f;
    }
    for (int k = 0; k < 30 && world_->terrain(p.x, p.z) > -1.6f; k++) p = p + normalize(vec3(p.x - eye_.x, 0, p.z - eye_.z)) * 12.f;
    // the sails: the row's piece types by how many cells each has, up to four bands (base 8 digits, type + 1)
    int count[7] = {};
    for (int t : types) count[std::clamp(t, 0, 6)]++;
    int order[7] = {0, 1, 2, 3, 4, 5, 6};
    std::sort(order, order + 7, [&](int a, int b) { return count[a] != count[b] ? count[a] > count[b] : a < b; });
    float code = 0.f;
    int n = 0;
    for (int i = 0; i < 4 && count[order[i]] > 0; i++, n++) code = code * 8.f + (float)(order[i] + 1);
    if (n == 0) code = 1.f;
    Boat b;
    b.kind = big ? 1 : 0;
    b.len = big ? 17.f : rng_.range(9.5f, 12.f);
    b.beam = big ? b.len * 0.5f : b.len * 0.32f;
    b.cruise = b.speed = big ? 8.f : rng_.range(6.f, 7.5f);
    b.col = rng_.chance(0.75f) ? lin(0xf6f4ee) : lin(0x1c2c4c);
    b.stripe = rng_.irange(0, 2);
    b.sailCode = 1.f + code;
    auto pal = [&](int t) { return lerp(pieceCol(t), vec3(1.f), 0.12f); };
    b.sailCol = pal(order[0]);
    b.jibCol = pal(count[order[1]] > 0 ? order[1] : order[0]);
    b.big = big;
    b.phase = rng_.range(0.f, 50.f);
    // its place in the fleet: along the route, beside it (normalized by the fleet's spread)
    const vec3 fh = normalize(vec3(fwd_.x, 0, fwd_.z)), rh(-fh.z, 0.f, fh.x);
    const vec3 d = p - eye_;
    b.sb = s_ + dot(d, fh);
    float lat = dot(d, rh);
    if (std::fabs(lat) < 60.f * spread_) lat = (lat < 0.f ? -60.f : 60.f) * spread_;
    b.latN = lat / spread_;
    b.launchP = p;
    b.launchT = 0.f;
    b.appear = 0.f;
    b.unfurl = 0.f;
    b.head = std::atan2(fh.z, fh.x);
    b.p = p;
    b.scale = std::clamp(1.f + alt_ / 160.f, 1.f, 10.f) * (big ? 1.25f : 1.f);
    // past the cap, the oldest boat sails away (sinks into the haze)
    int live = 0;
    for (const Boat& f : fleet_) live += f.sink <= 0.f;
    if (live >= 48)
        for (Boat& f : fleet_)
            if (f.sink <= 0.f && f.launchT > 12.f) { f.sink = 0.001f; break; }
    fleet_.push_back(b);
    releaseFlockAt(p, big ? 22 : 10);
}

void IslandsUniverse::updateFleet(float dt) {
    spread_ = approach(spread_, std::clamp(1.f + alt_ / 420.f, 1.f, 5.f), 0.25f, dt);
    const float scaleT = std::clamp(1.f + alt_ / 160.f, 1.f, 10.f);
    const vec3 fh = normalize(vec3(fwd_.x, 0, fwd_.z));
    for (Boat& b : fleet_) {
        b.phase += dt;
        b.launchT += dt;
        b.unfurl = b.launchT > 50.f ? 1.f : smoothstepf(0.3f, 2.8f, b.launchT);
        // the regatta: faster through the drop
        b.speed = b.cruise * (1.f + 1.2f * dropK_);
        b.sb += (double)(b.speed * dt);
        const double rel = b.sb - s_;
        // recycled ahead (it sails in again from the haze): behind the camera at once, ahead (in view) only once
        // it has faded out
        if (b.launchT > 10.f && rel > 1500.0 * spread_) b.moored = true; // (the field marks a boat fading out)
        if (b.moored) {
            b.age += dt;
            if (b.age > 1.5f) { b.sb = s_ + rng_.range(750.f, 1150.f) * spread_; b.appear = 0.f; b.moored = false; b.age = 0.f; }
        } else if (b.launchT > 10.f && rel < -150.0 * spread_) {
            b.sb = s_ + rng_.range(750.f, 1150.f) * spread_;
            b.appear = 0.f;
        }
        b.appear = std::min(1.f, b.appear + dt / 1.8f);
        const isl::RoutePt rp = world_->route(b.sb);
        const vec3 rr(-std::sin(rp.head), 0.f, std::cos(rp.head));
        // off the land: slide aside to the nearest water (the turn field holds where it goes)
        float lat0 = b.latN * spread_;
        // (inside a tight turn, an orbit's loop, never across to the other side of the loop)
        if (lat0 * rp.curv > 0.f) lat0 = std::clamp(lat0, -0.45f / std::max(std::fabs(rp.curv), 1e-5f), 0.45f / std::max(std::fabs(rp.curv), 1e-5f));
        auto wet = [&](float lat) { return world_->terrain(rp.x + rr.x * lat, rp.z + rr.z * lat) < -1.4f; };
        if (!wet(lat0 + b.turn)) {
            for (int k = 1; k <= 14; k++) {
                const float step = 22.f * k * std::sqrt(spread_);
                const float clear = 50.f * spread_; // (never onto the route: the camera flies there)
                if (std::fabs(lat0 + step) > clear && wet(lat0 + step)) { b.turn = step; break; }
                if (std::fabs(lat0 - step) > clear && wet(lat0 - step)) { b.turn = -step; break; }
            }
        } else if (b.turn != 0.f && wet(lat0)) b.turn = 0.f;
        // (sliding aside no faster than a gentle course change: a quarter of its speed, so it reads as steering)
        {
            const float maxStep = (0.25f * b.speed + 0.5f) * dt;
            b.latOff += std::clamp(approach(b.latOff, b.turn, 0.6f, dt) - b.latOff, -maxStep, maxStep);
        }
        vec3 pos(rp.x + rr.x * (lat0 + b.latOff), 0.f, rp.z + rr.z * (lat0 + b.latOff));
        float head = rp.head + 0.04f * std::sin(b.phase * 0.2f);
        if (b.launchT < 7.f) {
            // from its launch point (sailing out), it eases into its place in the fleet
            b.launchP = b.launchP + fh * (b.speed * dt);
            const float w = smoothstepf(1.5f, 7.f, b.launchT);
            pos = lerp(b.launchP, pos, w);
            head = std::atan2(fh.z, fh.x) * (1.f - w) + head * w;
        }
        // never too close to the camera (it would loom behind the well): eased aside
        {
            const vec3 dv(pos.x - eye_.x, 0.f, pos.z - eye_.z);
            const float dd = length(dv), keep = 45.f * spread_ * (b.big ? 1.4f : 1.f);
            if (dd < keep) pos = pos + (dd > 1e-3f ? dv / dd : rr) * (keep - dd);
        }
        // the bow points where it goes: the heading follows its actual motion (the route's turns, sliding aside, the
        // camera's clearance), eased; a jump (recycled ahead, just appeared) takes the route's heading at once
        {
            const vec3 mv(pos.x - b.p.x, 0.f, pos.z - b.p.z);
            const float ml = length(mv);
            if (b.appear < 0.05f || ml > b.speed * dt * 4.f + 2.f || b.launchT < 7.f) b.head = head;
            else if (ml > 1e-4f) {
                const float want = std::atan2(mv.z, mv.x);
                b.head += std::remainder(want - b.head, TAU) * std::min(1.f, 2.5f * dt);
            }
        }
        b.p = pos;
        float k = scaleT * (b.big ? 1.25f : 1.f) * smoothstepf(0.f, 1.f, b.appear);
        if (b.sink > 0.f) {
            b.sink += dt;
            k *= 1.f - smoothstepf(0.f, 2.5f, b.sink);
        }
        if (b.moored) k *= 1.f - smoothstepf(0.f, 1.5f, b.age);
        k *= 1.f - smoothstepf(6500.f, 8500.f, length(vec3(pos.x - eye_.x, 0.f, pos.z - eye_.z)));
        b.scale = k;
    }
    fleet_.erase(std::remove_if(fleet_.begin(), fleet_.end(), [](const Boat& b) { return b.sink > 2.5f; }), fleet_.end());
}

// Test hook: REFALL_ISLANDS_POPCHECK=1 reports a boat that pops (its size or place jumps in one frame) while in view.
void IslandsUniverse::popCheck(std::vector<Boat>& list, const char* what, float dt) {
    static const bool on = std::getenv("REFALL_ISLANDS_POPCHECK") != nullptr;
    for (Boat& b : list) {
        if (on && b.prevS >= 0.f && dt > 0.f) {
            const vec3 d = b.p - eye_;
            const float dl = length(d);
            const bool inView = dl < 6000.f && dot(d / std::max(dl, 1.f), fwd_) > 0.6f;
            const float jump = length(b.p - b.prevP) - (b.speed + vel_) * dt * 1.5f - 2.f;
            const bool teleported = length(b.p - b.prevP) > 50.f; // (a recycle while invisible: not a pop)
            const bool appears = b.scale - b.prevS > 0.15f * std::max(1.f, b.scale);
            const bool vanishes = !teleported && b.prevS - b.scale > 0.15f * std::max(1.f, b.prevS);
            if (inView && (appears || vanishes || (jump > 0.f && b.scale > 0.05f && b.prevS > 0.05f)))
                std::fprintf(stderr, "[islands] POP %s at %.0f m: scale %.2f -> %.2f, moved %.1f m (appear %.2f sink %.2f)\n", what, dl, b.prevS, b.scale, length(b.p - b.prevP), b.appear, b.sink);
        }
        b.prevS = b.scale;
        b.prevP = b.p;
    }
}

// On the drop's beat grid (every two beats): dolphins leap, landing on the next beat; fireworks at dusk.
void IslandsUniverse::dropBeat(double beatLen) {
    if (dropK_ < 0.4f) return;
    if (pods_.empty()) {
        for (int side = -1; side <= 1; side += 2) {
            Pod pod;
            pod.relF = rng_.range(45.f, 80.f);
            pod.relS = side * rng_.range(30.f, 48.f);
            for (int i = 0; i < 5; i++) {
                Dolphin d;
                d.off = vec3(rng_.range(-9.f, 9.f), 0.f, rng_.range(-6.f, 6.f));
                d.size = rng_.range(2.2f, 2.8f);
                d.seed = rng_.uniform();
                pod.d.push_back(d);
            }
            pods_.push_back(pod);
        }
    }
    for (Pod& pod : pods_) {
        int n = 2 + rng_.irange(0, 1); // two or three of the pod leap together
        for (Dolphin& d : pod.d) {
            if (n <= 0) break;
            if (worldTime_ - d.t0 < d.dur + 0.3f || rng_.chance(0.3f)) continue;
            d.t0 = worldTime_;
            d.dur = (float)(2.0 * beatLen) * 0.98f;
            n--;
        }
    }
    if (lights_ > 0.3f) firework();
}

void IslandsUniverse::firework() {
    // above a lit villa or village in view (else over the sea), beside the well
    std::vector<vec3> spots;
    for (const Draw& d : draws_) {
        if (d.dist > 4500.f || d.dist < 150.f) continue;
        if (d.I->type != isl::T_VILLA && d.I->type != isl::T_VILLAGE) continue;
        const vec3 c(d.I->focus.x, 0.f, d.I->focus.z);
        if (dot(normalize(c - eye_), fwd_) < 0.55f) continue;
        spots.push_back(c);
    }
    if (spots.empty()) spots.push_back(seaPointInBand(rng_.chance(0.5f) ? 1 : -1, 900.f));
    const vec3 c = spots[(size_t)rng_.irange(0, (int)spots.size() - 1)];
    const float S = std::clamp(length(c - eye_) / 600.f, 0.8f, 3.f);
    const vec3 at = c + vec3(rng_.range(-40.f, 40.f), rng_.range(70.f, 120.f) * S, rng_.range(-40.f, 40.f));
    const vec3 col = lerp(pieceCol(rng_.irange(0, 6)), vec3(1.f, 0.85f, 0.6f), 0.12f);
    for (int i = 0; i < 44; i++) {
        const float a = rng_.range(0.f, TAU), z = rng_.range(-1.f, 1.f);
        const float r = std::sqrt(1.f - z * z);
        const vec3 dir(r * std::cos(a), z, r * std::sin(a));
        sparks_.push_back({at, dir * (rng_.range(15.f, 19.f) * S), col, 0.f, rng_.range(1.8f, 2.6f)});
    }
}

void IslandsUniverse::updateBirds(float dt) {
    for (Flock& f : flocks_) {
        f.age += dt;
        // they keep up with the camera, slowly rising and drifting out
        if (f.follow) {
            const vec3 fh = normalize(vec3(fwd_.x, 0, fwd_.z));
            const float along = dot(f.v, fh);
            const vec3 keep = f.v + fh * (vel_ * 0.85f - along);
            f.v = lerp(f.v, keep, 1.f - std::exp(-0.6f * dt));
        }
        f.v.y = approach(f.v.y, 2.5f, 0.3f, dt);
        f.c = f.c + f.v * dt;
    }
    flocks_.erase(std::remove_if(flocks_.begin(), flocks_.end(), [](const Flock& f) { return f.age > f.life; }), flocks_.end());
}

void IslandsUniverse::lightLanterns(int n) {
    // along the shore of the nearest island in view (the lights at dusk)
    std::shared_ptr<isl::Island> best;
    float bd = 1e9f;
    for (const auto& I : world_->islands) {
        if (I->shore.empty()) continue;
        const vec3 d = vec3(I->x, 0, I->z) - eye_;
        const float l = length(d);
        if (l > 3000.f || dot(d / std::max(l, 1.f), fwd_) < 0.2f) continue;
        if (l < bd) { bd = l; best = I; }
    }
    if (!best) return;
    int lit = 0;
    for (const vec3& p : best->shore) {
        bool already = false;
        for (const Lantern& l : lanterns_)
            if (std::fabs(l.p.x - p.x) < 0.5f && std::fabs(l.p.z - p.z) < 0.5f) { already = true; break; }
        if (already) continue;
        lanterns_.push_back({p + vec3(0, 1.4f, 0), -0.25f * lit, rng_.uniform()});
        if (++lit >= n) break;
    }
}

void IslandsUniverse::buildClouds(float worldTime) {
    puffs_.clear();
    cloudShadows_.clear();
    const VariantDef& V = VARIANTS[variant_];
    const double C = 1100.0;
    const double wx = 2.6 * worldTime, wz = 1.1 * worldTime;
    const double ax = eye_.x + world_->ox - wx, az = eye_.z + world_->oz - wz;
    const float R = 14000.f;
    const long long i0 = (long long)std::floor((ax - R) / C), i1 = (long long)std::floor((ax + R) / C);
    const long long j0 = (long long)std::floor((az - R) / C), j1 = (long long)std::floor((az + R) / C);
    const vec3 L = sky_.sunDir;
    const vec3 focus = eye_ + vec3(fwd_.x, 0, fwd_.z) * std::clamp(alt_ * 1.5f, 80.f, 3000.f);
    struct Sh { float x, z, r, a, d2; };
    std::vector<Sh> shadows;
    for (long long j = j0; j <= j1; j++)
        for (long long i = i0; i <= i1; i++) {
            const uint32_t h = isl::hashu((uint32_t)(i * 2654435761LL) ^ (uint32_t)(j * 40503LL) ^ (uint32_t)seed_ * 7u);
            if (isl::hashf(h) > V.cloudP) continue;
            const float cx = (float)((i + 0.2 + 0.6 * isl::hashf(h + 1u)) * C + wx - world_->ox);
            const float cz = (float)((j + 0.2 + 0.6 * isl::hashf(h + 2u)) * C + wz - world_->oz);
            const float dx = cx - eye_.x, dz = cz - eye_.z;
            if (dx * dx + dz * dz > R * R) continue;
            const float base = 640.f + 280.f * isl::hashf(h + 3u);
            const float S = 120.f + 300.f * isl::hashf(h + 4u) * isl::hashf(h + 5u) + 60.f;
            const int n = 5 + (int)(isl::hashf(h + 6u) * 9.f);
            for (int k = 0; k < n; k++) {
                const uint32_t hk = h * 31u + (uint32_t)k * 977u;
                const float a = isl::hashf(hk) * TAU;
                const float rr = S * (0.15f + 0.6f * isl::hashf(hk + 1u));
                const float pr = S * (0.3f + 0.26f * isl::hashf(hk + 2u)) * (1.f - 0.35f * rr / S);
                const float py = base + pr * 0.55f + S * 0.35f * isl::hashf(hk + 3u) * (1.f - rr / S);
                const vec3 w(cx + std::cos(a) * rr, py, cz + std::sin(a) * rr * 0.8f);
                Puff p;
                p.rel = w - eye_;
                p.r = pr;
                p.seed = isl::hashf(hk + 4u);
                const float dist = length(p.rel);
                p.alpha = smoothstepf(pr * 0.7f, pr * 2.2f, dist) * smoothstepf(R, R * 0.8f, dist) * 0.92f;
                p.base = base;
                p.shade = isl::hashf(hk + 5u);
                p.d2 = dist * dist;
                if (p.alpha > 0.01f) puffs_.push_back(p);
            }
            // its shadow on the sea, where the sun casts it
            const float ly = std::max(L.y, 0.12f);
            const float sx = cx - L.x / ly * (base + S * 0.3f), sz = cz - L.z / ly * (base + S * 0.3f);
            const float fx = sx - focus.x, fz = sz - focus.z;
            shadows.push_back({sx, sz, S * 1.1f, 0.42f * saturate(L.y * 4.f), fx * fx + fz * fz});
        }
    std::sort(puffs_.begin(), puffs_.end(), [](const Puff& a, const Puff& b) { return a.d2 > b.d2; });
    std::sort(shadows.begin(), shadows.end(), [](const Sh& a, const Sh& b) { return a.d2 < b.d2; });
    for (size_t i = 0; i < shadows.size() && i < 24; i++) {
        const float v[4] = {shadows[i].x, shadows[i].z, shadows[i].r, shadows[i].a};
        cloudShadows_.insert(cloudShadows_.end(), v, v + 4);
    }
}

void IslandsUniverse::glow(const vec3& world, float size, const vec3& col, float soft) {
    const vec3 r = world - eye_;
    const float v[8] = {r.x, r.y - 0.5f * curv_ * (r.x * r.x + r.z * r.z), r.z, size, col.x, col.y, col.z, soft};
    glows_.insert(glows_.end(), v, v + 8);
}

void IslandsUniverse::push(const isl::Part& p) { inst_[p.mesh].push_back(p.in); }

// The islands in view: their level of detail (requested from the worker), their content, the shadow's share.
void IslandsUniverse::gatherIslands() {
    draws_.clear();
    for (auto& v : inst_) v.clear();
    // the view frustum's planes (relative to the eye)
    float pl[6][4];
    {
        const mat4& m = vp_;
        auto row = [&](int r, int c) { return m.at(r, c); };
        for (int i = 0; i < 3; i++)
            for (int s = 0; s < 2; s++) {
                float* p = pl[i * 2 + s];
                const float sg = s ? -1.f : 1.f;
                for (int c = 0; c < 4; c++) p[c] = row(3, c) + sg * row(i, c);
                const float l = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
                for (int c = 0; c < 4; c++) p[c] /= std::max(l, 1e-6f);
            }
    }
    const float shadowR = std::clamp(alt_ * 1.6f + 140.f, 180.f, 5000.f);
    const vec3 shC = eye_ + vec3(fwd_.x, 0, fwd_.z) * std::clamp(alt_ * 1.2f, 60.f, 2600.f);
    const float objDist = lerpf(4200.f, 3600.f, smoothstepf(900.f, 1500.f, alt_));
    for (const auto& Ip : world_->islands) {
        isl::Island& I = *Ip;
        const float dx = I.x - eye_.x, dz = I.z - eye_.z;
        const float dc = std::sqrt(dx * dx + dz * dz + eye_.y * eye_.y);
        const float dmin = std::max(0.f, dc - I.ext * 1.2f);
        if (dmin > far_) continue;
        // the level of detail: the vertex spacing grows with the distance (coarse even near: the facets of the
        // flat shading stay big, low poly)
        const float spacing = std::max(std::max(3.f, I.ext / 44.f), dmin * 0.012f);
        const float need = 2.f * I.ext / spacing;
        int want = 0;
        while (want < isl::LOD_COUNT - 1 && (float)(isl::LOD_N[want] - 1) < need) want++;
        if (!I.busy && (I.lod < 0 || want > I.lod || want < I.lod - 1)) world_->requestMesh(Ip, want);
        if (I.lod < 0) continue;
        // in view? (the bound: a sphere around the island, its height)
        const vec3 c(dx, std::max(I.top, 1.f) * 0.5f - eye_.y - 0.5f * curv_ * (dx * dx + dz * dz), dz);
        const float rad = I.ext * 1.42f + I.top;
        bool vis = true;
        for (auto& p : pl)
            if (p[0] * c.x + p[1] * c.y + p[2] * c.z + p[3] < -rad) { vis = false; break; }
        const float sx = I.x - shC.x, sz = I.z - shC.z;
        const bool inShadow = std::sqrt(sx * sx + sz * sz) < shadowR + I.ext * 1.42f;
        if (!vis && !inShadow) continue;
        draws_.push_back({&I, dc, inShadow && vis});
        if (dmin < objDist) {
            for (const isl::Part& p : I.parts) {
                // (each kind fades out, shrinking, over the last stretch before its cut: no pop)
                const float cut = p.mesh >= isl::M_PALM0 && p.mesh <= isl::M_PALM2 ? 2600.f : p.mesh == isl::M_LUMP ? 3000.f : objDist;
                if (dmin > cut) continue;
                const float k = 1.f - smoothstepf(cut * 0.8f, cut, dmin);
                isl::Part q = p;
                for (int i = 0; i < 3; i++) q.in.b[i] *= k;
                push(q);
            }
        }
    }
    std::sort(draws_.begin(), draws_.end(), [](const Draw& a, const Draw& b) { return a.dist < b.dist; });
}

// =====================================================================================================
// The well
// =====================================================================================================

void IslandsUniverse::syncCells(const Game& g) {
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const bool full = g.cell(x, y).type >= 0;
            if (full && seeds_[y][x] == 0.f) seeds_[y][x] = 0.001f + isl::hashf((uint32_t)(x * 131 + y * 977 + (int)(time_ * 10.f)));
            // (a cell of no known piece, or not the one locked there (the board filled anew): alone)
            if (full && (ids_[y][x] == 0 || (ids_[y][x] > 0 && ids_[y][x] % 8 != g.cell(x, y).type + 1))) ids_[y][x] = -1 - (y * Game::W + x);
            if (!full) { seeds_[y][x] = 0.f; ids_[y][x] = 0; }
        }
}

void IslandsUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Spawn: // the new piece's glass, one seed a cell
        for (int i = 0; i < 4; i++) pieceSeeds_[i] = 0.001f + isl::hashf((uint32_t)(i * 7919 + (int)(time_ * 1000.f) * 31 + 17));
        break;
    case GameEvent::Lock: {
        int maxY = -1;
        for (auto& c : ev.cells) maxY = std::max(maxY, c.y);
        float x0 = 99.f, x1 = -99.f;
        for (auto& c : ev.cells)
            if (c.y == maxY) { x0 = std::min(x0, (float)c.x); x1 = std::max(x1, (float)c.x); }
        // each cell keeps the tile it fell with (its own seed: a new one here changed its look as it settled), and
        // its piece (its tile runs on into the piece's other cells; the cells of a bigger lock, a filled board,
        // each alone)
        lockCount_++;
        for (size_t i = 0; i < ev.cells.size(); i++) {
            const auto& c = ev.cells[i];
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            seeds_[c.y][c.x] = i < 4 ? pieceSeeds_[i] : 0.001f + isl::hashf((uint32_t)(c.x * 31 + c.y * 131 + (int)(time_ * 100.f)));
            ids_[c.y][c.x] = ev.cells.size() <= 4 ? lockCount_ * 8 + std::clamp(c.type, 0, 6) + 1 : -1 - (c.y * Game::W + c.x);
        }
        if (maxY >= Game::HIDDEN && hardPending_) {
            // a puff of fine sand pushed out from under the piece
            const vec3 l = layout::cellPos(x0, (float)maxY), r = layout::cellPos(x1, (float)maxY);
            for (int k = 0; k < 8; k++) {
                const bool right = k % 2;
                Grain d;
                d.p = (right ? r + vec3(0.42f, -0.46f, 0.05f) : l + vec3(-0.42f, -0.46f, 0.05f));
                d.v = vec3((right ? 1.f : -1.f) * rng_.range(0.5f, 1.4f), rng_.range(0.1f, 0.6f), 0.f);
                d.t = 0.f;
                d.life = rng_.range(0.4f, 0.65f);
                d.size = rng_.range(0.05f, 0.09f);
                grains_.push_back(d);
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
                                     0.018f * std::fabs(c.x - 4.5f) + 0.025f * rowK, gridMask(c.x, y)});
            }
            rowK++;
        }
        float ns[Game::H][Game::W] = {};
        int ni[Game::H][Game::W] = {};
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (rows[y]) continue;
            std::memcpy(ns[dst], seeds_[y], sizeof(seeds_[y]));
            std::memcpy(ni[dst], ids_[y], sizeof(ids_[y]));
            dst--;
        }
        std::memcpy(seeds_, ns, sizeof(seeds_));
        std::memcpy(ids_, ni, sizeof(ids_));
        // the world: a flock of seabirds per clear (bigger with the rows), lanterns along the shore at dusk
        // a sailboat launched for each cleared row (its sails in the row's colors), a big catamaran for a quad
        if (ev.count >= 4) {
            std::vector<int> all;
            for (auto& c : ev.cells) all.push_back(c.type);
            launchBoat(all, true);
            releaseFlock(24, true);
        } else
            for (int y = 0; y < Game::H; y++) {
                if (!rows[y]) continue;
                std::vector<int> row;
                for (auto& c : ev.cells)
                    if (c.y == y) row.push_back(c.type);
                launchBoat(row, false);
            }
        if (lights_ > 0.25f) lightLanterns(ev.count >= 4 ? 30 : 5 * ev.count);
        if (ev.count >= 4) swell_ = std::max(swell_, 0.3f);
        break;
    }
    case GameEvent::HardDrop: hardPending_ = true; break;
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

void IslandsUniverse::quad(const vec3& c, float hw, float hh, const vec3& col, float a, float sx, float sy) {
    const float q[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
    for (auto& k : q) {
        const float v[11] = {c.x + k[0] * hw, c.y + k[1] * hh, c.z, col.x, col.y, col.z, a, k[0], k[1], sx, sy};
        flat_.insert(flat_.end(), v, v + 11);
    }
}
void IslandsUniverse::line(float x0, float y0, float x1, float y1, float z, float w, const vec3& col, float a) {
    const float ex = x1 - x0, ey = y1 - y0, l = std::sqrt(ex * ex + ey * ey);
    if (l < 1e-5f) return;
    const float nx = -ey / l * w * 0.5f, ny = ex / l * w * 0.5f;
    const float v[6][2] = {{x0 - nx, y0 - ny}, {x1 - nx, y1 - ny}, {x1 + nx, y1 + ny}, {x0 - nx, y0 - ny}, {x1 + nx, y1 + ny}, {x0 + nx, y0 + ny}};
    for (auto& p : v) {
        const float vv[11] = {p[0], p[1], z, col.x, col.y, col.z, a, 0.f, 0.f, 0.f, 0.f};
        flat_.insert(flat_.end(), vv, vv + 11);
    }
}

// The neighbours of a cell of the same piece, as the tiles' mask (1 left, 2 right, 4 up, 8 down, 16 up left, 32 up
// right, 64 down left, 128 down right; up on screen: the row above).
template <class F> static float neighbourMask(int x, int y, F same) {
    const int d[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    int m = 0;
    for (int k = 0; k < 8; k++)
        if (same(x + d[k][0], y + d[k][1])) m |= 1 << k;
    return (float)m;
}
float IslandsUniverse::gridMask(int x, int y) const {
    const int id = ids_[y][x];
    if (id <= 0) return 0.f;
    return neighbourMask(x, y, [&](int a, int b) { return a >= 0 && a < Game::W && b >= 0 && b < Game::H && ids_[b][a] == id; });
}

void IslandsUniverse::buildBoard(const Game& g) {
    glass_.clear();
    flat_.clear();
    if (bare_) return;
    const VariantDef& V = VARIANTS[variant_];
    const vec3 back = lin(V.backing);
    // the backing: a tinted pane of deep water over the scene (the sea and the islands show through, darkened),
    // denser toward the bottom where the stack stands; a soft shade around it, a fine pale rule
    quad(vec3(0.f, 0.f, BACK_Z - 0.01f), 6.1f, 11.1f, back * 0.4f, 0.3f, 0.16f, 0.085f);
    quad(vec3(0.f, 0.f, BACK_Z), 5.1f, 10.1f, back, 0.8f, 0.f, 0.f);
    quad(vec3(0.f, -4.f, BACK_Z + 0.001f), 5.1f, 6.1f, back, 0.35f, 0.f, 0.6f);
    const vec3 rule = mixDisplay(back, lin(0xf4ecd8), 0.4f);
    const float bx = 5.1f, by = 10.1f, zr = BACK_Z + 0.002f;
    line(-bx, -by, bx, -by, zr, 0.035f, rule, 0.6f);
    line(-bx, by, bx, by, zr, 0.035f, rule, 0.6f);
    line(-bx, -by, -bx, by, zr, 0.035f, rule, 0.6f);
    line(bx, -by, bx, by, zr, 0.035f, rule, 0.6f);
    auto dullAt = [&](int y) {
        if (overT_ < 0.f) return 0.f;
        return smoothstepf(0.f, 0.5f, overT_ - 0.05f * (float)(y - Game::HIDDEN));
    };
    auto tile = [&](const vec3& c, float sc, int type, float seed, float dull, float glowK, float alpha, bool shadow, float mask) {
        const vec3 col = pieceCol(type);
        if (shadow) {
            // its contact shadow, toward the sun's side
            quad(vec3(c.x + 0.06f * sc, c.y - 0.08f * sc, BACK_Z + 0.004f), 0.57f * sc, 0.57f * sc, vec3(0.f), 0.55f * alpha, 0.4f, 0.4f);
        }
        GlassInst gi;
        gi.a[0] = c.x; gi.a[1] = c.y; gi.a[2] = c.z; gi.a[3] = sc;
        gi.b[0] = col.x; gi.b[1] = col.y; gi.b[2] = col.z; gi.b[3] = seed;
        gi.c[0] = dull; gi.c[1] = glowK; gi.c[2] = alpha; gi.c[3] = mask;
        glass_.push_back(gi);
    };
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            tile(layout::cellPos((float)x, y - g.rowOffset(y)), 1.f, c.type, seeds_[y][x] > 0.f ? seeds_[y][x] : 0.3f, dullAt(y), 0.f, 1.f, true, gridMask(x, y));
        }
    // the cleared rows: the glass brightens and melts away, under half a second
    for (const ClearCell& cc : clearing_) {
        const float u = saturate((cc.t - cc.delay) / 0.32f);
        const float gk = smoothstepf(0.f, 0.3f, u) * (1.f - smoothstepf(0.5f, 1.f, u));
        tile(layout::cellPos((float)cc.x, (float)cc.y), 1.f - 0.35f * u * u, cc.type, cc.seed, 0.f, gk * 0.7f, 1.f - smoothstepf(0.45f, 1.f, u), false, cc.mask);
    }
    // the falling piece and its ghost
    if (g.hasPiece() && !g.over()) {
        const Piece& pc = g.piece();
        int cells[4][2];
        g.pieceCells(pc, cells);
        const float dx = g.pieceVisualX() - pc.x, dy = g.pieceVisualY() - pc.y;
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            if (c[1] < Game::HIDDEN - 1) continue;
            tile(layout::cellPos(c[0] + dx, c[1] + dy), 1.f, pc.type, pieceSeeds_[i], 0.f, 0.f, 1.f, true, neighbourMask(c[0], c[1], inPiece));
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
            // the piece's own color, a little lighter, and a faint fill: it reads over the translucent pane
            const vec3 ink = mixDisplay(pieceCol(pc.type), vec3(1.f), 0.2f);
            const float in = 0.08f, w = 0.065f, zz = BACK_Z + 0.008f;
            for (auto& c : gc)
                if (c[1] >= Game::HIDDEN) {
                    const vec3 p = layout::cellPos((float)c[0], (float)c[1]);
                    quad(vec3(p.x, p.y, zz - 0.001f), 0.5f, 0.5f, pieceCol(pc.type), 0.16f, 0.f, 0.f);
                }
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 p = layout::cellPos((float)c[0], (float)c[1]);
                const float l = p.x - 0.5f + in, r = p.x + 0.5f - in, t = p.y + 0.5f - in, b = p.y - 0.5f + in;
                const int x = c[0], y = c[1];
                const bool L = inG(x - 1, y), R = inG(x + 1, y), U = inG(x, y - 1), D = inG(x, y + 1);
                // an edge runs on to the next cell's edge, or turns at a corner: inset at an outer corner, past the
                // cell boundary by the inset at an inner one (where the diagonal neighbour is in the piece), so the
                // contour stays closed
                auto endX = [&](bool side, int sx, int sy, float inner, float edge) {
                    return !side ? inner : inG(x + sx, y + sy) ? edge + sx * in : edge;
                };
                auto endY = [&](bool side, int sx, int sy, float inner, float edge) {
                    return !side ? inner : inG(x + sx, y + sy) ? edge - sy * in : edge;
                };
                const float xl = p.x - 0.5f, xr = p.x + 0.5f, yt = p.y + 0.5f, yb = p.y - 0.5f;
                if (!L) line(l, endY(D, -1, 1, b, yb), l, endY(U, -1, -1, t, yt), zz, w, ink, 1.f);
                if (!R) line(r, endY(D, 1, 1, b, yb), r, endY(U, 1, -1, t, yt), zz, w, ink, 1.f);
                if (!U) line(endX(L, -1, -1, l, xl), t, endX(R, 1, -1, r, xr), t, zz, w, ink, 1.f);
                if (!D) line(endX(L, -1, 1, l, xl), b, endX(R, 1, 1, r, xr), b, zz, w, ink, 1.f);
            }
        }
    }
    // the sand of a hard drop
    for (const Grain& d : grains_) {
        const float u = saturate(d.t / d.life);
        const vec3 p = d.p + d.v * (d.t * (1.f - 0.45f * u)) + vec3(0, -0.6f * d.t * d.t, 0);
        quad(p, d.size, d.size, lin(0xf0e2c4), 0.75f * (1.f - u) * smoothstepf(0.f, 0.06f, d.t), 1.f, 1.f);
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
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            tile(vec3(px, py, 0.f), sc, type, 0.11f + 0.17f * i + 0.05f * type, dim, 0.f, 1.f, true, neighbourMask(c[0], c[1], inPiece));
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.55f : 0.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 0.f);
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

void IslandsUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    ugl::set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

const char* IslandsUniverse::hourName() const {
    const float t = todShown_;
    if (VARIANTS[variant_].hours == 1) {
        if (t < 0.3f) return "golden hour";
        if (t < 0.66f) return "sunset";
        if (t < 0.88f) return "dusk";
        return "nightfall";
    }
    if (t < 0.38f) return "midday";
    if (t < 0.64f) return "afternoon";
    if (t < 0.8f) return "golden hour";
    if (t < 0.93f) return "sunset";
    return "dusk";
}

// The HUD's inks (sRGB): a deep navy by day on a soft white halo, a warm white at dusk on a dark one.
void IslandsUniverse::hudInks(vec3& ink, vec3& halo, vec3& accent) const {
    const VariantDef& V = VARIANTS[variant_];
    // (the ink turns from navy to white quickly: never a mid grey on the dusk sky)
    const float dark = smoothstepf(0.4f, 0.52f, night_);
    ink = ugl::toSrgb(lerp(lin(0x0e2238), lin(0xfaf4ea), dark));
    halo = ugl::toSrgb(lerp(lin(0xf4f8fa), lin(0x0c1426), dark));
    accent = ugl::toSrgb(dark > 0.5f ? lin(V.accent) : mixDisplay(lin(V.accent), lin(0x000000), 0.1f));
}

// The panels (the lettering: hudLine).
void IslandsUniverse::drawHud(const Game& g, const std::vector<HudRect>& panels) {
    vec3 ink, halo, accent;
    hudInks(ink, halo, accent);

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
}

// The lettering, by the host (text.hpp): Outfit Medium for the names and labels, Light for the numbers and hints,
// Playfair Display Italic for the song, the notices and the hour after the universe's name; deep navy by day on a
// soft white halo, a warm white at dusk.
void IslandsUniverse::hudLine(const HudText& h, text::Line& out) {
    const float s = height_ / 1080.f;
    vec3 ink, halo, accent;
    hudInks(ink, halo, accent);
    text::Font* med = text::font(FONT_MED);
    text::Font* light = text::font(FONT_LIGHT);
    text::Font* ital = text::font(FONT_ITAL);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE:
        put(med, text::upper(h.text), base * 0.68f, base * 0.26f, ink, 0.3f + h.alpha * 1.5f, base * 0.1f);
        // the hour after it (on the left only), its cap line a little above the name's
        if (h.x >= 0 && !h.center) {
            out.add("", med, base * 0.68f, ink, 0.04f / 0.68f); // (a gap of 0.3 base after the name in all)
            text::Span& hr = out.add(std::string("  ") + hourName(), ital, base * 0.86f, accent, 0.f,
                                     saturate(0.3f + h.alpha * 1.5f) * 0.95f);
            hr.dy = -base * 0.04f;
        }
        break;
    case HUD_SECTION: put(med, text::upper(h.text), base * 0.82f, base * 0.26f, accent, 0.25f + h.alpha * 1.7f); break;
    case HUD_TITLE: put(ital, h.text, base * 1.2f, base * 0.01f, ink, h.alpha * 1.3f, -base * 0.16f); break;
    case HUD_INFO: put(light, h.text, base * 0.72f, base * 0.1f, ink, saturate(alpha * 1.2f) * 0.85f); break;
    case HUD_LABEL: put(med, text::upper(h.text), base * 0.66f, base * 0.24f, ink, saturate(alpha * 1.2f) * 0.85f, base * 0.04f); break;
    case HUD_VALUE: put(light, h.text, base * 1.06f, base * 0.04f, ink, alpha); break;
    case HUD_NOTICE: put(ital, h.text, base * 1.f, 0.f, accent, alpha); break;
    case HUD_KEY: put(med, h.text, base * 0.85f, 0.f, ink, alpha); break;
    case HUD_TOAST: put(med, text::upper(h.text), base * 1.1f, base * 0.24f, ink, alpha, -base * 0.1f); break;
    case HUD_TOAST_SUB: put(ital, h.text, base * 1.f, 0.f, accent, alpha * 0.9f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(med, h.text, base * 1.05f, base * 0.08f, ink, alpha, -base * 0.08f);
        else put(light, h.text, base * 0.84f, base * 0.04f, h.accent ? accent : ink, alpha);
        break;
    default: put(light, h.text, base * 0.82f, base * 0.03f, h.accent ? accent : ink, alpha); break;
    }
    out.look.halo = halo;
    out.look.haloA = 0.7f;
    out.look.haloW = 0.095f;
    out.look.haloSmooth = true;
    out.alpha = 1.f - smoothstepf(0.4f, 0.9f, bridge_.amount);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void IslandsUniverse::setCommon(GLuint p) {
    const VariantDef& V = VARIANTS[variant_];
    set3f(p, "uSunDir", sky_.sunDir);
    set3f(p, "uSunCol", sky_.sunCol);
    set3f(p, "uSkyTop", sky_.top);
    set3f(p, "uSkyHor", sky_.hor);
    set3f(p, "uSkyLow", sky_.low);
    set3f(p, "uFogCol", sky_.fog);
    const vec3 amb = mixDisplay(sky_.top, sky_.hor, 0.5f) * (0.55f + 0.1f * swell_);
    set3f(p, "uAmb", amb);
    set3f(p, "uBounce", lin(V.shallow) * 0.12f * saturate(sky_.sunCol.x / 2.f) + amb * 0.1f);
    set3f(p, "uEye", eye_);
    set1f(p, "uCurv", curv_);
    set1f(p, "uTime", worldTime_);
    set1f(p, "uNight", night_);
    set1f(p, "uHaze", sky_.haze);
    set1f(p, "uHazeH", 950.f);
    set1f(p, "uDip", std::sqrt(2.f * alt_ * curv_));
    const int nc = (int)(cloudShadows_.size() / 4);
    set1i(p, "uNumClouds", nc);
    if (nc > 0) glUniform4fv(ugl::U(p, "uClouds"), nc, cloudShadows_.data());
}

void IslandsUniverse::drawTerrain(GLuint prog, int mode, bool shadowPass) {
    const VariantDef& V = VARIANTS[variant_];
    glUseProgram(prog);
    set1i(prog, "uMode", mode);
    set3f(prog, "uSand", lin(V.sand));
    set3f(prog, "uSandWet", lin(V.sandWet));
    set3f(prog, "uGrass", lin(V.grass));
    set3f(prog, "uJungle", lin(V.jungle));
    set3f(prog, "uRock", lin(V.rock));
    set3f(prog, "uCliff", lin(V.cliff));
    set3f(prog, "uSeabed", lin(V.seabed));
    set3f(prog, "uReefA", lin(V.reefA));
    set3f(prog, "uReefB", lin(V.reefB));
    set3f(prog, "uDry", lin(V.dry));
    set1f(prog, "uDryK", V.dryK);
    set3f(prog, "uFoam", vec3(0.95f, 0.97f, 1.f));
    const GLint uOff = ugl::U(prog, "uOff"), uSeed = ugl::U(prog, "uSeed"), uIsleR = ugl::U(prog, "uIsleR"), uFallA = ugl::U(prog, "uFallA"), uFallW = ugl::U(prog, "uFallW");
    for (const Draw& d : draws_) {
        if (shadowPass && !d.shadow) continue;
        const isl::Island& I = *d.I;
        if (mode == 1 && I.type == isl::T_ISLET && d.dist > 6000.f) continue;
        glUniform3f(uOff, I.x - eye_.x, -eye_.y, I.z - eye_.z);
        glUniform1f(uSeed, (float)(I.seed % 997u) * 0.37f);
        glUniform1f(uIsleR, I.R);
        glUniform1f(uFallA, I.fallA);
        glUniform1f(uFallW, I.fallW);
        glBindVertexArray(I.vao);
        glDrawElements(GL_TRIANGLES, idxCount_[I.lod], GL_UNSIGNED_INT, nullptr);
    }
}

void IslandsUniverse::drawObjects(GLuint prog) {
    glUseProgram(prog);
    for (int m = 0; m < isl::M_COUNT; m++) {
        if (inst_[m].empty()) continue;
        glBindVertexArray(meshVao_[m]);
        glDrawArraysInstanced(GL_TRIANGLES, 0, meshCount_[m], (GLsizei)inst_[m].size());
    }
}

void IslandsUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    static const bool gpuTime = std::getenv("REFALL_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
    const auto cpu0 = std::chrono::steady_clock::now();
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    if (builtVariant_ != variant_) buildWorld();
    if (!bare_) syncCells(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    time_ += f.paused ? 0.f : dt;
    worldTime_ += wdt;
    if (!f.paused) songT_ += dt;
    const VariantDef& V = VARIANTS[variant_];

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wp = smoothstepf(1.1f, 2.f, lv_);
    swell_ = approach(swell_, 0.f, 0.35f, dt);
    kickIn_ = approach(kickIn_, 0.f, 5.f, dt);
    // the glitter on the sea at a peak: on the beat grid (every two beats), and the kicks
    if (!f.paused && music.hasSong) {
        const double hb = std::floor(music.beat / 2.0 + 1e-4);
        if (lastHalf_ < 0.0 || hb < lastHalf_ - 8.0) lastHalf_ = hb;
        if (hb > lastHalf_) {
            lastHalf_ = hb;
            if (wp > 0.3f) sparkle_ = std::max(sparkle_, 0.6f * wp);
            dropBeat(60.0 / std::max(60.f, music.bpm));
        }
    }
    sparkle_ = std::max(approach(sparkle_, 0.f, 2.5f, dt), kickIn_ * wp * 0.8f);
    std::string section;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) section = h.text;
    // the drop: the whole drop section (from the HUD's section, else the drop's peak), eased in, out over seconds
    {
        const bool inDrop = section.empty() ? dropMode_ : section == "DROP";
        const float target = inDrop && overT_ < 0.f ? 1.f : 0.f;
        dropK_ = approach(dropK_, target, target > dropK_ ? 1.2f : 0.4f, dt);
        if (dropK_ < 0.03f) pods_.clear();
    }

    // ---- The hour: the day passes over the song.
    {
        float t = saturate(songT_ / std::max(20.f, songLen_));
        if (hookTod() >= 0.f) t = hookTod();
        tod_ = t;
        if (!todInit_) { todShown_ = tod_; todInit_ = true; }
        todShown_ = std::fabs(tod_ - todShown_) > 0.2f ? approach(todShown_, tod_, 0.4f, dt) : approach(todShown_, tod_, 2.f, dt);
        const Hour* H = V.hours == 1 ? EVENING : DAY;
        const int HN = V.hours == 1 ? (int)(sizeof(EVENING) / sizeof(Hour)) : (int)(sizeof(DAY) / sizeof(Hour));
        int i = 0;
        while (i + 1 < HN - 1 && H[i + 1].t < todShown_) i++;
        const Hour& a = H[i];
        const Hour& b = H[i + 1];
        const float u = smoothstepf(0.f, 1.f, saturate((todShown_ - a.t) / std::max(1e-4f, b.t - a.t)));
        const float elev = lerpf(a.elev, b.elev, u) * PI / 180.f;
        const float az = lerpf(1.25f, 0.42f, todShown_);
        sky_.sunDir = normalize(vec3(std::cos(az) * std::cos(elev), std::sin(elev), std::sin(az) * std::cos(elev)));
        sky_.sunCol = lerp(lin(a.sun) * a.sunK, lin(b.sun) * b.sunK, u);
        sky_.top = mixDisplay(lin(a.top), lin(b.top), u);
        sky_.hor = mixDisplay(lin(a.hor), lin(b.hor), u);
        sky_.low = mixDisplay(lin(a.low), lin(b.low), u);
        sky_.fog = mixDisplay(lin(a.fog), lin(b.fog), u);
        sky_.haze = lerpf(a.haze, b.haze, u);
        lights_ = V.hours == 1 ? smoothstepf(0.45f, 0.75f, todShown_) : smoothstepf(0.82f, 0.95f, todShown_);
        night_ = V.hours == 1 ? smoothstepf(0.62f, 0.85f, todShown_) : smoothstepf(0.9f, 1.f, todShown_);
        // slow motion: a glassy blue hour, the sea calm
        sky_.sunCol = lerp(sky_.sunCol, sky_.sunCol * vec3(0.6f, 0.7f, 0.95f) * 0.75f, slow_);
        sky_.hor = lerp(sky_.hor, mixDisplay(sky_.hor, lin(0x8aa8d0), 0.55f), slow_);
        sky_.top = lerp(sky_.top, mixDisplay(sky_.top, lin(0x3a5a9a), 0.5f), slow_);
        sky_.fog = lerp(sky_.fog, mixDisplay(sky_.fog, lin(0x9ab4d4), 0.5f), slow_);
        // the drop: a real golden hour (a low warm sun, long shadows, golden glitter, a warm sky under a blue zenith)
        const float gk = smoothstepf(0.f, 1.f, dropK_) * (1.f - lights_) * (1.f - slow_);
        if (gk > 0.f) {
            const float e = std::asin(std::clamp(sky_.sunDir.y, -1.f, 1.f));
            const float e2 = lerpf(e, std::min(e, 0.2f), gk);
            const vec3 h = normalize(vec3(sky_.sunDir.x, 0.f, sky_.sunDir.z));
            sky_.sunDir = normalize(h * std::cos(e2) + vec3(0, std::sin(e2), 0));
            sky_.sunCol = lerp(sky_.sunCol, lin(0xffb05a) * 2.3f, gk);
            sky_.hor = mixDisplay(sky_.hor, lin(0xf6c070), gk * 0.7f);
            sky_.low = mixDisplay(sky_.low, lin(0xd0a888), gk * 0.5f);
            sky_.fog = mixDisplay(sky_.fog, lin(0xe8b880), gk * 0.5f);
            sky_.haze *= 1.f - 0.5f * gk;
            sky_.top = mixDisplay(sky_.top, lin(0x2462c8), gk * 0.5f);
        }
    }

    // ---- The flight, the world streaming around it.
    fly(dt, wdt, section);
    // the setup tilts the view down from the sky onto the sea, the teardown back up (the look only, not the flight)
    {
        const float in = 1.f - smoothstepf(0.f, 0.8f, f.setup), out = smoothstepf(0.05f, 0.95f, f.teardown);
        const float tilt = 0.5f * (in * in + out * out);
        if (tilt > 0.f) {
            fwd_ = normalize(fwd_ * std::cos(tilt) + up_ * std::sin(tilt));
            up_ = cross(right_, fwd_);
        }
    }
    curv_ = lerpf(1.f / 6.4e6f, 1.f / 1.3e5f, smoothstepf(250.f, 2600.f, alt_));
    world_->update(eye_, s_, std::clamp(alt_ * 6.f, 9000.f, 14000.f));
    if (std::fabs(eye_.x) > 50000.f || std::fabs(eye_.z) > 50000.f) {
        // (far from the origin: everything moves back, in whole 4096 m steps)
        const float dx = std::round(eye_.x / 4096.f) * 4096.f, dz = std::round(eye_.z / 4096.f) * 4096.f;
        world_->rebase(dx, dz);
        for (Boat& b : boats_) { b.p.x -= dx; b.p.z -= dz; }
        for (Flock& fl : flocks_) { fl.c.x -= dx; fl.c.z -= dz; }
        for (Lantern& l : lanterns_) { l.p.x -= dx; l.p.z -= dz; }
        plane_.p.x -= dx;
        plane_.p.z -= dz;
        eye_.x -= dx;
        eye_.z -= dz;
    }
    uploadMeshes();
    if (boats_.empty()) for (int k = 0; k < 18; k++) spawnBoat(true);
    updateBoats(wdt);
    updatePlane(wdt);
    updateBirds(wdt);
    updateFleet(wdt);
    popCheck(boats_, "boat", wdt);
    popCheck(fleet_, "fleet boat", wdt);
    for (Spark& sp : sparks_) {
        sp.t += wdt;
        sp.v = sp.v * std::exp(-1.1f * wdt) + vec3(0, -4.f * wdt, 0);
        sp.p = sp.p + sp.v * wdt;
    }
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& sp) { return sp.t > sp.life; }), sparks_.end());
    sparkle_ = std::max(sparkle_, 0.45f * dropK_ * (1.f - lights_));
    for (Lantern& l : lanterns_) l.t += dt;
    lanterns_.erase(std::remove_if(lanterns_.begin(), lanterns_.end(), [&](const Lantern& l) {
                        const float dx = l.p.x - eye_.x, dz = l.p.z - eye_.z;
                        return dx * dx + dz * dz > 6000.f * 6000.f || lights_ < 0.05f;
                    }),
                    lanterns_.end());
    for (ClearCell& c : clearing_) c.t += dt;
    clearing_.erase(std::remove_if(clearing_.begin(), clearing_.end(), [](const ClearCell& c) { return c.t > c.delay + 0.34f; }), clearing_.end());
    for (Grain& d : grains_) d.t += dt;
    grains_.erase(std::remove_if(grains_.begin(), grains_.end(), [](const Grain& d) { return d.t > d.life; }), grains_.end());
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }

    // ---- The camera's matrices (the eye at the origin).
    near_ = std::clamp(alt_ * 0.06f, 0.25f, 40.f);
    far_ = std::clamp(std::sqrt(2.f * alt_ / curv_) + 12000.f, 16000.f, 60000.f);
    fov_ = lerpf(52.f, 48.f, smoothstepf(200.f, 2000.f, alt_));
    view_ = mat4::lookAt(vec3(0, 0, 0), fwd_, up_);
    proj_ = mat4::perspective(fov_ * PI / 180.f, (float)width_ / height_, near_, far_);
    vp_ = proj_ * view_;
    invVp_ = inverse(vp_);

    // ---- The geometry of the frame: islands, boats, the seaplane, birds, clouds, glows.
    gatherIslands();
    for (const Boat& b : boats_) {
        if (b.scale > 0.01f) {
            boatParts(b);
        }
    }
    // the fleet (the boats the cleared rows launched)
    for (const Boat& b : fleet_) {
        const float dx = b.p.x - eye_.x, dz = b.p.z - eye_.z;
        if (dx * dx + dz * dz > 9000.f * 9000.f || b.scale < 0.02f) continue;
        boatParts(b);
    }
    // dolphins leaping beside the camera through the drop (each leap lands on the beat)
    if (!pods_.empty()) {
        const vec3 fh = normalize(vec3(fwd_.x, 0, fwd_.z)), rh(-fh.z, 0.f, fh.x);
        const float head = std::atan2(fh.z, fh.x);
        for (const Pod& pod : pods_) {
            const vec3 c = vec3(eye_.x, 0, eye_.z) + fh * pod.relF + rh * pod.relS;
            for (const Dolphin& d : pod.d) {
                const float u = (worldTime_ - d.t0) / d.dur;
                if (u < 0.f || u > 1.f) continue;
                const float H = 1.6f + 1.2f * d.seed, Lr = 7.f + 3.f * d.seed;
                const float y = -0.6f + (H + 0.6f) * 4.f * u * (1.f - u);
                const float slope = (H + 0.6f) * 4.f * (1.f - 2.f * u) / Lr;
                const vec3 p = c + fh * (d.off.x + Lr * (u - 0.5f)) + rh * d.off.z + vec3(0, y, 0);
                const Q q = qMul(qYaw(-head), qAxis(vec3(0, 0, 1), std::atan(slope)));
                inst_[isl::M_DOLPHIN].push_back(isl::inst(isl::MAT_HULL, p, vec3(d.size), q, lin(0x5c6e80), 0.f, d.seed));
            }
        }
    }
    if (plane_.on) {
        using namespace isl;
        const float head = std::atan2(plane_.v.z, plane_.v.x);
        const Q q = qMul(qYaw(-head), qAxis(vec3(1, 0, 0), plane_.bank));
        // (it fades in as it arrives and out before it goes)
        const float pdx = plane_.p.x - eye_.x, pdz = plane_.p.z - eye_.z;
        const float kp = smoothstepf(0.f, 1.5f, plane_.t) * (1.f - smoothstepf(70.f, 72.f, plane_.t)) * (1.f - smoothstepf(4200.f, 5200.f, std::sqrt(pdx * pdx + pdz * pdz)));
        auto at = [&](float x, float y, float z) { return plane_.p + qRot(q, vec3(x, y, z) * kp); };
        auto add = [&](int mesh, int mat, const vec3& p, const vec3& s, const Q& qq, const vec3& col, float param = 0.f) {
            inst_[mesh].push_back(isl::inst(mat, p, s * kp, qq, col, param, 0.3f));
        };
        const vec3 white(0.95f), red = lin(0xd8342c);
        add(M_SPHERE, MAT_PAINT, at(0, 0, 0), vec3(9.f, 1.5f, 1.4f), q, white);
        add(M_SPHERE, MAT_GLASS, at(2.2f, 0.45f, 0), vec3(2.4f, 0.9f, 1.15f), q, vec3(1.f), 0.f);
        add(M_BOX, MAT_PAINT, at(0.8f, 0.85f, 0), vec3(1.7f, 0.15f, 13.f), q, white);
        for (int s = -1; s <= 1; s += 2) add(M_BOX, MAT_PAINT, at(0.8f, 0.86f, s * 6.1f), vec3(1.72f, 0.16f, 0.9f), q, red);
        add(M_BOX, MAT_PAINT, at(-4.f, 1.0f, 0), vec3(1.6f, 1.8f, 0.14f), q, red);
        add(M_BOX, MAT_PAINT, at(-4.f, 0.35f, 0), vec3(1.2f, 0.12f, 4.f), q, white);
        for (int s = -1; s <= 1; s += 2) {
            add(M_HULL, MAT_HULL, at(0.4f, -1.9f, s * 1.6f), vec3(6.f, 0.6f, 0.62f), q, white, 0.f);
            add(M_BOX, MAT_METAL, at(1.2f, -1.f, s * 1.1f), vec3(0.1f, 1.5f, 0.1f), qMul(q, qAxis(vec3(1, 0, 0), s * 0.35f)), vec3(0.3f));
            add(M_BOX, MAT_METAL, at(-0.6f, -1.f, s * 1.1f), vec3(0.1f, 1.5f, 0.1f), qMul(q, qAxis(vec3(1, 0, 0), s * 0.35f)), vec3(0.3f));
        }
    }
    for (const Flock& fl : flocks_) {
        const float fade = smoothstepf(0.f, 0.8f, fl.age) * (1.f - smoothstepf(fl.life - 2.f, fl.life, fl.age));
        if (fade < 0.02f) continue;
        const float head = std::atan2(fl.v.z, fl.v.x);
        for (const Bird& b : fl.birds) {
            const float t = fl.age + b.phase;
            const vec3 wob(std::sin(t * 0.7f) * 2.f, std::sin(t * 0.9f + 1.f) * 1.2f, std::cos(t * 0.6f) * 2.f);
            const vec3 p = fl.c + b.off * (1.f + 0.4f * smoothstepf(0.f, 6.f, fl.age)) + wob * fl.scale;
            const float flap = std::sin(t * 0.35f + b.glide * 6.f) > -0.2f ? 1.f : 0.f;
            const Q q = qMul(qYaw(-head), qAxis(vec3(1, 0, 0), 0.25f * std::sin(t * 0.5f)));
            inst_[isl::M_BIRD].push_back(isl::inst(isl::MAT_BIRD, p, vec3(b.size * fade), q, fl.frigate ? vec3(0.07f) : vec3(0.95f), flap, b.phase));
        }
    }
    buildClouds(worldTime_);
    glows_.clear();
    {
        // the lights at dusk: windows, lamps, the pools, the lighthouses and their beams; the lanterns lit by the clears
        if (lights_ > 0.01f)
            for (const Draw& d : draws_) {
                if (d.dist > 5000.f) continue;
                for (const isl::Light& l : d.I->lights) {
                    if (l.kind == 3) {
                        glow(l.p, 3.f + d.dist * 0.004f, vec3(1.f, 0.85f, 0.6f) * (3.f * lights_), 0.4f);
                        const float a = worldTime_ * 0.9f + d.I->seed;
                        const vec3 dir(std::cos(a), -0.02f, std::sin(a));
                        for (int k = 1; k <= 40; k++) { // (overlapping soft sprites: one continuous beam)
                            const float r = 3.f + k * 7.f;
                            glow(l.p + dir * r, 2.f + r * 0.16f, vec3(1.f, 0.88f, 0.65f) * (0.14f * lights_ / (1.f + k * 0.09f)), 1.f);
                        }
                    } else if (l.kind == 2) glow(l.p, l.size, vec3(0.2f, 1.f, 0.95f) * (0.5f * lights_), 0.9f);
                    else glow(l.p, l.size * (l.kind == 1 ? 1.4f : 1.f), l.col * (1.2f * lights_), 0.5f);
                }
            }
        for (const Lantern& l : lanterns_) {
            if (l.t < 0.f) continue;
            const float on = smoothstepf(0.f, 0.6f, l.t) * lights_;
            const float fl = 0.85f + 0.15f * std::sin(worldTime_ * 7.f + l.seed * 40.f);
            glow(l.p, 1.2f, vec3(1.f, 0.62f, 0.3f) * (2.2f * on * fl), 0.45f);
        }
        if (plane_.on) glow(plane_.p + vec3(0, -0.5f, 0), 0.6f, vec3(1.f, 0.2f, 0.15f) * (2.f * std::max(lights_, 0.3f)), 0.2f);
        // the fleet's lanterns at dusk: at each masthead and over the stern, a field of warm lights on the sea
        if (lights_ > 0.01f)
            for (const Boat& b : fleet_) {
                if (b.scale < 0.05f) continue;
                const Q q = qYaw(-b.head);
                const float fl = 0.9f + 0.1f * std::sin(worldTime_ * 5.f + b.phase * 7.f);
                const float far = std::max(1.f, length(b.p - eye_) / 400.f);
                glow(b.p + qRot(q, vec3(0.08f * b.len, 0.9f + 1.3f * b.len, 0.f) * b.scale), 0.9f * b.scale * far, vec3(1.f, 0.7f, 0.4f) * (1.6f * lights_ * fl), 0.4f);
                glow(b.p + qRot(q, vec3(-0.45f * b.len, 1.6f, 0.f) * b.scale), 1.4f * b.scale * far, vec3(1.f, 0.6f, 0.3f) * (1.8f * lights_ * fl), 0.5f);
            }
        // fireworks: soft blooms, and their reflections on the sea (where the mirror image meets the surface)
        for (const Spark& sp : sparks_) {
            const float u = sp.t / sp.life;
            const float a = (1.f - u) * (1.f - u) * smoothstepf(0.12f, 0.45f, sp.t); // (born dim: no white flash while they overlap)
            const float far = std::max(1.f, length(sp.p - eye_) / 500.f);
            glow(sp.p, 1.5f * far, sp.col * (0.75f * a), 0.6f);
            const float h = std::max(sp.p.y, 1.f), A = std::max(eye_.y, 1.f);
            const vec3 dh = vec3(sp.p.x - eye_.x, 0.f, sp.p.z - eye_.z);
            const vec3 r = vec3(eye_.x, 0.15f, eye_.z) + dh * (A / (A + h));
            glow(r, 1.8f * far, sp.col * (0.22f * a), 0.9f);
        }
        // the splashes of the dolphins' leaps
        if (!pods_.empty()) {
            const vec3 fh = normalize(vec3(fwd_.x, 0, fwd_.z)), rh(-fh.z, 0.f, fh.x);
            for (const Pod& pod : pods_)
                for (const Dolphin& d : pod.d) {
                    const float u = (worldTime_ - d.t0) / d.dur;
                    if (u < -0.02f || u > 1.25f) continue;
                    const float Lr = 7.f + 3.f * d.seed;
                    const vec3 c = vec3(eye_.x, 0.3f, eye_.z) + fh * (pod.relF + d.off.x) + rh * (pod.relS + d.off.z);
                    const float k0 = std::exp(-u * u * 60.f), k1 = std::exp(-(u - 1.f) * (u - 1.f) * 40.f);
                    const vec3 lum = (sky_.sunCol * 0.3f + vec3(0.5f)) * 0.35f;
                    if (k0 > 0.02f) glow(c - fh * (Lr * 0.5f), 1.6f, lum * k0, 0.9f);
                    if (k1 > 0.02f) glow(c + fh * (Lr * 0.5f), 1.8f, lum * k1, 0.9f);
                }
        }
        // mist at the foot of the waterfalls
        for (const Draw& d : draws_) {
            if (d.I->fallW <= 0.f || d.dist > 3000.f) continue;
            const vec3 b(d.I->x + std::cos(d.I->fallA) * d.I->R * 1.01f, 3.f, d.I->z + std::sin(d.I->fallA) * d.I->R * 1.01f);
            glow(b, 14.f, vec3(0.5f, 0.55f, 0.6f) * (0.18f * saturate(sky_.sunCol.x)), 1.f);
        }
    }
    for (int m = 0; m < isl::M_COUNT; m++) {
        glBindBuffer(GL_ARRAY_BUFFER, instVbo_[m]);
        glBufferData(GL_ARRAY_BUFFER, inst_[m].size() * sizeof(Inst), inst_[m].data(), GL_STREAM_DRAW);
    }

    // ---- The sun's shadow: around where the camera looks, snapped to its texels.
    const vec3 L = sky_.sunDir.y > 0.06f ? sky_.sunDir : normalize(vec3(sky_.sunDir.x, 0.06f, sky_.sunDir.z));
    const float shadowR = std::clamp(alt_ * 1.6f + 140.f, 180.f, 5000.f);
    const bool shadowOn = sky_.sunDir.y > -0.02f;
    {
        vec3 c = vec3(fwd_.x, 0, fwd_.z) * std::clamp(alt_ * 1.2f, 60.f, 2600.f) - vec3(0, eye_.y, 0);
        const vec3 up = std::fabs(L.y) > 0.98f ? vec3(0, 0, 1) : vec3(0, 1, 0);
        mat4 lv = mat4::lookAt(vec3(0, 0, 0), L * -1.f, up);
        // snap the center to the map's texels (steady shadows while flying)
        const vec4 lc = lv * vec4(c, 1.f);
        const float texel = 2.f * shadowR / SHADOW_SIZE;
        const float sx = std::floor(lc.x / texel) * texel, sy = std::floor(lc.y / texel) * texel;
        const float depth = shadowR + 900.f;
        lightVp_ = mat4::ortho(sx - shadowR, sx + shadowR, sy - shadowR, sy + shadowR, -lc.z - depth, -lc.z + depth) * lv;
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    if (shadowOn) {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 2.f);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, shadowDummy_);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(progTerrain_);
        set1i(progTerrain_, "uShadow", 7);
        setMat(progTerrain_, "uVP", lightVp_);
        set1f(progTerrain_, "uCurv", curv_);
        drawTerrain(progTerrain_, 3, true);
        glUseProgram(progObj_);
        set1i(progObj_, "uShadow", 7);
        setMat(progObj_, "uVP", lightVp_);
        set3f(progObj_, "uEye", eye_);
        set1f(progObj_, "uCurv", curv_);
        set1f(progObj_, "uTime", worldTime_);
        set1i(progObj_, "uMode", 3);
        drawObjects(progObj_);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    auto bindShadow = [&](GLuint p) {
        setMat(p, "uLightVP", lightVp_);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        set1i(p, "uShadow", 1);
        set1f(p, "uSoft", 0.9f / SHADOW_SIZE * std::clamp(400.f / shadowR, 0.4f, 2.5f));
        set1f(p, "uShadowOn", shadowOn ? 1.f : 0.f);
        glActiveTexture(GL_TEXTURE0);
    };
    auto objUniforms = [&](GLuint p) {
        setCommon(p);
        set1f(p, "uWind", 0.4f + 0.6f * lv_ * 0.5f);
        set3f(p, "uLeaf", lin(V.leaf));
        set3f(p, "uLeaf2", lin(V.leaf2));
        set3f(p, "uTrunk", lin(V.trunk));
        set3f(p, "uFoliage", lin(V.fol1));
        set3f(p, "uFoliage2", lin(V.fol2));
        set3f(p, "uWater", lin(0x36d6e0));
        set3f(p, "uWaterGlow", lin(0x20e8f0) * 0.8f);
        set3f(p, "uRoomNight", vec3(1.f, 0.66f, 0.34f) * 1.8f);
        set1f(p, "uLights", lights_);
        {
            float pal[21];
            for (int t = 0; t < 7; t++) {
                const vec3 c = lerp(pieceCol(t), vec3(1.f), 0.12f);
                pal[t * 3] = c.x; pal[t * 3 + 1] = c.y; pal[t * 3 + 2] = c.z;
            }
            glUniform3fv(ugl::U(p, "uSailPal"), 7, pal);
        }
        bindShadow(p);
    };

    // ---- The seabed (half resolution): the terrain under the water, its distance (the sea's bands by depth).
    seabedT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(progTerrain_);
    setCommon(progTerrain_);
    bindShadow(progTerrain_);
    setMat(progTerrain_, "uVP", vp_);
    drawTerrain(progTerrain_, 1, false);
    // ---- The main pass (multisampled): the sky, the islands, their content, the sea, the clouds, the glows.
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(progSky_);
    setCommon(progSky_);
    setMat(progSky_, "uInvVP", invVp_);
    set1f(progSky_, "uStars", night_);
    ugl::drawFullscreen();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUseProgram(progTerrain_);
    setMat(progTerrain_, "uVP", vp_);
    drawTerrain(progTerrain_, 0, false);
    glUseProgram(progObj_);
    objUniforms(progObj_);
    setMat(progObj_, "uVP", vp_);
    set1i(progObj_, "uMode", 0);
    drawObjects(progObj_);
    // the sea
    glUseProgram(progWater_);
    setCommon(progWater_);
    setMat(progWater_, "uInvVP", invVp_);
    setMat(progWater_, "uVP", vp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, seabedT_.tex);
    set1i(progWater_, "uSeabed", 0);
    ugl::set2f(progWater_, "uInvRes", 1.f / width_, 1.f / height_);
    set3f(progWater_, "uShallow", lin(V.shallow));
    set3f(progWater_, "uDeep", lin(V.deep));
    set3f(progWater_, "uFoam", vec3(0.96f, 0.98f, 1.f));
    set1f(progWater_, "uWave", V.wave * (0.75f + 0.25f * lv_ + 0.2f * energy_));
    set1f(progWater_, "uSparkle", sparkle_);
    set1f(progWater_, "uCalm", slow_);
    {
        // the nearest boats' wakes
        std::vector<std::pair<float, const Boat*>> near;
        for (const Boat& b : boats_) {
            const float dx = b.p.x - eye_.x, dz = b.p.z - eye_.z;
            near.push_back({dx * dx + dz * dz, &b});
        }
        for (const Boat& b : fleet_) {
            if (b.scale < 0.3f) continue;
            const float dx = b.p.x - eye_.x, dz = b.p.z - eye_.z;
            near.push_back({(dx * dx + dz * dz) * 0.7f, &b});
        }
        std::sort(near.begin(), near.end(), [](const std::pair<float, const Boat*>& a, const std::pair<float, const Boat*>& b) { return a.first < b.first; });
        float A[24 * 4], B[24 * 4];
        int n = 0;
        for (const auto& pb : near) {
            if (n >= 24) break;
            const Boat& b = *pb.second;
            A[n * 4] = b.p.x; A[n * 4 + 1] = b.p.z; A[n * 4 + 2] = b.head; A[n * 4 + 3] = b.speed;
            B[n * 4] = b.len * b.scale; B[n * 4 + 1] = b.beam * b.scale; B[n * 4 + 2] = (float)b.kind; B[n * 4 + 3] = 0.f;
            n++;
        }
        set1i(progWater_, "uNumBoats", n);
        if (n) {
            glUniform4fv(ugl::U(progWater_, "uBoats"), n, A);
            glUniform4fv(ugl::U(progWater_, "uBoatsB"), n, B);
        }
        float Fv[16];
        int nf = 0;
        for (const Draw& d : draws_) {
            if (d.I->fallW <= 0.f || nf >= 4) continue;
            Fv[nf * 4] = d.I->x + std::cos(d.I->fallA) * d.I->R * 1.012f;
            Fv[nf * 4 + 1] = d.I->z + std::sin(d.I->fallA) * d.I->R * 1.012f;
            Fv[nf * 4 + 2] = 12.f;
            Fv[nf * 4 + 3] = 0.f;
            nf++;
        }
        set1i(progWater_, "uNumFalls", nf);
        if (nf) glUniform4fv(ugl::U(progWater_, "uFalls"), nf, Fv);
    }
    ugl::drawFullscreen();
    // the clouds
    if (!puffs_.empty()) {
        std::vector<float> pv;
        pv.reserve(puffs_.size() * 8);
        for (const Puff& p : puffs_) {
            const vec3 r = p.rel;
            const float v[8] = {r.x, r.y - 0.5f * curv_ * (r.x * r.x + r.z * r.z), r.z, p.r, p.seed, p.alpha, p.base, p.shade};
            pv.insert(pv.end(), v, v + 8);
        }
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE); // (the alpha marks the pieces only)
        glDepthMask(GL_FALSE);
        glUseProgram(progCloud_);
        setCommon(progCloud_);
        setMat(progCloud_, "uVP", vp_);
        set3f(progCloud_, "uRight", right_);
        set3f(progCloud_, "uUp", up_);
        set3f(progCloud_, "uFwd", fwd_);
        const vec3 amb = mixDisplay(sky_.top, sky_.hor, 0.6f);
        // lit by the sun (warm and strong when it is low: the clouds glow at the golden hour), shaded by the sky
        const float lowSun = 1.f - smoothstepf(0.05f, 0.45f, sky_.sunDir.y);
        set3f(progCloud_, "uCloudLit", sky_.sunCol * (0.42f + 0.25f * lowSun) + amb * (0.75f - 0.3f * lowSun));
        set3f(progCloud_, "uCloudShade", mixDisplay(amb, sky_.top, 0.35f * lowSun) * 0.72f + lin(V.shallow) * 0.04f);
        glBindVertexArray(cloudVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cloudVbo_);
        glBufferData(GL_ARRAY_BUFFER, pv.size() * sizeof(float), pv.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)(pv.size() / 8));
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    if (!glows_.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthMask(GL_FALSE);
        glUseProgram(progGlow_);
        setMat(progGlow_, "uVP", vp_);
        set3f(progGlow_, "uRight", right_);
        set3f(progGlow_, "uUp", up_);
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
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
        glUseProgram(progFlat_);
        setMat(progFlat_, "uVP", bvp_);
        glBindVertexArray(flatVao_);
        glBindBuffer(GL_ARRAY_BUFFER, flatVbo_);
        glBufferData(GL_ARRAY_BUFFER, flat_.size() * sizeof(float), flat_.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(flat_.size() / 11));
        if (!glass_.empty()) {
            // the cubes (depth tested among themselves): their color blends over the backing, their alpha marks the
            // pieces (kept out of the grade)
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(progGlass_);
            setMat(progGlass_, "uVP", bvp_);
            set3f(progGlass_, "uEyeB", beye_);
            // lit by the scene's sun as the world's camera sees it (board x right, y up, z toward the eye), kept a
            // little in front and above so the faces toward the eye and the tops never sink into the dark
            const vec3 sd = sky_.sunDir;
            vec3 l(dot(sd, right_), dot(sd, up_), -dot(sd, fwd_));
            l.y = std::max(l.y, 0.3f);
            l.z = std::max(l.z, 0.45f);
            l = normalize(l);
            set3f(progGlass_, "uKeyDir", l);
            // the sun's color (golden in the drop, warm at dusk; at night warm lanterns), scaled so that the faces
            // toward the eye show the piece's own color; the sky for the shade
            const vec3 sc = sky_.sunCol;
            const vec3 tint = sc / std::max(std::max(sc.x, sc.y), std::max(sc.z, 1e-3f));
            vec3 key = lerp(vec3(1.f), tint, 0.65f) * lerpf(0.82f, 1.f, saturate(sc.y / 1.5f));
            key = lerp(key, vec3(1.f, 0.78f, 0.55f) * 0.85f, night_ * 0.7f);
            set3f(progGlass_, "uKeyCol", key * (0.66f / std::max(l.z, 0.3f)));
            const vec3 st = sky_.top / std::max(std::max(sky_.top.x, sky_.top.y), std::max(sky_.top.z, 1e-3f));
            set3f(progGlass_, "uAmb", lerp(vec3(0.34f, 0.35f, 0.38f), st * 0.38f, 0.5f));
            glBindVertexArray(glassVao_);
            glBindBuffer(GL_ARRAY_BUFFER, glassVbo_);
            glBufferData(GL_ARRAY_BUFFER, glass_.size() * sizeof(GlassInst), glass_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLES, 0, cubeCount_, (GLsizei)glass_.size());
            glDisable(GL_DEPTH_TEST);
        }
        glDisable(GL_BLEND);
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
    set1f(progFinal_, "uExposure", lerpf(0.95f, 1.5f, night_) * (1.f + 0.07f * swell_));
    set1f(progFinal_, "uSat", 1.14f);
    set1f(progFinal_, "uSlow", slow_ * 0.45f);
    set3f(progFinal_, "uSlowTint", vec3(0.84f, 0.92f, 1.08f));
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.panels);
    glDisable(GL_BLEND);
    if (gpuQ) {
        const double cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpu0).count();
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
        static double acc = 0.0, cpuAcc = 0.0;
        cpuAcc += cpuMs;
        static int n = 0;
        acc += ns * 1e-6;
        if (++n == 120) {
            std::fprintf(stderr, "[islands] gpu %.2f ms/frame, cpu %.2f ms (alt %.0f m, %d islands drawn, %zu puffs)\n", acc / n, cpuAcc / n, alt_, (int)draws_.size(), puffs_.size());
            cpuAcc = 0.0;
            acc = 0.0;
            n = 0;
        }
    }
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_islands() {
    static const UniverseEntry e{
        "ISLANDS", "a flight over an endless tropical archipelago in flattened low poly, like drone footage; the pieces are low poly cubes",
        [] { return std::unique_ptr<Universe>(new IslandsUniverse()); },
        // bright, airy, flowing songs suit the flight best; it fits most songs
        [](const SongLook& l) { return 0.85f + 0.2f * l.brightness + 0.1f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
