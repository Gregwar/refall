// TERRARIUM: a sealed specimen jar on a lab bench under cold light, a tiny ecosystem growing in it, seen in 3D
// behind the well; over the song its life breaks out. The jar starts with soil, a pebble and a tuft of moss; every
// cleared line grows something (moss cushions, ferns uncurling, mushrooms, a pond filling, a sapling, flowers) and
// the plants press on the glass and fog it, until (about 15 lines at the reference pace, paceLines()) a crack runs
// through the glass, a few pieces fall out, and roots, vines and the beetle come out over the bench; by 30 the lab is
// overgrown (its benches, shelves, equipment, windows and lights swallowed, the lights failing one by one); by 45 the
// camera has pulled back out through the street window onto the city being taken over (concrete, neon smothered by
// leaves, trees bursting from roofs and the street), at nightfall. Outside, the light goes from morning through the
// golden hour to dusk and night over the same arc. The drop bursts the glass (once it is broken, a burst of spores
// and pollen) and the vines surge forward; clears release seeds that drift down and sprout where they land.
//
// The world is meshes at the full resolution (up to 1440 lines), 4x multisampled: the sky, the lab and the city and
// everything growing over them (lab.cpp: one static mesh, each part growing out of its pivot from its own line in the
// vertex shader), the jar's contents in its own units (world.cpp: the soil and its section against the glass, the
// pond, the slots' plants at their growth, rebuilt only when it changes; the sprouts and the beetle at their pose
// each frame), lit with a soft shadow map from the lab's cold key light; then the jar's glass: its far side laid over
// what is behind it (the room's faint reflection, mist, drops, vines), the world resolved with its depth, and the
// front glass marching its bent ray through that depth (refracting a little, thicker and tinted toward its edges, a
// fresnel reflection of the lab, condensation droplets that act as small lenses, trickles running down, the low mist
// integrated over the way through the jar, the crack and its hole); the glassware and the shards over it, the lights
// in the air (fireflies, seeds, spores, neon) as glows at their depth. The pieces are shards of fogged glass with
// moss growing in their cracks. Before a song the misted jar clears as the light comes up; after it the jar mists
// over again and the light goes down.
#include "universes/terrarium/terrarium.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/terrarium/shaders.hpp"

namespace {

// Fraunces Soft Italic (the song, the variant, the notices) and M PLUS Rounded 1c Medium (labels, numbers), SIL Open
// Font License, see OFL-Fraunces.txt and OFL-MPLUSRounded1c.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/terrarium/MPLUSRounded1c-Medium.ttf.inc"
};
const unsigned char FONT_ITAL[] = {
#include "universes/terrarium/Fraunces-SoftItalic.ttf.inc"
};


namespace shaders = terrarium_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;
using Slot = TerrariumUniverse::Slot;

GLuint compile(const char* vs, const char* fs, const char* name, bool scene = false) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, std::string(shaders::COMMON_GLSL) + (scene ? shaders::SCENE_GLSL : ""));
    std::string v(vs);
    v.insert(v.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(v.c_str(), f.c_str(), name);
}
constexpr int SHADOW_SIZE = 2048;

// The world's uniforms, their locations looked up once (cleared when the programs are made)
std::unordered_map<uint64_t, GLint>& locCache() { static std::unordered_map<uint64_t, GLint> c; return c; }
GLint UL(GLuint p, const char* n) {
    const uint64_t k = ((uint64_t)p << 48) ^ (uint64_t)(uintptr_t)n;
    auto it = locCache().find(k);
    if (it != locCache().end()) return it->second;
    const GLint l = glGetUniformLocation(p, n);
    locCache()[k] = l;
    return l;
}
void cset1f(GLuint p, const char* n, float v) { glUniform1f(UL(p, n), v); }
void cset1i(GLuint p, const char* n, int v) { glUniform1i(UL(p, n), v); }
void cset2f(GLuint p, const char* n, float a, float b) { glUniform2f(UL(p, n), a, b); }
void cset3f(GLuint p, const char* n, const vec3& v) { glUniform3f(UL(p, n), v.x, v.y, v.z); }
void cset4f(GLuint p, const char* n, float a, float b, float c, float d) { glUniform4f(UL(p, n), a, b, c, d); }
void csetMat(GLuint p, const char* n, const mat4& m) { glUniformMatrix4fv(UL(p, n), 1, GL_FALSE, m.m); }

vec2 V(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant (sRGB): its jar's world (0 the mossy forest, 1 the desert, 2 the tropics), its colors (the jar's, the
// beetle's, the lab's wall, the city's two neon colors), its seven pieces.
struct VariantDef {
    const char* name;
    int kind;
    unsigned soil, soil2, gravel, strata1, strata2, moss, moss2, leaf, leaf2, stem, cap, cap2, flower, flower2, bark,
        water, stone, bug, bugSpot, fly, sill, frame, wall, neonA, neonB;
    unsigned ink, accent, veil;
    unsigned piece[7];
    float pondDepth, spots;
};
const VariantDef VARIANTS[] = {
    // A mossy forest jar: deep earth, emerald moss and ferns, red toadstools, violets, a ladybird; magenta and cyan neon.
    {"MOSS", 0, 0x6a4326, 0x2e1b0e, 0x8d98a6, 0x2a2a2e, 0x46464c, 0x8ee04e, 0x2c7d2c, 0x9ae85c, 0x237a36, 0x5f9a3a,
     0xee2f2a, 0xfff2dc, 0x9a62ff, 0xf6eaff, 0x6e4a2e, 0x2a8ab0, 0x9aa8bc, 0xec2a2e, 0x15111a, 0xd8ff6e, 0x6f957e,
     0xf6f1e6, 0xbac6c4, 0xff2a8c, 0x22d8ff,
     0xf4fff8, 0xc8ff7a, 0x0b1c17,
     {0x3cc8a8, 0xf0b440, 0x9a72f0, 0x7ad04a, 0xf0603c, 0x3c84f0, 0xf086ac}, 0.07f, 1.f},
    // A desert jar: layered sand in coral and cream, succulent rosettes, aloes, cacti in flower, a turquoise scarab;
    // orange and pink neon.
    {"DESERT", 1, 0xe2ae74, 0xc48c56, 0xd8c8b4, 0xf47a66, 0xfff0d6, 0xa8c870, 0x6b8a4a, 0x8ad6b4, 0x2f8a7c, 0x8a9a5a,
     0xff7ab0, 0xffe8c0, 0xff5aa0, 0xffd27a, 0xcbb698, 0x4ab0c8, 0xe6d6c4, 0x22c4cc, 0x0d3a5c, 0xffe48a, 0xb8603c,
     0xfff6ea, 0xc0c8c6, 0xff8a1e, 0xff3ca0,
     0xfffaf2, 0xffc27a, 0x22130c,
     {0x3ccabc, 0xf4cc5c, 0xb07ae8, 0x9ad050, 0xf0703a, 0x4a7ee0, 0xf07cb4}, 0.0f, 0.f},
    // A tropical jar: rich dark earth, broad striped leaves, magenta orchids, orange caps, a jewel beetle; violet and
    // magenta neon.
    {"ORCHID", 2, 0x4e2c1a, 0x22130a, 0x7a8a72, 0x2a2a2e, 0x46464c, 0x5ad85a, 0x1d6a3a, 0x48d68c, 0x0e5a3a, 0x4a9a4a,
     0xffa83a, 0xfff0d0, 0xff48bc, 0xfff0fa, 0x5a3a28, 0x14a4a4, 0x66788a, 0x3ad46a, 0xe8c440, 0xa8fff0, 0x8a5636,
     0xeaf2ea, 0xb4c6c4, 0xa64cff, 0xff38c8,
     0xf6fff8, 0xff8ad4, 0x061d18,
     {0x30c4a4, 0xf0c440, 0x9a5cf0, 0x5ccc5a, 0xf05470, 0x3c76f0, 0xf08a40}, 0.07f, 0.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The slots: type, x, z, size, rotation, birth (lines on the 50-line scale), param.
// The forest's: the start (a pebble, a tuft of moss), then one new thing every ~3 lines, the last at 45.
const Slot SLOTS_MOSS[] = {
    {TerrariumUniverse::S_PEBBLE, 0.80f, 0.12f, 0.11f, 0.6f, -1.f, 0.f},
    {TerrariumUniverse::S_MOSS, -0.55f, 0.36f, 0.10f, 0.f, -1.f, 0.f},
    {TerrariumUniverse::S_MOSS, -0.72f, -0.10f, 0.23f, 0.f, 1.f, 0.f},
    {TerrariumUniverse::S_FERN, -0.62f, -0.45f, 0.66f, 0.3f, 3.5f, 0.f},
    {TerrariumUniverse::S_ROCK, 0.66f, 0.62f, 0.15f, 0.4f, 16.f, 1.f},
    {TerrariumUniverse::S_MUSH, 0.15f, -0.40f, 0.16f, 0.f, 28.f, 0.f},
    {TerrariumUniverse::S_FERN, 0.02f, -0.86f, 0.85f, 1.4f, 34.f, 0.f},
    {TerrariumUniverse::S_MUSH, -0.86f, -0.32f, 0.15f, 0.f, 40.f, 0.f},
    {TerrariumUniverse::S_MUSH, -0.40f, 0.56f, 0.22f, 0.f, 6.f, 0.f},
    {TerrariumUniverse::S_MOSS, 0.68f, -0.20f, 0.20f, 0.f, 9.f, 0.f},
    {TerrariumUniverse::S_TWIG, 0.56f, -0.52f, 0.50f, 0.f, 12.f, 1.f},
    {TerrariumUniverse::S_FLOWER, -0.80f, 0.20f, 0.22f, 0.f, 18.f, 0.f},
    {TerrariumUniverse::S_FERN, 0.80f, -0.40f, 0.51f, 1.1f, 21.f, 0.f},
    {TerrariumUniverse::S_MUSH, 0.86f, 0.36f, 0.16f, 0.f, 24.f, 0.f},
    {TerrariumUniverse::S_MOSS, 0.20f, 0.62f, 0.16f, 0.f, 27.f, 0.f},
    {TerrariumUniverse::S_FERN, -0.25f, -0.72f, 0.45f, 2.0f, 30.f, 0.f},
    {TerrariumUniverse::S_FLOWER, 0.40f, 0.05f, 0.18f, 0.f, 33.f, 0.f},
    {TerrariumUniverse::S_MOSS, -0.30f, 0.76f, 0.14f, 0.f, 36.f, 0.f},
    {TerrariumUniverse::S_FLOWER, -0.46f, -0.12f, 0.18f, 0.f, 39.f, 0.f},
    {TerrariumUniverse::S_MUSH, -0.18f, 0.32f, 0.14f, 0.f, 42.f, 0.f},
    {TerrariumUniverse::S_FERN, 0.28f, -0.80f, 0.42f, 0.6f, 45.f, 0.f},
};
const Slot SLOTS_DESERT[] = {
    {TerrariumUniverse::S_PEBBLE, 0.80f, 0.12f, 0.11f, 0.6f, -1.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.55f, 0.36f, 0.09f, 0.f, -1.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.72f, -0.10f, 0.18f, 0.4f, 1.f, 0.f},
    {TerrariumUniverse::S_ROCK, 0.66f, 0.62f, 0.16f, 0.4f, 16.f, 0.f},
    {TerrariumUniverse::S_CACTUS, 0.15f, -0.40f, 0.15f, 0.f, 28.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, 0.02f, -0.86f, 0.36f, 1.4f, 34.f, 1.f},
    {TerrariumUniverse::S_CACTUS, -0.86f, -0.32f, 0.11f, 0.f, 40.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.62f, -0.45f, 0.30f, 0.2f, 3.5f, 1.f},
    {TerrariumUniverse::S_CACTUS, -0.40f, 0.56f, 0.14f, 0.f, 6.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, 0.68f, -0.20f, 0.16f, 1.f, 9.f, 0.f},
    {TerrariumUniverse::S_TWIG, 0.56f, -0.52f, 0.50f, 0.f, 12.f, 0.f},
    {TerrariumUniverse::S_CACTUS, 0.50f, 0.42f, 0.16f, 0.f, 15.f, 0.f},
    {TerrariumUniverse::S_FLOWER, -0.80f, 0.20f, 0.16f, 0.f, 18.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, 0.80f, -0.40f, 0.27f, 0.5f, 21.f, 1.f},
    {TerrariumUniverse::S_CACTUS, 0.86f, 0.36f, 0.11f, 0.f, 24.f, 0.f},
    {TerrariumUniverse::S_PEBBLE, 0.20f, 0.62f, 0.09f, 2.f, 27.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.25f, -0.72f, 0.20f, 0.f, 30.f, 0.f},
    {TerrariumUniverse::S_CACTUS, 0.40f, 0.05f, 0.12f, 0.f, 33.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.30f, 0.76f, 0.12f, 0.7f, 36.f, 0.f},
    {TerrariumUniverse::S_FLOWER, -0.46f, -0.12f, 0.14f, 0.f, 39.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.18f, 0.32f, 0.11f, 0.3f, 42.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, 0.28f, -0.80f, 0.24f, 0.9f, 45.f, 1.f},
};
const Slot SLOTS_ORCHID[] = {
    {TerrariumUniverse::S_PEBBLE, 0.80f, 0.12f, 0.11f, 0.6f, -1.f, 0.f},
    {TerrariumUniverse::S_MOSS, -0.55f, 0.36f, 0.10f, 0.f, -1.f, 0.f},
    {TerrariumUniverse::S_MOSS, -0.72f, -0.10f, 0.23f, 0.f, 1.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.62f, -0.45f, 0.35f, 0.3f, 3.5f, 2.f},
    {TerrariumUniverse::S_ROCK, 0.66f, 0.62f, 0.15f, 0.4f, 16.f, 1.f},
    {TerrariumUniverse::S_MUSH, 0.15f, -0.40f, 0.15f, 0.f, 28.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, 0.02f, -0.86f, 0.38f, 1.4f, 34.f, 2.f},
    {TerrariumUniverse::S_MUSH, -0.86f, -0.32f, 0.13f, 0.f, 40.f, 0.f},
    {TerrariumUniverse::S_MUSH, -0.40f, 0.56f, 0.18f, 0.f, 6.f, 0.f},
    {TerrariumUniverse::S_MOSS, 0.68f, -0.20f, 0.20f, 0.f, 9.f, 0.f},
    {TerrariumUniverse::S_TWIG, 0.56f, -0.52f, 0.50f, 0.f, 12.f, 1.f},
    {TerrariumUniverse::S_FLOWER, -0.80f, 0.20f, 0.20f, 0.f, 18.f, 1.f},
    {TerrariumUniverse::S_FERN, 0.80f, -0.40f, 0.54f, 1.1f, 21.f, 0.f},
    {TerrariumUniverse::S_FLOWER, 0.86f, 0.36f, 0.16f, 0.f, 24.f, 1.f},
    {TerrariumUniverse::S_MOSS, 0.20f, 0.62f, 0.16f, 0.f, 27.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, -0.25f, -0.72f, 0.28f, 1.4f, 30.f, 2.f},
    {TerrariumUniverse::S_FLOWER, 0.40f, 0.05f, 0.16f, 0.f, 33.f, 0.f},
    {TerrariumUniverse::S_MOSS, -0.30f, 0.76f, 0.14f, 0.f, 36.f, 0.f},
    {TerrariumUniverse::S_FERN, -0.46f, -0.12f, 0.36f, 0.4f, 39.f, 0.f},
    {TerrariumUniverse::S_MUSH, -0.18f, 0.32f, 0.14f, 0.f, 42.f, 0.f},
    {TerrariumUniverse::S_ROSETTE, 0.28f, -0.80f, 0.27f, 0.9f, 45.f, 2.f},
};
const std::vector<Slot> SLOT_LISTS[VARIANT_COUNT] = {
    std::vector<Slot>(std::begin(SLOTS_MOSS), std::end(SLOTS_MOSS)),
    std::vector<Slot>(std::begin(SLOTS_DESERT), std::end(SLOTS_DESERT)),
    std::vector<Slot>(std::begin(SLOTS_ORCHID), std::end(SLOTS_ORCHID)),
};
constexpr float SLOT_SPAN = 3.f;   // lines a slot takes to grow
constexpr float BIRTH_SCALE = 0.3f; // the tables' births (0..45) -> the jar full by 16 lines, as it cracks
constexpr float POND_AT = 5.f;     // the pond fills from here, over 4 lines


// The camera's keys (on the 50-line scale): close on the sealed jar; the crack; the beetle and the vines out on the
// bench; the bench and its shelf; the lab; back toward the street window and out through it; the facade; the city.
struct CamKey { float lines; vec3 eye, target; };
const CamKey CAMS[] = {
    {0.f, {0.f, 0.8f, 2.9f}, {0.f, 0.36f, 0.f}},          // close on the sealed jar
    {10.f, {0.f, 0.9f, 3.4f}, {0.f, 0.4f, 0.f}},          // the whole jar, fogging
    {15.5f, {0.1f, 0.92f, 3.6f}, {0.1f, 0.4f, 0.f}},       // the crack, on its left flank
    {20.f, {-0.5f, 1.3f, 5.0f}, {-0.25f, 0.3f, 0.2f}},     // out over the bench
    {24.5f, {-1.0f, 2.4f, 8.8f}, {-0.9f, 0.6f, -0.2f}},    // the bench, the microscope swallowed
    {29.f, {0.f, 5.2f, 23.f}, {0.f, 2.4f, -6.f}},           // the bench and its shelf, the back windows
    {33.5f, {0.f, 5.5f, 27.f}, {0.f, 3.0f, -8.f}},          // the lab
    {37.f, {0.f, 4.6f, 31.6f}, {0.f, 3.4f, -6.f}},          // back toward the street window
    {40.f, {0.f, 5.0f, 45.f}, {0.f, 4.2f, 0.f}},            // out through it
    {44.f, {0.f, 30.f, 175.f}, {0.f, 6.f, 0.f}},            // the facade, the street
    {50.f, {0.f, 70.f, 780.f}, {0.f, 105.f, -200.f}},       // the city at night, its skyline
};
constexpr int CAM_KEYS = (int)(sizeof(CAMS) / sizeof(CAMS[0]));
vec3 catmull(const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float t) {
    const float t2 = t * t, t3 = t2 * t;
    return (p1 * 2.f + (p2 - p0) * t + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2 + (p1 * 3.f - p0 - p2 * 3.f + p3) * t3) * 0.5f;
}

// the jar's pond: center, radius, depth
struct PondDef { float x, z, r, depth; };
// the soil (the shader's soilH)
float soilAt(float x, float z, const PondDef& pd, float pondFill) {
    float h = 0.2f;
    const float ax = x + 0.5f, az = z + 0.45f, bx = x - 0.55f, bz = z + 0.5f, cx = x, cz = z + 0.72f;
    h += 0.055f * std::exp(-(ax * ax + az * az) * 5.f) + 0.045f * std::exp(-(bx * bx + bz * bz) * 6.f) +
         0.035f * std::exp(-(cx * cx + cz * cz) * 4.f);
    h -= 0.02f * z;
    h += 0.006f * std::sin(x * 9.f + 1.f) * std::sin(z * 8.f + 2.f);
    const float px = x - pd.x, pz = z - pd.z;
    const float dp = std::sqrt(px * px + pz * pz) / pd.r;
    h -= pd.depth * smoothstepf(0.f, 1.f, saturate(pondFill * 2.f)) * (1.f - smoothstepf(0.5f, 1.15f, dp));
    return h;
}

using Palette = TerrariumUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

int wrapV(int v) { return ((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT; }
Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[wrapV(v)];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    const unsigned* src = &d.soil;
    vec3* dst = &p.soil;
    for (int i = 0; i < 25; i++) dst[i] = L(src[i]);
    p.ink = L(d.ink); p.accent = L(d.accent); p.veil = L(d.veil);
    for (int i = 0; i < 7; i++) p.piece[i] = L(d.piece[i]);
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

// The day outside: morning, afternoon, golden hour, sunset, dusk, night (sRGB colors and their intensities).
struct Sky {
    unsigned zen, hor, sun, glow, ambS, ambG, fill;
    float sunI, glowI, ambSI, ambGI, fillI, elev, disc, moon, stars, night;
};
// Neighbours keep close hues, so that no blend between them goes grey.
constexpr int SKY_KEYS = 6;
const Sky SKIES[SKY_KEYS] = {
    // a pale overcast morning
    {0x5a86b8, 0xc4d6e2, 0xf4f4f0, 0xe8eef0, 0x9ab4cc, 0x8a8478, 0xe6ecf0, 1.6f, 0.35f, 0.6f, 0.32f, 0.4f, 0.8f, 0.f, 0.f, 0.f, 0.f},
    // afternoon
    {0x3f7cc8, 0xd8e4e4, 0xfff0d0, 0xffe8b8, 0xa8c0e0, 0x8a7a64, 0xffe8cc, 2.0f, 0.5f, 0.55f, 0.3f, 0.4f, 0.42f, 0.6f, 0.f, 0.f, 0.f},
    // golden hour
    {0x4a6cc0, 0xffb860, 0xffa848, 0xffa040, 0xe0b090, 0x8a5a40, 0xffc890, 2.2f, 1.2f, 0.45f, 0.26f, 0.36f, 0.12f, 1.f, 0.f, 0.f, 0.05f},
    // sunset
    {0x5248b0, 0xff7450, 0xff7040, 0xff6838, 0xd08aa8, 0x6a3a50, 0xffa080, 1.6f, 1.2f, 0.38f, 0.2f, 0.28f, 0.03f, 1.f, 0.f, 0.05f, 0.2f},
    // dusk
    {0x262a7a, 0xc06a9c, 0xff6a6a, 0xe0608a, 0x8a70d0, 0x4a3060, 0xffa0a0, 0.6f, 0.6f, 0.3f, 0.15f, 0.18f, -0.04f, 0.2f, 0.4f, 0.4f, 0.6f},
    // night: the city's own glow low in the sky, violet
    {0x0a1238, 0x3a2a6a, 0x9ab0ff, 0x6a5ac0, 0x4a5ab0, 0x201a40, 0x6070c0, 0.45f, 0.2f, 0.2f, 0.09f, 0.1f, 0.5f, 0.f, 1.f, 1.f, 1.f},
};
// The time of day from the lines (50-line scale): morning until 12, then afternoon (21), golden hour (29), sunset
// (35), dusk (41), night (46).
float todOf(float lines) {
    const float K[SKY_KEYS] = {12.f, 21.f, 29.f, 35.f, 41.f, 46.f};
    if (lines <= K[0]) return 0.f;
    for (int i = 1; i < SKY_KEYS; i++)
        if (lines < K[i]) return (float)(i - 1) + smoothstepf(K[i - 1], K[i], lines);
    return (float)(SKY_KEYS - 1);
}
// The story's stages (closing the HUD's corner line)
const char* stageName(int s) {
    static const char* N[] = {"sealed", "cracked", "loose", "overgrown", "the city"};
    return N[std::clamp(s, 0, 4)];
}

constexpr float CLEAR_T = 0.42f; // a cleared row shrinks away in this long (real time)
constexpr int MAX_FLIES = 32, MAX_SEEDS = 32, MAX_SPROUTS = 40, MAX_MOTES = 400, MAX_SPORES = 900, MAX_SHARDS = 90;
constexpr float JAR_IN = 0.84f;   // where things may stand (the floor's radius, less a margin)
// the story's lines (50-line scale): the crack spreading, the hole opening, the lab's lights failing
constexpr float CRACK_AT = 14.5f, HOLE_AT = 16.8f, ESCAPE_AT = 17.6f;

// Test hooks: REFALL_TERRARIUM_LINES=N adds N lines, REFALL_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("REFALL_TERRARIUM_LINES") ? std::atoi(std::getenv("REFALL_TERRARIUM_LINES")) : 0;
    static const double rate = std::getenv("REFALL_GROW_RATE") ? std::atof(std::getenv("REFALL_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

// the camera (world units: the jar's radius)
constexpr float TAN_H = 0.212f;
// the crack's place: toward it on the glass, its foot at the soil
const vec3 CRACK_DIR(std::sin(terra::lab::CRACK_U), 0.f, std::cos(terra::lab::CRACK_U));
float hash01(int i) { return (float)((splitmix64((uint64_t)i) >> 40) & 0xFFFFFF) / 16777216.f; }

// a neon sign's flicker (the shader's neonFlick, for its glows)
float neonFlick(float s, float t, float flicker) {
    const float h = terra::ghash1(s * 7.31f + 0.4f);
    const float buzz = (h >= 0.72f ? 1.f : 0.f) * (terra::ghash1(std::floor(t * 14.f) + s * 3.7f) >= 0.8f ? 1.f : 0.f);
    float fr = t * 0.11f + h * 5.f;
    fr -= std::floor(fr);
    const float dead = (h >= 0.9f ? 1.f : 0.f) * (fr >= 0.45f ? 1.f : 0.f) * (terra::ghash1(std::floor(t * 9.f) + s) >= 0.3f ? 1.f : 0.f);
    float f = 1.f - 0.75f * buzz - 0.85f * dead;
    f *= 1.f - flicker * 0.8f * (terra::ghash1(std::floor(t * 22.f) + s * 1.3f) >= 0.45f ? 1.f : 0.f);
    return f;
}

} // namespace

const std::vector<Slot>& TerrariumUniverse::slots() const { return SLOT_LISTS[wrapV(variant_)]; }

// =====================================================================================================
// Setup
// =====================================================================================================

bool TerrariumUniverse::init(int width, int height) {
    locCache().clear();
    progLab_ = compile(shaders::MESH_VS, shaders::LAB_FS, "terrarium lab", true);
    progSky_ = compile(shaders::SKY_VS, shaders::SKY_FS, "terrarium sky", true);
    progContent_ = compile(shaders::MESH_VS, shaders::CONTENT_FS, "terrarium content", true);
    progShadow_ = compile(shaders::MESH_VS, shaders::SHADOW_FS, "terrarium shadow");
    progGlass_ = compile(shaders::MESH_VS, shaders::GLASS_FS, "terrarium glass", true);
    progTube_ = compile(shaders::MESH_VS, shaders::TUBE_FS, "terrarium glassware", true);
    progGlow_ = compile(shaders::GLOW_VS, shaders::GLOW_FS, "terrarium glow");
    progComp_ = compile(ugl::FULLSCREEN_VS, shaders::COMP_FS, "terrarium comp");
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "terrarium sprite");
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "terrarium veil");
    progCell_ = compile(shaders::CELL_VS, shaders::CELL_FS, "terrarium cell");
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "terrarium shape");
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "terrarium post");

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
    instanced(spriteVao_, spriteVbo_, 3, sizeof(SpriteInst));
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    // the lights in the air: a quad, one instance each
    instanced(glowVao_, glowVbo_, 2, sizeof(float) * 8);
    glBindVertexArray(0);

    // ---- the fixed meshes: the lab, the city and their growth (one), the glassware, the jar's glass, its cork
    {
        std::vector<terra::Vtx> v;
        v.reserve(1 << 20);
        terra::buildLab(v);
        const size_t nLab = v.size();
        terra::buildCity(v, neon_, lights_, blooms_);
        const size_t nCity = v.size();
        worldLab_ = nLab;
        worldGrowth_ = v.size();
        terra::buildGrowth(v, blooms_, &worldBench_);
        upload(world_, v, false);
        if (std::getenv("REFALL_GPUTIME"))
            std::fprintf(stderr, "[terrarium] world: %zu vertices (lab %zu, city %zu, growth %zu), %zu neon points, %zu blooms\n", v.size(), nLab,
                         nCity - nLab, v.size() - nCity, neon_.size(), blooms_.size());
        v.clear();
        terra::buildLabGlass(v);
        upload(labGlass_, v, false);
        v.clear();
        terra::buildGlass(v, false);
        upload(glassOut_, v, false);
        v.clear();
        terra::buildGlass(v, true);
        upload(glassIn_, v, false);
        v.clear();
        terra::buildCork(v);
        upload(cork_, v, false);
    }

    // ---- the key light's shadow map
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
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

    pal_ = basePal_ = paletteOf(0);

    // the fireflies: homes all over the jar, most of them beside the well
    Rng r(0xF1F1);
    for (int i = 0; i < MAX_FLIES; i++) {
        Fly f;
        float x, z;
        for (int k = 0; k < 20; k++) {
            const float a = r.range(0.f, TAU), rr = std::sqrt(r.uniform()) * 0.78f;
            x = std::cos(a) * rr;
            z = std::sin(a) * rr;
            if (std::fabs(x) > 0.34f || r.chance(0.25f)) break;
        }
        f.home = vec3(x, r.range(0.3f, 0.74f), z);
        f.p = f.home;
        f.phase = r.range(0.f, TAU);
        f.freq = r.range(0.5f, 1.2f);
        f.seed = r.uniform();
        f.glow = 0.f;
        f.on = 0.f;
        flies_.push_back(f);
    }
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void TerrariumUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * (float)width_ / height_;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    pxPerUnit_ = height_ / (2.f * hh);
    // the world: multisampled at the full resolution (up to 1440 lines), resolved with its depth twice: once under
    // the jar's front glass (which reads it), once whole
    msH_ = std::max(180, std::min(1440, height_));
    msW_ = std::max(320, (int)std::lround((float)msH_ * width_ / height_));
    if (!msFbo_) { glGenFramebuffers(1, &msFbo_); glGenRenderbuffers(1, &msColor_); glGenRenderbuffers(1, &msDepth_); }
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGBA16F, msW_, msH_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_DEPTH_COMPONENT32F, msW_, msH_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[terrarium] multisampled framebuffer incomplete (%dx%d x%d)\n", msW_, msH_, samples_);
    auto depthTex = [&](GLuint& t, ugl::Target& tg) {
        tg.create(msW_, msH_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
        if (!t) glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, msW_, msH_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, tg.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, t, 0);
    };
    depthTex(worldDepth_, worldT_);
    depthTex(backDepth_, backT_);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    updateCamera();
}

void TerrariumUniverse::updateCamera(float lines) {
    aspect_ = (float)width_ / height_;
    const float L = std::clamp(lines, 0.f, CAMS[CAM_KEYS - 1].lines);
    int k = 0;
    while (k < CAM_KEYS - 2 && L > CAMS[k + 1].lines) k++;
    const float u = (L - CAMS[k].lines) / (CAMS[k + 1].lines - CAMS[k].lines);
    const int k0 = std::max(0, k - 1), k3 = std::min(CAM_KEYS - 1, k + 2);
    const vec3 eye = catmull(CAMS[k0].eye, CAMS[k].eye, CAMS[k + 1].eye, CAMS[k3].eye, u);
    const vec3 target = catmull(CAMS[k0].target, CAMS[k].target, CAMS[k + 1].target, CAMS[k3].target, u);
    eye_ = eye;
    fwd_ = normalize(target - eye);
    right_ = normalize(cross(fwd_, vec3(0.f, 1.f, 0.f)));
    up_ = cross(right_, fwd_);
    // a wider lens in the lab (the whole room), the long one again outside
    tanH_ = TAN_H * (1.f + 0.42f * smoothstepf(25.f, 31.f, L) * (1.f - smoothstepf(38.5f, 44.f, L)));
    // the depth's range follows the distance to what we look at (close on the jar, far over the city)
    const float d = length(target - eye);
    near_ = std::clamp(d * 0.012f, 0.03f, 4.f);
    far_ = 6000.f;
}

bool TerrariumUniverse::toScreen(const vec3& w, vec2& b) const {
    const vec3 v = w - eye_;
    const float z = dot(v, fwd_);
    if (z < 0.01f) return false;
    const float nx = dot(v, right_) / (z * tanH_ * aspect_), ny = dot(v, up_) / (z * tanH_);
    b = V(viewC_.x + nx * viewH_.x, viewC_.y + ny * viewH_.y);
    return true;
}

vec3 TerrariumUniverse::boardRay(vec2 b) const {
    const float nx = (b.x - viewC_.x) / viewH_.x, ny = (b.y - viewC_.y) / viewH_.y;
    return normalize(fwd_ + right_ * (nx * tanH_ * aspect_) + up_ * (ny * tanH_));
}

TerrariumUniverse::~TerrariumUniverse() {
    for (ugl::Target* t : {&worldT_, &backT_, &sceneT_}) t->destroy();
    for (GLuint p : {progLab_, progSky_, progContent_, progShadow_, progGlass_, progTube_, progGlow_, progComp_, progSprite_, progVeil_,
                     progCell_, progShape_, progPost_})
        glDeleteProgram(p);
    for (GLuint v : {spriteVao_, cellVao_, shapeVao_, glowVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, cellVbo_, shapeVbo_, glowVbo_}) glDeleteBuffers(1, &b);
    auto del = [](Mesh& m) {
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
    };
    for (Mesh* m : {&world_, &labGlass_, &glassOut_, &glassIn_, &cork_, &sproutMesh_, &beetle_, &shardMesh_}) del(*m);
    del(jarMesh_.mesh);
    del(jarMesh_.soilMesh);
    for (Mesh& m : jarMesh_.slot) del(m);
    for (GLuint t : {worldDepth_, backDepth_, shadowTex_})
        if (t) glDeleteTextures(1, &t);
    for (GLuint fb : {msFbo_, shadowFbo_})
        if (fb) glDeleteFramebuffers(1, &fb);
    for (GLuint rb : {msColor_, msDepth_})
        if (rb) glDeleteRenderbuffers(1, &rb);
}

void TerrariumUniverse::upload(Mesh& m, const std::vector<terra::Vtx>& v, bool dynamic) {
    if (!m.vao) {
        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);
        glBindVertexArray(m.vao);
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        const GLsizei st = sizeof(terra::Vtx);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)offsetof(terra::Vtx, p));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, st, (void*)offsetof(terra::Vtx, n));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(terra::Vtx, m));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(terra::Vtx, f));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(terra::Vtx, s));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, st, (void*)offsetof(terra::Vtx, g));
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(terra::Vtx), v.empty() ? nullptr : v.data(), dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    m.count = (int)v.size();
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void TerrariumUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.brightness > 0.6f && t.airWeight > 0.5f) v = 1;            // the desert for bright, airy songs
        else if (t.hasSong && t.bassWeight > 0.6f && t.dynamics > 0.5f) v = 2;      // the tropics for warm, heavy ones
        else if (t.hasSong && t.minor && t.brightness < 0.5f) v = 0;                 // the mossy forest for quiet ones
        else {
            static const int BY_HUE[] = {0, 2, 0, 1, 2, 0};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
    }
    if (v != variant_) growth_.clear();
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x7E44Aull);
    tagNo_ = 1 + (int)(splitmix64(seed_ ^ 0x7A6ull) % 98);
    if (seconds <= 0.f) palSet_ = false;
}

void TerrariumUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

void TerrariumUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void TerrariumUniverse::refresh(float strength) { swell_ = std::max(swell_, 0.3f * strength); }

vec2 TerrariumUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints TerrariumUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    const float tod = tod_;
    const Sky& s = SKIES[std::clamp((int)std::lround(tod), 0, SKY_KEYS - 1)];
    h.background = lerp(ugl::toLinear(hex(s.hor)) * 0.5f, p.leaf2, 0.4f) * (tod > 4.f ? 0.4f : 0.7f);
    h.light = false;
    h.ink = p.ink;
    h.accent = p.fly;
    h.styles = TS_RIPPLE | TS_INK | TS_LUMA;
    return h;
}

// =====================================================================================================
// The soil, places to stand
// =====================================================================================================

float TerrariumUniverse::soilHeight(float x, float z) const {
    return soilAt(x, z, PondDef{0.5f, 0.42f, 0.19f, VARIANTS[wrapV(variant_)].pondDepth}, pond_);
}

bool TerrariumUniverse::blocked(float x, float z, float r) const {
    if (x * x + z * z > (bugGoing_ ? 0.86f : JAR_IN) * (bugGoing_ ? 0.86f : JAR_IN)) return true;
    const std::vector<Slot>& sl = slots();
    for (size_t i = 0; i < sl.size(); i++) {
        const float g = i < growth_.size() ? growth_[i] : 0.f;
        if (g <= 0.02f) continue;
        const Slot& s = sl[i];
        float rad;
        switch (s.type) {
        case TerrariumUniverse::S_MOSS: rad = s.size * 0.95f * g; break;
        case TerrariumUniverse::S_FERN: rad = 0.07f; break;
        case TerrariumUniverse::S_MUSH: rad = s.size * 0.75f; break;
        case TerrariumUniverse::S_TWIG: rad = 0.06f; break;
        case TerrariumUniverse::S_FLOWER: rad = s.size * 0.6f; break;
        case TerrariumUniverse::S_ROSETTE: rad = s.size * (s.param > 1.5f ? 0.3f : 0.9f); break;
        case TerrariumUniverse::S_CACTUS: rad = s.size * 0.7f; break;
        default: rad = s.size * 1.05f; break;
        }
        const float dx = x - s.x, dz = z - s.z;
        if (dx * dx + dz * dz < (rad + r) * (rad + r)) return true;
    }
    if (VARIANTS[wrapV(variant_)].pondDepth > 0.f && pond_ > 0.05f) {
        const float dx = x - 0.5f, dz = z - 0.42f;
        if (dx * dx + dz * dz < (0.2f + r) * (0.2f + r)) return true;
    }
    return false;
}

// A free spot on the soil, beside the well on screen.
bool TerrariumUniverse::pickSpot(float& x, float& z, float clearance) {
    for (int k = 0; k < 40; k++) {
        const float a = rng_.range(0.f, TAU), rr = std::sqrt(rng_.uniform()) * JAR_IN;
        const float cx = std::cos(a) * rr, cz = std::sin(a) * rr;
        if (blocked(cx, cz, clearance)) continue;
        vec2 b;
        if (!toScreen(vec3(cx, soilHeight(cx, cz), cz), b)) continue;
        if (std::fabs(b.x) < 5.8f && k < 30) continue;
        bool nearSprout = false;
        for (const Sprout& s : sprouts_)
            if ((s.x - cx) * (s.x - cx) + (s.z - cz) * (s.z - cz) < 0.045f * 0.045f) nearSprout = true;
        if (nearSprout && k < 34) continue;
        x = cx;
        z = cz;
        return true;
    }
    return false;
}

// A free spot on the bench around the jar (once it is broken), beside the well on screen.
bool TerrariumUniverse::pickBenchSpot(vec3& at) {
    for (int k = 0; k < 30; k++) {
        const float a = rng_.range(0.f, TAU), rr = rng_.range(1.15f, 3.2f);
        const vec3 p(std::sin(a) * rr, 0.f, std::cos(a) * rr);
        if (p.z > 1.45f || p.z < -2.2f) continue;
        vec2 b;
        if (!toScreen(p, b)) continue;
        if (std::fabs(b.x) < 5.8f || std::fabs(b.x - viewC_.x) > viewH_.x * 0.95f || std::fabs(b.y - viewC_.y) > viewH_.y * 0.9f) continue;
        at = p;
        return true;
    }
    return false;
}

// =====================================================================================================
// Life
// =====================================================================================================

void TerrariumUniverse::addSeed(const vec3& from) {
    if ((int)seeds_.size() >= MAX_SEEDS) return;
    Seed s;
    s.p = from;
    s.v = vec3(0.f);
    s.target = 0;
    vec3 at;
    if (hole_ > 0.5f && pickBenchSpot(at)) { s.to = at; s.target = 1; }
    else {
        float x = 0.f, z = 0.f;
        if (!pickSpot(x, z, 0.03f)) return;
        s.to = vec3(x, soilHeight(x, z), z);
    }
    s.age = -rng_.range(0.f, 0.4f);
    s.life = rng_.range(4.5f, 6.5f);
    s.seed = rng_.uniform();
    s.landed = false;
    seeds_.push_back(s);
}

// Spores and pollen: from the crack while the camera is near it, from the flowers and the crowns already grown that
// are in view
void TerrariumUniverse::sporeBurst(int n, float speed) {
    std::vector<int> vis;
    for (int i = 0; i < (int)blooms_.size(); i++) {
        const terra::Bloom& b = blooms_[i];
        const float L = b.appear < terra::lab::GROUP_B ? linesA_ : linesB_;
        if (L < b.appear) continue;
        vec2 s;
        if (!toScreen(b.p, s)) continue;
        if (std::fabs(s.x - viewC_.x) > viewH_.x * 1.05f || std::fabs(s.y - viewC_.y) > viewH_.y * 1.05f) continue;
        const float dist = length(b.p - eye_);
        if (b.scale < dist * 0.004f) continue;
        vis.push_back(i);
    }
    const bool fromCrack = hole_ > 0.3f && length(eye_) < 14.f;
    for (int k = 0; k < n && (int)spores_.size() < MAX_SPORES; k++) {
        Spore s;
        vec3 at, dir;
        float sz;
        if ((fromCrack && (k % 3 != 2 || vis.empty())) || vis.empty()) {
            if (!fromCrack) return;
            at = CRACK_DIR * 1.02f + vec3(0.f, terra::lab::CRACK_V + rng_.range(-0.1f, 0.1f), 0.f) +
                 vec3(std::cos(terra::lab::CRACK_U), 0.f, -std::sin(terra::lab::CRACK_U)) * rng_.range(-0.15f, 0.15f);
            dir = normalize(CRACK_DIR + vec3(rng_.range(-0.6f, 0.6f), rng_.range(0.2f, 1.2f), rng_.range(-0.6f, 0.6f)));
            sz = 1.f;
        } else {
            const terra::Bloom& b = blooms_[vis[rng_.irange(0, (int)vis.size() - 1)]];
            at = b.p;
            dir = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(0.3f, 1.4f), rng_.range(-0.5f, 1.f)));
            sz = std::max(1.f, b.scale / 0.08f);
        }
        s.p = at;
        s.v = dir * (speed * rng_.range(0.4f, 1.3f) * 0.6f * std::sqrt(sz));
        s.age = -rng_.range(0.f, 0.3f);
        s.life = rng_.range(3.f, 6.5f);
        s.size = sz;
        s.seed = rng_.uniform();
        spores_.push_back(s);
    }
}

// The glass's pieces: from the hole (falling out, or flying at a burst), each a jagged little triangle of glass
void TerrariumUniverse::spawnShards(int n, float speed, bool landed) {
    for (int k = 0; k < n && (int)shards_.size() < MAX_SHARDS; k++) {
        Shard s;
        const float u = terra::lab::CRACK_U + rng_.range(-0.13f, 0.13f), v = terra::lab::CRACK_V + rng_.range(-0.11f, 0.11f);
        const vec3 d(std::sin(u), 0.f, std::cos(u));
        s.p = d * 1.0f + vec3(0.f, v, 0.f);
        s.v = (d * rng_.range(0.3f, 1.f) + vec3(rng_.range(-0.4f, 0.4f), rng_.range(0.f, 0.8f), rng_.range(-0.4f, 0.4f))) * speed;
        s.ax = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f)));
        s.ang = rng_.range(0.f, TAU);
        s.spin = rng_.range(-12.f, 12.f) * (0.3f + speed);
        s.size = rng_.range(0.03f, 0.085f);
        s.seed = rng_.uniform();
        s.rest = false;
        if (landed) {
            const float a = u + rng_.range(-0.5f, 0.5f), r = rng_.range(1.08f, 1.6f + speed * 0.5f);
            s.p = vec3(std::sin(a) * r, 0.003f, std::cos(a) * r);
            s.rest = true;
        }
        shards_.push_back(s);
    }
}

// =====================================================================================================
// The jar
// =====================================================================================================

// The jar as it is drawn this frame: its palette, pond, moss, vines and its slots at their growth.
void TerrariumUniverse::computeJar(const std::vector<Slot>& sl, float after) {
    const VariantDef& vd = VARIANTS[wrapV(variant_)];
    JarState& J = jar_;
    const vec3* pc = &pal_.soil;
    for (int i = 0; i < 17; i++) { J.pal[i * 3] = pc[i].x; J.pal[i * 3 + 1] = pc[i].y; J.pal[i * 3 + 2] = pc[i].z; }
    const PondDef pd{0.5f, 0.42f, 0.19f, vd.pondDepth};
    const float pondFill = pond_;
    J.pd = terra::Pond{pd.x, pd.z, pd.r, pd.depth * smoothstepf(0.f, 1.f, saturate(pondFill * 2.f))};
    J.level = 0.2f - 0.0084f - pd.depth * (1.f - 0.85f * pondFill) * 0.6f - 0.004f;
    J.fill = pondFill;
    J.moss = moss_;
    J.kind = vd.kind;
    J.vine = inVine_;
    const int n = std::min(24, (int)sl.size());
    J.slots.resize(n);
    for (int i = 0; i < n; i++) {
        const Slot& s = sl[i];
        terra::SlotDraw& d = J.slots[i];
        d.type = s.type;
        d.base = vec3(s.x, soilAt(s.x, s.z, pd, pondFill), s.z);
        d.size = s.size * after;
        d.g = std::max(0.f, i < (int)growth_.size() ? growth_[i] : 0.f);
        d.seed = (float)i * 1.37f + 0.5f;
        d.rot = s.rot;
        d.param = s.param;
    }
}

// The jar's mesh: its soil, its pond and its slots, each slot rebuilt only when what it was built for changed (its
// growth by more than a hair, its soil, its size; a rock's trickle with the pond).
void TerrariumUniverse::updateJarMesh() {
    const JarState& J = jar_;
    JarMesh& M = jarMesh_;
    bool dirty = M.dirty;
    if (std::fabs(M.soilDepth - J.pd.depth) > 0.0004f || (M.soilDepth != J.pd.depth && (J.pd.depth == 0.f || J.fill >= 1.f)) ||
        M.soilKind != J.kind) {
        std::vector<terra::Vtx> soil;
        terra::buildSoil(soil, J.pd, J.kind);
        upload(M.soilMesh, soil, true);
        M.soilDepth = J.pd.depth;
        M.soilKind = J.kind;
    }
    const bool pond = J.fill > 0.001f;
    if (pond != M.hasPond || (pond && std::fabs(M.pondLevel - J.level) > 1e-5f)) {
        M.hasPond = pond;
        M.pondLevel = J.level;
        dirty = true;
    }
    const int n = (int)J.slots.size();
    if ((int)M.slot.size() != n) {
        M.slot.resize(n);
        M.built.assign(n, terra::SlotDraw{-1, vec3(0.f), 0.f, -1.f, 0.f, 0.f, 0.f});
        M.builtPond.assign(n, -9.f);
    }
    const terra::PondState ps{J.pd, J.level, J.fill};
    std::vector<terra::Vtx> v;
    for (int i = 0; i < n; i++) {
        const terra::SlotDraw& s = J.slots[i];
        const terra::SlotDraw& b = M.built[i];
        const float pondKey = s.type == S_ROCK ? (J.fill > 0.3f ? J.level : -1.f) : 0.f;
        const bool same = b.type == s.type && std::fabs(b.g - s.g) < (s.g >= 1.f || s.g <= 0.f ? 1e-6f : 0.008f) &&
                          std::fabs(b.base.y - s.base.y) < 2e-4f && std::fabs(b.size - s.size) < 2e-4f && b.seed == s.seed &&
                          std::fabs(M.builtPond[i] - pondKey) < 2e-4f;
        if (same) continue;
        v.clear();
        terra::buildSlot(v, s, ps);
        upload(M.slot[i], v, true);
        M.built[i] = s;
        M.builtPond[i] = pondKey;
    }
    if (!dirty) return;
    v.clear();
    if (M.hasPond) terra::buildPond(v, J.pd, J.level);
    upload(M.mesh, v, true);
    M.dirty = false;
}

// =====================================================================================================
// The beetle
// =====================================================================================================

// The beetle's excursion, once the glass is broken: from inside the crack over its lip, a hop down onto the bench, a
// walk out, a long look at us, a wander, another look, back to the crack, a hop up, home. (World units: the jar's.)
void TerrariumUniverse::updateExcursion(float dt) {
    const float cu = terra::lab::CRACK_U;
    auto at = [&](float da, float r) { return vec3(std::sin(cu + da) * r, 0.f, std::cos(cu + da) * r); };
    const vec3 inside = at(0.f, 0.74f) + vec3(0.f, soilHeight(at(0.f, 0.74f).x, at(0.f, 0.74f).z) - 0.002f, 0.f);
    const vec3 lip = at(0.f, 1.0f) + vec3(0.f, 0.205f, 0.f);
    const vec3 foot = at(0.f, 1.2f);
    const vec3 x1 = at(-0.55f, 1.75f), x2 = at(0.25f, 2.1f);
    auto headOf = [](const vec3& d) { return std::atan2(-d.x, d.z); };
    auto turnTo = [&](float want, float rate) {
        float dh = want - bugHead_;
        while (dh > PI) dh -= TAU;
        while (dh < -PI) dh += TAU;
        bugHead_ += std::clamp(dh, -rate * dt, rate * dt);
        return std::fabs(dh);
    };
    // the script: (from, to, seconds, kind: 0 walk, 1 hop, 2 look)
    struct Step { vec3 a, b; float T; int kind; };
    const Step S[] = {
        {inside, lip, 2.2f, 0}, {lip, foot, 0.75f, 1}, {foot, x1, 6.f, 0}, {x1, x1, 5.f, 2}, {x1, x2, 6.f, 0},
        {x2, x2, 4.f, 2}, {x2, foot, 6.f, 0}, {foot, lip, 0.75f, 1}, {lip, inside, 2.2f, 0},
    };
    constexpr int NS = (int)(sizeof(S) / sizeof(S[0]));
    if (bugMode_ != 1) return;
    exT_ += dt;
    const Step& s = S[exPhase_];
    const float t = saturate(exT_ / s.T);
    if (s.kind == 0) {
        const float e = t * t * (3.f - 2.f * t);
        bugP_ = lerp(s.a, s.b, e);
        if (s.a.y < 0.05f && s.b.y < 0.05f) bugP_.y = 0.f;
        turnTo(headOf(s.b - s.a), 3.f);
        bugSpeed_ = length(s.b - s.a) / s.T * 1.5f * (1.f - std::fabs(t * 2.f - 1.f)) + 0.006f;
        bugLook_ = approach(bugLook_, 0.2f * std::sin(exT_ * 0.8f), 2.f, dt);
    } else if (s.kind == 1) {
        bugP_ = lerp(s.a, s.b, t) + vec3(0.f, std::sin(t * PI) * 0.09f, 0.f);
        turnTo(headOf(vec3(s.b.x - s.a.x, 0.f, s.b.z - s.a.z)), 4.f);
        bugSpeed_ = 0.02f;
    } else {
        // a long look at us (turning to face us, shuffling), then around
        const vec3 toEye = eye_ - bugP_;
        const float off = turnTo(headOf(toEye), 0.9f);
        bugSpeed_ = off > 0.3f ? 0.012f : 0.f;
        bugLook_ = approach(bugLook_, 0.35f * std::sin(exT_ * 0.9f) * smoothstepf(1.5f, 2.5f, exT_), 2.f, dt);
    }
    if (exT_ >= s.T) {
        exT_ = 0.f;
        exPhase_++;
        if (exPhase_ >= NS) {
            bugMode_ = 0;
            exPhase_ = 0;
            bugCool_ = rng_.range(18.f, 35.f);
            bug_.x = inside.x; bug_.z = inside.z; bug_.head = bugHead_; bug_.pause = 1.5f; bug_.speed = 0.f; bug_.lookT = 0.f;
        }
    }
}

void TerrariumUniverse::updateBeetle(float dt, float wm, float wp) {
    Beetle& b = bug_;
    (void)wp;
    auto wrapA = [](float a) {
        while (a > PI) a -= TAU;
        while (a < -PI) a += TAU;
        return a;
    };
    const float dx = b.tx - b.x, dz = b.tz - b.z;
    const float dist = std::sqrt(dx * dx + dz * dz);
    float want = 0.f, lookWant = 0.f;
    if (b.pause > 0.f) {
        b.pause -= dt;
        b.lookT += dt;
        // stopped: it turns round to face us (shuffling its feet), looks at us, then around
        const float toEye = std::atan2(-(eye_.x - b.x), eye_.z - b.z);
        const float turn = wrapA(toEye - b.head);
        if (b.lookT > 0.4f && std::fabs(turn) > 0.35f) {
            b.head = wrapA(b.head + std::clamp(turn, -0.9f * dt, 0.9f * dt));
            b.walk += dt * 9.f;
        }
        lookWant = std::clamp(wrapA(toEye - b.head), -0.75f, 0.75f) * (b.pause > 1.2f ? 1.f : 0.f);
        lookWant += 0.3f * std::sin(b.lookT * 0.8f) * smoothstepf(2.f, 3.5f, b.lookT);
        if (b.pause <= 0.f) {
            float x, z;
            // once the glass is broken it sometimes heads for the crack, and out
            if (hole_ > 0.9f && bugCool_ <= 0.f && rng_.chance(0.6f)) {
                b.tx = std::sin(terra::lab::CRACK_U) * 0.74f; b.tz = std::cos(terra::lab::CRACK_U) * 0.74f; bugGoing_ = true;
            } else if (pickSpot(x, z, 0.07f)) { b.tx = x; b.tz = z; bugGoing_ = false; }
        }
    } else if (dist < 0.03f && bugGoing_) {
        // out through the crack
        bugGoing_ = false; bugMode_ = 1; exPhase_ = 0; exT_ = 0.f;
        bugP_ = vec3(b.x, soilHeight(b.x, b.z), b.z); bugHead_ = b.head; bugLook_ = 0.f;
    } else if (dist < 0.03f) {
        b.pause = rng_.range(4.f, 8.f) * (1.2f - 0.3f * wm);
        b.lookT = 0.f;
    } else {
        // walk toward the target, steering around what grows
        float hx = dx / dist, hz = dz / dist;
        const std::vector<Slot>& sl = slots();
        for (size_t i = 0; i < sl.size(); i++) {
            const float g = i < growth_.size() ? growth_[i] : 0.f;
            if (g <= 0.02f) continue;
            const float ox = b.x - sl[i].x, oz = b.z - sl[i].z;
            const float od = std::sqrt(ox * ox + oz * oz) + 1e-4f;
            const float rad = (sl[i].type == TerrariumUniverse::S_FERN || sl[i].type == TerrariumUniverse::S_TWIG) ? 0.08f : sl[i].size * 0.9f;
            const float push = smoothstepf(rad + 0.16f, rad + 0.04f, od) * 1.6f;
            hx += ox / od * push;
            hz += oz / od * push;
        }
        const float rr = std::sqrt(b.x * b.x + b.z * b.z);
        const float lim = bugGoing_ ? 0.8f : 0.7f;
        if (rr > lim) { hx -= b.x / rr * (rr - lim) * 8.f; hz -= b.z / rr * (rr - lim) * 8.f; }
        const float h = std::atan2(-hx, hz);
        const float turn = wrapA(h - b.head);
        b.head = wrapA(b.head + std::clamp(turn, -1.4f * dt, 1.4f * dt));
        want = 0.05f * smoothstepf(0.f, 0.12f, dist) * (1.f - 0.6f * smoothstepf(0.6f, 1.6f, std::fabs(turn)));
        want = std::max(want, 0.012f);
        b.lookT += dt;
        lookWant = 0.15f * std::sin(b.lookT * 0.7f) + std::clamp(turn, -0.4f, 0.4f) * 0.5f;
    }
    b.speed = approach(b.speed, want, 3.f, dt);
    const float nx = b.x - std::sin(b.head) * b.speed * dt, nz = b.z + std::cos(b.head) * b.speed * dt;
    if (!blocked(nx, nz, 0.045f) || blocked(b.x, b.z, 0.045f)) { b.x = nx; b.z = nz; }
    else { b.head = wrapA(b.head + 1.2f * dt); }
    b.walk += b.speed * dt * 92.f; // (a planted foot moves back 2 x 0.009 x BS per half cycle: no sliding)
    b.look = approach(b.look, lookWant, 2.5f, dt);
    bugCool_ -= dt;
    // blinks
    b.blinkT -= dt;
    if (b.blinkT <= 0.f) { b.blinkT = rng_.range(2.5f, 5.5f); b.blink = 1.f; }
    b.blink = std::max(0.f, b.blink - dt * 6.f);
}


// =====================================================================================================
// Game events
// =====================================================================================================

void TerrariumUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::HardDrop: {
        // a puff of soil pushed out from under the piece
        float yLo = 99.f, xs = 0.f;
        int n = 0;
        for (auto& c : ev.cells) yLo = std::min(yLo, layout::cellPos((float)c.x, (float)c.y).y);
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > yLo + 0.01f) continue;
            xs += p.x;
            n++;
        }
        if (!n) break;
        xs /= n;
        for (int i = 0; i < 6 && (int)motes_.size() < MAX_MOTES; i++) {
            Mote m;
            m.p = V(xs + rng_.range(-0.5f, 0.5f) * n * 0.5f, yLo - 0.5f);
            m.v = V(rng_.range(-1.f, 1.f), rng_.range(0.05f, 0.35f));
            m.life = rng_.range(0.5f, 0.9f);
            m.size = rng_.range(0.16f, 0.26f);
            m.col = lerp(pal_.soil, pal_.ink, 0.3f);
            m.alpha = 0.3f;
            m.seed = rng_.uniform();
            motes_.push_back(m);
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        for (auto& cl : ev.cells) {
            if (cl.y < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)cl.x, (float)cl.y);
            fading_.push_back({p.x, p.y, std::clamp(cl.type, 0, 6), 0.f});
        }
        // seeds: from inside the jar, behind the cleared rows, drifting down to the soil (or out onto the bench once
        // the glass is broken); once the lab is overgrown, a puff of pollen from what grows in view
        if (linesB_ > 27.f) {
            int n = 0;
            for (int y = 0; y < Game::H; y++) n += rows[y] && y >= Game::HIDDEN ? 1 : 0;
            sporeBurst(6 * n, 0.9f);
            break;
        }
        for (int y = 0; y < Game::H; y++) {
            if (!rows[y] || y < Game::HIDDEN) continue;
            const int n = 2;
            for (int k = 0; k < n; k++) {
                const vec3 cp = layout::cellPos(rng_.range(0.f, 9.f), (float)y);
                const vec3 rd = boardRay(V(cp.x, cp.y));
                const float t = (0.05f - eye_.z) / rd.z;
                vec3 w = eye_ + rd * t;
                w.y = std::clamp(w.y, 0.4f, 0.78f);
                const float rr = std::sqrt(w.x * w.x + w.z * w.z);
                if (rr > 0.7f) { w.x *= 0.7f / rr; w.z *= 0.7f / rr; }
                addSeed(w);
            }
        }
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
        overCells_.clear();
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.x < 0 || c.x >= Game::W || c.y >= Game::H) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            overCells_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), (float)c.y});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Pieces
// =====================================================================================================

void TerrariumUniverse::cell(float x, float y, float half, const vec3& tint, float alpha, int mode, float erase, float seed) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, seed}, {tint.x, tint.y, tint.z, alpha}, {(float)mode, erase, 0.f, 0.f}});
}

void TerrariumUniverse::buildBoard(const Game& g) {
    auto tintOf = [&](int t) { return pal_.piece[std::clamp(t, 0, 6)]; };
    auto seedOf = [](int x, int y) { return (float)((x * 7 + y * 13) % 17) / 17.f; };
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            const vec3 p = layout::cellPos((float)x, y - off);
            cell(p.x, p.y, 0.5f, tintOf(c.type), 1.f, 0, 0.f, seedOf(x, y));
        }
    // game over: the shards dull from the top down
    if (overT_ >= 0.f)
        for (const Fading& o : overCells_) {
            const float rowFrac = (o.t - Game::HIDDEN) / (float)(Game::H - Game::HIDDEN);
            const float k = smoothstepf(rowFrac * 0.6f, rowFrac * 0.6f + 0.3f, over_);
            cell(o.x, o.y, 0.5f, tintOf(o.type), 1.f - 0.4f * smoothstepf(0.7f, 1.f, over_), 0, 0.f, 0.f);
            cells_.back().c[2] = k;
        }
    for (const Fading& f : fading_) cell(f.x, f.y, 0.5f, tintOf(f.type), 1.f, 0, saturate(f.t / CLEAR_T), 0.f);
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cell(w.x, w.y, 0.5f, tintOf(p.type), 0.55f, 1, 0.f, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, tintOf(p.type), 1.f, 0, 0.f, seedOf(c[0], c[1]));
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
        for (auto& c : cells) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            cell(px, py, 0.5f * sc, tintOf(type), alpha, 0, 0.f, seedOf(c[0], c[1]));
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void TerrariumUniverse::sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param, float param2) {
    if (alpha <= 0.004f || size <= 0.f) return;
    sprites_.push_back({{p.x, p.y, size, (float)mode}, {col.x, col.y, col.z, alpha}, {param, param2, 0.f, 0.f}});
}

// =====================================================================================================
// Text
// =====================================================================================================

// The HUD's plates (its lettering: hudLine, the host's).
void TerrariumUniverse::drawHud(const std::vector<HudRect>& panels) {
    if (panels.empty()) return;
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

// The HUD, lettered by the host (text.hpp): M PLUS Rounded capitals, spaced; the song, the variant and the notices in
// Fraunces' soft italics, all on a soft halo of the room's veil. The universe's line ends with the story's stage
// (sealed, cracked, loose, overgrown, the city). (The palette is linear: the lettering, over the post now, takes it to sRGB.)
void TerrariumUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* caps = text::font(FONT_CAPS);
    text::Font* ital = text::font(FONT_ITAL);
    const float s = height_ / 1080.f;
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    const vec3 ink = ugl::toSrgb(lerp(pal_.ink, bridge_.ink, b)), accent = ugl::toSrgb(lerp(pal_.accent, bridge_.ink, b));
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    float halo = 0.9f;
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        // the variant as a word: its first letter kept, the rest small
        if (!var.empty()) {
            size_t n = 1;
            while (n < var.size() && ((unsigned char)var[n] & 0xC0) == 0x80) n++;
            var = var.substr(0, n) + text::lower(var.substr(n));
        }
        const float sz = base * 0.95f, vsz = base * 1.3f, gap = base * 0.7f, a = saturate(0.35f + h.alpha * 1.6f);
        // the italics' cap line a quarter of the capitals higher
        const float vdy = -sz * 0.25f + text::capHeight(ital) * vsz - text::capHeight(caps) * sz;
        out.add(name, caps, sz, ink, 0.28f, a);
        // an italic run after a space (the run before's own tracking counts in it)
        auto then = [&](const std::string& t, const vec3& c, float al, float px) {
            const float before = out.spans.back().track * out.spans.back().size;
            text::Span& sp = out.add(t, ital, vsz, c, 0.f, al);
            sp.gap = (px - before) / vsz;
            sp.dy = vdy;
        };
        if (!var.empty()) then(var, accent, a * 0.92f, gap);
        if (!stageName_.empty()) then(stageName_, ink, a * 0.7f, gap * 1.4f);
        break;
    }
    case HUD_SECTION: put(caps, h.text, base * 0.9f, base * 0.45f, ink, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(ital, h.text, base * 1.7f, base * 0.01f, ink, h.alpha * 1.3f, -base * 0.35f); break;
    case HUD_INFO: put(caps, h.text, base * 0.66f, base * 0.12f, ink, saturate(alpha * 1.3f) * 0.8f); break;
    case HUD_LABEL: put(caps, h.text, base * 0.8f, base * 0.4f, lerp(ink, accent, 0.5f), saturate(alpha * 1.2f) * 0.95f); break;
    case HUD_VALUE: put(caps, h.text, base * 1.05f, base * 0.06f, ink, alpha); break;
    case HUD_NOTICE: put(ital, h.text, base * 1.6f, base * 0.01f, accent, alpha); break;
    case HUD_KEY: put(caps, h.text, base * 0.75f, base * 0.08f, ink, alpha); break;
    case HUD_TOAST: put(caps, h.text, base * 1.3f, base * 0.45f, ink, alpha, -base * 0.2f); halo = 0.85f; break;
    case HUD_TOAST_SUB: put(ital, h.text, base * 1.4f, base * 0.01f, accent, alpha * 0.9f); halo = 0.85f; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(caps, h.text, base * 1.15f, base * 0.25f, ink, alpha, -base * 0.1f);
        else put(caps, h.text, base * 0.85f, base * 0.14f, h.accent ? accent : ink, alpha);
        halo = 0.85f;
        break;
    default: put(caps, h.text, base * 0.85f, base * 0.12f, h.accent ? accent : ink, alpha); break;
    }
    // the letters a touch bold, on a soft halo of the veil
    out.look.weight = 0.0075f;
    out.look.halo = ugl::toSrgb(lerp(pal_.veil * 0.7f, bridge_.color, b));
    out.look.haloA = halo;
    out.look.haloW = 0.09f;
    out.look.haloSmooth = true;
    // the post's dim and fade in, which the lettering was under
    out.alpha = (1.f - dimT_ * 0.5f) * fade_;
}

// =====================================================================================================
// Frame
// =====================================================================================================


void TerrariumUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    // REFALL_GPUTIME: the GPU time of a frame, printed every 60 frames
    static const bool gpuTime = std::getenv("REFALL_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) {
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
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && overT_ < 0.f && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    if (isDrop && !wasDrop_) {
        if (std::getenv("REFALL_GPUTIME")) std::fprintf(stderr, "[terrarium] drop at %.1f s\n", time_);
        // the drop: the glass bursts (still whole: the escape starts now, its pieces fly), or a burst of spores
        // and pollen from what has grown; the lab's lights and the neon stutter
        if (hole_ < 0.5f && linesEased_ > 2.f) {
            shiftTarget_ = std::max(shiftTarget_, HOLE_AT + 1.2f - linesEased_);
            burstT_ = 0.f;
            spawnShards(46, 2.4f, false);
            shardsOut_ = 14;
            sporeBurst(160, 2.2f);
        } else sporeBurst(260, 1.6f);
        flicker_ = 1.f;
    }
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.2f : 0.25f, dt);
    allLit_ = approach(allLit_, isDrop ? 1.f : 0.f, isDrop ? 1.5f : 0.12f, dt);
    // the vines surge forward through the drop (and stay ahead), swaying in a wave
    if (isDrop && !f.paused) surgeAcc_ = std::min(surgeAcc_ + wdt * 0.55f, 7.f);
    surge_ = approach(surge_, isDrop ? 1.f : 0.f, isDrop ? 2.f : 0.4f, dt);
    flicker_ = approach(flicker_, 0.f, 1.1f, dt);
    if (isDrop && !f.paused && rng_.chance(std::min(1.f, wdt * 30.f))) sporeBurst(2, 1.2f);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    // the calm: mist in the jar, condensation on its glass (slowly); the setup clears a misted jar, the teardown
    // mists it over again
    const float breath = std::max(1.f - smoothstepf(0.1f, 0.85f, f.setup), smoothstepf(0.f, 0.9f, f.teardown));
    {
        // the plants growing press on the glass and fog it; once it is broken the damp air leaks out
        const float press = smoothstepf(3.f, 13.f, linesEased_);
        const float fogT = lerpf(lerpf(1.f, 0.35f, wm), 0.12f, wp) * (1.f - 0.6f * hole_);
        const float condT = std::min(1.f, lerpf(lerpf(0.75f, 0.35f, wm), 0.18f, wp) + 0.45f * press) * (1.f - 0.55f * hole_);
        fog_ = approach(fog_, fogT, 0.3f, dt);
        cond_ = approach(cond_, condT, 0.15f, dt);
    }
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
        pal_ = basePal_;
    }

    double beat;
    if (music.hasSong) beat = music.beat;
    else {
        if (!f.paused) ownBeat_ += dt * 2.0;
        beat = ownBeat_;
    }
    {
        const float k = std::max(music.kick, kickIn_) * wp;
        kickS_ = approach(kickS_, k, k > kickS_ ? 5.f : 1.2f, dt);
    }
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);

    // ---- Growth: the lines (on the 50-line scale) -> each slot, eased; a new game starts the jar again.
    const std::vector<Slot>& sl = slots();
    {
        const float lines = f.bare ? lines_ : paceLines(game, (float)extraLines(time_));
        if (linesShown_ < 0.f) {
            linesEased_ = lines;
            // (test hooks starting at a stage: the seedlings its clears would have left, the shards on the bench)
            if (lines > 0.f && (extraLines(0.0) > 0 || std::getenv("REFALL_LINES_HERE"))) {
                pond_ = VARIANTS[wrapV(variant_)].pondDepth > 0.f ? smoothstepf(POND_AT, POND_AT + 4.f, lines) : 0.f;
                growth_.assign(sl.size(), 0.f);
                for (size_t i = 0; i < sl.size(); i++)
                    growth_[i] = sl[i].birth < 0.f ? 1.f : smoothstepf(sl[i].birth * BIRTH_SCALE, sl[i].birth * BIRTH_SCALE + SLOT_SPAN, lines);
                const int n = std::min(MAX_SPROUTS, (int)(lines * 0.6f));
                for (int k = 0; k < n; k++) {
                    float x, z;
                    if (pickSpot(x, z, 0.03f)) sprouts_.push_back({x, z, rng_.range(3.f, 60.f), rng_.uniform(), false});
                }
                if (lines >= HOLE_AT + 1.f) {
                    spawnShards(14, 0.4f, true);
                    shardsOut_ = 14;
                }
            }
        }
        lines_ = lines;
        linesShown_ = lines;
        // the growth eases toward the lines: a line's step is a breath, a quad a few (faster in a short song)
        const float cap = (lines < linesEased_ ? 8.f : 1.1f) * std::clamp(paceScale(), 1.f, 2.5f);
        linesEased_ += std::clamp((lines - linesEased_) * 1.6f * wdt, -cap * wdt, cap * wdt);
        if (lines < linesEased_ - 6.f) {
            // a new game: the seedlings fade away, the jar is whole again
            for (Sprout& s : sprouts_) s.age = std::min(s.age, 1.f);
            shiftTarget_ = 0.f;
            surgeAcc_ = 0.f;
        }
        const float L = linesEased_;
        if (growth_.size() != sl.size()) growth_.assign(sl.size(), -1.f);
        for (size_t i = 0; i < sl.size(); i++) {
            const float target = sl[i].birth < 0.f ? 1.f : smoothstepf(sl[i].birth * BIRTH_SCALE, sl[i].birth * BIRTH_SCALE + SLOT_SPAN, L);
            if (growth_[i] < 0.f) growth_[i] = target;
            growth_[i] = approach(growth_[i], target, 2.5f, wdt);
        }
        const float pt = VARIANTS[wrapV(variant_)].pondDepth > 0.f ? smoothstepf(POND_AT, POND_AT + 4.f, L) : 0.f;
        pond_ = approach(pond_, pt, 2.f, wdt);
        moss_ = smoothstepf(4.f, 15.f, L);
        tod_ = todOf(L);
        inVine_ = smoothstepf(6.f, 14.f, L);
        // the story's clocks
        shiftA_ = shiftTarget_ <= 0.f ? approach(shiftA_, 0.f, 6.f, wdt) : approach(shiftA_, shiftTarget_, 3.f, wdt);
        if (linesShown_ >= 0.f && burstT_ < 0.f && shiftTarget_ <= 0.f) shiftA_ = std::min(shiftA_, 0.f);
        linesA_ = L + shiftA_ + surgeAcc_;
        linesB_ = L + surgeAcc_;
        crack_ = smoothstepf(CRACK_AT, CRACK_AT + 2.5f, linesA_);
        hole_ = smoothstepf(HOLE_AT, HOLE_AT + 1.f, linesA_);
        labOn_ = 1.f - 0.72f * smoothstepf(30.f, 36.5f, linesB_);
        stage_ = linesA_ < CRACK_AT ? 0 : (linesA_ < ESCAPE_AT ? 1 : (linesB_ < 26.5f ? 2 : (linesB_ < 39.f ? 3 : 4)));
        updateCamera(L);
        stageName_ = stageName(stage_);
    }
    if (burstT_ >= 0.f) burstT_ += wdt;
    // the pieces falling out of the hole as it opens; gone when the jar is whole again (a new game)
    {
        const int want = (int)(hole_ * 14.f);
        while (shardsOut_ < want) { spawnShards(1, 0.35f, false); shardsOut_++; }
        if (hole_ < 0.02f) { shards_.clear(); shardsOut_ = 0; burstT_ = -1.f; }
    }
    // the jar keeps growing slowly after its scene is complete
    const float after = 1.f + 0.12f * smoothstepf(50.f, 100.f, linesEased_);

    // ---- The day outside: blend the keyframes. The lab's own cold light inside.
    vec3 skyZ, skyH, sunC, glowC, ambS, ambG, fill;
    float elev, disc, moon, stars, night;
    {
        const float t = std::clamp(tod_, 0.f, (float)(SKY_KEYS - 1));
        const int a = std::min(SKY_KEYS - 2, (int)std::floor(t));
        const float k = t - a;
        const Sky& A = SKIES[a];
        const Sky& B = SKIES[a + 1];
        auto C = [&](unsigned x, unsigned y, float ix, float iy) {
            return lerp(ugl::toLinear(hex(x)) * ix, ugl::toLinear(hex(y)) * iy, k);
        };
        skyZ = C(A.zen, B.zen, 1.f, 1.f);
        skyH = C(A.hor, B.hor, 1.f, 1.f);
        sunC = C(A.sun, B.sun, A.sunI, B.sunI);
        glowC = C(A.glow, B.glow, A.glowI, B.glowI);
        ambS = C(A.ambS, B.ambS, A.ambSI, B.ambSI);
        ambG = C(A.ambG, B.ambG, A.ambGI, B.ambGI);
        fill = C(A.fill, B.fill, A.fillI, B.fillI);
        elev = lerpf(A.elev, B.elev, k);
        disc = lerpf(A.disc, B.disc, k);
        moon = lerpf(A.moon, B.moon, k);
        stars = lerpf(A.stars, B.stars, k);
        night = lerpf(A.night, B.night, k);
        // the desert's light is warmer, the tropics' air greener
        const int kind = VARIANTS[wrapV(variant_)].kind;
        if (kind == 1) { skyH = skyH * vec3(1.06f, 1.0f, 0.9f); fill = fill * vec3(1.08f, 1.f, 0.9f); }
        if (kind == 2) { skyH = skyH * vec3(0.95f, 1.03f, 0.98f); }
    }
    // the setup: the light comes up, outside and in the lab; the teardown: it goes down
    const float lamp = lerpf(0.35f, 1.f, smoothstepf(0.f, 0.8f, f.setup) * (1.f - smoothstepf(0.2f, 1.f, f.teardown)));
    // the lab's tubes: cold, failing one by one as the lab is swallowed, stuttering at the drop
    const float stutter = flicker_ * (terra::ghash1(std::floor(time_ * 18.f)) > 0.45f ? 0.75f : 0.1f);
    const float labOn = labOn_ * (1.f - stutter);
    vec3 labKey = vec3(0.78f, 0.87f, 1.0f) * 1.35f * labOn;
    vec3 labFill = vec3(0.19f, 0.22f, 0.26f) * labOn + fill * 0.1f;
    vec3 labAmbS = vec3(0.12f, 0.15f, 0.19f) * (0.3f + 0.7f * labOn) + ambS * 0.25f;
    vec3 labAmbG = vec3(0.045f, 0.047f, 0.05f) * (0.4f + 0.6f * labOn) + ambG * 0.2f;
    vec3 winGlow = glowC * 0.55f;
    vec3 labC = vec3(0.3f, 0.34f, 0.38f);
    for (vec3* c : {&labKey, &labFill, &labAmbS, &labAmbG, &winGlow, &sunC, &glowC, &ambS, &ambG, &fill}) *c = *c * lamp;
    skyZ = skyZ * (0.3f + 0.7f * lamp);
    skyH = skyH * (0.3f + 0.7f * lamp);
    labC = labC * lamp;
    // slow motion: a cool hush
    labKey = lerp(labKey, labKey * vec3(0.7f, 0.8f, 1.1f), slow_);
    const vec3 labDir = normalize(vec3(0.42f, 0.86f, 0.36f));
    vec3 outDir;
    {
        const float az = 0.3f;
        const float el = std::max(elev, -0.06f);
        const vec3 sd(std::sin(az) * std::cos(el), std::sin(el), -std::cos(az) * std::cos(el));
        const vec3 md = normalize(vec3(0.3f, 0.5f, -0.8f)); // (the moon)
        outDir = normalize(lerp(sd, md, smoothstepf(4.f, 4.9f, tod_)));
        if (outDir.y < 0.08f) outDir = normalize(vec3(outDir.x, 0.08f, outDir.z));
    }
    const float neonOn = 0.08f + 0.92f * smoothstepf(2.8f, 4.3f, tod_);
    const float litWin = smoothstepf(2.6f, 4.4f, tod_);
    vec3 haze = lerp(skyH, skyZ, 0.3f) * 0.85f;
    haze = lerp(haze, vec3(0.05f, 0.04f, 0.09f) + pal_.neonA * 0.015f, smoothstepf(3.5f, 5.f, tod_));

    // ---- Fireflies in the jar: they come out as the lab's lights fail; the drop lights them all at once.
    const float flyEase = smoothstepf(26.f, 34.f, linesB_);
    const float flyAmt = std::max(flyEase, allLit_);
    {
        const float halfBeat = (float)(0.5 + 0.5 * std::cos(beat * PI));   // every two beats
        for (int i = 0; i < (int)flies_.size(); i++) {
            Fly& fl = flies_[i];
            const float th = (float)i / MAX_FLIES * 0.85f;
            const float onT = std::max(smoothstepf(th, th + 0.12f, flyEase), allLit_) * (overT_ >= 0.f ? 0.f : 1.f);
            fl.on = approach(fl.on, onT, onT > fl.on ? 0.8f : 0.4f, dt);
            const float t = time_ * (0.6f + 0.25f * wm + 0.3f * wp);
            fl.p = fl.home + vec3(0.13f * std::sin(t * fl.freq * 0.5f + fl.phase) + 0.05f * std::sin(t * 0.9f + fl.seed * 9.f),
                                  0.07f * std::sin(t * fl.freq * 0.37f + fl.phase * 1.7f),
                                  0.11f * std::cos(t * fl.freq * 0.43f + fl.phase * 0.6f));
            const float r = std::sqrt(fl.p.x * fl.p.x + fl.p.z * fl.p.z);
            if (r > 0.86f) { fl.p.x *= 0.86f / r; fl.p.z *= 0.86f / r; }
            fl.p.y = std::max(fl.p.y, soilHeight(fl.p.x, fl.p.z) + 0.06f);
            const float pulse = smoothstepf(-0.6f, 0.9f, std::sin(time_ * fl.freq * 1.1f + fl.phase));
            float g = (0.3f + 0.7f * pulse) * (1.f + 0.3f * wp * halfBeat * smoothstepf(0.2f, 0.6f, pulse) + 0.25f * kickS_);
            g = lerpf(g, std::max(g, 0.9f), allLit_);
            fl.glow = g * fl.on;
        }
    }

    // ---- Seeds drifting down and the sprouts they leave (in the jar, then on the bench).
    for (Seed& s : seeds_) {
        s.age += wdt;
        if (s.age < 0.f || s.landed) continue;
        if (s.age >= s.life) {
            s.landed = true;
            if ((int)sprouts_.size() < MAX_SPROUTS && s.seed < 0.55f) sprouts_.push_back({s.to.x, s.to.z, 0.f, s.seed, s.target == 1});
        }
    }
    seeds_.erase(std::remove_if(seeds_.begin(), seeds_.end(), [](const Seed& s) { return s.landed; }), seeds_.end());
    for (Sprout& s : sprouts_) s.age += (s.age < 1.f && lines_ < linesEased_ - 6.f) ? -wdt : wdt;
    if (lines_ < linesEased_ - 6.f)
        for (Sprout& s : sprouts_) s.age = std::min(s.age, 1.f) - wdt * 0.6f;
    sprouts_.erase(std::remove_if(sprouts_.begin(), sprouts_.end(), [](const Sprout& s) { return s.age < -0.05f; }), sprouts_.end());
    // ---- Spores and pollen: drifting up and out, slowing, swirling
    for (Spore& s : spores_) {
        s.age += wdt;
        if (s.age < 0.f) continue;
        const float k = std::exp(-1.2f * wdt);
        s.v = s.v * k + vec3(std::sin(time_ * 1.3f + s.seed * 30.f), 0.6f + 0.4f * std::sin(time_ * 0.7f + s.seed * 11.f),
                             std::cos(time_ * 1.1f + s.seed * 17.f)) * (0.06f * s.size * wdt);
        s.p = s.p + s.v * wdt;
    }
    spores_.erase(std::remove_if(spores_.begin(), spores_.end(), [](const Spore& s) { return s.age >= s.life; }), spores_.end());
    // ---- The shards: flying, tumbling, landing flat on the bench (or the floor)
    bool shardsMoving = false;
    for (Shard& s : shards_) {
        if (s.rest) continue;
        shardsMoving = true;
        s.v.y -= 22.f * wdt;
        s.p = s.p + s.v * wdt;
        s.ang += s.spin * wdt;
        const float rr = std::sqrt(s.p.x * s.p.x + s.p.z * s.p.z);
        if (rr < 1.03f && s.p.y < 0.85f && s.p.y > 0.f) {
            // (off the jar's own glass)
            const float k = 1.03f / std::max(rr, 1e-3f);
            s.p.x *= k; s.p.z *= k;
            const vec3 nrm(s.p.x / 1.03f, 0.f, s.p.z / 1.03f);
            const float vn = dot(s.v, nrm);
            if (vn < 0.f) s.v = s.v - nrm * (vn * 1.4f);
        }
        const bool onBench = std::fabs(s.p.x) < 12.f && s.p.z > -3.6f && s.p.z < 1.6f;
        const float floorY = onBench ? 0.f : terra::lab::FLOOR;
        if (s.p.y <= floorY + 0.003f && (onBench || s.p.y < -0.5f)) {
            s.p.y = floorY + 0.003f;
            if (std::fabs(s.v.y) > 1.2f) { s.v.y = -s.v.y * 0.25f; s.v.x *= 0.5f; s.v.z *= 0.5f; s.spin *= 0.5f; }
            else s.rest = true;
        }
    }

    if (!f.paused) {
        if (bugMode_ == 0) updateBeetle(wdt, wm, wp);
        else {
            updateExcursion(wdt);
            bug_.walk += bugSpeed_ * wdt * 92.f;
            bug_.blinkT -= wdt;
            if (bug_.blinkT <= 0.f) { bug_.blinkT = rng_.range(2.5f, 5.5f); bug_.blink = 1.f; }
            bug_.blink = std::max(0.f, bug_.blink - wdt * 6.f);
        }
        if (overT_ >= 0.f || hole_ < 0.9f) { if (bugMode_ != 0) { bugMode_ = 0; bug_.x = -0.4f; bug_.z = 0.5f; } bugGoing_ = false; }
    }

    // ---- Motes over the board.
    for (Mote& m : motes_) {
        m.age += dt;
        m.p = vec2{m.p.x + m.v.x * dt, m.p.y + m.v.y * dt};
        m.v = vec2{m.v.x * std::exp(-2.5f * dt), m.v.y * std::exp(-2.5f * dt)};
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.age >= m.life; }), motes_.end());
    for (auto& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > CLEAR_T; }), fading_.end());

    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float world = 1.f - smoothstepf(0.f, 0.7f, bridge);
    sprites_.clear();
    if (!f.bare) {
        for (const Mote& m : motes_) {
            const float t = m.age / m.life;
            const float a = smoothstepf(0.f, 0.1f, t) * (1.f - smoothstepf(0.5f, 1.f, t)) * m.alpha;
            sprite(m.p, m.size * (1.f + t), m.col, a, 0);
        }
    }
    cells_.clear();
    if (!f.bare) buildBoard(game);

    // =========================================================== the world (meshes, multisampled, full resolution)
    const VariantDef& vd = VARIANTS[wrapV(variant_)];
    const float timeU = std::fmod(time_, 3000.f);
    const bool nearJar = length(eye_) < 40.f;
    computeJar(sl, after);
    updateJarMesh();
    // ---- what moves every frame: the sprouts, the beetle, the shards
    {
        std::vector<terra::Vtx> v;
        const terra::Pond& pd0 = jar_.pd;
        const int n = std::min(MAX_SPROUTS, (int)sprouts_.size());
        // (rebuilt when one of them grew, appeared or went)
        std::vector<float> key;
        for (int i = 0; i < n; i++) {
            const Sprout& s = sprouts_[i];
            key.insert(key.end(), {s.x, s.z, saturate(s.age / 3.f) * (0.85f + 0.15f * smoothstepf(5.f, 30.f, s.age)), s.bench ? 1.f : 0.f});
        }
        key.insert(key.end(), {pd0.depth, (float)vd.kind});
        bool same = key.size() == sproutKey_.size();
        for (size_t i = 0; same && i < key.size(); i++) same = std::fabs(key[i] - sproutKey_[i]) < 0.003f;
        if (!same) {
            for (int i = 0; i < n; i++) {
                const Sprout& s = sprouts_[i];
                const float y = s.bench ? 0.f : terra::soilH(s.x, s.z, pd0) - 0.004f;
                terra::buildSprout(v, vec3(s.x, y, s.z), key[i * 4 + 2], i, vd.kind);
            }
            upload(sproutMesh_, v, true);
            sproutKey_ = key;
        }
        v.clear();
        // the beetle: in the jar, or out on its excursion
        vec3 bp;
        float bh;
        if (bugMode_ == 0) { bp = vec3(bug_.x, terra::soilH(bug_.x, bug_.z, pd0) - 0.002f, bug_.z); bh = bug_.head; }
        else { bp = bugP_; bh = bugHead_; }
        const float bugMv = bugMode_ == 0 ? saturate(bug_.speed / 0.03f) : saturate(bugSpeed_ / 0.03f);
        terra::buildBeetle(v, bp, bh, bug_.walk, bugMv, bugMode_ == 0 ? bug_.look : bugLook_, timeU);
        upload(beetle_, v, true);
        // the shards: little jagged triangles of glass
        if (shardsMoving || shardMesh_.count != (int)shards_.size() * 6) {
            v.clear();
            for (const Shard& s : shards_) {
                vec3 ax = s.ax, n0;
                float ang = s.ang;
                if (s.rest) { ax = vec3(0.f, 1.f, 0.f); }
                // its plane: turned about its axis (lying flat once it rests)
                const vec3 t1 = normalize(cross(ax, std::fabs(ax.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0))), t2 = cross(ax, t1);
                const vec3 a1 = t1 * std::cos(ang) + t2 * std::sin(ang), a2 = cross(ax, a1);
                n0 = ax;
                const float r = s.size;
                const vec3 p0 = s.p + a1 * r, p1 = s.p + a1 * (-0.5f * r) + a2 * (0.75f * r * (0.7f + 0.6f * s.seed)),
                           p2 = s.p - a2 * (0.6f * r) - a1 * (0.2f * r);
                terra::Vtx x{};
                auto put = [&](const vec3& p) {
                    x.p[0] = p.x; x.p[1] = p.y; x.p[2] = p.z; x.n[0] = n0.x; x.n[1] = n0.y; x.n[2] = n0.z;
                    x.m[0] = 40.f; x.g[3] = -1000.f;
                    v.push_back(x);
                };
                put(p0); put(p1); put(p2);
                put(p0); put(p2); put(p1);
            }
            upload(shardMesh_, v, true);
        }
    }

    // ---- the lights in the air: their glows (x, y, z, strength; size, r, g, b)
    std::vector<float> GW;
    auto glowAt = [&](const vec3& p, float g, float size, const vec3& c) { GW.insert(GW.end(), {p.x, p.y, p.z, g, size, c.x, c.y, c.z}); };
    float FL[MAX_FLIES * 4] = {};
    int nFl = 0;
    const vec3 flyC = pal_.fly * 1.4f;
    for (int i = 0; i < (int)flies_.size(); i++) {
        const Fly& fl = flies_[i];
        if (fl.glow <= 0.01f) continue;
        FL[nFl * 4 + 0] = fl.p.x; FL[nFl * 4 + 1] = fl.p.y; FL[nFl * 4 + 2] = fl.p.z; FL[nFl * 4 + 3] = fl.glow;
        nFl++;
        if (nearJar) glowAt(fl.p, fl.glow, 1.f, flyC);
    }
    for (const Seed& s : seeds_) {
        if (s.age < 0.f) continue;
        const float t = saturate(s.age / s.life);
        const float eh = 1.f - std::pow(1.f - t, 2.6f);
        const float ev = t * t * (3.f - 2.f * t);
        vec3 p = lerp(s.p, s.to + vec3(0.f, 0.012f, 0.f), eh);
        p.y = lerpf(s.p.y, s.to.y + 0.012f, ev);
        p.x += std::sin(s.age * 1.9f + s.seed * 20.f) * 0.05f * (1.f - t);
        p.z += std::cos(s.age * 1.4f + s.seed * 13.f) * 0.03f * (1.f - t);
        const float a = smoothstepf(0.f, 0.4f, s.age) * (1.f - smoothstepf(0.85f, 1.f, t) * 0.6f);
        glowAt(p, a, 1.f, lerp(vec3(1.f, 0.95f, 0.82f), flyC, 0.2f));
    }
    {
        const vec3 pollen = lerp(vec3(1.f, 0.88f, 0.55f), pal_.flower2, 0.25f);
        for (const Spore& s : spores_) {
            if (s.age < 0.f) continue;
            const float t = s.age / s.life;
            const float a = smoothstepf(0.f, 0.15f, t) * (1.f - smoothstepf(0.6f, 1.f, t)) * (0.6f + 0.4f * std::sin(time_ * 3.f + s.seed * 20.f));
            const float dist = length(s.p - eye_);
            glowAt(s.p, a * 0.3f, std::max(1.f, dist * 0.004f) * std::min(s.size, 4.f) * 0.5f, pollen);
        }
    }
    // the neon's halo and the street lamps (at night), fireflies over the vines of the room and the city
    if (!nearJar || linesB_ > 30.f) {
        const vec3 nc[3] = {pal_.neonA, vec3(0.2f, 1.f, 0.45f), pal_.neonB};
        for (size_t i = 0; i < neon_.size(); i++) {
            const terra::NeonPt& q = neon_[i];
            const float on = neonFlick(q.sign, time_, flicker_) * neonOn;
            if (on < 0.02f) continue;
            glowAt(q.p, on * 0.6f, 12.f, nc[std::clamp(q.color, 0, 2)]);
        }
        for (const terra::CityLight& l : lights_)
            if (l.color == 3) glowAt(l.p + vec3(0.f, 3.f, 0.f), neonOn * 0.8f, 14.f, vec3(1.f, 0.78f, 0.5f));
        const float nightFlies = std::max(smoothstepf(3.2f, 4.6f, tod_), allLit_) * (overT_ >= 0.f ? 0.f : 1.f);
        if (nightFlies > 0.01f) {
            int count = 0;
            for (int i = 0; i < (int)blooms_.size() && count < 220; i++) {
                const terra::Bloom& b = blooms_[i];
                if (b.appear < terra::lab::GROUP_B || linesB_ < b.appear + 1.f) continue;
                if (hash01(i * 7 + 1) > 0.55f) continue;
                const float h = hash01(i * 13 + 5), h2 = hash01(i * 3 + 9);
                const float R = std::max(b.scale * 2.5f, 0.3f);
                const vec3 p = b.p + vec3(std::sin(time_ * (0.3f + 0.3f * h) + h * 20.f), 0.6f + 0.5f * std::sin(time_ * (0.2f + 0.2f * h2) + h2 * 9.f),
                                          std::cos(time_ * (0.25f + 0.3f * h2) + h * 7.f)) * R;
                const float dist = length(p - eye_);
                const float g = nightFlies * (0.3f + 0.7f * smoothstepf(-0.6f, 0.9f, std::sin(time_ * (0.7f + h) + i * 2.1f)));
                glowAt(p, g * 0.8f, std::max(1.f, dist * 0.0035f), flyC);
                count++;
            }
        }
    }

    vp3_ = mat4::perspective(2.f * std::atan(tanH_), aspect_, near_, far_) * mat4::lookAt(eye_, eye_ + fwd_, up_);
    {
        // the key light's shadow map, fitted to the jar and what stands around it on the bench
        const vec3 mn(-5.2f, -0.1f, -3.4f), mx(4.6f, 3.4f, 1.7f);
        const vec3 C = (mn + mx) * 0.5f;
        const mat4 lv = mat4::lookAt(C + labDir * 30.f, C, vec3(0.f, 0.f, -1.f));
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        for (int i = 0; i < 8; i++) {
            const vec4 l = lv * vec4((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z, 1.f);
            const float c[3] = {l.x, l.y, l.z};
            for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], c[k]); hi[k] = std::max(hi[k], c[k]); }
        }
        lightVp_ = mat4::ortho(lo[0], hi[0], lo[1], hi[1], -hi[2] - 1.f, -lo[2] + 1.f) * lv;
        shadowSoft_ = 0.011f / std::max(0.1f, 0.5f * (hi[0] - lo[0] + hi[1] - lo[1]));
    }
    const vec3 lines3(linesA_, linesB_, surge_);
    auto setScene = [&](GLuint P) {
        cset3f(P, "uEye", eye_);
        cset1f(P, "uTime", timeU);
        cset1f(P, "uPx", 2.f * tanH_ / msH_);
        cset1f(P, "uSway", 0.35f + 0.35f * wm + 0.3f * wp);
        cset1f(P, "uSpots", vd.spots);
        cset3f(P, "uLines", lines3);
        cset3f(P, "uSunDir", labDir);
        cset3f(P, "uSunCol", labKey);
        cset3f(P, "uSkyZ", skyZ);
        cset3f(P, "uSkyH", skyH);
        cset3f(P, "uAmbS", labAmbS);
        cset3f(P, "uAmbG", labAmbG);
        cset3f(P, "uFill", labFill);
        cset3f(P, "uGlowSun", winGlow);
        cset3f(P, "uOutDir", outDir);
        cset3f(P, "uOutCol", sunC * (1.f - 0.5f * night));
        cset3f(P, "uOutAmbS", ambS * (1.3f - 0.75f * night));
        cset3f(P, "uOutAmbG", ambG * (1.f - 0.5f * night));
        cset3f(P, "uHaze", haze);
        cset1f(P, "uSunDisc", disc);
        cset2f(P, "uSunSky", 0.3f, std::tan(std::clamp(elev, -0.2f, 1.2f)) * 2.5f);
        cset1f(P, "uMoon", moon);
        cset1f(P, "uStars", stars);
        cset1f(P, "uRain", 0.f);
        cset1f(P, "uFog", fog_ * (0.7f + 0.3f * (1.f - wp)) + 2.f * breath);
        cset1f(P, "uCond", std::max(cond_, breath));
        cset1f(P, "uNight", night);
        cset1f(P, "uFlyAmt", flyAmt);
        cset3f(P, "uBugC", pal_.bug); cset3f(P, "uBugSpot", pal_.bugSpot); cset3f(P, "uFlyC", flyC); cset3f(P, "uSill", pal_.sill);
        cset3f(P, "uFrame", pal_.frame); cset3f(P, "uWall", pal_.wall); cset3f(P, "uHill", pal_.neonA); cset3f(P, "uHill2", pal_.neonB);
        cset1i(P, "uFlies", nFl);
        glUniform4fv(UL(P, "uFly"), MAX_FLIES, FL);
        csetMat(P, "uLightVP", lightVp_);
        cset1i(P, "uShadow", 3);
        cset1f(P, "uSoft", std::max(shadowSoft_, 1.5f / SHADOW_SIZE));
        csetMat(P, "uVP", vp3_);
        cset3f(P, "uFoot", vec3(0.f));
        cset1f(P, "uScale", 1.f);
        cset4f(P, "uBug2", bug_.walk, 0.f, 0.f, bug_.blink > 0.f ? std::sin(bug_.blink * PI) : 0.f);
    };
    // the jar's own uniforms: its palette, pond, moss, vines, the feet of what grows in it
    auto setJar = [&](GLuint P) {
        const JarState& J = jar_;
        cset1i(P, "uKind", J.kind);
        cset4f(P, "uPondP", J.pd.x, J.pd.z, J.pd.r, J.pd.depth);
        cset1f(P, "uPondY", J.level);
        cset1f(P, "uPond", J.fill);
        cset1f(P, "uMossCover", J.moss);
        cset1f(P, "uInVine", J.vine);
        static const char* PAL[17] = {"uSoil", "uSoil2", "uGravel", "uStr1", "uStr2", "uMoss", "uMoss2", "uLeaf", "uLeaf2",
                                      "uStem", "uCap", "uCap2", "uFlower", "uFlower2", "uBark", "uWater", "uStone"};
        for (int i = 0; i < 17; i++) cset3f(P, PAL[i], vec3(J.pal[i * 3], J.pal[i * 3 + 1], J.pal[i * 3 + 2]));
        float occ[24 * 4] = {};
        int n = 0;
        for (const terra::SlotDraw& s : J.slots) {
            if (s.g <= 0.02f || n >= 24) continue;
            float r;
            switch (s.type) {
            case S_MOSS: r = s.size * s.g * 1.1f; break;
            case S_FERN: r = 0.06f; break;
            case S_TWIG: r = 0.045f; break;
            case S_MUSH: r = s.size * 0.7f; break;
            default: r = s.size * 0.75f; break;
            }
            occ[n * 4] = s.base.x; occ[n * 4 + 1] = s.base.z; occ[n * 4 + 2] = r; occ[n * 4 + 3] = 0.32f * smoothstepf(0.f, 0.5f, s.g);
            n++;
        }
        cset1i(P, "uOccN", n);
        glUniform4fv(UL(P, "uOcc"), 24, occ);
    };
    auto draw = [](const Mesh& m) {
        if (m.count <= 0) return;
        glBindVertexArray(m.vao);
        glDrawArrays(GL_TRIANGLES, 0, m.count);
    };
    // everything the jar holds, with the given program (the colour pass or the shadow pass)
    auto drawContents = [&](GLuint P) {
        setJar(P);
        draw(jarMesh_.soilMesh);
        draw(jarMesh_.mesh);
        for (const Mesh& m : jarMesh_.slot) draw(m);
        draw(sproutMesh_);
        if (bugMode_ == 0) draw(beetle_);
        if (P != progShadow_) draw(cork_);
        if (bugMode_ == 1) {
            cset1i(P, "uClip", 0);
            draw(beetle_);
            cset1i(P, "uClip", 1);
        }
    };

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    // ---- the key light's shadow map: what the jar holds and what stands around it
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    if (nearJar) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 2.f);
        glUseProgram(progShadow_);
        csetMat(progShadow_, "uVP", lightVp_);
        cset1f(progShadow_, "uTime", timeU);
        cset1f(progShadow_, "uSway", 0.35f + 0.35f * wm + 0.3f * wp);
        cset3f(progShadow_, "uLines", lines3);
        cset3f(progShadow_, "uFoot", vec3(0.f));
        cset1f(progShadow_, "uScale", 1.f);
        drawContents(progShadow_);
        if (length(eye_) < 12.f && world_.count > 0) {
            // (the lab, and what grows over the bench)
            glBindVertexArray(world_.vao);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)worldLab_);
            glDrawArrays(GL_TRIANGLES, (GLint)worldGrowth_, (GLsizei)(worldBench_ - worldGrowth_));
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    // ---- the sky, the lab and the city, the jar's contents: multisampled
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, msW_, msH_);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glActiveTexture(GL_TEXTURE0);
    // the world (close on the jar, the city out of sight: only the lab and what grows in it)
    auto drawWorld = [&] {
        if (world_.count <= 0) return;
        glBindVertexArray(world_.vao);
        if (length(eye_) < 12.f) {
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)worldLab_);
            glDrawArrays(GL_TRIANGLES, (GLint)worldGrowth_, (GLsizei)(world_.count - (int)worldGrowth_));
        } else glDrawArrays(GL_TRIANGLES, 0, world_.count);
    };
    // (the world's depth first: its shading, heavy, only where it shows)
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glUseProgram(progShadow_);
    csetMat(progShadow_, "uVP", vp3_);
    cset1f(progShadow_, "uTime", timeU);
    cset1f(progShadow_, "uSway", 0.35f + 0.35f * wm + 0.3f * wp);
    cset3f(progShadow_, "uLines", lines3);
    cset3f(progShadow_, "uFoot", vec3(0.f));
    cset1f(progShadow_, "uScale", 1.f);
    drawWorld();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glUseProgram(progLab_);
    setScene(progLab_);
    setJar(progLab_);
    cset3f(progLab_, "uLabC", labC * labOn);
    cset1f(progLab_, "uLabOn", labOn);
    cset1f(progLab_, "uFlicker", flicker_);
    cset1f(progLab_, "uMossR", 3.2f * smoothstepf(ESCAPE_AT, 25.f, linesA_) + 34.f * smoothstepf(26.f, 37.f, linesB_));
    cset1f(progLab_, "uCityGreen", smoothstepf(38.f, 50.f, linesB_));
    cset1f(progLab_, "uLitWin", litWin);
    cset1f(progLab_, "uNeonOn", neonOn);
    cset3f(progLab_, "uCrackW", CRACK_DIR * 1.15f);
    cset3f(progLab_, "uNeon0", pal_.neonA * 1.2f);
    cset3f(progLab_, "uNeon1", vec3(0.2f, 1.f, 0.45f));
    cset3f(progLab_, "uNeon2", pal_.neonB * 1.2f);
    cset3f(progLab_, "uLampC", vec3(1.f, 0.72f, 0.42f) * 0.9f);
    cset1f(progLab_, "uTagNo", (float)tagNo_);
    cset3f(progLab_, "uLeafA", pal_.leaf);
    cset3f(progLab_, "uLeafB", pal_.leaf2);
    {
        // the city's lights nearest the camera (16)
        float LP[16 * 4] = {}, LC[16 * 4] = {};
        std::vector<std::pair<float, int>> byD;
        for (int i = 0; i < (int)lights_.size(); i++) byD.push_back({length(lights_[i].p - eye_), i});
        std::sort(byD.begin(), byD.end());
        int n = 0;
        if (!nearJar || linesB_ > 33.f)
            for (int k = 0; k < (int)byD.size() && n < 16; k++) {
                const terra::CityLight& l = lights_[byD[k].second];
                LP[n * 4] = l.p.x; LP[n * 4 + 1] = l.p.y; LP[n * 4 + 2] = l.p.z; LP[n * 4 + 3] = l.reach;
                LC[n * 4] = (float)l.color; LC[n * 4 + 1] = l.sign;
                n++;
            }
        cset1i(progLab_, "uLightN", n);
        glUniform4fv(UL(progLab_, "uLightP"), 16, LP);
        glUniform4fv(UL(progLab_, "uLightC"), 16, LC);
    }
    drawWorld();
    glDepthFunc(GL_LESS);
    glUseProgram(progContent_);
    setScene(progContent_);
    cset1i(progContent_, "uClip", 1);
    drawContents(progContent_);
    // the sky where nothing stands
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glUseProgram(progSky_);
    setScene(progSky_);
    cset3f(progSky_, "uCamF", fwd_);
    cset3f(progSky_, "uCamR", right_);
    cset3f(progSky_, "uCamU", up_);
    cset1f(progSky_, "uTanH", tanH_);
    cset1f(progSky_, "uAspect", aspect_);
    ugl::drawFullscreen();
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    // ---- the jar's glass, its far side: laid over what is behind it (not in the depth: what lies behind the jar
    // still shows through it)
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUseProgram(progGlass_);
    setScene(progGlass_);
    setJar(progGlass_);
    cset1i(progGlass_, "uFront", 0);
    if (nearJar) draw(glassIn_);
    glDisable(GL_BLEND);
    // ---- what the front glass sees through: the world so far, resolved with its depth
    if (nearJar) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, backT_.fbo);
        glBlitFramebuffer(0, 0, msW_, msH_, 0, 0, msW_, msH_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBlitFramebuffer(0, 0, msW_, msH_, 0, 0, msW_, msH_, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    }
    // ---- the front glass: refracting the world behind it, cracked
    glDepthFunc(GL_LEQUAL);
    cset1i(progGlass_, "uFront", 1);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, backT_.tex);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, backDepth_);
    glActiveTexture(GL_TEXTURE0);
    cset1i(progGlass_, "uBack", 4);
    cset1i(progGlass_, "uBackDepth", 5);
    cset3f(progGlass_, "uFwd", fwd_);
    cset2f(progGlass_, "uNF", near_, far_);
    cset2f(progGlass_, "uCrackUV", terra::lab::CRACK_U, terra::lab::CRACK_V);
    cset1f(progGlass_, "uCrack", crack_);
    cset1f(progGlass_, "uHole", hole_);
    if (nearJar) draw(glassOut_);
    // ---- the glassware and the shards: a rim and the room's reflection added, the rest tinted
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC1_COLOR);
    glUseProgram(progTube_);
    setScene(progTube_);
    setJar(progTube_);
    cset1i(progTube_, "uTwoSided", 0);
    draw(labGlass_);
    cset1i(progTube_, "uTwoSided", 1);
    if (nearJar) draw(shardMesh_);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    // ---- resolved, with its depth (the opaque world's)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, worldT_.fbo);
    glBlitFramebuffer(0, 0, msW_, msH_, 0, 0, msW_, msH_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, msW_, msH_, 0, 0, msW_, msH_, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    // ---- the lights in the air: glows at their depth, hidden behind what stands in front of them
    worldT_.bind();
    if (!GW.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthMask(GL_FALSE);
        glUseProgram(progGlow_);
        csetMat(progGlow_, "uVP", vp3_);
        cset1f(progGlow_, "uTanH", tanH_);
        cset1f(progGlow_, "uAspect", aspect_);
        cset3f(progGlow_, "uEye", eye_);
        cset1f(progGlow_, "uPx", 2.f * tanH_ / msH_);
        cset1f(progGlow_, "uR", 0.35f);
        cset3f(progGlow_, "uGA", vec3(0.00008f, 0.0003f, 0.00012f));
        cset3f(progGlow_, "uGB", vec3(0.00003f, 0.0009f, 0.006f));
        cset1f(progGlow_, "uPxK", 1.3f);
        glBindVertexArray(glowVao_);
        glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
        glBufferData(GL_ARRAY_BUFFER, GW.size() * sizeof(float), GW.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(GW.size() / 8));
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // =========================================================== the scene (full resolution)
    sceneT_.bind();
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, worldT_.tex);
    set1i(progComp_, "uWorld", 0);
    set1f(progComp_, "uSlow", slow_);
    set1f(progComp_, "uWorldA", world);
    set3f(progComp_, "uBridgeCol", bridge_.color);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", lerp(pal_.veil, bridge_.color, bridge));
        set3f(progVeil_, "uRail", lerp(lerp(pal_.ink, pal_.accent, 0.3f) * 0.5f, bridge_.ink * 0.4f, bridge));
        set1f(progVeil_, "uWellA", 0.93f);
        set1f(progVeil_, "uColA", 0.4f * world);
        set1f(progVeil_, "uRailA", 0.55f);
        set1f(progVeil_, "uPx", pxPerUnit_);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.f, layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", 5.f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set3f(progCell_, "uLightC", vec3(0.92f, 0.97f, 1.f));
        set1f(progCell_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
        set3f(progCell_, "uBridgeInk", bridge_.ink);
        set1f(progCell_, "uMossAmt", 0.15f + 0.85f * smoothstepf(2.f, 45.f, linesB_));
        set3f(progCell_, "uMossA", pal_.moss);
        set3f(progCell_, "uMossB", pal_.moss2);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    if (!sprites_.empty()) {
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    drawHud(f.panels);
    fade_ = saturate(f.fade);
    glDisable(GL_BLEND);
    if (gpuQ) {
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
        static double acc = 0.0;
        static int n = 0;
        acc += ns * 1e-6;
        if (++n == 60) {
            std::fprintf(stderr, "[terrarium] gpu %.2f ms/frame (lines %.1f, %dx%d world)\n", acc / n, linesEased_, worldT_.w, worldT_.h);
            acc = 0.0;
            n = 0;
        }
    }

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

const UniverseEntry& universe_terrarium() {
    static const UniverseEntry e{
        "TERRARIUM", "a sealed specimen jar on a lab bench; its little world grows, cracks the glass and breaks out, swallowing the lab and then the city at night under its neon",
        [] { return std::unique_ptr<Universe>(new TerrariumUniverse()); },
        // Fits most songs; the calm, gentle ones a little better.
        [](const SongLook& l) { return 0.85f + 0.2f * (1.f - l.density) + 0.1f * (1.f - l.dynamics); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}
